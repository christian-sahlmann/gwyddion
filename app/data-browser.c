/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Chris Anderson
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, sidewinderasu@gmail.com.
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

#define DEBUG 1
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

/* The container prefix all graph reside in.  This is a bit silly but it does
 * not worth to break file compatibility with 1.x. */
#define GRAPH_PREFIX "/0/graph/graph"

/* Data type keys interesting can correspond to */
typedef enum {
    KEY_IS_NONE = 0,
    KEY_IS_DATA,
    KEY_IS_MASK,
    KEY_IS_SHOW,
    KEY_IS_GRAPH
} GwyAppKeyType;

/* Notebook pages */
enum {
    PAGE_CHANNELS,
    PAGE_GRAPHS
};

/* Sensitivity flags */
enum {
    SENS_OBJECT = 1 << 0,
    SENS_FILE   = 1 << 1,
    SENS_MASK   = 0x03
};

/* Channel and graph tree store columns */
enum {
    MODEL_ID,
    MODEL_OBJECT,
    MODEL_WIDGET,
    MODEL_N_COLUMNS
};

typedef struct _GwyAppDataBrowser GwyAppDataBrowser;
typedef struct _GwyAppDataProxy   GwyAppDataProxy;

/* Channel or graph list */
typedef struct {
    GtkListStore *list;
    gint last;
    gint active;
} GwyAppDataList;

/* The data browser */
struct _GwyAppDataBrowser {
    GList *proxy_list;
    struct _GwyAppDataProxy *current;
    gint active_page;
    GwySensitivityGroup *sensgroup;
    GtkWidget *filename;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *channels;
    GtkWidget *graphs;
};

/* The proxy associated with each Container (this is non-GUI object) */
struct _GwyAppDataProxy {
    struct _GwyAppDataBrowser *parent;
    gint refcount;    /* the number of views we manage */
    guint deserted_id;
    GwyContainer *container;
    GwyAppDataList channels;
    GwyAppDataList graphs;
};

static GwyAppDataBrowser* gwy_app_get_data_browser        (void);
static void gwy_app_data_browser_switch_data(GwyContainer *data);
static void gwy_app_data_browser_sync_mask  (GwyContainer *data,
                                             GQuark quark,
                                             GwyDataView *data_view);
static void gwy_app_data_browser_sync_show  (GwyContainer *data,
                                             GQuark quark,
                                             GwyDataView *data_view);

static GQuark container_quark = 0;
static GQuark own_key_quark   = 0;

/* The data browser */
static GwyAppDataBrowser *gwy_app_data_browser = NULL;

/**
 * gwy_app_data_proxy_compare_data:
 * @a: Pointer to a #GwyAppDataProxy.
 * @b: Pointer to a #GwyContainer.
 *
 * Compares two containers (one of them referenced by a data proxy).
 *
 * Returns: Zero if the containers are equal, nonzero otherwise.  This function
 *          is intended only for equality tests, not ordering.
 **/
static gint
gwy_app_data_proxy_compare_data(gconstpointer a,
                                gconstpointer b)
{
    GwyAppDataProxy *ua = (GwyAppDataProxy*)a;

    return (guchar*)ua->container - (guchar*)b;
}

/**
 * emit_row_changed:
 * @store: A list store.
 * @iter: A tree model iterator that belongs to @store.
 *
 * Auxiliary function to emit "row-changed" signal on a list store.
 **/
static void
emit_row_changed(GtkListStore *store,
                 GtkTreeIter *iter)
{
    GtkTreeModel *model = GTK_TREE_MODEL(store);
    GtkTreePath *path;

    path = gtk_tree_model_get_path(model, iter);
    gtk_tree_model_row_changed(model, path, iter);
    gtk_tree_path_free(path);
}

/**
 * gwy_app_data_proxy_analyse_key:
 * @strkey: String container key.
 * @type: Location to store data type to.
 * @len: Location to store the length of prefix up to the last digit of data
 *       number to, or %NULL.
 *
 * Infers expected data type from container key.
 *
 * When key is not recognized, @type is set to KEY_IS_NONE and value of @len
 * is unchanged.
 *
 * Returns: Data number (id), -1 when key does not correspond to any data
 *          object.
 **/
static gint
gwy_app_data_proxy_analyse_key(const gchar *strkey,
                               GwyAppKeyType *type,
                               guint *len)
{
    const gchar *s;
    guint i, n;

    *type = KEY_IS_NONE;

    if (strkey[0] != GWY_CONTAINER_PATHSEP)
        return -1;

    /* Graph */
    if (g_str_has_prefix(strkey, GRAPH_PREFIX GWY_CONTAINER_PATHSEP_STR)) {
        s = strkey + sizeof(GRAPH_PREFIX);
        /* Do not use strtol, it allows queer stuff like spaces */
        for (i = 0; g_ascii_isdigit(s[i]); i++)
            ;
        if (!i || s[i])
            return -1;

        *type = KEY_IS_GRAPH;
        if (len)
            *len = (s + i) - strkey;

        return atoi(s);
    }

    /* Other data */
    s = strkey + 1;
    for (i = 0; g_ascii_isdigit(s[i]); i++)
        ;
    if (!i || s[i] != GWY_CONTAINER_PATHSEP)
        return -1;

    n = i + 2;
    i = atoi(s);
    s = strkey + n;
    if (gwy_strequal(s, "data"))
        *type = KEY_IS_DATA;
    else if (gwy_strequal(s, "mask"))
        *type = KEY_IS_MASK;
    else if (gwy_strequal(s, "show"))
        *type = KEY_IS_SHOW;
    else
        i = -1;

    if (len && i > -1)
        *len = n;

    return i;
}

static void
gwy_app_data_proxy_connect_object(GwyAppDataList *list,
                                  gint i,
                                  GObject *object)
{
    GtkTreeIter iter;

    gtk_list_store_append(list->list, &iter);
    gtk_list_store_set(list->list, &iter,
                       MODEL_ID, i,
                       MODEL_OBJECT, object,
                       MODEL_WIDGET, NULL,
                       -1);
    if (list->last < i)
        list->last = i;
}

static void
gwy_app_data_proxy_channel_changed(GwyDataField *channel,
                                   GwyAppDataProxy *proxy)
{
    gwy_debug("proxy=%p channel=%p", proxy, channel);
}

static void
gwy_app_data_proxy_connect_channel(GwyAppDataProxy *proxy,
                                   gint i,
                                   GObject *object)
{
    gchar key[24];
    GQuark quark;

    gwy_app_data_proxy_connect_object(&proxy->channels, i, object);
    g_snprintf(key, sizeof(key), "/%d/data", i);
    gwy_debug("Setting keys on DataField %p (%s)", object, key);
    quark = g_quark_from_string(key);
    g_object_set_qdata(object, container_quark, proxy->container);
    g_object_set_qdata(object, own_key_quark, GUINT_TO_POINTER(quark));

    g_signal_connect(object, "data-changed",
                     G_CALLBACK(gwy_app_data_proxy_channel_changed), proxy);
}

static void
gwy_app_data_proxy_disconnect_channel(GwyAppDataProxy *proxy,
                                      GtkTreeIter *iter)
{
    GObject *object;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->channels.list), iter,
                       MODEL_OBJECT, &object,
                       -1);
    g_signal_handlers_disconnect_by_func(object,
                                         gwy_app_data_proxy_channel_changed,
                                         proxy);
    g_object_unref(object);
    gtk_list_store_remove(proxy->channels.list, iter);
}

static void
gwy_app_data_proxy_reconnect_channel(GwyAppDataProxy *proxy,
                                     GtkTreeIter *iter,
                                     GObject *object)
{
    GObject *old;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->channels.list), iter,
                       MODEL_OBJECT, &old,
                       -1);
    g_signal_handlers_disconnect_by_func(old,
                                         gwy_app_data_proxy_channel_changed,
                                         proxy);
    g_object_set_qdata(object, container_quark,
                       g_object_get_qdata(old, container_quark));
    g_object_set_qdata(object, own_key_quark,
                       g_object_get_qdata(old, own_key_quark));
    gtk_list_store_set(proxy->channels.list, iter,
                       MODEL_OBJECT, object,
                       -1);
    g_signal_connect(object, "data-changed",
                     G_CALLBACK(gwy_app_data_proxy_channel_changed), proxy);
    g_object_unref(old);
}

static void
gwy_app_data_proxy_graph_changed(GwyGraphModel *graph,
                                 GwyAppDataProxy *proxy)
{
    gwy_debug("proxy=%p, graph=%p", proxy, graph);
}

static void
gwy_app_data_proxy_connect_graph(GwyAppDataProxy *proxy,
                                 gint i,
                                 GObject *object)
{
    gchar key[32];
    GQuark quark;

    gwy_app_data_proxy_connect_object(&proxy->graphs, i, object);
    g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, i);
    gwy_debug("Setting keys on GraphModel %p (%s)", object, key);
    quark = g_quark_from_string(key);
    g_object_set_qdata(object, container_quark, proxy->container);
    g_object_set_qdata(object, own_key_quark, GUINT_TO_POINTER(quark));

    g_signal_connect(object, "layout-updated", /* FIXME */
                     G_CALLBACK(gwy_app_data_proxy_graph_changed), proxy);
}

