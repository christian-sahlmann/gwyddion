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
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libgwydgets/gwygraphwindow.h>
#include <app/gwyapp.h>
#include "gwyappinternal.h"

enum {
    ITEM_PIXELSQUARE,
    ITEM_REALSQUARE
};

static GtkWidget *gwy_app_main_window = NULL;

/* list of existing windows of different kinds. FIXME: maybe some common
 * management functions could be factored out? Unfortunately the data window
 * managements logic differs slightly from the other two. */
static GList *current_data = NULL;
static GList *current_graph = NULL;
static GList *current_3d = NULL;
static GtkWidget *current_any = NULL;

static const gchar* current_tool = NULL;
static gint untitled_no = 0;
static GQuark corner_item_quark = 0;

static GHookList window_list_hook_list;

static gboolean   gwy_app_confirm_quit             (void);
static void       gather_unsaved_cb                (GwyDataWindow *data_window,
                                                    GSList **unsaved);
static gboolean   gwy_app_confirm_quit_dialog      (GSList *unsaved);
static GtkWidget* gwy_app_menu_data_popup_create   (GtkAccelGroup *accel_group);
static GtkWidget* gwy_app_menu_data_corner_create  (GtkAccelGroup *accel_group);
static void       gwy_app_data_window_change_square(GtkWidget *item,
                                                    gpointer user_data);
static gboolean   gwy_app_data_corner_menu_popup_mouse(GtkWidget *menu,
                                                       GdkEventButton *event,
                                                       GtkWidget *view);
static gboolean   gwy_app_data_popup_menu_popup_mouse(GtkWidget *menu,
                                                      GdkEventButton *event,
                                                      GtkWidget *view);
static void       gwy_app_data_popup_menu_popup_key(GtkWidget *menu,
                                                    GtkWidget *data_window);
static gboolean   gwy_app_set_current_window       (GtkWidget *window);
static void       gwy_app_3d_window_orphaned       (GtkWidget *view,
                                                    GtkWidget *gwy3dwindow);
static void       gwy_app_3d_window_destroyed      (GtkWidget *gwy3dwindow,
                                                    GtkWidget *view);
static void       gwy_app_3d_window_title_changed  (GtkWidget *data_window,
                                                    GtkWidget *gwy3dwindow);
static void       gwy_app_3d_window_export         (Gwy3DWindow *window);

/*****************************************************************************
 *                                                                           *
 *     Main, toolbox                                                         *
 *                                                                           *
 *****************************************************************************/

gboolean
gwy_app_quit(void)
{
    gwy_debug("");
    if (!gwy_app_confirm_quit())
        return TRUE;

    gwy_app_data_browser_shut_down();

    gtk_main_quit();
    return TRUE;
}

gboolean
gwy_app_main_window_save_position(void)
{
    gwy_app_save_window_position(GTK_WINDOW(gwy_app_main_window),
                                 "/app/toolbox", TRUE, FALSE);
    /* to be usable as an event handler */
    return FALSE;
}

void
gwy_app_main_window_restore_position(void)
{
    gwy_app_restore_window_position(GTK_WINDOW(gwy_app_main_window),
                                    "/app/toolbox", FALSE);
}

/**
 * gwy_app_add_main_accel_group:
 * @window: A window.
 *
 * Adds main (global) application accelerator group to a window.
 **/
void
gwy_app_add_main_accel_group(GtkWindow *window)
{
    GtkWidget *main_window;
    GtkAccelGroup *accel_group;

    g_return_if_fail(GTK_IS_WINDOW(window));
    main_window = gwy_app_main_window_get();
    g_return_if_fail(GTK_IS_WINDOW(main_window));

    accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                    "accel_group"));
    g_return_if_fail(GTK_IS_ACCEL_GROUP(accel_group));
    gtk_window_add_accel_group(window, accel_group);
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

    if (gwy_undo_container_get_modified(data))
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

/*****************************************************************************
 *                                                                           *
 *     Data window list management                                           *
 *                                                                           *
 *****************************************************************************/

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
 * Makes a data window current, including tool switch, etc.
 *
 * The window must be present in the list.
 *
 * Returns: Always FALSE, no matter what (to be usable as an event handler).
 **/
