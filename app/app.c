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

#include <math.h>
#include <stdarg.h>
#include <string.h>

#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include "gwyapp.h"
#include "gwyappinternal.h"

static GtkWidget *gwy_app_main_window = NULL;

static GList *current_data = NULL;
static GList *current_graphs = NULL;
static const gchar* current_tool = NULL;
static gint untitled_no = 0;

static GHookList window_list_hook_list;

static gboolean   gwy_app_confirm_quit             (void);
static void       gather_unsaved_cb                (GwyDataWindow *data_window,
                                                    GSList **unsaved);
static gboolean   gwy_app_confirm_quit_dialog      (GSList *unsaved);
static void       gwy_app_data_window_list_updated (void);
static GtkWidget* gwy_app_menu_data_popup_create   (GtkAccelGroup *accel_group);
static gboolean   gwy_app_data_popup_menu_popup    (GtkWidget *menu,
                                                    GdkEventButton *event);
static void       gwy_app_graph_list_toggle_cb     (GtkWidget *toggle,
                                                    GwyDataWindow *data_window);
static gboolean   gwy_app_graph_list_delete_cb     (GtkWidget *toggle);

gboolean
gwy_app_quit(void)
{
    GwyDataWindow *data_window;

    gwy_debug("");
    if (!gwy_app_confirm_quit())
        return TRUE;

    while ((data_window = gwy_app_data_window_get_current())) {
        gtk_widget_destroy(GTK_WIDGET(data_window));
    }

    gtk_main_quit();
    return TRUE;
}

gboolean
gwy_app_main_window_save_position(void)
{
    GwyContainer *settings;
    gint x, y;

    g_return_val_if_fail(GTK_IS_WINDOW(gwy_app_main_window), FALSE);

    settings = gwy_app_settings_get();
    /* FIXME: read the gtk_window_get_position() docs about how this is
     * a broken approach */
    gtk_window_get_position(GTK_WINDOW(gwy_app_main_window), &x, &y);
    gwy_container_set_int32_by_name(settings, "/app/toolbox/position/x", x);
    gwy_container_set_int32_by_name(settings, "/app/toolbox/position/y", y);

    /* to be usable as an event handler */
    return FALSE;
}

void
gwy_app_main_window_restore_position(void)
{
    GwyContainer *settings;
    gint x, y;

    g_return_if_fail(GTK_IS_WINDOW(gwy_app_main_window));

    settings = gwy_app_settings_get();
    if (gwy_container_gis_int32_by_name(settings,
                                        "/app/toolbox/position/x", &x)
        && gwy_container_gis_int32_by_name(settings,
                                           "/app/toolbox/position/y", &y)) {
        gtk_window_move(GTK_WINDOW(gwy_app_main_window), x, y);
    }
}

/**
 * gwy_app_data_window_get_current:
 *
 * Returns the currently active data window, may be %NULL if none is active.
 *
 * Returns: The active data window as a #GwyDataWindow.
 **/
GwyDataWindow*
gwy_app_data_window_get_current(void)
{
    return current_data ? (GwyDataWindow*)current_data->data : NULL;
}

/**
 * gwy_app_get_current_data:
 *
 * Returns the data of currently active data window.
 *
 * Returns: The current data as a #GwyContainer.
 *          May return %NULL if none is currently active.
 **/
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
 * gwy_app_data_window_set_current:
 * @window: A data window.
 *
 * Makes a data window active, including tool switch, etc.
 *
 * The window must be present in the list.
 **/