static void
gwy_app_data_proxy_disconnect_graph(GwyAppDataProxy *proxy,
                                    GtkTreeIter *iter)
{
    GObject *object;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->graphs.list), iter,
                       MODEL_OBJECT, &object,
                       -1);
    g_signal_handlers_disconnect_by_func(object,
                                         gwy_app_data_proxy_graph_changed,
                                         proxy);
    g_object_unref(object);
    gtk_list_store_remove(proxy->graphs.list, iter);
}

static void
gwy_app_data_proxy_reconnect_graph(GwyAppDataProxy *proxy,
                                   GtkTreeIter *iter,
                                   GObject *object)
{
    GObject *old;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->graphs.list), iter,
                       MODEL_OBJECT, &old,
                       -1);
    g_signal_handlers_disconnect_by_func(old,
                                         gwy_app_data_proxy_graph_changed,
                                         proxy);
    g_object_set_qdata(object, container_quark,
                       g_object_get_qdata(old, container_quark));
    g_object_set_qdata(object, own_key_quark,
                       g_object_get_qdata(old, own_key_quark));
    gtk_list_store_set(proxy->graphs.list, iter,
                       MODEL_OBJECT, object,
                       -1);
    g_signal_connect(object, "layout-updated", /* FIXME */
                     G_CALLBACK(gwy_app_data_proxy_graph_changed), proxy);
    g_object_unref(old);
}

/**
 * gwy_app_data_proxy_scan_data:
 * @key: Container quark key.
 * @value: Value at @key.
 * @userdata: Data proxy.
 *
 * Adds a data object from Container to data proxy.
 *
 * More precisely, if the key and value is found to be data channel or graph
 * it's added.  Other container items are ignored.
 **/
static void
gwy_app_data_proxy_scan_data(gpointer key,
                             gpointer value,
                             gpointer userdata)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GValue *gvalue = (GValue*)value;
    GwyAppDataProxy *proxy = (GwyAppDataProxy*)userdata;
    const gchar *strkey;
    GwyAppKeyType type;
    GObject *object;
    gint i;

    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    if (i == -1)
        return;

    g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
    object = g_value_get_object(gvalue);

    switch (type) {
        case KEY_IS_DATA:
        gwy_debug("Found data %d (%s)", i, strkey);
        gwy_app_data_proxy_connect_channel(proxy, i, object);
        break;

        case KEY_IS_GRAPH:
        gwy_debug("Found graph %d (%s)", i, strkey);
        gwy_app_data_proxy_connect_graph(proxy, i, object);
        break;

        case KEY_IS_MASK:
        /* FIXME */
        gwy_debug("Found mask %d (%s)", i, strkey);
        break;

        case KEY_IS_SHOW:
        /* FIXME */
        gwy_debug("Found presentation %d (%s)", i, strkey);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

/**
 * gwy_app_data_proxy_finalize_list:
 * @model: A tree model.
 * @column: Model column that contains the objects.
 * @func: A callback connected to the objects.
 * @data: User data for @func.
 *
 * Disconnect a callback from all objects in a tree model.
 **/
static void
gwy_app_data_proxy_finalize_list(GtkTreeModel *model,
                                 gint column,
                                 gpointer func,
                                 gpointer data)
{
    GObject *object;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        gtk_tree_model_get(model, &iter, column, &object, -1);
        g_signal_handlers_disconnect_by_func(object, func, data);
        g_object_unref(object);
    } while (gtk_tree_model_iter_next(model, &iter));

    g_object_unref(model);
}

/**
 * gwy_app_data_proxy_find_object:
 * @model: Data proxy list store (channels, graphs).
 * @i: Object number to find.
 * @iter: Tree iterator to set to row containing object @i.
 *
 * Find an object in data proxy list store.
 *
 * Returns: %TRUE if object was found and @iter set, %FALSE otherwise (@iter
 *          is invalid then).
 **/
static gboolean
gwy_app_data_proxy_find_object(GtkListStore *store,
                               gint i,
                               GtkTreeIter *iter)
{
    GtkTreeModel *model;
    gint objid;

    model = GTK_TREE_MODEL(store);
    if (!gtk_tree_model_get_iter_first(model, iter))
        return FALSE;

    do {
        gtk_tree_model_get(model, iter, MODEL_ID, &objid, -1);
        if (objid == i)
            return TRUE;
    } while (gtk_tree_model_iter_next(model, iter));

    return FALSE;
}

/**
 * gwy_app_data_proxy_item_changed:
 * @data: A data container.
 * @quark: Quark key of item that has changed.
 * @proxy: Data proxy.
 *
 * Updates a data proxy in response to a Container "item-changed" signal.
 **/
