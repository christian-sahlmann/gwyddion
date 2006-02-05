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
    SENS_MASK   = 0x01
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
    GList *container_list;
    struct _GwyAppDataProxy *current;
    gint active_page;
    GwySensitivityGroup *sensgroup;
    GtkWidget *window;
    GtkWidget *channels;
    GtkWidget *graphs;
};

/* The proxy associated with each Container (this is non-GUI object) */
struct _GwyAppDataProxy {
    struct _GwyAppDataBrowser *parent;
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

static gboolean
gwy_app_data_proxy_delayed_unref(gpointer userdata)
{
    gwy_object_unref(userdata);
    return FALSE;
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
 * gwy_app_data_proxy_container_finalized:
 * @userdata: Item from @container_list corresponding to finalized data.
 * @deceased_data: A #GwyContainer pointer (the object may not longer exits).
 *
 * Destroys data proxy for a container.
 **/
static void
gwy_app_data_proxy_container_finalized(gpointer userdata,
                                       GObject *deceased_data)
{
    GList *item = (GList*)userdata;
    GwyAppDataProxy *proxy = (GwyAppDataProxy*)item->data;
    GwyAppDataBrowser *browser = proxy->parent;

    /* FIXME: this is crude */
    if (browser->current == proxy)
        gwy_app_data_browser_switch_data(NULL);

    gwy_debug("Freeing proxy for Container %p", deceased_data);
    g_assert(proxy->container == (GwyContainer*)deceased_data);
    g_assert(browser);
    browser->container_list = g_list_delete_link(browser->container_list, item);

    gwy_app_data_proxy_finalize_list(GTK_TREE_MODEL(proxy->channels.list),
                                     MODEL_OBJECT,
                                     &gwy_app_data_proxy_channel_changed,
                                     proxy);
    gwy_app_data_proxy_finalize_list(GTK_TREE_MODEL(proxy->graphs.list),
                                     MODEL_OBJECT,
                                     &gwy_app_data_proxy_graph_changed,
                                     proxy);
    g_free(proxy);
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
    proxy = g_new0(GwyAppDataProxy, 1);
    proxy->container = data;
    proxy->parent = browser;
    browser->container_list = g_list_prepend(browser->container_list, proxy);
    g_object_weak_ref(G_OBJECT(data),
                      gwy_app_data_proxy_container_finalized,
                      browser->container_list);
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

    item = g_list_find_custom(browser->container_list, data,
                              &gwy_app_data_proxy_compare_data);
    if (!item) {
        if (do_create)
            return gwy_app_data_proxy_new(browser, data);
        else
            return NULL;
    }

    /* Move container to head */
    if (item != browser->container_list) {
        browser->container_list = g_list_remove_link(browser->container_list,
                                                     item);
        browser->container_list = g_list_concat(item, browser->container_list);
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
    gwy_debug("has_dfield: %d, has_layer: %d\n", has_dfield, has_layer);

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
        widget = gwy_app_data_browser_create_channel(browser,
                                                     GWY_DATA_FIELD(object));
        gtk_list_store_set(proxy->channels.list, &iter,
                           MODEL_WIDGET, widget,
                           -1);
    }
    else {
        gwy_app_data_proxy_update_visibility(object, FALSE);
        g_object_ref(proxy->container);
        window = gtk_widget_get_toplevel(widget);
        gwy_app_data_window_remove(GWY_DATA_WINDOW(window));
        gtk_widget_destroy(window);
        gtk_list_store_set(proxy->channels.list, &iter,
                           MODEL_WIDGET, NULL,
                           -1);
        g_object_unref(widget);

        /* Delay proxy destruction, the tree model does not like being
         * destroyed inside "toggled" callback */
        g_idle_add(&gwy_app_data_proxy_delayed_unref, proxy->container);
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
gwy_app_data_browser_graph_deleted(GtkWidget *graph_window)
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
    graph = gwy_graph_window_get_graph(GWY_GRAPH_WINDOW(graph_window));
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

    gtk_list_store_set(proxy->graphs.list, &iter, MODEL_WIDGET, NULL, -1);
    gwy_app_graph_window_remove(graph_window);
    gtk_widget_destroy(graph_window);

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
                                  GwyGraphModel *gmodel)
{
    GtkWidget *graph, *graph_window;

    graph = gwy_graph_new(gmodel);
    graph_window = gwy_graph_window_new(GWY_GRAPH(graph));

    /* Graphs do not reference Container, fake it */
    g_object_ref(browser->current->container);
    g_object_weak_ref(G_OBJECT(graph_window),
                      (GWeakNotify)g_object_unref, browser->current->container);

    gwy_app_data_proxy_update_visibility(G_OBJECT(gmodel), TRUE);
    g_signal_connect_swapped(graph_window, "focus-in-event",
                             G_CALLBACK(gwy_app_data_browser_select_graph),
                             graph);
    g_signal_connect(graph_window, "delete-event",
                     G_CALLBACK(gwy_app_data_browser_graph_deleted), NULL);
    gwy_app_add_main_accel_group(GTK_WINDOW(graph_window));
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
        widget = gwy_app_data_browser_create_graph(browser,
                                                   GWY_GRAPH_MODEL(object));
        gtk_list_store_set(proxy->graphs.list,
                           &iter, MODEL_WIDGET,
                           widget, -1);
    }
    else {
        gwy_app_data_proxy_update_visibility(object, FALSE);
        g_object_ref(proxy->container);
        window = gtk_widget_get_toplevel(widget);
        gwy_app_graph_window_remove(window);
        gtk_widget_destroy(window);
        gtk_list_store_set(proxy->graphs.list, &iter,
                           MODEL_WIDGET, NULL,
                           -1);
        g_object_unref(widget);

        /* Delay proxy destruction, the tree model does not like being
         * destroyed inside "toggled" callback */
        g_idle_add(&gwy_app_data_proxy_delayed_unref, proxy->container);
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
    gwy_debug("Implement me!");
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
    GtkWidget *notebook, *label, *box_page, *scwin, *vbox, *hbox, *button;
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

    /* Create the notebook */
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

    /* Create Data Channels tab */
    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new(_("Data Channels"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box_page), scwin, TRUE, TRUE, 0);

    browser->channels = gwy_app_data_browser_construct_channels(browser);
    gtk_container_add(GTK_CONTAINER(scwin), browser->channels);

    /* Create Graphs tab */
    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new(_("Graphs"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

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

    g_signal_connect_swapped(notebook, "switch-page",
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

    browser = gwy_app_get_data_browser();
    if (browser->current && browser->current->container == data)
        return;
    if (!data) {
        gtk_tree_view_set_model(GTK_TREE_VIEW(browser->channels), NULL);
        gtk_tree_view_set_model(GTK_TREE_VIEW(browser->graphs), NULL);
        return;
    }

    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);
    browser->current = proxy;
    gwy_app_data_browser_restore_active(GTK_TREE_VIEW(browser->channels),
                                        &proxy->channels);
    gwy_app_data_browser_restore_active(GTK_TREE_VIEW(browser->graphs),
                                        &proxy->graphs);

    /* TODO: set title, selection, ... */
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
                                                             dfield);
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
                                                           gmodel);
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

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, TRUE);

    /* Show first window
     * XXX: crude */
    if (!gwy_app_data_browser_reconstruct_visibility(proxy)) {
        if (gwy_app_data_proxy_find_object(proxy->channels.list, 0, &iter)) {
            gtk_tree_model_get(GTK_TREE_MODEL(proxy->channels.list), &iter,
                               MODEL_OBJECT, &dfield,
                               -1);
            widget = gwy_app_data_browser_create_channel(browser, dfield);
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
}

gint
gwy_app_data_browser_add_graph_model(GwyGraphModel *gmodel,
                                     GwyContainer *data,
                                     gboolean showit)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    gchar key[32];

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), -1);
    if (showit)
        g_warning("showit not implemented");

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    g_return_val_if_fail(proxy, -1);

    g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, proxy->graphs.last + 1);
    /* This invokes "item-changed" callback that will finish the work */
    gwy_container_set_object_by_name(proxy->container, key, gmodel);