void
gwy_app_data_window_set_current(GwyDataWindow *window)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO
            | GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW,
        GWY_MENU_FLAG_DATA
    };
    static GwyDataWindow *already_current = NULL;
    GList *item;
    GwyContainer *data;

    gwy_debug("win = %p, tool = %p", window, current_tool);
    if (already_current == window) {
        gwy_debug("window already current");
        g_assert(current_data && current_data->data == (gpointer)window);
        return;
    }

    g_return_if_fail(GWY_IS_DATA_WINDOW(window));
    item = g_list_find(current_data, window);
    g_assert(item);
    current_data = g_list_remove_link(current_data, item);
    current_data = g_list_concat(item, current_data);

    if (current_tool)
        gwy_tool_func_use(current_tool, window, GWY_TOOL_SWITCH_WINDOW);

    if (gwy_app_data_window_has_undo(window))
        sens_data.set_to |= GWY_MENU_FLAG_UNDO;
    if (gwy_app_data_window_has_redo(window))
        sens_data.set_to |= GWY_MENU_FLAG_REDO;

    data = gwy_data_window_get_data(window);
    if (gwy_container_contains_by_name(data, "/0/mask"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_MASK;
    if (gwy_container_contains_by_name(data, "/0/show"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_SHOW;

    gwy_app_toolbox_update_state(&sens_data);
    already_current = window;
}

/**
 * gwy_app_data_window_remove:
 * @window: A data window.
 *
 * Removes the data window @window from the list of data windows.
 *
 * All associated structures are freed, active tool gets switch to %NULL
 * window.
 **/
void
gwy_app_data_window_remove(GwyDataWindow *window)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO
            | GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW,
        0
    };
    GList *item;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_WINDOW(window));

    item = g_list_find(current_data, window);
    if (!item) {
        g_critical("Trying to remove GwyDataWindow %p not present in the list",
                   window);
        return;
    }
    gwy_app_undo_clear(window);
    current_data = g_list_delete_link(current_data, item);
    if (current_data) {
        gwy_app_data_window_set_current(GWY_DATA_WINDOW(current_data->data));
        gwy_app_data_window_list_updated();
        return;
    }

    if (current_tool)
        gwy_tool_func_use(current_tool, NULL, GWY_TOOL_SWITCH_WINDOW);
    gwy_app_toolbox_update_state(&sens_data);

    gwy_app_data_window_list_updated();
}

/**
 * gwy_app_data_window_create:
 * @data: A data container.
 *
 * Creates a new data window showing @data and does some basic setup.
 *
 * Also calls gtk_window_present() on it.
 *
 * Returns: The newly created data window.
 **/
GtkWidget*
gwy_app_data_window_create(GwyContainer *data)
{
    static GtkWidget *popup_menu = NULL;

    GtkWidget *data_window, *data_view, *corner;
    GtkObject *layer;

    if (!popup_menu) {
        popup_menu = gwy_app_menu_data_popup_create(NULL);
        gtk_widget_show_all(popup_menu);
    }

    data_view = gwy_data_view_new(data);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(data_view),
                                 GWY_PIXMAP_LAYER(layer));
    data_window = gwy_data_window_new(GWY_DATA_VIEW(data_view));
    gtk_window_add_accel_group
        (GTK_WINDOW(data_window),
         g_object_get_data(G_OBJECT(gwy_app_main_window_get()), "accel_group"));

    corner = gtk_toggle_button_new();
    g_object_set(G_OBJECT(corner),
                 "can-default", FALSE,
                 "can-focus", FALSE,
                 "border-width", 1,
                 NULL);
    gtk_widget_set_name(corner, "cornerbutton");
    gtk_container_add(GTK_CONTAINER(corner),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH,
                                               GTK_ICON_SIZE_MENU));
    gtk_widget_show_all(corner);
    gwy_data_window_set_ul_corner_widget(GWY_DATA_WINDOW(data_window),
                                         corner);

    g_signal_connect(data_window, "focus-in-event",
                     G_CALLBACK(gwy_app_data_window_set_current), NULL);
    g_signal_connect(data_window, "destroy",
                     G_CALLBACK(gwy_app_data_window_remove), NULL);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(g_object_unref), data);
    g_signal_connect(corner, "toggled",
                     G_CALLBACK(gwy_app_graph_list_toggle_cb), data_window);

    current_data = g_list_append(current_data, data_window);
    g_signal_connect_swapped(data_view, "button_press_event",
                             G_CALLBACK(gwy_app_data_popup_menu_popup),
                             popup_menu);

    gwy_data_window_update_title(GWY_DATA_WINDOW(data_window));
    gwy_app_data_view_update(data_view);
    gtk_window_present(GTK_WINDOW(data_window));

    gwy_app_data_window_list_updated();

    return data_window;
}