static void
gwy_app_data_proxy_item_changed(GwyContainer *data,
                                GQuark quark,
                                GwyAppDataProxy *proxy)
{
    GObject *object = NULL;
    const gchar *strkey;
    GwyAppKeyType type;
    GtkTreeIter iter;
    GwyDataView *data_view;
    gboolean found;
    gint i;

    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    if (i < 0)
        return;

    gwy_container_gis_object(data, quark, &object);
    switch (type) {
        case KEY_IS_DATA:
        found = gwy_app_data_proxy_find_object(proxy->channels.list, i, &iter);
        gwy_debug("Channel <%s>: %s in container, %s in list store",
                  strkey,
                  object ? "present" : "missing",
                  found ? "present" : "missing");
        g_return_if_fail(object || found);
        if (object && !found)
            gwy_app_data_proxy_connect_channel(proxy, i, object);
        else if (!object && found)
            gwy_app_data_proxy_disconnect_channel(proxy, &iter);
        else {
            gwy_app_data_proxy_reconnect_channel(proxy, &iter, object);
            emit_row_changed(proxy->channels.list, &iter);
        }
        break;

        case KEY_IS_GRAPH:
        found = gwy_app_data_proxy_find_object(proxy->graphs.list, i, &iter);
        gwy_debug("Graph <%s>: %s in container, %s in list store",
                  strkey,
                  object ? "present" : "missing",
                  found ? "present" : "missing");
        g_return_if_fail(object || found);
        if (object && !found)
            gwy_app_data_proxy_connect_graph(proxy, i, object);
        else if (!object && found)
            gwy_app_data_proxy_disconnect_graph(proxy, &iter);
        else {
            gwy_app_data_proxy_reconnect_graph(proxy, &iter, object);
            emit_row_changed(proxy->channels.list, &iter);
        }
        break;

        case KEY_IS_MASK:
        case KEY_IS_SHOW:
        /* FIXME */
        found = gwy_app_data_proxy_find_object(proxy->channels.list, i, &iter);
        if (found)
            emit_row_changed(proxy->channels.list, &iter);
        gtk_tree_model_get(GTK_TREE_MODEL(proxy->channels.list), &iter,
                           MODEL_WIDGET, &data_view,
                           -1);
        /* XXX: This is not a good place to do that, DataProxy should be
         * non-GUI */
        if (data_view) {
            if (type == KEY_IS_MASK)
                gwy_app_data_browser_sync_mask(data, quark, data_view);
            else
                gwy_app_data_browser_sync_show(data, quark, data_view);
            g_object_unref(data_view);
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

/**
 * gwy_app_data_proxy_deserted:
 * @proxy: A data proxy.
 *
 * Handle all-views-were-closed situation.  To be used as idle function.
 *
 * The current reaction is to simply close the file.
 *
 * Returns: Always %FALSE.
 **/
static gboolean
gwy_app_data_proxy_deserted(GwyAppDataProxy *proxy)
{

    GwyAppDataBrowser *browser = proxy->parent;

    proxy->deserted_id = 0;
    if (proxy->refcount)
        return FALSE;

    /* FIXME: this is crude */
    if (browser->current == proxy)
        gwy_app_data_browser_switch_data(NULL);

    gwy_debug("Freeing proxy for Container %p", proxy->container);
    browser->proxy_list = g_list_remove(browser->proxy_list, proxy);

    gwy_app_data_proxy_finalize_list(GTK_TREE_MODEL(proxy->channels.list),
                                     MODEL_OBJECT,
                                     &gwy_app_data_proxy_channel_changed,
                                     proxy);
    gwy_app_data_proxy_finalize_list(GTK_TREE_MODEL(proxy->graphs.list),
                                     MODEL_OBJECT,
                                     &gwy_app_data_proxy_graph_changed,
                                     proxy);
    g_object_unref(proxy->container);
    g_free(proxy);

    /* Ask for removal if used in idle function */
    return FALSE;
}

static void
gwy_app_data_proxy_unref(GwyAppDataProxy *proxy)
{
    gwy_debug("proxy %p, old refcount = %d", proxy, proxy->refcount);
    g_return_if_fail(proxy->refcount > 0);
    proxy->refcount--;
    if (proxy->refcount || proxy->deserted_id)
        return;

    /* Delay proxy destruction, the tree model does not like being
     * destroyed inside "toggled" callback */
    proxy->deserted_id = g_idle_add((GSourceFunc)&gwy_app_data_proxy_deserted,
                                    proxy);
}

static void
gwy_app_data_proxy_list_setup(GwyAppDataList *list)
{
    list->list = gtk_list_store_new(MODEL_N_COLUMNS,
                                    G_TYPE_INT,
                                    G_TYPE_OBJECT,
                                    G_TYPE_OBJECT);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list->list),
                                         MODEL_ID, GTK_SORT_ASCENDING);
    list->last = -1;
    list->active = -1;
}

/**
 * gwy_app_data_proxy_new:
 * @browser: Parent data browser for the new proxy.
 * @data: Data container to manage by the new proxy.
 *
 * Creates a data proxy for a data container.
 *
 * Note not only @parent field of the new proxy is set to @browser, but in
 * addition the new proxy is added to @browser's container list (as the new
 * list head).
 *
 * Returns: A new data proxy.
 **/
static GwyAppDataProxy*
gwy_app_data_proxy_new(GwyAppDataBrowser *browser,
                       GwyContainer *data)
{
    GwyAppDataProxy *proxy;

    gwy_debug("Creating proxy for Container %p", data);
    g_object_ref(data);
    proxy = g_new0(GwyAppDataProxy, 1);
    proxy->container = data;
    proxy->parent = browser;
    browser->proxy_list = g_list_prepend(browser->proxy_list, proxy);
    g_signal_connect_after(data, "item-changed",
                           G_CALLBACK(gwy_app_data_proxy_item_changed), proxy);

    gwy_app_data_proxy_list_setup(&proxy->channels);
    gwy_app_data_proxy_list_setup(&proxy->graphs);
    /* For historical reasons, graphs are numbered from 1 */
    proxy->graphs.last = 0;

    gwy_container_foreach(data, NULL, gwy_app_data_proxy_scan_data, proxy);

    return proxy;
}

/**
 * gwy_app_data_browser_get_proxy:
 * @browser: A data browser.
 * @data: The container to find data proxy for.
 * @do_create: %TRUE to create a new proxy when none is found, %FALSE to return
 *             %NULL when proxy is not found.
 *
 * Finds data proxy managing a container.
 *
 * Returns: The data proxy managing container (perhaps newly created when
 *          @do_create is %TRUE), or %NULL.  It is assumed only one proxy
 *          exists for each container.
 **/
static GwyAppDataProxy*
gwy_app_data_browser_get_proxy(GwyAppDataBrowser *browser,
                               GwyContainer *data,
                               gboolean do_create)
{
    GList *item;

    /* Optimize the fast path */
    if (browser->current && browser->current->container == data)
        return browser->current;

    item = g_list_find_custom(browser->proxy_list, data,
                              &gwy_app_data_proxy_compare_data);
    if (!item) {
        if (do_create)
            return gwy_app_data_proxy_new(browser, data);
        else
            return NULL;
    }

    /* Move container to head */
    if (item != browser->proxy_list) {
        browser->proxy_list = g_list_remove_link(browser->proxy_list, item);
        browser->proxy_list = g_list_concat(item, browser->proxy_list);
    }

    return (GwyAppDataProxy*)item->data;
}

static void
gwy_app_data_proxy_update_visibility(GObject *object,
                                     gboolean visible)
{
    GwyContainer *data;
    const gchar *strkey;
    gchar key[48];
    GQuark quark;

    data = g_object_get_qdata(object, container_quark);
    quark = GPOINTER_TO_UINT(g_object_get_qdata(object, own_key_quark));
    strkey = g_quark_to_string(quark);
    g_snprintf(key, sizeof(key), "%s/visible", strkey);
    if (visible)
        gwy_container_set_boolean_by_name(data, key, TRUE);
    else
        gwy_container_remove_by_name(data, key);
}

static void
gwy_app_data_browser_render_visible(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                    GtkCellRenderer *renderer,
                                    GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    G_GNUC_UNUSED gpointer userdata)
{
    GtkWidget *widget;

    gtk_tree_model_get(model, iter, MODEL_WIDGET, &widget, -1);
    g_object_set(G_OBJECT(renderer), "active", widget != NULL, NULL);
    gwy_object_unref(widget);
}

static void
gwy_app_data_browser_channel_render_title(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                          GtkCellRenderer *renderer,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          gpointer userdata)
{
    GwyAppDataBrowser *browser = (GwyAppDataBrowser*)userdata;
    const guchar *title = NULL;
    GwyContainer *data;
    gchar key[32];
    gint channel;

    /* XXX: browser->current must match what is visible in the browser */
    data = browser->current->container;

    gtk_tree_model_get(model, iter, MODEL_ID, &channel, -1);
    g_snprintf(key, sizeof(key), "/%i/data/title", channel);
    gwy_container_gis_string_by_name(data, key, &title);
    if (!title) {
        g_snprintf(key, sizeof(key), "/%i/data/untitled", channel);
        gwy_container_gis_string_by_name(data, key, &title);
    }
    /* Support 1.x titles */
    if (!title)
        gwy_container_gis_string_by_name(data, "/filename/title", &title);

    if (!title)
        title = _("Unknown channel");

    g_object_set(G_OBJECT(renderer), "text", title, NULL);
}

static void
gwy_app_data_browser_channel_selection_changed(GtkTreeSelection *selection,
                                               GwyAppDataBrowser *browser)
{
    gboolean any;

    /* This can happen when data is manipulated programatically */
    if (browser->active_page != PAGE_CHANNELS)
        return;

    any = gtk_tree_selection_get_selected(selection, NULL, NULL);
    gwy_debug("Any channel: %d", any);
    gwy_sensitivity_group_set_state(browser->sensgroup,
                                    SENS_OBJECT, any ? SENS_OBJECT : 0);
}

static void
gwy_app_data_browser_channel_render_flags(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                          GtkCellRenderer *renderer,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          gpointer userdata)
{
    GwyAppDataBrowser *browser = (GwyAppDataBrowser*)userdata;
    gboolean has_mask, has_show;
    GwyContainer *data;
    gchar key[24];
    gint channel;

    /* XXX: browser->current must match what is visible in the browser */
    data = browser->current->container;

    gtk_tree_model_get(model, iter, MODEL_ID, &channel, -1);
    g_snprintf(key, sizeof(key), "/%i/mask", channel);
    has_mask = gwy_container_contains_by_name(data, key);
    g_snprintf(key, sizeof(key), "/%i/show", channel);
    has_show = gwy_container_contains_by_name(data, key);

    g_snprintf(key, sizeof(key), "%s%s",
               has_mask ? "M" : "",
               has_show ? "P" : "");

    g_object_set(G_OBJECT(renderer), "text", key, NULL);
}

/**
 * gwy_app_data_browser_channel_deleted:
 * @data_window: A data window that was deleted.
 *
 * Destroys a deleted data window, updating proxy.
 *
 * This functions makes sure various updates happen in reasonable order,
 * simple gtk_widget_destroy() on the data window would not do that.
 *
 * Returns: Always %TRUE to be usable as terminal event handler.
 **/
static gboolean
gwy_app_data_browser_channel_deleted(GwyDataWindow *data_window)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GwyAppKeyType type;
    GwyContainer *data;
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GtkTreeIter iter;
    const gchar *strkey;
    GObject *dfield;
    GQuark quark;
    gint i;

    gwy_debug("Data window %p deleted", data_window);
    data_view  = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(data_view);
    layer = gwy_data_view_get_base_layer(data_view);
    strkey = gwy_pixmap_layer_get_data_key(layer);
    quark = g_quark_from_string(strkey);
    g_return_val_if_fail(data && quark, TRUE);
    dfield = gwy_container_get_object(data, quark);

    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    g_return_val_if_fail(i >= 0 && type == KEY_IS_DATA, TRUE);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    if (!gwy_app_data_proxy_find_object(proxy->channels.list, i, &iter)) {
        g_critical("Cannot find data field %p (%d)", dfield, i);
        return TRUE;
    }

    gwy_app_data_proxy_unref(proxy);
    gtk_list_store_set(proxy->channels.list, &iter, MODEL_WIDGET, NULL, -1);
    gwy_app_data_window_remove(data_window);
    gtk_widget_destroy(GTK_WIDGET(data_window));

    return TRUE;
}

static void
gwy_app_data_proxy_setup_mask(GwyContainer *data,
                              gint i)
{
    static const gchar *keys[] = {
        "/%d/mask/red", "/%d/mask/green", "/%d/mask/blue", "/%d/mask/alpha"
    };

    GwyContainer *settings;
    gchar key[32];
    const gchar *gkey;
    gdouble x;
    guint j;

    settings = gwy_app_settings_get();
    for (j = 0; j < G_N_ELEMENTS(keys); j++) {
        g_snprintf(key, sizeof(key), keys[j], i);
        if (gwy_container_contains_by_name(data, key))
            continue;
        /* XXX: This is a dirty trick stripping the first 3 chars of key */
        gkey = keys[j] + 3;
        if (!gwy_container_gis_double_by_name(data, gkey, &x))
            /* be noisy when we don't have default mask color */
            x = gwy_container_get_double_by_name(settings, gkey);
        gwy_container_set_double_by_name(data, key, x);
    }
}

