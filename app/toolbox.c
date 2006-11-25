/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include "gwyappinternal.h"
#include "gwyddion.h"

enum {
    DND_TARGET_STRING = 1,
};

typedef struct {
    const gchar *stock_id;
    const gchar *tooltip;
    GCallback callback;
    gconstpointer cbdata;
} Action;

static GtkWidget* gwy_app_toolbox_process_new  (void);
static GtkWidget* gwy_app_toolbox_graph_new    (void);
static GtkWidget* gwy_app_toolbox_tools_new    (void);
static GtkWidget* gwy_app_menu_create_meta_menu(GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_edit_menu(GtkAccelGroup *accel_group);
static void       gwy_app_toolbox_create_group (GtkBox *box,
                                                const gchar *text,
                                                const gchar *id,
                                                GtkWidget *toolbox);
static void       gwy_app_toolbox_showhide_cb  (GtkWidget *expander);
static void       gwy_app_toolbox_focus_first  (GtkWidget *toolbar);
static void       toolbox_dnd_data_received    (GtkWidget *widget,
                                                GdkDragContext *context,
                                                gint x,
                                                gint y,
                                                GtkSelectionData *data,
                                                guint info,
                                                guint time_,
                                                gpointer user_data);
static void       gwy_app_meta_browser         (void);
static void       delete_app_window            (void);
static void       gwy_app_zoom_set_cb          (gpointer user_data);
static void       gwy_app_undo_cb              (void);
static void       gwy_app_redo_cb              (void);
static void       gwy_app_tool_use_cb          (const gchar *toolname,
                                                GtkWidget *button);
static void gwy_app_change_default_mask_color_cb(void);
static void       gwy_app_gl_view_maybe_cb     (void);

static GtkTargetEntry dnd_target_table[] = {
    { "STRING",     0, DND_TARGET_STRING },
    { "text/plain", 0, DND_TARGET_STRING },
};

/* Toolbox contents.  To certain degree ready to externalize */
static const gchar *proc_functions[] = {
    "fix_zero", "scale", "rotate", "unrotate",
    "level", "facet-level", "line_correct_median", "scars_remove",
    "grain_mark", "grain_wshed", "grain_rem_threshold", "grain_dist",
    "shade", "polylevel", "laplace",
};

static const gchar *graph_functions[] = {
    "graph_cd", "graph_fit",
};

/* There is no way to access tools other than the toolbox, therefore we handle
 * tools differently: after adding tools from the following list we add all
 * remaining tools. */
static const gchar *default_tools[] = {
    "GwyToolReadValue", "GwyToolDistance", "GwyToolPolynom", "GwyToolCrop",
    "GwyToolFilter", "GwyToolLevel3", "GwyToolStats", "GwyToolSFunctions",
    "GwyToolProfile", "GwyToolGrainRemover", "GwyToolSpotRemover",
    "GwyToolColorRange", "GwyToolMaskEditor", "GwyToolLineStats",
};

/* FIXME: A temporary hack. */
static void
set_sensitivity(GtkItemFactory *item_factory, ...)
{
    GwySensitivityGroup *sensgroup;
    GwyMenuSensFlags mask;
    const gchar *path;
    GtkWidget *widget;
    va_list ap;

    sensgroup = gwy_app_sensitivity_get_group();
    va_start(ap, item_factory);
    while ((path = va_arg(ap, const gchar*))) {
        mask = va_arg(ap, guint);
        widget = gtk_item_factory_get_widget(item_factory, path);
        if (!widget) {
            g_warning("Cannot find menu item %s", path);
            continue;
        }
        gwy_sensitivity_group_add_widget(sensgroup, widget, mask);
    }
    va_end(ap);
}

static void
toolbox_add_menubar(GtkWidget *container,
                    GtkWidget *menu,
                    const gchar *item_label)
{
    GtkWidget *item, *alignment, *menubar;
    GtkTextDirection direction;

    menubar = gtk_menu_bar_new();
    direction = gtk_widget_get_direction(menubar);
    alignment = gtk_alignment_new(direction == GTK_TEXT_DIR_RTL ? 1.0 : 0.0,
                                  0.0, 1.0, 0.0);
    gtk_container_add(GTK_CONTAINER(container), alignment);
    gtk_container_add(GTK_CONTAINER(alignment), menubar);
    gtk_widget_set_name(menubar, "toolboxmenubar");

    item = gtk_menu_item_new_with_mnemonic(item_label);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
}

static GtkWidget*
add_button(GtkWidget *toolbar,
           guint i,
           const Action *action,
           GtkTooltips *tips)
{
    GtkWidget *button;
    const gchar *stock_id;

    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_table_attach_defaults(GTK_TABLE(toolbar), button,
                              i%4, i%4 + 1, i/4, i/4 + 1);
    stock_id = action->stock_id ? action->stock_id : GTK_STOCK_MISSING_IMAGE;
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    g_signal_connect_swapped(button, "clicked",
                             action->callback, (gpointer)action->cbdata);
    gtk_tooltips_set_tip(tips, button, _(action->tooltip), NULL);

    return button;
}

static GtkWidget*
add_rbutton(GtkWidget *toolbar,
            guint i,
            const Action *action,
            GtkRadioButton *group,
            GtkTooltips *tips)
{
    GtkWidget *button;
    const gchar *stock_id;

    button = gtk_radio_button_new_from_widget(group);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_table_attach_defaults(GTK_TABLE(toolbar), button,
                              i%4, i%4 + 1, i/4, i/4 + 1);
    stock_id = action->stock_id ? action->stock_id : GTK_STOCK_MISSING_IMAGE;
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(stock_id,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    g_signal_connect_swapped(button, "clicked",
                             action->callback, (gpointer)action->cbdata);
    gtk_tooltips_set_tip(tips, button, _(action->tooltip), NULL);

    return button;
}

GtkWidget*
gwy_app_toolbox_create(void)
{
    static const Action view_actions[] = {
        {
            GWY_STOCK_ZOOM_IN,
            N_("Zoom in"),
            G_CALLBACK(gwy_app_zoom_set_cb),
            GINT_TO_POINTER(1),
        },
        {
            GWY_STOCK_ZOOM_1_1,
            N_("Zoom 1:1"),
            G_CALLBACK(gwy_app_zoom_set_cb),
            GINT_TO_POINTER(10000),
        },
        {
            GWY_STOCK_ZOOM_OUT,
            N_("Zoom out"),
            G_CALLBACK(gwy_app_zoom_set_cb),
            GINT_TO_POINTER(-1),
        },
        {
            GWY_STOCK_3D_BASE,
            N_("Display a 3D view of data"),
            G_CALLBACK(gwy_app_gl_view_maybe_cb),
            NULL,
        },
    };
    GtkWidget *toolbox, *toolbar, *menu, *button, *container;
    GtkBox *vbox;
    GtkTooltips *tooltips;
    GtkAccelGroup *accel_group;
    guint i;

    toolbox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(toolbox), g_get_application_name());
    gtk_window_set_role(GTK_WINDOW(toolbox), "toolbox");
    gtk_window_set_resizable(GTK_WINDOW(toolbox), FALSE);
    gwy_app_main_window_set(toolbox);

    accel_group = gtk_accel_group_new();
    g_object_set_data(G_OBJECT(toolbox), "accel_group", accel_group);

    vbox = GTK_BOX(gtk_vbox_new(FALSE, 0));
    container = GTK_WIDGET(vbox);
    gtk_container_add(GTK_CONTAINER(toolbox), container);

    tooltips = gwy_app_get_tooltips();

    toolbox_add_menubar(container,
                        gwy_app_menu_create_file_menu(accel_group), _("_File"));
    toolbox_add_menubar(container,
                        gwy_app_menu_create_edit_menu(accel_group), _("_Edit"));

    menu = gwy_app_build_process_menu(accel_group);
    gwy_app_process_menu_add_run_last(menu);
    gtk_accel_group_lock(gtk_menu_get_accel_group(GTK_MENU(menu)));
    toolbox_add_menubar(container, menu, _("_Data Process"));

    menu = gwy_app_build_graph_menu(accel_group);
    gtk_accel_group_lock(gtk_menu_get_accel_group(GTK_MENU(menu)));
    toolbox_add_menubar(container, menu, _("_Graph"));

    toolbox_add_menubar(container,
                        gwy_app_menu_create_meta_menu(accel_group), _("_Meta"));

    /***************************************************************/
    toolbar = gtk_table_new(1, 4, TRUE);
    for (i = 0; i < G_N_ELEMENTS(view_actions); i++) {
        button = add_button(toolbar, i, view_actions + i, tooltips);
        gwy_app_sensitivity_add_widget(button, GWY_MENU_FLAG_DATA);
    }
    gwy_app_toolbox_create_group(vbox, _("View"), "zoom", toolbar);

    toolbar = gwy_app_toolbox_process_new();
    gwy_app_toolbox_create_group(vbox, _("Data Process"), "proc", toolbar);

    toolbar = gwy_app_toolbox_graph_new();
    gwy_app_toolbox_create_group(vbox, _("Graph"), "graph", toolbar);

    toolbar = gwy_app_toolbox_tools_new();
    gwy_app_toolbox_create_group(vbox, _("Tools"), "tool", toolbar);
    gwy_app_toolbox_focus_first(toolbar);

    /***************************************************************/
    gtk_drag_dest_set(toolbox, GTK_DEST_DEFAULT_ALL,
                      dnd_target_table, G_N_ELEMENTS(dnd_target_table),
                      GDK_ACTION_COPY);
    g_signal_connect(toolbox, "drag-data-received",
                     G_CALLBACK(toolbox_dnd_data_received), NULL);

    /***************************************************************/
    /* XXX */
    g_signal_connect(toolbox, "delete-event", G_CALLBACK(gwy_app_quit), NULL);

    gtk_window_add_accel_group(GTK_WINDOW(toolbox), accel_group);
    gtk_widget_show_all(toolbox);

    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    return toolbox;
}

static GtkWidget*
gwy_app_toolbox_process_new(void)
{
    GwyMenuSensFlags sens;
    GtkTooltips *tooltips;
    GtkWidget *toolbar, *button;
    GPtrArray *funcs;
    const gchar *name;
    Action action;
    guint i;

    funcs = g_ptr_array_new();
    for (i = 0; i < G_N_ELEMENTS(proc_functions); i++) {
        if (!gwy_process_func_exists(proc_functions[i]))
            continue;
        g_ptr_array_add(funcs, (gpointer)proc_functions[i]);
    }

    tooltips = gwy_app_get_tooltips();
    toolbar = gtk_table_new((funcs->len + 3)/4, 4, TRUE);

    action.callback = G_CALLBACK(gwy_app_run_process_func);
    for (i = 0; i < funcs->len; i++) {
        name = g_ptr_array_index(funcs, i);
        action.stock_id = gwy_process_func_get_stock_id(name);
        action.tooltip = gwy_process_func_get_tooltip(name);
        action.cbdata = name;
        button = add_button(toolbar, i, &action, tooltips);
        sens = gwy_process_func_get_sensitivity_mask(name);
        gwy_app_sensitivity_add_widget(button, sens);
    }
    g_ptr_array_free(funcs, TRUE);

    return toolbar;
}

static GtkWidget*
gwy_app_toolbox_graph_new(void)
{
    GwyMenuSensFlags sens;
    GtkTooltips *tooltips;
    GtkWidget *toolbar, *button;
    GPtrArray *funcs;
    const gchar *name;
    Action action;
    guint i;

    funcs = g_ptr_array_new();
    for (i = 0; i < G_N_ELEMENTS(graph_functions); i++) {
        if (!gwy_graph_func_exists(graph_functions[i]))
            continue;
        g_ptr_array_add(funcs, (gpointer)graph_functions[i]);
    }

    tooltips = gwy_app_get_tooltips();
    toolbar = gtk_table_new((funcs->len + 3)/4, 4, TRUE);

    action.callback = G_CALLBACK(gwy_app_run_graph_func);
    for (i = 0; i < funcs->len; i++) {
        name = g_ptr_array_index(funcs, i);
        action.stock_id = gwy_graph_func_get_stock_id(name);
        action.tooltip = gwy_graph_func_get_tooltip(name);
        action.cbdata = name;
        button = add_button(toolbar, i, &action, tooltips);
        sens = gwy_graph_func_get_sensitivity_mask(name);
        gwy_app_sensitivity_add_widget(button, sens);
    }
    g_ptr_array_free(funcs, TRUE);

    return toolbar;
}

static void
add_tool(const gchar *typename,
         GArray *tools)
{
    GType type;
    GwyToolClass *tool_class;
    guint i;

    gwy_debug("typename: <%s>", typename);

    type = g_type_from_name(typename);
    if (!type || !G_TYPE_IS_INSTANTIATABLE(type))
        return;
    gwy_debug("%lu", (gulong)type);
    tool_class = g_type_class_peek(type);
    if (!tool_class)
        return;

    gwy_debug("%p", tool_class);
    if (!GWY_IS_TOOL_CLASS(tool_class)) {
        g_warning("Tool %s is not a subclass of GwyTool",
                  g_type_name(type));
        return;
    }

    /* Filter out already added tools (from defaults).
     * FIXME: This is makes the tool enumeration O(n^2).  But there should
     * not be more than a handful of tools. */
    for (i = 0; i < tools->len; i++) {
        if (g_array_index(tools, GType, i) == type)
            return;
    }

    g_array_append_val(tools, type);
}

static GtkWidget*
gwy_app_toolbox_tools_new(void)
{
    const gchar *first_tool = NULL;
    GtkTooltips *tooltips;
    GtkWidget *toolbar, *button;
    GArray *funcs;
    GtkRadioButton *group;
    Action action;
    guint i;

    funcs = g_array_new(TRUE, FALSE, sizeof(GType));
    for (i = 0; i < G_N_ELEMENTS(default_tools); i++)
        add_tool(default_tools[i], funcs);
    gwy_tool_func_foreach((GFunc)&add_tool, funcs);

    tooltips = gwy_app_get_tooltips();
    toolbar = gtk_table_new((funcs->len + 3)/4, 4, TRUE);

    action.callback = G_CALLBACK(gwy_app_tool_use_cb);
    group = NULL;
    for (i = 0; i < funcs->len; i++) {
        GwyToolClass *tool_class;

        tool_class = g_type_class_peek(g_array_index(funcs, GType, i));
        action.stock_id = gwy_tool_class_get_stock_id(tool_class);
        action.tooltip = gwy_tool_class_get_tooltip(tool_class);
        action.cbdata = g_type_name(g_array_index(funcs, GType, i));
        button = add_rbutton(toolbar, i, &action, group, tooltips);
        if (!group) {
            group = GTK_RADIO_BUTTON(button);
            first_tool = default_tools[i];
        }
    }
    g_array_free(funcs, TRUE);

    if (first_tool)
        gwy_app_switch_tool(first_tool);

    return toolbar;
}

/*************************************************************************/
static GtkWidget*
gwy_app_menu_create_meta_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL
        },
        {
            N_("/Show _Data Browser"),
            NULL,
            gwy_app_data_browser_show,
            0,
            "<Item>",
            NULL
        },
        {
            N_("/_Metadata Browser"),
            NULL,
            gwy_app_meta_browser,
            0,
            "<Item>",
            NULL
        },
        {
            N_("/Module _Browser"),
            NULL,
            gwy_module_browser,
            0,
            "<Item>",
            NULL
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL },
        {
            N_("/_About Gwyddion"),
            NULL,
            gwy_app_about,
            0,
            "<StockItem>",
            GTK_STOCK_ABOUT
        },
    };
    GtkItemFactory *item_factory;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<meta>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    "<meta>/Metadata Browser", GWY_MENU_FLAG_DATA,
                    NULL);

    return gtk_item_factory_get_widget(item_factory, "<meta>");
}

