/* @(#) $Id$ */

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include "init.h"

/* TODO */
static GwyContainer *current_data = NULL;

GwyContainer* gwy_app_get_current_data (void);
void          gwy_app_set_current_data (GwyContainer *data);

GtkWidget*
create_aligned_menu(GtkItemFactoryEntry *menu_items,
                    gint nitems,
                    const gchar *root_path)
{
    GtkItemFactory *item_factory;
    GtkWidget *widget, *alignment;

    /* TODO must use one accel group for all menus, otherwise they don't work */
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, root_path, NULL);
    gtk_item_factory_create_items(item_factory, nitems, menu_items, NULL);
    widget = gtk_item_factory_get_widget(item_factory, root_path);
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), widget);

    return alignment;
}

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
    g_signal_connect_swapped(data_window, "focus-in-event",
                             G_CALLBACK(gwy_app_set_current_data), data);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(gwy_app_set_current_data), NULL);
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
create_file_menu(void)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_File", NULL, NULL, 0, "<Branch>", NULL },
        { "/File/_Open...", "<control>O", file_open_cb, 0, "<StockItem>", GTK_STOCK_OPEN },
        { "/File/_Save", "<control>S", file_save_cb, 0, "<StockItem>", GTK_STOCK_SAVE },
        { "/File/Save _As...", "<control><shift>S", file_save_as_cb, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
        { "/File/---", NULL, NULL, 0, "<Separator>", NULL },
        { "/File/_Quit...", "<control>Q", gtk_main_quit, 0, "<StockItem>", GTK_STOCK_QUIT },
    };

    return create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items), "<file>");
}

GtkWidget*
create_view_menu(void)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_View", NULL, NULL, 0, "<Branch>", NULL },
        { "/_View/Show modules", NULL, gwy_module_browser, 0, "<Item>", NULL },
    };

    return create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items), "<view>");
}

GtkWidget*
create_data_menu(void)
{
    GtkItemFactory *item_factory;
    GtkWidget *widget, *alignment;

    item_factory = GTK_ITEM_FACTORY(gwy_build_process_menu());
    widget = gtk_item_factory_get_widget(item_factory, "<data>");
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), widget);

    return alignment;
}

void
foo(void)
{
    GtkWidget *window, *vbox, *toolbar;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    vbox = gtk_vbox_new(0, FALSE);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    gtk_box_pack_start(GTK_BOX(vbox), create_file_menu(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_view_menu(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), create_data_menu(), FALSE, FALSE, 0);

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_SMALL_TOOLBAR);

    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_ZOOM_IN,
                             "Zoom in", NULL,
                             GTK_SIGNAL_FUNC(NULL), NULL, 0);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_ZOOM_100,
                             "Zoom 1:1", NULL,
                             GTK_SIGNAL_FUNC(NULL), NULL, 0);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_ZOOM_OUT,
                             "Zoom out", NULL,
                             GTK_SIGNAL_FUNC(NULL), NULL, 0);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, TRUE, TRUE, 0);

    gtk_widget_show_all(window);

    /* XXX */
    g_signal_connect(window, "destroy", gtk_main_quit, NULL);
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

GwyContainer*
gwy_app_get_current_data(void)
{
    return current_data;
}

void
gwy_app_set_current_data(GwyContainer *data)
{
    if (data) {
        gwy_debug("%s: %s",
                  __FUNCTION__, g_type_name(G_TYPE_FROM_INSTANCE(data)));
        g_return_if_fail(GWY_IS_CONTAINER(data));
    }
    else
        gwy_debug("%s: NULL", __FUNCTION__);
    current_data = data;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
