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
#define DEBUG 1
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

typedef struct {
    guchar md5sum[8];
    gchar *filename_utf8;
    gchar *thumbnail_filename;
} RecentFile;

enum {
    FILELIST_INDEX,
    FILELIST_FILENAME,
    FILELIST_LAST
};

typedef struct {
    GtkWidget *open;
    GtkWidget *prune;
    GtkWidget *list;
} Controls;

static GtkWidget* gwy_app_recent_file_list_construct     (Controls *controls);
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

static guint remember_recent_files = 256;
static GArray *recent_files = NULL;
static GList *recent_file_list = NULL;  /* for menu update, API wants GList */

GtkWidget*
gwy_app_recent_file_list_new(void)
{
    GtkWidget *window, *vbox, *buttonbox, *list, *scroll;
    Controls *controls;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Graph list for FIXME");
    gtk_window_set_default_size(GTK_WINDOW(window), -1, 280);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    controls = g_new(Controls, 1);
    list = gwy_app_recent_file_list_construct(controls);
    g_signal_connect_swapped(list, "destroy", G_CALLBACK(g_free), controls);
    gtk_container_add(GTK_CONTAINER(scroll), list);

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
gwy_app_recent_file_list_construct(Controls *controls)
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
    GtkTreeIter iter;
    guint i;

    g_return_val_if_fail(recent_files, NULL);
    store = gtk_list_store_new(1, G_TYPE_UINT);

    for (i = 0; i < recent_files->len; i++) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, FILELIST_INDEX, i, -1);
    }

    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    controls->list = list;
    /*gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(list), TRUE);*/
    g_object_unref(store);

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
    gchar *filename_sys, *dirname;
    guint i;
    GwyContainer *data;
    RecentFile *rf;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    if (gtk_tree_selection_get_selected(selection, &store, &iter)) {
        gtk_tree_model_get(store, &iter, FILELIST_INDEX, &i, -1);
        g_return_if_fail(i < recent_files->len);
        rf = &g_array_index(recent_files, RecentFile, i);
        filename_sys = g_filename_from_utf8(rf->filename_utf8,
                                            -1, NULL, NULL, NULL);
        if (filename_sys) {
            data = gwy_file_load(filename_sys);
            if (data) {
                /* XXX: this is copied from file_real_open().
                 * Need an API for doing such things. */
                gwy_container_set_string_by_name(data, "/filename",
                                                 g_strdup(rf->filename_utf8));
                gwy_app_data_window_create(data);
                /* FIXME: this is wrong, we just have to exchange the two
                 * files */
                gwy_app_recent_file_list_update(rf->filename_utf8);

                /* change directory to that of the loaded file */
                dirname = g_path_get_dirname(filename_sys);
                if (strcmp(dirname, "."))
                    chdir(dirname);
                g_free(dirname);
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
    guint id, i;
    RecentFile *rf;

    id = GPOINTER_TO_UINT(userdata);
    g_assert(id == FILELIST_FILENAME);
    gtk_tree_model_get(model, piter, FILELIST_INDEX, &i, -1);
    g_return_if_fail(i < recent_files->len);
    rf = &g_array_index(recent_files, RecentFile, i);
    switch (id) {
        case FILELIST_FILENAME:
        g_object_set(cell, "text", rf->filename_utf8, NULL);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}






gboolean
gwy_app_recent_file_list_load(const gchar *filename)
{
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0;
    gchar **files;
    guint n;

    /* TODO: do something with existing recent_files, recent_file_list */
    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        recent_files = g_array_new(FALSE, FALSE, sizeof(RecentFile));
        return FALSE;
    }

#ifdef G_OS_WIN32
    gwy_strkill(buffer, "\r");
#endif
    files = g_strsplit(buffer, "\n", 0);
    g_free(buffer);
    if (!files) {
        recent_files = g_array_new(FALSE, FALSE, sizeof(RecentFile));
        return TRUE;
    }

    for (n = 0; files[n]; n++)
        ;
    recent_files = g_array_sized_new(FALSE, FALSE, sizeof(RecentFile), n);
    for (n = 0; files[n]; n++) {
        if (*files[n]) {
            RecentFile rf;

            memset(&rf, 0, sizeof(RecentFile));
            rf.filename_utf8 = files[n];
            g_array_append_val(recent_files, rf);

            if (n < (guint)gwy_app_n_recent_files) {
                recent_file_list = g_list_append(recent_file_list,
                                                 rf.filename_utf8);
            }
        }
        else
            g_free(files[n]);
    }
    g_free(files);

    return TRUE;
}

gboolean
gwy_app_recent_file_list_save(const gchar *filename)
{
    FILE *fh;
    guint i;

    fh = fopen(filename, "w");
    if (!fh)
        return FALSE;

    for (i = 0; i < recent_files->len && i < remember_recent_files; i++) {
        RecentFile *rf = &g_array_index(recent_files, RecentFile, i);
        fputs(rf->filename_utf8, fh);
        fputc('\n', fh);
    }
    fclose(fh);

    return TRUE;
}

void
gwy_app_recent_file_list_update(const gchar *filename_utf8)
{
    GList *l;
    guint i;

    gwy_debug("%s", filename_utf8);
    g_return_if_fail(recent_files);

    if (filename_utf8) {
        RecentFile rfnew;

        memset(&rfnew, 0, sizeof(RecentFile));
        rfnew.filename_utf8 = g_strdup(filename_utf8);

        /* TODO: optimize move near begining */

        for (i = 0; i < recent_files->len; i++) {
            RecentFile *rf = &g_array_index(recent_files, RecentFile, i);
            if (!strcmp(rf->filename_utf8, filename_utf8)) {
                g_free(rf->filename_utf8);
                g_free(rf->thumbnail_filename);
                g_array_remove_index(recent_files, i);
                break;
            }
        }

        g_array_insert_val(recent_files, 0, rfnew);
    }
    for (i = 0; i < recent_files->len; i++) {
        RecentFile *rf = &g_array_index(recent_files, RecentFile, i);
        gwy_debug("%u: <%s>", i, rf->filename_utf8);
    }

    l = recent_file_list;
    for (i = 0;
         i < (guint)gwy_app_n_recent_files && i < recent_files->len;
         i++) {
        RecentFile *rf = &g_array_index(recent_files, RecentFile, i);

        if (l) {
            l->data = rf->filename_utf8;
            l = g_list_next(l);
        }
        else {
            recent_file_list = g_list_append(recent_file_list,
                                             rf->filename_utf8);
        }
    }
    /* This should not happen here as we added a file */
    if (l) {
        if (!l->prev)
            recent_file_list = NULL;
        else {
            l->prev->next = NULL;
            l->prev = NULL;
        }
        g_list_free(l);
    }
    gwy_app_menu_recent_files_update(recent_file_list);
    /* FIXME: update the list view */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
