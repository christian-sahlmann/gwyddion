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

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwytoolbox.h>
#include <libgwymodule/gwymodule.h>
#include "gwyapp.h"

#include "gwyappinternal.h"
#include "gwyddion.h"

enum {
    DND_TARGET_STRING = 1,
};

static GtkWidget* gwy_menu_create_aligned_menu (GtkItemFactoryEntry *menu_items,
                                                gint nitems,
                                                const gchar *root_path,
                                                GtkAccelGroup *accel_group,
                                                GtkItemFactory **factory);
static GtkWidget* gwy_app_toolbox_create_label (const gchar *text,
                                                const gchar *id);
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

static GtkTargetEntry dnd_target_table[] = {
  { "STRING",     0, DND_TARGET_STRING },
  { "text/plain", 0, DND_TARGET_STRING },
};

/* append item only if function exists */
static inline GtkWidget*
toolbox_append_proc_func(GtkWidget *toolbox,
                            const char *tooltip_text,
                            const gchar *stock_id,
                            const gchar *name)
{
    if (!gwy_process_func_get_run_types(name))
        return NULL;
    return gwy_toolbox_append(GWY_TOOLBOX(toolbox), GTK_TYPE_BUTTON, NULL,
                              tooltip_text, NULL, stock_id,
                              G_CALLBACK(gwy_app_run_process_func_cb),
                              (gpointer)name);
}

static inline GtkWidget*
toolbox_append_graph_func(GtkWidget *toolbox,
                          const char *tooltip_text,
                          const gchar *stock_id,
                          const gchar *name)
{
    if (!gwy_graph_func_exists(name))
        return NULL;
    return gwy_toolbox_append(GWY_TOOLBOX(toolbox), GTK_TYPE_BUTTON, NULL,
                              tooltip_text, NULL, stock_id,
                              G_CALLBACK(gwy_app_run_graph_func_cb),
                              (gpointer)name);
}

