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

/*
 * This file implements -- among other things -- Thumbnail Managing Standard
 * http://triq.net/~jens/thumbnail-spec/index.html
 *
 * The implementation is quite minimal: we namely ignore large
 * thumbnails altogether (as they would be usually larger than SPM data).
 * We try not to break other TMS aware applications though.
 */

/* TODO:
 * - add some equivalent of file_real_open() to API and use it from the other
 *   places
 * - Do NOT store thumbnails for anything in ~/.thumbnails
 */

#include <libgwyddion/gwymacros.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <libgwyddion/gwywin32unistd.h>

#if (defined(HAVE_SYS_STAT_H) || defined(_WIN32))
#include <sys/stat.h>
/* And now we are in a deep s... */
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwydgets.h>
#include "gwyapp.h"
#include "gwyappinternal.h"

/* PNG (additional in TMS) */
#define KEY_DESCRIPTION "tEXt::Description"
#define KEY_SOFTWARE    "tEXt::Software"
/* TMS, required */
#define KEY_THUMB_URI   "tEXt::Thumb::URI"
#define KEY_THUMB_MTIME "tEXt::Thumb::MTime"
/* TMS, additional */
#define KEY_THUMB_FILESIZE "tEXt::Thumb::Size"
#define KEY_THUMB_MIMETYPE "tEXt::Thumb::Mimetype"
/* TMS, format specific
 * XXX: we use Image::Width, Image::Height, even tough the data are not images
 * but they are very image-like... */
#define KEY_THUMB_IMAGE_WIDTH "tEXt::Thumb::Image::Width"
#define KEY_THUMB_IMAGE_HEIGHT "tEXt::Thumb::Image::Height"
/* Gwyddion specific, unimplemented */
#define KEY_THUMB_GWY_IMAGES "tEXt::Thumb::X-Gwyddion::Images"
#define KEY_THUMB_GWY_GRAPHS "tEXt::Thumb::X-Gwyddion::Graphs"

enum {
    THUMB_SIZE = 60,
    TMS_NORMAL_THUMB_SIZE = 128
};

typedef enum {
    FILE_STATE_UNKNOWN = 0,
    FILE_STATE_OLD,
    FILE_STATE_OK,
    FILE_STATE_FAILED
} FileState;

enum {
    FILELIST_RAW,
    FILELIST_THUMB,
    FILELIST_FILENAME,
    FILELIST_LAST
};

typedef struct {
    FileState file_state;
    gchar *file_utf8;
    gchar *file_sys;
    gchar *file_uri;
    gulong file_mtime;
    gulong file_size;

    gint image_width;
    gint image_height;
    gint image_images;
    gint image_graphs;

    FileState thumb_state;
    gchar *thumb_sys;    /* doesn't matter, names are ASCII */
    gulong thumb_mtime;
    GdkPixbuf *pixbuf;
} GwyRecentFile;

typedef struct {
    GtkListStore *store;
    GList *recent_file_list;
    GtkWidget *window;
    GtkWidget *list;
    GtkWidget *open;
    GtkWidget *prune;
    GtkTooltips *tooltips;
    GdkPixbuf *failed_pixbuf;
} Controls;

static GtkWidget* gwy_app_recent_file_list_construct (Controls *controls);
static void  cell_renderer_desc                      (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *piter,
                                                      gpointer userdata);
