/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <math.h>
#include <string.h>
#include <libgwyddion/gwyddion.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
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

static GtkWidget *gwy_app_main_window = NULL;

static GList *current_data = NULL;
static GList *current_graphs = NULL;
static const gchar* current_tool = NULL;
static gint untitled_no = 0;

static gint       compare_data_window_data_cb (GwyDataWindow *window,
                                               GwyContainer *data);
static void       undo_redo_clean             (GObject *window,
                                               gboolean undo,
                                               gboolean redo);
static gboolean   gwy_app_confirm_quit        (void);
static void       gather_unsaved_cb           (GwyDataWindow *data_window,
                                               GSList **unsaved);
static gboolean   gwy_app_confirm_quit_dialog (GSList *unsaved);

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
    return FALSE;
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
 * Eventually adds @window it to the data window list if it isn't present
 * there.
 **/
void
gwy_app_data_window_set_current(GwyDataWindow *window)
{
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_DATA | GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_DATA
    };
    GList *item;

    gwy_debug("%p", window);

    g_return_if_fail(GWY_IS_DATA_WINDOW(window));
    item = g_list_find(current_data, window);
    if (item) {
        current_data = g_list_remove_link(current_data, item);
        current_data = g_list_concat(item, current_data);
    }
    else
        current_data = g_list_prepend(current_data, window);
    /* FIXME: this calls the use function a little bit too often */
    if (current_tool)
        gwy_tool_func_use(current_tool, window, GWY_TOOL_SWITCH_WINDOW);

    /* XXX: GwyAppFuckingUndo */
    if (g_object_get_data(G_OBJECT(window), "undo"))
        sens_data.set_to |= GWY_MENU_FLAG_UNDO;
    if (g_object_get_data(G_OBJECT(window), "redo"))
        sens_data.set_to |= GWY_MENU_FLAG_REDO;

    gwy_app_toolbox_update_state(&sens_data);
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
        GWY_MENU_FLAG_DATA, 0
    };
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

    if (current_tool)
        gwy_tool_func_use(current_tool, NULL, GWY_TOOL_SWITCH_WINDOW);
    gwy_app_toolbox_update_state(&sens_data);
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
    GtkWidget *data_window, *data_view;
    GtkObject *layer;

    data_view = gwy_data_view_new(data);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(data_view),
                                 GWY_PIXMAP_LAYER(layer));
    data_window = gwy_data_window_new(GWY_DATA_VIEW(data_view));
    gtk_window_add_accel_group(GTK_WINDOW(data_window),
                               g_object_get_data(G_OBJECT(gwy_app_main_window_get()),
                                                 "accel_group"));
    g_signal_connect(data_window, "focus-in-event",
                     G_CALLBACK(gwy_app_data_window_set_current), NULL);
    g_signal_connect(data_window, "destroy",
                     G_CALLBACK(gwy_app_data_window_remove), NULL);
    g_signal_connect_swapped(data_window, "destroy",
                             G_CALLBACK(g_object_unref), data);

    gwy_data_window_set_units(GWY_DATA_WINDOW(data_window), "m");
    gwy_data_window_update_title(GWY_DATA_WINDOW(data_window));
    gwy_app_data_view_update(data_view);
    gtk_window_present(GTK_WINDOW(data_window));

    return data_window;
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
    if (item) {
        current_graphs = g_list_remove_link(current_graphs, item);
        current_graphs = g_list_concat(item, current_graphs);
    }
    else
        current_graphs = g_list_prepend(current_graphs, window);

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

    g_signal_connect(window, "focus-in-event",
                     G_CALLBACK(gwy_app_graph_window_set_current), NULL);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(gwy_app_graph_window_remove), NULL);

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
 * then gets a title like "Foo 42", or a string "Bar %d" containing a single
 * '%d', the window then gets a title like "Bar 666".
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
    GwyDataWindow *data_window;

    gwy_debug("%s", toolname ? toolname : "NONE");
    /* don't catch deactivations */
    if (button && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        return;

    if (current_tool && (!toolname || strcmp(current_tool, toolname)))
        gwy_tool_func_use(current_tool, NULL, GWY_TOOL_SWITCH_TOOL);
    if (toolname) {
        data_window = gwy_app_data_window_get_current();
        if (data_window) {
            if (gwy_tool_func_use(toolname, data_window, GWY_TOOL_SWITCH_TOOL))
                current_tool = toolname;
            else
                gwy_tool_func_use(current_tool, data_window,
                                  GWY_TOOL_SWITCH_TOOL);
        }
    }
    else
        current_tool = toolname;
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
 * gwy_app_undo_checkpoint:
 * @data: A data container.
 * @what: What in the data should be put into the undo queue.
 *
 * Create a point in the undo history we can return to.
 *
 * XXX: It can only save the state of standard datafields.
 **/
void
gwy_app_undo_checkpoint(GwyContainer *data,
                        const gchar *what)
{
    const char *good_keys[] = {
        "/0/data", "/0/mask", "/0/show", NULL
    };
    GwyMenuSensData sens_data = {
        GWY_MENU_FLAG_UNDO | GWY_MENU_FLAG_REDO,
        GWY_MENU_FLAG_UNDO
    };
    GwyDataWindow *data_window;
    GwyAppFuckingUndo *undo;
    GObject *object;
    GList *l;
    const gchar **p;

    g_return_if_fail(GWY_IS_CONTAINER(data));
    for (p = good_keys; *p && strcmp(what, *p); p++)
        ;
    if (!*p) {
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
    g_object_set_data(G_OBJECT(data), "modified",
        GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(data),
                                                          "modified")) + 1));
    gwy_app_toolbox_update_state(&sens_data);
}