static void
gwy_app_data_browser_sync_show(GwyContainer *data,
                               GQuark quark,
                               G_GNUC_UNUSED GwyDataView *data_view)
{
    gboolean has_show;

    if (data != gwy_app_get_current_data())
        return;

    has_show = gwy_container_contains(data, quark);
    gwy_debug("Syncing show sens flags");
    gwy_app_sensitivity_set_state(GWY_MENU_FLAG_DATA_SHOW,
                                  has_show ? GWY_MENU_FLAG_DATA_SHOW: 0);
}

static void
gwy_app_data_browser_sync_mask(GwyContainer *data,
                               GQuark quark,
                               GwyDataView *data_view)
{
    gboolean has_dfield, has_layer;
    const gchar *strkey;
    GwyPixmapLayer *layer;
    GwyAppKeyType type;
    gint i;

    has_dfield = gwy_container_contains(data, quark);
    has_layer = gwy_data_view_get_alpha_layer(data_view) != NULL;
    gwy_debug("has_dfield: %d, has_layer: %d", has_dfield, has_layer);

    if (has_dfield && !has_layer) {
        strkey = g_quark_to_string(quark);
        i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
        g_return_if_fail(i >= 0 && type == KEY_IS_MASK);
        gwy_app_data_proxy_setup_mask(data, i);
        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, strkey);
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), strkey);
        gwy_data_view_set_alpha_layer(data_view, layer);
    }
    else if (!has_dfield && has_layer)
        gwy_data_view_set_alpha_layer(data_view, NULL);

    if (has_dfield != has_layer
        && data == gwy_app_get_current_data()) {
        gwy_debug("Syncing mask sens flags");
        gwy_app_sensitivity_set_state(GWY_MENU_FLAG_DATA_MASK,
                                      has_dfield ? GWY_MENU_FLAG_DATA_MASK : 0);
    }
}

/**
 * gwy_app_data_browser_create_channel:
 * @browser: A data browser.
 * @dfield: The data field to create data window for.
 *
 * Creates a data window for a data field when its visibility is switched on.
 *
 * Returns: The data view (NOT data window).
 **/
static GtkWidget*
gwy_app_data_browser_create_channel(G_GNUC_UNUSED GwyAppDataBrowser *browser,
                                    GwyAppDataProxy *proxy,
                                    GwyDataField *dfield)
{
    GtkWidget *data_view, *data_window;
    GwyContainer *data;
    GwyPixmapLayer *layer;
    GwyLayerBasic *layer_basic;
    GwyAppKeyType type;
    const gchar *strkey;
    GQuark quark;
    gchar key[40];
    guint len;
    gint i;

    data = GWY_CONTAINER(g_object_get_qdata(G_OBJECT(dfield), container_quark));
    quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(dfield),
                                                own_key_quark));
    strkey = g_quark_to_string(quark);
    gwy_debug("Making <%s> visible", strkey);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    g_return_val_if_fail(i >= 0 && type == KEY_IS_DATA, NULL);
    proxy->refcount++;

    layer = gwy_layer_basic_new();
    layer_basic = GWY_LAYER_BASIC(layer);
    gwy_pixmap_layer_set_data_key(layer, strkey);
    g_snprintf(key, sizeof(key), "/%d/show", i);
    gwy_layer_basic_set_presentation_key(layer_basic, key);
    g_snprintf(key, sizeof(key), "/%d/base", i);
    gwy_layer_basic_set_min_max_key(layer_basic, key);
    len = strlen(key);
    g_strlcat(key, "/range-type", sizeof(key));
    gwy_layer_basic_set_range_type_key(layer_basic, key);
    key[len] = '\0';
    g_strlcat(key, "/palette", sizeof(key));
    gwy_layer_basic_set_gradient_key(layer_basic, key);

    data_view = gwy_data_view_new(data);
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(data_view), layer);

    data_window = gwy_data_window_new(GWY_DATA_VIEW(data_view));

    gwy_app_data_proxy_update_visibility(G_OBJECT(dfield), TRUE);
    g_signal_connect_swapped(data_window, "focus-in-event",
                             G_CALLBACK(gwy_app_data_browser_select_data_view),
                             data_view);
    g_signal_connect(data_window, "delete-event",
                     G_CALLBACK(gwy_app_data_browser_channel_deleted), NULL);
    gwy_app_data_window_setup(GWY_DATA_WINDOW(data_window));
    gwy_app_add_main_accel_group(GTK_WINDOW(data_window));
    gtk_widget_show_all(data_window);
    /* This primarily adds the window to the list of visible windows */
    gwy_app_data_window_set_current(GWY_DATA_WINDOW(data_window));

    g_snprintf(key, sizeof(key), "/%d/mask", i);
    quark = g_quark_from_string(key);
    gwy_app_data_browser_sync_mask(data, quark, GWY_DATA_VIEW(data_view));

    return data_view;
}

static void
gwy_app_data_browser_channel_toggled(GtkCellRendererToggle *renderer,
                                     gchar *path_str,
                                     GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    GtkWidget *widget, *window;
    GObject *object;
    gboolean active;

    gwy_debug("Toggled data row %s", path_str);
    proxy = browser->current;
    g_return_if_fail(proxy);

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(proxy->channels.list);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter,
                       MODEL_WIDGET, &widget,
                       MODEL_OBJECT, &object,
                       -1);
    active = gtk_cell_renderer_toggle_get_active(renderer);
    g_assert(active == (widget != NULL));
    if (!active) {
        widget = gwy_app_data_browser_create_channel(browser, proxy,
                                                     GWY_DATA_FIELD(object));
        gtk_list_store_set(proxy->channels.list, &iter,
                           MODEL_WIDGET, widget,
                           -1);
    }
    else {
        gwy_app_data_proxy_update_visibility(object, FALSE);
        window = gtk_widget_get_toplevel(widget);
        gwy_app_data_window_remove(GWY_DATA_WINDOW(window));
        gtk_widget_destroy(window);
        gtk_list_store_set(proxy->channels.list, &iter,
                           MODEL_WIDGET, NULL,
                           -1);
        g_object_unref(widget);
        gwy_app_data_proxy_unref(proxy);
    }
    g_object_unref(object);
}

static GtkWidget*
gwy_app_data_browser_construct_channels(GwyAppDataBrowser *browser)
{
    GtkWidget *retval;
    GtkTreeView *treeview;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    /* Construct the GtkTreeView that will display data channels */
    retval = gtk_tree_view_new();
    treeview = GTK_TREE_VIEW(retval);

    /* Add the thumbnail column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Thumbnail", renderer,
                                                      NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the visibility column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer), "activatable", TRUE, NULL);
    g_signal_connect(renderer, "toggled",
                     G_CALLBACK(gwy_app_data_browser_channel_toggled), browser);
    column = gtk_tree_view_column_new_with_attributes("Visible", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_render_visible, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the title column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer),
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "ellipsize-set", TRUE,
                 NULL);
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_channel_render_title, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the flags column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "width-chars", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes("Flags", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_channel_render_flags, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    gtk_tree_view_set_headers_visible(treeview, FALSE);

    selection = gtk_tree_view_get_selection(treeview);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_data_browser_channel_selection_changed),
                     browser);

    return retval;
}

static void
gwy_app_data_browser_graph_render_title(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                        GtkCellRenderer *renderer,
                                        GtkTreeModel *model,
                                        GtkTreeIter *iter,
                                        G_GNUC_UNUSED gpointer userdata)
{
    GwyGraphModel *gmodel;

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &gmodel, -1);
    g_object_set(G_OBJECT(renderer), "text", gwy_graph_model_get_title(gmodel),
                 NULL);
    g_object_unref(gmodel);
}

static void
gwy_app_data_browser_graph_render_ncurves(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                          GtkCellRenderer *renderer,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          G_GNUC_UNUSED gpointer userdata)
{
    GwyGraphModel *gmodel;
    gchar s[8];

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &gmodel, -1);
    g_snprintf(s, sizeof(s), "%d", gwy_graph_model_get_n_curves(gmodel));
    g_object_set(G_OBJECT(renderer), "text", s, NULL);
    g_object_unref(gmodel);
}

static void
gwy_app_data_browser_graph_selection_changed(GtkTreeSelection *selection,
                                             GwyAppDataBrowser *browser)
{
    gboolean any;

    /* This can happen when data is manipulated programatically */
    if (browser->active_page != PAGE_GRAPHS)
        return;

    any = gtk_tree_selection_get_selected(selection, NULL, NULL);
    gwy_debug("Any graph: %d", any);
    gwy_sensitivity_group_set_state(browser->sensgroup,
                                    SENS_OBJECT, any ? SENS_OBJECT : 0);
}

/**
 * gwy_app_data_browser_graph_deleted:
 * @graph_window: A graph window that was deleted.
 *
 * Destroys a deleted graph window, updating proxy.
 *
 * This functions makes sure various updates happen in reasonable order,
 * simple gtk_widget_destroy() on the graph window would not do that.
 *
 * Returns: Always %TRUE to be usable as terminal event handler.
 **/