static void  cell_renderer_thumb                     (GtkTreeViewColumn *column,
                                                      GtkCellRenderer *cell,
                                                      GtkTreeModel *model,
                                                      GtkTreeIter *iter,
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

static void  gwy_app_recent_file_create_dirs         (void);
static GwyRecentFile* gwy_recent_file_new            (gchar *filename_utf8,
                                                      gchar *filename_sys);
static gboolean recent_file_try_load_thumbnail       (GwyRecentFile *rf);
static void     gwy_recent_file_update_thumbnail     (GwyRecentFile *rf,
                                                      GwyDataWindow *data_window);
static void  gwy_recent_file_free                    (GwyRecentFile *rf);
static gchar* gwy_recent_file_thumbnail_name         (const gchar *uri);
static G_CONST_RETURN gchar* gwy_recent_file_thumbnail_dir (void);

static guint remember_recent_files = 256;

static Controls gcontrols = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

/**
 * gwy_app_recent_file_list_new:
 *
 * Creates document history browser.
 *
 * There should be at most one document history browser, so this function
 * fails if it already exists.
 *
 * Returns: The newly created document history browser window.
 *
 * Since: 1.5
 **/
GtkWidget*
gwy_app_recent_file_list_new(void)
{
    GtkWidget *vbox, *buttonbox, *list, *scroll;
    GtkTreeSelection *selection;

    g_return_val_if_fail(gcontrols.store, gcontrols.window);
    g_return_val_if_fail(gcontrols.window == NULL, gcontrols.window);

    gcontrols.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gcontrols.window), _("Document History"));
    gtk_window_set_default_size(GTK_WINDOW(gcontrols.window), -1,
                                3*gdk_screen_height()/4);

    /* FIXME: who will unref this beast? */
    gcontrols.tooltips = gtk_tooltips_new();

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
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    gtk_widget_set_sensitive(gcontrols.open,
                             gtk_tree_selection_get_selected(selection,
                                                             NULL, NULL));

    gcontrols.prune = gwy_stock_like_button_new(_("_Prune"),
                                                GTK_STOCK_FIND);
    gtk_box_pack_start(GTK_BOX(buttonbox), gcontrols.prune, TRUE, TRUE, 0);
    gtk_tooltips_set_tip(gcontrols.tooltips, gcontrols.prune,
                         _("Remove entries of files that no longer exist"),
                         NULL);
    g_signal_connect_swapped(gcontrols.prune, "clicked",
                             G_CALLBACK(gwy_app_recent_file_list_prune),
                             &gcontrols);

    g_signal_connect_swapped(gcontrols.window, "destroy",
                             G_CALLBACK(gwy_app_recent_file_list_destroyed),
                             &gcontrols);

    gcontrols.failed_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
                                             THUMB_SIZE, THUMB_SIZE);
    gwy_debug_objects_creation(G_OBJECT(gcontrols.failed_pixbuf));
    gdk_pixbuf_fill(gcontrols.failed_pixbuf, 0);

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
        { "Preview",    FILELIST_THUMB },
        { "File Path",  FILELIST_FILENAME },
    };

    GtkWidget *list;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    gsize i;

    g_return_val_if_fail(controls->store, NULL);

    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(controls->store));
    controls->list = list;
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);

    /* thumbnail name column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes(_(columns[0].title),
                                                      renderer, NULL);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            cell_renderer_thumb,
                                            GUINT_TO_POINTER(columns[0].id),
                                            NULL);  /* destroy notify */
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    /* other columns */
    for (i = 1; i < G_N_ELEMENTS(columns); i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(columns[i].title),
                                                          renderer,
                                                          NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                cell_renderer_desc,
                                                GUINT_TO_POINTER(columns[i].id),
                                                NULL);  /* destroy notify */
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    }

    /* selection */
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

    if (!controls->window)
        return;

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

    if (!controls->window)
        return;

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
    gwy_object_unref(controls->failed_pixbuf);
    controls->window = NULL;
    controls->open = NULL;
    controls->prune = NULL;
    controls->list = NULL;
}

static void
gwy_app_recent_file_list_prune(Controls *controls)
{
    GtkTreeIter iter;
    GwyRecentFile *rf;
    gboolean ok;

    g_return_if_fail(controls->store);

    if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(controls->store), &iter))
        return;

    do {
        gtk_tree_model_get(GTK_TREE_MODEL(controls->store), &iter,
                           FILELIST_RAW, &rf, -1);
        gwy_debug("<%s>", rf->file_utf8);
        if (!g_file_test(rf->file_utf8, G_FILE_TEST_IS_REGULAR)) {
            if (rf->thumb_sys && rf->thumb_state != FILE_STATE_FAILED)
                unlink(rf->thumb_sys);
            gwy_recent_file_free(rf);
            ok = gtk_list_store_remove(controls->store, &iter);
        }
        else
            ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(controls->store),
                                          &iter);
    } while (ok);

    gwy_app_recent_file_list_update_menu(controls);
}

static void
gwy_app_recent_file_list_open_file(const gchar *filename_utf8)
{
    GwyContainer *data;
    GtkWidget *data_window;
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
        data_window = gwy_app_data_window_create(data);
        gwy_app_recent_file_list_update(GWY_DATA_WINDOW(data_window),
                                        filename_utf8,
                                        filename_sys);

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
    GwyRecentFile *rf;

    model = gtk_tree_view_get_model(treeview);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, FILELIST_RAW, &rf, -1);
    gwy_app_recent_file_list_open_file(rf->file_utf8);
}

