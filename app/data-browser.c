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
    MODEL_VISIBLE,
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

static void gwy_app_data_browser_switch_data(GwyContainer *data);

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
 *       number to.
 *
 * Infers expected data type from container key.
 *
 * When key is not recognized, @type is set to KEY_IS_NONE and value of @len
 * is undefined (this does NOT mean unchanged).
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
    guint i;

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

        *len = (s + i) - strkey;
        *type = KEY_IS_GRAPH;
        return atoi(s);
    }

    /* Other data */
    s = strkey + 1;
    for (i = 0; g_ascii_isdigit(s[i]); i++)
        ;
    if (!i || s[i] != GWY_CONTAINER_PATHSEP)
        return -1;

    *len = i + 2;
    i = atoi(s);
    s = strkey + *len;
    if (gwy_strequal(s, "data"))
        *type = KEY_IS_DATA;
    else if (gwy_strequal(s, "mask"))
        *type = KEY_IS_MASK;
    else if (gwy_strequal(s, "show"))
        *type = KEY_IS_SHOW;
    else
        i = -1;

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

    gtk_list_store_append(proxy->channels, &iter);
    gtk_list_store_set(proxy->channels, &iter,
                       MODEL_ID, i,
                       MODEL_OBJECT, object,
                       MODEL_VISIBLE, FALSE,
                       -1);
    if (proxy->last_channel < i)
        proxy->last_channel = i;

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

    gtk_list_store_append(proxy->graphs, &iter);
    gtk_list_store_set(proxy->graphs, &iter,
                       MODEL_ID, i,
                       MODEL_OBJECT, object,
                       MODEL_VISIBLE, FALSE,
                       -1);
    if (proxy->last_graph < i)
        proxy->last_graph = i;

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
    guint len;
    gint i;

    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, &len);
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
    guint len;
    gint i;

    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, &len);
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
                                         G_TYPE_BOOLEAN);
    proxy->last_channel = -1;
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(proxy->channels),
                                         MODEL_ID, GTK_SORT_ASCENDING);

    proxy->graphs = gtk_list_store_new(MODEL_N_COLUMNS,
                                       G_TYPE_INT,
                                       G_TYPE_OBJECT,
                                       G_TYPE_BOOLEAN);
    /* For compatibility reasons, graphs are numbered from 1 */
    proxy->last_graph = 0;
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(proxy->graphs),
                                         MODEL_ID, GTK_SORT_ASCENDING);

    gwy_container_foreach(data, NULL, gwy_app_data_proxy_scan_data, proxy);

    return proxy;
}

static GwyAppDataProxy*
gwy_app_data_proxy_get_for_data(GwyAppDataBrowser *browser,
                                GwyContainer *data,
                                gboolean do_create)
{
    GList *item;

    item = g_list_find_custom(browser->container_list, data,
                              &gwy_app_data_proxy_compare_data);
    if (!item) {
        if (do_create)
            return gwy_app_data_proxy_new(browser, data);
        else
            return NULL;
    }

    /* move container to head */
    if (item != browser->container_list) {
        browser->container_list = g_list_remove_link(browser->container_list,
                                                     item);
        browser->container_list = g_list_concat(item, browser->container_list);
    }

    return (GwyAppDataProxy*)item->data;
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
    /*
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    gboolean enabled;

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(browser->channel_store);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, VIS_COLUMN, &enabled, -1);
    enabled = !enabled;

    gtk_list_store_set(browser->channel_store, &iter, VIS_COLUMN, enabled, -1);

    gtk_tree_path_free(path);
    */
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
                                                      "active", MODEL_VISIBLE,
                                                      NULL);
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