    return proxy->graphs.last + 1;
}

gint
gwy_app_data_browser_add_channel(GwyDataField *dfield,
                                 GwyContainer *data,
                                 gboolean showit)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    gchar key[24];

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), -1);
    if (showit)
        g_warning("showit not implemented");

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    g_return_val_if_fail(proxy, -1);

    g_snprintf(key, sizeof(key), "/%d/data", proxy->channels.last + 1);
    /* This invokes "item-changed" callback that will finish the work */
    gwy_container_set_object_by_name(proxy->container, key, dfield);

    return proxy->channels.last + 1;
}

void
gwy_app_data_browser_shut_down(void)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;

    browser = gwy_app_data_browser;
    if (!browser)
        return;

    gwy_app_save_window_position(GTK_WINDOW(browser->window),
                                 "/app/data-browser", TRUE, TRUE);
    browser->current = NULL;
    gtk_tree_view_set_model(GTK_TREE_VIEW(browser->channels), NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(browser->graphs), NULL);
    while (browser->container_list) {
        proxy = (GwyAppDataProxy*)browser->container_list->data;
        gwy_app_data_proxy_container_finalized(browser->container_list,
                                               (GObject*)proxy->container);
    }
}

/************************** Documentation ****************************/

/**
 * SECTION:data-browser
 * @title: data-browser
 * @short_description: Data browser
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
