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
#include <libgwydgets/gwygraphwindow.h>
#include <libgwydgets/gwydatawindow.h>
#include <app/undo.h>
#include <app/data-browser.h>

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

/* Channel and graph tree store columns */
enum {
    MODEL_ID,
    MODEL_OBJECT,
    MODEL_WIDGET,
    MODEL_N_COLUMNS
};

typedef struct _GwyAppDataBrowser GwyAppDataBrowser;
typedef struct _GwyAppDataProxy   GwyAppDataProxy;

/* The data browser */
struct _GwyAppDataBrowser {
    GList *container_list;
    struct _GwyAppDataProxy *current;
    GtkWidget *window;
    GtkWidget *channels;
    GtkWidget *graphs;
};

/* The proxy associated with each Container (this is non-GUI object) */
struct _GwyAppDataProxy {
    struct _GwyAppDataBrowser *parent;
    GwyContainer *container;
    GtkListStore *channels;
    gint last_channel;
    GtkListStore *graphs;
    gint last_graph;
};

static GwyAppDataBrowser* gwy_app_get_data_browser        (void);
static void               gwy_app_data_browser_switch_data(GwyContainer *data);

static GQuark container_quark = 0;
static GQuark own_key_quark   = 0;

static gint
gwy_app_data_proxy_compare_data(gconstpointer a,
                                gconstpointer b)
{
    GwyAppDataProxy *ua = (GwyAppDataProxy*)a;

    /* sign does not matter, only used for equality test */
    return (guchar*)ua->container - (guchar*)b;
}

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
    GtkTreeIter iter;
    gchar key[24];
    GQuark quark;

    gtk_list_store_append(proxy->channels, &iter);
    gtk_list_store_set(proxy->channels, &iter,
                       MODEL_ID, i,
                       MODEL_OBJECT, object,
                       MODEL_WIDGET, NULL,
                       -1);
    if (proxy->last_channel < i)
        proxy->last_channel = i;

    g_snprintf(key, sizeof(key), "/%d/data", i);
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

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->channels), iter,
                       MODEL_OBJECT, &object,
                       -1);
    g_signal_handlers_disconnect_by_func(object,
                                         gwy_app_data_proxy_channel_changed,
                                         proxy);
    g_object_unref(object);
    gtk_list_store_remove(proxy->channels, iter);
}

static void
gwy_app_data_proxy_reconnect_channel(GwyAppDataProxy *proxy,
                                     GtkTreeIter *iter,
                                     GObject *object)
{
    GObject *old;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->channels), iter,
                       MODEL_OBJECT, &old,
                       -1);
    g_signal_handlers_disconnect_by_func(old,
                                         gwy_app_data_proxy_channel_changed,
                                         proxy);
    gtk_list_store_set(proxy->channels, iter,
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
    GtkTreeIter iter;
    gchar key[32];
    GQuark quark;

    gtk_list_store_append(proxy->graphs, &iter);
    gtk_list_store_set(proxy->graphs, &iter,
                       MODEL_ID, i,
                       MODEL_OBJECT, object,
                       MODEL_WIDGET, NULL,
                       -1);
    if (proxy->last_graph < i)
        proxy->last_graph = i;

    g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, i);
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

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->graphs), iter,
                       MODEL_OBJECT, &object,
                       -1);
    g_signal_handlers_disconnect_by_func(object,
                                         gwy_app_data_proxy_graph_changed,
                                         proxy);
    g_object_unref(object);
    gtk_list_store_remove(proxy->graphs, iter);
}

static void
gwy_app_data_proxy_reconnect_graph(GwyAppDataProxy *proxy,
                                   GtkTreeIter *iter,
                                   GObject *object)
{
    GObject *old;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->graphs), iter,
                       MODEL_OBJECT, &old,
                       -1);
    g_signal_handlers_disconnect_by_func(old,
                                         gwy_app_data_proxy_graph_changed,
                                         proxy);
    gtk_list_store_set(proxy->graphs, iter,
                       MODEL_OBJECT, object,
                       -1);
    g_signal_connect(object, "layout-updated", /* FIXME */
                     G_CALLBACK(gwy_app_data_proxy_graph_changed), proxy);
    g_object_unref(old);
}

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

