/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <stdlib.h>

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule-file.h>
#include "gwyapp.h"
#include "gwyappinternal.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

/* chdir */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <direct.h>
#endif

enum {
    FILELIST_KEY,
    FILELIST_FILENAME,
    FILELIST_LAST
};

typedef struct {
    GtkWidget *open;
    GtkWidget *prune;
    GtkWidget *list;
} Controls;

static GtkWidget* gwy_app_recent_file_list_construct     (GwyContainer *data,
                                                          Controls *controls);
static void       gwy_app_recent_file_list_cell_renderer (GtkTreeViewColumn *column,
                                                          GtkCellRenderer *cell,
                                                          GtkTreeModel *model,
                                                          GtkTreeIter *piter,
                                                          gpointer userdata);
static void       gwy_app_recent_file_list_row_inserted  (GtkTreeModel *store,
                                                          GtkTreePath *path,
                                                          GtkTreeIter *iter,
                                                          Controls *controls);
static void       gwy_app_recent_file_list_row_deleted   (GtkTreeModel *store,
                                                          GtkTreePath *path,
                                                          Controls *controls);
static void       gwy_app_recent_file_list_selection_changed(GtkTreeSelection *selection,
                                                             Controls *controls);
static void       gwy_app_recent_file_list_prune         (GtkWidget *list);
static void       gwy_app_recent_file_list_open          (GtkWidget *list);
static void       gwy_app_recent_file_list_add_line      (gpointer hkey,
                                                          GValue *value,
                                                          GtkListStore *store);

GtkWidget*
gwy_app_recent_file_list(void)
{
    GtkWidget *window, *vbox, *buttonbox, *list;
    Controls *controls;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Graph list for FIXME");
    gtk_window_set_default_size(GTK_WINDOW(window), -1, 180);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    controls = g_new(Controls, 1);
    list = gwy_app_recent_file_list_construct(gwy_app_settings_get(),
                                       controls);
    g_signal_connect_swapped(window, "delete_event",
                             G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect_swapped(list, "destroy", G_CALLBACK(g_free), controls);
    gtk_box_pack_start(GTK_BOX(vbox), list, TRUE, TRUE, 0);

    buttonbox = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);

    controls->open = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->open, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->open, "clicked",
                             G_CALLBACK(gwy_app_recent_file_list_open), list);

    controls->prune = gtk_button_new_with_mnemonic(_("_Remove dangling items"));
    gtk_box_pack_start(GTK_BOX(buttonbox), controls->prune, TRUE, TRUE, 0);
    g_signal_connect_swapped(controls->prune, "clicked",
                             G_CALLBACK(gwy_app_recent_file_list_prune), list);

    g_object_set_data(G_OBJECT(window), "gwy-app-graph-list-view", list);

    gtk_widget_show_all(vbox);

    return window;
}

static GtkWidget*
gwy_app_recent_file_list_construct(GwyContainer *data,
                                   Controls *controls)
{
    static const struct {
        const gchar *title;
        const guint id;
    }
    columns[] = {
        { "File name", FILELIST_FILENAME },
    };

    GtkWidget *list;
    GtkListStore *store;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    /* use an `editable' boolean column which is alwas true */
    store = gtk_list_store_new(1, G_TYPE_UINT);

    gwy_container_foreach(data, "/app/recent",
                          (GHFunc)(gwy_app_recent_file_list_add_line), store);

    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    controls->list = list;
    /*gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(list), TRUE);*/
    g_object_unref(store);
    g_object_set_data(G_OBJECT(store), "container", data);

    for (i = 0; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(columns[i].title,
                                                          renderer,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func
            (column, renderer,
             gwy_app_recent_file_list_cell_renderer,
             GUINT_TO_POINTER(columns[i].id),
             NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_recent_file_list_selection_changed),
                     controls);
    g_signal_connect(store, "row-deleted",
                     G_CALLBACK(gwy_app_recent_file_list_row_deleted),
                     controls);
    g_signal_connect(store, "row-inserted",
                     G_CALLBACK(gwy_app_recent_file_list_row_inserted),
                     controls);

    return list;
}

static void
gwy_app_recent_file_list_row_inserted(G_GNUC_UNUSED GtkTreeModel *store,
                                      G_GNUC_UNUSED GtkTreePath *path,
                                      G_GNUC_UNUSED GtkTreeIter *iter,
                                      Controls *controls)
{
    GtkTreeSelection *selection;

    gtk_widget_set_sensitive(controls->prune, TRUE);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->list));
    gtk_widget_set_sensitive(controls->open,
                             gtk_tree_selection_get_selected(selection,
                                                             NULL, NULL));
}

