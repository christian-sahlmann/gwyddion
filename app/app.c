/* @(#) $Id$ */

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include "tools/tools.h"
#include "init.h"
#include "file.h"
#include "menu.h"
#include "settings.h"
#include "app.h"

/* TODO */
GtkWidget *gwy_app_main_window = NULL;
static GSList *current_data = NULL;

static const gchar *menu_list[] = {
    "<file>", "<proc>", "<xtns>", "<edit>",
};

void
gwy_app_quit(void)
{
    GwyDataWindow *data_window;

    gwy_debug("%s", __FUNCTION__);
    gwy_tools_crop_use(NULL);
    while ((data_window = gwy_app_get_current_data_window()))
        gtk_widget_destroy(GTK_WIDGET(data_window));

    gtk_main_quit();
}

static void
zoom_set_cb(GtkWidget *button, gpointer data)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_get_current_data_window();
    gwy_data_window_set_zoom(data_window, GPOINTER_TO_INT(data));
}

void
foo(void)
{
    GtkWidget *window, *vbox, *toolbar, *menu;
    GtkAccelGroup *accel_group;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gwy_app_main_window = window;

    accel_group = gtk_accel_group_new();
    g_object_set_data(G_OBJECT(window), "accel_group", accel_group);

    vbox = gtk_vbox_new(0, FALSE);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    menu = gwy_menu_create_file_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(window), "<file>", menu);

    menu = gwy_menu_create_edit_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(window), "<edit>", menu);

    menu = gwy_menu_create_proc_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(window), "<proc>", menu);

    menu = gwy_menu_create_xtns_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(window), "<xtns>", menu);

    /***************************************************************/
    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_ZOOM_IN,
                             "Zoom in", NULL,
                             GTK_SIGNAL_FUNC(zoom_set_cb),
                             GINT_TO_POINTER(1), -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_ZOOM_1_1,
                             "Zoom 1:1", NULL,
                             GTK_SIGNAL_FUNC(zoom_set_cb),
                             GINT_TO_POINTER(10000), -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_ZOOM_OUT,
                             "Zoom out", NULL,
                             GTK_SIGNAL_FUNC(zoom_set_cb),
                             GINT_TO_POINTER(-1), -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_ZOOM_FIT,
                             "Zoom to fit", NULL,
                             NULL, NULL, -1);

    /***************************************************************/
    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_POINTER,
                             "Pointer", NULL,
                             NULL, NULL, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_CROP,
                             "Crop", NULL,
                             NULL, NULL, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_SCALE,
                             "Scale", NULL,
                             NULL, NULL, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_ROTATE,
                             "Rotate", NULL,
                             NULL, NULL, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_SHADER,
                             "Shade", NULL,
                             NULL, NULL, -1);

    /***************************************************************/
    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_FIT_PLANE,
                             "Fit plane", NULL,
                             NULL, NULL, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_FIT_TRIANGLE,
                             "Fit plane using three points", NULL,
                             NULL, NULL, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_GRAPH,
                             "Graph", NULL,
                             NULL, NULL, -1);

    /***************************************************************/
    gtk_widget_show_all(window);
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

    /* XXX */
    g_signal_connect(window, "destroy", gwy_app_quit, NULL);
}

int
main(int argc, char *argv[])
{
    const gchar *module_dirs[] = { GWY_MODULE_DIR, NULL };
    GwyContainer *data;
    gchar *filename_utf8;
    gchar *config_file;
    gint i;

    gtk_init(&argc, &argv);
    config_file = g_build_filename(g_get_home_dir(), ".gwydrc", NULL);
    gwy_type_init();
    gwy_app_settings_load(config_file);
    gwy_module_register_modules(module_dirs);
    foo();
    for (i = 1; i < argc; i++) {
        if (!(data = gwy_file_load(argv[i])))
            continue;
        filename_utf8 = g_filename_to_utf8(argv[i], -1, NULL, NULL, NULL);
        gwy_container_set_string_by_name(data, "/filename", filename_utf8);
        gwy_app_create_data_window(data);
    }
    gtk_main();
    gwy_app_settings_save(config_file);

    return 0;
}

GwyDataWindow*
gwy_app_get_current_data_window(void)
{
    return current_data ? (GwyDataWindow*)current_data->data : NULL;
}

GwyContainer*
gwy_app_get_current_data(void)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;

    data_window = gwy_app_get_current_data_window();
    if (!data_window)
        return NULL;

    data_view = gwy_data_window_get_data_view(data_window);
    if (!data_view)
        return NULL;

    return gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
}

void
gwy_app_set_current_data_window(GwyDataWindow *window)
{
    GwyMenuSensitiveData sens_data;
    gsize i;
    gboolean update_state;

    gwy_debug("%s: %p", __FUNCTION__, window);
    if (window) {
        g_return_if_fail(GWY_IS_DATA_WINDOW(window));
        update_state = (current_data == NULL);
        current_data = g_slist_remove(current_data, window);
        current_data = g_slist_prepend(current_data, window);
    }
    else {
        if (!current_data)
            return;
        update_state = TRUE;
        current_data = g_slist_remove(current_data, current_data->data);
    }
    gwy_tools_crop_use(window);

    if (!update_state)
        return;

    sens_data.flags = GWY_MENU_FLAG_DATA;
    sens_data.set_to = current_data ? GWY_MENU_FLAG_DATA : 0;
    for (i = 0; i < G_N_ELEMENTS(menu_list); i++) {
        GtkWidget *menu = g_object_get_data(G_OBJECT(gwy_app_main_window),
                                            menu_list[i]);

        g_assert(menu);
        gtk_container_foreach(GTK_CONTAINER(menu),
                              (GtkCallback)gwy_menu_set_sensitive_recursive,
                              &sens_data);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