gboolean
gwy_app_data_window_set_current(GwyDataWindow *window)
{
    GwyMenuSensFlags mask = (GWY_MENU_FLAG_DATA
                             | GWY_MENU_FLAG_UNDO
                             | GWY_MENU_FLAG_REDO
                             | GWY_MENU_FLAG_DATA_MASK
                             | GWY_MENU_FLAG_DATA_SHOW);
    GwyMenuSensFlags state = GWY_MENU_FLAG_DATA;
    static GwyDataWindow *already_current = NULL;
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GList *item;
    GwyContainer *data;

    gwy_debug("win = %p, tool = %p", window, current_tool);
    if (gwy_app_set_current_window(GTK_WIDGET(window)))
        return FALSE;

    data_view = gwy_data_window_get_data_view(window);
    gwy_app_data_browser_select_data_view(data_view);

    if (already_current == window) {
        /* FIXME: Data browser seems to make this sometimes fail, but rarely */
        g_assert(current_data && current_data->data == (gpointer)window);
        return FALSE;
    }

    g_return_val_if_fail(GWY_IS_DATA_WINDOW(window), FALSE);
    item = g_list_find(current_data, window);
    if (item) {
        current_data = g_list_remove_link(current_data, item);
        current_data = g_list_concat(item, current_data);
    }
    else
        current_data = g_list_prepend(current_data, window);

    if (current_tool)
        gwy_tool_func_use(current_tool, window, GWY_TOOL_SWITCH_WINDOW);

    data = gwy_data_view_get_data(data_view);
    if (gwy_undo_container_has_undo(data))
        state |= GWY_MENU_FLAG_UNDO;
    if (gwy_undo_container_has_redo(data))
        state |= GWY_MENU_FLAG_REDO;

    layer = gwy_data_view_get_base_layer(data_view);
    if (gwy_layer_basic_get_has_presentation(GWY_LAYER_BASIC(layer)))
        state |= GWY_MENU_FLAG_DATA_SHOW;
    if (gwy_data_view_get_alpha_layer(data_view))
        state |= GWY_MENU_FLAG_DATA_MASK;

    gwy_app_sensitivity_set_state(mask, state);
    already_current = window;

    return FALSE;
}

/**
 * gwy_app_data_window_remove:
 * @window: A data window.
 *
 * Removes the data window @window from the list of data windows.
 *
 * All associated structures are freed, active tool gets switched to %NULL
 * window.  But the widget itself is NOT destroyed by this function.
 * It just makes the application `forget' about the window.
 **/
void
gwy_app_data_window_remove(GwyDataWindow *window)
{
    GwyMenuSensFlags mask = (GWY_MENU_FLAG_DATA
                             | GWY_MENU_FLAG_UNDO
                             | GWY_MENU_FLAG_REDO
                             | GWY_MENU_FLAG_DATA_MASK
                             | GWY_MENU_FLAG_DATA_SHOW);
    GList *item;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_WINDOW(window));

    item = g_list_find(current_data, window);
    if (!item) {
        g_critical("Trying to remove GwyDataWindow %p not present in the list",
                   window);
        return;
    }
    current_data = g_list_delete_link(current_data, item);
    gwy_debug("Removed window, %p is new head\n",
              current_data ? current_data->data : NULL);
    if (current_data) {
        gwy_app_data_window_set_current(GWY_DATA_WINDOW(current_data->data));
        return;
    }

    if (current_tool)
        gwy_tool_func_use(current_tool, NULL, GWY_TOOL_SWITCH_WINDOW);
    gwy_app_sensitivity_set_state(mask, 0);
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
    /*
    GtkWidget *window, *view;
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    */

    g_warning("gwy_app_data_window_create() is deprecated and broken");
    gwy_app_data_browser_add(data);
    return NULL;

    /*
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    if (!popup_menu) {
        popup_menu = gwy_app_menu_data_popup_create(NULL);
        gtk_widget_show_all(popup_menu);
    }

    view = gwy_data_view_new(data);
    data_view = GWY_DATA_VIEW(view);
    gwy_app_data_view_setup_layers1(data_view);
    window = gwy_data_window_new(data_view);
    data_window = GWY_DATA_WINDOW(window);

    g_signal_connect(data_window, "focus-in-event",
                     G_CALLBACK(gwy_app_data_window_set_current), NULL);
    g_signal_connect(data_window, "destroy",
                     G_CALLBACK(gwy_app_data_window_remove), NULL);

    gwy_app_data_window_add(data_window);
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_DATA, GWY_MENU_FLAG_DATA);
    gwy_app_data_view_setup_layers2(data_view);
    gtk_window_present(GTK_WINDOW(window));

    return window;
    */
}

void
gwy_app_data_window_setup(GwyDataWindow *data_window)
{
    static GtkWidget *popup_menu = NULL;
    static GtkWidget *corner_menu = NULL;

    GwyDataView *data_view;
    GtkWidget *corner, *ebox;

    if (!popup_menu) {
        GtkWidget *main_window;
        GtkAccelGroup *accel_group;

        main_window = gwy_app_main_window_get();
        g_return_if_fail(GTK_IS_WINDOW(main_window));
        accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                        "accel_group"));
        popup_menu = gwy_app_menu_data_popup_create(accel_group);
        gtk_widget_show_all(popup_menu);
    }

    if (!corner_menu) {
        GtkWidget *main_window;
        GtkAccelGroup *accel_group;

        main_window = gwy_app_main_window_get();
        g_return_if_fail(GTK_IS_WINDOW(main_window));
        accel_group = GTK_ACCEL_GROUP(g_object_get_data(G_OBJECT(main_window),
                                                        "accel_group"));
        corner_menu = gwy_app_menu_data_corner_create(accel_group);
        gtk_widget_show_all(corner_menu);
    }

    corner = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_ETCHED_OUT);
    gtk_misc_set_alignment(GTK_MISC(corner), 0.5, 0.5);
    gtk_misc_set_padding(GTK_MISC(corner), 2, 0);

    ebox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(ebox), corner);
    gtk_widget_add_events(ebox, GDK_BUTTON_PRESS_MASK);
    gtk_widget_show_all(ebox);

    gwy_data_window_set_ul_corner_widget(data_window, ebox);

    data_view = gwy_data_window_get_data_view(data_window);
    g_signal_connect_swapped(data_view, "button-press-event",
                             G_CALLBACK(gwy_app_data_popup_menu_popup_mouse),
                             popup_menu);
    g_signal_connect_swapped(data_window, "popup-menu",
                             G_CALLBACK(gwy_app_data_popup_menu_popup_key),
                             popup_menu);
    g_signal_connect_swapped(ebox, "button-press-event",
                             G_CALLBACK(gwy_app_data_corner_menu_popup_mouse),
                             corner_menu);
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
 **/
