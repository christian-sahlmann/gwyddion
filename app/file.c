/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>

#include <string.h>

#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkmessagedialog.h>
#include "gwyapp.h"

enum {
    COLUMN_FILETYPE,
    COLUMN_LABEL
};

static void              file_save_as_ok_cb    (GtkFileSelection *selector);
static GtkFileSelection* create_save_as_dialog (const gchar *title,
                                                GCallback ok_callback);
static gboolean          confirm_overwrite     (GtkWindow *parent,
                                                const gchar *filename);
static void              remove_data_window_callback (GtkWidget *selector,
                                                      GwyDataWindow *data_window);

static gchar *current_dir = NULL;

/* FIXME: I'd like to close Ctrl-W also other windows, but 3D View hangs
 * (again), and sending "delete_event" signal does nothing. */
void
gwy_app_file_close_cb(void)
{
    GtkWidget *window;

    window = gwy_app_get_current_window(GWY_APP_WINDOW_TYPE_ANY);
    gwy_debug("current is %p: %s", window,
              g_type_name(G_TYPE_FROM_INSTANCE(window)));
    if (!GWY_IS_DATA_WINDOW(window))
        return;
    gtk_widget_destroy(window);
}

void
gwy_app_file_save_as_cb(void)
{
    GtkFileSelection *selector;

    selector = create_save_as_dialog(_("Save File"),
                                     G_CALLBACK(file_save_as_ok_cb));
    if (!selector)
        return;
    gtk_widget_show_all(GTK_WIDGET(selector));
}

void
gwy_app_file_save_cb(void)
{
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in GLib encoding */

    /* FIXME: if a graph window is active, save its associated data (cannot
     * do until 1.6), if a 3D window is active, save its associated data */
    data = gwy_app_get_current_data();
    g_return_if_fail(GWY_IS_CONTAINER(data));

    if (gwy_container_contains_by_name(data, "/filename"))
        filename_utf8 = gwy_container_get_string_by_name(data, "/filename");
    else {
        gwy_app_file_save_as_cb();
        return;
    }
    gwy_debug("%s", filename_utf8);
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    if (!filename_sys
        || !*filename_sys
        || !gwy_file_save(data, filename_sys, GWY_RUN_INTERACTIVE, NULL))
        gwy_app_file_save_as_cb();
    else
        gwy_undo_container_set_unmodified(data);
}

void
gwy_app_file_duplicate_cb(void)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyLayerBasic *layer;
    GwyContainer *data;
    GObject *show;
    const gchar *key;

    data_window = gwy_app_data_window_get_current();
    data_view = gwy_data_window_get_data_view(data_window);
    layer = GWY_LAYER_BASIC(gwy_data_view_get_base_layer(data_view));
    data = gwy_container_duplicate(gwy_data_view_get_data(data_view));
    /* XXX: An ugly and largery undocumented hack: if a presentation has
     * "is_preview" set, it is a preview and is not duplicated.  But at least
     * it works ... so we are better than Gimp! Better than Gimp! :o)  */
    if (gwy_layer_basic_get_has_presentation(layer)) {
        key = gwy_layer_basic_get_presentation_key(layer);
        show = gwy_container_get_object_by_name(data, key);
        if (g_object_get_data(show, "is_preview"))
            gwy_container_remove_by_name(data, key);
    }
    data_window = GWY_DATA_WINDOW(gwy_app_data_window_create(data));
    gwy_app_data_window_set_untitled(data_window, NULL);
    g_object_unref(data);
}

void
gwy_app_file_export_cb(const gchar *name)
{
    GtkFileSelection *selector;

    selector = create_save_as_dialog(_("Export Data"),
                                     G_CALLBACK(file_save_as_ok_cb));
    if (!selector)
        return;
    gtk_file_selection_set_filename(selector, gwy_app_get_current_directory());
    g_object_set_data(G_OBJECT(selector), "file-type", (gpointer)name);
    gtk_widget_show_all(GTK_WIDGET(selector));
}

static GtkFileSelection*
create_save_as_dialog(const gchar *title,
                      GCallback ok_callback)
{
    GtkFileSelection *selector;
    GwyDataWindow *data_window;
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */

    data_window = gwy_app_data_window_get_current();
    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    data = gwy_app_get_current_data();
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    selector = GTK_FILE_SELECTION(gtk_file_selection_new(title));
    if (gwy_container_contains_by_name(data, "/filename")) {
        gchar *filename_sys;

        filename_utf8 = gwy_container_get_string_by_name(data, "/filename");
        filename_sys = g_filename_from_utf8(filename_utf8, -1,
                                            NULL, NULL, NULL);
        gtk_file_selection_set_filename(selector, filename_sys);
        g_free(filename_sys);
    }
    else
        gtk_file_selection_set_filename(selector,
                                        gwy_app_get_current_directory());
    g_object_set_data(G_OBJECT(selector), "data", data);
    g_object_set_data(G_OBJECT(selector), "window", data_window);

    g_signal_connect_swapped(selector->ok_button, "clicked",
                             ok_callback, selector);
    g_signal_connect_swapped(selector->cancel_button, "clicked",
                             G_CALLBACK(gtk_widget_destroy), selector);
    g_signal_connect(selector, "destroy",
                     G_CALLBACK(remove_data_window_callback), data_window);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(gtk_widget_destroy), selector);

    return selector;
}