GtkWidget*
gwy_app_toolbox_create(gboolean gl_ok)
{
    GwyMenuSensData sens_data_data = { GWY_MENU_FLAG_DATA, 0 };
    GwyMenuSensData sens_data_graph = { GWY_MENU_FLAG_GRAPH, 0 };
    GtkWidget *toolbox, *vbox, *toolbar, *menu, *label, *button;
    GtkAccelGroup *accel_group;
    GList *list;
    GSList *labels = NULL, *l;
    GSList *toolbars = NULL;    /* list of all toolbars for sensitivity */
    GSList *menus = NULL;    /* list of all menus for sensitivity */
    const gchar *first_tool;

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

    menu = gwy_app_menu_create_file_menu(accel_group);
    menus = g_slist_append(menus, menu);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

    menu = gwy_app_menu_create_edit_menu(accel_group);
    menus = g_slist_append(menus, menu);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

    menu = gwy_app_menu_create_proc_menu(accel_group);
    menus = g_slist_append(menus, menu);
    g_object_set_data(G_OBJECT(toolbox), "<proc>", menu);     /* XXX */
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

    menu = gwy_app_menu_create_graph_menu(accel_group);
    menus = g_slist_append(menus, menu);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

    menu = gwy_app_menu_create_meta_menu(accel_group);
    menus = g_slist_append(menus, menu);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("View"), "zoom");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    labels = g_slist_append(labels, label);

    toolbar = gwy_toolbox_new(4);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Zoom in"), NULL, GWY_STOCK_ZOOM_IN,
                       G_CALLBACK(gwy_app_zoom_set_cb), GINT_TO_POINTER(1));
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Zoom 1:1"), NULL, GWY_STOCK_ZOOM_1_1,
                       G_CALLBACK(gwy_app_zoom_set_cb), GINT_TO_POINTER(10000));
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Zoom out"), NULL, GWY_STOCK_ZOOM_OUT,
                       G_CALLBACK(gwy_app_zoom_set_cb), GINT_TO_POINTER(-1));
    button = gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                                _("Display a 3D view of data"), NULL,
                                GWY_STOCK_3D_BASE,
                                G_CALLBACK(gwy_app_3d_view_cb), NULL);

    gwy_app_menu_set_flags_recursive(toolbar, &sens_data_data);
    gwy_app_menu_set_sensitive_recursive(toolbar, &sens_data_data);
    gwy_app_menu_set_sensitive_both(button,
                                    GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_GL_OK,
                                    gl_ok ? GWY_MENU_FLAG_GL_OK : 0);

    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Data Process"), "proc");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    labels = g_slist_append(labels, label);

    toolbar = gwy_toolbox_new(4);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    toolbox_append_proc_func(toolbar, _("Fix minimum value to zero"),
                             GWY_STOCK_FIX_ZERO, "fixzero");
    toolbox_append_proc_func(toolbar, _("Scale data"),
                             GWY_STOCK_SCALE, "scale");
    toolbox_append_proc_func(toolbar, _("Rotate by arbitrary angle"),
                             GWY_STOCK_ROTATE, "rotate");
    toolbox_append_proc_func(toolbar, _("Automatically correct rotation"),
                             GWY_STOCK_UNROTATE, "unrotate");
    toolbox_append_proc_func(toolbar, _("Automatically level data"),
                             GWY_STOCK_FIT_PLANE, "level");
    toolbox_append_proc_func(toolbar, _("Facet-level data"),
                             GWY_STOCK_FACET_LEVEL, "facet_level");
    toolbox_append_proc_func(toolbar, _("Fast Fourier Transform"),
                             GWY_STOCK_FFT, "fft");
    toolbox_append_proc_func(toolbar, _("Continuous Wavelet Transform"),
                             GWY_STOCK_CWT, "cwt");
    toolbox_append_proc_func(toolbar, _("Mark grains by threshold"),
                             GWY_STOCK_GRAINS, "mark_threshold");
    toolbox_append_proc_func(toolbar, _("Mark grains by watershed"),
                             GWY_STOCK_GRAINS_WATER, "wshed_threshold");
    button = toolbox_append_proc_func(toolbar, _("Remove grains by threshold"),
                                      GWY_STOCK_GRAINS_REMOVE,
                                      "remove_threshold");
    if (button)
        gwy_app_menu_set_sensitive_both(button, GWY_MENU_FLAG_DATA_MASK, 0);
    button = toolbox_append_proc_func(toolbar, _("Grain distribution"),
                                      GWY_STOCK_GRAINS_GRAPH, "grain_dist");
    if (button)
        gwy_app_menu_set_sensitive_both(button, GWY_MENU_FLAG_DATA_MASK, 0);
    toolbox_append_proc_func(toolbar, _("Calculate fractal dimension"),
                             GWY_STOCK_FRACTAL, "fractal");
    toolbox_append_proc_func(toolbar, _("Shade data"),
                             GWY_STOCK_SHADER, "shade");

    gwy_app_menu_set_flags_recursive(toolbar, &sens_data_data);
    gwy_app_menu_set_sensitive_recursive(toolbar, &sens_data_data);
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Graph"), "graph");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    labels = g_slist_append(labels, label);

    toolbar = gwy_toolbox_new(4);
    toolbars = g_slist_append(toolbars, toolbar);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    toolbox_append_graph_func(toolbar, _("Read coordinates"),
                              GWY_STOCK_GRAPH_POINTER, "read");
    toolbox_append_graph_func(toolbar, _("Zoom a part of graph"),
                              GWY_STOCK_GRAPH_ZOOM_IN, "graph_zoom");
    toolbox_append_graph_func(toolbar, _("Reset zoom to display complete data"),
                              GWY_STOCK_GRAPH_ZOOM_FIT, "graph_unzoom");
    toolbox_append_graph_func(toolbar, _("Measure graph point distances"),
                              GWY_STOCK_GRAPH_RULER, "graph_points");
    toolbox_append_graph_func(toolbar, _("Fit critical dimension"),
                              GWY_STOCK_GRAPH_MEASURE, "graph_cd");
    toolbox_append_graph_func(toolbar, _("Fit functions to graph data"),
                              GWY_STOCK_GRAPH_FIT_FUNC, "graph_fit");

    gwy_app_menu_set_flags_recursive(toolbar, &sens_data_graph);
    gwy_app_menu_set_sensitive_recursive(toolbar, &sens_data_graph);
    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Tools"), "tool");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    labels = g_slist_append(labels, label);

    toolbar = gwy_tool_func_build_toolbox(G_CALLBACK(gwy_app_tool_use_cb),
                                          4, &first_tool);
    toolbars = g_slist_append(toolbars, toolbar);
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
    g_signal_connect(toolbox, "drag_data_received",
                     G_CALLBACK(toolbox_dnd_data_received), NULL);

    /***************************************************************/
    /* XXX */
    g_signal_connect(toolbox, "delete_event",
                     G_CALLBACK(gwy_app_main_window_save_position), NULL);
    g_signal_connect(toolbox, "delete_event", G_CALLBACK(gwy_app_quit), NULL);

    gtk_window_add_accel_group(GTK_WINDOW(toolbox), accel_group);

    gtk_widget_show_all(toolbox);
    gwy_app_main_window_restore_position();
    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    for (l = labels; l; l = g_slist_next(l))
        g_signal_emit_by_name(l->data, "clicked");
    g_slist_free(labels);

    g_object_set_data_full(G_OBJECT(toolbox), "toolbars", toolbars,
                           (GDestroyNotify)g_list_free);
    g_object_set_data_full(G_OBJECT(toolbox), "menus", menus,
                           (GDestroyNotify)g_list_free);

    return toolbox;
}