gboolean
gwy_app_data_window_list_remove_hook(gulong hook_id)
{
    gwy_debug("");
    g_return_val_if_fail(window_list_hook_list.is_setup, FALSE);

    return g_hook_destroy(&window_list_hook_list, hook_id);
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

/**
 * gwy_app_data_window_get_for_data:
 * @data: A data container.
 *
 * Finds a data window displaying given data.
 *
 * Returns: Data window displaying given data, or %NULL if there is no such
 *          data window.
 **/
GwyDataWindow*
gwy_app_data_window_get_for_data(GwyContainer *data)
{
    GList *l;

    for (l = current_data; l; l = g_list_next(l)) {
        if (gwy_data_window_get_data((GwyDataWindow*)l->data) == data)
            return (GwyDataWindow*)l->data;
    }
    return NULL;
}

/*****************************************************************************
 *                                                                           *
 *     Graph window list management                                          *
 *                                                                           *
 *****************************************************************************/

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
    return current_graph ? current_graph->data : NULL;
}

/**
 * gwy_app_graph_window_set_current:
 * @window: A graph window.
 *
 * Makes a graph window current.
 *
 * Eventually adds @window it to the graph window list if it isn't present
 * there.
 *
 * Returns: Always FALSE, no matter what (to be usable as an event handler).
 **/
gboolean
gwy_app_graph_window_set_current(GtkWidget *window)
{
    GtkWidget *graph;
    GList *item;

    gwy_debug("%p", window);
    if (gwy_app_set_current_window(window))
        return FALSE;

    item = g_list_find(current_graph, window);
    if (item) {
        current_graph = g_list_remove_link(current_graph, item);
        current_graph = g_list_concat(item, current_graph);
    }
    else
        current_graph = g_list_prepend(current_graph, window);

    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_GRAPH, GWY_MENU_FLAG_GRAPH);
    graph = gwy_graph_window_get_graph(GWY_GRAPH_WINDOW(window));
    gwy_app_data_browser_select_graph(GWY_GRAPH(graph));

    return FALSE;
}

/**
 * gwy_app_graph_window_remove:
 * @window: A graph window.
 *
 * Removes the graph window @window from the list of graph windows.
 *
 * All associated structures are freed, but the widget itself is NOT destroyed
 * by this function.  It just makes the application `forget' about the window.
 **/
void
gwy_app_graph_window_remove(GtkWidget *window)
{
    GList *item;

    item = g_list_find(current_graph, window);
    if (!item) {
        g_critical("Trying to remove GwyGraph %p not present in the list",
                   window);
        return;
    }
    current_graph = g_list_delete_link(current_graph, item);
    if (current_graph)
        gwy_app_graph_window_set_current(current_graph->data);
    else
        gwy_app_sensitivity_set_state(GWY_MENU_FLAG_GRAPH, 0);
}

/**
 * gwy_app_graph_window_foreach:
 * @func: A function to call on each graph window.
 * @user_data: Data to pass to @func.
 *
 * Calls @func on each graph window, in no particular order.
 *
 * The function should not create or remove graph windows.
 **/
void
gwy_app_graph_window_foreach(GFunc func,
                             gpointer user_data)
{
    GList *l;

    for (l = current_graph; l; l = g_list_next(l))
        func(l->data, user_data);
}

/**
 * gwy_app_graph_window_create:
 * @graph: A graph widget.
 * @data: A data container to put the graph model to.
 *
 * Creates a new graph window showing a graph and does some basic setup.
 *
 * It calls gwy_app_graph_list_add() and does not assume a reference on the
 * graph model, so you usually wish to unreference it after this call.
 *
 * Also calls gtk_window_present() on the newly created window, associates
 * it with a data window, and sets its title.
 *
 * Returns: The newly created graph window.
 **/
GtkWidget*
gwy_app_graph_window_create(GwyGraph *graph,
                            GwyContainer *data)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    g_warning("gwy_app_graph_window_create() is deprecated and broken");
    gwy_app_data_browser_add_graph_model(gwy_graph_get_model(graph),
                                         data, TRUE);
    gtk_widget_destroy(GTK_WIDGET(graph));
    return NULL;
}

/*****************************************************************************
 *                                                                           *
 *     3D window list management                                             *
 *                                                                           *
 *****************************************************************************/

void
gwy_app_3d_view_cb(void)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(data_window);
    gwy_app_3d_window_create(data_window);
}

/**
 * gwy_app_3d_window_create:
 * @data_window: A data window to create associated 3D view for.
 *
 * Creates a new 3D view window showing the same data as @data_window.
 *
 * Also does some housekeeping and calls gtk_window_present() on it.
 *
 * Returns: The newly created 3D view window.
 **/
