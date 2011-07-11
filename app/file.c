/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwydgetutils.h>
#include <gtk/gtk.h>
#include <app/gwyapp.h>
#include "gwyappfilechooser.h"

typedef struct {
    const gchar *funcname;
    const gchar *modname;
    const GwyModuleInfo *modinfo;
} FindFileFuncModuleData;

static void          warn_broken_load_func     (const gchar *name,
                                                GwyContainer *data);
static GwyContainer* gwy_app_file_load_real    (const gchar *filename_utf8,
                                                const gchar *filename_sys,
                                                const gchar *name,
                                                gboolean do_add_loaded);
static void          gwy_app_file_add_loaded   (GwyContainer *data,
                                                const gchar *filename_utf8,
                                                const gchar *filename_sys);
static void          gwy_app_file_open_or_merge(gboolean merge);

static gchar *current_dir = NULL;

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
 * The file is loaded in interactive mode, modules can ask for user input.
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
    GwyContainer *data;

    data = gwy_app_file_load_real(filename_utf8, filename_sys, name, TRUE);
    /* gwy_app_file_add_loaded() takes a reference therefore we can release
     * the initial one */
    if (data) {
        g_object_unref(data);
        warn_broken_load_func(name, data);
    }

    return data;
}

static void
find_file_func_author(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    const gchar *modname = (const gchar*)hkey;
    const GwyModuleInfo *modinfo = (const GwyModuleInfo*)hvalue;
    FindFileFuncModuleData *ffdata = (FindFileFuncModuleData*)user_data;
    GSList *l;

    l = gwy_module_get_functions(modname);
    while (l) {
        const gchar *funcname = (const gchar*)l->data;
        if (g_str_has_prefix(funcname, "file::")
            && gwy_strequal(funcname + sizeof("file::") - 1,
                            ffdata->funcname)) {
            ffdata->modname = modname;
            ffdata->modinfo = modinfo;
            return;
        }
        l = g_slist_next(l);
    }
}

static void
warn_broken_load_func(const gchar *name,
                      GwyContainer *data)
{
    static const gchar *broken_file_funcs[] = {
        "ambfile",
        /*"at present no modules are broken and this is not a file type name",*/
    };

    FindFileFuncModuleData ffdata;
    const gchar *description;
    GtkWidget *dialog;
    guint i;

    if (!name) {
        g_return_if_fail(GWY_IS_CONTAINER(data));
        gwy_file_get_data_info(data, &name, NULL);
        g_return_if_fail(name);
    }

    for (i = 0; i < G_N_ELEMENTS(broken_file_funcs); i++) {
        if (gwy_strequal(name, broken_file_funcs[i]))
            break;
    }
    if (i == G_N_ELEMENTS(broken_file_funcs))
        return;

    description = gwy_file_func_get_description(name);
    gwy_clear(&ffdata, 1);
    ffdata.funcname = name;
    gwy_module_foreach(find_file_func_author, &ffdata);
    g_return_if_fail(ffdata.modinfo && ffdata.modname);

    dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_WARNING,
                                    GTK_BUTTONS_OK,
                                    _("Imported data are likely incorrect."));

    gtk_message_dialog_format_secondary_text
        (GTK_MESSAGE_DIALOG(dialog),
         _("Import support for files of type\n\n"
           "%s\n\n"
           "is incomplete due to the lack of documentation, "
           "testing files and/or people willing to help with the testing.\n\n"
           "If you want to help to improve the import please contact the "
           "author of module %s-%s:\n\n"
           "%s"),
       _(description), ffdata.modname, ffdata.modinfo->version,
       ffdata.modinfo->author);

    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
}

