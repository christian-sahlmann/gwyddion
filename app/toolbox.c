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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

#include "gwyappinternal.h"
#include "gwyddion.h"

#include "mac_integration.h"

enum {
    DND_TARGET_STRING = 1,
};

typedef struct {
    GtkTooltips *tips;
    GtkBox *box;
    GString *path;
    GtkWidget *group;
    gint pos;
    gint width;
    GtkRadioButton *first_tool;
    const gchar *first_tool_func;
    GPtrArray *unseen_tools;
} GwyAppToolboxBuilder;

typedef enum {
    GWY_APP_FUNC_TYPE_NONE = -1,
    GWY_APP_FUNC_TYPE_PLACEHOLDER = 0,
    GWY_APP_FUNC_TYPE_BUILTIN,
    GWY_APP_FUNC_TYPE_PROC,
    GWY_APP_FUNC_TYPE_GRAPH,
    GWY_APP_FUNC_TYPE_TOOL
} GwyAppFuncType;

typedef struct {
    const gchar *stock_id;
    const gchar *tooltip;
    GCallback callback;
    GwyMenuSensFlags sens;
} Action;

static GtkWidget* gwy_app_menu_create_meta_menu(GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_edit_menu(GtkAccelGroup *accel_group);
static void       gwy_app_toolbox_create_group (GtkBox *box,
                                                const gchar *text,
                                                const gchar *id,
                                                GtkWidget *toolbox);
static void       gwy_app_toolbox_showhide_cb  (GtkWidget *expander);
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
    { "STRING",        0, DND_TARGET_STRING, },
    { "text/plain",    0, DND_TARGET_STRING, },
    { "text/uri-list", 0, DND_TARGET_STRING, },
};

/* Translatability hack, intltool seems overkill at this point. */
#define GWY_TOOLBOX_IGNORE(x) /* */
GWY_TOOLBOX_IGNORE((_("View"), _("Data Process"), _("Graph"), _("Tools")))

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

static gboolean
gwy_app_builtin_func_get_info(const gchar *name,
                              Action *action)
{
    action->sens = GWY_MENU_FLAG_DATA;

    if (gwy_strequal(name, "display_3d")) {
        action->callback = gwy_app_gl_view_maybe_cb;
        action->stock_id = GWY_STOCK_3D_BASE;
        action->tooltip = N_("Display a 3D view of data");
        return TRUE;
    }

    action->callback = G_CALLBACK(gwy_app_zoom_set_cb);
    if (gwy_strequal(name, "zoom_in")) {
        action->stock_id = GWY_STOCK_ZOOM_IN;
        action->tooltip = N_("Zoom in");
    }
    else if (gwy_strequal(name, "zoom_1_1")) {
        action->stock_id = GWY_STOCK_ZOOM_1_1;
        action->tooltip = N_("Zoom 1:1");
    }
    else if (gwy_strequal(name, "zoom_out")) {
        action->stock_id = GWY_STOCK_ZOOM_OUT;
        action->tooltip = N_("Zoom out");
    }
    else
        return FALSE;

    return TRUE;
}

static void
toolbox_ui_start_toolbox(GwyAppToolboxBuilder *builder,
                         const gchar **attribute_names,
                         const gchar **attribute_values)
{
    guint i;
    gint vi;

    if (strlen(builder->path->str)) {
        g_warning("Ignoring non top-level <toolbox>");
        return;
    }

    for (i = 0; attribute_names[i]; i++) {
        if (gwy_strequal(attribute_names[i], "width")) {
            vi = atoi(attribute_values[i]);
            if (vi > 0 && vi < 1024)
                builder->width = vi;
            else
                g_warning("Ignoring wrong toolbox width %d", vi);
        }
        else {
            gwy_debug("Unimplemented <toolbox> attribute %s",
                      attribute_names[i]);
        }
    }
}