GtkWidget*
gwy_app_3d_window_create(GwyDataWindow *data_window)
{
    GtkWidget *gwy3dview, *gwy3dwindow, *button;
    GwyDataView *view;
    GwyContainer *data;
    gchar *name, *title;

    g_return_val_if_fail(GWY_IS_DATA_WINDOW(data_window), NULL);
    view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(view);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    gwy3dview = gwy_3d_view_new(data);
    gwy_3d_view_set_data_key(GWY_3D_VIEW(gwy3dview), "/0/data");
    gwy_3d_view_set_gradient_key(GWY_3D_VIEW(gwy3dview), "/0/3d/palette");
    gwy_3d_view_set_material_key(GWY_3D_VIEW(gwy3dview), "/0/3d/material");
    gwy3dwindow = gwy_3d_window_new(GWY_3D_VIEW(gwy3dview));
    gwy_app_add_main_accel_group(GTK_WINDOW(gwy3dwindow));

    name = gwy_data_window_get_base_name(data_window);
    title = g_strconcat("3D ", name, NULL);
    gtk_window_set_title(GTK_WINDOW(gwy3dwindow), title);
    g_free(title);
    g_free(name);

    button = gwy_stock_like_button_new(_("Export"), GTK_STOCK_SAVE);
    gwy_3d_window_add_action_widget(GWY_3D_WINDOW(gwy3dwindow), button);
    gwy_3d_window_add_small_toolbar_button(GWY_3D_WINDOW(gwy3dwindow),
                                           GTK_STOCK_SAVE,
                                           _("Export 3D view to PNG image"),
                                           G_CALLBACK(gwy_app_3d_window_export),
                                           gwy3dwindow);

    current_3d = g_list_append(current_3d, gwy3dwindow);
    /*
     * There are some signal cross-connections.
     *
     * object    signal            handler           data      destroy in
     *
     * window    "title-changed"   title_changed()   3dwindow  destroyed()
     * window    "destroyed"       orphaned()        3dwindow  destroyed()
     * view      "updated"         3d_view_update()  3dview    destroyed()
     * 3dwindow  "destroy"         destroyed()       window    orphaned()
     * 3dwindow  "focun-in-event"  set_current()     NULL      --
     * button    "clicked"         export()          3dwindow  --
     */
    g_signal_connect(data_window, "title-changed",
                     G_CALLBACK(gwy_app_3d_window_title_changed), gwy3dwindow);
    g_signal_connect(data_window, "destroy",
                     G_CALLBACK(gwy_app_3d_window_orphaned), gwy3dwindow);
    g_signal_connect(gwy3dwindow, "focus-in-event",
                     G_CALLBACK(gwy_app_3d_window_set_current), NULL);
    g_signal_connect(gwy3dwindow, "destroy",
                     G_CALLBACK(gwy_app_3d_window_destroyed), data_window);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_3d_window_export), gwy3dwindow);

    gtk_widget_show_all(gwy3dwindow);
    gtk_window_present(GTK_WINDOW(gwy3dwindow));

    return gwy3dwindow;
}

/**
 * gwy_app_3d_window_get_current:
 *
 * Returns the currently active 3D view window.
 *
 * Returns: The active 3D view window as a #GtkWidget.
 *          May return %NULL if none is currently active.
 **/
GtkWidget*
gwy_app_3d_window_get_current(void)
{
    return current_3d ? current_3d->data : NULL;
}

/**
 * gwy_app_3d_window_set_current:
 * @window: A 3D view window.
 *
 * Makes a 3D view window current.
 *
 * Eventually adds @window it to the 3D view window list if it isn't present
 * there.
 *
 * Returns: Always FALSE, no matter what (to be usable as an event handler).
 **/
gboolean
gwy_app_3d_window_set_current(GtkWidget *window)
{
    /*
    static const GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_3D, GWY_MENU_FLAG_3D
    };
    */
    GList *item;

    gwy_debug("%p", window);
    if (gwy_app_set_current_window(window))
        return FALSE;

    item = g_list_find(current_3d, window);
    g_return_val_if_fail(item, FALSE);
    current_3d = g_list_remove_link(current_3d, item);
    current_3d = g_list_concat(item, current_3d);

    return FALSE;
}

/**
 * gwy_app_3d_window_remove:
 * @window: A 3D view window.
 *
 * Removes the 3D view window @window from the list of 3D view windows.
 *
 * All associated structures are freed, but the widget itself is NOT destroyed
 * by this function.  It just makes the application `forget' about the window.
 **/
void
gwy_app_3d_window_remove(GtkWidget *window)
{
    /*
    static const GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_3D, 0
    };
    */
    GList *item;

    g_return_if_fail(GWY_IS_3D_WINDOW(window));

    item = g_list_find(current_3d, window);
    if (!item) {
        g_critical("Trying to remove 3D window %p not present in the list",
                   window);
        return;
    }
    g_free(g_object_get_data(G_OBJECT(item->data), "gwy-app-export-filename"));
    current_3d = g_list_delete_link(current_3d, item);
    if (current_3d)
        gwy_app_3d_window_set_current(current_3d->data);
    /* FIXME: hangs.
    else
        gwy_app_toolbox_update_state(&sens_data);
        */
}

