/* @(#) $Id$ */

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include "init.h"

GtkWidget*
create_aligned_menu(GtkItemFactoryEntry *menu_items,
                    gint nitems,
                    const gchar *root_path)
{
    GtkItemFactory *item_factory;
    GtkWidget *widget, *alignment;

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, root_path, NULL);
    gtk_item_factory_create_items(item_factory, nitems, menu_items, NULL);
    widget = gtk_item_factory_get_widget(item_factory, root_path);
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), widget);

    return alignment;
}

GtkWidget*
create_file_menu(void)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_File", NULL, NULL, 0, "<Branch>", NULL },
        { "/File/_New...", "<control>N", NULL, TRUE, "<StockItem>", GTK_STOCK_NEW },
        { "/File/_Open...", "<control>O", NULL, 0, "<StockItem>", GTK_STOCK_OPEN },
        { "/File/_Save", "<control>S", NULL, 0, "<StockItem>", GTK_STOCK_SAVE },
        { "/File/Save _As...", "<control><shift>S", NULL, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
    };

    return create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items), "<file>");
}

GtkWidget*
create_view_menu(void)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_View", NULL, NULL, 0, "<Branch>", NULL },
        { "/_View/Hide", NULL, NULL, 0, "<Item>", NULL },
        { "/_View/Hide better", NULL, NULL, 0, "<Item>", NULL },
    };

    return create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items), "<view>");
}

GtkWidget*
create_data_menu(void)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_Data Process", NULL, NULL, 0, "<Branch>", NULL },
        { "/_Data Process/Foo", NULL, NULL, 0, "<Item>", NULL },
        { "/_Data Process/Bar", NULL, NULL, 0, "<Item>", NULL },
        { "/_Data Process/Slow FT", NULL, NULL, 0, "<Item>", NULL },
        { "/_Data Process/Whatever", NULL, NULL, 0, "<Item>", NULL },
    };

    return create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items), "<data>");
}

void
foo(void)
{
    GtkItemFactory *item_factory;
    GtkWidget *window, *vbox, *toolbar, *widget, *alignment;

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