static void
toolbox_ui_start_group(GwyAppToolboxBuilder *builder,
                       const gchar **attribute_names,
                       const gchar **attribute_values)
{
    const gchar *id = NULL, *title = NULL;
    guint i, l;

    if (!gwy_strequal(builder->path->str, "/toolbox")) {
        g_warning("Ignoring <group> not in <toolbox>");
        return;
    }

    for (i = 0; attribute_names[i]; i++) {
        if (gwy_strequal(attribute_names[i], "id")) {
            if (gwy_strisident(attribute_values[i], NULL, NULL))
                id = attribute_values[i];
            else
                g_warning("Ignoring non-identifier id=\"%s\"",
                          attribute_values[i]);
        }
        else if (gwy_strequal(attribute_names[i], "title")) {
            if ((l = strlen(attribute_values[i]))
                && g_utf8_validate(attribute_values[i], l, NULL))
                title = attribute_values[i];
            else
                g_warning("Ignoring invalid title");
        }
    }

    if (!id || !title) {
        g_warning("Ignoring <group> with missing/invalid id and title");
        return;
    }

    builder->group = gtk_table_new(1, builder->width, TRUE);
    gwy_app_toolbox_create_group(builder->box, gettext(title), id,
                                 builder->group);
    builder->pos = 0;
}

static GtkWidget*
toolbox_ui_make_tool(GwyAppToolboxBuilder *builder,
                     GwyToolClass *tool_class,
                     Action *action)
{
    GtkWidget *button;
    const gchar *name;
    gboolean found;
    guint i;

    action->stock_id = gwy_tool_class_get_stock_id(tool_class);
    action->tooltip = gwy_tool_class_get_tooltip(tool_class);
    action->callback = G_CALLBACK(gwy_app_tool_use_cb);
    action->sens = -1;
    button = gtk_radio_button_new_from_widget(builder->first_tool);
    name = g_type_name(G_TYPE_FROM_CLASS(tool_class));
    if (!builder->first_tool) {
        builder->first_tool = GTK_RADIO_BUTTON(button);
        builder->first_tool_func = name;
    }
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);

    found = FALSE;
    for (i = 0; i < builder->unseen_tools->len; i++) {
        if (gwy_strequal(name, g_ptr_array_index(builder->unseen_tools, i))) {
            g_ptr_array_remove_index_fast(builder->unseen_tools, i);
            found = TRUE;
            break;
        }
    }
    if (!found)
        g_warning("Tool %s is not in unseen -- being added for a second time?",
                  name);

    return button;
}