static void
gwy_app_recent_file_list_open(GtkWidget *list)
{
    GtkTreeSelection *selection;
    GtkTreeModel *store;
    GtkTreeIter iter;
    GwyRecentFile *rf;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    if (!gtk_tree_selection_get_selected(selection, &store, &iter))
        return;
    gtk_tree_model_get(store, &iter, FILELIST_RAW, &rf, -1);
    gwy_app_recent_file_list_open_file(rf->file_utf8);
}

static void
cell_renderer_desc(G_GNUC_UNUSED GtkTreeViewColumn *column,
                   GtkCellRenderer *cell,
                   GtkTreeModel *model,
                   GtkTreeIter *iter,
                   gpointer userdata)
{
    guint id;
    GwyRecentFile *rf;
    gchar s[32];
    gchar *str;

    id = GPOINTER_TO_UINT(userdata);
    gtk_tree_model_get(model, iter, FILELIST_RAW, &rf, -1);
    switch (id) {
        case FILELIST_FILENAME:
        if (!rf->image_width || !rf->image_height)
            s[0] = '\0';
        else
            g_snprintf(s, sizeof(s), "%d×%d",
                       rf->image_width, rf->image_height);
        str = g_strconcat(rf->file_utf8, "\n", s, NULL);
        g_object_set(cell, "text", str, NULL);
        g_free(str);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static void
cell_renderer_thumb(G_GNUC_UNUSED GtkTreeViewColumn *column,
                    GtkCellRenderer *cell,
                    GtkTreeModel *model,
                    GtkTreeIter *iter,
                    gpointer userdata)
{
    GwyRecentFile *rf;
    guint id;

    id = GPOINTER_TO_UINT(userdata);
    g_return_if_fail(id == FILELIST_THUMB);
    gtk_tree_model_get(model, iter, FILELIST_RAW, &rf, -1);
    switch (rf->thumb_state) {
        case FILE_STATE_UNKNOWN:
        if (!recent_file_try_load_thumbnail(rf))
            return;
        case FILE_STATE_FAILED:
        case FILE_STATE_OK:
        case FILE_STATE_OLD:
        g_object_set(cell, "pixbuf", rf->pixbuf, NULL);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

/**
 * gwy_app_recent_file_list_load:
 * @filename: Name of file containing list of recently open files.
 *
 * Loads list of recently open files from @filename.
 *
 * Cannot be called more than once (at least not without doing
 * gwy_app_recent_file_list_free() first).  Must be called before any other
 * document history function can be used, even if on a nonexistent file.
 *
 * Returns: %TRUE if the file was read successfully, %FALSE otherwise.
 *
 * Since: 1.5
 **/
gboolean
gwy_app_recent_file_list_load(const gchar *filename)
{
    GtkTreeIter iter;
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0;
    gchar **files;
    guint n;

    gwy_app_recent_file_create_dirs();

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
            GwyRecentFile *rf;

            gwy_debug("%s", files[n]);
            rf = gwy_recent_file_new(gwy_canonicalize_path(files[n]), NULL);
            gtk_list_store_append(gcontrols.store, &iter);
            gtk_list_store_set(gcontrols.store, &iter, FILELIST_RAW, rf, -1);
            if (n < (guint)gwy_app_n_recent_files) {
                gcontrols.recent_file_list
                    = g_list_append(gcontrols.recent_file_list, rf->file_utf8);
            }
        }
        g_free(files[n]);
    }
    g_free(files);

    return TRUE;
}


/**
 * gwy_app_recent_file_list_save:
 * @filename: Name of file to save the list of recently open files to.
 *
 * Saves list of recently open files to @filename.
 *
 * Returns: %TRUE if the file was written successfully, %FALSE otherwise.
 *
 * Since: 1.5
 **/
gboolean
gwy_app_recent_file_list_save(const gchar *filename)
{
    GtkTreeIter iter;
    GwyRecentFile *rf;
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
            fputs(rf->file_utf8, fh);
            fputc('\n', fh);
            i++;
        } while (i < remember_recent_files
                 && gtk_tree_model_iter_next(GTK_TREE_MODEL(gcontrols.store),
                                             &iter));
    }
    fclose(fh);

    return TRUE;
}

/**
 * gwy_app_recent_file_list_free:
 *
 * Frees all memory taken by recent file list.
 *
 * Should not be called while the recent file menu still exists.
 *
 * Since: 1.5
 **/