static gboolean
gwy_app_data_browser_graph_deleted(GwyGraphWindow *graph_window)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GwyAppKeyType type;
    GwyGraphModel *gmodel;
    GwyContainer *data;
    GtkWidget *graph;
    GtkTreeIter iter;
    const gchar *strkey;
    GQuark quark;
    gint i;

    gwy_debug("Graph window %p deleted", graph_window);
    graph = gwy_graph_window_get_graph(graph_window);
    gmodel = gwy_graph_get_model(GWY_GRAPH(graph));
    data = g_object_get_qdata(G_OBJECT(gmodel), container_quark);
    quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(gmodel),
                                                own_key_quark));
    g_return_val_if_fail(data && quark, TRUE);

    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    g_return_val_if_fail(i >= 0 && type == KEY_IS_GRAPH, TRUE);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    if (!gwy_app_data_proxy_find_object(proxy->graphs.list, i, &iter)) {
        g_critical("Cannot find graph model %p (%d)", gmodel, i);
        return TRUE;
    }

    gwy_app_data_proxy_unref(proxy);
    gtk_list_store_set(proxy->graphs.list, &iter, MODEL_WIDGET, NULL, -1);
    gwy_app_graph_window_remove(GTK_WIDGET(graph_window));
    gtk_widget_destroy(GTK_WIDGET(graph_window));

    return TRUE;
}

/**
 * gwy_app_data_browser_create_graph:
 * @browser: A data browser.
 * @gmodel: The graph model to create graph window for.
 *
 * Creates a graph window for a graph model when its visibility is switched on.
 *
 * Returns: The graph widget (NOT graph window).
 **/
static GtkWidget*
gwy_app_data_browser_create_graph(GwyAppDataBrowser *browser,
                                  GwyAppDataProxy *proxy,
                                  GwyGraphModel *gmodel)
{
    GtkWidget *graph, *graph_window;

    graph = gwy_graph_new(gmodel);
    graph_window = gwy_graph_window_new(GWY_GRAPH(graph));

    /* Graphs do not reference Container, fake it */
    g_object_ref(browser->current->container);
    g_object_weak_ref(G_OBJECT(graph_window),
                      (GWeakNotify)g_object_unref, browser->current->container);
    proxy->refcount++;

    gwy_app_data_proxy_update_visibility(G_OBJECT(gmodel), TRUE);
    g_signal_connect_swapped(graph_window, "focus-in-event",
                             G_CALLBACK(gwy_app_data_browser_select_graph),
                             graph);
    g_signal_connect(graph_window, "delete-event",
                     G_CALLBACK(gwy_app_data_browser_graph_deleted), NULL);
    gwy_app_add_main_accel_group(GTK_WINDOW(graph_window));
    gtk_window_set_default_size(GTK_WINDOW(graph_window), 480, 360);
    gtk_widget_show_all(graph_window);
    /* This primarily adds the window to the list of visible windows */
    gwy_app_graph_window_set_current(graph_window);

    return graph;
}

static void
gwy_app_data_browser_graph_toggled(GtkCellRendererToggle *renderer,
                                   gchar *path_str,
                                   GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    GtkWidget *widget, *window;
    GObject *object;
    gboolean active;

    gwy_debug("Toggled graph row %s", path_str);
    proxy = browser->current;
    g_return_if_fail(proxy);

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(proxy->graphs.list);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter,
                       MODEL_WIDGET, &widget,
                       MODEL_OBJECT, &object,
                       -1);
    active = gtk_cell_renderer_toggle_get_active(renderer);
    g_assert(active == (widget != NULL));
    if (!active) {
        widget = gwy_app_data_browser_create_graph(browser, proxy,
                                                   GWY_GRAPH_MODEL(object));
        gtk_list_store_set(proxy->graphs.list,
                           &iter, MODEL_WIDGET,
                           widget, -1);
    }
    else {
        gwy_app_data_proxy_update_visibility(object, FALSE);
        window = gtk_widget_get_toplevel(widget);
        gwy_app_graph_window_remove(window);
        gtk_widget_destroy(window);
        gtk_list_store_set(proxy->graphs.list, &iter,
                           MODEL_WIDGET, NULL,
                           -1);
        g_object_unref(widget);
        gwy_app_data_proxy_unref(proxy);
    }
    g_object_unref(object);
}

static GtkWidget*
gwy_app_data_browser_construct_graphs(GwyAppDataBrowser *browser)
{
    GtkTreeView *treeview;
    GtkWidget *retval;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    /* Construct the GtkTreeView that will display graphs */
    retval = gtk_tree_view_new();
    treeview = GTK_TREE_VIEW(retval);

    /* Add the thumbnail column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Thumbnail", renderer,
                                                      NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the visibility column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer), "activatable", TRUE, NULL);
    g_signal_connect(renderer, "toggled",
                     G_CALLBACK(gwy_app_data_browser_graph_toggled), browser);
    column = gtk_tree_view_column_new_with_attributes("Visible", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_render_visible, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the title column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer),
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "ellipsize-set", TRUE,
                 NULL);
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_graph_render_title, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the flags column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "width-chars", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes("Curves", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_graph_render_ncurves, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    gtk_tree_view_set_headers_visible(treeview, FALSE);

    selection = gtk_tree_view_get_selection(treeview);
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_data_browser_graph_selection_changed),
                     browser);

    return retval;
}

static void
gwy_app_data_browser_delete_object(GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeSelection *selection;
    GtkTreeView *treeview;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GObject *object;
    GtkWidget *widget, *window;
    GwyContainer *data;
    gchar key[32];
    gint i, page;

    g_return_if_fail(browser->current);
    proxy = browser->current;
    page = browser->active_page;

    switch (page) {
        case PAGE_CHANNELS:
        treeview = GTK_TREE_VIEW(browser->channels);
        break;

        case PAGE_GRAPHS:
        treeview = GTK_TREE_VIEW(browser->graphs);
        break;

        default:
        g_return_if_reached();
        break;
    }

    selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        g_warning("Nothing is selected");
        return;
    }

    data = proxy->container;
    gtk_tree_model_get(model, &iter,
                       MODEL_ID, &i,
                       MODEL_OBJECT, &object,
                       MODEL_WIDGET, &widget,
                       -1);
    /* XXX */
    if (page == PAGE_CHANNELS && i == 0) {
        g_warning("Cowardly refusing to delete \"/0/data\" as it is likely to "
                  "lead to an instant crash in active tool.");
        return;
    }

    /* Get rid of widget displaying this object.  This may invoke complete
     * destruction later in idle handler. */
    if (widget) {
        window = gtk_widget_get_toplevel(widget);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                           MODEL_WIDGET, NULL, -1);
        switch (page) {
            case PAGE_CHANNELS:
            gwy_app_data_window_remove(GWY_DATA_WINDOW(window));
            break;

            case PAGE_GRAPHS:
            gwy_app_graph_window_remove(window);
            break;
        }
        gwy_app_data_proxy_unref(proxy);
        g_object_unref(widget);
        gtk_widget_destroy(window);
    }

    /* Remove object from container, this causes of removal from tree model
     * too */
    switch (page) {
        case PAGE_CHANNELS:
        /* XXX: Cannot just remove /0, because all graphs are under
         * GRAPH_PREFIX == "/0/graph/graph" */
        /* XXX: This is too crude and makes 3D views crash. Must integrate
         * them somehow. */
        g_snprintf(key, sizeof(key), "/%d/data", i);
        gwy_container_remove_by_prefix(data, key);
        g_snprintf(key, sizeof(key), "/%d/base", i);
        gwy_container_remove_by_prefix(data, key);
        g_snprintf(key, sizeof(key), "/%d/mask", i);
        gwy_container_remove_by_prefix(data, key);
        g_snprintf(key, sizeof(key), "/%d/show", i);
        gwy_container_remove_by_prefix(data, key);
        g_snprintf(key, sizeof(key), "/%d/select", i);
        gwy_container_remove_by_prefix(data, key);
        break;

        case PAGE_GRAPHS:
        g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, i);
        gwy_container_remove_by_prefix(data, key);
        break;
    }
    g_object_unref(object);
}

static void
gwy_app_data_browser_close_file(GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkWidget *widget, *window;

    proxy = browser->current;
    g_return_if_fail(proxy);

    model = GTK_TREE_MODEL(proxy->channels.list);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gtk_tree_model_get(model, &iter, MODEL_WIDGET, &widget, -1);
            if (widget) {
                window = gtk_widget_get_toplevel(widget);
                gwy_app_data_browser_channel_deleted(GWY_DATA_WINDOW(window));
                g_object_unref(widget);
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    model = GTK_TREE_MODEL(proxy->graphs.list);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gtk_tree_model_get(model, &iter, MODEL_WIDGET, &widget, -1);
            if (widget) {
                window = gtk_widget_get_toplevel(widget);
                gwy_app_data_browser_graph_deleted(GWY_GRAPH_WINDOW(window));
                g_object_unref(widget);
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    g_return_if_fail(!proxy->refcount);
    gwy_app_data_proxy_deserted(proxy);
}

static void
gwy_app_data_browser_page_changed(GwyAppDataBrowser *browser,
                                  G_GNUC_UNUSED GtkNotebookPage *useless_crap,
                                  gint pageno)
{
    GtkTreeSelection *selection;

    gwy_debug("Page changed to: %d", pageno);

    browser->active_page = pageno;
    switch (pageno) {
        case PAGE_CHANNELS:
        selection
            = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->channels));
        gwy_app_data_browser_channel_selection_changed(selection, browser);
        break;

        case PAGE_GRAPHS:
        selection
            = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->graphs));
        gwy_app_data_browser_graph_selection_changed(selection, browser);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static gboolean
