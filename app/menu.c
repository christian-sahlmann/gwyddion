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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwymodule/gwymodulebrowser.h>
#include "app.h"
#include "file.h"
#include "arith.h"
#include "meta.h"
#include "about.h"
#include "menu.h"

#define set_sensitive(item, sens) \
    g_object_set_qdata(G_OBJECT(item), sensitive_key, \
                       GUINT_TO_POINTER(sens))
#define set_sensitive_state(item, state) \
    g_object_set_qdata(G_OBJECT(item), sensitive_state_key, \
                       GUINT_TO_POINTER(state))
#define set_sensitive_both(item, sens, state) \
    do { \
        set_sensitive(item, sens); \
        set_sensitive_state(item, state); \
    } while (0)

static void       gwy_app_rerun_process_func_cb (void);
static void       gwy_app_update_last_process_func (GtkWidget *menu,
                                                    const gchar *name);
static void       setup_sensitivity_keys       (void);
static void       gwy_menu_set_flags_recursive (GtkWidget *widget,
                                                GwyMenuSensitiveData *data);
static void       gwy_menu_set_sensitive_array (GtkItemFactory *item_factory,
                                                const gchar *root,
                                                const gchar **items,
                                                guint flags);
static GtkWidget* gwy_menu_create_aligned_menu (GtkItemFactoryEntry *menu_items,
                                                gint nitems,
                                                const gchar *root_path,
                                                GtkAccelGroup *accel_group,
                                                GtkItemFactory **factory);
static void       gwy_app_meta_browser         (void);
static void       delete_app_window            (void);
static void       gwy_app_kill_mask_cb         (void);
static void       gwy_app_kill_show_cb         (void);

static GQuark sensitive_key = 0;
static GQuark sensitive_state_key = 0;

static GtkWidget *recent_files_menu = NULL;

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
    setup_sensitivity_keys();
    gwy_menu_set_flags_recursive(menu, &sens_data);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    /* re-run last item */
    menu = gtk_item_factory_get_widget(item_factory, "<proc>/Data Process");
    last = gtk_menu_item_new_with_mnemonic(_("_Last Used"));
    g_object_set_data(G_OBJECT(last), "run-last-item", GINT_TO_POINTER(TRUE));
    gtk_menu_shell_insert(GTK_MENU_SHELL(menu), last, 1);
    set_sensitive_both(last, GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_LAST_PROC, 0);
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
    setup_sensitivity_keys();
    gwy_menu_set_flags_recursive(menu, &sens_data);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    return alignment;
}

GtkWidget*
gwy_app_menu_create_xtns_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/E_xterns", NULL, NULL, 0, "<Branch>", NULL },
        { "/Externs/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/Externs/Module browser", NULL, gwy_module_browser, 0, "<Item>", NULL },
        { "/Externs/Metadata browser", NULL, gwy_app_meta_browser, 0, "<Item>", NULL },
        { "/Externs/---", NULL, NULL, 0, "<Separator>", NULL },
        { "/Externs/About", NULL, gwy_app_about, 0, "<Item>", NULL },
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu, *item;
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    setup_sensitivity_keys();
    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<xtns>", accel_group, &item_factory);
    item = gtk_item_factory_get_item(item_factory,
                                     "<xtns>/Externs/Metadata browser");
    set_sensitive(item, sens_data.flags);
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
    setup_sensitivity_keys();
    gwy_menu_set_sensitive_array(item_factory, "file", items_need_data,
                                 sens_data.flags);
    item = gtk_item_factory_get_item(item_factory, "<file>/File/Export To");
    gwy_menu_set_flags_recursive(item, &sens_data);
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    recent_files_menu = gtk_item_factory_get_widget(item_factory,
                                                    "<file>/File/Open Recent");

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
        { "/Edit/Remove _Mask", NULL, gwy_app_kill_mask_cb, 0, NULL, NULL },
        { "/Edit/Remove _Presentation", NULL, gwy_app_kill_show_cb, 0, NULL, NULL },
        { "/Edit/Change Mask _Color", NULL, gwy_app_change_mask_color_cb, 0, NULL, NULL },
        { "/Edit/Change Default Mask _Color", NULL, gwy_app_change_mask_color_cb, 1, NULL, NULL },
    };
    static const gchar *items_need_data[] = {
        "/Edit/Duplicate", "/Edit/Data Arithmetic",
        "/Edit/Remove Mask", "/Edit/Remove Presentation",
        "/Edit/Change Mask Color", NULL
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu, *item;
    GwyMenuSensitiveData sens_data;

    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<edit>", accel_group, &item_factory);

    /* set up sensitivity  */
    setup_sensitivity_keys();
    item = gtk_item_factory_get_item(item_factory, "<edit>/Edit/Undo");
    set_sensitive(item, GWY_MENU_FLAG_UNDO);
    item = gtk_item_factory_get_item(item_factory, "<edit>/Edit/Redo");
    set_sensitive(item, GWY_MENU_FLAG_REDO);
    gwy_menu_set_sensitive_array(item_factory, "edit", items_need_data,
                                 GWY_MENU_FLAG_DATA);
    sens_data.flags = GWY_MENU_FLAG_DATA
                      | GWY_MENU_FLAG_REDO
                      | GWY_MENU_FLAG_UNDO;
    sens_data.set_to = 0;
    gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

    return menu;
}

