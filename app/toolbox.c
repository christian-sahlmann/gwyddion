/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwytoolbox.h>
#include <libgwymodule/gwymodule.h>
#include "file.h"
#include "menu.h"
#include "app.h"
#include "settings.h"

#include "gwyddion.h"

static GtkWidget* gwy_menu_create_aligned_menu (GtkItemFactoryEntry *menu_items,
                                                gint nitems,
                                                const gchar *root_path,
                                                GtkAccelGroup *accel_group,
                                                GtkItemFactory **factory);
static GtkWidget* gwy_app_toolbox_create_label (const gchar *text,
                                                const gchar *id);
static void       gwy_app_toolbox_showhide_cb  (GtkWidget *button,
                                                GtkWidget *widget);
static void       gwy_app_rerun_process_func_cb (void);
static void       gwy_app_meta_browser         (void);
static void       delete_app_window            (void);

GtkWidget*
gwy_app_toolbox_create(void)
{
    GtkWidget *toolbox, *vbox, *toolbar, *menu, *label;
    GtkAccelGroup *accel_group;
    GSList *labels = NULL, *l;
    const gchar *first_tool;

    toolbox = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(toolbox), g_get_application_name());
    gtk_window_set_wmclass(GTK_WINDOW(toolbox), "toolbox",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(toolbox), FALSE);
    gwy_app_main_window_set(toolbox);

    accel_group = gtk_accel_group_new();
    g_object_set_data(G_OBJECT(toolbox), "accel_group", accel_group);

    vbox = gtk_vbox_new(0, FALSE);
    gtk_container_add(GTK_CONTAINER(toolbox), vbox);

    menu = gwy_app_menu_create_file_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(toolbox), "<file>", menu);

    menu = gwy_app_menu_create_edit_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(toolbox), "<edit>", menu);

    menu = gwy_app_menu_create_proc_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(toolbox), "<proc>", menu);

    menu = gwy_app_menu_create_graph_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(toolbox), "<graph>", menu);

    menu = gwy_app_menu_create_xtns_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(toolbox), "<xtns>", menu);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Zoom"), "zoom");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    labels = g_slist_append(labels, label);

    toolbar = gwy_toolbox_new(4);
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

    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Data Process"), "proc");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    labels = g_slist_append(labels, label);

    toolbar = gwy_toolbox_new(4);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Fix minimum value to zero"), NULL, GWY_STOCK_FIX_ZERO,
                       G_CALLBACK(gwy_app_run_process_func_cb), "fixzero");
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Automatically level data"), NULL, GWY_STOCK_FIT_PLANE,
                       G_CALLBACK(gwy_app_run_process_func_cb), "level");
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Facet-level data"), NULL, GWY_STOCK_FACET_LEVEL,
                       G_CALLBACK(gwy_app_run_process_func_cb), "facet_level");
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Scale data"), NULL, GWY_STOCK_SCALE,
                       G_CALLBACK(gwy_app_run_process_func_cb), "scale");
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Rotate by arbitrary angle"), NULL, GWY_STOCK_ROTATE,
                       G_CALLBACK(gwy_app_run_process_func_cb), "rotate");
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Shade data"), NULL, GWY_STOCK_SHADER,
                       G_CALLBACK(gwy_app_run_process_func_cb), "shade");
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Fast Fourier Transform"), NULL, GWY_STOCK_FFT,
                       G_CALLBACK(gwy_app_run_process_func_cb), "fft");
    gwy_toolbox_append(GWY_TOOLBOX(toolbar), GTK_TYPE_BUTTON, NULL,
                       _("Continuous Wavelet Transform"), NULL, GWY_STOCK_CWT,
                       G_CALLBACK(gwy_app_run_process_func_cb), "cwt");

    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    /***************************************************************/
    label = gwy_app_toolbox_create_label(_("Tools"), "tool");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    labels = g_slist_append(labels, label);

    toolbar = gwy_tool_func_build_toolbox(G_CALLBACK(gwy_app_tool_use_cb),
                                          4, &first_tool);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    g_signal_connect(label, "clicked",
                     G_CALLBACK(gwy_app_toolbox_showhide_cb), toolbar);

    gwy_app_tool_use_cb(first_tool, NULL);

    /***************************************************************/
    gtk_widget_show_all(toolbox);
    for (l = labels; l; l = g_slist_next(l))
        g_signal_emit_by_name(l->data, "clicked");
    g_slist_free(labels);
    gtk_window_add_accel_group(GTK_WINDOW(toolbox), accel_group);

    /* XXX */
    g_signal_connect(toolbox, "delete_event", G_CALLBACK(gwy_app_quit), NULL);

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
    GtkWidget *menu, *alignment, *last;
    GtkItemFactory *item_factory;
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_DATA, 0 };

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
    last = gtk_menu_item_new_with_mnemonic(_("_Last Used"));
    g_object_set_data(G_OBJECT(last), "run-last-item", GINT_TO_POINTER(TRUE));
    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), last, 1);
    gwy_app_menu_set_sensitive_both(last,
                                    GWY_MENU_FLAG_DATA
                                    | GWY_MENU_FLAG_LAST_PROC, 0);
    g_signal_connect(last, "activate",
                     G_CALLBACK(gwy_app_rerun_process_func_cb), NULL);

    return alignment;
}