static void
gwy_app_data_browser_graph_toggled(G_GNUC_UNUSED GtkCellRendererToggle *cell_renderer,
                                   gchar *path_str,
                                   G_GNUC_UNUSED GwyAppDataBrowser *browser)
{
    gwy_debug("Toggled data row %s", path_str);
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
                                                      "active", MODEL_VISIBLE,
                                                      NULL);
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

    proxy = gwy_app_data_proxy_get_for_data(browser, data, FALSE);
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
    guint len;
    gint i;

    data = gwy_data_view_get_data(data_view);
    gwy_app_data_browser_switch_data(data);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_proxy_get_for_data(browser, data, FALSE);
    g_return_if_fail(proxy);

    layer = gwy_data_view_get_base_layer(data_view);
    strkey = gwy_pixmap_layer_get_data_key(layer);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, &len);
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
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GwyContainer *data;
    const gchar *strkey;
    GwyAppKeyType type;
    GQuark quark;
    guint len;
    gint i;

    data = g_object_get_data(G_OBJECT(graph), "gwy-app-data-browser-container");
    g_return_if_fail(data);
    gwy_app_data_browser_switch_data(data);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_proxy_get_for_data(browser, data, FALSE);
    g_return_if_fail(proxy);

    quark = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(graph),
                                               "gwy-app-data-browser-quark"));
    strkey = g_quark_to_string(quark);
    i = gwy_app_data_proxy_analyse_key(strkey, &type, &len);
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
    gwy_app_data_proxy_get_for_data(browser, data, TRUE);
    /* Show first window or something like that */
}

#if 0
typedef struct {
    GwyContainer *container;
    GtkListStore *channel_store;
} DataBrowser;

enum {
    VIS_COLUMN,
    TITLE_COLUMN,
    N_COLUMNS
};

static GtkWidget* gwy_browser_construct_channels(DataBrowser *browser);

static void   gwy_browser_channel_toggled(GtkCellRendererToggle *cell_renderer,
                                   gchar *path_str,
                                   DataBrowser *browser);

/**
 * gwy_app_data_browser:
 * @data: A data container to be browsed.
 *
 * Creates and displays a data browser window. All data channels, graphs,
 * etc. within @data will be displayed.
 **/
void
gwy_app_data_browser(GwyContainer *data)
{
    DataBrowser *browser;
    GtkWidget *window, *notebook;
    GtkWidget *channels;
    GtkWidget *box_page;
    GtkWidget *label;
    const guchar *filename = NULL;
    gchar *base_name;
    gchar *window_title;

    g_return_if_fail(GWY_IS_CONTAINER(data));
    //g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    /* Setup browser structure */
    browser = g_new0(DataBrowser, 1);
    browser->container = data;

    /* Setup the window */
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 300);
    window_title = g_strdup("Data Browser");
    if (gwy_container_gis_string_by_name(data, "/filename", &filename)) {
        base_name = g_path_get_basename(filename);
        window_title = g_strconcat(window_title, ": ", base_name, NULL);
        g_free(base_name);
    }
    gtk_window_set_title(GTK_WINDOW(window), window_title);
    g_free(window_title);

    /* Create the notebook */
    notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(GTK_WINDOW(window)), notebook);

    /* Create the notebook tabs */
    channels = gwy_browser_construct_channels(browser);
    box_page = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box_page), channels, FALSE, FALSE, 0);
    label = gtk_label_new("Data Channels");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new("Graphs");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new("Masks");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), box_page, label);

    /* Connect signals */
    //g_signal_connect(data_window, "destroy",
    //                 G_CALLBACK(gwy_app_data_window_remove), NULL);

    gtk_widget_show_all(window);
    gtk_window_present(GTK_WINDOW(window));
    //return window;
}

static GtkWidget* gwy_browser_construct_channels(DataBrowser *browser)
{
    GtkListStore *store;
    GtkTreeIter iter;
    GtkWidget *tree;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    GArray *chan_numbers;
    gchar *channel_title = NULL;
    gint i, number;

    /* Create a list store to hold the channel data */
    store = gtk_list_store_new(N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING);
    browser->channel_store = store;

    /* Add channels to list store */
    chan_numbers = gwy_browser_get_channel_numbers(browser->container);
    for (i=0; i<chan_numbers->len; i++) {
        g_debug("num: %i", g_array_index(chan_numbers, gint, i));

        number = g_array_index(chan_numbers, gint, i);
        channel_title = gwy_browser_get_channel_title(browser->container,
                                                      number);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, VIS_COLUMN, TRUE,
                           TITLE_COLUMN, channel_title, -1);
        g_free(channel_title);
    }
    g_array_free(chan_numbers, TRUE);

    /* Construct the GtkTreeView that will display data channels */
    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    /* Add the "Visible" column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(G_OBJECT(renderer), "activatable", TRUE, NULL);
    g_signal_connect(renderer, "toggled",
                     G_CALLBACK(gwy_browser_channel_toggled),
                     browser);
    column = gtk_tree_view_column_new_with_attributes("Visible", renderer,
                                                      "active", VIS_COLUMN,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    /* Add the "Title" column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      "text", TITLE_COLUMN,
                                                      NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    /* connect signals */
    //g_signal_connect(browser->container, "item-changed",
    //                 G_CALLBACK(gwy_browser_item_changed), browser);

    return tree;
}