static void
toolbox_ui_start_item(GwyAppToolboxBuilder *builder,
                      const gchar **attribute_names,
                      const gchar **attribute_values)
{
    static const GwyEnum types[] = {
        { "empty",   GWY_APP_FUNC_TYPE_PLACEHOLDER, },
        { "builtin", GWY_APP_FUNC_TYPE_BUILTIN, },
        { "proc",    GWY_APP_FUNC_TYPE_PROC,    },
        { "graph",   GWY_APP_FUNC_TYPE_GRAPH,   },
        { "tool",    GWY_APP_FUNC_TYPE_TOOL,    },
    };

    GwyAppFuncType t, type = GWY_APP_FUNC_TYPE_NONE;
    const gchar *func = NULL, *icon = NULL;
    GwyToolClass *tool_class;
    GType gtype;
    GtkWidget *button = NULL;
    Action action;
    gchar *s;
    guint i;

    if (!builder->group
        || !gwy_strequal(builder->path->str, "/toolbox/group")) {
        g_warning("Ignoring <item> not in a <group>");
        return;
    }

    for (i = 0; attribute_names[i]; i++) {
        if (gwy_strequal(attribute_names[i], "type")
            && (t = gwy_string_to_enum(attribute_values[i],
                                       types, G_N_ELEMENTS(types))) != -1)
            type = t;
        else if (gwy_strequal(attribute_names[i], "function"))
            func = attribute_values[i];
        else if (gwy_strequal(attribute_names[i], "icon"))
            icon = attribute_values[i];
    }

    if ((!func && type != GWY_APP_FUNC_TYPE_TOOL)
        || (func && !gwy_strisident(func, "_-", NULL))) {
        g_warning("Ignoring item with invalid function=\"%s\"", func);
        return;
    }

    switch (type) {
        case GWY_APP_FUNC_TYPE_NONE:
        g_warning("Ignoring item with invalid type");
        return;
        break;

        case GWY_APP_FUNC_TYPE_PLACEHOLDER:
        builder->pos++;
        return;
        break;

        case GWY_APP_FUNC_TYPE_BUILTIN:
        if (!gwy_app_builtin_func_get_info(func, &action)) {
            g_warning("Function builtin::%s does not exist", func);
            return;
        }
        break;

        case GWY_APP_FUNC_TYPE_PROC:
        if (!gwy_process_func_exists(func)) {
            g_warning("Function proc::%s does not exist", func);
            return;
        }
        action.stock_id = gwy_process_func_get_stock_id(func);
        action.tooltip = gwy_process_func_get_tooltip(func);
        action.callback = G_CALLBACK(gwy_app_run_process_func);
        action.sens = gwy_process_func_get_sensitivity_mask(func);
        break;

        case GWY_APP_FUNC_TYPE_GRAPH:
        if (!gwy_graph_func_exists(func)) {
            g_warning("Function graph::%s does not exist", func);
            return;
        }
        action.stock_id = gwy_graph_func_get_stock_id(func);
        action.tooltip = gwy_graph_func_get_tooltip(func);
        action.callback = G_CALLBACK(gwy_app_run_graph_func);
        action.sens = gwy_graph_func_get_sensitivity_mask(func);
        break;

        case GWY_APP_FUNC_TYPE_TOOL:
        /* Handle unseen tools */
        if (!func) {
            const gchar **attr_names, **attr_values;
            guint n;

            for (n = 0; attribute_names[n]; n++)
                ;
            attr_names = g_newa(const gchar*, n+2);
            attr_values = g_newa(const gchar*, n+2);
            for (i = 0; i < n; i++) {
                attr_names[i] = attribute_names[i];
                attr_values[i] = attribute_values[i];
            }
            attr_names[n] = "function";
            attr_names[n+1] = attr_values[n+1] = NULL;

            /* FIXME: Attempt to keep a stable order? */
            while (builder->unseen_tools->len) {
                attr_values[n] = g_ptr_array_index(builder->unseen_tools, 0);
                toolbox_ui_start_item(builder, attr_names, attr_values);
            }
            return;
        }
        if (!(gtype = g_type_from_name(func))) {
            g_warning("Function tool::%s does not exist", func);
            return;
        }
        tool_class = g_type_class_peek(gtype);
        if (!GWY_IS_TOOL_CLASS(tool_class)) {
            g_warning("Type %s is not a GwyTool", func);
            return;
        }
        button = toolbox_ui_make_tool(builder, tool_class, &action);
        break;

        default:
        g_return_if_reached();
        break;
    }

    if (!button)
        button = gtk_button_new();
    if (icon)
        action.stock_id = icon;

    if (!action.stock_id) {
        g_warning("Function %s::%s has not icon set",
                  gwy_enum_to_string(type, types, G_N_ELEMENTS(types)), func);
        action.stock_id = GTK_STOCK_MISSING_IMAGE;
    }
    else if (!gtk_icon_factory_lookup_default(action.stock_id)) {
        g_warning("Function %s::%s icon %s not found",
                  gwy_enum_to_string(type, types, G_N_ELEMENTS(types)), func,
                  action.stock_id);
        action.stock_id = GTK_STOCK_MISSING_IMAGE;
    }

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_table_attach_defaults(GTK_TABLE(builder->group), button,
                              builder->pos % builder->width,
                              builder->pos % builder->width + 1,
                              builder->pos/builder->width,
                              builder->pos/builder->width + 1);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(action.stock_id,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    /* XXX: We have already a const string identical to func somewhere. */
    s = g_strdup(func);
    g_signal_connect_swapped(button, "clicked", action.callback, s);
    g_signal_connect_swapped(button, "destroy", G_CALLBACK(g_free), s);
    gtk_tooltips_set_tip(builder->tips, button, _(action.tooltip), NULL);

    if (action.sens != -1)
        gwy_app_sensitivity_add_widget(button, action.sens);

    builder->pos++;
}

static void
toolbox_ui_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                         const gchar *name,
                         const gchar **attribute_names,
                         const gchar **attribute_values,
                         gpointer user_data,
                         G_GNUC_UNUSED GError **error)
{
    GwyAppToolboxBuilder *builder = (GwyAppToolboxBuilder*)user_data;

    if (gwy_strequal(name, "toolbox"))
        toolbox_ui_start_toolbox(builder, attribute_names, attribute_values);
    else if (gwy_strequal(name, "group"))
        toolbox_ui_start_group(builder, attribute_names, attribute_values);
    else if (gwy_strequal(name, "item"))
        toolbox_ui_start_item(builder, attribute_names, attribute_values);

    g_string_append_c(builder->path, '/');
    g_string_append(builder->path, name);
}