static GtkWidget*
gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL
        },
        {
            N_("/_Open"),
            "<control>O",
            gwy_app_file_open,
            0,
            "<StockItem>",
            GTK_STOCK_OPEN
        },
        {
            N_("/Open _Recent"),
            NULL,
            NULL,
            0,
            "<Branch>",
            NULL
        },
        {
            N_("/_Save"),
            "<control>S",
            gwy_app_file_save,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE
        },
        {
            N_("/Save _As"),
            "<control><shift>S",
            gwy_app_file_save_as,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE_AS
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL
        },
        {
            N_("/_Quit"),
            "<control>Q",
            delete_app_window,
            0,
            "<StockItem>",
            GTK_STOCK_QUIT
        },
    };
    GtkItemFactory *item_factory;
    GtkWidget *item, *menu;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<file>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    "<file>/Save",         GWY_MENU_FLAG_DATA,
                    "<file>/Save As",      GWY_MENU_FLAG_DATA,
                    NULL);

    item = gtk_item_factory_get_item(item_factory, "<file>/Open Recent");
    menu = gwy_app_menu_recent_files_get();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);

    return gtk_item_factory_get_widget(item_factory, "<file>");
}

GtkWidget*
gwy_app_menu_create_edit_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL
        },
        {
            N_("/_Undo"),
            "<control>Z",
            gwy_app_undo_cb,
            0,
            "<StockItem>",
            GTK_STOCK_UNDO
        },
        {
            N_("/_Redo"),
            "<control>Y",
            gwy_app_redo_cb,
            0,
            "<StockItem>",
            GTK_STOCK_REDO
        },
        {
            "/---",
            NULL,
            NULL,
            0,
            "<Separator>",
            NULL
        },
        {
            N_("/Default Mask _Color..."),
            NULL,
            gwy_app_change_default_mask_color_cb,
            0,
            NULL,
            NULL
        },
        {
            N_("/Color _Gradients..."),
            NULL,
            gwy_app_gradient_editor,
            0,
            NULL,
            NULL
        },
        {
            N_("/G_L Materials..."),
            NULL,
            gwy_app_gl_material_editor,
            0,
            NULL,
            NULL
        },
    };
    GtkItemFactory *item_factory;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<edit>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);

    set_sensitivity(item_factory,
                    "<edit>/Undo", GWY_MENU_FLAG_UNDO,
                    "<edit>/Redo", GWY_MENU_FLAG_REDO,
                    NULL);

    return gtk_item_factory_get_widget(item_factory, "<edit>");
}