static void
gwy_app_3d_window_orphaned(GtkWidget *data_window,
                           GtkWidget *gwy3dwindow)
{
    g_signal_handlers_disconnect_matched(gwy3dwindow,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gwy_app_3d_window_destroyed,
                                         data_window);
}

static void
gwy_app_3d_window_title_changed(GtkWidget *data_window,
                                GtkWidget *gwy3dwindow)
{
    gchar *name, *title;

    name = gwy_data_window_get_base_name(GWY_DATA_WINDOW(data_window));
    title = g_strconcat("3D ", name, NULL);
    gtk_window_set_title(GTK_WINDOW(gwy3dwindow), title);
    g_free(title);
    g_free(name);
}

static void
gwy_app_3d_window_destroyed(GtkWidget *gwy3dwindow,
                            GtkWidget *data_window)
{
    g_signal_handlers_disconnect_matched(data_window,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gwy_app_3d_window_title_changed,
                                         gwy3dwindow);
    g_signal_handlers_disconnect_matched(data_window,
                                         G_SIGNAL_MATCH_FUNC
                                         | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gwy_app_3d_window_orphaned,
                                         gwy3dwindow);
    gwy_app_3d_window_remove(gwy3dwindow);
}


/*****************************************************************************
 *                                                                           *
 *     Any window list management                                            *
 *                                                                           *
 *****************************************************************************/

static gboolean
gwy_app_set_current_window(GtkWidget *window)
{
    g_return_val_if_fail(GTK_IS_WINDOW(window), FALSE);

    if (current_any == window)
        return TRUE;

    current_any = window;
    return FALSE;
}

/**
 * gwy_app_get_current_window:
 * @type: Type of window to return.
 *
 * Returns the currently active window of a given type.
 *
 * Returns: The window, or %NULL if there is no such window.
 **/
GtkWidget*
gwy_app_get_current_window(GwyAppWindowType type)
{
    switch (type) {
        case GWY_APP_WINDOW_TYPE_ANY:
        return current_any;
        break;

        case GWY_APP_WINDOW_TYPE_DATA:
        return current_data ? (GtkWidget*)current_data->data : NULL;
        break;

        case GWY_APP_WINDOW_TYPE_GRAPH:
        return current_graph ? (GtkWidget*)current_graph->data : NULL;
        break;

        case GWY_APP_WINDOW_TYPE_3D:
        return current_3d ? (GtkWidget*)current_3d->data : NULL;
        break;

        default:
        g_assert_not_reached();
        return NULL;
        break;
    }
}

/*****************************************************************************
 *                                                                           *
 *     Miscellaneous                                                         *
 *                                                                           *
 *****************************************************************************/

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
    GwyDataView *data_view;
    GwyContainer *data;
    gchar *title, *p;

    data_view = gwy_data_window_get_data_view(window);
    data = gwy_data_view_get_data(data_view);
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

    return untitled_no;
}

static GtkWidget*
gwy_app_menu_data_popup_create(GtkAccelGroup *accel_group)
{
    static struct {
        const gchar *label;
        gpointer callback;
        gpointer cbdata;
        guint key;
        GdkModifierType mods;
    }
    const menu_items[] = {
        {
            NULL, gwy_app_run_process_func,
            "mask_remove", GDK_K, GDK_CONTROL_MASK
        },
        {
            N_("Mask _Color..."),  gwy_app_change_mask_color_cb,
            NULL, 0, 0
        },
        {
            NULL, gwy_app_run_process_func,
            "fix_zero", 0, 0
        },
        {
            NULL, gwy_app_run_process_func,
            "presentation_remove", GDK_K, GDK_CONTROL_MASK | GDK_SHIFT_MASK
        },
        {
            NULL, gwy_app_run_process_func,
            "level", GDK_L, GDK_CONTROL_MASK
        },
        {
            N_("Zoom _1:1"), gwy_app_zoom_set_cb,
            GINT_TO_POINTER(10000), 0, 0
        },
    };
    GwySensitivityGroup *sensgroup;
    GtkWidget *menu, *item;
    const gchar *name;
    guint i, mask;

    menu = gtk_menu_new();
    if (accel_group)
        gtk_menu_set_accel_group(GTK_MENU(menu), accel_group);
    sensgroup = gwy_app_sensitivity_get_group();
    for (i = 0; i < G_N_ELEMENTS(menu_items); i++) {
        if (menu_items[i].callback == gwy_app_run_process_func
            && !gwy_process_func_get_run_types((gchar*)menu_items[i].cbdata)) {
            g_warning("Processing function <%s> for "
                      "data view context menu is not available.",
                      (gchar*)menu_items[i].cbdata);
            continue;
        }
        if (menu_items[i].callback == gwy_app_run_process_func) {
            name = _(gwy_process_func_get_menu_path(menu_items[i].cbdata));
            name = strrchr(name, '/');
            if (!name) {
                g_warning("Invalid translated menu path for <%s>",
                          (const gchar*)menu_items[i].cbdata);
                continue;
            }
            item = gtk_menu_item_new_with_mnemonic(name + 1);
            mask = gwy_process_func_get_sensitivity_mask(menu_items[i].cbdata);
            gwy_sensitivity_group_add_widget(sensgroup, item, mask);
        }
        else
            item = gtk_menu_item_new_with_mnemonic(_(menu_items[i].label));

        if (menu_items[i].key)
            gtk_widget_add_accelerator(item, "activate", accel_group,
                                       menu_items[i].key, menu_items[i].mods,
                                       GTK_ACCEL_VISIBLE | GTK_ACCEL_LOCKED);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        g_signal_connect_swapped(item, "activate",
                                 G_CALLBACK(menu_items[i].callback),
                                 menu_items[i].cbdata);
    }

    return menu;
}

