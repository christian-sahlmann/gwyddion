/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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
#include <gdk/gdkkeysyms.h>
#include <gwyconfig.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
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

typedef gboolean (*ActionCheckFunc)(gconstpointer);

static GtkWidget* gwy_app_menu_create_meta_menu (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_proc_menu (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_graph_menu(GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_file_menu (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_create_edit_menu (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_toolbox_create_label (const gchar *text,
                                                const gchar *id,
                                                gboolean *pvisible);
static void       gwy_app_toolbox_showhide_cb  (GtkWidget *button,
                                                GtkWidget *widget);
static void       gwy_app_rerun_process_func_cb(gpointer user_data);
static void       toolbox_dnd_data_received    (GtkWidget *widget,
                                                GdkDragContext *context,
                                                gint x,
                                                gint y,
                                                GtkSelectionData *data,
                                                guint info,
                                                guint time,
                                                gpointer user_data);
static void       gwy_app_meta_browser         (void);
static void       delete_app_window            (void);
static void       gwy_app_undo_cb              (void);
static void       gwy_app_redo_cb              (void);
static void       gwy_app_gl_view_maybe_cb     (void);

static GtkTargetEntry dnd_target_table[] = {
  { "STRING",     0, DND_TARGET_STRING },
  { "text/plain", 0, DND_TARGET_STRING },
};

static GSList*
toolbox_add_menubar(GtkWidget *container,
                    GtkWidget *menu,
                    const gchar *item_label,
                    GSList *menus)
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
    menus = g_slist_append(menus, menubar);

    return menus;
}

static GtkWidget*
add_button(GtkWidget *toolbar,
           guint i,
           const Action *action,
           ActionCheckFunc check_func,
           GtkTooltips *tips)
{
    GtkWidget *button;

    if (check_func && !check_func(action->cbdata))
        return NULL;

    button = gtk_button_new();
    gtk_table_attach_defaults(GTK_TABLE(toolbar), button,
                              i%4, i%4 + 1, i/4, i/4 + 1);
    gtk_container_add(GTK_CONTAINER(button),
                      gtk_image_new_from_stock(action->stock_id,
                                               GTK_ICON_SIZE_BUTTON));
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
    static const Action proc_actions[] = {
        {
            GWY_STOCK_FIX_ZERO,
            N_("Fix minimum value to zero"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "fix_zero",
        },
        {
            GWY_STOCK_SCALE,
            N_("Scale data"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "scale",
        },
        {
            GWY_STOCK_ROTATE,
            N_("Rotate by arbitrary angle"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "rotate",
        },
        {
            GWY_STOCK_UNROTATE,
            N_("Automatically correct rotation"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "unrotate",
        },
        {
            GWY_STOCK_FIT_PLANE,
            N_("Automatically level data"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "level",
        },
        {
            GWY_STOCK_FACET_LEVEL,
            N_("Facet-level data"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "facet_level",
        },
        {
            GWY_STOCK_FFT,
            N_("Fast Fourier Transform"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "fft",
        },
        {
            GWY_STOCK_CWT,
            N_("Continuous Wavelet Transform"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "cwt",
        },
        {
            GWY_STOCK_GRAINS,
            N_("Mark grains by threshold"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "mark_threshold",
        },
        {
            GWY_STOCK_GRAINS_WATER,
            N_("Mark grains by watershed"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "wshed_threshold",
        },
        {
            GWY_STOCK_GRAINS_REMOVE,
            N_("Remove grains by threshold"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "remove_threshold",
        },
        {
            GWY_STOCK_GRAINS_GRAPH,
            N_("Grain size distribution"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "grain_dist",
        },
        {
            GWY_STOCK_FRACTAL,
            N_("Calculate fractal dimension"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "fractal",
        },
        {
            GWY_STOCK_SHADER,
            N_("Shade data"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "shade",
        },
        {
            GWY_STOCK_POLYNOM_REMOVE,
            N_("Remove polynomial background"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "poly_level",
        },
        {
            GWY_STOCK_SCARS,
            N_("Remove scars"),
            G_CALLBACK(gwy_app_run_process_func_cb),
            "scars_remove",
        },
    };
    static const Action graph_actions[] = {
        {
            GWY_STOCK_GRAPH_POINTER,
            N_("Read coordinates"),
            G_CALLBACK(gwy_app_run_graph_func_cb),
            "read",
        },
        {
            GWY_STOCK_GRAPH_ZOOM_IN,
            N_("Zoom a part of graph"),
            G_CALLBACK(gwy_app_run_graph_func_cb),
            "graph_zoom",
        },
        {
            GWY_STOCK_GRAPH_ZOOM_FIT,
            N_("Reset zoom to display complete data"),
            G_CALLBACK(gwy_app_run_graph_func_cb),
            "graph_unzoom",
        },
        {
            GWY_STOCK_GRAPH_RULER,
            N_("Measure graph point distances"),
            G_CALLBACK(gwy_app_run_graph_func_cb),
            "graph_points",
        },
        {
            GWY_STOCK_GRAPH_MEASURE,
            N_("Fit critical dimension"),
            G_CALLBACK(gwy_app_run_graph_func_cb),
            "graph_cd",
        },
        {
            GWY_STOCK_GRAPH_FIT_FUNC,
            N_("Fit functions to graph data"),
            G_CALLBACK(gwy_app_run_graph_func_cb),
            "graph_fit",
        },
    };
    GwyMenuSensData sens_data_data = { GWY_MENU_FLAG_DATA, 0 };
    GwyMenuSensData sens_data_graph = { GWY_MENU_FLAG_GRAPH, 0 };
    GwyMenuSensData sens_data_all = {
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_GRAPH
            | GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW
            | GWY_MENU_FLAG_LAST_PROC | GWY_MENU_FLAG_LAST_GRAPH
            | GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        0
    };
    GtkWidget *toolbox, *vbox, *toolbar, *menu, *label, *button, *container;
    GtkTooltips *tooltips;
    GtkAccelGroup *accel_group;
    GList *list;
    GSList *toolbars = NULL;    /* list of all toolbars for sensitivity */
    GSList *menus = NULL;    /* list of all menus for sensitivity */
    GSList *l;
    const gchar *first_tool;
    gboolean visible;
    guint i, j;

    toolbox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(toolbox), g_get_application_name());
    gtk_window_set_wmclass(GTK_WINDOW(toolbox), "toolbox",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(toolbox), FALSE);
    gwy_app_main_window_set(toolbox);
    gwy_app_main_window_restore_position();

    accel_group = gtk_accel_group_new();
    g_object_set_data(G_OBJECT(toolbox), "accel_group", accel_group);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(toolbox), vbox);
    container = vbox;

    tooltips = gwy_app_tooltips_get();

    menus = toolbox_add_menubar(container,
                                gwy_app_menu_create_file_menu(accel_group),
                                _("_File"), menus);
    menus = toolbox_add_menubar(container,
                                gwy_app_menu_create_edit_menu(accel_group),
                                _("_Edit"), menus);
    menu = gwy_app_menu_create_proc_menu(accel_group);
    g_object_set_data(G_OBJECT(toolbox), "<proc>", menu);     /* XXX */
    menus = toolbox_add_menubar(container, menu, _("_Data Process"), menus);
    menus = toolbox_add_menubar(container,
                                gwy_app_menu_create_graph_menu(accel_group),
                                _("_Graph"), menus);
    menus = toolbox_add_menubar(container,
                                gwy_app_menu_create_meta_menu(accel_group),
                                _("_Meta"), menus);

    gwy_app_menu_set_sensitive_recursive(container, &sens_data_all);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("View"), "zoom", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gtk_table_new(1, 4, TRUE);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    for (i = 0; i < G_N_ELEMENTS(view_actions); i++)
        button = add_button(toolbar, i, view_actions + i, NULL, tooltips);

    gwy_app_menu_set_flags_recursive(toolbar, &sens_data_data);
    gwy_app_menu_set_sensitive_recursive(toolbar, &sens_data_data);

    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Data Process"), "proc", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gtk_table_new(4, 4, TRUE);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    for (j = i = 0; i < G_N_ELEMENTS(proc_actions); i++) {
        button = add_button(toolbar, j, proc_actions + i,
                            (ActionCheckFunc)gwy_process_func_get_run_types,
                            tooltips);
        if (!button)
            continue;
        if (gwy_strequal(proc_actions[i].cbdata, "remove_threshold")
            || gwy_strequal(proc_actions[i].cbdata, "grain_dist"))
            gwy_app_menu_set_sensitive_both(button, GWY_MENU_FLAG_DATA_MASK, 0);
        j++;
    }

    gwy_app_menu_set_flags_recursive(toolbar, &sens_data_data);
    gwy_app_menu_set_sensitive_recursive(toolbar, &sens_data_data);
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Graph"), "graph", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gtk_table_new(1, 4, TRUE);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    for (j = i = 0; i < G_N_ELEMENTS(graph_actions); i++) {
        button = add_button(toolbar, j, graph_actions + i,
                            (ActionCheckFunc)gwy_graph_func_exists, tooltips);
        if (!button)
            continue;
        j++;
    }

    gwy_app_menu_set_flags_recursive(toolbar, &sens_data_graph);
    gwy_app_menu_set_sensitive_recursive(toolbar, &sens_data_graph);
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Tools"), "tool", &visible);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    toolbar = gwy_tool_func_build_toolbox(G_CALLBACK(gwy_app_tool_use_cb),
                                          4, tooltips, &first_tool);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_widget_set_no_show_all(toolbar, !visible);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    gwy_app_menu_set_flags_recursive(toolbar, &sens_data_data);
    gwy_app_menu_set_sensitive_recursive(toolbar, &sens_data_data);
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    list = gtk_container_get_children(GTK_CONTAINER(toolbar));
    gwy_app_tool_use_cb(first_tool, NULL);
    gwy_app_tool_use_cb(first_tool, list ? GTK_WIDGET(list->data) : NULL);
    g_list_free(list);

    /***************************************************************/
    gtk_drag_dest_set(toolbox, GTK_DEST_DEFAULT_ALL,
                      dnd_target_table, G_N_ELEMENTS(dnd_target_table),
                      GDK_ACTION_COPY);
    g_signal_connect(toolbox, "drag-data-received",
                     G_CALLBACK(toolbox_dnd_data_received), NULL);

    /***************************************************************/
    /* XXX */
    g_signal_connect(toolbox, "delete-event",
                     G_CALLBACK(gwy_app_main_window_save_position), NULL);
    g_signal_connect(toolbox, "delete-event", G_CALLBACK(gwy_app_quit), NULL);

    gtk_window_add_accel_group(GTK_WINDOW(toolbox), accel_group);

    gtk_widget_show_all(toolbox);
    gwy_app_main_window_restore_position();
    for (l = toolbars; l; l = g_slist_next(l))
        gtk_widget_set_no_show_all(GTK_WIDGET(l->data), FALSE);
    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    g_object_set_data_full(G_OBJECT(toolbox), "toolbars", toolbars,
                           (GDestroyNotify)g_slist_free);
    g_object_set_data_full(G_OBJECT(toolbox), "menus", menus,
                           (GDestroyNotify)g_slist_free);

    return toolbox;
}

/*************************************************************************/
static GtkWidget*
gwy_app_menu_create_proc_menu(GtkAccelGroup *accel_group)
{
    static const gchar *reshow_accel_path = "<proc>/Re-show Last";
    static const gchar *repeat_accel_path = "<proc>/Repeat Last";
    GtkWidget *menu, *last;
    GtkItemFactory *item_factory;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<proc>",
                                        accel_group);
    gwy_process_func_build_menu(GTK_OBJECT(item_factory), "",
                                G_CALLBACK(gwy_app_run_process_func_cb));
    menu = gtk_item_factory_get_widget(item_factory, "<proc>");

    /* set up sensitivity: all items need an active data window */
    gwy_app_menu_set_flags_recursive(menu, &sens_data);

    /* re-run last item */
    menu = gtk_item_factory_get_widget(item_factory, "<proc>");

    last = gtk_menu_item_new_with_mnemonic(_("Re-show Last"));
    gtk_menu_item_set_accel_path(GTK_MENU_ITEM(last), reshow_accel_path);
    gtk_accel_map_add_entry(reshow_accel_path, GDK_f,
                            GDK_CONTROL_MASK | GDK_SHIFT_MASK);
    g_object_set_data(G_OBJECT(last), "show-last-item", GINT_TO_POINTER(TRUE));
    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), last, 1);
    gwy_app_menu_set_sensitive_both(last,
                                    GWY_MENU_FLAG_DATA
                                    | GWY_MENU_FLAG_LAST_PROC, 0);
    g_signal_connect_swapped(last, "activate",
                             G_CALLBACK(gwy_app_rerun_process_func_cb),
                             GUINT_TO_POINTER(GWY_RUN_MODAL));

    last = gtk_menu_item_new_with_mnemonic(_("Repeat Last"));
    gtk_menu_item_set_accel_path(GTK_MENU_ITEM(last), repeat_accel_path);
    gtk_accel_map_add_entry(repeat_accel_path, GDK_f,
                            GDK_CONTROL_MASK);
    g_object_set_data(G_OBJECT(last), "run-last-item", GINT_TO_POINTER(TRUE));
    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), last, 1);
    gwy_app_menu_set_sensitive_both(last,
                                    GWY_MENU_FLAG_DATA
                                    | GWY_MENU_FLAG_LAST_PROC, 0);
    g_signal_connect_swapped(last, "activate",
                             G_CALLBACK(gwy_app_rerun_process_func_cb),
                             GUINT_TO_POINTER(GWY_RUN_NONINTERACTIVE));

    gtk_accel_group_lock(gtk_menu_get_accel_group(GTK_MENU(menu)));

    return menu;
}

static GtkWidget*
gwy_app_menu_create_graph_menu(GtkAccelGroup *accel_group)
{
    GtkWidget *menu;
    GtkItemFactory *item_factory;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_GRAPH, 0 };

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<graph>", accel_group);
    gwy_graph_func_build_menu(GTK_OBJECT(item_factory), "",
                              G_CALLBACK(gwy_app_run_graph_func_cb));
    menu = gtk_item_factory_get_widget(item_factory, "<graph>");
    gtk_widget_show_all(menu);

    /* set up sensitivity: all items need an active graph window */
    gwy_app_menu_set_flags_recursive(menu, &sens_data);

    return menu;
}

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
            N_("/Module _Browser"),
            NULL,
            gwy_module_browser,
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
    static const gchar *items_need_data[] = {
        "/Metadata Browser", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<meta>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);
    menu = gtk_item_factory_get_widget(item_factory, "<meta>");
    gwy_app_menu_set_sensitive_array(item_factory, "meta", items_need_data,
                                     GWY_MENU_FLAG_DATA);

    return menu;
}

static GtkWidget*
gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items1[] = {
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
            gwy_app_file_open_cb,
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
            N_("/Open Recent/---"),
            NULL,
            NULL,
            0,
            "<Tearoff>",
            NULL
        },
        {
            N_("/_Save"),
            "<control>S",
            gwy_app_file_save_cb,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE
        },
        {
            N_("/Save _As"),
            "<control><shift>S",
            gwy_app_file_save_as_cb,
            0,
            "<StockItem>",
            GTK_STOCK_SAVE_AS
        },
    };
    static GtkItemFactoryEntry menu_items2[] = {
        {
            N_("/_Close"),
            "<control>W",
            gwy_app_file_close_cb,
            0,
            "<StockItem>",
            GTK_STOCK_CLOSE
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
    static const gchar *items_need_data[] = {
        "/Save", "/Save As", "/Close", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu, *item;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<file>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items1), menu_items1, NULL);
    gwy_file_func_build_menu(GTK_OBJECT(item_factory), N_("/_Export To"),
                             G_CALLBACK(gwy_app_file_export_cb), GWY_FILE_SAVE);
    gwy_file_func_build_menu(GTK_OBJECT(item_factory), N_("/_Import From"),
                             G_CALLBACK(gwy_app_file_import_cb), GWY_FILE_LOAD);
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items2), menu_items2, NULL);
    menu = gtk_item_factory_get_widget(item_factory, "<file>");

    /* set up sensitivity  */
    gwy_app_menu_set_sensitive_array(item_factory, "file", items_need_data,
                                     sens_data.flags);
    item = gtk_item_factory_get_item(item_factory, "<file>/Export To");
    gwy_app_menu_set_flags_recursive(item, &sens_data);
    gwy_app_menu_set_recent_files_menu(
        gtk_item_factory_get_widget(item_factory, "<file>/Open Recent"));

    return menu;
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
            N_("/_Duplicate"),
            "<control>D",
            gwy_app_file_duplicate_cb,
            0,
            "<StockItem>",
            GTK_STOCK_COPY 
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
            N_("/Remove _Mask"),
            "<control>K",
            gwy_app_mask_kill_cb,
            0,
            NULL,
            NULL 
        },
        {
            N_("/Remove _Presentation"),
            "<control><shift>K",
            gwy_app_show_kill_cb,
            0,
            NULL,
            NULL 
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
            N_("/Mask _Color..."),
            NULL,
            gwy_app_change_mask_color_cb,
            0,
            NULL,
            NULL 
        },
        {
            N_("/Default Mask _Color..."),
            NULL,
            gwy_app_change_mask_color_cb,
            1,
            NULL,
            NULL 
        },
        {
            N_("/Color _Gradients..."),
            NULL,
            gwy_app_gradient_editor,
            1,
            NULL,
            NULL 
        },
        {
            N_("/G_L Materials..."),
            NULL,
            gwy_app_gl_material_editor,
            1,
            NULL,
            NULL 
        },
    };
    static const gchar *items_need_data[] = {
        "/Duplicate", NULL
    };
    static const gchar *items_need_data_mask[] = {
        "/Remove Mask", "/Mask Color...", NULL
    };
    static const gchar *items_need_data_show[] = {
        "/Remove Presentation", NULL
    };
    static const gchar *items_need_undo[] = {
        "/Undo", NULL
    };
    static const gchar *items_need_redo[] = {
        "/Redo", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu;
    GwyMenuSensData sens_data;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<edit>", accel_group);
#ifdef ENABLE_NLS
    gtk_item_factory_set_translate_func(item_factory,
                                        (GtkTranslateFunc)&gettext,
                                        NULL, NULL);
#endif
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items), menu_items, NULL);
    menu = gtk_item_factory_get_widget(item_factory, "<edit>");

    /* set up sensitivity  */
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_undo,
                                     GWY_MENU_FLAG_UNDO);
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_redo,
                                     GWY_MENU_FLAG_REDO);
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_data,
                                     GWY_MENU_FLAG_DATA);
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_data_mask,
                                     GWY_MENU_FLAG_DATA_MASK);
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_data_show,
                                     GWY_MENU_FLAG_DATA_SHOW);
    sens_data.flags = GWY_MENU_FLAG_DATA
                      | GWY_MENU_FLAG_REDO
                      | GWY_MENU_FLAG_UNDO
                      | GWY_MENU_FLAG_DATA_MASK
                      | GWY_MENU_FLAG_DATA_SHOW;
    sens_data.set_to = 0;

    return menu;
}