static void
gwy_app_toolbox_create_group(GtkBox *box,
                             const gchar *text,
                             const gchar *id,
                             GtkWidget *toolbox)
{
    GwyContainer *settings;
    GtkWidget *expander;
    gboolean visible = TRUE;
    gchar *s, *key;
    GQuark quark;

    settings = gwy_app_settings_get();
    key = g_strconcat("/app/toolbox/visible/", id, NULL);
    quark = g_quark_from_string(key);
    g_free(key);
    gwy_container_gis_boolean(settings, quark, &visible);

    gtk_box_pack_start(box, gtk_hseparator_new(), FALSE, FALSE, 0);

    s = g_strconcat("<small>", text, "</small>", NULL);
    expander = gtk_expander_new(s);
    gtk_expander_set_use_markup(GTK_EXPANDER(expander), TRUE);
    g_free(s);
    g_object_set_data(G_OBJECT(expander), "key", GUINT_TO_POINTER(quark));
    gtk_container_add(GTK_CONTAINER(expander), toolbox);
    gtk_expander_set_expanded(GTK_EXPANDER(expander), visible);
    gtk_box_pack_start(box, expander, FALSE, FALSE, 0);
    g_signal_connect_after(expander, "activate",
                           G_CALLBACK(gwy_app_toolbox_showhide_cb), NULL);
}