static GwyContainer*
gwy_app_file_load_real(const gchar *filename_utf8,
                       const gchar *filename_sys,
                       const gchar *name,
                       gboolean do_add_loaded)
{
    GtkWidget *dialog;
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
        data = gwy_file_load_with_func(filename_sys, GWY_RUN_INTERACTIVE,
                                       &name, &err);

    if (data) {
        gwy_data_validate(data,
                          GWY_DATA_VALIDATE_CORRECT
                          | GWY_DATA_VALIDATE_NO_REPORT);
        if (do_add_loaded)
            gwy_app_file_add_loaded(data, filename_utf8, filename_sys);
    }
    else {
        if (err && !g_error_matches(err,
                                    GWY_MODULE_FILE_ERROR,
                                    GWY_MODULE_FILE_ERROR_CANCELLED)) {
            gchar *filename_disp = g_filename_display_basename(filename_utf8);
            GString *message = g_string_new(NULL);

            dialog = gtk_message_dialog_new(NULL, 0,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Opening of `%s' failed"),
                                            filename_disp);
            g_free(filename_disp);
            g_string_append(message, err->message);
            g_string_append(message, "\n\n");
            g_string_append_printf(message,
                                   _("Full file path: %s."), filename_utf8);
            if (name) {
                g_string_append(message, "\n\n");
                g_string_append_printf(message, _("Loaded using: %s."), name);
            }
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "%s", message->str);
            g_string_free(message, TRUE);
            g_signal_connect(dialog, "response",
                             G_CALLBACK(gtk_widget_destroy), NULL);
            gtk_widget_show_all(dialog);
            gtk_window_present(GTK_WINDOW(dialog));
            g_clear_error(&err);
        }
        else if (!err)
            g_warning("A file module failed to report error properly.");
    }

    if (free_sys)
        g_free((gpointer)filename_sys);
    if (free_utf8)
        g_free((gpointer)filename_utf8);

    return data;
}

static void
gwy_app_file_add_loaded(GwyContainer *data,
                        const gchar *filename_utf8,
                        const gchar *filename_sys)
{
    if (filename_utf8)
        gwy_container_set_string_by_name(data, "/filename",
                                         g_strdup(filename_utf8));

    gwy_app_data_browser_add(data);
    gwy_app_data_browser_reset_visibility(data,
                                          GWY_VISIBILITY_RESET_DEFAULT);
    if (filename_utf8)
        gwy_app_recent_file_list_update(data, filename_utf8, filename_sys, 0);
    gwy_app_set_current_directory(filename_sys);
}

/**
 * gwy_app_file_open:
 *
 * Opens a user-selected file (very high-level app function).
 **/
void
gwy_app_file_open(void)
{
    gwy_app_file_open_or_merge(FALSE);
}

/**
 * gwy_app_file_merge:
 *
 * Merges a user-selected file (very high-level app function).
 *
 * Since: 2.7
 **/
void
gwy_app_file_merge(void)
{
    gwy_app_file_open_or_merge(TRUE);
}

static void
gwy_app_file_open_or_merge(gboolean merge)
{
    GtkWidget *dialog;
    GSList *filenames = NULL, *l;
    gchar *name;
    gint response;

    dialog = _gwy_app_file_chooser_get(GTK_FILE_CHOOSER_ACTION_OPEN);
    if (merge)
        gtk_window_set_title(GTK_WINDOW(dialog), _("Merge File"));
    else
        gtk_window_set_title(GTK_WINDOW(dialog), _("Open File"));

    /* XXX: UTF-8 conversion missing */
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                        gwy_app_get_current_directory());

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    name = _gwy_app_file_chooser_get_selected_type(GWY_APP_FILE_CHOOSER(dialog));
    if (response == GTK_RESPONSE_OK)
        filenames = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
    gtk_widget_hide(dialog);

    for (l = filenames; l; l = g_slist_next(l)) {
        gchar *fname_sys = (gchar*)l->data;

        if (merge) {
            GwyContainer *data;

            data = gwy_app_file_load_real(NULL, fname_sys, name, FALSE);
            if (data) {
                gwy_app_data_browser_merge(data);
                g_object_unref(data);
                warn_broken_load_func(name, data);
            }
        }
        else
            gwy_app_file_load(NULL, fname_sys, name);

        g_free(fname_sys);
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
 * The file is saved in interactive mode, modules can ask for user input.
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
    GwyContainer *container;
    GtkWidget *dialog;
    gboolean free_utf8 = FALSE, free_sys = FALSE;
    GwyFileOperationType saveok;
    GError *err = NULL;
    gint id;

    /* If the @data is the current container, make thumbnail from the current
     * channel.
     * FIXME: Needs new data browser functions to do this less hackishly. */
    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &container,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    if (data != container)
        id = 0;

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
        saveok = gwy_file_save_with_func(data, filename_sys,
                                         GWY_RUN_INTERACTIVE, &name, &err);

    switch (saveok) {
        case GWY_FILE_OPERATION_SAVE:
        if (free_utf8) {
            gwy_undo_container_set_unmodified(data);
            gwy_container_set_string_by_name(data, "/filename", filename_utf8);
            free_utf8 = FALSE;
        }
        else
            gwy_container_set_string_by_name(data, "/filename",
                                             g_strdup(filename_utf8));
        gwy_app_recent_file_list_update(data, filename_utf8, filename_sys, id);

        case GWY_FILE_OPERATION_EXPORT:
        gwy_app_set_current_directory(filename_sys);
        break;

        default:
        if (err && !g_error_matches(err,
                                    GWY_MODULE_FILE_ERROR,
                                    GWY_MODULE_FILE_ERROR_CANCELLED)) {
            gchar *filename_disp = g_filename_display_basename(filename_utf8);
            GString *message = g_string_new(NULL);

            dialog = gtk_message_dialog_new(NULL, 0,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Saving of `%s' failed"),
                                            filename_disp);
            g_free(filename_disp);
            g_string_append(message, err->message);
            g_string_append(message, "\n\n");
            g_string_append_printf(message,
                                   _("Full file path: %s."), filename_utf8);
            if (name) {
                g_string_append(message, "\n\n");
                g_string_append_printf(message, _("Saved using: %s."), name);
            }
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                                     "%s", message->str);
            g_string_free(message, TRUE);
            g_signal_connect(dialog, "response",
                             G_CALLBACK(gtk_widget_destroy), NULL);
            gtk_widget_show_all(dialog);
            gtk_window_present(GTK_WINDOW(dialog));
            g_clear_error(&err);
        }
        else if (!err)
            g_warning("A file module failed to report error properly.");
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

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
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
 * gwy_app_file_save_as:
 *
 * Saves current data to a user-selected file (very high-level app function).
 **/
void
gwy_app_file_save_as(void)
{
    GtkWidget *dialog;
    gchar *name = NULL, *filename_sys = NULL, *filename_utf8;
    gint response;
    GwyContainer *data;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, 0);
    g_return_if_fail(data);
    gwy_file_get_data_info(data, NULL, (const gchar**)&filename_sys);

    dialog = _gwy_app_file_chooser_get(GTK_FILE_CHOOSER_ACTION_SAVE);
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

    filename_sys = NULL;
    while (TRUE) {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        name = _gwy_app_file_chooser_get_selected_type(GWY_APP_FILE_CHOOSER(dialog));
        if (response != GTK_RESPONSE_OK)
            break;

        filename_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename_sys && gwy_app_file_confirm_overwrite(dialog)) {
            gwy_app_file_write(data, NULL, filename_sys, name);
            break;
        }
        g_free(filename_sys);
        filename_sys = NULL;
    }
    gtk_widget_hide(dialog);
    g_free(filename_sys);
    g_free(name);
}

