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

/* TODO:
 * - add some equivalent of file_real_open() to API and use it from the other
 *   places
 * - add thumbnails, see Thumbnail Managing Standard
 *   http://triq.net/~jens/thumbnail-spec/index.html
 */

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwydgets.h>
#include "gwyapp.h"
#include "gwyappinternal.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

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
    FILELIST_RAW,
    FILELIST_FILENAME,
    FILELIST_LAST
};

typedef struct {
    GtkListStore *store;
    GList *recent_file_list;
    GtkWidget *window;
    GtkWidget *list;
    GtkWidget *open;
    GtkWidget *prune;
} Controls;

static GtkWidget* gwy_app_recent_file_list_construct (Controls *controls);
static void  gwy_app_recent_file_list_cell_renderer  (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *piter,
                                                      gpointer userdata);
static void  gwy_app_recent_file_list_row_inserted   (GtkTreeModel *store,
                                                      GtkTreePath *path,
                                                      GtkTreeIter *iter,
                                                      Controls *controls);
static void  gwy_app_recent_file_list_row_deleted    (GtkTreeModel *store,
                                                      GtkTreePath *path,
                                                      Controls *controls);
static void  gwy_app_recent_file_list_selection_changed(GtkTreeSelection *selection,
                                                        Controls *controls);
static void  gwy_app_recent_file_list_row_activated  (GtkTreeView *treeview,
                                                      GtkTreePath *path,
                                                      GtkTreeViewColumn *column,
                                                      gpointer user_data);
static void  gwy_app_recent_file_list_destroyed      (Controls *controls);
static void  gwy_app_recent_file_list_prune          (Controls *controls);
static void  gwy_app_recent_file_list_open           (GtkWidget *list);
static void  gwy_app_recent_file_list_update_menu    (Controls *controls);

static guint remember_recent_files = 256;

static Controls gcontrols = { NULL, NULL, NULL, NULL, NULL, NULL };

GtkWidget*
gwy_app_recent_file_list_new(void)
{
    GtkWidget *vbox, *buttonbox, *list, *scroll;

    g_return_val_if_fail(gcontrols.window == NULL, gcontrols.window);

    gcontrols.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gcontrols.window), _("Document History"));
    gtk_window_set_default_size(GTK_WINDOW(gcontrols.window), -1, 280);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gcontrols.window), vbox);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    list = gwy_app_recent_file_list_construct(&gcontrols);
    gtk_container_add(GTK_CONTAINER(scroll), list);

    buttonbox = gtk_hbox_new(TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(buttonbox), 2);
    gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);

    gcontrols.open = gtk_button_new_from_stock(GTK_STOCK_OPEN);
    gtk_box_pack_start(GTK_BOX(buttonbox), gcontrols.open, TRUE, TRUE, 0);
    g_signal_connect_swapped(gcontrols.open, "clicked",
                             G_CALLBACK(gwy_app_recent_file_list_open), list);

    gcontrols.prune = gwy_stock_like_button_new(_("Prune"),
                                                GTK_STOCK_FIND);
    gtk_box_pack_start(GTK_BOX(buttonbox), gcontrols.prune, TRUE, TRUE, 0);
    g_signal_connect_swapped(gcontrols.prune, "clicked",
                             G_CALLBACK(gwy_app_recent_file_list_prune),
                             &gcontrols);

    g_signal_connect_swapped(gcontrols.window, "destroy",
                             G_CALLBACK(gwy_app_recent_file_list_destroyed),
                             &gcontrols);

    gtk_widget_show_all(vbox);

    return gcontrols.window;
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
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    guint i;

    g_return_val_if_fail(controls->store, NULL);

    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controls->store));
    controls->list = list;
    /*gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(list), TRUE);*/
    /* silly for one column */
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);

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
    g_signal_connect(controls->store, "row-deleted",
                     G_CALLBACK(gwy_app_recent_file_list_row_deleted),
                     controls);
    g_signal_connect(controls->store, "row-inserted",
                     G_CALLBACK(gwy_app_recent_file_list_row_inserted),
                     controls);
    g_signal_connect(controls->list, "row-activated",
                     G_CALLBACK(gwy_app_recent_file_list_row_activated),
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

static void
gwy_app_recent_file_list_destroyed(Controls *controls)
{
    controls->window = NULL;
    controls->open = NULL;
    controls->prune = NULL;
    controls->list = NULL;
}

static void
gwy_app_recent_file_list_prune(Controls *controls)
{
    GtkTreeIter iter;
    RecentFile *rf;

    g_return_if_fail(controls->store);

    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(controls->store), &iter))
        return;

    do {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->store), &iter,
                           FILELIST_RAW, &rf, -1);
        if (!g_file_test(rf->filename_utf8, G_FILE_TEST_IS_REGULAR)) {
            /* the remove moves to the next row itself */
            if (gtk_list_store_remove(controls->store, &iter))
                continue;
            else
                break;
        }
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(controls->store), &iter));

    gwy_app_recent_file_list_update_menu(controls);
}