static void
gwy_app_toolbox_showhide_cb(GtkWidget *expander)
{
    GwyContainer *settings;
    gboolean visible;
    GQuark quark;

    settings = gwy_app_settings_get();
    quark = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(expander), "key"));
    visible = gtk_expander_get_expanded(GTK_EXPANDER(expander));
    gwy_container_set_boolean(settings, quark, visible);
}

static void
gwy_app_toolbox_focus_first(GtkWidget *toolbar)
{
    GtkWidget *child;

    g_return_if_fail(GTK_IS_TABLE(toolbar));
    child = gwy_table_get_child_widget(toolbar, 0, 0);
    if (child)
        gtk_widget_grab_focus(child);
}

static void
toolbox_dnd_data_received(G_GNUC_UNUSED GtkWidget *widget,
                          GdkDragContext *context,
                          G_GNUC_UNUSED gint x,
                          G_GNUC_UNUSED gint y,
                          GtkSelectionData *data,
                          G_GNUC_UNUSED guint info,
                          guint time_,
                          G_GNUC_UNUSED gpointer user_data)
{
    gchar *filename;
    gchar **file_list;
    gboolean ok = FALSE;
    gint i, n;

    if (data->length <= 0 || data->format != 8) {
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }

    file_list = g_strsplit((gchar*)data->data, "\n", 0);
    if (!file_list) {
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }

    /* Stop on an empty line too.
     * This (1) kills the last empty line (2) prevents some cases of total
     * bogus to be processed any further */
    for (n = 0; file_list[n] && file_list[n][0]; n++)
        ;

    for (i = 0; i < n; i++) {
        filename = g_strstrip(file_list[i]);
        if (g_str_has_prefix(filename, "file://"))
            filename += sizeof("file://") - 1;
        gwy_debug("filename = %s", filename);
        if (g_file_test(filename, G_FILE_TEST_IS_REGULAR
                                  | G_FILE_TEST_IS_SYMLINK)) {
            /* FIXME: what about charset conversion? */
            if (gwy_app_file_load(filename, NULL, NULL))
                ok = TRUE;    /* FIXME: what if we accept only some? */
        }
    }
    g_strfreev(file_list);
    gtk_drag_finish(context, ok, FALSE, time_);
}

