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
#include <gtk/gtk.h>
#include "gwyapp.h"
#include "data-browser.h"

enum {
    COLUMN_FILETYPE,
    COLUMN_LABEL
};

typedef struct {
    GSList *list;
    GwyFileOperationType fileop;
} TypeListData;

static gboolean   confirm_overwrite              (GtkWidget *chooser);
static void       gwy_app_file_add_types         (GtkListStore *store,
                                                  GwyFileOperationType fileop);
static void       gwy_app_file_select_type       (GtkWidget *selector);
static gchar*     gwy_app_file_get_and_store_type(GtkWidget *selector);
static GtkWidget* gwy_app_file_types_new         (GtkFileChooserAction action);

static gchar *current_dir = NULL;

/*** Queer stuff that maybe even doesn't belong here ***********************/

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
    GtkWidget *data_window = NULL, *dialog, *data_browser;
    gboolean free_utf8 = FALSE, free_sys = FALSE;
    GwyContainer *data, *split_data;
    GError *err = NULL;
    gint i, channel_count;
    gchar *channel_key;

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
        gwy_app_data_browser(data);

        /*XXX: This code is total crap, but handy for now */
        /*
        channel_count = gwy_browser_get_n_channels(data);
        for (i=0; i<channel_count; i++) {
            channel_key = gwy_browser_get_channel_key(i);

            split_data = gwy_container_duplicate_by_prefix(data,
                                                           channel_key, NULL);
            gwy_container_rename(split_data,
                                 g_quark_from_string(channel_key),
                                 g_quark_from_static_string("/0/data"),
                                 TRUE);
            data_window = gwy_app_data_window_create(split_data);
            g_object_unref(split_data);
        }
        if (!data_window)
            data_window = gwy_app_data_window_create(data);
        */
        g_object_unref(data);

        gwy_app_recent_file_list_update(GWY_DATA_WINDOW(data_window),
                                        filename_utf8,
                                        filename_sys);
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

/**
 * gwy_app_file_open:
 *
 * Opens a user-selected file (very high-level app function).
 **/
void
gwy_app_file_open(void)
{
    GtkWidget *dialog, *types;
    GSList *filenames = NULL, *l;
    gchar *name;
    gint response;

    dialog = gtk_file_chooser_dialog_new(_("Open File"), NULL,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
    /* XXX: UTF-8 conversion missing */
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());
    types = gwy_app_file_types_new(GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), types);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    name = gwy_app_file_get_and_store_type(types);
    if (response == GTK_RESPONSE_OK)
        filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
    gtk_widget_destroy(dialog);

    for (l = filenames; l; l = g_slist_next(l)) {
        gwy_app_file_load(NULL, (const gchar*)l->data, name);
        g_free(l->data);
    }
    g_slist_free(filenames);
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

/**
 * gwy_app_file_save:
 *
 * Saves current data to a file (very high-level app function).
 *
 * May fall back to gwy_app_file_save_as() when current data has no file name
 * associated with it, or the format it was loaded from is not saveable.
 **/
void
gwy_app_file_save(void)
{
    GwyFileOperationType saveops;
    const gchar *name, *filename_sys;
    GwyContainer *data;

    data = gwy_app_get_current_data();
    g_return_if_fail(data);
    gwy_file_get_data_info(data, &name, &filename_sys);
    if (!name || !filename_sys) {
        gwy_app_file_save_as();
        return;
    }
    saveops = gwy_file_func_get_operations(name);
    if (!(saveops & (GWY_FILE_OPERATION_SAVE | GWY_FILE_OPERATION_EXPORT))) {
        gwy_app_file_save_as();
        return;
    }

    gwy_app_file_write(data, NULL, filename_sys, name);
}

/**
 * gwy_app_file_save:
 *
 * Saves current data to a user-selected file (very high-level app function).
 **/