static void
gwy_app_graph_list_toggle_cb(GtkWidget *toggle,
                             GwyDataWindow *data_window)
{
    GtkWidget *graph_view;

    graph_view = g_object_get_data(G_OBJECT(data_window),
                                   "gwy-app-graph-list-window");

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle))) {
        gtk_widget_hide(graph_view);
        return;
    }

    if (graph_view) {
        gtk_widget_show(graph_view);
        return;
    }

    graph_view = gwy_app_graph_list(data_window);
    g_signal_connect_swapped(graph_view, "delete_event",
                             G_CALLBACK(gwy_app_graph_list_delete_cb), toggle);
    gtk_window_set_transient_for(GTK_WINDOW(graph_view),
                                 GTK_WINDOW(data_window));
    gtk_window_present(GTK_WINDOW(graph_view));
}

static gboolean
gwy_app_graph_list_delete_cb(GtkWidget *toggle)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), FALSE);
    return TRUE;
}

static GtkWidget*
gwy_app_menu_data_popup_create(GtkAccelGroup *accel_group)
{
    static struct {
        const gchar *path;
        gpointer callback;
        gpointer cbdata;
    }
    const menu_items[] = {
        { "/_Remove Mask", gwy_app_mask_kill_cb, NULL },
        { "/Change Mask _Color",  gwy_app_change_mask_color_cb, NULL },
        { "/Fix _Zero", gwy_app_run_process_func_cb, "fixzero" },
        { "/_Level", gwy_app_run_process_func_cb, "level" },
        { "/Zoom _1:1", gwy_app_zoom_set_cb, GINT_TO_POINTER(10000) },
    };
    static const gchar *items_need_data_mask[] = {
        "/Remove Mask", "/Change Mask Color", NULL
    };
    GtkItemFactoryEntry entry = { NULL, NULL, NULL, 0, NULL, NULL };
    GtkItemFactory *item_factory;
    GtkWidget *menu;
    GwyMenuSensData sens_data = { GWY_MENU_FLAG_DATA_MASK, 0 };
    GList *menus;
    gsize i;

    /* XXX: it is probably wrong to use this accel group */
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<data-popup>",
                                        accel_group);
    for (i = 0; i < G_N_ELEMENTS(menu_items); i++) {
        entry.path = (gchar*)menu_items[i].path;
        entry.callback = (GtkItemFactoryCallback)menu_items[i].callback;
        gtk_item_factory_create_item(item_factory, &entry,
                                     menu_items[i].cbdata, 1);
    }
    menu = gtk_item_factory_get_widget(item_factory, "<data-popup>");
    gwy_app_menu_set_sensitive_array(item_factory, "data-popup",
                                     items_need_data_mask, sens_data.flags);

    /* XXX: assuming g_list_append() doesn't change nonempty list head */
    menus = (GList*)g_object_get_data(G_OBJECT(gwy_app_main_window_get()),
                                      "menus");
    g_assert(menus);
    g_list_append(menus, menu);

    return menu;
}

static gboolean
gwy_app_data_popup_menu_popup(GtkWidget *menu,
                              GdkEventButton *event)
{
    if (event->button != 3)
        return FALSE;

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);
    return TRUE;
}

static void
gwy_app_data_window_list_updated(void)
{
    gwy_debug("");

    if (!window_list_hook_list.is_setup)
        return;

    g_hook_list_invoke(&window_list_hook_list, FALSE);
}

/**
 * gwy_app_data_window_list_add_hook:
 * @func: Function to be called (with @data as its only argument).
 * @data: Data passed to @func.
 *
 * Adds a hook function called just after a data window is created or
 * destroyed.
 *
 * Returns: Hook id to be used in gwy_app_data_window_list_remove_hook().
 *
 * Since: 1.2.
 **/
gulong
gwy_app_data_window_list_add_hook(gpointer func,
                                  gpointer data)
{
    GHook *hook;

    gwy_debug("");
    if (!window_list_hook_list.is_setup) {
        gwy_debug("initializing window_list_hook_list");
        g_hook_list_init(&window_list_hook_list, sizeof(GHook));
    }

    hook = g_hook_alloc(&window_list_hook_list);
    hook->func = func;
    hook->data = data;
    g_hook_append(&window_list_hook_list, hook);
    gwy_debug("id = %lu", hook->hook_id);

    return hook->hook_id ;
}