static void
file_save_as_ok_cb(GtkFileSelection *selector)
{
    GtkWindow *data_window;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in GLib encoding */
    GwyContainer *data;
    const gchar *name;
    GwyFileOperationType saveop;

    data = GWY_CONTAINER(g_object_get_data(G_OBJECT(selector), "data"));
    g_return_if_fail(GWY_IS_CONTAINER(data));
    data_window = GTK_WINDOW(g_object_get_data(G_OBJECT(selector), "window"));
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));

    name = (const gchar*)g_object_get_data(G_OBJECT(selector), "file-type");

    filename_sys = gtk_file_selection_get_filename(selector);
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if (g_file_test(filename_sys,
                     G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK)) {
        if (!confirm_overwrite(GTK_WINDOW(selector), filename_utf8))
            return;
    }
    else if (g_file_test(filename_sys, G_FILE_TEST_EXISTS)) {
        g_warning("Not a regular file `%s'", filename_utf8);
        return;
    }

    /* FIXME: why we do not just run gwy_file_save()? */
    if (name) {
        if (gwy_file_func_run_save(name, data, filename_sys,
                                   GWY_RUN_INTERACTIVE, NULL))
            saveop = GWY_FILE_OPERATION_SAVE;
        else {
            if (!gwy_file_func_run_export(name, data, filename_sys,
                                          GWY_RUN_INTERACTIVE, NULL))
                return;
            saveop = GWY_FILE_OPERATION_EXPORT;
        }
    }
    else {
        saveop = gwy_file_save(data, filename_sys, GWY_RUN_INTERACTIVE, NULL);
        if (!saveop)
            return;
    }

    if (saveop == GWY_FILE_OPERATION_SAVE) {
        gwy_undo_container_set_unmodified(data);
        gwy_container_set_string_by_name(data, "/filename", filename_utf8);
        gwy_container_remove_by_name(data, "/filename/untitled");
        gwy_app_recent_file_list_update(GWY_DATA_WINDOW(data_window),
                                        filename_utf8,
                                        filename_sys);
    }

    gtk_widget_destroy(GTK_WIDGET(selector));

    /* change directory to that of the saved file */
    gwy_app_set_current_directory(filename_sys);
}

static gboolean
confirm_overwrite(GtkWindow *parent,
                  const gchar *filename)
{
    GtkWidget *dialog;
    gint response;

    dialog = gtk_message_dialog_new(parent,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("File %s already exists. Overwrite?"),
                                    filename);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Overwrite?"));
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response == GTK_RESPONSE_YES;
}

static void
remove_data_window_callback(GtkWidget *selector,
                            GwyDataWindow *data_window)
{
    g_signal_handlers_disconnect_matched(selector,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         remove_data_window_callback,
                                         data_window);
    g_signal_handlers_disconnect_matched(data_window,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gtk_widget_destroy,
                                         selector);

}

/*** Current directory handling ********************************************/

/**
 * gwy_app_get_current_directory:
 *
 * Returns what the app uses as `current directory'.
 *
 * Warning: This function is probably temporary.
 *
 * Returns: A string in GLib file name encoding that should not be modified
 *          neither freed, valid only until next call to
 *          gwy_app_set_current_directory().  It ends with a
 *          %G_DIR_SEPARATOR_S.
 **/
const gchar*
gwy_app_get_current_directory(void)
{
    if (!current_dir) {
        gchar *s = g_get_current_dir();

        gwy_app_set_current_directory(s);
        g_free(s);
    }

    return current_dir;
}

/**
 * gwy_app_set_current_directory:
 * @directory: The directory to set, or a filename to take directory part
 *             from, it must be an absolute path.  In GLib file name encoding.
 *
 * Sets what the app should use as `current directory'.
 *
 * Warning: This function is probably temporary.
 **/