void
gwy_app_undo_undo(void)
{
    GwyMenuSensData sens_data = {
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
    g_object_set_data(G_OBJECT(data), "modified",
        GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(data),
                                                          "modified")) - 1));
    gwy_app_data_view_update(data_view);
    gwy_app_toolbox_update_state(&sens_data);
}

void
gwy_app_undo_redo(void)
{
    GwyMenuSensData sens_data = {
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
    g_object_set_data(G_OBJECT(data), "modified",
        GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(data),
                                                          "modified")) + 1));
    gwy_app_data_view_update(data_view);
    gwy_app_toolbox_update_state(&sens_data);
}

static void
undo_redo_clean(GObject *window,
                gboolean undo,
                gboolean redo)
{
    GwyAppFuckingUndo *gafu;

    gwy_debug("");

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
    static const gdouble default_mask_color[4] = { 1.0, 0.0, 0.0, 0.5 };
    static const gchar *keys[] = {
        "/0/mask/red", "/0/mask/green", "/0/mask/blue", "/0/mask/alpha"
    };
    GwyDataWindow *data_window;
    GtkWidget *data_view = NULL;
    GwyContainer *data, *settings;
    GtkWidget *selector, *dialog;
    GdkColor gdkcolor;
    guint16 gdkalpha;
    gdouble p[4];
    gint i, response;

    if (!defaultc) {
        data_window = gwy_app_data_window_get_current();
        g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
        data_view = gwy_data_window_get_data_view(data_window);
        data = gwy_data_view_get_data(GWY_DATA_VIEW(data_view));
    }
    else
        data = NULL;

    settings = gwy_app_settings_get();
    for (i = 0; i < 4; i++) {
        if (data && gwy_container_contains_by_name(data, keys[i]))
            p[i] = gwy_container_get_double_by_name(data, keys[i]);
        else if (gwy_container_contains_by_name(settings, keys[i] + 2))
            p[i] = gwy_container_get_double_by_name(settings, keys[i] + 2);
        else
            p[i] = default_mask_color[i];
    }

    gdkcolor.red = (guint16)floor(p[0]*65535.999999);
    gdkcolor.green = (guint16)floor(p[1]*65535.999999);
    gdkcolor.blue = (guint16)floor(p[2]*65535.999999);
    gdkalpha = (guint16)floor(p[3]*65535.999999);
    gdkcolor.pixel = (guint32)-1; /* FIXME */

    if (defaultc)
        dialog = gtk_color_selection_dialog_new(_("Change Default Mask Color"));
    else
        dialog = gtk_color_selection_dialog_new(_("Change Mask Color"));
    selector = GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel;
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(selector),
                                          &gdkcolor);
    gtk_color_selection_set_current_alpha(GTK_COLOR_SELECTION(selector),
                                          gdkalpha);
    gtk_color_selection_set_has_palette(GTK_COLOR_SELECTION(selector), FALSE);
    gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(selector),
                                                TRUE);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(selector),
                                          &gdkcolor);
    gdkalpha
        = gtk_color_selection_get_current_alpha(GTK_COLOR_SELECTION(selector));
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_OK)
        return;

    p[0] = gdkcolor.red/65535.0;
    p[1] = gdkcolor.green/65535.0;
    p[2] = gdkcolor.blue/65535.0;
    p[3] = gdkalpha/65535.0;

    for (i = 0; i < 4; i++)
        gwy_container_set_double_by_name(defaultc ? settings : data,
                                         keys[i] + (defaultc ? 2 : 0),
                                         p[i]);
    if (!defaultc)
        gwy_data_view_update(GWY_DATA_VIEW(data_view));
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
        g_warning("Setting app main window to a non-GtkWindow");
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

    if (g_object_get_data(G_OBJECT(data), "modified"))
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

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    return response == GTK_RESPONSE_YES;
}

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
        gwy_app_undo_checkpoint(data, "/0/mask");
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
        gwy_app_undo_checkpoint(data, "/0/show");
        gwy_container_remove_by_name(data, "/0/show");
        gwy_data_view_update(GWY_DATA_VIEW(data_view));
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