/*************************************************************************/
static GtkWidget*
gwy_menu_create_aligned_menu(GtkItemFactoryEntry *menu_items,
                             gint nitems,
                             const gchar *root_path,
                             GtkAccelGroup *accel_group,
                             GtkItemFactory **factory)
{
    GtkItemFactory *item_factory;
    GtkWidget *widget, *alignment;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, root_path,
                                        accel_group);
    gtk_item_factory_create_items(item_factory, nitems, menu_items, NULL);
    widget = gtk_item_factory_get_widget(item_factory, root_path);
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), widget);
    if (factory)
        *factory = item_factory;

    return alignment;
}

GtkWidget*
gwy_app_menu_create_proc_menu(GtkAccelGroup *accel_group)
{
    static const gchar *reshow_accel_path = "<proc>/Data Process/Re-show Last";
    static const gchar *repeat_accel_path = "<proc>/Data Process/Repeat Last";
    GtkWidget *menu, *alignment, *last;
    GtkItemFactory *item_factory;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<proc>",
                                        accel_group);
    gwy_process_func_build_menu(GTK_OBJECT(item_factory), "/_Data Process",
                                G_CALLBACK(gwy_app_run_process_func_cb));
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    menu = gtk_item_factory_get_widget(item_factory, "<proc>");
    gtk_container_add(GTK_CONTAINER(alignment), menu);

    /* set up sensitivity: all items need an active data window */
    gwy_app_menu_set_flags_recursive(menu, &sens_data);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    /* re-run last item */
    menu = gtk_item_factory_get_widget(item_factory, "<proc>/Data Process");

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

    return alignment;
}

GtkWidget*
gwy_app_menu_create_graph_menu(GtkAccelGroup *accel_group)
{
    GtkWidget *menu, *alignment;
    GtkItemFactory *item_factory;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_GRAPH, 0 };

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<graph>",
                                        accel_group);
    gwy_graph_func_build_menu(GTK_OBJECT(item_factory), "/_Graph",
                              G_CALLBACK(gwy_app_run_graph_func_cb));
    menu = gtk_item_factory_get_widget(item_factory, "<graph>");
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), menu);

    /* set up sensitivity: all items need an active graph window */
    gwy_app_menu_set_flags_recursive(menu, &sens_data);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    return alignment;
}