static void
gwy_menu_set_sensitive_array(GtkItemFactory *item_factory,
                             const gchar *root,
                             const gchar **items,
                             guint flags)
{
    GtkWidget *item;
    gsize i, len, maxlen;
    gchar *path;

    g_return_if_fail(GTK_IS_ITEM_FACTORY(item_factory));
    g_return_if_fail(root);
    g_return_if_fail(items);

    maxlen = 0;
    for (i = 0; items[i]; i++) {
        len = strlen(items[i]);
        if (len > maxlen)
            maxlen = len;
    }

    len = strlen(root);
    path = g_new(gchar, maxlen + len + 3);
    strcpy(path + 1, root);
    path[0] = '<';
    path[len+1] = '>';
    for (i = 0; items[i]; i++) {
        strcpy(path + len + 2, items[i]);
        item = gtk_item_factory_get_item(item_factory, path);
        set_sensitive(item, flags);
    }
    g_free(path);
}

void
gwy_app_menu_set_sensitive_recursive(GtkWidget *widget,
                                     GwyMenuSensitiveData *data)
{
    GObject *obj;
    guint i, j;

    obj = G_OBJECT(widget);
    i = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_key));
    if (i & data->flags) {
        j = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_state_key));
        j = (j & ~data->flags) | (data->set_to & data->flags);
        set_sensitive_state(obj, j);
        gtk_widget_set_sensitive(widget, (j & i) == i);

    }
    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU_BAR(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              data);
    }
    else if (GTK_IS_MENU_ITEM(widget)
             && (widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_app_menu_set_sensitive_recursive,
                              data);
}