static void
gwy_browser_channel_toggled(G_GNUC_UNUSED GtkCellRendererToggle *cell_renderer,
                            gchar *path_str,
                            DataBrowser *browser)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    gboolean enabled;

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(browser->channel_store);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, VIS_COLUMN, &enabled, -1);
    enabled = !enabled;

    /*TODO: implement show/hide of windows here */

    gtk_list_store_set(browser->channel_store, &iter, VIS_COLUMN, enabled, -1);

    gtk_tree_path_free(path);
}

/**
 * gwy_browser_get_n_channels:
 * @data: A data container.
 *
 * Used to get the number of data channels stored within the @data
 * container.
 *
 *
 * Returns: the number of channels as a #gint.
 **/
gint
gwy_browser_get_n_channels(GwyContainer *data)
{
    /*XXX: is this needed at all?*/
    return 0;
}

/**
 * gwy_browser_get_channel_title:
 * @data: A data container.
 * @channel: the data channel.
 *
 * Used to get the title of the given data channel stored within @data. If the
 * title can't be found, "Unknown Channel" will be returned.
 *
 * Returns: a new string containing the title (free it after use).
 **/
gchar*
gwy_browser_get_channel_title(GwyContainer *data, guint channel)
{
    gchar* channel_key;
    const guchar* channel_title = NULL;

    channel_key = g_strdup_printf("/%i/data/title", channel);
    gwy_container_gis_string_by_name(data, channel_key, &channel_title);

    /* Need to support "old" files (1.x) */
    if (!channel_title)
        gwy_container_gis_string_by_name(data, "/filename/title",
                                         &channel_title);

    if (channel_title)
        return g_strdup(channel_title);
    else
        return g_strdup("Unknown Channel");
}

/**
 * gwy_browser_get_channel_key:
 * @channel: the data channel.
 *
 * Used to automatically generate the appropriate container key for a given
 * data channel. (ie. channel=0 returns "/0/data", channel=1 returns "/1/data")
 *
 * Returns: a new string containing the key (free it after use).
 **/
gchar*
gwy_browser_get_channel_key(guint channel)
{
    gchar* channel_key;

    channel_key = g_strdup_printf("/%i/data", channel);

    return channel_key;
}

static void
gwy_browser_extract_channel_number(GQuark key,
                                   G_GNUC_UNUSED GValue *value,
                                   GArray *numbers)
{
    const gchar* str;
    gchar **tokens;
    gchar *delimiter;
    gdouble d_num;
    gint i_num;

    str = g_quark_to_string(key);
    if (g_str_has_suffix(str, "/data")) {
        delimiter = g_strdup("/");
        tokens = g_strsplit(str, delimiter, 3);
        d_num = g_ascii_strtod(tokens[1], NULL);
        i_num = (gint)d_num;
        g_array_append_val(numbers, i_num);
        g_free(delimiter);
        g_strfreev(tokens);
    }
}

static gint
gwy_browser_sort_channels(gconstpointer a, gconstpointer b)
{
    gint num1, num2;
    num1 = *(gint*)a;
    num2 = *(gint*)b;

    if (num1 < num2)
        return -1;
    else if (num2 < num1)
        return 1;
    else
        return 0;
}

/**
 * gwy_browser_get_channel_numbers:
 * @data: the data container.
 *
 * Used to find out what the reference numbers are for all the channels stored
 * within @data. Channels are stored under the key: "/N/data", where N
 * represents the channel reference number. Checking the "len" member of the
 * returned #GArray will tell you how many channels are stored within @data.
 *
 * Returns: a new #GArray containing the channel numbers as #gint (free the
 * #GArray after use).
 **/
GArray*
gwy_browser_get_channel_numbers(GwyContainer *data)
{
    GArray *numbers;

    numbers = g_array_new(FALSE, TRUE, sizeof(gint));
    gwy_container_foreach(data, "/",
                          (GHFunc)gwy_browser_extract_channel_number,
                          numbers);
    g_array_sort(numbers, (GCompareFunc)gwy_browser_sort_channels);
    return numbers;
}
#endif


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