gwy_app_data_browser_deleted(GwyAppDataBrowser *browser)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(GTK_WINDOW(browser->window),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_WARNING,
                                    GTK_BUTTONS_OK,
                                    "Write 100 times: I will never try to "
                                    "close Data Browser again.");
    gtk_window_set_title(GTK_WINDOW(dialog), "Punishment");
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(dialog);

    return TRUE;
}

/**
 * gwy_app_get_data_browser:
 *
 * Gets the application data browser.
 *
 * When it does not exist yet, it is created as a side effect.
 *
 * Returns: The data browser.
 **/
static GwyAppDataBrowser*
gwy_app_get_data_browser(void)
{
    GtkWidget *label, *box_page, *scwin, *vbox, *hbox, *button;
    GwyAppDataBrowser *browser;

    if (gwy_app_data_browser)
        return gwy_app_data_browser;

    own_key_quark
        = g_quark_from_static_string("gwy-app-data-browser-own-key");
    container_quark
        = g_quark_from_static_string("gwy-app-data-browser-container");

    /* Window setup */
    browser = g_new0(GwyAppDataBrowser, 1);
    gwy_app_data_browser = browser;
    browser->sensgroup = gwy_sensitivity_group_new();
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_default_size(GTK_WINDOW(browser->window), 300, 300);
    gtk_window_set_title(GTK_WINDOW(browser->window), _("Data Browser"));
    gwy_app_add_main_accel_group(GTK_WINDOW(browser->window));

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(browser->window), vbox);

    browser->filename = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(browser->filename),
                            PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment(GTK_MISC(browser->filename), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(browser->filename), 4, 2);
    gtk_box_pack_start(GTK_BOX(vbox), browser->filename, FALSE, FALSE, 0);

    /* Create the notebook */
    browser->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), browser->notebook, TRUE, TRUE, 0);

    /* Create Data Channels tab */
    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new(_("Data Channels"));
    gtk_notebook_append_page(GTK_NOTEBOOK(browser->notebook), box_page, label);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box_page), scwin, TRUE, TRUE, 0);

    browser->channels = gwy_app_data_browser_construct_channels(browser);
    gtk_container_add(GTK_CONTAINER(scwin), browser->channels);

    /* Create Graphs tab */
    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new(_("Graphs"));
    gtk_notebook_append_page(GTK_NOTEBOOK(browser->notebook), box_page, label);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box_page), scwin, TRUE, TRUE, 0);

    browser->graphs = gwy_app_data_browser_construct_graphs(browser);
    gtk_container_add(GTK_CONTAINER(scwin), browser->graphs);

    /* Create the bottom toolbar */
    hbox = gtk_hbox_new(TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gwy_tool_like_button_new(_("_Delete"), GTK_STOCK_DELETE);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gwy_sensitivity_group_add_widget(browser->sensgroup, button, SENS_OBJECT);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_data_browser_delete_object),
                             browser);

    button = gwy_tool_like_button_new(_("_Close File"), GTK_STOCK_CLOSE);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gwy_sensitivity_group_add_widget(browser->sensgroup, button, SENS_FILE);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_data_browser_close_file),
                             browser);

    g_signal_connect_swapped(browser->notebook, "switch-page",
                             G_CALLBACK(gwy_app_data_browser_page_changed),
                             browser);
    g_signal_connect_swapped(browser->window, "delete-event",
                             G_CALLBACK(gwy_app_data_browser_deleted), browser);
    g_object_unref(browser->sensgroup);

    gwy_app_restore_window_position(GTK_WINDOW(browser->window),
                                    "/app/data-browser", FALSE);
    gtk_widget_show_all(browser->window);
    gtk_window_present(GTK_WINDOW(browser->window));
    gwy_app_restore_window_position(GTK_WINDOW(browser->window),
                                    "/app/data-browser", FALSE);

    return browser;
}

static void
gwy_app_data_browser_restore_active(GtkTreeView *treeview,
                                    GwyAppDataList *list)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(list->list));
    if (!gwy_app_data_proxy_find_object(list->list, list->active, &iter))
        return;

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_select_iter(selection, &iter);
}

static void
gwy_app_data_browser_switch_data(GwyContainer *data)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    const guchar *filename;

    browser = gwy_app_get_data_browser();
    if (!data) {
        gtk_tree_view_set_model(GTK_TREE_VIEW(browser->channels), NULL);
        gtk_tree_view_set_model(GTK_TREE_VIEW(browser->graphs), NULL);
        gtk_label_set_text(GTK_LABEL(browser->filename), "");
        gwy_sensitivity_group_set_state(browser->sensgroup,
                                        SENS_FILE | SENS_OBJECT, 0);
        return;
    }
    if (browser->current && browser->current->container == data)
        return;

    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);
    browser->current = proxy;
    gwy_app_data_browser_restore_active(GTK_TREE_VIEW(browser->channels),
                                        &proxy->channels);
    gwy_app_data_browser_restore_active(GTK_TREE_VIEW(browser->graphs),
                                        &proxy->graphs);

    if (gwy_container_gis_string_by_name(data, "/filename", &filename)) {
        gchar *s;

        s = g_path_get_basename(filename);
        gtk_label_set_text(GTK_LABEL(browser->filename), s);
        g_free(s);
    }
    gwy_sensitivity_group_set_state(browser->sensgroup, SENS_FILE, SENS_FILE);
}

/**
 * gwy_app_data_browser_select_data_view:
 * @data_view: A data view widget.
 *
 * Switches application data browser to display container of @data_view's data
 * and selects @data_view's data in the channel list.
 **/
void
gwy_app_data_browser_select_data_view(GwyDataView *data_view)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GtkTreeSelection *selection;
    GtkWidget *data_window;
    GwyPixmapLayer *layer;
    GtkTreeIter iter;
    GwyContainer *data;
    const gchar *strkey;
    GwyAppKeyType type;
    gint i;

    data = gwy_data_view_get_data(data_view);
    gwy_app_data_browser_switch_data(data);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);

    layer = gwy_data_view_get_base_layer(data_view);
    strkey = gwy_pixmap_layer_get_data_key(layer);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    g_return_if_fail(i >= 0 && type == KEY_IS_DATA);
    proxy->channels.active = i;
    gwy_app_data_proxy_find_object(proxy->channels.list, i, &iter);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->channels));
    gtk_tree_selection_select_iter(selection, &iter);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->notebook),
                                  PAGE_CHANNELS);

    /* FIXME: This updated the other notion of current data */
    data_window = gtk_widget_get_toplevel(GTK_WIDGET(data_view));
    if (data_window != (GtkWidget*)gwy_app_data_window_get_current())
        gwy_app_data_window_set_current(GWY_DATA_WINDOW(data_window));
}

/**
 * gwy_app_data_browser_select_graph:
 * @graph: A graph widget.
 *
 * Switches application data browser to display container of @graph's data
 * and selects @graph's data in the graph list.
 **/
void
gwy_app_data_browser_select_graph(GwyGraph *graph)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GwyGraphModel *gmodel;
    GtkTreeSelection *selection;
    GtkWidget *graph_window;
    GtkTreeIter iter;
    GwyContainer *data;
    const gchar *strkey;
    GwyAppKeyType type;
    GQuark quark;
    gint i;

    gmodel = gwy_graph_get_model(graph);
    data = g_object_get_qdata(G_OBJECT(gmodel), container_quark);
    g_return_if_fail(data);
    gwy_app_data_browser_switch_data(data);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);

    quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(gmodel),
                                                own_key_quark));
    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    g_return_if_fail(i >= 0 && type == KEY_IS_GRAPH);
    proxy->graphs.active = i;
    gwy_app_data_proxy_find_object(proxy->graphs.list, i, &iter);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->graphs));
    gtk_tree_selection_select_iter(selection, &iter);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->notebook),
                                  PAGE_GRAPHS);

    /* FIXME: This updated the other notion of current graph */
    graph_window = gtk_widget_get_toplevel(GTK_WIDGET(graph));
    if (graph_window != gwy_app_graph_window_get_current())
        gwy_app_graph_window_set_current(graph_window);
}