static void
gwy_app_recent_file_list_open_file(const gchar *filename_utf8)
{
    GwyContainer *data;
    gchar *filename_sys, *dirname;

    /* XXX: this is copied from file_real_open().
     * Need an API for doing such things.
     * Especially when one has to include the silly MS headers */
    filename_sys = g_filename_from_utf8(filename_utf8,
                                        -1, NULL, NULL, NULL);
    g_return_if_fail(filename_sys);

    data = gwy_file_load(filename_sys);
    if (data) {
        gwy_container_set_string_by_name(data, "/filename",
                                         g_strdup(filename_utf8));
        gwy_app_data_window_create(data);
        gwy_app_recent_file_list_update(filename_utf8);

        /* change directory to that of the loaded file */
        dirname = g_path_get_dirname(filename_sys);
        if (strcmp(dirname, "."))
            chdir(dirname);
        g_free(dirname);
    }
    g_free(filename_sys);
}

static void
gwy_app_recent_file_list_row_activated(GtkTreeView *treeview,
                                       GtkTreePath *path,
                                       G_GNUC_UNUSED GtkTreeViewColumn *column,
                                       G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    RecentFile *rf;

    model = gtk_tree_view_get_model(treeview);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, FILELIST_RAW, &rf, -1);
    gwy_app_recent_file_list_open_file(rf->filename_utf8);
}

static void
gwy_app_recent_file_list_open(GtkWidget *list)
{
    GtkTreeSelection *selection;
    GtkTreeModel *store;
    GtkTreeIter iter;
    RecentFile *rf;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    g_return_if_fail(gtk_tree_selection_get_selected(selection, &store, &iter));
    gtk_tree_model_get(store, &iter, FILELIST_RAW, &rf, -1);
    gwy_app_recent_file_list_open_file(rf->filename_utf8);
}

static void
gwy_app_recent_file_list_cell_renderer(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                       GtkCellRenderer *cell,
                                       GtkTreeModel *model,
                                       GtkTreeIter *iter,
                                       gpointer userdata)
{
    guint id;
    RecentFile *rf;

    id = GPOINTER_TO_UINT(userdata);
    g_assert(id == FILELIST_FILENAME);
    gtk_tree_model_get(model, iter, FILELIST_RAW, &rf, -1);
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
    GtkTreeIter iter;
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0;
    gchar **files;
    guint n;

    /* TODO: do something with existing stuff? */
    g_return_val_if_fail(gcontrols.store == NULL, FALSE);
    gcontrols.store = gtk_list_store_new(1, G_TYPE_POINTER);

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_clear_error(&err);
        return FALSE;
    }

#ifdef G_OS_WIN32
    gwy_strkill(buffer, "\r");