static GtkWidget*
gwy_app_toolbox_create_label(const gchar *text,
                             const gchar *id,
                             gboolean *pvisible)
{
    GwyContainer *settings;
    GtkWidget *label, *hbox, *button, *arrow;
    gboolean visible = TRUE;
    gchar *s, *key;
    GQuark quark;

    settings = gwy_app_settings_get();
    key = g_strconcat("/app/toolbox/visible/", id, NULL);
    quark = g_quark_from_string(key);
    g_free(key);
    gwy_container_gis_boolean(settings, quark, &visible);

    hbox = gtk_hbox_new(FALSE, 2);

    arrow = gtk_arrow_new(visible ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                          GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start(GTK_BOX(hbox), arrow, FALSE, FALSE, 0);

    s = g_strconcat("<small>", text, "</small>", NULL);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    g_free(s);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    button = gtk_button_new();
    gtk_widget_set_name(button, "toolboxheader");
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_HALF);
    g_object_set(button, "can-focus", FALSE, "can-default", FALSE, NULL);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    g_object_set_data(G_OBJECT(button), "arrow", arrow);
    g_object_set_data(G_OBJECT(button), "key", GUINT_TO_POINTER(quark));

    *pvisible = visible;

    return button;
}

static void
gwy_app_toolbox_showhide_cb(GtkWidget *button,
                            GtkWidget *widget)
{
    GwyContainer *settings;
    GtkWidget *arrow;
    gboolean visible;
    GQuark quark;

    settings = gwy_app_settings_get();
    quark = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "key"));
    visible = gwy_container_get_boolean(settings, quark);
    arrow = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "arrow"));
    g_assert(GTK_IS_ARROW(arrow));
    visible = !visible;
    gwy_container_set_boolean(settings, quark, visible);

    if (visible)
        gtk_widget_show_all(widget);
    else
        gtk_widget_hide(widget);
    g_object_set(arrow, "arrow-type",
                 visible ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
                 NULL);
}