void
gwy_app_recent_file_list_free(void)
{
    GtkTreeIter iter;
    GwyRecentFile *rf;

    if (!gcontrols.store)
        return;

    if (gcontrols.window)
        gtk_widget_destroy(gcontrols.window);

    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gcontrols.store),
                                      &iter)) {
        do {
            gtk_tree_model_get(GTK_TREE_MODEL(gcontrols.store), &iter,
                               FILELIST_RAW, &rf, -1);
            gwy_recent_file_free(rf);
        } while (gtk_list_store_remove(gcontrols.store, &iter));
    }
    gcontrols.store = NULL;

    g_list_free(gcontrols.recent_file_list);
    gcontrols.recent_file_list = NULL;
    gwy_app_recent_file_list_update_menu(&gcontrols);
}

/**
 * gwy_app_recent_file_list_update:
 * @data_window: A data window corresponding to the file (for thumbnails).
 * @filename_utf8: A recent file to insert or move to the first position in
 *                 document history, in UTF-8.
 * @filename_sys: A recent file to insert or move to the first position in
 *                 document history, in system encoding.
 *
 * Moves @filename_utf8 to the first position in document history, eventually
 * adding it if not present yet.
 *
 * At least one of @filename_utf8, @filename_sys should be set.
 *
 * Since: 1.5
 **/
void
gwy_app_recent_file_list_update(GwyDataWindow *data_window,
                                const gchar *filename_utf8,
                                const gchar *filename_sys)
{
    g_return_if_fail(gcontrols.store);

    if (!filename_utf8 && filename_sys)
        filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);

    if (filename_utf8) {
        GtkTreeIter iter;
        GwyRecentFile *rf;
        gboolean found = FALSE;

        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gcontrols.store),
                                          &iter)) {
            do {
                gtk_tree_model_get(GTK_TREE_MODEL(gcontrols.store), &iter,
                                   FILELIST_RAW, &rf,
                                   -1);
                if (strcmp(filename_utf8, rf->file_utf8) == 0) {
                    found = TRUE;
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(gcontrols.store),
                                              &iter));

            if (found)
                gtk_list_store_move_after(gcontrols.store, &iter, NULL);
        }

        if (!found) {
            rf = gwy_recent_file_new(gwy_canonicalize_path(filename_utf8),
                                     gwy_canonicalize_path(filename_sys));
            gtk_list_store_prepend(gcontrols.store, &iter);
            gtk_list_store_set(gcontrols.store, &iter, FILELIST_RAW, rf, -1);
        }

        if (data_window)
            gwy_recent_file_update_thumbnail(rf, data_window);
    }

    gwy_app_recent_file_list_update_menu(&gcontrols);
}