static gboolean
gwy_app_data_browser_reconstruct_visibility(GwyAppDataProxy *proxy)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkWidget *widget;
    GwyDataField *dfield;
    GwyGraphModel *gmodel;
    const gchar *strkey;
    GQuark quark;
    gboolean visible;
    gchar key[48];
    gint count = 0;

    /* Data channels */
    model = GTK_TREE_MODEL(proxy->channels.list);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            visible = FALSE;
            gtk_tree_model_get(model, &iter, MODEL_OBJECT, &dfield, -1);
            quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(dfield),
                                                        own_key_quark));
            strkey = g_quark_to_string(quark);
            g_snprintf(key, sizeof(key), "%s/visible", strkey);
            gwy_container_gis_boolean_by_name(proxy->container, key, &visible);
            if (visible) {
                widget = gwy_app_data_browser_create_channel(proxy->parent,
                                                             proxy, dfield);
                gtk_list_store_set(proxy->channels.list, &iter,
                                   MODEL_WIDGET, widget,
                                   -1);
                count++;
            }
            g_object_unref(dfield);
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    /* Graphs */
    model = GTK_TREE_MODEL(proxy->graphs.list);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            visible = FALSE;
            gtk_tree_model_get(model, &iter, MODEL_OBJECT, &gmodel, -1);
            quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(gmodel),
                                                        own_key_quark));
            strkey = g_quark_to_string(quark);
            g_snprintf(key, sizeof(key), "%s/visible", strkey);
            gwy_container_gis_boolean_by_name(proxy->container, key, &visible);
            if (visible) {
                widget = gwy_app_data_browser_create_graph(proxy->parent,
                                                           proxy, gmodel);
                gtk_list_store_set(proxy->graphs.list, &iter,
                                   MODEL_WIDGET, widget,
                                   -1);
                count++;
            }
            g_object_unref(gmodel);
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    return count > 0;
}

/**
 * gwy_app_data_browser_add:
 * @data: A data container.
 *
 * Adds a data container to application data browser.
 *
 * The container is then managed by the data browser until it's destroyed.
 * Since the data browser does not add any reference, the container is normally
 * destroyed when there is no view (any: channel, graph) showing its
 * contents.  Make sure such a view exists before you release your reference
 * to @data.
 **/
void
gwy_app_data_browser_add(GwyContainer *data)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GtkWidget *widget;
    GtkTreeIter iter;
    GwyDataField *dfield;

    g_return_if_fail(GWY_IS_CONTAINER(data));

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, TRUE);

    /* Show first window
     * XXX: crude */
    if (!gwy_app_data_browser_reconstruct_visibility(proxy)) {
        if (gwy_app_data_proxy_find_object(proxy->channels.list, 0, &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(proxy->channels.list), &iter,
                               MODEL_OBJECT, &dfield,
                               -1);
            widget = gwy_app_data_browser_create_channel(browser, proxy,
                                                         dfield);
            gtk_list_store_set(proxy->channels.list, &iter,
                               MODEL_WIDGET, widget,
                               -1);
            g_object_unref(dfield);
        }
        else {
            g_warning("There are no data channels in container.  "
                      "It will be likely finalized right away.");
        }
    }
    gwy_sensitivity_group_set_state(browser->sensgroup, SENS_FILE, SENS_FILE);
}

/**
 * gwy_app_data_browser_add_graph_model:
 * @gmodel: A graph model to add.
 * @data: A data container to add @gmodel to.
 *        It can be %NULL to add the graph model to current data container.
 * @showit: %TRUE to display it immediately, %FALSE to just add it.
 *
 * Adds a graph model to a data container.
 *
 * Returns: The id of the graph model in the container.
 **/
gint
gwy_app_data_browser_add_graph_model(GwyGraphModel *gmodel,
                                     GwyContainer *data,
                                     gboolean showit)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    gchar key[32];

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), -1);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), -1);

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    g_return_val_if_fail(proxy, -1);

    g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, proxy->graphs.last + 1);
    /* This invokes "item-changed" callback that will finish the work.
     * Among other things, it will update proxy->graphs.last. */
    gwy_container_set_object_by_name(proxy->container, key, gmodel);

    if (showit) {
        GtkWidget *widget;
        GtkTreeIter iter;

        gwy_app_data_proxy_find_object(proxy->graphs.list,
                                       proxy->graphs.last,
                                       &iter);
        widget = gwy_app_data_browser_create_graph(browser, proxy, gmodel);
        gtk_list_store_set(proxy->graphs.list, &iter,
                           MODEL_WIDGET, widget,
                           -1);
    }

    return proxy->graphs.last;
}

/**
 * gwy_app_data_browser_add_data_field:
 * @dfield: A data field to add.
 * @data: A data container to add @dfield to.
 *        It can be %NULL to add the data field to current data container.
 * @showit: %TRUE to display it immediately, %FALSE to just add it.
 *
 * Adds a data field to a data container.
 *
 * Returns: The id of the data field in the container.
 **/
gint
gwy_app_data_browser_add_data_field(GwyDataField *dfield,
                                    GwyContainer *data,
                                    gboolean showit)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    gchar key[24];

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), -1);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), -1);

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    g_return_val_if_fail(proxy, -1);

    g_snprintf(key, sizeof(key), "/%d/data", proxy->channels.last + 1);
    /* This invokes "item-changed" callback that will finish the work.
     * Among other things, it will update proxy->channels.last. */
    gwy_container_set_object_by_name(proxy->container, key, dfield);

    if (showit) {
        GtkWidget *widget;
        GtkTreeIter iter;

        gwy_app_data_proxy_find_object(proxy->channels.list,
                                       proxy->channels.last,
                                       &iter);
        widget = gwy_app_data_browser_create_channel(browser, proxy, dfield);
        gtk_list_store_set(proxy->channels.list, &iter,
                           MODEL_WIDGET, widget,
                           -1);
    }

    return proxy->channels.last;
}

#if 0
static GQuark
gwy_app_data_browser_mangle_key(GQuark quark,
                                const gchar *suffix)
{
    const gchar *strkey, *p;
    gchar *newkey;
    guint len;

    strkey = g_quark_to_string(quark);
    g_return_val_if_fail(strkey[0] == GWY_CONTAINER_PATHSEP, 0);
    /* Premature optimization is the root of all evil */
    len = strlen(strkey);
    for (p = strkey + len-1; *p != GWY_CONTAINER_PATHSEP; p--)
        ;
    len = strlen(suffix);
    newkey = g_newa(gchar, p-strkey + 2 + len);
    memcpy(newkey, strkey, p-strkey + 1);
    memcpy(newkey + (p-strkey + 1), suffix, len+1);

    return g_quark_from_string(newkey);
}
#endif

/**
 * gwy_app_get_mask_key_for_id:
 * @id: Data number in container.
 *
 * Calculates mask quark identifier from its id.
 *
 * Returns: The quark key identifying mask number @id.
 **/
GQuark
gwy_app_get_mask_key_for_id(gint id)
{
    static GQuark quarks[12] = { 0, };
    gchar key[24];

    g_return_val_if_fail(id >= 0, 0);
    if (id < G_N_ELEMENTS(quarks) && quarks[id])
        return quarks[id];

    g_snprintf(key, sizeof(key), "/%d/mask", id);

    if (id < G_N_ELEMENTS(quarks)) {
        quarks[id] = g_quark_from_string(key);
        return quarks[id];
    }
    return g_quark_from_string(key);
}

/**
 * gwy_app_get_presentation_key_for_id:
 * @id: Data number in container.
 *
 * Calculates presentation quark identifier from its id.
 *
 * Returns: The quark key identifying presentation number @id.
 **/
GQuark
gwy_app_get_presentation_key_for_id(gint id)
{
    static GQuark quarks[12] = { 0, };
    gchar key[24];

    g_return_val_if_fail(id >= 0, 0);
    if (id < G_N_ELEMENTS(quarks) && quarks[id])
        return quarks[id];

    g_snprintf(key, sizeof(key), "/%d/show", id);

    if (id < G_N_ELEMENTS(quarks)) {
        quarks[id] = g_quark_from_string(key);
        return quarks[id];
    }
    return g_quark_from_string(key);
}

/**
 * gwy_app_set_data_field_title:
 * @data: A data container.
 * @id: The data channel id.
 * @name: The title to set.  It can be %NULL to use somthing like "Untitled".
 *        The id will be appended to it or (replaced in it if it already ends
 *        with digits).
 *
 * Sets channel title.
 **/
void
gwy_app_set_data_field_title(GwyContainer *data,
                             gint id,
                             const gchar *name)
{
    gchar key[32], *title;
    const gchar *p;

    if (!name) {
        name = _("Untitled");
        p = name + strlen(name);
    }
    else {
        p = name + strlen(name);
        while (p > name && g_ascii_isdigit(*p))
            p--;
        if (!g_ascii_isspace(*p))
            p = name + strlen(name);
    }
    title = g_strdup_printf("%.*s %d", (gint)(p - name), name, id);
    g_snprintf(key, sizeof(key), "/%i/data/title", id);
    gwy_container_set_string_by_name(data, key, title);
}

/**
 * gwy_app_data_browser_get_current:
 * @what: First information about current objects to obtain.
 * @...: pointer to store the information to (object pointer for objects,
 *       #GQuark pointer for keys, #gint pointer for id's), followed by
 *       0-terminated list of #GwyAppWhat, pointer couples.
 *
 * Gets information about current objects.
 *
 * All output arguments are always set to some value, even if the requested
 * object does not exist.  Object arguments are set to pointer to the object if
 * it exists (no reference added), or cleared to %NULL if no such object
 * exists.
 *
 * Quark arguments are set to the corresponding key even if no such object is
 * actually present (use object arguments to check for object presence) but the
 * location where it would be stored is known.  This is commond with
 * presentations and masks.  They are be set to 0 if no corresponding location
 * exists -- for example, when current mask key is requested but the current
 * data contain no data field (or there is no current data at all).
 *
 * The rules for id arguments are similar to quarks, except they are set to -1
 * to indicate undefined result.
 **/