static gboolean
gwy_app_data_popup_menu_popup_mouse(GtkWidget *menu,
                                    GdkEventButton *event,
                                    GtkWidget *view)
{
    GtkWidget *window;

    if (event->button != 3)
        return FALSE;

    window = gtk_widget_get_toplevel(view);
    g_return_val_if_fail(window, FALSE);
    gwy_app_data_window_set_current(GWY_DATA_WINDOW(window));

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);

    return TRUE;
}

static void
gwy_app_data_popup_menu_position(G_GNUC_UNUSED GtkMenu *menu,
                                 gint *x,
                                 gint *y,
                                 gboolean *push_in,
                                 GtkWidget *window)
{
    GwyDataView *data_view;

    data_view = gwy_data_window_get_data_view(GWY_DATA_WINDOW(window));
    gdk_window_get_origin(GTK_WIDGET(data_view)->window, x, y);
    *push_in = TRUE;
}

static void
gwy_app_data_popup_menu_popup_key(GtkWidget *menu,
                                  GtkWidget *data_window)
{
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   (GtkMenuPositionFunc)gwy_app_data_popup_menu_position,
                   data_window,
                   0, gtk_get_current_event_time());
}

/**
 * gwy_app_data_corner_menu_update:
 * @menu: Data window corner menu.
 * @window: The corresponding data window.
 *
 * Updates corner menu to reflect data window's state before we show it.
 **/
static void
gwy_app_data_corner_menu_update(GtkWidget *menu,
                                GtkWidget *window)
{
    gboolean realsquare = FALSE;
    GwyDataView *data_view;
    GwyContainer *data;
    const gchar *key;
    gchar *s;
    GtkWidget *item;
    GList *l;
    gulong id;
    guint i;

    /* Square mode */
    data_view = gwy_data_window_get_data_view(GWY_DATA_WINDOW(window));
    data = gwy_data_view_get_data(data_view);
    key = gwy_data_view_get_data_prefix(data_view);
    s = g_strconcat(key, "/realsquare", NULL);
    gwy_container_gis_boolean_by_name(data, s, &realsquare);
    gwy_debug("view's realsquare: %d", realsquare);
    g_free(s);

    /* Update stuff */
    l = gtk_container_get_children(GTK_CONTAINER(menu));
    while (l) {
        item = GTK_WIDGET(l->data);
        i = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(item),
                                                corner_item_quark));
        switch (i) {
            case ITEM_PIXELSQUARE:
            if (!realsquare) {
                gwy_debug("setting Pixelwise active");
                id = g_signal_handler_find(item, G_SIGNAL_MATCH_FUNC,
                                           0, 0, NULL,
                                           gwy_app_data_window_change_square,
                                           NULL);
                g_signal_handler_block(item, id);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
                g_signal_handler_unblock(item, id);
            }
            break;

            case ITEM_REALSQUARE:
            if (realsquare) {
                gwy_debug("setting Physical active");
                id = g_signal_handler_find(item, G_SIGNAL_MATCH_FUNC,
                                           0, 0, NULL,
                                           gwy_app_data_window_change_square,
                                           NULL);
                g_signal_handler_block(item, id);
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
                g_signal_handler_unblock(item, id);
            }
            break;

            default:
            break;
        }
        l = g_list_next(l);
    }
}

static gboolean
gwy_app_data_corner_menu_popup_mouse(GtkWidget *menu,
                                     GdkEventButton *event,
                                     GtkWidget *view)
{
    GtkWidget *window;

    if (event->button != 1)
        return FALSE;

    window = gtk_widget_get_toplevel(view);
    g_return_val_if_fail(window, FALSE);
    gwy_app_data_window_set_current(GWY_DATA_WINDOW(window));

    gwy_app_data_corner_menu_update(menu, window);

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   event->button, event->time);

    return FALSE;
}