static void
gwy_app_data_proxy_finalize_list(GtkTreeModel *model,
                                 gpointer func,
                                 gpointer data)
{
    GObject *object;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        gtk_tree_model_get(model, &iter, MODEL_OBJECT, &object, -1);
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

    gwy_app_data_proxy_finalize_list(GTK_TREE_MODEL(proxy->channels),
                                     &gwy_app_data_proxy_channel_changed,
                                     proxy);
    gwy_app_data_proxy_finalize_list(GTK_TREE_MODEL(proxy->graphs),
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
gwy_app_data_proxy_find_object(GtkTreeModel *model,
                               gint i,
                               GtkTreeIter *iter)
{
    gint objid;

    if (!gtk_tree_model_get_iter_first(model, iter))
        return FALSE;

    do {
        gtk_tree_model_get(model, iter, MODEL_ID, &objid, -1);
        if (objid == i)
            return TRUE;
    } while (gtk_tree_model_iter_next(model, iter));

    return FALSE;
}

static void
gwy_app_data_proxy_item_changed(GwyContainer *data,
                                GQuark quark,
                                GwyAppDataProxy *proxy)
{
    GObject *object = NULL;
    const gchar *strkey;
    GwyAppKeyType type;
    GtkTreeIter iter;
    gboolean found;
    gint i;

    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    if (i < 0)
        return;

    gwy_container_gis_object(data, quark, &object);
    switch (type) {
        case KEY_IS_DATA:
        found = gwy_app_data_proxy_find_object(GTK_TREE_MODEL(proxy->channels),
                                               i, &iter);
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
            emit_row_changed(proxy->channels, &iter);
        }
        break;

        case KEY_IS_GRAPH:
        found = gwy_app_data_proxy_find_object(GTK_TREE_MODEL(proxy->graphs),
                                               i, &iter);
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
            emit_row_changed(proxy->channels, &iter);
        }
        break;

        case KEY_IS_MASK:
        case KEY_IS_SHOW:
        /* FIXME */
        found = gwy_app_data_proxy_find_object(GTK_TREE_MODEL(proxy->channels),
                                               i, &iter);
        if (found)
            emit_row_changed(proxy->channels, &iter);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

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

    proxy->channels = gtk_list_store_new(MODEL_N_COLUMNS,
                                         G_TYPE_INT,
                                         G_TYPE_OBJECT,
                                         G_TYPE_OBJECT);
    proxy->last_channel = -1;
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(proxy->channels),
                                         MODEL_ID, GTK_SORT_ASCENDING);

    proxy->graphs = gtk_list_store_new(MODEL_N_COLUMNS,
                                       G_TYPE_INT,
                                       G_TYPE_OBJECT,
                                       G_TYPE_OBJECT);
    /* For compatibility reasons, graphs are numbered from 1 */
    proxy->last_graph = 0;
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(proxy->graphs),
                                         MODEL_ID, GTK_SORT_ASCENDING);

    gwy_container_foreach(data, NULL, gwy_app_data_proxy_scan_data, proxy);

    return proxy;
}

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

static void
gwy_app_data_browser_channel_toggled(G_GNUC_UNUSED GtkCellRendererToggle *cell_renderer,
                                     gchar *path_str,
                                     G_GNUC_UNUSED GwyAppDataBrowser *browser)
{
    gwy_debug("Toggled data row %s", path_str);
}

static GtkWidget*
gwy_app_data_browser_construct_channels(GwyAppDataBrowser *browser)
{
    GtkWidget *tree;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* Construct the GtkTreeView that will display data channels */
    tree = gtk_tree_view_new();

    /* Add the thumbnail column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Thumbnail", renderer,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

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
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

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
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    /* Add the flags column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "width-chars", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes("Flags", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_channel_render_flags, browser, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

    return tree;
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
}

static gboolean
gwy_app_data_browser_graph_deleted(GwyGraphWindow *graph_window)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GwyAppKeyType type;
    GwyGraphModel *gmodel;
    GwyContainer *data;
    GwyGraph *graph;
    GtkTreeIter iter;
    const gchar *strkey;
    GQuark quark;
    gint i;

    gwy_debug("Graph window %p deleted", graph_window);
    graph = (GwyGraph*)gwy_graph_window_get_graph(graph_window);
    gmodel = gwy_graph_get_model(graph);
    data = g_object_get_qdata(G_OBJECT(gmodel), container_quark);
    quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(gmodel),
                                                own_key_quark));
    g_return_val_if_fail(data && quark, TRUE);

    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, NULL);
    g_return_val_if_fail(i >= 0 && type == KEY_IS_GRAPH, TRUE);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    if (!gwy_app_data_proxy_find_object(GTK_TREE_MODEL(proxy->graphs),
                                        i, &iter)) {
        g_critical("Cannot find graph model %p", gmodel);
        return TRUE;
    }

    gtk_list_store_set(proxy->graphs, &iter, MODEL_WIDGET, NULL, -1);
    gtk_widget_destroy(GTK_WIDGET(graph_window));

    return TRUE;
}

static GtkWidget*
gwy_app_data_browser_create_graph(GwyAppDataBrowser *browser,
                                  GtkCellRendererToggle *renderer,
                                  GwyGraphModel *gmodel)
{
    GtkWidget *graph, *graph_window;

    graph = gwy_graph_new(gmodel);
    graph_window = gwy_graph_window_new(GWY_GRAPH(graph));

    /* Graphs do not reference Container, fake it */
    g_object_ref(browser->current->container);
    g_object_weak_ref(G_OBJECT(graph_window),
                      (GWeakNotify)g_object_unref, browser->current->container);

    g_signal_connect_swapped(graph_window, "focus-in-event",
                             G_CALLBACK(gwy_app_data_browser_select_graph),
                             graph);
    g_signal_connect(graph_window, "delete-event",
                     G_CALLBACK(gwy_app_data_browser_graph_deleted), renderer);
    gtk_widget_show_all(graph_window);

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
    GtkWidget *widget;
    GObject *object;
    gboolean active;

    gwy_debug("Toggled graph row %s", path_str);
    proxy = browser->current;
    g_return_if_fail(proxy);

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(proxy->graphs);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter,
                       MODEL_WIDGET, &widget,
                       MODEL_OBJECT, &object,
                       -1);
    active = gtk_cell_renderer_toggle_get_active(renderer);
    g_assert(active == (widget != NULL));
    if (!active) {
        widget = gwy_app_data_browser_create_graph(browser, renderer,
                                                   GWY_GRAPH_MODEL(object));
        gtk_list_store_set(proxy->graphs, &iter, MODEL_WIDGET, widget, -1);
    }
    else {
        gtk_widget_destroy(gtk_widget_get_toplevel(widget));
        gtk_list_store_set(proxy->graphs, &iter, MODEL_WIDGET, NULL, -1);
        g_object_unref(widget);
    }
    g_object_unref(object);
}