/**
 * gwy_app_file_confirm_overwrite:
 * @chooser: A file chooser for save action.
 *
 * Asks for file overwrite for a file save chooser.
 *
 * Returns: %TRUE if it is OK to overwrite the file, %FALSE when user cancelled
 *          it or there was other problem.
 **/
gboolean
gwy_app_file_confirm_overwrite(GtkWidget *chooser)
{
    GtkWidget *dialog, *toplevel;
    gchar *filename_sys, *filename_utf8, *dirname_sys, *dirname_utf8,
          *fullname_sys;
    gint response;

    g_return_val_if_fail(gtk_file_chooser_get_action(GTK_FILE_CHOOSER(chooser))
                         == GTK_FILE_CHOOSER_ACTION_SAVE,
                         FALSE);
    fullname_sys = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    if (!fullname_sys)
        return FALSE;
    if (!g_file_test(fullname_sys, G_FILE_TEST_EXISTS))
        return TRUE;

    filename_sys = g_path_get_basename(fullname_sys);
    dirname_sys = g_path_get_dirname(fullname_sys);
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    dirname_utf8 = g_filename_to_utf8(dirname_sys, -1, NULL, NULL, NULL);

    toplevel = gtk_widget_get_toplevel(chooser);
    dialog = gtk_message_dialog_new(GTK_WINDOW(toplevel),
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

/************************** Documentation ****************************/

/**
 * SECTION:file
 * @title: file
 * @short_description: High level file functions
 *
 * High-level functions gwy_app_file_load() and gwy_app_file_write()
 * wrap low-level file handling functions from
 * <link linkend="libgwymodule-gwymodule-file">gwymodule-file</link>
 * and handle registration of loaded containers in data browser, showing
 * windows, remembering file types, or the differenced between save and export.
 *
 * They are complemented by application-level functions gwy_app_file_open(),
 * gwy_app_file_save(), and gwy_app_file_save_as() that perform the
 * corresponding operations of File menu.  These are probably not of general
 * interest.
 *
 * Beside that, functions to maintain application-level idea of current
 * directory are provided: gwy_app_get_current_directory(),
 * gwy_app_set_current_directory().  They should be used in place of system
 * chdir() which has various unwanted side-effect, like change of the directory
 * where core is dumped on segfault.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