void
gwy_app_set_current_directory(const gchar *directory)
{
    g_return_if_fail(directory);

    if (g_file_test(directory, G_FILE_TEST_IS_DIR)) {
        g_free(current_dir);
        if (g_str_has_suffix(directory, G_DIR_SEPARATOR_S))
            current_dir = g_strdup(directory);
        else
            current_dir = g_strconcat(directory, G_DIR_SEPARATOR_S, NULL);
    }
    else if (g_file_test(directory, G_FILE_TEST_EXISTS)) {
        gchar *s;

        g_free(current_dir);
        s = g_path_get_dirname(directory);
        /* "/" */
        if (g_str_has_suffix(s, G_DIR_SEPARATOR_S))
            current_dir = s;
        else {
            current_dir = g_strconcat(s, G_DIR_SEPARATOR_S, NULL);
            g_free(s);
        }
    }
    else
        g_warning("Invalid or nonexistent directory `%s'", directory);
}

/*** File loading **********************************************************/

/**
 * gwy_app_file_load:
 * @filename_utf8: Name of file to load, in UTF-8.
 * @filename_sys: Name of file to load, in GLib encoding.
 * @name: File type to open file as, but normally %NULL to automatically
 *        detect from file contents.
 *
 * Loads a file into application (a high-level function).
 *
 * At least one of @filename_utf8, @filename_sys must be non-%NULL.
 *
 * The file is loaded in interactive mode, modules can ask for argument.
 * Upon a successful load all necessary setup tasks are performed.  If the
 * load fails, an error dialog is presented.
 *
 * Returns: Container of the just loaded file on success, %NULL on failure.
 **/