static void
gwy_app_rerun_process_func_cb(gpointer user_data)
{
    GtkWidget *menu;
    GwyRunType run, available_run_modes;
    gchar *name;

    menu = GTK_WIDGET(g_object_get_data(G_OBJECT(gwy_app_main_window_get()),
                                        "<proc>"));
    g_return_if_fail(menu);
    name = (gchar*)g_object_get_data(G_OBJECT(menu), "last-func");
    g_return_if_fail(name);
    run = GPOINTER_TO_UINT(user_data);
    available_run_modes = gwy_process_func_get_run_types(name);
    g_return_if_fail(available_run_modes);
    gwy_debug("run mode = %u, available = %u", run, available_run_modes);

    /* try to find some mode `near' to requested one, otherwise just use any */
    if (!(run & available_run_modes)) {
        if (run == GWY_RUN_NONINTERACTIVE
            && (available_run_modes & GWY_RUN_WITH_DEFAULTS))
            run = GWY_RUN_WITH_DEFAULTS;
        else
            run = 0;
    }
    if (run)
        gwy_app_run_process_func_in_mode(name, run);
    else
        gwy_app_run_process_func_cb(name);
}

static void
toolbox_dnd_data_received(G_GNUC_UNUSED GtkWidget *widget,
                          GdkDragContext *context,
                          G_GNUC_UNUSED gint x,
                          G_GNUC_UNUSED gint y,
                          GtkSelectionData *data,
                          G_GNUC_UNUSED guint info,
                          guint time,
                          G_GNUC_UNUSED gpointer user_data)
{
    gchar *filename;
    gchar **file_list;
    GwyContainer **containers;
    GtkWidget *data_window;
    gboolean ok = FALSE;
    gint i, n;

    if (data->length <= 0 || data->format != 8) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    file_list = g_strsplit((gchar*)data->data, "\n", 0);
    if (!file_list) {
        gtk_drag_finish(context, FALSE, FALSE, time);
        return;
    }

    /* Stop on an empty line too.
     * This (1) kills the last empty line (2) prevents some cases of total
     * bogus to be processed any further */
    for (n = 0; file_list[n] && file_list[n][0]; n++)
        ;

    containers = g_new0(GwyContainer*, n);
    for (i = 0; i < n; i++) {
        filename = g_strstrip(file_list[i]);
        if (g_str_has_prefix(filename, "file://"))
            filename += sizeof("file://") - 1;
        gwy_debug("filename = %s", filename);
        if (g_file_test(filename, G_FILE_TEST_IS_REGULAR
                                  | G_FILE_TEST_IS_SYMLINK)) {
            containers[i] = gwy_file_load(filename);
            ok = TRUE;    /* FIXME: what if we accept only some? */
        }
    }
    g_strfreev(file_list);
    gtk_drag_finish(context, ok, FALSE, time);
    for (i = 0; i < n; i++) {
        if (!containers[i])
            continue;
        data_window = gwy_app_data_window_create(containers[i]);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }
    g_free(containers);

    return;
}

static void
gwy_app_meta_browser(void)
{
    gwy_app_metadata_browser(gwy_app_data_window_get_current());
}

static void
delete_app_window(void)
{
    gboolean boo;

    g_signal_emit_by_name(gwy_app_main_window_get(), "delete_event",
                          NULL, &boo);
}

static void
gwy_app_undo_cb(void)
{
    GwyContainer *data;

    if ((data = gwy_data_window_get_data(gwy_app_data_window_get_current())))
        gwy_app_undo_undo_container(data);
}

static void
gwy_app_redo_cb(void)
{
    GwyContainer *data;

    if ((data = gwy_data_window_get_data(gwy_app_data_window_get_current())))
        gwy_app_undo_redo_container(data);
}

static void
gwy_app_gl_view_maybe_cb(void)
{
    static GtkWidget *dialog = NULL;

    if (gwy_app_gl_is_ok()) {
        gwy_app_3d_view_cb();
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