static void
toolbox_ui_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                       const gchar *name,
                       gpointer user_data,
                       G_GNUC_UNUSED GError **error)
{
    GwyAppToolboxBuilder *builder = (GwyAppToolboxBuilder*)user_data;
    gchar *p;

    if (gwy_strequal(name, "toolbox")) {
        if (builder->first_tool) {
            gwy_app_switch_tool(builder->first_tool_func);
            gtk_widget_grab_focus(GTK_WIDGET(builder->first_tool));
        }
    }
    else if (gwy_strequal(name, "group")) {
        builder->group = NULL;
    }

    p = strrchr(builder->path->str, '/');
    g_return_if_fail(p);
    g_string_truncate(builder->path, p - builder->path->str);
}

static void
toolbox_ui_text(G_GNUC_UNUSED GMarkupParseContext *context,
                const gchar *text,
                gsize text_len,
                G_GNUC_UNUSED gpointer user_data,
                G_GNUC_UNUSED GError **error)
{
    gsize i;

    for (i = 0; i < text_len; i++) {
        if (!g_ascii_isspace(text[i])) {
            g_warning("Non element content: %s", text);
            return;
        }
    }
}

static void
gather_tools(const gchar *name,
             GPtrArray *tools)
{
    g_ptr_array_add(tools, (gpointer)name);
}

static void
gwy_app_toolbox_build(GtkBox *vbox,
                      GtkTooltips *tips)
{
    static const GMarkupParser parser = {
        toolbox_ui_start_element,
        toolbox_ui_end_element,
        toolbox_ui_text,
        NULL,
        NULL
    };

    GwyAppToolboxBuilder builder;
    GMarkupParseContext *context;
    GError *err = NULL;
    gchar *p, *q, *ui;
    gsize ui_len;

    p = g_build_filename(gwy_get_user_dir(), "ui", "toolbox.xml", NULL);
    if (!g_file_get_contents(p, &ui, &ui_len, NULL)) {
        g_free(p);
        q = gwy_find_self_dir("data");
        p = g_build_filename(q, "ui", "toolbox.xml", NULL);
        g_free(q);
        if (!g_file_get_contents(p, &ui, &ui_len, NULL)) {
            g_critical("Cannot find toolbox user interface %s", p);
            exit(1);
        }
    }
    g_free(p);

    memset(&builder, 0, sizeof(GwyAppToolboxBuilder));
    builder.width = 4;
    builder.box = vbox;
    builder.tips = tips;
    builder.unseen_tools = g_ptr_array_new();
    builder.path = g_string_new(NULL);

    gwy_tool_func_foreach((GFunc)gather_tools, builder.unseen_tools);

    context = g_markup_parse_context_new(&parser, 0, &builder, NULL);
    if (!g_markup_parse_context_parse(context, ui, ui_len, &err)) {
        g_printerr("Toolbox parsing failed: %s\n", err->message);
        g_clear_error(&err);
    }
    g_markup_parse_context_free(context);

    g_ptr_array_free(builder.unseen_tools, TRUE);
    g_string_free(builder.path, TRUE);
    g_free(ui);
}

