/* @(#) $Id$ */

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwylayer-basic.h>
#include <gtk/gtkfilesel.h>
#include "app.h"
#include "file.h"
#include "tools/tools.h"

static void file_open_ok_cb            (GtkFileSelection *selector);
static void file_save_as_ok_cb         (GtkFileSelection *selector);
static void gwy_app_create_data_window (GwyContainer *data);

void
gwy_app_file_open_cb(void)
{
    GtkFileSelection *selector;

    selector = GTK_FILE_SELECTION(gtk_file_selection_new("Open file"));
    gtk_file_selection_set_filename(selector, "");

    g_signal_connect_swapped(selector->ok_button, "clicked",
                             G_CALLBACK(file_open_ok_cb), selector);
    g_signal_connect_swapped(selector->cancel_button, "clicked",
                             G_CALLBACK(gtk_widget_destroy), selector);

    gtk_widget_show_all(GTK_WIDGET(selector));
}

void
gwy_app_file_save_as_cb(void)
{
    GtkFileSelection *selector;
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */

    data = gwy_app_get_current_data();
    g_return_if_fail(GWY_IS_CONTAINER(data));

    selector = GTK_FILE_SELECTION(gtk_file_selection_new("Save file as"));
    if (gwy_container_contains_by_name(data, "/filename"))
        filename_utf8 = gwy_container_get_string_by_name(data, "/filename");
    else
        filename_utf8 = "";
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    gtk_file_selection_set_filename(selector, filename_sys);
    g_object_set_data(G_OBJECT(selector), "data", data);

    g_signal_connect_swapped(selector->ok_button, "clicked",
                             G_CALLBACK(file_save_as_ok_cb), selector);
    g_signal_connect_swapped(selector->cancel_button, "clicked",
                             G_CALLBACK(gtk_widget_destroy), selector);

    gtk_widget_show_all(GTK_WIDGET(selector));
}

void
gwy_app_file_save_cb(void)
{
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */

    data = gwy_app_get_current_data();
    g_return_if_fail(GWY_IS_CONTAINER(data));

    if (gwy_container_contains_by_name(data, "/filename"))
        filename_utf8 = gwy_container_get_string_by_name(data, "/filename");
    else {
        gwy_app_file_save_as_cb();
        return;
    }
    gwy_debug("%s: %s", __FUNCTION__, filename_utf8);
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    if (!filename_sys || !*filename_sys || !gwy_file_save(data, filename_sys))
        gwy_app_file_save_as_cb();
}

void
gwy_app_file_duplicate_cb(void)
{
    GwyContainer *data, *duplicate;

    data = gwy_app_get_current_data();
    g_return_if_fail(GWY_IS_CONTAINER(data));
    duplicate = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    g_return_if_fail(GWY_IS_CONTAINER(duplicate));
    gwy_container_remove_by_name(duplicate, "/filename");
    gwy_app_create_data_window(duplicate);
}

static void
file_open_ok_cb(GtkFileSelection *selector)
{
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */
    GwyContainer *data;

    filename_sys = gtk_file_selection_get_filename(selector);
    if (!g_file_test(filename_sys,
                     G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
        return;

    data = gwy_file_load(filename_sys);
    if (!data)
        return;

    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    gwy_container_set_string_by_name(data, "/filename", filename_utf8);
    gtk_widget_destroy(GTK_WIDGET(selector));
    gwy_app_create_data_window(data);
}

/* FIXME: to be moved somewhere? refactored? */
static void
gwy_app_create_data_window(GwyContainer *data)
{
    GtkWidget *data_window, *data_view;
    GwyDataViewLayer *layer;

    data_view = gwy_data_view_new(data);
    layer = (GwyDataViewLayer*)gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(data_view), layer);

    data_window = gwy_data_window_new(GWY_DATA_VIEW(data_view));
    gtk_window_add_accel_group(GTK_WINDOW(data_window),
                               g_object_get_data(G_OBJECT(gwy_app_main_window),
                                                 "accel_group"));
    g_signal_connect(data_window, "focus-in-event",
                     G_CALLBACK(gwy_app_set_current_data_window), NULL);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(gwy_app_set_current_data_window), NULL);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(g_object_unref), data);

    gtk_widget_show_all(data_window);
    gwy_tools_crop_use(data_window);
}

static void
file_save_as_ok_cb(GtkFileSelection *selector)
{
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */
    GwyContainer *data;

    data = (GwyContainer*)g_object_get_data(G_OBJECT(selector), "data");
    g_assert(GWY_IS_CONTAINER(data));

    filename_sys = gtk_file_selection_get_filename(selector);
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if (g_file_test(filename_sys,
                     G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK)) {
        g_warning("Won't overwrite `%s' (yet)", filename_utf8);
        return;
    }
    if (g_file_test(filename_sys, G_FILE_TEST_EXISTS)) {
        g_warning("Not a regular file `%s'", filename_utf8);
        return;
    }

    if (!gwy_file_save(data, filename_sys))
        return;

    gwy_container_set_string_by_name(data, "/filename", filename_utf8);
    gtk_widget_destroy(GTK_WIDGET(selector));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