static void
gwy_app_meta_browser(void)
{
    GwyContainer *data;
    gint id;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    gwy_app_metadata_browser(data, id);
}

static void
delete_app_window(void)
{
    gboolean boo;

    g_signal_emit_by_name(gwy_app_main_window_get(), "delete-event",
                          NULL, &boo);
}

/* FIXME: we should zoom whatever is currently active: datawindow, 3dwindow,
 * graph */
static void
gwy_app_zoom_set_cb(gpointer user_data)
{
    GtkWidget *window, *view;

    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &view, 0);
    window = gtk_widget_get_ancestor(view, GWY_TYPE_DATA_WINDOW);
    g_return_if_fail(window);
    gwy_data_window_set_zoom(GWY_DATA_WINDOW(window),
                             GPOINTER_TO_INT(user_data));
}

static void
gwy_app_undo_cb(void)
{
    GwyContainer *data;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    if (data)
        gwy_app_undo_undo_container(data);
}

static void
gwy_app_redo_cb(void)
{
    GwyContainer *data;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    if (data)
        gwy_app_undo_redo_container(data);
}

static void
gwy_app_tool_use_cb(const gchar *toolname,
                    GtkWidget *button)
{
    /* don't catch deactivations */
    if (button && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
        gwy_debug("deactivation");
    }
    else
        gwy_app_switch_tool(toolname);
}

