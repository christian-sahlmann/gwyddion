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

static GList *current_data = NULL;
static GwyToolUseFunc current_tool_use_func = NULL;

static const gchar *menu_list[] = {
    "<file>", "<proc>", "<xtns>", "<edit>",
};

static GtkWidget* gwy_app_toolbar_append_tool(GtkWidget *toolbar,
                                              GtkWidget *radio,
                                              const gchar *stock_id,
                                              const gchar *tooltip,
                                              GwyToolUseFunc tool_use_func);
static void       gwy_app_use_tool_cb        (GtkWidget *unused,
                                              GwyToolUseFunc tool_use_func);
static void      gwy_app_update_toolbox_state(GwyMenuSensitiveData *sens_data);

void
gwy_app_quit(void)
{
    GwyDataWindow *data_window;

    gwy_debug("%s", __FUNCTION__);
    /* current_tool_use_func(NULL); */
    while ((data_window = gwy_app_data_window_get_current()))
        gtk_widget_destroy(GTK_WIDGET(data_window));

    gtk_main_quit();
}

static void
zoom_set_cb(GtkWidget *button, gpointer data)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    gwy_data_window_set_zoom(data_window, GPOINTER_TO_INT(data));
}

void
gwy_app_create_toolbox(void)
{
    GtkWidget *window, *vbox, *toolbar, *menu, *grp, *button;
    GtkAccelGroup *accel_group;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_wmclass(GTK_WINDOW(window), "toolbox",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
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

    /***************************************************************/
    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    button = gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_FIT_PLANE,
                                      "Automatically level data", NULL,
                                      NULL, NULL, -1);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_run_process_func_cb),
                             "level");
    button = gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_SCALE,
                                      "Rescale data", NULL,
                                      NULL, NULL, -1);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_run_process_func_cb),
                             "scale");
    button = gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GWY_STOCK_ROTATE,
                                      "Rotate data", NULL,
                                      NULL, NULL, -1);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_run_process_func_cb),
                             "rotate");

    /***************************************************************/
    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    grp = gwy_app_toolbar_append_tool(toolbar, NULL, GWY_STOCK_POINTER_MEASURE,
                                      _("Pointer tooltip"),
                                      gwy_tool_pointer_use);
    gwy_app_toolbar_append_tool(toolbar, grp, GWY_STOCK_CROP,
                                _("Crop tooltip"),
                                gwy_tool_crop_use);
    gwy_app_toolbar_append_tool(toolbar, grp, GWY_STOCK_SHADER,
                                _("Shader tooltip"),
                                NULL);
    gwy_app_toolbar_append_tool(toolbar, grp, GWY_STOCK_FIT_TRIANGLE,
                                _("Fit plane using three points"),
                                gwy_tool_level3_use);
    gwy_app_toolbar_append_tool(toolbar, grp, GWY_STOCK_GRAPH,
                                _("Graph tooltip"),
                                NULL);

    /***************************************************************/
    gtk_widget_show_all(window);
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

    /* XXX */
    g_signal_connect(window, "destroy", gwy_app_quit, NULL);
}

int
main(int argc, char *argv[])
{
    const gchar *module_dirs[] = {
        GWY_MODULE_DIR "/file",
        GWY_MODULE_DIR "/process",
        NULL
    };
    GwyContainer *data;
    gchar *filename_utf8;
    gchar *config_file;
    gint i;

    gtk_init(&argc, &argv);
    config_file = g_build_filename(g_get_home_dir(), ".gwydrc", NULL);
    gwy_type_init();
    gwy_app_settings_load(config_file);
    gwy_module_register_modules(module_dirs);
    gwy_app_create_toolbox();
    for (i = 1; i < argc; i++) {
        if (!(data = gwy_file_load(argv[i])))
            continue;
        filename_utf8 = g_filename_to_utf8(argv[i], -1, NULL, NULL, NULL);
        gwy_container_set_string_by_name(data, "/filename", filename_utf8);
        gwy_app_data_window_create(data);
    }
    gtk_main();
    gwy_app_settings_save(config_file);

    return 0;
}

GwyDataWindow*
gwy_app_data_window_get_current(void)
{
    return current_data ? (GwyDataWindow*)current_data->data : NULL;
}

