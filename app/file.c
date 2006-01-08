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

static void              file_open_ok_cb       (GtkFileSelection *selector);
static void              file_save_as_ok_cb    (GtkFileSelection *selector);
static gboolean          file_real_open        (const gchar *filename_sys,
                                                const gchar *name);
static GtkFileSelection* create_save_as_dialog (const gchar *title,
                                                GCallback ok_callback);
static GtkFileSelection* create_open_dialog    (const gchar *title,
                                                GCallback ok_callback);
static gboolean          confirm_overwrite     (GtkWindow *parent,
                                                const gchar *filename);
static void              remove_data_window_callback (GtkWidget *selector,
                                                      GwyDataWindow *data_window);

static gchar *current_dir = NULL;

void
gwy_app_file_open_cb(void)
{
    GtkFileSelection *selector;

    selector = create_open_dialog(_("Open File"),
                                  G_CALLBACK(file_open_ok_cb));
    gtk_widget_show_all(GTK_WIDGET(selector));
}

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
    const gchar *filename_sys;  /* in system (disk) encoding */

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

void
gwy_app_file_import_cb(const gchar *name)
{
    GtkFileSelection *selector;

    selector = create_open_dialog(_("Import Data"),
                                  G_CALLBACK(file_open_ok_cb));
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

static GtkFileSelection*
create_open_dialog(const gchar *title,
                   GCallback ok_callback)
{
    GtkFileSelection *selector;

    selector = GTK_FILE_SELECTION(gtk_file_selection_new(title));
    gtk_file_selection_set_filename(selector, gwy_app_get_current_directory());

    g_signal_connect_swapped(selector->ok_button, "clicked",
                             G_CALLBACK(ok_callback), selector);
    g_signal_connect_swapped(selector->cancel_button, "clicked",
                             G_CALLBACK(gtk_widget_destroy), selector);

    return selector;
}

static void
file_open_ok_cb(GtkFileSelection *selector)
{
    const gchar *filename_sys;  /* in system (disk) encoding */
    const gchar *name;

    filename_sys = gtk_file_selection_get_filename(selector);
    if (!g_file_test(filename_sys,
                     G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return;

    name = (const gchar*)g_object_get_data(G_OBJECT(selector), "file-type");
    if (file_real_open(filename_sys, name))
        gtk_widget_destroy(GTK_WIDGET(selector));
}

void
gwy_app_file_open_recent_cb(GObject *item)
{
    const gchar *filename_utf8;  /* in UTF-8 */
    gchar *filename_sys;  /* in system (disk) encoding */

    filename_utf8 = g_object_get_data(G_OBJECT(item), "filename");
    g_return_if_fail(filename_utf8);
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    file_real_open(filename_sys, NULL);
    g_free(filename_sys);
}

static gboolean
file_real_open(const gchar *filename_sys,
               const gchar *name)
{
    GtkWidget *data_window;
    gchar *filename_utf8;  /* in UTF-8 */
    GwyContainer *data;

    if (name)
        data = gwy_file_func_run_load(name, filename_sys,
                                      GWY_RUN_INTERACTIVE, NULL);
    else
        data = gwy_file_load(filename_sys, GWY_RUN_INTERACTIVE, NULL);

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if (data) {
        gwy_container_set_string_by_name(data, "/filename", filename_utf8);
        data_window = gwy_app_data_window_create(data);
        gwy_app_recent_file_list_update(GWY_DATA_WINDOW(data_window),
                                        filename_utf8,
                                        filename_sys);
        g_object_unref(data);

        /* change directory to that of the loaded file */
        gwy_app_set_current_directory(filename_sys);
    }
    else {
        /* TODO: show some warning */
        g_warning("Open of <%s> failed", filename_utf8);
        g_free(filename_utf8);
    }

    return data != NULL;
}

static void
file_save_as_ok_cb(GtkFileSelection *selector)
{
    GtkWindow *data_window;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */
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

    if (saveop = GWY_FILE_OPERATION_SAVE) {
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

/**
 * gwy_app_file_open_initial:
 * @args: A file list.
 * @n: The number of items in @args.
 *
 * Opens a list of files [given on command line] and created recent-files
 * menu.
 *
 * Note the file names are assumed to be in system encoding, not UTF-8, but
 * who knows, what we get on the command line...
 **/
void
gwy_app_file_open_initial(gchar **args, gint n)
{
    gchar **p;
    gchar *cwd, *filename;

    /* FIXME: cwd is probably in UTF-8?! */
    cwd = g_get_current_dir();
    for (p = args; n; p++, n--) {
        if (g_path_is_absolute(*p))
            filename = g_strdup(*p);
        else
            filename = g_build_filename(cwd, *p, NULL);
        file_real_open(filename, NULL);
        g_free(filename);
    }
    g_free(cwd);
}

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

/************************** Documentation ****************************/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