/**
 * gwy_app_data_window_list_remove_hook:
 * @hook_id: Hook id, as returned by gwy_app_data_window_list_add_hook().
 *
 * Removes a data window list hook function added by
 * gwy_app_data_window_list_add_hook().
 *
 * Returns: Whether such a hook was found and removed.
 *
 * Since: 1.2.
 **/
gboolean
gwy_app_data_window_list_remove_hook(gulong hook_id)
{
    gwy_debug("");
    g_return_val_if_fail(window_list_hook_list.is_setup, FALSE);

    return g_hook_destroy(&window_list_hook_list, hook_id);
}

/**
 * gwy_app_graph_window_get_current:
 *
 * Returns the currently active graph window.
 *
 * Returns: The active graph window as a #GtkWidget.
 *          May return %NULL if none is currently active.
 **/
GtkWidget*
gwy_app_graph_window_get_current(void)
{
    return current_graphs ? current_graphs->data : NULL;
}

/**
 * gwy_app_graph_window_set_current:
 * @window: A graph window.
 * 
 * Makes a graph window active.
 *
 * Eventually adds @window it to the graph window list if it isn't present
 * there.
 **/
void
gwy_app_graph_window_set_current(GtkWidget *window)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_GRAPH, GWY_MENU_FLAG_GRAPH
    };
    GList *item;

    gwy_debug("%p", window);

    item = g_list_find(current_graphs, window);
    g_assert(item);
    current_graphs = g_list_remove_link(current_graphs, item);
    current_graphs = g_list_concat(item, current_graphs);

    gwy_app_toolbox_update_state(&sens_data);
}

/**
 * gwy_app_graph_window_remove:
 * @window: A data window.
 *
 * Removes the graph window @window from the list of graph windows.
 *
 * All associated structures are freed.
 **/
void
gwy_app_graph_window_remove(GtkWidget *window)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_GRAPH, 0
    };
    GList *item;

    /*g_return_if_fail(GWY_IS_GRAPH(graph));*/

    item = g_list_find(current_graphs, window);
    if (!item) {
        g_critical("Trying to remove GwyGraph %p not present in the list",
                   window);
        return;
    }
    current_graphs = g_list_delete_link(current_graphs, item);
    if (current_graphs)
        gwy_app_graph_window_set_current(current_graphs->data);
    else
        gwy_app_toolbox_update_state(&sens_data);
}

/**
 * gwy_app_graph_window_create:
 * @graph: A #GwyGraph;
 *
 * Creates a new graph window showing @data and does some basic setup.
 *
 * Also calls gtk_window_present() on it.
 *
 * Returns: The newly created graph window.
 **/
GtkWidget*
gwy_app_graph_window_create(GtkWidget *graph)
{

    GtkWidget *window;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);

    if (graph == NULL)
        graph = gwy_graph_new();

    /* TODO: this is broken because we do not actually know which data window
     * is the right one, but for GraphModel testing it doesn't matter much. */
    gwy_app_graph_list_add(gwy_app_data_window_get_current(), GWY_GRAPH(graph));

    g_signal_connect(window, "focus-in-event",
                     G_CALLBACK(gwy_app_graph_window_set_current), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(gwy_app_graph_window_remove), NULL);

    current_graphs = g_list_append(current_graphs, window);

    gtk_container_add (GTK_CONTAINER (window), graph);
    gtk_widget_show(graph);
    gtk_widget_show_all(window);
    gtk_window_present(GTK_WINDOW(window));

    return window;
}

/**
 * gwy_app_data_view_update:
 * @data_view: A #GwyDataView.
 *
 * Repaints a data view.
 *
 * Use this function instead of gwy_data_view_update() if you want to
 * automatically show (hide) the mask layer if present (missing).
 **/