GwyContainer*
gwy_app_get_current_data(void)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;

    data_window = gwy_app_data_window_get_current();
    if (!data_window)
        return NULL;

    data_view = gwy_data_window_get_data_view(data_window);
    if (!data_view)
        return NULL;

    return gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
}

/**
 * Add a data window and make it current data window.
 **/
void
gwy_app_data_window_set_current(GwyDataWindow *window)
{
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_DATA, GWY_MENU_FLAG_DATA };
    gboolean update_state;
    GList *item;

    gwy_debug("%s: %p", __FUNCTION__, window);

    g_return_if_fail(GWY_IS_DATA_WINDOW(window));
    update_state = (current_data == NULL);
    item = g_list_find(current_data, window);
    if (item) {
        current_data = g_list_remove_link(current_data, item);
        current_data = g_list_concat(item, current_data);
    }
    else
        current_data = g_list_prepend(current_data, window);
    /* FIXME: this calls the use function a little bit too often */
    if (current_tool_use_func)
        current_tool_use_func(window);

    if (update_state)
        gwy_app_update_toolbox_state(&sens_data);
}

void
gwy_app_data_window_remove(GwyDataWindow *window)
{
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_DATA, 0 };
    GList *item;

    g_return_if_fail(GWY_IS_DATA_WINDOW(window));

    item = g_list_find(current_data, window);
    if (!item) {
        g_critical("Trying to remove GwyDataWindow %p not present in the list",
                   window);
        return;
    }
    current_data = g_list_delete_link(current_data, item);
    if (current_data) {
        gwy_app_data_window_set_current(GWY_DATA_WINDOW(current_data->data));
        return;
    }

    if (current_tool_use_func)
        current_tool_use_func(NULL);
    gwy_app_update_toolbox_state(&sens_data);
}

static void
gwy_app_update_toolbox_state(GwyMenuSensitiveData *sens_data)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(menu_list); i++) {
        GtkWidget *menu = g_object_get_data(G_OBJECT(gwy_app_main_window),
                                            menu_list[i]);

        g_assert(menu);
        gtk_container_foreach(GTK_CONTAINER(menu),
                              (GtkCallback)gwy_menu_set_sensitive_recursive,
                              sens_data);
    }
}

/**
 * Assures @window is present in the data window list, but doesn't make
 * it current.
 *
 * XXX: WTF?
 **/
void
gwy_app_data_window_add(GwyDataWindow *window)
{
    gwy_debug("%s: %p", __FUNCTION__, window);

    g_return_if_fail(GWY_IS_DATA_WINDOW(window));

    if (g_list_find(current_data, window))
        return;

    current_data = g_list_append(current_data, window);
}

void
gwy_app_data_window_foreach(GFunc func,
                            gpointer user_data)
{
    GList *l;

    for (l = current_data; l; l = g_list_next(l))
        func(l->data, user_data);
}

static GtkWidget*
gwy_app_toolbar_append_tool(GtkWidget *toolbar,
                            GtkWidget *radio,
                            const gchar *stock_id,
                            const gchar *tooltip,
                            GwyToolUseFunc tool_use_func)
{
    GtkWidget *icon;
    GtkStockItem stock_item;

    g_return_val_if_fail(GTK_IS_TOOLBAR(toolbar), NULL);
    g_return_val_if_fail(stock_id, NULL);
    g_return_val_if_fail(tooltip, NULL);

    if (!gtk_stock_lookup(stock_id, &stock_item)) {
        g_warning("Couldn't find item for stock id `%s'", stock_id);
        stock_item.label = "???";
    }
    icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    return gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
                                      GTK_TOOLBAR_CHILD_RADIOBUTTON, radio,
                                      stock_item.label, tooltip, NULL, icon,
                                      GTK_SIGNAL_FUNC(gwy_app_use_tool_cb),
                                      tool_use_func);
}

static void
gwy_app_use_tool_cb(GtkWidget *unused,
                    GwyToolUseFunc tool_use_func)
{
    GwyDataWindow *data_window;

    gwy_debug("%s: %p", __FUNCTION__, tool_use_func);
    if (current_tool_use_func)
        current_tool_use_func(NULL);
    current_tool_use_func = tool_use_func;
    if (tool_use_func) {
        data_window = gwy_app_data_window_get_current();
        if (data_window)
            current_tool_use_func(data_window);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