GtkWidget*
gwy_app_menu_create_graph_menu(GtkAccelGroup *accel_group)
{
    GtkWidget *menu, *alignment;
    GtkItemFactory *item_factory;
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_GRAPH, 0 };

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<graph>",
                                        accel_group);
    gwy_graph_func_build_menu(GTK_OBJECT(item_factory), "/_Graph",
                              G_CALLBACK(gwy_app_run_graph_func_cb));
    menu = gtk_item_factory_get_widget(item_factory, "<graph>");
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), menu);

    /* set up sensitivity: all items need an active data window */
    gwy_app_menu_set_flags_recursive(menu, &sens_data);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    return alignment;
}

GtkWidget*
gwy_app_menu_create_xtns_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/E_xterns", NULL, NULL, 0, "<Branch>", NULL },
        { "/Externs/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/Externs/Module Browser", NULL, gwy_module_browser, 0, "<Item>", NULL },
        { "/Externs/Metadata Browser", NULL, gwy_app_meta_browser, 0, "<Item>", NULL },
        { "/Externs/---", NULL, NULL, 0, "<Separator>", NULL },
        { "/Externs/About", NULL, gwy_app_about, 0, "<Item>", NULL },
    };
    static const gchar *items_need_data[] = {
        "/Externs/Metadata Browser", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu;
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<xtns>", accel_group, &item_factory);
    gwy_app_menu_set_sensitive_array(item_factory, "xtns", items_need_data,
                                     GWY_MENU_FLAG_DATA);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    return menu;
}

GtkWidget*
gwy_app_menu_create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items1[] = {
        { "/_File", NULL, NULL, 0, "<Branch>", NULL },
        { "/File/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/File/_Open", "<control>O", gwy_app_file_open_cb, 0, "<StockItem>", GTK_STOCK_OPEN },
        { "/File/Open _Recent", NULL, NULL, 0, "<Branch>", NULL },
        { "/File/Open Recent/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/File/_Save", "<control>S", gwy_app_file_save_cb, 0, "<StockItem>", GTK_STOCK_SAVE },
        { "/File/Save _As", "<control><shift>S", gwy_app_file_save_as_cb, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
    };
    static GtkItemFactoryEntry menu_items2[] = {
        { "/File/_Close", "<control>W", gwy_app_file_close_cb, 0, "<StockItem>", GTK_STOCK_CLOSE },
        { "/File/---", NULL, NULL, 0, "<Separator>", NULL },
        { "/File/_Quit", "<control>Q", delete_app_window, 0, "<StockItem>", GTK_STOCK_QUIT },
    };
    static const gchar *items_need_data[] = {
        "/File/Save", "/File/Save As", "/File/Close", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *alignment, *menu, *item;
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_DATA, 0 };

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
        { "/_Edit", NULL, NULL, 0, "<Branch>", NULL },
        { "/Edit/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/Edit/_Undo", "<control>Z", gwy_app_undo_undo, 0, "<StockItem>", GTK_STOCK_UNDO },
        { "/Edit/_Redo", "<control>R", gwy_app_undo_redo, 0, "<StockItem>", GTK_STOCK_REDO },
        { "/Edit/_Duplicate", "<control>D", gwy_app_file_duplicate_cb, 0, "<StockItem>", GTK_STOCK_COPY },
        { "/Edit/Data _Arithmetic", NULL, gwy_app_data_arith, 0, NULL, NULL },
        { "/Edit/---", NULL, NULL, 0, "<Separator>", NULL },
        { "/Edit/Remove _Mask", NULL, gwy_app_mask_kill_cb, 0, NULL, NULL },
        { "/Edit/Remove _Presentation", NULL, gwy_app_show_kill_cb, 0, NULL, NULL },
        { "/Edit/Change Mask _Color", NULL, gwy_app_change_mask_color_cb, 0, NULL, NULL },
        { "/Edit/Change Default Mask _Color", NULL, gwy_app_change_mask_color_cb, 1, NULL, NULL },
    };
    static const gchar *items_need_data[] = {
        "/Edit/Duplicate", "/Edit/Data Arithmetic",
        "/Edit/Remove Mask", "/Edit/Remove Presentation",
        "/Edit/Change Mask Color", NULL
    };
    static const gchar *items_need_undo[] = {
        "/Edit/Undo", NULL
    };
    static const gchar *items_need_redo[] = {
        "/Edit/Redo", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu;
    GwyMenuSensitiveData sens_data;

    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<edit>", accel_group, &item_factory);

    /* set up sensitivity  */
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_undo,
                                     GWY_MENU_FLAG_UNDO);
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_redo,
                                     GWY_MENU_FLAG_REDO);
    gwy_app_menu_set_sensitive_array(item_factory, "edit", items_need_data,
                                     GWY_MENU_FLAG_DATA);
    sens_data.flags = GWY_MENU_FLAG_DATA
                      | GWY_MENU_FLAG_REDO
                      | GWY_MENU_FLAG_UNDO;
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
gwy_app_rerun_process_func_cb(void)
{
    GtkWidget *menu;
    gchar *name;

    menu = GTK_WIDGET(g_object_get_data(G_OBJECT(gwy_app_main_window_get()),
                                        "<proc>"));
    g_return_if_fail(menu);
    name = (gchar*)g_object_get_data(G_OBJECT(menu), "last-func");
    g_return_if_fail(name);
    gwy_app_run_process_func_cb(name);
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