static GtkWidget*
gwy_app_menu_data_corner_create(GtkAccelGroup *accel_group)
{
    GtkWidget *menu, *item;
    GtkRadioMenuItem *r;

    corner_item_quark = g_quark_from_static_string("id");

    menu = gtk_menu_new();
    if (accel_group)
        gtk_menu_set_accel_group(GTK_MENU(menu), accel_group);

    item = gtk_radio_menu_item_new_with_mnemonic(NULL,
                                                 _("Pi_xelwise Square"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_qdata(G_OBJECT(item), corner_item_quark,
                       GUINT_TO_POINTER(ITEM_PIXELSQUARE));
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_app_data_window_change_square),
                     GINT_TO_POINTER(FALSE));

    r = GTK_RADIO_MENU_ITEM(item);
    item = gtk_radio_menu_item_new_with_mnemonic_from_widget(r,
                                                             _("_Physically "
                                                               "Square"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_qdata(G_OBJECT(item), corner_item_quark,
                       GUINT_TO_POINTER(ITEM_REALSQUARE));
    g_signal_connect(item, "activate",
                     G_CALLBACK(gwy_app_data_window_change_square),
                     GINT_TO_POINTER(TRUE));

    return menu;
}

static void
gwy_app_data_window_change_square(GtkWidget *item,
                                  gpointer user_data)
{
    gboolean realsquare = GPOINTER_TO_INT(user_data);
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyContainer *data;
    const gchar *key;
    gchar *s;

    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item))) {
        gwy_debug("bogus update");
        return;
    }

    gwy_debug("new square mode: %s", realsquare ? "Physical" : "Pixelwise");
    data_window = gwy_app_data_window_get_current();
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(data_view);
    key = gwy_data_view_get_data_prefix(data_view);
    g_return_if_fail(key);
    s = g_strconcat(key, "/realsquare", NULL);
    if (realsquare)
        gwy_container_set_boolean_by_name(data, s, realsquare);
    else
        gwy_container_remove_by_name(data, s);
    g_free(s);
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

/* FIXME: we should zoom whatever is currently active: datawindow, 3dwindow,
 * graph */
void
gwy_app_zoom_set_cb(gpointer data)
{
    GwyDataWindow *data_window;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(data_window);
    gwy_data_window_set_zoom(data_window, GPOINTER_TO_INT(data));
}

static void
gwy_app_save_3d_export(GtkWidget *button, Gwy3DWindow *gwy3dwindow)
{
    const gchar *filename;
    gchar *filename_sys, *filename_utf8;
    GdkPixbuf *pixbuf;
    GtkWidget *dialog, *gwy3dview;
    GError *err = NULL;

    gwy3dview = gwy_3d_window_get_3d_view(GWY_3D_WINDOW(gwy3dwindow));

    dialog = gtk_widget_get_toplevel(button);
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    filename_sys = g_strdup(filename);
    gtk_widget_destroy(dialog);

    pixbuf = gwy_3d_view_get_pixbuf(GWY_3D_VIEW(gwy3dview));
    filename_utf8 = g_filename_to_utf8(filename_sys, -1, NULL, NULL, NULL);
    if (!gdk_pixbuf_save(pixbuf, filename_sys, "png", &err, NULL)) {
        dialog = gtk_message_dialog_new(NULL,
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        "Cannot save report to %s.\n%s\n",
                                        filename_utf8,
                                        err->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_clear_error(&err);
    }
    g_free(filename_sys);
    g_object_unref(pixbuf);
    g_free(g_object_get_data(G_OBJECT(gwy3dwindow), "gwy-app-export-filename"));
    g_object_set_data(G_OBJECT(gwy3dwindow), "gwy-app-export-filename",
                      filename_utf8);
}

static void
gwy_app_3d_window_export(Gwy3DWindow *gwy3dwindow)
{
    GwyContainer *data;
    GtkWidget *dialog, *gwy3dview;
    const guchar *filename_utf8;
    gchar *filename_sys;
    gboolean need_free_utf = FALSE;

    gwy3dview = gwy_3d_window_get_3d_view(gwy3dwindow);
    data = gwy_3d_view_get_data(GWY_3D_VIEW(gwy3dview));

    filename_utf8 = g_object_get_data(G_OBJECT(gwy3dwindow),
                                      "gwy-app-export-filename");
    if (!filename_utf8) {
        if (gwy_container_gis_string_by_name(data, "/filename",
                                             &filename_utf8)) {
            /* FIXME: this is ugly, invent a better filename */
            filename_utf8 = g_strconcat(filename_utf8, ".png", NULL);
            need_free_utf = TRUE;
        }
        else
            filename_utf8 = "3d.png";
    }
    filename_sys = g_filename_from_utf8(filename_utf8, -1, NULL, NULL, NULL);
    if (need_free_utf)
        g_free((gpointer)filename_utf8);

    dialog = gtk_file_selection_new(_("Export 3D View"));
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), filename_sys);
    g_free(filename_sys);

    g_signal_connect(GTK_FILE_SELECTION(dialog)->ok_button, "clicked",
                     G_CALLBACK(gwy_app_save_3d_export), gwy3dwindow);
    g_signal_connect_swapped(GTK_FILE_SELECTION(dialog)->cancel_button,
                             "clicked",
                             G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gwy3dwindow));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_widget_show_all(dialog);
}

void
gwy_app_change_mask_color_cb(void)
{
    GwyDataWindow *data_window;
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GwyContainer *data, *settings;
    const gchar *key;
    GwyRGBA rgba;

    data_window = gwy_app_data_window_get_current();
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(data_view);
    g_assert(data);
    layer = gwy_data_view_get_alpha_layer(data_view);
    g_return_if_fail(GWY_IS_LAYER_MASK(layer));
    key = gwy_layer_mask_get_color_key(GWY_LAYER_MASK(layer));
    g_return_if_fail(key);
    gwy_debug("<%s>", key);

    /* copy defaults to data container if necessary */
    if (!gwy_rgba_get_from_container(&rgba, data, key)) {
        settings = gwy_app_settings_get();
        gwy_rgba_get_from_container(&rgba, settings, "/mask");
        gwy_rgba_store_to_container(&rgba, data, key);
    }
    gwy_color_selector_for_mask(NULL, NULL, data, key);
}

