/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include "tools/tools.h"
#include "init.h"
#include "file.h"
#include "menu.h"
#include "settings.h"
#include "app.h"

/* TODO XXX FIXME fuck shit braindamaged silly stupid ugly broken borken
 * (the previous line is here for grep) */
typedef struct {
    GQuark key;
    GObject *data;
} GwyAppFuckingUndo;

GtkWidget *gwy_app_main_window = NULL;

static GList *current_data = NULL;
static GwyToolUseFunc current_tool_use_func = NULL;
static gint untitled_no = 0;

static const gchar *menu_list[] = {
    "<file>", "<proc>", "<xtns>", "<edit>",
};

static void       gwy_app_quit                (void);
void              gwy_app_create_toolbox      (void);
static GtkWidget* gwy_app_toolbar_append_tool (GtkWidget *toolbar,
                                               GtkWidget *radio,
                                               const gchar *stock_id,
                                               const gchar *tooltip,
                                               GwyToolUseFunc tool_use_func);
static void       gwy_app_use_tool_cb         (GtkWidget *unused,
                                               GwyToolUseFunc tool_use_func);
static void       gwy_app_update_toolbox_state(GwyMenuSensitiveData *sens_data);
static gint       compare_data_window_data_cb (GwyDataWindow *window,
                                               GwyContainer *data);
static void       undo_redo_clean             (GObject *window,
                                               gboolean undo,
                                               gboolean redo);

int
main(int argc, char *argv[])
{
    const gchar *module_dirs[] = {
        GWY_MODULE_DIR "/file",
        GWY_MODULE_DIR "/process",
        NULL
    };
    gchar *config_file;

    gtk_init(&argc, &argv);
    config_file = g_build_filename(g_get_home_dir(), ".gwydrc", NULL);
    gwy_type_init();
    gwy_app_settings_load(config_file);
    gwy_app_settings_get();
    gwy_module_register_modules(module_dirs);
    gwy_app_create_toolbox();
    gwy_app_file_open_initial(argv + 1);
    gtk_main();
    gwy_app_settings_save(config_file);

    return 0;
}

static void
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
    gtk_window_set_title(GTK_WINDOW(window), g_get_application_name());
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

GwyDataWindow*
gwy_app_data_window_get_current(void)
{
    return current_data ? (GwyDataWindow*)current_data->data : NULL;
}

GwyContainer*
gwy_app_get_current_data(void)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    if (!data_window)
        return NULL;

    return gwy_data_window_get_data(data_window);
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
    undo_redo_clean(G_OBJECT(window), TRUE, TRUE);
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

/* FIXME: to be moved somewhere? refactored? */
GtkWidget*
gwy_app_data_window_create(GwyContainer *data)
{
    GtkWidget *data_window, *data_view;
    GtkObject *layer;

    data_view = gwy_data_view_new(data);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(data_view),
                                 GWY_DATA_VIEW_LAYER(layer));
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        layer = gwy_layer_mask_new();
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(data_view),
                                      GWY_DATA_VIEW_LAYER(layer));
    }

    data_window = gwy_data_window_new(GWY_DATA_VIEW(data_view));
    gtk_window_add_accel_group(GTK_WINDOW(data_window),
                               g_object_get_data(G_OBJECT(gwy_app_main_window),
                                                 "accel_group"));
    g_signal_connect(data_window, "focus-in-event",
                     G_CALLBACK(gwy_app_data_window_set_current), NULL);
    g_signal_connect(data_window, "destroy",
                     G_CALLBACK(gwy_app_data_window_remove), NULL);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(g_object_unref), data);

    gwy_data_window_set_units(GWY_DATA_WINDOW(data_window), "m");
    gwy_data_window_update_title(GWY_DATA_WINDOW(data_window));
    gtk_window_present(GTK_WINDOW(data_window));

    return data_window;
}

/**
 * gwy_app_data_window_set_untitled:
 * @data_window: A data window.
 * @templ: A title template string.
 *
 * Clears any file name for @data_window and sets its "/filename/untitled"
 * data.
 *
 * The template tring @templ can be either %NULL, the window then gets a
 * title like "Untitled 37", or a string "Foo" not containing `%', the window
 * then gets a title like "Foo 42", or a string "Bar %d" containing a single
 * '%d', the window then gets a title like "Bar 666".
 *
 * Returns: The number that will appear in the title (probably useless).
 **/
gint
gwy_app_data_window_set_untitled(GwyDataWindow *data_window,
                                 const gchar *templ)
{
    GtkWidget *data_view;
    GwyContainer *data;
    gchar *title, *p;

    data_view = gwy_data_window_get_data_view(data_window);
    data = GWY_CONTAINER(gwy_data_view_get_data(GWY_DATA_VIEW(data_view)));
    gwy_container_remove_by_prefix(data, "/filename");
    untitled_no++;
    if (!templ)
        title = g_strdup_printf(_("Untitled %d"), untitled_no);
    else {
        do {
            p = strchr(templ, '%');
        } while (p && p[1] == '%' && (p += 2));

        if (!p)
            title = g_strdup_printf("%s %d", templ, untitled_no);
        else if (p[1] == 'd' && !strchr(p+2, '%'))
            title = g_strdup_printf(templ, untitled_no);
        else {
            g_warning("Wrong template `%s'", templ);
            title = g_strdup_printf(_("Untitled %d"), untitled_no);
        }
    }
    gwy_container_set_string_by_name(data, "/filename/untitled", title);
    gwy_data_window_update_title(data_window);

    return untitled_no;
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

void
gwy_app_undo_checkpoint(GwyContainer *data,
                        const gchar *what)
{
    GwyMenuSensitiveData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_UNDO
    };
    GwyDataWindow *data_window;
    GwyAppFuckingUndo *undo;
    GObject *object;
    GList *l;

    g_return_if_fail(GWY_IS_CONTAINER(data));
    if (strcmp(what, "/0/data") && strcmp(what, "/0/mask")) {
        g_warning("FIXME: Undo works only for standard datafields");
        return;
    }

    l = g_list_find_custom(current_data, data, 
                           (GCompareFunc)compare_data_window_data_cb);
    if (!l) {
        g_critical("Cannot find data window for container %p", data);
        return;
    }
    data_window = GWY_DATA_WINDOW(l->data);

    if (gwy_container_contains_by_name(data, what)) {
        object = gwy_container_get_object_by_name(data, what);
        g_return_if_fail(GWY_IS_DATA_FIELD(object));
        object = gwy_serializable_duplicate(object);
    }
    else
        object = NULL;

    undo_redo_clean(G_OBJECT(data_window), TRUE, TRUE);
    undo = g_new(GwyAppFuckingUndo, 1);
    undo->key = g_quark_from_string(what);
    undo->data = object;
    g_object_set_data(G_OBJECT(data_window), "undo", undo);
    gwy_app_update_toolbox_state(&sens_data);
}