#endif
    files = g_strsplit(buffer, "\n", 0);
    g_free(buffer);
    if (!files)
        return TRUE;

    for (n = 0; files[n]; n++) {
        if (*files[n]) {
            RecentFile *rf;

            rf = g_new0(RecentFile, 1);
            rf->filename_utf8 = files[n];
            gtk_list_store_append(gcontrols.store, &iter);
            gtk_list_store_set(gcontrols.store, &iter, FILELIST_RAW, rf, -1);
            if (n < (guint)gwy_app_n_recent_files) {
                gcontrols.recent_file_list
                    = g_list_append(gcontrols.recent_file_list,
                                    rf->filename_utf8);
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
    GtkTreeIter iter;
    RecentFile *rf;
    guint i;
    FILE *fh;

    g_return_val_if_fail(gcontrols.store, FALSE);
    fh = fopen(filename, "w");
    if (!fh)
        return FALSE;

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gcontrols.store), &iter)) {
        i = 0;
        do {
            gtk_tree_model_get(GTK_TREE_MODEL(gcontrols.store), &iter,
                               FILELIST_RAW, &rf, -1);
            fputs(rf->filename_utf8, fh);
            fputc('\n', fh);
            i++;
        } while (i < remember_recent_files
                 && gtk_tree_model_iter_next(GTK_TREE_MODEL(gcontrols.store),
                                             &iter));
    }
    fclose(fh);

    return TRUE;
}

/* Do NOT call this function when the recent file menu can still exist! */
void
gwy_app_recent_file_list_free(void)
{
    GtkTreeIter iter;
    RecentFile *rf;

    if (!gcontrols.store)
        return;
    if (gcontrols.window)
        gtk_widget_destroy(gcontrols.window);

    while (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gcontrols.store),
                                         &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(gcontrols.store), &iter,
                           FILELIST_RAW, &rf, -1);
        g_free(rf->filename_utf8);
        g_free(rf->thumbnail_filename);
        g_free(rf);
        gtk_list_store_remove(gcontrols.store, &iter);
    }
    gcontrols.store = NULL;

    g_list_free(gcontrols.recent_file_list);
    gcontrols.recent_file_list = NULL;
}

void
gwy_app_recent_file_list_update(const gchar *filename_utf8)
{
    gwy_debug("%s", filename_utf8);
    g_return_if_fail(gcontrols.store);

    if (filename_utf8) {
        GtkTreeIter iter;
        RecentFile *rf;
        gboolean found = FALSE;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gcontrols.store),
                                          &iter)) {
            do {
                gtk_tree_model_get(GTK_TREE_MODEL(gcontrols.store), &iter,
                                   FILELIST_RAW, &rf,
                                   -1);
                if (strcmp(filename_utf8, rf->filename_utf8) == 0) {
                    found = TRUE;
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(gcontrols.store),
                                              &iter));

            if (found)
                gtk_list_store_move_after(gcontrols.store, &iter, NULL);
        }

        if (!found) {
            rf = g_new0(RecentFile, 1);
            rf->filename_utf8 = g_strdup(filename_utf8);
            gtk_list_store_prepend(gcontrols.store, &iter);
            gtk_list_store_set(gcontrols.store, &iter, FILELIST_RAW, rf, -1);
        }
    }

    gwy_app_recent_file_list_update_menu(&gcontrols);
}

static void
gwy_app_recent_file_list_update_menu(Controls *controls)
{
    GtkTreeIter iter;
    GList *l;
    guint i;

    l = controls->recent_file_list;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(controls->store), &iter)) {
        i = 0;
        do {
            RecentFile *rf;

            gtk_tree_model_get(GTK_TREE_MODEL(controls->store), &iter,
                               FILELIST_RAW, &rf, -1);
            if (l) {
                l->data = rf->filename_utf8;
                l = g_list_next(l);
            }
            else {
                controls->recent_file_list
                    = g_list_append(controls->recent_file_list,
                                    rf->filename_utf8);
            }
            i++;
        } while (i < (guint)gwy_app_n_recent_files
                 && gtk_tree_model_iter_next(GTK_TREE_MODEL(controls->store),
                                             &iter));
    }
    /* This should not happen here as we added a file */
    if (l) {
        if (!l->prev)
            controls->recent_file_list = NULL;
        else {
            l->prev->next = NULL;
            l->prev = NULL;
        }
        g_list_free(l);
    }
    gwy_app_menu_recent_files_update(controls->recent_file_list);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
