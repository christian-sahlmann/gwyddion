/* @(#) $Id$ */

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwymodule/gwymodulebrowser.h>
#include "app.h"
#include "file.h"
#include "arith.h"
#include "menu.h"

static GQuark sensitive_key = 0;
static GQuark sensitive_state_key = 0;

#define set_sensitive(item, sens) \
    g_object_set_qdata(G_OBJECT(item), sensitive_key, \
                       GUINT_TO_POINTER(sens))
#define set_sensitive_state(item, state) \
    g_object_set_qdata(G_OBJECT(item), sensitive_state_key, \
                       GUINT_TO_POINTER(state))
#define set_sensitive_both(item, sens, state) \
    do { \
        set_sensitive(item, sens); \
        set_sensitive_state(item, state); \
    } while (0)

static void       setup_sensitivity_keys       (void);
static void       gwy_menu_set_flags_recursive (GtkWidget *widget,
                                                GwyMenuSensitiveData *data);
static GtkWidget* gwy_menu_create_aligned_menu (GtkItemFactoryEntry *menu_items,
                                                gint nitems,
                                                const gchar *root_path,
                                                GtkAccelGroup *accel_group,
                                                GtkItemFactory **factory);

static GtkWidget*
gwy_menu_create_aligned_menu(GtkItemFactoryEntry *menu_items,
                             gint nitems,
                             const gchar *root_path,
                             GtkAccelGroup *accel_group,
                             GtkItemFactory **factory)
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
    if (factory)
        *factory = item_factory;

    return alignment;
}

GtkWidget*
gwy_menu_create_proc_menu(GtkAccelGroup *accel_group)
{
    GtkWidget *widget, *alignment;
    GtkObject *item_factory;
    GwyMenuSensitiveData sens_data = { GWY_MENU_FLAG_DATA, 0 };

    item_factory
        = gwy_build_process_menu(accel_group,
                                 G_CALLBACK(gwy_app_run_process_func_cb));
    widget = gtk_item_factory_get_widget(GTK_ITEM_FACTORY(item_factory),
                                         "<proc>");
    alignment = gtk_alignment_new(1.0, 1.5, 1.0, 1.0);
    gtk_container_add(GTK_CONTAINER(alignment), widget);

    /* set up sensitivity: all items need an active data window */
    setup_sensitivity_keys();
    gwy_menu_set_flags_recursive(widget, &sens_data);
    gwy_menu_set_sensitive_recursive(widget, &sens_data);

    return alignment;
}

GtkWidget*
gwy_menu_create_xtns_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/E_xterns", NULL, NULL, 0, "<Branch>", NULL },
        { "/Externs/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/Externs/Module browser", NULL, gwy_module_browser, 0, "<Item>", NULL },
    };

    setup_sensitivity_keys();
    return gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<xtns>", accel_group, NULL);
}

GtkWidget*
gwy_menu_create_file_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_File", NULL, NULL, 0, "<Branch>", NULL },
        { "/File/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/File/_Open...", "<control>O", gwy_app_file_open_cb, 0, "<StockItem>", GTK_STOCK_OPEN },
        { "/File/_Save", "<control>S", gwy_app_file_save_cb, 0, "<StockItem>", GTK_STOCK_SAVE },
        { "/File/Save _As...", "<control><shift>S", gwy_app_file_save_as_cb, 0, "<StockItem>", GTK_STOCK_SAVE_AS },
        { "/File/_Close", "<control>W", gwy_app_file_close_cb, 0, "<StockItem>", GTK_STOCK_CLOSE },
        { "/File/---", NULL, NULL, 0, "<Separator>", NULL },
        { "/File/_Quit...", "<control>Q", gwy_app_quit, 0, "<StockItem>", GTK_STOCK_QUIT },
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu, *item;
    GwyMenuSensitiveData sens_data;

    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<file>", accel_group, &item_factory);

    /* set up sensitivity  */
    setup_sensitivity_keys();
    item = gtk_item_factory_get_item(item_factory, "<file>/File/Save");
    set_sensitive(item, GWY_MENU_FLAG_DATA);
    item = gtk_item_factory_get_item(item_factory, "<file>/File/Save As...");
    set_sensitive(item, GWY_MENU_FLAG_DATA);
    item = gtk_item_factory_get_item(item_factory, "<file>/File/Close");
    set_sensitive(item, GWY_MENU_FLAG_DATA);
    sens_data.flags = GWY_MENU_FLAG_DATA;
    sens_data.set_to = 0;
    gwy_menu_set_sensitive_recursive(menu, &sens_data);

    return menu;
}