void
gwy_app_file_save_as(void)
{
    GtkWidget *dialog, *types;
    gchar *name = NULL, *filename_sys = NULL, *filename_utf8;
    gint response;
    GwyContainer *data;

    data = gwy_app_get_current_data();
    g_return_if_fail(data);
    gwy_file_get_data_info(data, NULL, (const gchar**)&filename_sys);

    dialog = gtk_file_chooser_dialog_new(_("Save File"), NULL,
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
    /* XXX: UTF-8 conversion missing */
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());
    if (filename_sys) {
        filename_sys = g_path_get_basename(filename_sys);
        filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                          filename_utf8);
        g_free(filename_utf8);
        g_free(filename_sys);
    }
    types = gwy_app_file_types_new(GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), types);

    filename_sys = NULL;
    while (TRUE) {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        name = gwy_app_file_get_and_store_type(types);
        if (response != GTK_RESPONSE_OK)
            break;

        filename_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename_sys
            && (!g_file_test(filename_sys, G_FILE_TEST_EXISTS)
                || confirm_overwrite(dialog))) {
            gwy_app_file_write(data, NULL, filename_sys, name);
            break;
        }
        g_free(filename_sys);
        filename_sys = NULL;
    }
    gtk_widget_destroy(dialog);
    g_free(filename_sys);
    g_free(name);
}

static gboolean
confirm_overwrite(GtkWidget *chooser)
{
    GtkWidget *dialog;
    gchar *filename_sys, *filename_utf8, *dirname_sys, *dirname_utf8,
          *fullname_sys;
    gint response;

    fullname_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    g_return_val_if_fail(fullname_sys, FALSE);

    filename_sys = g_path_get_basename(fullname_sys);
    dirname_sys = g_path_get_dirname(fullname_sys);
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    dirname_utf8 = g_filename_to_utf8(dirname_sys, -1, NULL, NULL, NULL);

    dialog = gtk_message_dialog_new(GTK_WINDOW(chooser),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("File `%s' already exists.  Replace?"),
                                    filename_utf8);
    gtk_message_dialog_format_secondary_text
        (GTK_MESSAGE_DIALOG(dialog),
         _("The file already exists in `%s'.  "
           "Replacing it will overwrite its contents."),
         dirname_utf8);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Replace File?"));

    g_free(fullname_sys);
    g_free(filename_sys);
    g_free(dirname_sys);
    g_free(filename_utf8);
    g_free(dirname_utf8);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response == GTK_RESPONSE_YES;
}

/*** Common ****************************************************************/

static void
gwy_app_file_add_type(const gchar *name,
                      TypeListData *data)
{
    if (!(gwy_file_func_get_operations(name) & data->fileop))
        return;

    data->list = g_slist_prepend(data->list, (gpointer)name);
}

static gint
gwy_app_file_type_compare(gconstpointer a,
                          gconstpointer b)
{
    return g_utf8_collate(_(gwy_file_func_get_description((const gchar*)a)),
                          _(gwy_file_func_get_description((const gchar*)b)));
}

static void
gwy_app_file_add_types(GtkListStore *store,
                       GwyFileOperationType fileop)
{
    TypeListData tldata;
    GtkTreeIter iter;
    GSList *l;

    tldata.list = NULL;
    tldata.fileop = fileop;
    gwy_file_func_foreach((GFunc)gwy_app_file_add_type, &tldata);
    tldata.list = g_slist_sort(tldata.list, gwy_app_file_type_compare);

    for (l = tldata.list; l; l = g_slist_next(l)) {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           COLUMN_FILETYPE, l->data,
                           COLUMN_LABEL,
                           gettext(gwy_file_func_get_description(l->data)),
                           -1);
    }

    g_slist_free(tldata.list);
}

/**
 * gwy_app_file_select_type:
 * @selector: File type selection widget.
 *
 * Selects the same file type as the last time.
 *
 * If no information about last selection is available or the type is not
 * present any more, first combo item is selected.
 **/