void
gwy_app_data_view_update(GtkWidget *data_view)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA_SHOW | GWY_MENU_FLAG_DATA,
        GWY_MENU_FLAG_DATA
    };
    static const gchar *keys[] = {
        "/0/mask/red", "/0/mask/green", "/0/mask/blue", "/0/mask/alpha"
    };
    GwyDataViewLayer *layer;
    GwyContainer *data, *settings;
    gboolean has_mask;
    gboolean has_alpha;
    gdouble x;
    gsize i;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    has_mask = gwy_container_contains_by_name(data, "/0/mask");
    has_alpha = gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(data_view)) != NULL;

    if (has_mask && !has_alpha) {
        /* TODO: Container */
        settings = gwy_app_settings_get();
        for (i = 0; i < G_N_ELEMENTS(keys); i++) {
            if (gwy_container_contains_by_name(data, keys[i])
                || !gwy_container_contains_by_name(settings, keys[i] + 2))
                continue;
            x = gwy_container_get_double_by_name(settings, keys[i] + 2);
            gwy_container_set_double_by_name(data, keys[i], x);
        }

        layer = GWY_DATA_VIEW_LAYER(gwy_layer_mask_new());
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(data_view),
                                      GWY_PIXMAP_LAYER(layer));
    }
    else if (!has_mask && has_alpha) {
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(data_view), NULL);
    }
    else {
        gwy_data_view_update(GWY_DATA_VIEW(data_view));
    }

    if (gwy_container_contains_by_name(data, "/0/mask"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_MASK;
    if (gwy_container_contains_by_name(data, "/0/show"))
        sens_data.set_to |= GWY_MENU_FLAG_DATA_SHOW;
    gwy_app_toolbox_update_state(&sens_data);
}

/**
 * gwy_app_data_window_set_untitled:
 * @window: A data window.
 * @templ: A title template string.
 *
 * Clears any file name for @window and sets its "/filename/untitled"
 * data.
 *
 * The template tring @templ can be either %NULL, the window then gets a
 * title like "Untitled 37", or a string "Foo" not containing `%', the window
 * then gets a title like "Foo 42", or a string "Bar %%d" containing a single
 * '%%d', the window then gets a title like "Bar 666".
 *
 * Returns: The number that will appear in the title (probably useless).
 **/
gint
gwy_app_data_window_set_untitled(GwyDataWindow *window,
                                 const gchar *templ)
{
    GtkWidget *data_view;
    GwyContainer *data;
    gchar *title, *p;

    data_view = gwy_data_window_get_data_view(window);
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
    gwy_data_window_update_title(window);

    return untitled_no;
}

/*
 * Assures @window is present in the data window list, but doesn't make
 * it current.
 *
 * XXX: WTF?
 */
void
gwy_app_data_window_add(GwyDataWindow *window)
{
    gwy_debug("%p", window);

    g_return_if_fail(GWY_IS_DATA_WINDOW(window));

    if (g_list_find(current_data, window))
        return;

    current_data = g_list_append(current_data, window);
}

/**
 * gwy_app_data_window_foreach:
 * @func: A function to call on each data window.
 * @user_data: Data to pass to @func.
 *
 * Calls @func on each data window, in no particular order.
 *
 * The function should not create or remove data windows.
 **/
void
gwy_app_data_window_foreach(GFunc func,
                            gpointer user_data)
{
    GList *l;

    for (l = current_data; l; l = g_list_next(l))
        func(l->data, user_data);
}

void
gwy_app_tool_use_cb(const gchar *toolname,
                     GtkWidget *button)
{
    static GtkWidget *old_button = NULL;
    GwyDataWindow *data_window;
    gboolean ok = TRUE;

    gwy_debug("%s", toolname ? toolname : "NONE");
    /* don't catch deactivations */
    if (button && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
        gwy_debug("deactivation");
        old_button = button;
        return;
    }

    if (current_tool && (!toolname || strcmp(current_tool, toolname)))
        gwy_tool_func_use(current_tool, NULL, GWY_TOOL_SWITCH_TOOL);
    if (toolname) {
        data_window = gwy_app_data_window_get_current();
        if (data_window)
            ok = gwy_tool_func_use(toolname, data_window, GWY_TOOL_SWITCH_TOOL);
    }
    if (ok) {
        current_tool = toolname;
        old_button = button;
        return;
    }
    /* FIXME: this is really ugly */
    g_signal_emit_by_name(old_button, "clicked");
}

void
gwy_app_zoom_set_cb(gpointer data)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    if (data_window)
        gwy_data_window_set_zoom(data_window, GPOINTER_TO_INT(data));
}


/**
 * gwy_app_clean_up_data:
 * @data: A data container.
 *
 * Cleans-up a data container.
 *
 * XXX: Generally, it should remove some things that you might not want to
 * copy to the new data window.  Currently it removes selection.
 **/