static void
gwy_app_recent_file_list_row_deleted(GtkTreeModel *store,
                                     G_GNUC_UNUSED GtkTreePath *path,
                                     Controls *controls)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    gboolean has_rows;

    has_rows = gtk_tree_model_get_iter_first(store, &iter);
    gtk_widget_set_sensitive(controls->prune, has_rows);
    if (has_rows) {
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(controls->list));
        gtk_widget_set_sensitive(controls->open,
                                 gtk_tree_selection_get_selected(selection,
                                                                 NULL, NULL));
    }
    else
        gtk_widget_set_sensitive(controls->open, has_rows);
}

static void
gwy_app_recent_file_list_selection_changed(GtkTreeSelection *selection,
                                           Controls *controls)
{
    gtk_widget_set_sensitive(controls->open,
                             gtk_tree_selection_get_selected(selection,
                                                             NULL, NULL));
}

/*
static gboolean
gwy_app_recent_file_list_delete_graph(GtkTreeModel *store,
                                G_GNUC_UNUSED GtkTreePath *path,
                                GtkTreeIter *iter,
                                G_GNUC_UNUSED gpointer userdata)
{
    GtkWidget *graph;
    GObject *gmodel;
    GwyContainer *data;
    gint id;
    gchar key[32];

    gtk_tree_model_get(store, iter, FILELIST_GMODEL, &gmodel, -1);
    graph = GTK_WIDGET(GWY_FILE_MODEL(gmodel)->graph);
    if (graph)
        gtk_widget_destroy(gtk_widget_get_toplevel(graph));

    gtk_list_store_remove(GTK_LIST_STORE(store), iter);
    id = GPOINTER_TO_INT(g_object_get_data(gmodel, "gwy-app-graph-list-id"));
    g_assert(id);
    g_snprintf(key, sizeof(key), "/0/graph/graph/%d", id);
    data = (GwyContainer*)g_object_get_data(G_OBJECT(store), "container");
    g_assert(GWY_IS_CONTAINER(data));
    gwy_container_remove_by_name(data, key);
    g_object_unref(gmodel);

    return FALSE;
}
*/

static void
gwy_app_recent_file_list_prune(GtkWidget *list)
{
    GtkTreeModel *store;
    GtkTreeIter iter;

    /*
    store = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    while (gtk_tree_model_get_iter_first(store, &iter))
        gwy_app_recent_file_list_delete_graph(store, NULL, &iter, list);
    gtk_widget_queue_draw(list);
    */
}

static void
gwy_app_recent_file_list_open(GtkWidget *list)
{
    GtkTreeSelection *selection;
    GtkTreeModel *store;
    GtkTreeIter iter;
    const guchar *filename_utf8;
    gchar *filename_sys, *dirname;
    GQuark key;
    GwyContainer *data;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    if (gtk_tree_selection_get_selected(selection, &store, &iter)) {
        gtk_tree_model_get(store, &iter, FILELIST_KEY, &key, -1);
        if (gwy_container_gis_string(gwy_app_settings_get(), key,
                                     &filename_utf8)) {
            filename_sys = g_filename_from_utf8(filename_utf8,
                                                -1, NULL, NULL, NULL);
            if (filename_sys) {
                data = gwy_file_load(filename_sys);
                if (data) {
                    /* XXX: this is copied from file_real_open().
                     * Need an API for doing such things. */
                    gwy_container_set_string_by_name(data, "/filename",
                                                     filename_utf8);
                    gwy_app_data_window_create(data);
                    /* XXX: can't! No API.
                     * recent_files_update(filename_utf8); */

                    /* change directory to that of the loaded file */
                    dirname = g_path_get_dirname(filename_sys);
                    if (strcmp(dirname, "."))
                        chdir(dirname);
                    g_free(dirname);
                }
            }
            g_free(filename_sys);
        }
    }
    gtk_widget_queue_draw(list);
}

static void
gwy_app_recent_file_list_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                       GtkCellRenderer *cell,
                                       GtkTreeModel *model,
                                       GtkTreeIter *piter,
                                       gpointer userdata)
{
    GwyContainer *data;
    GQuark key;
    gulong id;
    const gchar *filename;

    id = GPOINTER_TO_UINT(userdata);
    g_assert(id == FILELIST_FILENAME);
    gtk_tree_model_get(model, piter, FILELIST_KEY, &key, -1);
    g_return_if_fail(key);
    data = gwy_app_settings_get();
    switch (id) {
        case FILELIST_FILENAME:
        filename = gwy_container_get_string(data, key);
        g_return_if_fail(filename);
        g_object_set(cell, "text", filename, NULL);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
gwy_app_recent_file_list_add_line(gpointer hkey,
                                  GValue *value,
                                  GtkListStore *store)
{
    GObject *gmodel;
    GtkTreeIter iter;
    GQuark quark;

    g_return_if_fail(G_VALUE_HOLDS_STRING(value));
    quark = GPOINTER_TO_UINT(hkey);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, FILELIST_KEY, quark, -1);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