GtkWidget*
gwy_app_menu_create_meta_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_Meta", NULL,
            NULL, 0, "<Branch>", NULL },
        { "/Meta/---", NULL,
            NULL, 0, "<Tearoff>", NULL },
        { "/Meta/Module _Browser", NULL,
            gwy_module_browser, 0, "<Item>", NULL },
        { "/Meta/_Metadata Browser", NULL,
            gwy_app_meta_browser, 0, "<Item>", NULL },
        { "/Meta/---", NULL, NULL, 0,
            "<Separator>", NULL },
        { "/Meta/_About Gwyddion", NULL,
            gwy_app_about, 0, "<Item>", NULL },
    };
    static const gchar *items_need_data[] = {
        "/Meta/Metadata Browser", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<meta>", accel_group, &item_factory);
    gwy_app_menu_set_sensitive_array(item_factory, "meta", items_need_data,
                                     GWY_MENU_FLAG_DATA);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    return menu;
}

GtkWidget*
gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items1[] = {
        { "/_File", NULL,
            NULL, 0, "<Branch>", NULL },
        { "/File/---", NULL,
            NULL, 0, "<Tearoff>", NULL },
        { "/File/_Open", "<control>O",
            gwy_app_file_open_cb, 0, "<StockItem>", GTK_STOCK_OPEN },
        { "/File/Open _Recent", NULL,
            NULL, 0, "<Branch>", NULL },
        { "/File/Open Recent/---", NULL,
            NULL, 0, "<Tearoff>", NULL },
        { "/File/_Save", "<control>S",
            gwy_app_file_save_cb, 0, "<StockItem>", GTK_STOCK_SAVE },
        { "/File/Save _As", "<control><shift>S",
            gwy_app_file_save_as_cb, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
    };
    static GtkItemFactoryEntry menu_items2[] = {
        { "/File/_Close", "<control>W",
            gwy_app_file_close_cb, 0, "<StockItem>", GTK_STOCK_CLOSE },
        { "/File/---", NULL, NULL, 0,
            "<Separator>", NULL },
        { "/File/_Quit", "<control>Q",
            delete_app_window, 0, "<StockItem>", GTK_STOCK_QUIT },
    };
    static const gchar *items_need_data[] = {
        "/File/Save", "/File/Save As", "/File/Close", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *alignment, *menu, *item;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<file>",
                                        accel_group);
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items1), menu_items1, NULL);
    gwy_file_func_build_menu(GTK_OBJECT(item_factory), "/File/_Export To",
                             G_CALLBACK(gwy_app_file_export_cb), GWY_FILE_SAVE);
    gwy_file_func_build_menu(GTK_OBJECT(item_factory), "/File/_Import From",
                             G_CALLBACK(gwy_app_file_import_cb), GWY_FILE_LOAD);
    gtk_item_factory_create_items(item_factory,
                                  G_N_ELEMENTS(menu_items2), menu_items2, NULL);
    menu = gtk_item_factory_get_widget(item_factory, "<file>");
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), menu);

    /* set up sensitivity  */
    gwy_app_menu_set_sensitive_array(item_factory, "file", items_need_data,
                                     sens_data.flags);
    item = gtk_item_factory_get_item(item_factory, "<file>/File/Export To");
    gwy_app_menu_set_flags_recursive(item, &sens_data);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);
    gwy_app_menu_set_recent_files_menu(
        gtk_item_factory_get_widget(item_factory, "<file>/File/Open Recent"));

    return alignment;
}