GtkWidget*
gwy_app_toolbox_create(void)
{
    GtkWidget *toolbox, *menu, *container;
    GtkBox *vbox;
    GtkTooltips *tooltips;
    GtkAccelGroup *accel_group;

    toolbox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(toolbox), g_get_application_name());
    gtk_window_set_role(GTK_WINDOW(toolbox), GWY_TOOLBOX_WM_ROLE);
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

    gwy_app_toolbox_build(vbox, tooltips);

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

    gwy_osx_get_menu_from_widget(container);
    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    gwy_remote_setup(toolbox);
    g_signal_connect(toolbox, "destroy", G_CALLBACK(gwy_remote_finalize), NULL);

    return toolbox;
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
            N_("/_Tip of the Day"),
            NULL,
            gwy_app_tip_of_the_day,
            0,
            "<StockItem>",
            GTK_STOCK_DIALOG_INFO
        },
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
            N_("/_Open..."),
            "<control>O",
            gwy_app_file_open,
            0,
            "<StockItem>",
            GTK_STOCK_OPEN
        },
        {
            N_("/_Merge..."),
            "<control><shift>M",
            gwy_app_file_merge,
            0,
            NULL,
            NULL
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
            N_("/Save _As..."),
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
                    "<file>/Save",         GWY_MENU_FLAG_FILE,
                    "<file>/Save As...",   GWY_MENU_FLAG_FILE,
                    "<file>/Merge...",     GWY_MENU_FLAG_FILE,
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
            "<StockItem>",
            GWY_STOCK_MASK
        },
        {
            N_("/Color _Gradients..."),
            NULL,
            gwy_app_gradient_editor,
            0,
            "<StockItem>",
            GWY_STOCK_PALETTES
        },
        {
            N_("/G_L Materials..."),
            NULL,
            gwy_app_gl_material_editor,
            0,
            "<StockItem>",
            GWY_STOCK_GL_MATERIAL
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

static gboolean
toolbox_dnd_open_files(gpointer user_data)
{
    GPtrArray *files = (GPtrArray*)user_data;
    gchar *filename;
    guint i;

    for (i = 0; i < files->len; i++) {
        filename = (gchar*)g_ptr_array_index(files, i);
        gwy_app_file_load(NULL, filename, NULL);
        g_free(filename);
    }
    g_ptr_array_free(files, TRUE);

    return FALSE;
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
    gchar *uri, *filename, *text;
    gchar **file_list;
    gboolean ok = FALSE;
    GPtrArray *files;
    guint i;

    if (data->length <= 0 || data->format != 8) {
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }

    text = g_strdelimit(g_strdup((gchar*)data->data), "\r\n", '\n');
    file_list = g_strsplit(text, "\n", 0);
    g_free(text);
    if (!file_list) {
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }

    files = g_ptr_array_new();
    for (i = 0; file_list[i]; i++) {
        uri = g_strstrip(file_list[i]);
        if (!*uri)
            continue;
        filename = g_filename_from_uri(uri, NULL, NULL);
        if (!filename)
            continue;
        gwy_debug("filename = %s", filename);
        if (gwy_file_detect(filename, FALSE, GWY_FILE_OPERATION_LOAD)) {
            /* FIXME: what about charset conversion? */
            g_ptr_array_add(files, filename);
            ok = TRUE;    /* FIXME: what if we accept only some? */
        }
        else
            g_free(filename);
    }
    g_strfreev(file_list);
    gtk_drag_finish(context, ok, FALSE, time_);

    if (files->len)
        g_idle_add(toolbox_dnd_open_files, files);
    else
        g_ptr_array_free(files, TRUE);
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
    gint izoom;

    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &view, 0);
    window = gtk_widget_get_ancestor(view, GWY_TYPE_DATA_WINDOW);
    g_return_if_fail(window);

    if (gwy_strequal(user_data, "zoom_in"))
        izoom = 1;
    else if (gwy_strequal(user_data, "zoom_out"))
        izoom = -1;
    else if (gwy_strequal(user_data, "zoom_1_1"))
        izoom = 10000;
    else {
        g_warning("Wrong zoom type passedto zoom set callback");
        return;
    }
    gwy_data_window_set_zoom(GWY_DATA_WINDOW(window), izoom);
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
#ifdef GWYDDION_HAS_OPENGL
    if (gwy_app_gl_disabled()) {
        gtk_message_dialog_format_secondary_markup
            (GTK_MESSAGE_DIALOG(dialog),
             _("OpenGL was disabled with a command-line option."));
    }
    else {
        gtk_message_dialog_format_secondary_markup
            (GTK_MESSAGE_DIALOG(dialog),
             /* FIXME: Makes sense only on Unix */
             /* FIXME: It would be nice to give a more helpful message, but the
              * trouble is we don't know why the silly thing failed either. */
             _("Initialization of OpenGL failed.  Check output of "
               "<tt>glxinfo</tt> and warning messages printed to console "
               "during Gwyddion startup."));
    }
#else
    gtk_message_dialog_format_secondary_markup
        (GTK_MESSAGE_DIALOG(dialog),
         _("This version of Gwyddion was built without OpenGL support."));
#endif
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    g_object_add_weak_pointer(G_OBJECT(dialog), (gpointer*)&dialog);
    gtk_widget_show(dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