static void
gwy_app_change_default_mask_color_cb(void)
{
    gwy_color_selector_for_mask(_("Change Default Mask Color"),
                                NULL, gwy_app_settings_get(), "/mask");
}

static void
gwy_app_gl_view_maybe_cb(void)
{
    static GtkWidget *dialog = NULL;

    if (gwy_app_gl_is_ok()) {
        GwyContainer *data;
        gint id;

        gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data,
                                         GWY_APP_DATA_FIELD_ID, &id,
                                         0);
        g_return_if_fail(data);
        gwy_app_data_browser_show_3d(data, id);
        return;
    }

    if (dialog) {
        gtk_window_present(GTK_WINDOW(dialog));
        return;
    }

    dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("OpenGL 3D graphics not available"));
    gtk_message_dialog_format_secondary_markup
        (GTK_MESSAGE_DIALOG(dialog),
#ifdef GWYDDION_HAS_OPENGL
         /* FIXME: Makes sense only on Unix */
         /* FIXME: It would be nice to give a more helpful message, but the
          * trouble is we don't know why the silly thing failed either. */
         _("Initialization of OpenGL failed.  Check output of "
           "<tt>glxinfo</tt> and warning messages printed to console during "
           "Gwyddion startup.")
#else
         _("This version of Gwyddion was built without OpenGL support.")
#endif
        );
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer*)&dialog);
    gtk_widget_show(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