GtkWidget*
gwy_app_menu_create_edit_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_Edit", NULL,
            NULL, 0, "<Branch>", NULL },
        { "/Edit/---", NULL,
            NULL, 0, "<Tearoff>", NULL },
        { "/Edit/_Undo", "<control>Z",
            gwy_app_undo_undo, 0, "<StockItem>", GTK_STOCK_UNDO },
        { "/Edit/_Redo", "<control>Y",
            gwy_app_undo_redo, 0, "<StockItem>", GTK_STOCK_REDO },
        { "/Edit/_Duplicate", "<control>D",
            gwy_app_file_duplicate_cb, 0, "<StockItem>", GTK_STOCK_COPY },
        { "/Edit/---", NULL,
            NULL, 0, "<Separator>", NULL },
        { "/Edit/Remove _Mask", "<control>K",
            gwy_app_mask_kill_cb, 0, NULL, NULL },
        { "/Edit/Remove _Presentation", NULL,
            gwy_app_show_kill_cb, 0, NULL, NULL },
        { "/Edit/Mask _Color", NULL,
            gwy_app_change_mask_color_cb, 0, NULL, NULL },
        { "/Edit/Default Mask _Color", NULL,
            gwy_app_change_mask_color_cb, 1, NULL, NULL },
        /*
        { "/Edit/---", NULL,
            NULL, 0, "<Separator>", NULL },
        { "/Edit/Palette _Z-Range", NULL,
            gwy_app_change_palette_range_cb, 0, NULL, NULL },
            */
    };
    static const gchar *items_need_data[] = {
        "/Edit/Duplicate", NULL
    };
    static const gchar *items_need_data_mask[] = {
        "/Edit/Remove Mask", "/Edit/Mask Color", NULL
    };
    static const gchar *items_need_data_show[] = {
        "/Edit/Remove Presentation", NULL
    };
    static const gchar *items_need_undo[] = {
        "/Edit/Undo", NULL
    };
    static const gchar *items_need_redo[] = {
        "/Edit/Redo", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu;
    GwyMenuSensData sens_data;

    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<edit>", accel_group, &item_factory);

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
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    return menu;
}

static GtkWidget*
gwy_app_toolbox_create_label(const gchar *text,
                             const gchar *id)
{
    GwyContainer *settings;
    GtkWidget *label, *hbox, *button, *arrow;
    gboolean visible = TRUE;
    gchar *s, *key;

    settings = gwy_app_settings_get();
    key = g_strconcat("/app/toolbox/visible/", id, NULL);
    if (gwy_container_contains_by_name(settings, key))
        visible = gwy_container_get_boolean_by_name(settings, key);
    gwy_container_set_boolean_by_name(settings, key, !visible);

    hbox = gtk_hbox_new(FALSE, 2);

    /* note we create the label in the OTHER state, then call showhide */
    arrow = gtk_arrow_new(visible ? GTK_ARROW_RIGHT : GTK_ARROW_DOWN,
                          GTK_SHADOW_ETCHED_IN);
    gtk_box_pack_start(GTK_BOX(hbox), arrow, FALSE, FALSE, 0);

    s = g_strconcat("<small>", text, "</small>", NULL);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), s);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    g_free(s);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_HALF);
    g_object_set(button, "can-focus", FALSE, NULL);
    gtk_container_add(GTK_CONTAINER(button), hbox);

    g_object_set_data(G_OBJECT(button), "arrow", arrow);
    g_object_set_data(G_OBJECT(button), "key", key);
    g_signal_connect_swapped(button, "destroy", G_CALLBACK(g_free), key);

    return button;
}

static void
gwy_app_toolbox_showhide_cb(GtkWidget *button,
                            GtkWidget *widget)
{
    GwyContainer *settings;
    GtkWidget *arrow;
    gboolean visible;
    const gchar *key;

    settings = gwy_app_settings_get();
    key = (const gchar*)g_object_get_data(G_OBJECT(button), "key");
    visible = gwy_container_get_boolean_by_name(settings, key);
    arrow = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "arrow"));
    g_assert(GTK_IS_ARROW(arrow));
    visible = !visible;
    gwy_container_set_boolean_by_name(settings, key, visible);

    if (visible)
        gtk_widget_show(widget);
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
        if (run == GWY_RUN_MODAL
            && (available_run_modes & GWY_RUN_INTERACTIVE))
            run = GWY_RUN_INTERACTIVE;
        else if (run == GWY_RUN_NONINTERACTIVE
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