void
gwy_app_undo_undo(void)
{
    GwyMenuSensitiveData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_REDO
    };
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyAppFuckingUndo *undo, *redo;
    GwyDataField *dfield, *df;
    GObject *window, *object;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));

    window = G_OBJECT(data_window);
    undo = (GwyAppFuckingUndo*)g_object_get_data(window, "undo");
    g_return_if_fail(undo);

    /* duplicate current state to redo */
    if (gwy_container_contains(data, undo->key)) {
        object = gwy_container_get_object(data, undo->key);
        g_return_if_fail(GWY_IS_DATA_FIELD(object));
        object = gwy_serializable_duplicate(object);
    }
    else
        object = NULL;

    redo = g_new(GwyAppFuckingUndo, 1);
    redo->key = undo->key;
    redo->data = object;

    /* transfer undo to current state */
    if (gwy_container_contains(data, undo->key)) {
        if (undo->data) {
            dfield = GWY_DATA_FIELD(gwy_container_get_object(data, undo->key));
            df = GWY_DATA_FIELD(undo->data);
            gwy_data_field_resample(dfield,
                                    gwy_data_field_get_xres(df),
                                    gwy_data_field_get_yres(df),
                                    GWY_INTERPOLATION_NONE);
            gwy_data_field_copy(df, dfield);
        }
        else
            gwy_container_remove(data, undo->key);
    }
    else {
        /* this refs the undo->data and undo_redo_clean() unrefs it again */
        if (undo->data)
            gwy_container_set_object(data, undo->key, undo->data);
        else
            g_warning("Trying to undo a NULL datafield to another NULL.");
    }

    undo_redo_clean(window, TRUE, TRUE);
    g_object_set_data(window, "redo", redo);
    gwy_data_view_update(GWY_DATA_VIEW(data_view));
    gwy_app_update_toolbox_state(&sens_data);
}

void
gwy_app_undo_redo(void)
{
    GwyMenuSensitiveData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_UNDO
    };
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyAppFuckingUndo *undo, *redo;
    GwyDataField *dfield, *df;
    GObject *window, *object;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));

    window = G_OBJECT(data_window);
    redo = (GwyAppFuckingUndo*)g_object_get_data(window, "redo");
    g_return_if_fail(redo);

    /* duplicate current state to undo */
    if (gwy_container_contains(data, redo->key)) {
        object = gwy_container_get_object(data, redo->key);
        g_return_if_fail(GWY_IS_DATA_FIELD(object));
        object = gwy_serializable_duplicate(object);
    }
    else
        object = NULL;

    undo = g_new(GwyAppFuckingUndo, 1);
    undo->key = redo->key;
    undo->data = object;

    /* transfer redo to current state */
    if (gwy_container_contains(data, redo->key)) {
        if (redo->data) {
            dfield = GWY_DATA_FIELD(gwy_container_get_object(data, redo->key));
            df = GWY_DATA_FIELD(redo->data);
            gwy_data_field_resample(dfield,
                                    gwy_data_field_get_xres(df),
                                    gwy_data_field_get_yres(df),
                                    GWY_INTERPOLATION_NONE);
            gwy_data_field_copy(df, dfield);
        }
        else
            gwy_container_remove(data, redo->key);
    }
    else {
        /* this refs the redo->data and redo_redo_clean() unrefs it again */
        if (redo->data)
            gwy_container_set_object(data, redo->key, redo->data);
        else
            g_warning("Trying to redo a NULL datafield to another NULL.");
    }

    undo_redo_clean(window, TRUE, TRUE);
    g_object_set_data(window, "undo", undo);
    gwy_data_view_update(GWY_DATA_VIEW(data_view));
    gwy_app_update_toolbox_state(&sens_data);
}

static void
undo_redo_clean(GObject *window,
                gboolean undo,
                gboolean redo)
{
    GwyAppFuckingUndo *gafu;

    gwy_debug("%s", __FUNCTION__);

    if (undo) {
        gafu = (GwyAppFuckingUndo*)g_object_get_data(window, "undo");
        if (gafu) {
            g_object_set_data(window, "undo", NULL);
            gwy_object_unref(gafu->data);
            g_free(gafu);
        }
    }

    if (redo) {
        gafu = (GwyAppFuckingUndo*)g_object_get_data(window, "redo");
        if (gafu) {
            g_object_set_data(window, "redo", NULL);
            gwy_object_unref(gafu->data);
            g_free(gafu);
        }
    }
}

static gint
compare_data_window_data_cb(GwyDataWindow *window,
                            GwyContainer *data)
{
    return gwy_data_window_get_data(window) != data;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