GtkWidget*
gwy_menu_create_edit_menu(GtkAccelGroup *accel_group)
{
    static GtkItemFactoryEntry menu_items[] = {
        { "/_Edit", NULL, NULL, 0, "<Branch>", NULL },
        { "/Edit/---", NULL, NULL, 0, "<Tearoff>", NULL },
        { "/Edit/_Undo", "<control>Z", NULL, 0, "<StockItem>", GTK_STOCK_UNDO },
        { "/Edit/_Redo", "<control>Y", NULL, 0, "<StockItem>", GTK_STOCK_REDO },
        { "/Edit/_Duplicate", "<control>D", gwy_app_file_duplicate_cb, 0, NULL, NULL },
        { "/Edit/Data _Arithmetic", NULL, gwy_app_data_arith, 0, NULL, NULL },
    };
    GtkItemFactory *item_factory;
    GtkWidget *menu, *item;
    GwyMenuSensitiveData sens_data;

    menu = gwy_menu_create_aligned_menu(menu_items, G_N_ELEMENTS(menu_items),
                                        "<edit>", accel_group, &item_factory);

    /* set up sensitivity  */
    setup_sensitivity_keys();
    item = gtk_item_factory_get_item(item_factory, "<edit>/Edit/Duplicate");
    set_sensitive(item, GWY_MENU_FLAG_DATA);
    item = gtk_item_factory_get_item(item_factory, "<edit>/Edit/Undo");
    set_sensitive(item, GWY_MENU_FLAG_UNDO);
    item = gtk_item_factory_get_item(item_factory, "<edit>/Edit/Redo");
    set_sensitive(item, GWY_MENU_FLAG_REDO);
    item = gtk_item_factory_get_item(item_factory, "<edit>/Edit/Data Arithmetic");
    set_sensitive(item, GWY_MENU_FLAG_DATA);
    sens_data.flags = GWY_MENU_FLAG_DATA
                      | GWY_MENU_FLAG_REDO
                      | GWY_MENU_FLAG_UNDO;
    sens_data.set_to = 0;
    gwy_menu_set_sensitive_recursive(menu, &sens_data);

    return menu;
}

void
gwy_menu_set_sensitive_recursive(GtkWidget *widget,
                                 GwyMenuSensitiveData *data)
{
    GObject *obj;
    guint i, j;

    obj = G_OBJECT(widget);
    i = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_key));
    if (i & data->flags) {
        j = GPOINTER_TO_UINT(g_object_get_qdata(obj, sensitive_state_key));
        j = (j & ~data->flags) | (data->set_to & data->flags);
        set_sensitive_state(obj, j);
        gtk_widget_set_sensitive(widget, (j & i) == i);

    }
    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU_BAR(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_menu_set_sensitive_recursive,
                              data);
    }
    else if (GTK_IS_MENU_ITEM(widget)) {
        if ((widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
            gtk_container_foreach(GTK_CONTAINER(widget),
                                  (GtkCallback)gwy_menu_set_sensitive_recursive,
                                  data);
    }
}

static void
gwy_menu_set_flags_recursive(GtkWidget *widget,
                             GwyMenuSensitiveData *data)
{
    if (!GTK_IS_TEAROFF_MENU_ITEM(widget))
        set_sensitive_both(widget, data->flags, data->set_to);

    if (GTK_IS_ALIGNMENT(widget)
        || GTK_IS_MENU_BAR(widget)) {
        gtk_container_foreach(GTK_CONTAINER(widget),
                              (GtkCallback)gwy_menu_set_flags_recursive,
                              data);
    }
    else if (GTK_IS_MENU_ITEM(widget)) {
        if ((widget = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget))))
            gtk_container_foreach(GTK_CONTAINER(widget),
                                  (GtkCallback)gwy_menu_set_flags_recursive,
                                  data);
    }
}

static void
setup_sensitivity_keys(void)
{
    if (!sensitive_key)
        sensitive_key = g_quark_from_static_string("sensitive");
    if (!sensitive_state_key)
        sensitive_state_key = g_quark_from_static_string("sensitive-state");
}

void
gwy_app_run_process_func_cb(gchar *name)
{
    GwyRunType run_types[] = {
        GWY_RUN_INTERACTIVE, GWY_RUN_MODAL,
        GWY_RUN_NONINTERACTIVE, GWY_RUN_WITH_DEFAULTS,
    };
    GwyRunType run;
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyContainer *data;
    gsize i;

    gwy_debug("%s: `%s'", __FUNCTION__, name);
    data_window = gwy_app_data_window_get_current();
    data_view = GWY_DATA_VIEW(gwy_data_window_get_data_view(data_window));
    data = gwy_data_view_get_data(data_view);
    g_return_if_fail(data);
    run = gwy_process_func_get_run_types(name);
    for (i = 0; i < G_N_ELEMENTS(run_types); i++) {
        if (run & run_types[i]) {
            gwy_process_func_run(name, data, run_types[i]);
            /* FIXME: the ugliest hack! */
            gwy_data_view_update(data_view);

            return;
        }
    }
    g_critical("Trying to run `%s', but no run mode found (%d)", name, run);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