static GtkWidget*
gwy_app_data_browser_construct_graphs(GwyAppDataBrowser *browser)
{
    GtkWidget *tree;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* Construct the GtkTreeView that will display graphs */
    tree = gtk_tree_view_new();

    /* Add the thumbnail column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Thumbnail", renderer,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

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
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

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
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    /* Add the flags column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "width-chars", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes("Curves", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_graph_render_ncurves, browser, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

    return tree;
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

static GwyAppDataBrowser*
gwy_app_get_data_browser(void)
{
    /* The list of all Containers we manage */
    static GwyAppDataBrowser *browser = NULL;

    GtkWidget *notebook, *label, *box_page, *scwin;

    if (browser)
        return browser;

    own_key_quark
        = g_quark_from_static_string("gwy-app-data-browser-own-key");
    container_quark
        = g_quark_from_static_string("gwy-app-data-browser-container");

    browser = g_new0(GwyAppDataBrowser, 1);
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_default_size(GTK_WINDOW(browser->window), 300, 300);
    gtk_window_set_title(GTK_WINDOW(browser->window), _("Data Browser"));

    /* Create the notebook */
    notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(browser->window), notebook);

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

    g_signal_connect_swapped(browser->window, "delete-event",
                             G_CALLBACK(gwy_app_data_browser_deleted), browser);

    gtk_widget_show_all(browser->window);
    gtk_window_present(GTK_WINDOW(browser->window));

    return browser;
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
    gtk_tree_view_set_model(GTK_TREE_VIEW(browser->channels),
                            GTK_TREE_MODEL(proxy->channels));
    gtk_tree_view_set_model(GTK_TREE_VIEW(browser->graphs),
                            GTK_TREE_MODEL(proxy->graphs));

    /* TODO: set title, selection, ... */
}

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
    gwy_app_data_proxy_find_object(GTK_TREE_MODEL(proxy->channels), i, &iter);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->channels));
    gtk_tree_selection_select_iter(selection, &iter);
}

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
    gwy_app_data_proxy_find_object(GTK_TREE_MODEL(proxy->graphs), i, &iter);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->graphs));
    gtk_tree_selection_select_iter(selection, &iter);
}

void
gwy_app_data_browser_add(GwyContainer *data)
{
    GwyAppDataBrowser *browser;

    browser = gwy_app_get_data_browser();
    gwy_app_data_browser_get_proxy(browser, data, TRUE);
    /* Show first window or something like that */
}

void
gwy_app_data_browser_add_graph(GwyGraphModel *gmodel,
                               GwyContainer *data,
                               gboolean showit)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    gchar key[32];

    g_return_if_fail(GWY_IS_GRAPH_MODEL(gmodel));
    if (showit)
        g_warning("showit not implemented");

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    g_return_if_fail(proxy);

    g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, proxy->last_graph + 1);
    gwy_debug("Setting keys on GraphModel %p (%s)", gmodel, key);
    /* This invokes "item-changed" callback that will finish the work */
    gwy_container_set_object_by_name(proxy->container, key, gmodel);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