GwyContainer*
gwy_app_file_load(const gchar *filename_utf8,
                  const gchar *filename_sys,
                  const gchar *name)
{
    GtkWidget *data_window, *dialog, *data_browser;
    gboolean free_utf8 = FALSE, free_sys = FALSE;
    GwyContainer *data;
    GError *err = NULL;

    g_return_val_if_fail(filename_utf8 || filename_sys, NULL);
    if (!filename_sys) {
        filename_sys = g_filename_from_utf8(filename_utf8, -1,
                                            NULL, NULL, NULL);
        if (!filename_sys) {
            g_warning("FIXME: file name not convertible to system encoding");
            return NULL;
        }
        free_sys = TRUE;
    }

    if (!filename_utf8) {
        filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                           NULL, NULL, NULL);
        free_utf8 = TRUE;
    }

    if (name)
        data = gwy_file_func_run_load(name, filename_sys,
                                      GWY_RUN_INTERACTIVE, &err);
    else
        data = gwy_file_load(filename_sys, GWY_RUN_INTERACTIVE, &err);

    if (data) {
        if (free_utf8) {
            gwy_container_set_string_by_name(data, "/filename", filename_utf8);
            free_utf8 = FALSE;
        }
        else
            gwy_container_set_string_by_name(data, "/filename",
                                             g_strdup(filename_utf8));

        /* TODO: replace by browser construction */
        data_window = gwy_app_data_window_create(data);
        data_browser = gwy_app_data_browser_create(data);

        gwy_app_recent_file_list_update(GWY_DATA_WINDOW(data_window),
                                        filename_utf8,
                                        filename_sys);
        g_object_unref(data);
        gwy_app_set_current_directory(filename_sys);
    }
    else {
        if (err->code != GWY_MODULE_FILE_ERROR_CANCELLED) {
            dialog = gtk_message_dialog_new(NULL, 0,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Opening of `%s' failed"),
                                            filename_utf8);
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "%s", err->message);
            g_signal_connect(dialog, "response",
                             G_CALLBACK(gtk_widget_destroy), NULL);
            gtk_widget_show_all(dialog);
            gtk_window_present(GTK_WINDOW(dialog));
            g_clear_error(&err);
        }
    }

    if (free_sys)
        g_free((gpointer)filename_sys);
    if (free_utf8)
        g_free((gpointer)filename_utf8);

    return data;
}

static void
gwy_app_file_load_add(const gchar *name,
                      GtkListStore *store)
{
    GtkTreeIter iter;

    if (!(gwy_file_func_get_operations(name) & GWY_FILE_OPERATION_LOAD))
        return;

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_FILETYPE, name,
                       COLUMN_LABEL,
                       gettext(gwy_file_func_get_description(name)),
                       -1);
}

void
gwy_app_file_open(const gchar *title)
{
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkWidget *dialog, *types, *hbox, *label;
    GtkTreeIter iter;
    GSList *filenames = NULL, *l;
    gchar *name;
    gint response;

    if (!title)
        title = _("Open File");
    dialog = gtk_file_chooser_dialog_new(title, NULL,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                         NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
    /* XXX: UTF-8 conversion missing */
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());

    /* TODO: This needs usability improvements */
    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_FILETYPE, "",
                       COLUMN_LABEL, _("Detect automatically"),
                       -1);
    gwy_file_func_foreach((GFunc)gwy_app_file_load_add, store);
    types = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(types), 1);
    /* TODO: remember in settings */
    gtk_combo_box_set_active(GTK_COMBO_BOX(types), 0);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(types), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(types), renderer,
                                  "text", COLUMN_LABEL);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store),
                                         COLUMN_FILETYPE, GTK_SORT_ASCENDING);

    hbox = gtk_hbox_new(FALSE, 4);
    label = gtk_label_new_with_mnemonic(_("File _type:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), types, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), types);
    gtk_widget_show_all(hbox);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), hbox);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        gtk_combo_box_get_active_iter(GTK_COMBO_BOX(types), &iter);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                           COLUMN_FILETYPE, &name,
                           -1);
        if (!*name) {
            g_free(name);
            name = NULL;
        }
        filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
    }
    gtk_widget_destroy(dialog);

    for (l = filenames; l; l = g_slist_next(l)) {
        gwy_app_file_load(NULL, (const gchar*)l->data, name);
        g_free(l->data);
    }
    g_slist_free(filenames);
    g_object_unref(store);
    g_free(name);
}

/*** File saving ***********************************************************/

/**
 * gwy_app_file_write:
 * @data: Data to write.
 * @filename_utf8: Name of file to write data to, in UTF-8.
 * @filename_sys: Name of file to write data to, in GLib encoding.
 * @name: File type to open file as, but normally %NULL to automatically detect
 *        from file name.
 *
 * Writes container to a file (a high-level function).
 *
 * At least one of @filename_utf8, @filename_sys must be non-%NULL.
 *
 * The file is saved in interactive mode, modules can ask for argument.
 * If the write fails, an error dialog is presented.
 *
 * Returns: %TRUE on success.
 **/
gboolean
gwy_app_file_write(GwyContainer *data,
                   const gchar *filename_utf8,
                   const gchar *filename_sys,
                   const gchar *name)
{
    GtkWidget *dialog;
    gboolean free_utf8 = FALSE, free_sys = FALSE;
    GwyFileOperationType saveok;
    GError *err = NULL;

    g_return_val_if_fail(filename_utf8 || filename_sys, FALSE);
    if (!filename_sys) {
        filename_sys = g_filename_from_utf8(filename_utf8, -1,
                                            NULL, NULL, NULL);
        if (!filename_sys) {
            g_warning("FIXME: file name not convertible to system encoding");
            return FALSE;
        }
        free_sys = TRUE;
    }

    if (!filename_utf8) {
        filename_utf8 = g_filename_to_utf8(filename_sys, -1,
                                           NULL, NULL, NULL);
        free_utf8 = TRUE;
    }

    if (name) {
        saveok = gwy_file_func_get_operations(name);
        if (saveok & GWY_FILE_OPERATION_SAVE
            && gwy_file_func_run_save(name, data, filename_sys,
                                      GWY_RUN_INTERACTIVE, &err))
            saveok = GWY_FILE_OPERATION_SAVE;
        else if (saveok & GWY_FILE_OPERATION_EXPORT
                 && gwy_file_func_run_export(name, data, filename_sys,
                                             GWY_RUN_INTERACTIVE, &err))
            saveok = GWY_FILE_OPERATION_EXPORT;
        else
            saveok = 0;
    }
    else
        saveok = gwy_file_save(data, filename_sys, GWY_RUN_INTERACTIVE, &err);

    switch (saveok) {
        case GWY_FILE_OPERATION_SAVE:
        if (free_utf8) {
            gwy_container_set_string_by_name(data, "/filename", filename_utf8);
            free_utf8 = FALSE;
        }
        else
            gwy_container_set_string_by_name(data, "/filename",
                                             g_strdup(filename_utf8));

        /* FIXME: get rid of GwyDataWindowism */
        gwy_app_recent_file_list_update(gwy_app_data_window_get_for_data(data),
                                        filename_utf8,
                                        filename_sys);

        case GWY_FILE_OPERATION_EXPORT:
        gwy_app_set_current_directory(filename_sys);
        break;

        default:
        if (err->code != GWY_MODULE_FILE_ERROR_CANCELLED) {
            dialog = gtk_message_dialog_new(NULL, 0,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Saving of `%s' failed"),
                                            filename_utf8);
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "%s", err->message);
            g_signal_connect(dialog, "response",
                             G_CALLBACK(gtk_widget_destroy), NULL);
            gtk_widget_show_all(dialog);
            gtk_window_present(GTK_WINDOW(dialog));
            g_clear_error(&err);
        }
        break;
    }

    if (free_sys)
        g_free((gpointer)filename_sys);
    if (free_utf8)
        g_free((gpointer)filename_utf8);

    return saveok != 0;
}

/************************** Documentation ****************************/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