void
gwy_app_change_default_mask_color_cb(void)
{
    gwy_color_selector_for_mask(_("Change Default Mask Color"),
                                NULL, gwy_app_settings_get(), "/mask");
}

/**
 * gwy_app_save_window_position:
 * @window: A window to save position of.
 * @prefix: Unique prefix in settings to store the information under.
 * @position: %TRUE to save position information.
 * @size: %TRUE to save size information.
 *
 * Saves position and/or size of a window to settings.
 *
 * Some sanity checks are included, therefore if window position and/or size
 * is too suspicious, it is not saved.
 **/
void
gwy_app_save_window_position(GtkWindow *window,
                             const gchar *prefix,
                             gboolean position,
                             gboolean size)
{
    GwyContainer *settings;
    GdkScreen *screen;
    gint x, y, w, h, scw, sch;
    guint len;
    gchar *key;

    g_return_if_fail(GTK_IS_WINDOW(window));
    g_return_if_fail(prefix);
    if (!(position || size))
        return;

    len = strlen(prefix);
                              /* The longest suffix */
    key = g_newa(gchar, len + sizeof("/position/height"));
    strcpy(key, prefix);

    settings = gwy_app_settings_get();
    screen = gtk_window_get_screen(window);
    scw = gdk_screen_get_width(screen);
    sch = gdk_screen_get_height(screen);
    /* FIXME: read the gtk_window_get_position() docs about how this is
     * a broken approach */
    if (position) {
        gtk_window_get_position(window, &x, &y);
        if (x >= 0 && y >= 0 && x+1 < scw && y+1 < sch) {
            strcpy(key + len, "/position/x");
            gwy_container_set_int32_by_name(settings, key, x);
            strcpy(key + len, "/position/y");
            gwy_container_set_int32_by_name(settings, key, y);
        }
    }
    if (size) {
        gtk_window_get_size(window, &w, &h);
        if (w > 1 && h > 1) {
            strcpy(key + len, "/position/width");
            gwy_container_set_int32_by_name(settings, key, w);
            strcpy(key + len, "/position/height");
            gwy_container_set_int32_by_name(settings, key, h);
        }
    }
}

/**
 * gwy_app_restore_window_position:
 * @window: A window to restore position of.
 * @prefix: Unique prefix in settings to get the information from (the same as
 *          in gwy_app_save_window_position()).
 * @grow_only: %TRUE to only attempt set the window default size bigger than it
 *              requests, never smaller.
 *
 * Restores a window position and/or size from settings.
 *
 * Unlike gwy_app_save_window_position(), this function has no @position and
 * @size arguments, it simply restores all attributes that were saved.
 *
 * Note to restore position (not size) it should be called twice for each
 * window to accommodate sloppy window managers: once before the window is
 * shown, second time immediately after showing the window.
 *
 * Some sanity checks are included, therefore if saved window position and/or
 * size is too suspicious, it is not restored.
 **/
void
gwy_app_restore_window_position(GtkWindow *window,
                                const gchar *prefix,
                                gboolean grow_only)
{
    GwyContainer *settings;
    GtkRequisition req;
    GdkScreen *screen;
    gint x, y, w, h, scw, sch;
    guint len;
    gchar *key;

    g_return_if_fail(GTK_IS_WINDOW(window));
    g_return_if_fail(prefix);

    len = strlen(prefix);
                              /* The longest suffix */
    key = g_newa(gchar, len + sizeof("/position/height"));
    strcpy(key, prefix);

    settings = gwy_app_settings_get();
    screen = gtk_window_get_screen(window);
    scw = gdk_screen_get_width(screen);
    sch = gdk_screen_get_height(screen);
    x = y = w = h = -1;
    strcpy(key + len, "/position/x");
    gwy_container_gis_int32_by_name(settings, key, &x);
    strcpy(key + len, "/position/y");
    gwy_container_gis_int32_by_name(settings, key, &y);
    strcpy(key + len, "/position/width");
    gwy_container_gis_int32_by_name(settings, key, &w);
    strcpy(key + len, "/position/height");
    gwy_container_gis_int32_by_name(settings, key, &h);
    if (x >= 0 && y >= 0 && x+1 < scw && y+1 < sch)
        gtk_window_move(window, x, y);
    if (w > 1 && h > 1) {
        if (grow_only) {
            gtk_widget_size_request(GTK_WIDGET(window), &req);
            w = MAX(w, req.width);
            h = MAX(h, req.height);
        }
        gtk_window_set_default_size(window, w, h);
    }
}

/************************** Documentation ****************************/

/**
 * SECTION:app
 * @title: app
 * @short_description: Core application interface, window management
 **/

/**
 * GwyAppWindowType:
 * @GWY_APP_WINDOW_TYPE_DATA: Normal 2D data window.
 * @GWY_APP_WINDOW_TYPE_GRAPH: Graph window.
 * @GWY_APP_WINDOW_TYPE_3D: 3D view window.
 * @GWY_APP_WINDOW_TYPE_ANY: Window of any type.
 *
 * Application window types.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