void
gwy_app_clean_up_data(GwyContainer *data)
{
    /* TODO: Container */
    /* FIXME: This is dirty. Clean-up various individual stuff. */
    gwy_container_remove_by_prefix(data, "/0/select");
}

void
gwy_app_change_mask_color_cb(G_GNUC_UNUSED gpointer unused,
                             gboolean defaultc)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyContainer *data, *settings;
    GwyRGBA rgba;

    gwy_debug("defaultc = %d", defaultc);

    settings = gwy_app_settings_get();
    if (defaultc) {
        gwy_color_selector_for_mask(_("Change Default Mask Color"),
                                    NULL, NULL, settings, "/mask");
        return;
    }

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    g_assert(data);

    /* copy defaults to data container if necessary */
    if (!gwy_rgba_get_from_container(&rgba, data, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, settings, "/mask");
        gwy_rgba_store_to_container(&rgba, data, "/0/mask");
    }
    gwy_color_selector_for_mask(NULL, GWY_DATA_VIEW(data_view), NULL, data,
                                "/0/mask");
}

/**
 * gwy_app_main_window_get:
 *
 * Returns Gwyddion main application window (toolbox).
 *
 * Returns: The Gwyddion toolbox.
 **/
GtkWidget*
gwy_app_main_window_get(void)
{
    if (!gwy_app_main_window)
        g_critical("Trying to access app main window before its creation");
    return gwy_app_main_window;
}

/**
 * gwy_app_main_window_set:
 * @window: A window.
 *
 * Sets Gwyddion main application window (toolbox) for
 * gwy_app_main_window_get().
 *
 * This function can be called only once and should be called at Gwyddion
 * startup so, ignore it.
 **/
void
gwy_app_main_window_set(GtkWidget *window)
{
    if (gwy_app_main_window && window != gwy_app_main_window)
        g_critical("Trying to change app main window");
    if (!GTK_IS_WINDOW(window))
        g_critical("Setting app main window to a non-GtkWindow");
    gwy_app_main_window = window;
}

static gboolean
gwy_app_confirm_quit(void)
{
    GSList *unsaved = NULL;
    gboolean ok;

    gwy_app_data_window_foreach((GFunc)gather_unsaved_cb, &unsaved);
    if (!unsaved)
        return TRUE;
    ok = gwy_app_confirm_quit_dialog(unsaved);
    g_slist_free(unsaved);

    return ok;
}

static void
gather_unsaved_cb(GwyDataWindow *data_window,
                  GSList **unsaved)
{
    GwyContainer *data = gwy_data_window_get_data(data_window);

    if (g_object_get_data(G_OBJECT(data), "gwy-app-modified"))
        *unsaved = g_slist_prepend(*unsaved, data_window);
}

static gboolean
gwy_app_confirm_quit_dialog(GSList *unsaved)
{
    GtkWidget *dialog;
    gchar *text;
    gint response;

    text = NULL;
    while (unsaved) {
        GwyDataWindow *data_window = GWY_DATA_WINDOW(unsaved->data);
        gchar *filename = gwy_data_window_get_base_name(data_window);

        text = g_strconcat(filename, "\n", text, NULL);
        unsaved = g_slist_next(unsaved);
        g_free(filename);
    }
    dialog = gtk_message_dialog_new(GTK_WINDOW(gwy_app_main_window_get()),
                                    GTK_DIALOG_MODAL,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_YES_NO,
                                    _("Some data are unsaved:\n"
                                      "%s\n"
                                      "Really quit?"),
                                    text);
    g_free(text);

    gtk_window_present(GTK_WINDOW(dialog));
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response == GTK_RESPONSE_YES;
}

/* FIXME: this functionality is provided by modules now -- remove? */
void
gwy_app_mask_kill_cb(void)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        gwy_container_remove_by_name(data, "/0/mask");
        gwy_app_data_view_update(data_view);
    }
}

void
gwy_app_show_kill_cb(void)
{
    GwyDataWindow *data_window;
    GtkWidget *data_view;
    GwyContainer *data;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    if (gwy_container_contains_by_name(data, "/0/show")) {
        gwy_app_undo_checkpoint(data, "/0/show", NULL);
        gwy_container_remove_by_name(data, "/0/show");
        gwy_data_view_update(GWY_DATA_VIEW(data_view));
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