static void
gwy_app_file_select_type(GtkWidget *selector)
{
    GtkWidget *types;
    GtkTreeModel *model;
    GtkTreeIter iter;
    const guchar *name;
    gboolean ok;
    gchar *s;
    GQuark key;

    types = g_object_get_data(G_OBJECT(selector), "combo");
    g_return_if_fail(GTK_IS_COMBO_BOX(types));
    key = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(selector), "key"));
    g_return_if_fail(key);

    ok = gwy_container_gis_string(gwy_app_settings_get(), key, &name);
    if (!ok) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(types), 0);
        return;
    }

    model = gtk_combo_box_get_model(GTK_COMBO_BOX(types));
    if (!gtk_tree_model_get_iter_first(model, &iter)) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(types), 0);
        return;
    }

    do {
        gtk_tree_model_get(model, &iter, COLUMN_FILETYPE, &s, -1);
        ok = gwy_strequal(name, s);
        g_free(s);
        if (ok) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(types), &iter);
            return;
        }
    } while (gtk_tree_model_iter_next(model, &iter));

    gtk_combo_box_set_active(GTK_COMBO_BOX(types), 0);
}

/**
 * gwy_app_file_get_and_store_type:
 * @selector: File type selection widget.
 *
 * Stores selected file type to settings and returns it.
 *
 * Returns: The selected file type name as a new string that must be freed
 *          by caller.  If nothing is selected (should not happen) or empty
 *          (automatic) file type is selected, %NULL is returned.
 **/
static gchar*
gwy_app_file_get_and_store_type(GtkWidget *selector)
{
    GtkWidget *types;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *s;
    GQuark key;

    types = g_object_get_data(G_OBJECT(selector), "combo");
    g_return_val_if_fail(GTK_IS_COMBO_BOX(types), NULL);
    key = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(selector), "key"));
    g_return_val_if_fail(key, NULL);

    if (!(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(types), &iter)))
        return NULL;
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(types));
    gtk_tree_model_get(model, &iter, COLUMN_FILETYPE, &s, -1);
    if (!*s) {
        g_free(s);
        gwy_container_remove(gwy_app_settings_get(), key);
        s = NULL;
    }
    else
        gwy_container_set_string(gwy_app_settings_get(), key, g_strdup(s));

    return s;
}

/**
 * gwy_app_file_types_new:
 * @action: File operation (only %GTK_FILE_CHOOSER_ACTION_OPEN and
 *          %GTK_FILE_CHOOSER_ACTION_SAVE are allowed).
 *
 * Creates file type selection widget for given action.
 *
 * Returns: A new file type selection widget.  Nothing should be assumed
 *          about its type, only gwy_app_file_get_and_store_type() and
 *          gwy_app_file_select_type() can be used to access it.
 **/
static GtkWidget*
gwy_app_file_types_new(GtkFileChooserAction action)
{
    GtkWidget *types, *hbox, *label;
    GtkCellRenderer *renderer;
    GtkListStore *store;
    GtkTreeIter iter;
    GQuark key;

    store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    switch (action) {
        case GTK_FILE_CHOOSER_ACTION_SAVE:
        key = g_quark_from_static_string("/app/file/save/type");
        gtk_list_store_set(store, &iter,
                           COLUMN_FILETYPE, "",
                           COLUMN_LABEL, _("Automatic by extension"),
                           -1);
        gwy_app_file_add_types(store, GWY_FILE_OPERATION_SAVE);
        gwy_app_file_add_types(store, GWY_FILE_OPERATION_EXPORT);
        break;

        case GTK_FILE_CHOOSER_ACTION_OPEN:
        key = g_quark_from_static_string("/app/file/load/type");
        gtk_list_store_set(store, &iter,
                           COLUMN_FILETYPE, "",
                           COLUMN_LABEL, _("Automatically detected"),
                           -1);
        gwy_app_file_add_types(store, GWY_FILE_OPERATION_LOAD);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    /* TODO: This needs usability improvements */
    types = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(types), 1);

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(types), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(types), renderer,
                                  "text", COLUMN_LABEL);

    hbox = gtk_hbox_new(FALSE, 8);
    label = gtk_label_new_with_mnemonic(_("File _type:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), types, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), types);

    g_object_set_data(G_OBJECT(hbox), "combo", types);
    g_object_set_data(G_OBJECT(hbox), "key", GUINT_TO_POINTER(key));
    gwy_app_file_select_type(hbox);
    gtk_widget_show_all(hbox);

    return hbox;
}

/************************** Documentation ****************************/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