static void
gwy_app_recent_file_list_update_menu(Controls *controls)
{
    GtkTreeIter iter;
    GList *l;
    guint i;

    if (!controls->store) {
        g_return_if_fail(controls->recent_file_list == NULL);
        gwy_app_menu_recent_files_update(controls->recent_file_list);
        return;
    }

    l = controls->recent_file_list;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(controls->store), &iter)) {
        i = 0;
        do {
            GwyRecentFile *rf;

            gtk_tree_model_get(GTK_TREE_MODEL(controls->store), &iter,
                               FILELIST_RAW, &rf, -1);
            if (l) {
                l->data = rf->file_utf8;
                l = g_list_next(l);
            }
            else {
                controls->recent_file_list
                    = g_list_append(controls->recent_file_list,
                                    rf->file_utf8);
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

static void
gwy_app_recent_file_create_dirs(void)
{
    const gchar *base;
    gchar *dir;

    base = gwy_recent_file_thumbnail_dir();
    if (!g_file_test(base, G_FILE_TEST_IS_DIR)) {
        gwy_debug("Creating base thumbnail directory <%s>", base);
        mkdir(base, 0700);
    }

    dir = g_build_filename(base, "normal", NULL);
    if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) {
        gwy_debug("Creating normal thumbnail directory <%s>", dir);
        mkdir(dir, 0700);
    }
    g_free(dir);
}

/* XXX: eats arguments! */
static GwyRecentFile*
gwy_recent_file_new(gchar *filename_utf8,
                    gchar *filename_sys)
{
    GError *err = NULL;
    GwyRecentFile *rf;

    g_return_val_if_fail(filename_utf8 || filename_sys, NULL);

    if (!filename_utf8)
        filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                           NULL, NULL, NULL);
    if (!filename_sys)
        filename_sys = g_filename_from_utf8(filename_utf8, -1,
                                            NULL, NULL, NULL);

    rf = g_new0(GwyRecentFile, 1);
    rf->file_utf8 = filename_utf8;
    rf->file_sys = filename_sys;
    if (!(rf->file_uri = g_filename_to_uri(filename_sys, NULL, &err))) {
        /* TODO: recovery ??? */
        rf->thumb_state = FILE_STATE_FAILED;
        g_clear_error(&err);
        return rf;
    }
    rf->thumb_sys = gwy_recent_file_thumbnail_name(rf->file_uri);

    return rf;
}

static void
gwy_recent_file_free(GwyRecentFile *rf)
{
    gwy_object_unref(rf->pixbuf);
    g_free(rf->file_utf8);
    g_free(rf->file_sys);
    g_free(rf->file_uri);
    g_free(rf->thumb_sys);
    g_free(rf);
}

static gboolean
recent_file_try_load_thumbnail(GwyRecentFile *rf)
{
    GdkPixbuf *pixbuf;
    gint width, height;
    const gchar *option;
    struct stat st;
    gdouble scale;

    gwy_debug("<%s>", rf->thumb_sys);
    rf->thumb_state = FILE_STATE_FAILED;
    gwy_object_unref(rf->pixbuf);

    if (!rf->thumb_sys) {
        rf->pixbuf = (GdkPixbuf*)g_object_ref(gcontrols.failed_pixbuf);
        return FALSE;
    }

    pixbuf = gdk_pixbuf_new_from_file(rf->thumb_sys, NULL);
    if (!pixbuf) {
        rf->pixbuf = (GdkPixbuf*)g_object_ref(gcontrols.failed_pixbuf);
        return FALSE;
    }
    gwy_debug_objects_creation(G_OBJECT(pixbuf));

    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    scale = (gdouble)THUMB_SIZE/MAX(width, height);
    width = CLAMP((gint)(scale*width), 1, THUMB_SIZE);
    height = CLAMP((gint)(scale*height), 1, THUMB_SIZE);
    rf->pixbuf = gdk_pixbuf_scale_simple(pixbuf, width, height,
                                         GDK_INTERP_TILES);

    option = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_URI);
    gwy_debug("uri = <%s>", rf->file_uri);
    if (!option || strcmp(option, rf->file_uri)) {
        g_warning("URI <%s> from thumb doesn't match <%s>. "
                  "If this isn't an MD5 collision, it's an implementation bug",
                  option, rf->file_uri);
    }

    if ((option = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_MTIME))) {
        sscanf(option, "%lu", &rf->thumb_mtime);
        gwy_debug("mtime = %lu", rf->thumb_mtime);
    }
    else
        rf->thumb_mtime = 0;

    if ((option = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_FILESIZE))) {
        sscanf(option, "%lu", &rf->file_size);
        gwy_debug("file size = %lu", rf->file_size);
    }
    else
        rf->file_size = 0;

    if ((option = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_IMAGE_WIDTH))) {
        sscanf(option, "%d", &rf->image_width);
        gwy_debug("image width = %d", rf->image_width);
    }
    else
        rf->image_width = 0;

    if ((option = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_IMAGE_HEIGHT))) {
        sscanf(option, "%d", &rf->image_height);
        gwy_debug("image height = %d", rf->image_height);
    }
    else
        rf->image_height = 0;

    if ((option = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_GWY_IMAGES))) {
        sscanf(option, "%d", &rf->image_images);
        gwy_debug("image images = %d", rf->image_images);
    }
    else
        rf->image_images = 0;

    if ((option = gdk_pixbuf_get_option(pixbuf, KEY_THUMB_GWY_GRAPHS))) {
        sscanf(option, "%d", &rf->image_graphs);
        gwy_debug("image graphs = %d", rf->image_graphs);
    }
    else
        rf->image_graphs = 0;

    if (stat(rf->file_sys, &st) == 0) {
        rf->file_state = FILE_STATE_OK;
        rf->file_mtime = st.st_mtime;
        if (rf->thumb_mtime != rf->file_mtime)
            rf->thumb_state = FILE_STATE_OLD;
        else
            rf->thumb_state = FILE_STATE_OK;
    }
    else {
        rf->thumb_state = FILE_STATE_OLD;
        rf->file_state = FILE_STATE_FAILED;
    }

    g_object_unref(pixbuf);
    gwy_debug("<%s> thumbnail loaded OK", rf->file_utf8);

    return TRUE;
}