static void
gwy_menu_set_flags_recursive(GtkWidget *widget,
                             GwyMenuSensitiveData *data)
{
    if (!GTK_IS_TEAROFF_MENU_ITEM(widget))
        set_sensitive_both(widget, data->flags, data->set_to);

    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU_BAR(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_menu_set_flags_recursive,
                              data);
    }
    else if (GTK_IS_MENU_ITEM(widget)) {
        if ((widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
            gtk_container_foreach(GTK_CONTAINER(widget),
                                  (GtkCallback)gwy_menu_set_flags_recursive,
                                  data);
    }
}

static void
setup_sensitivity_keys(void)
{
    if (!sensitive_key)
        sensitive_key = g_quark_from_static_string("sensitive");
    if (!sensitive_state_key)
        sensitive_state_key = g_quark_from_static_string("sensitive-state");
}

void
gwy_app_run_process_func_cb(gchar *name)
{
    GwyRunType run_types[] = {
        GWY_RUN_INTERACTIVE, GWY_RUN_MODAL,
        GWY_RUN_NONINTERACTIVE, GWY_RUN_WITH_DEFAULTS,
    };
    GwyMenuSensitiveData sens_data = {
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_LAST_PROC,
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_LAST_PROC
    };
    GwyRunType run;
    GwyDataWindow *data_window;
    GtkWidget *data_view, *menu;
    GwyContainer *data;
    gsize i;

    gwy_debug("`%s'", name);
    data_window = gwy_app_data_window_get_current();
    if (!data_window)
        return;
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    g_return_if_fail(data);
    run = gwy_process_func_get_run_types(name);
    for (i = 0; i < G_N_ELEMENTS(run_types); i++) {
        if (run & run_types[i]) {
            gwy_process_func_run(name, data, run_types[i]);
            /* FIXME: the ugliest hack! */
            gwy_app_data_view_update(data_view);
            menu = GTK_WIDGET(g_object_get_data(G_OBJECT(gwy_app_main_window),
                                                "<proc>"));
            gwy_app_update_last_process_func(menu, name);
            gwy_app_menu_set_sensitive_recursive(menu, &sens_data);

            return;
        }
    }
    g_critical("Trying to run `%s', but no run mode found (%d)", name, run);
}

static void
gwy_app_update_last_process_func(GtkWidget *menu,
                                 const gchar *name)
{
    static GtkWidget *label = NULL;
    GtkWidget *item;
    const gchar *menu_path;
    gsize len, i, j;
    gchar *s, *mp;
    GList *l;

    g_object_set_data(G_OBJECT(menu), "last-func", (gpointer)name);
    /* Find the "run-last-item" menu item
     * FIXME: this is very fragile */
    if (!label) {
        while (GTK_IS_BIN(menu))
            menu = GTK_BIN(menu)->child;
        item = GTK_WIDGET(GTK_MENU_SHELL(menu)->children->data);
        menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
        for (l = GTK_MENU_SHELL(menu)->children; l; l = g_list_next(l)) {
            if (g_object_get_data(G_OBJECT(l->data), "run-last-item"))
                break;
        }
        if (!l) {
            g_warning("Cannot find `Last Used' menu item");
            return;
        }
        item = GTK_WIDGET(l->data);
        label = GTK_BIN(item)->child;
    }

    menu_path = gwy_process_func_get_menu_path(name);
    menu_path = strrchr(menu_path, '/');
    g_assert(menu_path);
    menu_path++;
    len = strlen(menu_path);
    if (g_str_has_suffix(menu_path, "..."))
        len -= 3;
    mp = g_new(gchar, len+1);
    for (i = j = 0; i < len; i++) {
        if (menu_path[i] != '_') {
            mp[j++] = menu_path[i];
        }
    }
    mp[j] = '\0';
    s = g_strconcat(_("_Last Used"), " (", mp, ")", NULL);
    gtk_label_set_text_with_mnemonic(GTK_LABEL(label), s);
    g_free(mp);
    g_free(s);
}

static void
gwy_app_rerun_process_func_cb(void)
{
    GtkWidget *menu;
    gchar *name;

    menu = GTK_WIDGET(g_object_get_data(G_OBJECT(gwy_app_main_window),
                                        "<proc>"));
    g_return_if_fail(menu);
    name = (gchar*)g_object_get_data(G_OBJECT(menu), "last-func");
    g_return_if_fail(name);
    gwy_app_run_process_func_cb(name);
}

void
gwy_app_run_graph_func_cb(gchar *name)
{
    GtkWidget *graph_window, *graph;

    gwy_debug("`%s'", name);
    graph_window = gwy_app_graph_window_get_current();
    if (!graph_window)
        return;
    graph = GTK_BIN(graph_window)->child;
    g_return_if_fail(GWY_IS_GRAPH(graph));
    gwy_graph_func_run(name, GWY_GRAPH(graph));
    /* FIXME TODO: some equivalent of this:
    gwy_app_data_view_update(data_view);
    */
}

void
gwy_app_menu_recent_files_update(GList *recent_files)
{
    GtkWidget *item;
    GQuark quark;
    GList *l, *child;
    gchar *s, *label, *filename;
    gint i;

    g_return_if_fail(GTK_IS_MENU(recent_files_menu));
    child = GTK_MENU_SHELL(recent_files_menu)->children;
    if (GTK_IS_TEAROFF_MENU_ITEM(child->data))
        child = g_list_next(child);

    quark = g_quark_from_string("filename");
    for (i = 0, l = recent_files;
         l && i < gwy_app_n_recent_files;
         l = g_list_next(l), i++) {
        filename = (gchar*)l->data;
        s = g_path_get_basename(filename);
        label = g_strdup_printf("%s%d. %s", i < 10 ? "_" : "", i, s);
        if (child) {
            item = GTK_BIN(child->data)->child;
            gwy_debug("reusing item %p for <%s> [#%d]", item, s, i);
            gtk_label_set_text_with_mnemonic(GTK_LABEL(item), label);
            g_free(g_object_get_qdata(G_OBJECT(child->data), quark));
            g_object_set_qdata(G_OBJECT(child->data), quark,
                               g_strdup(filename));
            child = g_list_next(child);
        }
        else {
            item = gtk_menu_item_new_with_mnemonic(label);
            gwy_debug("creating item %p for <%s> [#%d]", item, s, i);
            g_object_set_qdata_full(G_OBJECT(item), quark, g_strdup(filename),
                                    g_free);
            gtk_menu_shell_append(GTK_MENU_SHELL(recent_files_menu), item);
            g_signal_connect(item, "activate",
                             G_CALLBACK(gwy_app_file_open_recent_cb), NULL);
            gtk_widget_show(item);
        }
        g_free(label);
        g_free(s);
    }
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

    g_signal_emit_by_name(gwy_app_main_window, "delete_event", NULL, &boo);
}

static void
gwy_app_kill_mask_cb(void)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        gwy_app_undo_checkpoint(data, "/0/mask");
        gwy_container_remove_by_name(data, "/0/mask");
        gwy_app_data_view_update(data_view);
    }
}

static void
gwy_app_kill_show_cb(void)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    if (gwy_container_contains_by_name(data, "/0/show")) {
        gwy_app_undo_checkpoint(data, "/0/show");
        gwy_container_remove_by_name(data, "/0/show");
        gwy_data_view_update(GWY_DATA_VIEW(data_view));
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