void
gwy_app_data_browser_get_current(GwyAppWhat what,
                                 ...)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *current = NULL;
    GwyAppDataList *channels = NULL, *graphs = NULL;
    GtkTreeIter iter;
    GObject *object, **otarget;
    GObject *dfield = NULL, *gmodel = NULL;  /* Cache current */
    GwyDataWindow *dw;
    GwyGraphWindow *gw;
    GQuark quark, *qtarget;
    gint *itarget;
    va_list ap;

    if (!what)
        return;

    va_start(ap, what);
    browser = gwy_app_data_browser;
    if (browser) {
        current = browser->current;
        channels = &current->channels;
        graphs = &current->graphs;
    }

    do {
        switch (what) {
            case GWY_APP_CONTAINER:
            otarget = va_arg(ap, GObject**);
            *otarget = current ? G_OBJECT(current->container) : NULL;
            break;

            case GWY_APP_DATA_VIEW:
            otarget = va_arg(ap, GObject**);
            /* XXX: This can be a data view NOT showing current container */
            dw = gwy_app_data_window_get_current();
            *otarget = dw ? G_OBJECT(gwy_data_window_get_data_view(dw)) : NULL;
            break;

            case GWY_APP_GRAPH:
            otarget = va_arg(ap, GObject**);
            /* XXX: This can be a graph NOT showing current container */
            gw = GWY_GRAPH_WINDOW(gwy_app_graph_window_get_current());
            *otarget = gw ? G_OBJECT(gwy_graph_window_get_graph(gw)) : NULL;
            break;

            case GWY_APP_DATA_FIELD:
            case GWY_APP_DATA_FIELD_KEY:
            case GWY_APP_DATA_FIELD_ID:
            case GWY_APP_MASK_FIELD:
            case GWY_APP_MASK_FIELD_KEY:
            case GWY_APP_SHOW_FIELD:
            case GWY_APP_SHOW_FIELD_KEY:
            if (!dfield
                && current
                && gwy_app_data_proxy_find_object(channels->list,
                                                  channels->active, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(channels->list), &iter,
                                   MODEL_OBJECT, &object, -1);
                dfield = object;
                g_object_unref(object);
            }
            else
                quark = 0;
            switch (what) {
                case GWY_APP_DATA_FIELD:
                otarget = va_arg(ap, GObject**);
                *otarget = dfield;
                break;

                case GWY_APP_DATA_FIELD_KEY:
                qtarget = va_arg(ap, GQuark*);
                *qtarget = 0;
                if (dfield)
                    *qtarget = GPOINTER_TO_UINT(g_object_get_qdata
                                                      (dfield, own_key_quark));
                break;

                case GWY_APP_DATA_FIELD_ID:
                itarget = va_arg(ap, gint*);
                *itarget = dfield ? channels->active : -1;
                break;

                case GWY_APP_MASK_FIELD:
                otarget = va_arg(ap, GObject**);
                *otarget = NULL;
                if (dfield) {
                    quark = gwy_app_get_mask_key_for_id(channels->active);
                    gwy_container_gis_object(current->container, quark,
                                             otarget);
                }
                break;

                case GWY_APP_MASK_FIELD_KEY:
                qtarget = va_arg(ap, GQuark*);
                *qtarget = 0;
                if (dfield)
                    *qtarget = gwy_app_get_mask_key_for_id(channels->active);
                break;

                case GWY_APP_SHOW_FIELD:
                otarget = va_arg(ap, GObject**);
                *otarget = NULL;
                if (dfield) {
                    quark = gwy_app_get_presentation_key_for_id(channels->active);
                    gwy_container_gis_object(current->container, quark,
                                             otarget);
                }
                break;

                case GWY_APP_SHOW_FIELD_KEY:
                qtarget = va_arg(ap, GQuark*);
                *qtarget = 0;
                if (dfield)
                    *qtarget = gwy_app_get_presentation_key_for_id(channels->active);
                break;

                default:
                /* Hi, gcc */
                break;
            }
            break;

            case GWY_APP_GRAPH_MODEL:
            case GWY_APP_GRAPH_MODEL_KEY:
            case GWY_APP_GRAPH_MODEL_ID:
            if (!gmodel
                && current
                && gwy_app_data_proxy_find_object(graphs->list,
                                                  graphs->active, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(graphs->list), &iter,
                                   MODEL_OBJECT, &object, -1);
                gmodel = object;
                g_object_unref(object);
            }
            switch (what) {
                case GWY_APP_GRAPH_MODEL:
                otarget = va_arg(ap, GObject**);
                *otarget = gmodel;
                break;

                case GWY_APP_GRAPH_MODEL_KEY:
                qtarget = va_arg(ap, GQuark*);
                *qtarget = 0;
                if (gmodel)
                    *qtarget = GPOINTER_TO_UINT(g_object_get_qdata
                                                       (gmodel, own_key_quark));
                break;

                case GWY_APP_GRAPH_MODEL_ID:
                itarget = va_arg(ap, gint*);
                *itarget = gmodel ? graphs->active : -1;
                break;

                default:
                /* Hi, gcc */
                break;
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while ((what = va_arg(ap, GwyAppWhat)));

    va_end(ap);
}

/**
 * gwy_app_copy_data_items:
 * @source: Source container.
 * @dest: Target container (may be identical to source).
 * @from_id: Data number to copy items from.
 * @to_id: Data number to copy items to.
 * @...: 0-terminated list of #GwyDataItem values defining the items to copy.
 *
 * Copy auxiliary data items between data containers.
 **/
void
gwy_app_copy_data_items(GwyContainer *source,
                        GwyContainer *dest,
                        gint from_id,
                        gint to_id,
                        ...)
{
    GwyDataItem what;
    gchar key[40];
    const guchar *name;
    GwyRGBA rgba;
    guint enumval;
    gdouble dbl;
    va_list ap;

    g_return_if_fail(GWY_IS_CONTAINER(source));
    g_return_if_fail(GWY_IS_CONTAINER(dest));
    g_return_if_fail(from_id >= 0 && to_id >= 0);
    if (source == dest && from_id == to_id)
        return;

    va_start(ap, to_id);
    while ((what = va_arg(ap, GwyDataItem))) {
        switch (what) {
            case GWY_DATA_ITEM_GRADIENT:
            g_snprintf(key, sizeof(key), "/%d/base/palette", from_id);
            if (gwy_container_gis_string_by_name(source, key, &name)) {
                g_snprintf(key, sizeof(key), "/%d/base/palette", to_id);
                gwy_container_set_string_by_name(dest, key, g_strdup(name));
            }
            break;

            case GWY_DATA_ITEM_MASK_COLOR:
            g_snprintf(key, sizeof(key), "/%d/mask", from_id);
            if (gwy_rgba_get_from_container(&rgba, source, key)) {
                g_snprintf(key, sizeof(key), "/%d/mask", to_id);
                gwy_rgba_store_to_container(&rgba, dest, key);
            }
            break;

            case GWY_DATA_ITEM_RANGE:
            g_snprintf(key, sizeof(key), "/%d/base/min", from_id);
            if (gwy_container_gis_double_by_name(source, key, &dbl)) {
                g_snprintf(key, sizeof(key), "/%d/base/min", to_id);
                gwy_container_set_double_by_name(dest, key, dbl);
            }
            g_snprintf(key, sizeof(key), "/%d/base/max", from_id);
            if (gwy_container_gis_double_by_name(source, key, &dbl)) {
                g_snprintf(key, sizeof(key), "/%d/base/max", to_id);
                gwy_container_set_double_by_name(dest, key, dbl);
            }
            case GWY_DATA_ITEM_RANGE_TYPE:
            g_snprintf(key, sizeof(key), "/%d/base/range-type", from_id);
            if (gwy_container_gis_enum_by_name(source, key, &enumval)) {
                g_snprintf(key, sizeof(key), "/%d/base/range-type", to_id);
                gwy_container_set_enum_by_name(dest, key, enumval);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
    va_end(ap);
}

void
gwy_app_data_browser_shut_down(void)
{
    GwyAppDataBrowser *browser;

    browser = gwy_app_data_browser;
    if (!browser)
        return;

    gwy_app_save_window_position(GTK_WINDOW(browser->window),
                                 "/app/data-browser", TRUE, TRUE);
    /* This clean-up is only to make sure we've got the references right.
     * Remove in production version. */
    while (browser->proxy_list) {
        browser->current = (GwyAppDataProxy*)browser->proxy_list->data;
        gwy_app_data_browser_close_file(browser);
    }
    gtk_tree_view_set_model(GTK_TREE_VIEW(browser->channels), NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(browser->graphs), NULL);
}

/************************** Documentation ****************************/

/**
 * SECTION:data-browser
 * @title: data-browser
 * @short_description: Data browser
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
