/* @(#) $Id$ */

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include "init.h"

/* TODO */
static GSList *current_data = NULL;
static GtkWidget *gwy_app_main_window = NULL;

GwyDataWindow*  gwy_app_get_current_data_window  (void);
GwyContainer*   gwy_app_get_current_data         (void);
void            gwy_app_set_current_data_window  (GwyDataWindow *data_window);
static void     gwy_menu_set_sensitive_recursive (GtkWidget *widget,
                                                  gpointer data);

void
file_open_ok_cb(GtkFileSelection *selector)
{
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */
    GwyContainer *data;
    GtkWidget *data_window, *data_view;
    GwyDataViewLayer *layer;

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
}

void
file_open_cb(void)
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

void
file_save_as_cb(void)
{
    GtkFileSelection *selector;
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */

    data = gwy_app_get_current_data();
    g_return_if_fail(data);

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
file_save_cb(void)
{
    GwyContainer *data;
    const gchar *filename_utf8;  /* in UTF-8 */
    const gchar *filename_sys;  /* in system (disk) encoding */

    data = gwy_app_get_current_data();
    g_return_if_fail(data);

    if (gwy_container_contains_by_name(data, "/filename"))
        filename_utf8 = gwy_container_get_string_by_name(data, "/filename");
    else {
        file_save_as_cb();
        return;
    }
    gwy_debug("%s: %s", __FUNCTION__, filename_utf8);
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    if (!filename_sys || !*filename_sys || !gwy_file_save(data, filename_sys))
        file_save_as_cb();
}

GtkWidget*
create_aligned_menu(GtkItemFactoryEntry *menu_items,
                    gint nitems,
                    const gchar *root_path,
                    GtkAccelGroup *accel_group)
{
    GtkItemFactory *item_factory;
    GtkWidget *widget, *alignment;

    /* TODO must use one accel group for all menus, otherwise they don't work */
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, root_path,
                                        accel_group);
    gtk_item_factory_create_items(item_factory, nitems, menu_items, NULL);
    widget = gtk_item_factory_get_widget(item_factory, root_path);
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), widget);

    return alignment;
}

GtkWidget*
create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_File", NULL, NULL, 0, "<Branch>", NULL },
        { "/File/_Open...", "<control>O", file_open_cb, 0, "<StockItem>", GTK_STOCK_OPEN },
        { "/File/_Save", "<control>S", file_save_cb, 0, "<StockItem>", GTK_STOCK_SAVE },
        { "/File/Save _As...", "<control><shift>S", file_save_as_cb, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
        { "/File/---", NULL, NULL, 0, "<Separator>", NULL },
        { "/File/_Quit...", "<control>Q", gtk_main_quit, 0, "<StockItem>", GTK_STOCK_QUIT },
    };

    return create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items), "<file>",
                               accel_group);
}

GtkWidget*
create_view_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_View", NULL, NULL, 0, "<Branch>", NULL },
        { "/_View/Show modules", NULL, gwy_module_browser, 0, "<Item>", NULL },
    };

    return create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items), "<view>",
                               accel_group);
}

static void
run_process_func_cb(gchar *name,
                    guint cb_action,
                    GtkWidget *who_knows)
{
    GwyContainer *data;

    gwy_debug("first argument = %s", name);
    gwy_debug("second argument = %u", cb_action);
    gwy_debug("third argument = %p (%s)",
              who_knows, g_type_name(G_TYPE_FROM_INSTANCE(who_knows)));
    data = gwy_app_get_current_data();
    g_return_if_fail(data);
    gwy_run_process_func(name, data, GWY_RUN_NONINTERACTIVE);
}

GtkWidget*
create_data_menu(GtkAccelGroup *accel_group)
{
    GtkItemFactory *item_factory;
    GtkWidget *widget, *alignment;

    item_factory = GTK_ITEM_FACTORY(gwy_build_process_menu(
                                        accel_group,
                                        G_CALLBACK(run_process_func_cb)));
    widget = gtk_item_factory_get_widget(item_factory, "<data>");
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), widget);

    return alignment;
}

static void
gwy_app_quit(void)
{
    GwyDataWindow *data_window;

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

    menu = create_file_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

    menu = create_view_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

    menu = create_data_menu(accel_group);
    gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);
    gtk_container_foreach(GTK_CONTAINER(menu),
                          gwy_menu_set_sensitive_recursive, FALSE);
    g_object_set_data(G_OBJECT(window), "<data>", menu);

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_SMALL_TOOLBAR);

    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_ZOOM_IN,
                             "Zoom in", NULL,
                             GTK_SIGNAL_FUNC(zoom_set_cb),
                             GINT_TO_POINTER(1), 0);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_ZOOM_100,
                             "Zoom 1:1", NULL,
                             GTK_SIGNAL_FUNC(zoom_set_cb),
                             GINT_TO_POINTER(10000), 1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_ZOOM_OUT,
                             "Zoom out", NULL,
                             GTK_SIGNAL_FUNC(zoom_set_cb),
                             GINT_TO_POINTER(-1), 2);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    gtk_widget_show_all(window);
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

    /* XXX */
    g_signal_connect(window, "destroy", gwy_app_quit, NULL);
}

int
main(int argc, char *argv[])
{
    const gchar *module_dirs[] = { GWY_MODULE_DIR, NULL };

    gtk_init(&argc, &argv);
    gwy_type_init();
    gwy_module_register_modules(module_dirs);
    foo();
    gtk_main();

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

static void
gwy_menu_set_sensitive_recursive(GtkWidget *widget,
                                 gpointer data)
{
    gboolean sensitive = GPOINTER_TO_INT(data);

    gtk_widget_set_sensitive(widget, sensitive);
    if (GTK_IS_MENU_BAR(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              gwy_menu_set_sensitive_recursive, data);
    }
    else if (GTK_IS_MENU_ITEM(widget)) {
        if ((widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
            gtk_container_foreach(GTK_CONTAINER(widget),
                                  gwy_menu_set_sensitive_recursive, data);
    }
}

void
gwy_app_set_current_data_window(GwyDataWindow *window)
{
    GtkWidget *data_process_menu;
    gboolean update_state;

    if (window) {
        g_return_if_fail(GWY_IS_DATA_WINDOW(window));
        update_state = (current_data == NULL);
        current_data = g_slist_remove(current_data, window);
        current_data = g_slist_prepend(current_data, window);
    }
    else {
        update_state = (current_data != NULL);
        current_data = g_slist_remove(current_data, current_data->data);
    }

    data_process_menu = g_object_get_data(G_OBJECT(gwy_app_main_window),
                                          "<data>");
    if (update_state)
        gtk_container_foreach(GTK_CONTAINER(data_process_menu),
                              gwy_menu_set_sensitive_recursive,
                              GINT_TO_POINTER(current_data != NULL));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