static void
gwy_recent_file_update_thumbnail(GwyRecentFile *rf,
                                 GwyDataWindow *data_window)
{
    GtkWidget *data_view;
    GwyContainer *data;
    GwyDataField *dfield;
    GdkPixbuf *pixbuf;
    struct stat st;
    gchar *fnm, *str_mtime, *str_size, *str_width, *str_height;
    GError *err = NULL;

    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));

    if (stat(rf->file_sys, &st) != 0) {
        g_warning("File <%s> was just loaded or saved, but it doesn't seem to "
                  "exist any more: %s",
                  rf->file_utf8, g_strerror(errno));
        return;
    }

    if (rf->file_mtime == (gulong)st.st_mtime)
        return;

    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    rf->file_mtime = st.st_mtime;
    rf->file_size = st.st_size;
    rf->image_width = gwy_data_field_get_xres(dfield);
    rf->image_height = gwy_data_field_get_yres(dfield);
    rf->file_state = FILE_STATE_OK;

    /* FIXME: bad test, must end with / or nothing! */
    if (g_str_has_prefix(rf->file_sys, gwy_recent_file_thumbnail_dir())) {
        return;
    }

    pixbuf = gwy_data_view_get_pixbuf(GWY_DATA_VIEW(data_view),
                                      TMS_NORMAL_THUMB_SIZE,
                                      TMS_NORMAL_THUMB_SIZE);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));

    str_mtime = g_strdup_printf("%lu", rf->file_mtime);
    str_size = g_strdup_printf("%lu", rf->file_size);
    str_width = g_strdup_printf("%d", rf->image_width);
    str_height = g_strdup_printf("%d", rf->image_height);

    /* invent an unique temporary name for atomic save
     * FIXME: rough, but works on Win32 */
    fnm = g_strdup_printf("%s.%u", rf->thumb_sys, getpid());
    if (!gdk_pixbuf_save(pixbuf, fnm, "png", &err,
                         KEY_SOFTWARE, PACKAGE_NAME,
                         KEY_THUMB_URI, rf->file_uri,
                         KEY_THUMB_MTIME, str_mtime,
                         KEY_THUMB_FILESIZE, str_size,
                         KEY_THUMB_IMAGE_WIDTH, str_width,
                         KEY_THUMB_IMAGE_HEIGHT, str_height,
                         NULL)) {
        g_clear_error(&err);
        rf->thumb_state = FILE_STATE_FAILED;
    }
#ifndef G_OS_WIN32
    chmod(fnm, 0600);
#endif
    unlink(rf->thumb_sys);
    if (rename(fnm, rf->thumb_sys) != 0) {
        unlink(fnm);
        rf->thumb_state = FILE_STATE_FAILED;
        rf->thumb_mtime = 0;
    }
    else {
        rf->thumb_state = FILE_STATE_UNKNOWN;  /* force reload */
        rf->thumb_mtime = rf->file_mtime;
    }
    g_free(fnm);
    g_free(str_mtime);
    g_free(str_size);
    g_free(str_width);
    g_free(str_height);

    gwy_object_unref(rf->pixbuf);
    g_object_unref(pixbuf);
}

static gchar*
gwy_recent_file_thumbnail_name(const gchar *uri)
{
    static const gchar *hex2digit = "0123456789abcdef";
    guchar md5sum[16];
    gchar buffer[37], *p;
    gsize i;

    gwy_md5_get_digest(uri, -1, md5sum);
    p = buffer;
    for (i = 0; i < 16; i++) {
        *p++ = hex2digit[(guint)md5sum[i] >> 4];
        *p++ = hex2digit[(guint)md5sum[i] & 0x0f];
    }
    *p++ = '.';
    *p++ = 'p';
    *p++ = 'n';
    *p++ = 'g';
    *p = '\0';

    return g_build_filename(gwy_recent_file_thumbnail_dir(), "normal", buffer,
                            NULL);
}

static G_CONST_RETURN gchar*
gwy_recent_file_thumbnail_dir(void)
{
    const gchar *thumbdir =
#ifdef G_OS_WIN32
        "thumbnails";
#else
        ".thumbnails";
#endif
    static gchar *thumbnail_dir = NULL;

    if (thumbnail_dir)
        return thumbnail_dir;

    thumbnail_dir = g_build_filename(gwy_get_home_dir(), thumbdir, NULL);
    return thumbnail_dir;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
