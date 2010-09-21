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

/* XXX: The purpose of this file is to contain all ugliness from the rest of
 * source files.  And indeed it has managed to gather lots of it. */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/stats.h>
#include <libdraw/gwypixfield.h>
#include <libgwydgets/gwydatawindow.h>
#include <libgwydgets/gwycoloraxis.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwygraphwindow.h>
#include <libgwydgets/gwy3dwindow.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>
#include "app/gwyappinternal.h"

/* The GtkTargetEntry for tree model drags.
 * FIXME: Is it Gtk+ private or what? */
#define GTK_TREE_MODEL_ROW \
    { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_APP, 0 }

/* Data browser window manager role */
#define GWY_DATABROWSER_WM_ROLE "gwyddion-databrowser"

/* The container prefix all graph reside in.  This is a bit silly but it does
 * not worth to break file compatibility with 1.x. */
#define GRAPH_PREFIX "/0/graph/graph"

/* Single point spectra prefix.  This one is sane and should remain so. */
#define SPECTRA_PREFIX "/sps"

enum {
    THUMB_TIMEOUT = 100,
    THUMB_SIZE = 60
};

/* Notebook pages */
enum {
    PAGE_CHANNELS,
    PAGE_GRAPHS,
    PAGE_SPECTRA,
    NPAGES,
    PAGE_NOPAGE = G_MAXINT-1,
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
    MODEL_TIMESTAMP,
    MODEL_THUMBNAIL,
    MODEL_N_COLUMNS
};

typedef struct _GwyAppDataBrowser GwyAppDataBrowser;
typedef struct _GwyAppDataProxy   GwyAppDataProxy;

typedef gboolean (*SetVisibleFunc)(GwyAppDataProxy *proxy,
                                   GtkTreeIter *iter,
                                   gboolean visible);

/* Channel or graph list */
typedef struct {
    GtkListStore *store;
    gint last;  /* The id of last object, if no object is present, it is equal
                   to the smallest possible id minus 1 */
    gint active;
    gint visible_count;
} GwyAppDataList;

/* FIXME: Crude.  3D is not a first class citizen. */
typedef struct {
    Gwy3DWindow *window;
    gint id;
} GwyApp3DAssociation;

typedef struct {
    GwyAppDataWatchFunc function;
    gpointer user_data;
    gulong id;
} GwyAppWatcherData;

/* The data browser */
struct _GwyAppDataBrowser {
    GList *proxy_list;
    struct _GwyAppDataProxy *current;
    gint active_page;
    gint untitled_counter;
    GwySensitivityGroup *sensgroup;
    GtkWidget *filename;
    GtkWidget *window;
    GtkWidget *notebook;
    GtkWidget *lists[NPAGES];
};

/* The proxy associated with each Container (this is non-GUI object) */
struct _GwyAppDataProxy {
    guint finalize_id;
    gint untitled_no;
    gboolean keep_invisible;
    struct _GwyAppDataBrowser *parent;
    GwyContainer *container;
    GwyAppDataList lists[NPAGES];
    GList *associated3d;
};

static GwyAppDataBrowser* gwy_app_get_data_browser        (void);
static void gwy_app_data_browser_update_filename(GwyAppDataProxy *proxy);
static GwyAppDataProxy* gwy_app_data_browser_get_proxy(GwyAppDataBrowser *browser,
                                                       GwyContainer *data,
                                                       gboolean do_create);
static gboolean gwy_app_data_proxy_find_object(GtkListStore *store,
                                               gint i,
                                               GtkTreeIter *iter);
static void gwy_app_data_browser_switch_data(GwyContainer *data);
static void gwy_app_data_browser_sync_mask  (GwyContainer *data,
                                             GQuark quark,
                                             GwyDataView *data_view);
static void gwy_app_data_browser_sync_show  (GwyContainer *data,
                                             GQuark quark,
                                             GwyDataView *data_view);
static void gwy_app_data_proxy_destroy_all_3d(GwyAppDataProxy *proxy);
static void gwy_app_data_proxy_update_window_titles(GwyAppDataProxy *proxy);
static void gwy_app_update_data_window_title(GwyDataView *data_view,
                                             gint id);
static void gwy_app_update_3d_window_title  (Gwy3DWindow *window3d,
                                             gint id);
static void gwy_app_update_data_range_type(GwyDataView *data_view,
                                           gint id);
static gboolean gwy_app_data_proxy_channel_set_visible(GwyAppDataProxy *proxy,
                                                       GtkTreeIter *iter,
                                                       gboolean visible);
static gboolean gwy_app_data_proxy_graph_set_visible(GwyAppDataProxy *proxy,
                                                     GtkTreeIter *iter,
                                                     gboolean visible);
static GList*   gwy_app_data_proxy_find_3d  (GwyAppDataProxy *proxy,
                                             Gwy3DWindow *window3d);
static GList*   gwy_app_data_proxy_get_3d   (GwyAppDataProxy *proxy,
                                             gint id);
static void     gwy_app_data_browser_copy_object(GwyAppDataProxy *srcproxy,
                                                 guint pageno,
                                                 GtkTreeModel *model,
                                                 GtkTreeIter *iter,
                                                 GwyAppDataProxy *destproxy);
static void     gwy_app_data_browser_copy_other(GtkTreeModel *model,
                                                GtkTreeIter *iter,
                                                GtkWidget *window,
                                                GwyContainer *container);
static gboolean gwy_app_data_browser_select_data_view2(GwyDataView *data_view);
static gboolean gwy_app_data_browser_select_graph2    (GwyGraph *graph);
static const gchar*
gwy_app_data_browser_figure_out_channel_title(GwyContainer *data,
                                              gint channel);
static void gwy_app_data_browser_show_real   (GwyAppDataBrowser *browser);
static void gwy_app_data_browser_hide_real   (GwyAppDataBrowser *browser);
static GdkPixbuf* gwy_app_get_graph_thumbnail(GwyContainer *data,
                                              gint id,
                                              gint max_width,
                                              gint max_height);
static void gwy_app_data_browser_notify_watch(GList *watchers,
                                              GwyContainer *container,
                                              gint id);

static GQuark container_quark    = 0;
static GQuark own_key_quark      = 0;
static GQuark page_id_quark      = 0;  /* NB: data is pageno+1, not pageno */
static GQuark filename_quark     = 0;
static GQuark column_id_quark    = 0;
static GQuark graph_window_quark = 0;

/* The data browser */
static GwyAppDataBrowser *gwy_app_data_browser = NULL;
static gboolean gui_disabled = FALSE;

static gulong watcher_id = 0;
static GList *channel_watchers = NULL;
/*
static GList *graph_watchers = NULL;
static GList *spectra_watchers = NULL;
*/

/* Use doubles for timestamps.  They have 53bit mantisa, which is sufficient
 * for microsecond precision. */
static inline gdouble
gwy_get_timestamp(void)
{
    GTimeVal timestamp;

    g_get_current_time(&timestamp);
    return timestamp.tv_sec + 1e-6*timestamp.tv_usec;
}

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
 * gwy_app_data_proxy_compare:
 * @a: Pointer to a #GwyAppDataProxy.
 * @b: Pointer to a #GwyAppDataProxy.
 *
 * Compares two data proxies using file name ordering.
 *
 * Returns: -1, 1 or 0 according to alphabetical order.
 **/
static gint
gwy_app_data_proxy_compare(gconstpointer a,
                           gconstpointer b)
{
    GwyContainer *ua = ((GwyAppDataProxy*)a)->container;
    GwyContainer *ub = ((GwyAppDataProxy*)b)->container;
    const guchar *fa = NULL, *fb = NULL;

    gwy_container_gis_string(ua, filename_quark, &fa);
    gwy_container_gis_string(ub, filename_quark, &fb);
    if (!fa && !fb)
        return (guchar*)ua - (guchar*)ub;
    if (!fa)
        return -1;
    if (!fb)
        return 1;
    return g_utf8_collate(fa, fb);
}

/**
 * gwy_app_data_browser_set_file_present:
 * @browser: A data browser.
 * @present: %TRUE when a file is opened, %FALSE when no file is opened.
 *
 * Updates sensitivity groups according to file existence state.
 **/
static void
gwy_app_data_browser_set_file_present(GwyAppDataBrowser *browser,
                                      gboolean present)
{
    GwySensitivityGroup *sensgroup;

    if (browser->sensgroup) {
        if (present)
            gwy_sensitivity_group_set_state(browser->sensgroup,
                                            SENS_FILE, SENS_FILE);
        else
            gwy_sensitivity_group_set_state(browser->sensgroup,
                                            SENS_FILE | SENS_OBJECT, 0);
    }

    if ((sensgroup = _gwy_app_sensitivity_get_group()))
        gwy_sensitivity_group_set_state(sensgroup,
                                        GWY_MENU_FLAG_FILE,
                                        present ? GWY_MENU_FLAG_FILE : 0);
}

/**
 * gwy_app_widget_queue_manage:
 * @widget: Widget to add or remove.
 * @remv: %TRUE to remove, %FALSE to add.
 *
 * Adds or removes widget to queue.
 *
 * If a new widget is added, it becomes the new head.  If the added widget
 * is already present, it is just moved to the head.
 *
 * Returns: The first widget in the queue of the same type as @widget.  The
 *          returned value is interesting only when @remove is %TRUE, for
 *          adding the returned value is always equal to @widget.
 **/
static GtkWidget*
gwy_app_widget_queue_manage(GtkWidget *widget, gboolean remv)
{
    static GList *list = NULL;

    GList *item;
    GType type;

    type = G_TYPE_FROM_INSTANCE(widget);

    if (remv) {
        list = g_list_remove(list, widget);
        for (item = list; item; item = g_list_next(item)) {
            if (G_TYPE_FROM_INSTANCE(item->data) == type)
                return GTK_WIDGET(item->data);
        }
        return NULL;
    }

    item = g_list_find(list, widget);
    if (item) {
        list = g_list_remove_link(list, item);
        list = g_list_concat(item, list);
    }
    else
        list = g_list_prepend(list, widget);

    return widget;
}

/**
 * _gwy_app_analyse_data_key:
 * @strkey: String container key.
 * @type: Location to store data type to.
 * @len: Location to store the length of common prefix or %NULL.
 *       Usually this is up to the last digit of data number,
 *       however selections have also "/select" skipped,
 *       titles have "/data" skipped.  Note the remaining part of the key
 *       still includes the leading "/" (if non-empty).
 *
 * Infers expected data type from container key.
 *
 * When key is not recognized, @type is set to %KEY_IS_NONE and value of @len
 * is unchanged.
 *
 * Returns: Data number (id), -1 when key does not correspond to any data
 *          object.  Note -1 is also returned for %KEY_IS_FILENAME type.
 **/
gint
_gwy_app_analyse_data_key(const gchar *strkey,
                          GwyAppKeyType *type,
                          guint *len)
{
    const gchar *s;
    gint i;
    guint n;

    *type = KEY_IS_NONE;

    if (strkey[0] != GWY_CONTAINER_PATHSEP)
        return -1;

    /* Graph */
    if (g_str_has_prefix(strkey, GRAPH_PREFIX GWY_CONTAINER_PATHSEP_STR)) {
        s = strkey + sizeof(GRAPH_PREFIX);
        /* Do not use strtol, it allows queer stuff like spaces */
        for (i = 0; g_ascii_isdigit(s[i]); i++)
            ;
        if (!i || (s[i] && s[i] != GWY_CONTAINER_PATHSEP))
            return -1;

        if (gwy_strequal(s + i, "/visible")) {
            *type = KEY_IS_GRAPH_VISIBLE;
        }
        else if (!s[i])
            *type = KEY_IS_GRAPH;
        else
            return -1;

        if (len)
            *len = (s + i) - strkey;

        return atoi(s);
    }

    /* Spectra */
    if (g_str_has_prefix(strkey, SPECTRA_PREFIX GWY_CONTAINER_PATHSEP_STR)) {
        s = strkey + sizeof(SPECTRA_PREFIX);
        /* Do not use strtol, it allows queer stuff like spaces */
        for (i = 0; g_ascii_isdigit(s[i]); i++)
            ;
        if (!i || (s[i] && s[i] != GWY_CONTAINER_PATHSEP))
            return -1;

        if (gwy_strequal(s + i, "/visible")) {
            *type = KEY_IS_SPECTRA_VISIBLE;
        }
        else if (!s[i])
            *type = KEY_IS_SPECTRA;
        else
            return -1;

        if (len)
            *len = (s + i) - strkey;

        return atoi(s);
    }

    /* Non-id */
    if (gwy_strequal(strkey, "/filename")) {
        if (len)
            *len = 0;
        *type = KEY_IS_FILENAME;
        return -1;
    }
    if (gwy_strequal(strkey, "/0/graph/lastid")) {
        if (len)
            *len = 0;
        *type = KEY_IS_GRAPH_LASTID;
        return -1;
    }

    /* Other data */
    s = strkey + 1;
    for (i = 0; g_ascii_isdigit(s[i]); i++)
        ;
    if (!i || s[i] != GWY_CONTAINER_PATHSEP)
        return -1;

    n = i + 1;
    i = atoi(s);
    s = strkey + n + 1;
    if (gwy_strequal(s, "data"))
        *type = KEY_IS_DATA;
    else if (gwy_strequal(s, "mask"))
        *type = KEY_IS_MASK;
    else if (gwy_strequal(s, "show"))
        *type = KEY_IS_SHOW;
    else if (gwy_strequal(s, "base/palette"))
        *type = KEY_IS_PALETTE;
    else if (g_str_has_prefix(s, "select/")
             && !strchr(s + sizeof("select/")-1, '/')) {
        *type = KEY_IS_SELECT;
        n += strlen("select/");
    }
    else if (gwy_strequal(s, "data/visible"))
        *type = KEY_IS_DATA_VISIBLE;
    else if (gwy_strequal(s, "data/title")
             || gwy_strequal(s, "data/untitled")) {
        *type = KEY_IS_TITLE;
        n += strlen("data/");
    }
    else if (gwy_strequal(s, "base/range-type"))
        *type = KEY_IS_RANGE_TYPE;
    else if (gwy_strequal(s, "base/min")
             || gwy_strequal(s, "base/max")) {
        *type = KEY_IS_RANGE;
        n += strlen("base/");
    }
    else if (gwy_strequal(s, "mask/red")
             || gwy_strequal(s, "mask/blue")
             || gwy_strequal(s, "mask/green")
             || gwy_strequal(s, "mask/alpha")) {
        *type = KEY_IS_MASK_COLOR;
        n += strlen("mask/");
    }
    else if (gwy_strequal(s, "meta"))
        *type = KEY_IS_META;
    else if (gwy_strequal(s, "data/realsquare"))
        *type = KEY_IS_REAL_SQUARE;
    else if (gwy_strequal(s, "sps-id"))
        *type = KEY_IS_SPS_REF;
    else if (gwy_strequal(s, "3d/setup"))
        *type = KEY_IS_3D_SETUP;
    else if (gwy_strequal(s, "3d/palette"))
        *type = KEY_IS_3D_PALETTE;
    else if (gwy_strequal(s, "3d/material"))
        *type = KEY_IS_3D_MATERIAL;
    else if (gwy_strequal(s, "3d/x")
             || gwy_strequal(s, "3d/y")
             || gwy_strequal(s, "3d/min")
             || gwy_strequal(s, "3d/max")) {
        *type = KEY_IS_3D_LABEL;
        n += strlen("3d/");
    }
    else
        i = -1;

    if (len && i > -1)
        *len = n;

    return i;
}

/**
 * gwy_app_data_proxy_add_object:
 * @list: A data proxy list.
 * @i: Object id.
 * @object: The object to add (data field, graph model, ...).
 *
 * Adds an object to data proxy list.
 **/
static void
gwy_app_data_proxy_add_object(GwyAppDataList *list,
                              gint i,
                              GtkTreeIter *iter,
                              GObject *object)
{
    gtk_list_store_insert_with_values(list->store, iter, G_MAXINT,
                                      MODEL_ID, i,
                                      MODEL_OBJECT, object,
                                      MODEL_WIDGET, NULL,
                                      MODEL_THUMBNAIL, NULL,
                                      -1);
    if (list->last < i)
        list->last = i;
}

/**
 * gwy_app_data_proxy_switch_object_data:
 * @proxy: Data proxy (not actually used except for sanity check).
 * @old: Old object.
 * @object: New object.
 *
 * Moves qdata set on data proxy object list objects from one object to another
 * one, unsetting them on the old object.
 **/
static void
gwy_app_data_proxy_switch_object_data(G_GNUC_UNUSED GwyAppDataProxy *proxy,
                                      GObject *old,
                                      GObject *object)
{
    gpointer old_container, old_own_key;

    old_container = g_object_get_qdata(old, container_quark);
    g_return_if_fail(old_container == proxy->container);

    old_own_key = g_object_get_qdata(old, own_key_quark);
    g_return_if_fail(own_key_quark);

    g_object_set_qdata(old, container_quark, NULL);
    g_object_set_qdata(old, own_key_quark, NULL);
    g_object_set_qdata(object, container_quark, old_container);
    g_object_set_qdata(object, own_key_quark, old_own_key);
}

/**
 * gwy_app_data_proxy_channel_changed:
 * @channel: The data field representing a channel.
 * @proxy: Data proxy.
 *
 * Updates channel display in the data browser when channel data change.
 **/
static void
gwy_app_data_proxy_channel_changed(GwyDataField *channel,
                                   GwyAppDataProxy *proxy)
{
    GwyAppKeyType type;
    GtkTreeIter iter;
    GQuark quark;
    gint id;

    gwy_debug("proxy=%p channel=%p", proxy, channel);
    quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(channel),
                                                own_key_quark));
    g_return_if_fail(quark);
    id = _gwy_app_analyse_data_key(g_quark_to_string(quark), &type, NULL);
    g_return_if_fail(id >= 0);
    if (!gwy_app_data_proxy_find_object(proxy->lists[PAGE_CHANNELS].store, id,
                                        &iter))
        return;

    gtk_list_store_set(proxy->lists[PAGE_CHANNELS].store, &iter,
                       MODEL_TIMESTAMP, gwy_get_timestamp(),
                       -1);
    gwy_app_data_browser_notify_watch(channel_watchers, proxy->container, id);
}

/**
 * gwy_app_data_proxy_connect_channel:
 * @proxy: Data proxy.
 * @i: Channel id.
 * @object: The data field to add (passed as #GObject).
 *
 * Adds a data field as channel of specified id, setting qdata and connecting
 * signals.
 **/
static void
gwy_app_data_proxy_connect_channel(GwyAppDataProxy *proxy,
                                   gint id,
                                   GtkTreeIter *iter,
                                   GObject *object)
{
    gchar key[24];
    GQuark quark;

    gwy_app_data_proxy_add_object(&proxy->lists[PAGE_CHANNELS], id, iter,
                                  object);
    g_snprintf(key, sizeof(key), "/%d/data", id);
    gwy_debug("%p: %d in %p", object, i, proxy->container);
    quark = g_quark_from_string(key);
    g_object_set_qdata(object, container_quark, proxy->container);
    g_object_set_qdata(object, own_key_quark, GUINT_TO_POINTER(quark));

    g_signal_connect(object, "data-changed",
                     G_CALLBACK(gwy_app_data_proxy_channel_changed), proxy);
    gwy_app_data_browser_notify_watch(channel_watchers, proxy->container, id);
}

/**
 * gwy_app_data_proxy_disconnect_channel:
 * @proxy: Data proxy.
 * @iter: Tree iterator pointing to the channel in @proxy's list store.
 *
 * Disconnects signals from a channel data field, removes qdata and finally
 * removes it from the data proxy list store.
 **/
static void
gwy_app_data_proxy_disconnect_channel(GwyAppDataProxy *proxy,
                                      GtkTreeIter *iter)
{
    GObject *object;
    gint id;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_CHANNELS].store), iter,
                       MODEL_OBJECT, &object,
                       MODEL_ID, &id,
                       -1);
    gwy_debug("%p: from %p", object, proxy->container);
    g_object_set_qdata(object, container_quark, NULL);
    g_object_set_qdata(object, own_key_quark, NULL);
    g_signal_handlers_disconnect_by_func(object,
                                         gwy_app_data_proxy_channel_changed,
                                         proxy);
    g_object_unref(object);
    gtk_list_store_remove(proxy->lists[PAGE_CHANNELS].store, iter);
    gwy_app_data_browser_notify_watch(channel_watchers, proxy->container, id);
}

/**
 * gwy_app_data_proxy_reconnect_channel:
 * @proxy: Data proxy.
 * @iter: Tree iterator pointing to the channel in @proxy's list store.
 * @object: The data field representing the channel (passed as #GObject).
 *
 * Updates data proxy's list store when the data field representing a channel
 * is switched for another data field.
 **/
static void
gwy_app_data_proxy_reconnect_channel(GwyAppDataProxy *proxy,
                                     GtkTreeIter *iter,
                                     GObject *object)
{
    GObject *old;
    gint id;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_CHANNELS].store), iter,
                       MODEL_OBJECT, &old,
                       MODEL_ID, &id,
                       -1);
    g_signal_handlers_disconnect_by_func(old,
                                         gwy_app_data_proxy_channel_changed,
                                         proxy);
    gwy_app_data_proxy_switch_object_data(proxy, old, object);
    gtk_list_store_set(proxy->lists[PAGE_CHANNELS].store, iter,
                       MODEL_OBJECT, object,
                       -1);
    g_signal_connect(object, "data-changed",
                     G_CALLBACK(gwy_app_data_proxy_channel_changed), proxy);
    g_object_unref(old);
    gwy_app_data_browser_notify_watch(channel_watchers, proxy->container, id);
}

/**
 * gwy_app_data_proxy_graph_changed:
 * @graph: The graph model representing a graph.
 * @proxy: Data proxy.
 *
 * Updates graph display in the data browser when graph data change.
 **/
static void
gwy_app_data_proxy_graph_changed(GwyGraphModel *graph,
                                 G_GNUC_UNUSED GParamSpec *pspec,
                                 GwyAppDataProxy *proxy)
{
    GwyAppKeyType type;
    GtkTreeIter iter;
    GQuark quark;
    gint id;

    gwy_debug("proxy=%p, graph=%p", proxy, graph);
    if (!(quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(graph),
                                                      own_key_quark))))
        return;
    id = _gwy_app_analyse_data_key(g_quark_to_string(quark), &type, NULL);
    g_return_if_fail(type == KEY_IS_GRAPH);
    if (!gwy_app_data_proxy_find_object(proxy->lists[PAGE_GRAPHS].store, id,
                                        &iter))
        return;
    gwy_list_store_row_changed(proxy->lists[PAGE_GRAPHS].store,
                               &iter, NULL, -1);
}

/**
 * gwy_app_data_proxy_connect_graph:
 * @proxy: Data proxy.
 * @i: Channel id.
 * @object: The graph model to add (passed as #GObject).
 *
 * Adds a graph model as graph of specified id, setting qdata and connecting
 * signals.
 **/
static void
gwy_app_data_proxy_connect_graph(GwyAppDataProxy *proxy,
                                 gint i,
                                 GtkTreeIter *iter,
                                 GObject *object)
{
    GQuark quark;

    gwy_app_data_proxy_add_object(&proxy->lists[PAGE_GRAPHS], i, iter,
                                  object);
    gwy_debug("%p: %d in %p", object, i, proxy->container);
    quark = gwy_app_get_graph_key_for_id(i);
    g_object_set_qdata(object, container_quark, proxy->container);
    g_object_set_qdata(object, own_key_quark, GUINT_TO_POINTER(quark));

    g_signal_connect(object, "notify::n-curves", /* FIXME */
                     G_CALLBACK(gwy_app_data_proxy_graph_changed), proxy);
}

/**
 * gwy_app_data_proxy_disconnect_graph:
 * @proxy: Data proxy.
 * @iter: Tree iterator pointing to the graph in @proxy's list store.
 *
 * Disconnects signals from a graph graph model, removes qdata and finally
 * removes it from the data proxy list store.
 **/
static void
gwy_app_data_proxy_disconnect_graph(GwyAppDataProxy *proxy,
                                    GtkTreeIter *iter)
{
    GObject *object;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_GRAPHS].store), iter,
                       MODEL_OBJECT, &object,
                       -1);
    gwy_debug("%p: from %p", object, proxy->container);
    g_object_set_qdata(object, container_quark, NULL);
    g_object_set_qdata(object, own_key_quark, NULL);
    g_signal_handlers_disconnect_by_func(object,
                                         gwy_app_data_proxy_graph_changed,
                                         proxy);
    g_object_unref(object);
    gtk_list_store_remove(proxy->lists[PAGE_GRAPHS].store, iter);
}

/**
 * gwy_app_data_proxy_reconnect_graph:
 * @proxy: Data proxy.
 * @iter: Tree iterator pointing to the graph in @proxy's list store.
 * @object: The graph model representing the graph (passed as #GObject).
 *
 * Updates data proxy's list store when the graph model representing a graph
 * is switched for another graph model.
 **/
static void
gwy_app_data_proxy_reconnect_graph(GwyAppDataProxy *proxy,
                                   GtkTreeIter *iter,
                                   GObject *object)
{
    GObject *old;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_GRAPHS].store), iter,
                       MODEL_OBJECT, &old,
                       -1);
    g_signal_handlers_disconnect_by_func(old,
                                         gwy_app_data_proxy_graph_changed,
                                         proxy);
    gwy_app_data_proxy_switch_object_data(proxy, old, object);
    gtk_list_store_set(proxy->lists[PAGE_GRAPHS].store, iter,
                       MODEL_OBJECT, object,
                       -1);
    g_signal_connect(object, "notify::n-curves", /* FIXME */
                     G_CALLBACK(gwy_app_data_proxy_graph_changed), proxy);
    g_object_unref(old);
}

/**
 * gwy_app_data_proxy_spectra_changed:
 * @spectra: The spectra object.
 * @proxy: Data proxy.
 *
 * Updates spectra display in the data browser when spectra data change.
 **/
static void
gwy_app_data_proxy_spectra_changed(GwySpectra *spectra,
                                   GwyAppDataProxy *proxy)
{
    GwyAppKeyType type;
    GtkTreeIter iter;
    GQuark quark;
    gint id;

    gwy_debug("proxy=%p, spectra=%p", proxy, spectra);
    if (!(quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(spectra),
                                                      own_key_quark))))
        return;
    id = _gwy_app_analyse_data_key(g_quark_to_string(quark), &type, NULL);
    g_return_if_fail(type == KEY_IS_SPECTRA);
    if (!gwy_app_data_proxy_find_object(proxy->lists[PAGE_SPECTRA].store, id,
                                        &iter))
        return;
    gwy_list_store_row_changed(proxy->lists[PAGE_SPECTRA].store,
                               &iter, NULL, -1);
}

/**
 * gwy_app_data_proxy_connect_spectra:
 * @proxy: Data proxy.
 * @i: Channel id.
 * @object: The spectra to add (passed as #GObject).
 *
 * Adds a spectra object of specified id, setting qdata and connecting
 * signals.
 **/
static void
gwy_app_data_proxy_connect_spectra(GwyAppDataProxy *proxy,
                                   gint i,
                                   GtkTreeIter *iter,
                                   GObject *object)
{
    GQuark quark;

    gwy_app_data_proxy_add_object(&proxy->lists[PAGE_SPECTRA], i, iter, object);
    gwy_debug("%p: %d in %p", object, i, proxy->container);
    quark = gwy_app_get_spectra_key_for_id(i);
    g_object_set_qdata(object, container_quark, proxy->container);
    g_object_set_qdata(object, own_key_quark, GUINT_TO_POINTER(quark));

    g_signal_connect(object, "data-changed",
                     G_CALLBACK(gwy_app_data_proxy_spectra_changed), proxy);
}

/**
 * gwy_app_data_proxy_disconnect_spectra:
 * @proxy: Data proxy.
 * @iter: Tree iterator pointing to the spectra in @proxy's list store.
 *
 * Disconnects signals from a spectra object, removes qdata and finally
 * removes it from the data proxy list store.
 **/
static void
gwy_app_data_proxy_disconnect_spectra(GwyAppDataProxy *proxy,
                                      GtkTreeIter *iter)
{
    GObject *object;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_SPECTRA].store), iter,
                       MODEL_OBJECT, &object,
                       -1);
    gwy_debug("%p: from %p", object, proxy->container);
    g_object_set_qdata(object, container_quark, NULL);
    g_object_set_qdata(object, own_key_quark, NULL);
    g_signal_handlers_disconnect_by_func(object,
                                         gwy_app_data_proxy_spectra_changed,
                                         proxy);
    g_object_unref(object);
    gtk_list_store_remove(proxy->lists[PAGE_SPECTRA].store, iter);
}

/**
 * gwy_app_data_proxy_reconnect_spectra:
 * @proxy: Data proxy.
 * @iter: Tree iterator pointing to the spectra in @proxy's list store.
 * @object: The spectra object (passed as #GObject).
 *
 * Updates data proxy's list store when the spectra object is switched for
 * another spectra object.
 **/
static void
gwy_app_data_proxy_reconnect_spectra(GwyAppDataProxy *proxy,
                                     GtkTreeIter *iter,
                                     GObject *object)
{
    GObject *old;

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_SPECTRA].store), iter,
                       MODEL_OBJECT, &old,
                       -1);
    g_signal_handlers_disconnect_by_func(old,
                                         gwy_app_data_proxy_spectra_changed,
                                         proxy);
    gwy_app_data_proxy_switch_object_data(proxy, old, object);
    gtk_list_store_set(proxy->lists[PAGE_SPECTRA].store, iter,
                       MODEL_OBJECT, object,
                       -1);
    g_signal_connect(object, "data-changed",
                     G_CALLBACK(gwy_app_data_proxy_spectra_changed), proxy);
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
    GtkTreeIter iter;
    GObject *object;
    gint i;

    strkey = g_quark_to_string(quark);
    i = _gwy_app_analyse_data_key(strkey, &type, NULL);
    if (i < 0)
        return;

    switch (type) {
        case KEY_IS_DATA:
        gwy_debug("Found data %d (%s)", i, strkey);
        g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
        object = g_value_get_object(gvalue);
        g_return_if_fail(GWY_IS_DATA_FIELD(object));
        gwy_app_data_proxy_connect_channel(proxy, i, &iter, object);
        break;

        case KEY_IS_GRAPH:
        gwy_debug("Found graph %d (%s)", i, strkey);
        g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
        object = g_value_get_object(gvalue);
        g_return_if_fail(GWY_IS_GRAPH_MODEL(object));
        gwy_app_data_proxy_connect_graph(proxy, i, &iter, object);
        break;

        case KEY_IS_SPECTRA:
        gwy_debug("Found spectra %d (%s)", i, strkey);
        g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
        object = g_value_get_object(gvalue);
        g_return_if_fail(GWY_IS_SPECTRA(object));
        gwy_app_data_proxy_connect_spectra(proxy, i, &iter, object);
        break;

        case KEY_IS_MASK:
        /* FIXME */
        gwy_debug("Found mask %d (%s)", i, strkey);
        g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
        object = g_value_get_object(gvalue);
        g_return_if_fail(GWY_IS_DATA_FIELD(object));
        break;

        case KEY_IS_SHOW:
        /* FIXME */
        gwy_debug("Found presentation %d (%s)", i, strkey);
        g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
        object = g_value_get_object(gvalue);
        g_return_if_fail(GWY_IS_DATA_FIELD(object));
        break;

        case KEY_IS_SELECT:
        g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
        object = g_value_get_object(gvalue);
        g_return_if_fail(GWY_IS_SELECTION(object));
        break;

        default:
        break;
    }
}

/**
 * gwy_app_data_proxy_visible_count:
 * @proxy: Data proxy.
 *
 * Calculates the total number of visible objects in all data proxy object
 * lists.
 *
 * Returns: The total number of visible objects.
 **/
static inline gint
gwy_app_data_proxy_visible_count(GwyAppDataProxy *proxy)
{
    gint i, n = 0;

    for (i = 0; i < NPAGES; i++) {
        n += proxy->lists[i].visible_count;
    }

    g_assert(n >= 0);
    gwy_debug("%p total visible_count: %d", proxy, n);

    return n;
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

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gtk_tree_model_get(model, &iter, column, &object, -1);
            g_signal_handlers_disconnect_by_func(object, func, data);
            g_object_unref(object);
        } while (gtk_tree_model_iter_next(model, &iter));
    }

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

    gwy_debug("looking for objid %d", i);
    if (i < 0)
        return FALSE;

    model = GTK_TREE_MODEL(store);
    if (!gtk_tree_model_get_iter_first(model, iter))
        return FALSE;

    do {
        gtk_tree_model_get(model, iter, MODEL_ID, &objid, -1);
        gwy_debug("found objid %d", objid);
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
    GwyAppDataList *list;
    const gchar *strkey;
    GwyAppKeyType type;
    GtkTreeIter iter;
    GwyDataView *data_view = NULL;
    gboolean found;
    GList *item;
    gint id, pageno = -1;

    strkey = g_quark_to_string(quark);
    id = _gwy_app_analyse_data_key(strkey, &type, NULL);
    if (id < 0) {
        if (type == KEY_IS_FILENAME) {
            gwy_app_data_browser_update_filename(proxy);
            gwy_app_data_proxy_update_window_titles(proxy);
        }
        return;
    }

    switch (type) {
        case KEY_IS_DATA:
        gwy_container_gis_object(data, quark, &object);
        pageno = PAGE_CHANNELS;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        gwy_debug("Channel <%s>: %s in container, %s in list store",
                  strkey,
                  object ? "present" : "missing",
                  found ? "present" : "missing");
        g_return_if_fail(object || found);
        if (object && !found)
            gwy_app_data_proxy_connect_channel(proxy, id, &iter, object);
        else if (!object && found)
            gwy_app_data_proxy_disconnect_channel(proxy, &iter);
        else {
            gwy_app_data_proxy_reconnect_channel(proxy, &iter, object);
            gwy_list_store_row_changed(list->store, &iter, NULL, -1);
        }
        /* Prevent thumbnail update */
        if (!object)
            pageno = -1;
        break;

        case KEY_IS_GRAPH:
        gwy_container_gis_object(data, quark, &object);
        pageno = PAGE_GRAPHS;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        gwy_debug("Graph <%s>: %s in container, %s in list store",
                  strkey,
                  object ? "present" : "missing",
                  found ? "present" : "missing");
        g_return_if_fail(object || found);
        if (object && !found)
            gwy_app_data_proxy_connect_graph(proxy, id, &iter, object);
        else if (!object && found)
            gwy_app_data_proxy_disconnect_graph(proxy, &iter);
        else {
            gwy_app_data_proxy_reconnect_graph(proxy, &iter, object);
            gwy_list_store_row_changed(list->store, &iter, NULL, -1);
        }
        /* Prevent thumbnail update */
        if (!object)
            pageno = -1;
        break;

        case KEY_IS_SPECTRA:
        gwy_container_gis_object(data, quark, &object);
        pageno = PAGE_SPECTRA;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        gwy_debug("Spectra <%s>: %s in container, %s in list store",
                  strkey,
                  object ? "present" : "missing",
                  found ? "present" : "missing");
        g_return_if_fail(object || found);
        if (object && !found)
            gwy_app_data_proxy_connect_spectra(proxy, id, &iter, object);
        else if (!object && found)
            gwy_app_data_proxy_disconnect_spectra(proxy, &iter);
        else {
            gwy_app_data_proxy_reconnect_spectra(proxy, &iter, object);
            gwy_list_store_row_changed(list->store, &iter, NULL, -1);
        }
        /* Prevent thumbnail update */
        if (!object)
            pageno = -1;
        break;

        case KEY_IS_MASK:
        gwy_container_gis_object(data, quark, &object);
        pageno = PAGE_CHANNELS;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        if (found) {
            gwy_list_store_row_changed(proxy->lists[PAGE_CHANNELS].store,
                                       &iter, NULL, -1);
            gtk_tree_model_get(GTK_TREE_MODEL(list->store), &iter,
                               MODEL_WIDGET, &data_view,
                               -1);
        }
        /* XXX: This is not a good place to do that, DataProxy should be
         * non-GUI */
        if (data_view) {
            gwy_app_data_browser_sync_mask(data, quark, data_view);
            g_object_unref(data_view);
        }
        /* Prevent thumbnail update */
        if (!found)
            pageno = -1;
        break;

        case KEY_IS_SHOW:
        gwy_container_gis_object(data, quark, &object);
        pageno = PAGE_CHANNELS;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        if (found) {
            gwy_list_store_row_changed(proxy->lists[PAGE_CHANNELS].store,
                                       &iter, NULL, -1);
            gtk_tree_model_get(GTK_TREE_MODEL(list->store), &iter,
                               MODEL_WIDGET, &data_view,
                               -1);
        }
        /* XXX: This is not a good place to do that, DataProxy should be
         * non-GUI */
        if (data_view) {
            gwy_app_data_browser_sync_show(data, quark, data_view);
            gwy_app_update_data_range_type(data_view, id);
            g_object_unref(data_view);
        }
        /* Prevent thumbnail update */
        if (!found)
            pageno = -1;
        break;

        case KEY_IS_TITLE:
        pageno = PAGE_CHANNELS;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        if (found)
            gtk_tree_model_get(GTK_TREE_MODEL(list->store), &iter,
                               MODEL_WIDGET, &data_view,
                               -1);
        /* XXX: This is not a good place to do that, DataProxy should be
         * non-GUI */
        if (data_view) {
            gwy_app_update_data_window_title(data_view, id);
            g_object_unref(data_view);
        }
        if ((item = gwy_app_data_proxy_get_3d(proxy, id))) {
            GwyApp3DAssociation *assoc = (GwyApp3DAssociation*)item->data;

            gwy_app_update_3d_window_title(assoc->window, id);
        }
        /* Prevent thumbnail update */
        pageno = -1;
        break;

        case KEY_IS_RANGE_TYPE:
        pageno = PAGE_CHANNELS;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        if (found)
            gtk_tree_model_get(GTK_TREE_MODEL(list->store), &iter,
                               MODEL_WIDGET, &data_view,
                               -1);
        /* XXX: This is not a good place to do that, DataProxy should be
         * non-GUI */
        if (data_view) {
            gwy_app_update_data_range_type(data_view, id);
            g_object_unref(data_view);
        }
        /* Prevent thumbnail update */
        if (!found)
            pageno = -1;
        break;

        case KEY_IS_PALETTE:
        case KEY_IS_MASK_COLOR:
        case KEY_IS_REAL_SQUARE:
        pageno = PAGE_CHANNELS;
        list = &proxy->lists[pageno];
        found = gwy_app_data_proxy_find_object(list->store, id, &iter);
        /* Prevent thumbnail update */
        if (!found)
            pageno = -1;
        break;

        default:
        break;
    }

    if (pageno == -1)
        return;

    /* XXX: This code asserts list and iter was set above. */
    gtk_list_store_set(list->store, &iter,
                       MODEL_TIMESTAMP, gwy_get_timestamp(),
                       -1);
}

/**
 * gwy_app_data_proxy_finalize:
 * @user_data: A data proxy.
 *
 * Finalizes a data proxy, which was already removed from the data browser.
 *
 * Usually called in idle loop as things do not like being finalized inside
 * their signal callbacks.
 *
 * Returns: Always %FALSE.
 **/
static gboolean
gwy_app_data_proxy_finalize(gpointer user_data)
{
    GwyAppDataProxy *proxy = (GwyAppDataProxy*)user_data;
    GwyAppDataBrowser *browser;

    proxy->finalize_id = 0;

    if (gwy_app_data_proxy_visible_count(proxy)) {
        g_assert(gwy_app_data_browser_get_proxy(gwy_app_data_browser,
                                                proxy->container, FALSE));
        return FALSE;
    }

    gwy_debug("Freeing proxy for Container %p", proxy->container);

    browser = gwy_app_data_browser;
    if (browser == proxy->parent) {
        /* FIXME: This is crude. */
        if (browser->current == proxy)
            gwy_app_data_browser_switch_data(NULL);

        browser->proxy_list = g_list_remove(browser->proxy_list, proxy);
    }

    gwy_app_data_proxy_finalize_list
        (GTK_TREE_MODEL(proxy->lists[PAGE_CHANNELS].store),
         MODEL_OBJECT, &gwy_app_data_proxy_channel_changed, proxy);
    gwy_app_data_proxy_finalize_list
        (GTK_TREE_MODEL(proxy->lists[PAGE_GRAPHS].store),
         MODEL_OBJECT, &gwy_app_data_proxy_graph_changed, proxy);
    gwy_app_data_proxy_finalize_list
        (GTK_TREE_MODEL(proxy->lists[PAGE_SPECTRA].store),
         MODEL_OBJECT, &gwy_app_data_proxy_spectra_changed, proxy);

    g_object_unref(proxy->container);
    g_free(proxy);

    /* Ask for removal if used in idle function */
    return FALSE;
}

static void
gwy_app_data_proxy_queue_finalize(GwyAppDataProxy *proxy)
{
    gwy_debug("proxy %p", proxy);

    if (proxy->finalize_id)
        return;

    proxy->finalize_id = g_idle_add(&gwy_app_data_proxy_finalize, proxy);
}

/**
 * gwy_app_data_proxy_maybe_finalize:
 * @proxy: Data proxy.
 *
 * Checks whether there are any visible objects in a data proxy.
 *
 * If there are none, it queues finalization.  However, if @keep_invisible
 * flag is set on the proxy, it is not finalized.
 **/
static void
gwy_app_data_proxy_maybe_finalize(GwyAppDataProxy *proxy)
{
    gwy_debug("proxy %p", proxy);

    if (!proxy->keep_invisible
        && gwy_app_data_proxy_visible_count(proxy) == 0) {
        gwy_app_data_proxy_destroy_all_3d(proxy);
        gwy_app_data_proxy_queue_finalize(proxy);
    }
}

/**
 * gwy_app_data_proxy_list_setup:
 * @list: A data proxy list.
 *
 * Creates the list store of a data proxy object list and performs some basic
 * setup.
 *
 * XXX: The @last field is set to -1, however for historical reasons graphs
 * are 1-based and therefore graph lists need to set it to 0.
 **/
static void
gwy_app_data_proxy_list_setup(GwyAppDataList *list)
{
    list->store = gtk_list_store_new(MODEL_N_COLUMNS,
                                     G_TYPE_INT,
                                     G_TYPE_OBJECT,
                                     G_TYPE_OBJECT,
                                     G_TYPE_DOUBLE,
                                     GDK_TYPE_PIXBUF);
    gwy_debug_objects_creation(G_OBJECT(list->store));
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list->store),
                                         MODEL_ID, GTK_SORT_ASCENDING);
    list->last = -1;
    list->active = -1;
    list->visible_count = 0;
}

/**
 * gwy_app_data_list_update_last:
 * @list: A data proxy list.
 * @empty_last: The value to set @last item to when there are no objects.
 *
 * Updates the value of @last field to the actual last object id.
 *
 * This function is intended to be used after object removal to keep the
 * object id set compact (and the id numbers low).
 **/
static void
gwy_app_data_list_update_last(GwyAppDataList *list,
                              gint empty_last)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint id, max = empty_last;

    model = GTK_TREE_MODEL(list->store);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gtk_tree_model_get(model, &iter, MODEL_ID, &id, -1);
            if (id > max)
                max = id;
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    gwy_debug("new last item id: %d", max);
    list->last = max;
}

static void
gwy_app_data_browser_update_filename(GwyAppDataProxy *proxy)
{
    GwyAppDataBrowser *browser;
    const guchar *filename;
    gchar *s;

    browser = gwy_app_data_browser;
    if (!browser->window)
        return;

    if (gwy_container_gis_string(proxy->container, filename_quark, &filename))
        s = g_path_get_basename(filename);
    else
        s = g_strdup_printf("%s %d", _("Untitled"), proxy->untitled_no);
    gtk_label_set_text(GTK_LABEL(browser->filename), s);
    g_free(s);
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
    guint i;

    gwy_debug("Creating proxy for Container %p", data);
    g_object_ref(data);
    proxy = g_new0(GwyAppDataProxy, 1);
    proxy->container = data;
    proxy->parent = browser;
    proxy->untitled_no = ++browser->untitled_counter;
    browser->proxy_list = g_list_prepend(browser->proxy_list, proxy);
    g_signal_connect_after(data, "item-changed",
                           G_CALLBACK(gwy_app_data_proxy_item_changed), proxy);

    for (i = 0; i < NPAGES; i++) {
        gwy_app_data_proxy_list_setup(&proxy->lists[i]);
        g_object_set_qdata(G_OBJECT(proxy->lists[i].store),
                           page_id_quark, GUINT_TO_POINTER(i + 1));
    }
    /* For historical reasons, graphs are numbered from 1 */
    proxy->lists[PAGE_GRAPHS].last = 0;

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

/**************************************************************************
 *
 * All treeviews
 *
 **************************************************************************/

/**
 * gwy_app_data_list_row_activated:
 * @treeview: Tree view representing a data browser object list.
 * @path: Path of the activated row in the tree view.
 * @column: Activated tree column.
 * @user_data: Unused.
 *
 * Starts editing of object title when its row is activated.
 **/
static void
gwy_app_data_list_row_activated(GtkTreeView *treeview,
                                GtkTreePath *path,
                                GtkTreeViewColumn *column,
                                G_GNUC_UNUSED gpointer user_data)
{
    GList *list;
    GtkCellRenderer *renderer;
    const gchar *col_id;

    /* Only do anything if the "title" column was activated */
    col_id = g_object_get_qdata(G_OBJECT(column), column_id_quark);
    if (!col_id || !gwy_strequal(col_id, "title"))
        return;

    list = gtk_tree_view_column_get_cell_renderers(column);
    if (g_list_length(list) > 1)
        g_warning("Too many cell renderers in title column");

    renderer = GTK_CELL_RENDERER(list->data);
    g_list_free(list);
    g_return_if_fail(GTK_IS_CELL_RENDERER_TEXT(renderer));

    /* The trick to make title editable on double click is to enable edit
     * here and disable it again in "edited" signal handler of the
     * renderer (this is set in particular lists handlers) */
    g_object_set(renderer, "editable", TRUE, "editable-set", TRUE, NULL);
    gtk_tree_view_set_cursor(treeview, path, column, TRUE);
}

static void
gwy_app_data_list_disable_edit(GtkCellRenderer *renderer)
{
    gwy_debug("%p", renderer);
    g_object_set(renderer, "editable", FALSE, NULL);
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
    g_object_set(renderer, "active", widget != NULL, NULL);
    gwy_object_unref(widget);
}

static void
gwy_app_data_browser_selection_changed(GtkTreeSelection *selection,
                                       GwyAppDataBrowser *browser)
{
    gint pageno;
    gboolean any;

    pageno = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(selection),
                                                page_id_quark)) - 1;
    if (pageno != browser->active_page)
        return;

    any = gtk_tree_selection_get_selected(selection, NULL, NULL);
    gwy_debug("Any: %d (page %d)", any, pageno);

    gwy_sensitivity_group_set_state(browser->sensgroup,
                                    SENS_OBJECT, any ? SENS_OBJECT : 0);
}

/**************************************************************************
 *
 * Channels treeview
 *
 **************************************************************************/

static void
gwy_app_data_browser_channel_render_title(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                          GtkCellRenderer *renderer,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          gpointer userdata)
{
    GwyAppDataBrowser *browser = (GwyAppDataBrowser*)userdata;
    const guchar *title;
    GwyContainer *data;
    gint channel;

    /* XXX: browser->current must match what is visible in the browser */
    data = browser->current->container;
    gtk_tree_model_get(model, iter, MODEL_ID, &channel, -1);
    title = gwy_app_data_browser_figure_out_channel_title(data, channel);
    g_object_set(renderer, "text", title, NULL);
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
    g_snprintf(key, sizeof(key), "/%d/mask", channel);
    has_mask = gwy_container_contains_by_name(data, key);
    g_snprintf(key, sizeof(key), "/%d/show", channel);
    has_show = gwy_container_contains_by_name(data, key);

    g_snprintf(key, sizeof(key), "%s%s",
               has_mask ? "M" : "",
               has_show ? "P" : "");

    g_object_set(renderer, "text", key, NULL);
}

static void
gwy_app_data_browser_render_channel(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                    GtkCellRenderer *renderer,
                                    GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    G_GNUC_UNUSED gpointer userdata)
{
    GwyContainer *container;
    GObject *object;
    GdkPixbuf *pixbuf;
    gdouble timestamp, *pbuf_timestamp = NULL;
    gint id;

    gtk_tree_model_get(model, iter,
                       MODEL_ID, &id,
                       MODEL_OBJECT, &object,
                       MODEL_TIMESTAMP, &timestamp,
                       MODEL_THUMBNAIL, &pixbuf,
                       -1);

    container = g_object_get_qdata(object, container_quark);
    g_object_unref(object);

    if (pixbuf) {
        pbuf_timestamp = (gdouble*)g_object_get_data(G_OBJECT(pixbuf),
                                                     "timestamp");
        if (*pbuf_timestamp >= timestamp) {
            g_object_set(renderer, "pixbuf", pixbuf, NULL);
            g_object_unref(pixbuf);
            return;
        }
    }

    pixbuf = gwy_app_get_channel_thumbnail(container, id,
                                           THUMB_SIZE, THUMB_SIZE);
    pbuf_timestamp = g_new(gdouble, 1);
    *pbuf_timestamp = gwy_get_timestamp();
    g_object_set_data_full(G_OBJECT(pixbuf), "timestamp", pbuf_timestamp,
                           g_free);
    gtk_list_store_set(GTK_LIST_STORE(model), iter,
                       MODEL_THUMBNAIL, pixbuf,
                       -1);
    g_object_set(renderer, "pixbuf", pixbuf, NULL);
    g_object_unref(pixbuf);
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
    GwyAppDataList *list;
    GwyAppKeyType type;
    GwyContainer *data;
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GtkTreeIter iter;
    const gchar *strkey;
    GObject *object;
    GQuark quark;
    gint i;

    gwy_debug("Data window %p deleted", data_window);
    data_view = gwy_data_window_get_data_view(data_window);
    data = gwy_data_view_get_data(data_view);
    layer = gwy_data_view_get_base_layer(data_view);
    strkey = gwy_pixmap_layer_get_data_key(layer);
    quark = g_quark_from_string(strkey);
    g_return_val_if_fail(data && quark, TRUE);
    object = gwy_container_get_object(data, quark);

    i = _gwy_app_analyse_data_key(strkey, &type, NULL);
    g_return_val_if_fail(i >= 0 && type == KEY_IS_DATA, TRUE);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    list = &proxy->lists[PAGE_CHANNELS];
    if (!gwy_app_data_proxy_find_object(list->store, i, &iter)) {
        g_critical("Cannot find data field %p (%d)", object, i);
        return TRUE;
    }

    gwy_app_data_proxy_channel_set_visible(proxy, &iter, FALSE);
    gwy_app_data_proxy_maybe_finalize(proxy);

    return TRUE;
}

static gboolean
gwy_app_graph_window_dnd_curve_received(GtkWidget *destwidget,
                                        GtkTreeModel *model,
                                        GtkTreePath *path)
{
    GwyGraphWindow *destwindow, *srcwindow;
    GwyGraphModel *destmodel, *srcmodel;
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *destunit, *srcunit;
    const gint *indices;
    GtkWidget *w;
    gboolean ok;

    srcwindow = GWY_GRAPH_WINDOW(g_object_get_qdata(G_OBJECT(model),
                                                    graph_window_quark));
    destwindow = GWY_GRAPH_WINDOW(destwidget);

    w = gwy_graph_window_get_graph(srcwindow);
    srcmodel = gwy_graph_get_model(GWY_GRAPH(w));
    w = gwy_graph_window_get_graph(destwindow);
    destmodel = gwy_graph_get_model(GWY_GRAPH(w));

    /* Ignore drops to the same graph */
    if (srcmodel == destmodel)
        return FALSE;

    /* Check units compatibility */
    g_object_get(srcmodel, "si-unit-x", &srcunit, NULL);
    g_object_get(destmodel, "si-unit-x", &destunit, NULL);
    ok = gwy_si_unit_equal(srcunit, destunit);
    g_object_unref(srcunit);
    g_object_unref(destunit);
    if (!ok)
        return FALSE;

    g_object_get(srcmodel, "si-unit-y", &srcunit, NULL);
    g_object_get(destmodel, "si-unit-y", &destunit, NULL);
    ok = gwy_si_unit_equal(srcunit, destunit);
    g_object_unref(srcunit);
    g_object_unref(destunit);
    if (!ok)
        return FALSE;

    /* Copy curve */
    indices = gtk_tree_path_get_indices(path);
    gcmodel = gwy_graph_model_get_curve(srcmodel, indices[0]);
    gcmodel = gwy_graph_curve_model_duplicate(gcmodel);
    gwy_graph_model_add_curve(destmodel, gcmodel);
    g_object_unref(gcmodel);

    return TRUE;
}

static void
gwy_app_window_dnd_data_received(GtkWidget *window,
                                 GdkDragContext *context,
                                 G_GNUC_UNUSED gint x,
                                 G_GNUC_UNUSED gint y,
                                 GtkSelectionData *data,
                                 G_GNUC_UNUSED guint info,
                                 guint time_,
                                 gpointer user_data)
{
    GwyAppDataBrowser *browser = (GwyAppDataBrowser*)user_data;
    GwyAppDataProxy *srcproxy, *destproxy;
    GwyContainer *container = NULL;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    guint pageno;

    if (!gtk_tree_get_row_drag_data(data, &model, &path)) {
        g_warning("Cannot get row drag data");
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }

    window = gtk_widget_get_ancestor(window, GTK_TYPE_WINDOW);
    if (GWY_IS_GRAPH_WINDOW(window)
        && g_object_get_qdata(G_OBJECT(model), graph_window_quark)) {
        gboolean ok;

        ok = gwy_app_graph_window_dnd_curve_received(window, model, path);
        gtk_tree_path_free(path);
        gtk_drag_finish(context, ok, FALSE, time_);
        return;
    }

    srcproxy = browser->current;
    if (!(pageno = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(model),
                                                       page_id_quark)))) {
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }
    pageno--;

    if (!gtk_tree_model_get_iter(model, &iter, path)) {
        g_warning("Received data browser drop of a nonexistent path");
        gtk_drag_finish(context, FALSE, FALSE, time_);
        return;
    }
    gtk_tree_path_free(path);

    if (GWY_IS_DATA_WINDOW(window)) {
        container = gwy_data_window_get_data(GWY_DATA_WINDOW(window));
    }
    else if (GWY_IS_GRAPH_WINDOW(window)) {
        GtkWidget *graph;
        GObject *object;

        graph = gwy_graph_window_get_graph(GWY_GRAPH_WINDOW(window));
        object = G_OBJECT(gwy_graph_get_model(GWY_GRAPH(graph)));
        container = g_object_get_qdata(object, container_quark);
    }

    /* Foreign tree models */
    if (pageno == PAGE_NOPAGE) {
        gwy_app_data_browser_copy_other(model, &iter, window, container);
    }
    else if (container) {
        destproxy = gwy_app_data_browser_get_proxy(browser, container, FALSE);
        gwy_app_data_browser_copy_object(srcproxy, pageno, model, &iter,
                                         destproxy);
    }
    else
        g_warning("Cannot determine drop target GwyContainer");

    gtk_drag_finish(context, TRUE, FALSE, time_);
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
    GwyContainer *current_data;
    gboolean has_show;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &current_data, 0);
    if (data != current_data)
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
    GwyContainer *current_data;
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
        i = _gwy_app_analyse_data_key(strkey, &type, NULL);
        g_return_if_fail(i >= 0 && type == KEY_IS_MASK);
        gwy_app_data_proxy_setup_mask(data, i);
        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, strkey);
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), strkey);
        gwy_data_view_set_alpha_layer(data_view, layer);
    }
    else if (!has_dfield && has_layer)
        gwy_data_view_set_alpha_layer(data_view, NULL);

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &current_data, 0);
    if (has_dfield != has_layer
        && data == current_data) {
        gwy_debug("Syncing mask sens flags");
        gwy_app_sensitivity_set_state(GWY_MENU_FLAG_DATA_MASK,
                                      has_dfield ? GWY_MENU_FLAG_DATA_MASK : 0);
    }
}

/**
 * gwy_app_data_browser_create_channel:
 * @browser: A data browser.
 * @id: The channel id.
 *
 * Creates a data window for a data field when its visibility is switched on.
 *
 * This is actually `make visible', should not be used outside
 * gwy_app_data_proxy_channel_set_visible().
 *
 * Returns: The data view (NOT data window).
 **/
static GtkWidget*
gwy_app_data_browser_create_channel(GwyAppDataBrowser *browser,
                                    GwyAppDataProxy *proxy,
                                    gint id)
{
    static const GtkTargetEntry dnd_target_table[] = { GTK_TREE_MODEL_ROW };

    GtkWidget *data_view, *data_window;
    GObject *dfield = NULL;
    GwyPixmapLayer *layer;
    GwyLayerBasic *layer_basic;
    GQuark quark;
    gchar key[40];
    guint len;

    g_snprintf(key, sizeof(key), "/%d/data", id);
    gwy_container_gis_object_by_name(proxy->container, key, &dfield);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);

    layer = gwy_layer_basic_new();
    layer_basic = GWY_LAYER_BASIC(layer);
    gwy_pixmap_layer_set_data_key(layer, key);
    g_snprintf(key, sizeof(key), "/%d/show", id);
    gwy_layer_basic_set_presentation_key(layer_basic, key);
    g_snprintf(key, sizeof(key), "/%d/base", id);
    gwy_layer_basic_set_min_max_key(layer_basic, key);
    len = strlen(key);
    g_strlcat(key, "/range-type", sizeof(key));
    gwy_layer_basic_set_range_type_key(layer_basic, key);
    key[len] = '\0';
    g_strlcat(key, "/palette", sizeof(key));
    gwy_layer_basic_set_gradient_key(layer_basic, key);

    data_view = gwy_data_view_new(proxy->container);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(data_view),
                                  gwy_pixmap_layer_get_data_key(layer));
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(data_view), layer);

    data_window = gwy_data_window_new(GWY_DATA_VIEW(data_view));
    gwy_app_update_data_window_title(GWY_DATA_VIEW(data_view), id);

    gwy_app_data_proxy_update_visibility(dfield, TRUE);
    g_signal_connect_swapped(data_window, "focus-in-event",
                             G_CALLBACK(gwy_app_data_browser_select_data_view2),
                             data_view);
    g_signal_connect(data_window, "delete-event",
                     G_CALLBACK(gwy_app_data_browser_channel_deleted), NULL);
    _gwy_app_data_window_setup(GWY_DATA_WINDOW(data_window));

    /* Channel DnD */
    gtk_drag_dest_set(data_window, GTK_DEST_DEFAULT_ALL,
                      dnd_target_table, G_N_ELEMENTS(dnd_target_table),
                      GDK_ACTION_COPY);
    g_signal_connect(data_window, "drag-data-received",
                     G_CALLBACK(gwy_app_window_dnd_data_received), browser);

    g_snprintf(key, sizeof(key), "/%d/mask", id);
    quark = g_quark_from_string(key);
    gwy_app_data_browser_sync_mask(proxy->container, quark,
                                   GWY_DATA_VIEW(data_view));
    gwy_app_update_data_range_type(GWY_DATA_VIEW(data_view), id);

    /* FIXME: A silly place for this? */
    gwy_app_data_browser_set_file_present(browser, TRUE);
    gtk_widget_show_all(data_window);
    _gwy_app_data_view_set_current(GWY_DATA_VIEW(data_view));

    return data_view;
}

static void
gwy_app_update_data_range_type(GwyDataView *data_view,
                               gint id)
{
    GtkWidget *data_window, *widget;
    GwyPixmapLayer *layer;
    GwyColorAxis *color_axis;
    GwyContainer *data;
    GwyTicksStyle ticks_style;
    gboolean show_labels;
    gchar key[40];

    data_window = gtk_widget_get_ancestor(GTK_WIDGET(data_view),
                                          GWY_TYPE_DATA_WINDOW);
    if (!data_window) {
        g_warning("GwyDataView has no GwyDataWindow ancestor");
        return;
    }

    widget = gwy_data_window_get_color_axis(GWY_DATA_WINDOW(data_window));
    color_axis = GWY_COLOR_AXIS(widget);
    data = gwy_data_view_get_data(data_view);

    g_snprintf(key, sizeof(key), "/%d/show", id);

    if (gwy_container_contains_by_name(data, key)) {
        ticks_style = GWY_TICKS_STYLE_CENTER;
        show_labels = FALSE;
    }
    else {
        layer = gwy_data_view_get_base_layer(data_view);
        switch (gwy_layer_basic_get_range_type(GWY_LAYER_BASIC(layer))) {
            case GWY_LAYER_BASIC_RANGE_FULL:
            case GWY_LAYER_BASIC_RANGE_FIXED:
            case GWY_LAYER_BASIC_RANGE_AUTO:
            ticks_style = GWY_TICKS_STYLE_AUTO;
            show_labels = TRUE;
            break;

            case GWY_LAYER_BASIC_RANGE_ADAPT:
            ticks_style = GWY_TICKS_STYLE_NONE;
            show_labels = TRUE;
            break;

            default:
            g_warning("Unknown range type");
            ticks_style = GWY_TICKS_STYLE_NONE;
            show_labels = FALSE;
            break;
        }
    }

    gwy_color_axis_set_ticks_style(color_axis, ticks_style);
    gwy_color_axis_set_labels_visible(color_axis, show_labels);
}

static void
gwy_app_update_data_window_title(GwyDataView *data_view,
                                 gint id)
{
    GtkWidget *data_window;
    GwyContainer *data;
    const gchar *ctitle;
    const guchar *filename;
    gchar *title, *bname;

    data_window = gtk_widget_get_ancestor(GTK_WIDGET(data_view),
                                          GWY_TYPE_DATA_WINDOW);
    if (!data_window) {
        g_warning("GwyDataView has no GwyDataWindow ancestor");
        return;
    }

    data = gwy_data_view_get_data(data_view);
    ctitle = gwy_app_data_browser_figure_out_channel_title(data, id);
    if (gwy_container_gis_string(data, filename_quark, &filename)) {
        bname = g_path_get_basename(filename);
        title = g_strdup_printf("%s [%s]", bname, ctitle);
        g_free(bname);
    }
    else {
        GwyAppDataBrowser *browser;
        GwyAppDataProxy *proxy;

        browser = gwy_app_get_data_browser();
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
        title = g_strdup_printf("%s %d [%s]",
                                _("Untitled"), proxy->untitled_no, ctitle);
    }
    gwy_data_window_set_data_name(GWY_DATA_WINDOW(data_window), title);
    g_free(title);
}

static void
gwy_app_data_proxy_update_window_titles(GwyAppDataProxy *proxy)
{
    GwyDataView *data_view;
    GwyAppDataList *list;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GList *item;
    gint id;

    list = &proxy->lists[PAGE_CHANNELS];
    model = GTK_TREE_MODEL(list->store);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gtk_tree_model_get(model, &iter,
                               MODEL_ID, &id,
                               MODEL_WIDGET, &data_view,
                               -1);
            if (data_view) {
                gwy_app_update_data_window_title(data_view, id);
                g_object_unref(data_view);
            }
            if ((item = gwy_app_data_proxy_get_3d(proxy, id))) {
                GwyApp3DAssociation *assoc = (GwyApp3DAssociation*)item->data;

                gwy_app_update_3d_window_title(assoc->window, id);
            }
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

static gboolean
gwy_app_data_proxy_channel_set_visible(GwyAppDataProxy *proxy,
                                       GtkTreeIter *iter,
                                       gboolean visible)
{
    GwyAppDataList *list;
    GtkTreeModel *model;
    GtkWidget *widget, *window, *succ;
    GObject *object;
    gint id;

    list = &proxy->lists[PAGE_CHANNELS];
    model = GTK_TREE_MODEL(list->store);

    gtk_tree_model_get(model, iter,
                       MODEL_WIDGET, &widget,
                       MODEL_OBJECT, &object,
                       MODEL_ID, &id,
                       -1);
    if (visible == (widget != NULL)) {
        g_object_unref(object);
        gwy_object_unref(widget);
        return FALSE;
    }

    if (visible) {
        widget = gwy_app_data_browser_create_channel(proxy->parent, proxy, id);
        gtk_list_store_set(list->store, iter, MODEL_WIDGET, widget, -1);
        list->visible_count++;
    }
    else {
        gwy_app_data_proxy_update_visibility(object, FALSE);
        window = gtk_widget_get_ancestor(widget, GWY_TYPE_DATA_WINDOW);
        succ = gwy_app_widget_queue_manage(widget, TRUE);
        gtk_widget_destroy(window);
        gtk_list_store_set(list->store, iter, MODEL_WIDGET, NULL, -1);
        g_object_unref(widget);
        list->visible_count--;

        /* FIXME */
        if (succ)
            gwy_app_data_browser_select_data_view(GWY_DATA_VIEW(succ));
        else
            _gwy_app_data_view_set_current(NULL);
    }
    g_object_unref(object);

    gwy_debug("visible_count: %d", list->visible_count);

    return TRUE;
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
    gboolean active, toggled;

    gwy_debug("Toggled channel row %s", path_str);
    proxy = browser->current;
    g_return_if_fail(proxy);

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(proxy->lists[PAGE_CHANNELS].store);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    active = gtk_cell_renderer_toggle_get_active(renderer);
    toggled = gwy_app_data_proxy_channel_set_visible(proxy, &iter, !active);
    g_assert(toggled);

    gwy_app_data_proxy_maybe_finalize(proxy);
}

static void
gwy_app_data_browser_channel_name_edited(GtkCellRenderer *renderer,
                                         const gchar *strpath,
                                         const gchar *text,
                                         GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    gchar *title;
    gint id;

    g_return_if_fail(browser->current);
    proxy = browser->current;
    model = GTK_TREE_MODEL(proxy->lists[PAGE_CHANNELS].store);

    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter, MODEL_ID, &id, -1);
    title = g_strstrip(g_strdup(text));
    if (!*title) {
        g_free(title);
        gwy_app_set_data_field_title(proxy->container, id, NULL);
    }
    else {
        gchar key[32];

        g_snprintf(key, sizeof(key), "/%d/data/title", id);
        gwy_container_set_string_by_name(proxy->container, key, title);
    }

    gwy_app_data_list_disable_edit(renderer);
}

static GtkWidget*
gwy_app_data_browser_construct_channels(GwyAppDataBrowser *browser)
{
    static const GtkTargetEntry dnd_target_table[] = { GTK_TREE_MODEL_ROW };

    GtkWidget *retval;
    GtkTreeView *treeview;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    /* Construct the GtkTreeView that will display data channels */
    retval = gtk_tree_view_new();
    treeview = GTK_TREE_VIEW(retval);
    g_signal_connect(treeview, "row-activated",
                     G_CALLBACK(gwy_app_data_list_row_activated), NULL);

    /* Add the thumbnail column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Thumbnail", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_render_channel, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the visibility column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(renderer, "activatable", TRUE, NULL);
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
    g_object_set(renderer,
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "ellipsize-set", TRUE,
                 NULL);
    g_signal_connect(renderer, "edited",
                     G_CALLBACK(gwy_app_data_browser_channel_name_edited),
                     browser);
    g_signal_connect(renderer, "editing-canceled",
                     G_CALLBACK(gwy_app_data_list_disable_edit), NULL);
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_channel_render_title, browser, NULL);
    g_object_set_qdata(G_OBJECT(column), column_id_quark, "title");
    gtk_tree_view_append_column(treeview, column);

    /* Add the flags column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "width-chars", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes("Flags", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_channel_render_flags, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    gtk_tree_view_set_headers_visible(treeview, FALSE);

    /* Selection */
    selection = gtk_tree_view_get_selection(treeview);
    g_object_set_qdata(G_OBJECT(selection), page_id_quark,
                       GINT_TO_POINTER(PAGE_CHANNELS + 1));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_data_browser_selection_changed),
                     browser);

    /* DnD */
    gtk_tree_view_enable_model_drag_source(treeview,
                                           GDK_BUTTON1_MASK,
                                           dnd_target_table,
                                           G_N_ELEMENTS(dnd_target_table),
                                           GDK_ACTION_COPY);

    return retval;
}

/**************************************************************************
 *
 * Channels 3D
 *
 **************************************************************************/

static void
gwy_app_update_3d_window_title(Gwy3DWindow *window3d,
                               gint id)
{
    GtkWidget *view3d;
    GwyContainer *data;
    const gchar *ctitle;
    gchar *title;

    view3d = gwy_3d_window_get_3d_view(window3d);
    data = gwy_3d_view_get_data(GWY_3D_VIEW(view3d));
    ctitle = gwy_app_data_browser_figure_out_channel_title(data, id);
    title = g_strconcat("3D ", ctitle, NULL);
    gtk_window_set_title(GTK_WINDOW(window3d), title);
    g_free(title);
}

static GList*
gwy_app_data_proxy_find_3d(GwyAppDataProxy *proxy,
                           Gwy3DWindow *window3d)
{
    GList *l;

    for (l = proxy->associated3d; l; l = g_list_next(l)) {
        GwyApp3DAssociation *assoc = (GwyApp3DAssociation*)l->data;

        if (assoc->window == window3d)
            return l;
    }

    return NULL;
}

static GList*
gwy_app_data_proxy_get_3d(GwyAppDataProxy *proxy,
                          gint id)
{
    GList *l;

    for (l = proxy->associated3d; l; l = g_list_next(l)) {
        GwyApp3DAssociation *assoc = (GwyApp3DAssociation*)l->data;

        if (assoc->id == id)
            return l;
    }

    return NULL;
}

static gboolean
gwy_app_data_proxy_select_3d(Gwy3DWindow *window3d)
{
    gwy_app_widget_queue_manage(GTK_WIDGET(window3d), FALSE);

    return FALSE;
}

static void
gwy_app_data_proxy_3d_destroyed(Gwy3DWindow *window3d,
                                GwyAppDataProxy *proxy)
{
    GwyApp3DAssociation *assoc;
    GList *item;

    /* XXX: The return value is not useful for anything -- yet? */
    gwy_app_widget_queue_manage(GTK_WIDGET(window3d), TRUE);

    item = gwy_app_data_proxy_find_3d(proxy, window3d);
    g_return_if_fail(item);

    assoc = (GwyApp3DAssociation*)item->data;
    g_free(assoc);
    proxy->associated3d = g_list_delete_link(proxy->associated3d, item);
}

static void
gwy_app_data_proxy_channel_destroy_3d(GwyAppDataProxy *proxy,
                                      gint id)
{
    GwyApp3DAssociation *assoc;
    GList *l;

    l = gwy_app_data_proxy_get_3d(proxy, id);
    if (!l)
        return;

    proxy->associated3d = g_list_remove_link(proxy->associated3d, l);
    assoc = (GwyApp3DAssociation*)l->data;
    gwy_app_widget_queue_manage(GTK_WIDGET(assoc->window), TRUE);
    g_signal_handlers_disconnect_by_func(assoc->window,
                                         gwy_app_data_proxy_3d_destroyed,
                                         proxy);
    gtk_widget_destroy(GTK_WIDGET(assoc->window));
    g_free(assoc);
    g_list_free_1(l);
}

static void
gwy_app_data_proxy_destroy_all_3d(GwyAppDataProxy *proxy)
{
    while (proxy->associated3d) {
        GwyApp3DAssociation *assoc;

        assoc = (GwyApp3DAssociation*)proxy->associated3d->data;
        gwy_app_data_proxy_channel_destroy_3d(proxy, assoc->id);
    }
}

static GtkWidget*
gwy_app_data_browser_create_3d(G_GNUC_UNUSED GwyAppDataBrowser *browser,
                               GwyAppDataProxy *proxy,
                               gint id)
{
    GtkWidget *view3d, *window3d;
    GObject *dfield = NULL;
    GwyApp3DAssociation *assoc;
    gchar key[40];
    guint len;

    g_snprintf(key, sizeof(key), "/%d/data", id);
    gwy_container_gis_object_by_name(proxy->container, key, &dfield);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), NULL);

    view3d = gwy_3d_view_new(proxy->container);

    g_snprintf(key, sizeof(key), "/%d/", id);
    len = strlen(key);

    g_strlcat(key, "3d", sizeof(key));
    /* Since gwy_3d_view_set_setup_prefix() instantiates a new 3d setup if none
     * is present, we have to check whether any is present and create a new
     * one with user's defaults before calling this method.  After that we
     * cannot tell whether the 3d setup was in the container from previous
     * 3d views or it has been just created. */
    _gwy_app_3d_view_init_setup(proxy->container, key);
    gwy_3d_view_set_setup_prefix(GWY_3D_VIEW(view3d), key);

    key[len] = '\0';
    g_strlcat(key, "data", sizeof(key));
    gwy_3d_view_set_data_key(GWY_3D_VIEW(view3d), key);

    key[len] = '\0';
    g_strlcat(key, "3d/palette", sizeof(key));
    gwy_3d_view_set_gradient_key(GWY_3D_VIEW(view3d), key);

    key[len] = '\0';
    g_strlcat(key, "3d/material", sizeof(key));
    gwy_3d_view_set_material_key(GWY_3D_VIEW(view3d), key);

    window3d = gwy_3d_window_new(GWY_3D_VIEW(view3d));

    gwy_app_update_3d_window_title(GWY_3D_WINDOW(window3d), id);

    g_signal_connect(window3d, "focus-in-event",
                     G_CALLBACK(gwy_app_data_proxy_select_3d), NULL);
    g_signal_connect(window3d, "destroy",
                     G_CALLBACK(gwy_app_data_proxy_3d_destroyed), proxy);

    assoc = g_new(GwyApp3DAssociation, 1);
    assoc->window = GWY_3D_WINDOW(window3d);
    assoc->id = id;
    proxy->associated3d = g_list_prepend(proxy->associated3d, assoc);

    _gwy_app_3d_window_setup(GWY_3D_WINDOW(window3d));
    gwy_app_data_proxy_select_3d(GWY_3D_WINDOW(window3d));
    gtk_widget_show_all(window3d);

    return window3d;
}

/**
 * gwy_app_data_browser_show_3d:
 * @data: A data container.
 * @id: Channel id.
 *
 * Shows a 3D window displaying a channel.
 *
 * If a 3D window of the specified channel already exists, it is just presented
 * to the user.  If it does not exist, it is created.
 *
 * The caller must ensure 3D display is available, for example by checking
 * gwy_app_gl_is_ok().
 **/
void
gwy_app_data_browser_show_3d(GwyContainer *data,
                             gint id)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GtkWidget *window3d;
    GList *item;

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);

    if (gui_disabled)
        return;

    item = gwy_app_data_proxy_get_3d(proxy, id);
    if (item)
        window3d = GTK_WIDGET(((GwyApp3DAssociation*)item->data)->window);
    else
        window3d = gwy_app_data_browser_create_3d(browser, proxy, id);
    g_return_if_fail(window3d);
    gtk_window_present(GTK_WINDOW(window3d));
}

/**************************************************************************
 *
 * Graphs treeview
 *
 **************************************************************************/

static void
gwy_app_data_browser_graph_render_title(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                        GtkCellRenderer *renderer,
                                        GtkTreeModel *model,
                                        GtkTreeIter *iter,
                                        G_GNUC_UNUSED gpointer userdata)
{
    GObject *gmodel;
    gchar *title;

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &gmodel, -1);
    g_object_get(gmodel, "title", &title, NULL);
    g_object_set(renderer, "text", title, NULL);
    g_free(title);
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
    g_object_set(renderer, "text", s, NULL);
    g_object_unref(gmodel);
}

G_GNUC_UNUSED static void
gwy_app_data_browser_render_graph(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                  GtkCellRenderer *renderer,
                                  GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  G_GNUC_UNUSED gpointer userdata)
{
    GwyContainer *container;
    GObject *object;
    GdkPixbuf *pixbuf;
    gdouble timestamp, *pbuf_timestamp = NULL;
    gint id;

    gtk_tree_model_get(model, iter,
                       MODEL_ID, &id,
                       MODEL_OBJECT, &object,
                       MODEL_TIMESTAMP, &timestamp,
                       MODEL_THUMBNAIL, &pixbuf,
                       -1);

    container = g_object_get_qdata(object, container_quark);

    if (pixbuf) {
        pbuf_timestamp = (gdouble*)g_object_get_data(G_OBJECT(pixbuf),
                                                     "timestamp");
        if (*pbuf_timestamp >= timestamp) {
            g_object_set(renderer, "pixbuf", pixbuf, NULL);
            g_object_unref(pixbuf);
            return;
        }
    }

    pixbuf = gwy_app_get_graph_thumbnail(container, id,
                                         THUMB_SIZE, THUMB_SIZE);
    pbuf_timestamp = g_new(gdouble, 1);
    *pbuf_timestamp = gwy_get_timestamp();
    g_object_set_data_full(G_OBJECT(pixbuf), "timestamp", pbuf_timestamp,
                           g_free);
    gtk_list_store_set(GTK_LIST_STORE(model), iter,
                       MODEL_THUMBNAIL, pixbuf,
                       -1);
    g_object_set(renderer, "pixbuf", pixbuf, NULL);
    g_object_unref(pixbuf);
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
    GwyAppDataList *list;
    GwyAppKeyType type;
    GObject *object;
    GwyContainer *data;
    GtkWidget *graph;
    GtkTreeIter iter;
    const gchar *strkey;
    GQuark quark;
    gint i;

    gwy_debug("Graph window %p deleted", graph_window);
    graph = gwy_graph_window_get_graph(graph_window);
    object = G_OBJECT(gwy_graph_get_model(GWY_GRAPH(graph)));
    data = g_object_get_qdata(object, container_quark);
    quark = GPOINTER_TO_UINT(g_object_get_qdata(object, own_key_quark));
    g_return_val_if_fail(data && quark, TRUE);

    strkey = g_quark_to_string(quark);
    i = _gwy_app_analyse_data_key(strkey, &type, NULL);
    g_return_val_if_fail(i >= 0 && type == KEY_IS_GRAPH, TRUE);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    list = &proxy->lists[PAGE_GRAPHS];
    if (!gwy_app_data_proxy_find_object(list->store, i, &iter)) {
        g_critical("Cannot find graph model %p (%d)", object, i);
        return TRUE;
    }

    gwy_app_data_proxy_graph_set_visible(proxy, &iter, FALSE);
    gwy_app_data_proxy_maybe_finalize(proxy);

    return TRUE;
}

/**
 * gwy_app_data_browser_create_graph:
 * @browser: A data browser.
 * @id: The graph id.
 *
 * Creates a graph window for a graph model when its visibility is switched on.
 *
 * This is actually `make visible', should not be used outside
 * gwy_app_data_proxy_graph_set_visible().
 *
 * Returns: The graph widget (NOT graph window).
 **/
static GtkWidget*
gwy_app_data_browser_create_graph(GwyAppDataBrowser *browser,
                                  GwyAppDataProxy *proxy,
                                  gint id)
{
    static const GtkTargetEntry dnd_target_table[] = { GTK_TREE_MODEL_ROW };

    GtkWidget *graph, *curves, *graph_window;
    GtkTreeModel *model;
    gchar key[40];
    GObject *gmodel;

    g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, id);
    gwy_container_gis_object_by_name(proxy->container, key, &gmodel);
    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), NULL);

    graph = gwy_graph_new(GWY_GRAPH_MODEL(gmodel));
    graph_window = gwy_graph_window_new(GWY_GRAPH(graph));

    /* Graphs do not reference Container, fake it */
    g_object_ref(proxy->container);
    g_object_weak_ref(G_OBJECT(graph_window),
                      (GWeakNotify)g_object_unref, proxy->container);

    gwy_app_data_proxy_update_visibility(gmodel, TRUE);
    g_signal_connect_swapped(graph_window, "focus-in-event",
                             G_CALLBACK(gwy_app_data_browser_select_graph2),
                             graph);
    g_signal_connect(graph_window, "delete-event",
                     G_CALLBACK(gwy_app_data_browser_graph_deleted), NULL);
    _gwy_app_graph_window_setup(GWY_GRAPH_WINDOW(graph_window));
    gtk_window_set_default_size(GTK_WINDOW(graph_window), 480, 360);

    /* Graph DnD */
    gtk_drag_dest_set(graph_window, GTK_DEST_DEFAULT_ALL,
                      dnd_target_table, G_N_ELEMENTS(dnd_target_table),
                      GDK_ACTION_COPY);
    g_signal_connect(graph_window, "drag-data-received",
                     G_CALLBACK(gwy_app_window_dnd_data_received), browser);

    /* Graph curve DnD */
    curves = gwy_graph_window_get_graph_curves(GWY_GRAPH_WINDOW(graph_window));
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(curves));
    g_object_set_qdata(G_OBJECT(model), graph_window_quark, graph_window);
    gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(curves),
                                           GDK_BUTTON1_MASK,
                                           dnd_target_table,
                                           G_N_ELEMENTS(dnd_target_table),
                                           GDK_ACTION_COPY);

    /* FIXME: A silly place for this? */
    gwy_app_data_browser_set_file_present(browser, TRUE);
    gtk_widget_show_all(graph_window);
    _gwy_app_graph_set_current(GWY_GRAPH(graph));

    return graph;
}

static gboolean
gwy_app_data_proxy_graph_set_visible(GwyAppDataProxy *proxy,
                                     GtkTreeIter *iter,
                                     gboolean visible)
{
    GwyAppDataList *list;
    GtkTreeModel *model;
    GtkWidget *widget, *window, *succ;
    GObject *object;
    gint id;

    list = &proxy->lists[PAGE_GRAPHS];
    model = GTK_TREE_MODEL(list->store);

    gtk_tree_model_get(model, iter,
                       MODEL_WIDGET, &widget,
                       MODEL_OBJECT, &object,
                       MODEL_ID, &id,
                       -1);
    if (visible == (widget != NULL)) {
        g_object_unref(object);
        gwy_object_unref(widget);
        return FALSE;
    }

    if (visible) {
        widget = gwy_app_data_browser_create_graph(proxy->parent, proxy, id);
        gtk_list_store_set(list->store, iter, MODEL_WIDGET, widget, -1);
        list->visible_count++;
    }
    else {
        gwy_app_data_proxy_update_visibility(object, FALSE);
        window = gtk_widget_get_toplevel(widget);
        succ = gwy_app_widget_queue_manage(widget, TRUE);
        gtk_widget_destroy(window);
        gtk_list_store_set(list->store, iter, MODEL_WIDGET, NULL, -1);
        g_object_unref(widget);
        list->visible_count--;

        /* FIXME */
        if (succ)
            gwy_app_data_browser_select_graph(GWY_GRAPH(succ));
        else
            _gwy_app_graph_set_current(NULL);
    }
    g_object_unref(object);

    gwy_debug("visible_count: %d", list->visible_count);

    return TRUE;
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
    gboolean active, toggled;

    gwy_debug("Toggled graph row %s", path_str);
    proxy = browser->current;
    g_return_if_fail(proxy);

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(proxy->lists[PAGE_GRAPHS].store);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    active = gtk_cell_renderer_toggle_get_active(renderer);
    toggled = gwy_app_data_proxy_graph_set_visible(proxy, &iter, !active);
    g_assert(toggled);

    gwy_app_data_proxy_maybe_finalize(proxy);
}

static void
gwy_app_data_browser_graph_name_edited(GtkCellRenderer *renderer,
                                       const gchar *strpath,
                                       const gchar *text,
                                       GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GwyGraphModel *gmodel;
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;
    gchar *title;
    gint id;

    g_return_if_fail(browser->current);
    proxy = browser->current;
    model = GTK_TREE_MODEL(proxy->lists[PAGE_GRAPHS].store);

    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter, MODEL_ID, &id, MODEL_OBJECT, &gmodel, -1);
    title = g_strstrip(g_strdup(text));
    if (!*title) {
        g_free(title);
        title = g_strdup_printf("%s %d", _("Untitled"), id);
    }
    g_object_set(gmodel, "title", title, NULL);
    g_free(title);
    g_object_unref(gmodel);

    gwy_app_data_list_disable_edit(renderer);
}

static GtkWidget*
gwy_app_data_browser_construct_graphs(GwyAppDataBrowser *browser)
{
    static const GtkTargetEntry dnd_target_table[] = { GTK_TREE_MODEL_ROW };

    GtkTreeView *treeview;
    GtkWidget *retval;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    /* Construct the GtkTreeView that will display graphs */
    retval = gtk_tree_view_new();
    treeview = GTK_TREE_VIEW(retval);
    g_signal_connect(treeview, "row-activated",
                     G_CALLBACK(gwy_app_data_list_row_activated), NULL);

    /* Add the thumbnail column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Thumbnail", renderer,
                                                      NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_append_column(treeview, column);

    /* Add the visibility column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(renderer, "activatable", TRUE, NULL);
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
    g_object_set(renderer,
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "ellipsize-set", TRUE,
                 NULL);
    g_signal_connect(renderer, "edited",
                     G_CALLBACK(gwy_app_data_browser_graph_name_edited),
                     browser);
    g_signal_connect(renderer, "editing-canceled",
                     G_CALLBACK(gwy_app_data_list_disable_edit), NULL);
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_graph_render_title, browser, NULL);
    g_object_set_qdata(G_OBJECT(column), column_id_quark, "title");
    gtk_tree_view_append_column(treeview, column);

    /* Add the flags column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "width-chars", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes("Curves", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_graph_render_ncurves, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    gtk_tree_view_set_headers_visible(treeview, FALSE);

    /* Selection */
    selection = gtk_tree_view_get_selection(treeview);
    g_object_set_qdata(G_OBJECT(selection), page_id_quark,
                       GINT_TO_POINTER(PAGE_GRAPHS + 1));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_data_browser_selection_changed),
                     browser);

    /* DnD */
    gtk_tree_view_enable_model_drag_source(treeview,
                                           GDK_BUTTON1_MASK,
                                           dnd_target_table,
                                           G_N_ELEMENTS(dnd_target_table),
                                           GDK_ACTION_COPY);

    return retval;
}

/**************************************************************************
 *
 * Spectra treeview
 *
 **************************************************************************/

static void
gwy_app_data_browser_spectra_toggled(GtkCellRendererToggle *renderer,
                                     gchar *path_str,
                                     GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model;
    gboolean active, toggled;

    gwy_debug("Toggled spectra row %s", path_str);
    proxy = browser->current;
    g_return_if_fail(proxy);

    path = gtk_tree_path_new_from_string(path_str);
    model = GTK_TREE_MODEL(proxy->lists[PAGE_GRAPHS].store);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    active = gtk_cell_renderer_toggle_get_active(renderer);
    g_warning("Cannot make spectra visible and this column should not be "
              "visible anyway.");
    toggled = FALSE;
    /*
    toggled = gwy_app_data_proxy_graph_set_visible(proxy, &iter, !active);
    g_assert(toggled);

    gwy_app_data_proxy_maybe_finalize(proxy);
    */
}

static void
gwy_app_data_browser_spectra_name_edited(GtkCellRenderer *renderer,
                                         const gchar *strpath,
                                         const gchar *text,
                                         GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeModel *model;
    GwySpectra *spectra;
    GtkTreePath *path;
    GtkTreeIter iter;
    gchar *title;
    gint id;

    g_return_if_fail(browser->current);
    proxy = browser->current;
    model = GTK_TREE_MODEL(proxy->lists[PAGE_SPECTRA].store);

    path = gtk_tree_path_new_from_string(strpath);
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_path_free(path);

    gtk_tree_model_get(model, &iter, MODEL_ID, &id, MODEL_OBJECT, &spectra, -1);
    title = g_strstrip(g_strdup(text));
    if (!*title) {
        g_free(title);
        title = g_strdup_printf("%s %d", _("Untitled"), id);
    }
    g_object_set(spectra, "title", title, NULL);
    g_free(title);
    g_object_unref(spectra);

    gwy_app_data_list_disable_edit(renderer);
}

/* XXX: Performs some common tasks as `select_spectra' */
static void
gwy_app_data_browser_spectra_selected(GtkTreeSelection *selection,
                                      GwyAppDataBrowser *browser)
{
    GwySpectra *tspectra, *aspectra;
    GwyContainer *data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    const gchar *strkey;
    GwyAppKeyType type;
    GQuark quark;
    gint i, id;

    gwy_app_data_browser_get_current(GWY_APP_SPECTRA, &aspectra,
                                     GWY_APP_SPECTRA_ID, &id,
                                     GWY_APP_DATA_FIELD_ID, &i,
                                     0);
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, MODEL_OBJECT, &tspectra, -1);
        g_object_unref(tspectra);
    }
    else
        tspectra = NULL;

    gwy_debug("tspectra: %p, aspectra: %p", tspectra, aspectra);
    if (aspectra == tspectra) {
        /* Ensure the selection is remembered. A spectra item is selected by
         * default even if the user has not specifically selected anything,
         * therefore we can get here even if sps-id is not set in the
         * container.
         * Since GwyContainer is intelligent and does not emit "item-changed"
         * when the value does not actually change, we won't recurse to
         * death here. */
        if (aspectra) {
            gchar key[40];

            data = g_object_get_qdata(G_OBJECT(aspectra), container_quark);
            g_return_if_fail(data == browser->current->container);
            g_snprintf(key, sizeof(key), "/%d/data/sps-id", i);
            gwy_container_set_int32_by_name(data, key, id);
        }
        return;
    }

    if (tspectra) {
        data = g_object_get_qdata(G_OBJECT(tspectra), container_quark);
        g_return_if_fail(data == browser->current->container);
        quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(tspectra),
                                                    own_key_quark));
        strkey = g_quark_to_string(quark);
        id = _gwy_app_analyse_data_key(strkey, &type, NULL);
        g_return_if_fail(i >= 0 && type == KEY_IS_SPECTRA);
        browser->current->lists[PAGE_SPECTRA].active = id;
    }
    else
        id = -1;

    /* XXX: Do not delete the reference when i == -1 because this can happen
     * on descruction.  Must prevent it or handle it differently. */
    if (id > -1 && i > -1) {
        gchar key[40];

        g_snprintf(key, sizeof(key), "/%d/data/sps-id", i);
        gwy_container_set_int32_by_name(data, key, id);
    }

    _gwy_app_spectra_set_current(tspectra);
}

static void
gwy_app_data_browser_spectra_render_title(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                          GtkCellRenderer *renderer,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          G_GNUC_UNUSED gpointer userdata)
{
    GObject *spectra;
    gchar *title;

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &spectra, -1);
    g_object_get(spectra, "title", &title, NULL);
    g_object_set(renderer, "text", title, NULL);
    g_free(title);
    g_object_unref(spectra);
}

static void
gwy_app_data_browser_spectra_render_npoints(G_GNUC_UNUSED GtkTreeViewColumn *column,
                                            GtkCellRenderer *renderer,
                                            GtkTreeModel *model,
                                            GtkTreeIter *iter,
                                            G_GNUC_UNUSED gpointer userdata)
{
    GwySpectra *spectra;
    gchar s[8];

    gtk_tree_model_get(model, iter, MODEL_OBJECT, &spectra, -1);
    g_snprintf(s, sizeof(s), "%d", gwy_spectra_get_n_spectra(spectra));
    g_object_set(renderer, "text", s, NULL);
    g_object_unref(spectra);
}

static GtkWidget*
gwy_app_data_browser_construct_spectra(GwyAppDataBrowser *browser)
{
    static const GtkTargetEntry dnd_target_table[] = { GTK_TREE_MODEL_ROW };

    GtkWidget *retval;
    GtkTreeView *treeview;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

    /* Construct the GtkTreeView that will display data channels */
    retval = gtk_tree_view_new();
    treeview = GTK_TREE_VIEW(retval);
    g_signal_connect(treeview, "row-activated",
                     G_CALLBACK(gwy_app_data_list_row_activated), NULL);

    /* Add the thumbnail column */
    renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new_with_attributes("Thumbnail", renderer,
                                                      NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_append_column(treeview, column);

    /* Add the visibility column */
    renderer = gtk_cell_renderer_toggle_new();
    g_object_set(renderer, "activatable", TRUE, NULL);
    g_signal_connect(renderer, "toggled",
                     G_CALLBACK(gwy_app_data_browser_spectra_toggled), browser);
    column = gtk_tree_view_column_new_with_attributes("Visible", renderer,
                                                      NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_render_visible, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    /* Add the title column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "ellipsize", PANGO_ELLIPSIZE_END,
                 "ellipsize-set", TRUE,
                 NULL);
    g_signal_connect(renderer, "edited",
                     G_CALLBACK(gwy_app_data_browser_spectra_name_edited),
                     browser);
    g_signal_connect(renderer, "editing-canceled",
                     G_CALLBACK(gwy_app_data_list_disable_edit), NULL);
    column = gtk_tree_view_column_new_with_attributes("Title", renderer,
                                                      NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_spectra_render_title, browser, NULL);
    g_object_set_qdata(G_OBJECT(column), column_id_quark, "title");
    gtk_tree_view_append_column(treeview, column);

    /* Add the flags column */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "width-chars", 3, NULL);
    column = gtk_tree_view_column_new_with_attributes("Points", renderer,
                                                      NULL);
    gtk_tree_view_column_set_cell_data_func
        (column, renderer,
         gwy_app_data_browser_spectra_render_npoints, browser, NULL);
    gtk_tree_view_append_column(treeview, column);

    gtk_tree_view_set_headers_visible(treeview, FALSE);

    /* Selection */
    selection = gtk_tree_view_get_selection(treeview);
    g_object_set_qdata(G_OBJECT(selection), page_id_quark,
                       GINT_TO_POINTER(PAGE_SPECTRA + 1));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_data_browser_selection_changed),
                     browser);
    /* XXX: For spectra changing selection in the list actually changes the
     * current spectra. */
    g_signal_connect(selection, "changed",
                     G_CALLBACK(gwy_app_data_browser_spectra_selected),
                     browser);

    /* DnD */
    gtk_tree_view_enable_model_drag_source(treeview,
                                           GDK_BUTTON1_MASK,
                                           dnd_target_table,
                                           G_N_ELEMENTS(dnd_target_table),
                                           GDK_ACTION_COPY);

    return retval;
}

/**************************************************************************
 *
 * Common GUI
 *
 **************************************************************************/

/* GUI only */
static void
gwy_app_data_browser_delete_object(GwyAppDataProxy *proxy,
                                   guint pageno,
                                   GtkTreeModel *model,
                                   GtkTreeIter *iter)
{
    GObject *object;
    GtkWidget *widget;
    GwyContainer *data;
    gchar key[32];
    gint i;

    data = proxy->container;
    gtk_tree_model_get(model, iter,
                       MODEL_ID, &i,
                       MODEL_OBJECT, &object,
                       MODEL_WIDGET, &widget,
                       -1);

    /* Get rid of widget displaying this object.  This may invoke complete
     * destruction later in idle handler. */
    if (pageno == PAGE_CHANNELS)
        gwy_app_data_proxy_channel_destroy_3d(proxy, i);

    if (widget) {
        g_object_unref(widget);
        switch (pageno) {
            case PAGE_CHANNELS:
            gwy_app_data_proxy_channel_set_visible(proxy, iter, FALSE);
            break;

            case PAGE_GRAPHS:
            gwy_app_data_proxy_graph_set_visible(proxy, iter, FALSE);
            break;

            case PAGE_SPECTRA:
            /* FIXME */
            break;
        }
        gwy_app_data_proxy_maybe_finalize(proxy);
    }

    /* Remove object from container, this causes of removal from tree model
     * too */
    switch (pageno) {
        case PAGE_CHANNELS:
        /* XXX: Cannot just remove /0, because all graphs are under
         * GRAPH_PREFIX == "/0/graph/graph" */
        if (i) {
            g_snprintf(key, sizeof(key), "/%d", i);
            gwy_container_remove_by_prefix(data, key);
            gwy_undo_container_remove(data, key);
        }
        else {
            /* TODO: should be done in one pass through the container */
            g_snprintf(key, sizeof(key), "/%d/data", i);
            gwy_container_remove_by_prefix(data, key);
            gwy_undo_container_remove(data, key);
            g_snprintf(key, sizeof(key), "/%d/base", i);
            gwy_container_remove_by_prefix(data, key);
            g_snprintf(key, sizeof(key), "/%d/mask", i);
            gwy_container_remove_by_prefix(data, key);
            gwy_undo_container_remove(data, key);
            g_snprintf(key, sizeof(key), "/%d/show", i);
            gwy_container_remove_by_prefix(data, key);
            gwy_undo_container_remove(data, key);
            g_snprintf(key, sizeof(key), "/%d/select", i);
            gwy_container_remove_by_prefix(data, key);
            g_snprintf(key, sizeof(key), "/%d/meta", i);
            gwy_container_remove_by_prefix(data, key);
            g_snprintf(key, sizeof(key), "/%d/3d", i);
            gwy_container_remove_by_prefix(data, key);
        }
        break;

        case PAGE_GRAPHS:
        g_snprintf(key, sizeof(key), "%s/%d", GRAPH_PREFIX, i);
        gwy_container_remove_by_prefix(data, key);
        break;

        case PAGE_SPECTRA:
        g_snprintf(key, sizeof(key), "%s/%d", SPECTRA_PREFIX, i);
        gwy_container_remove_by_prefix(data, key);
        break;
    }
    g_object_unref(object);

    switch (pageno) {
        case PAGE_CHANNELS:
        gwy_app_data_list_update_last(&proxy->lists[pageno], -1);
        break;

        case PAGE_GRAPHS:
        gwy_app_data_list_update_last(&proxy->lists[pageno], 0);
        break;

        case PAGE_SPECTRA:
        gwy_app_data_list_update_last(&proxy->lists[pageno], -1);
        break;
    }
}

static void
gwy_app_data_browser_copy_object(GwyAppDataProxy *srcproxy,
                                 guint pageno,
                                 GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 GwyAppDataProxy *destproxy)
{
    GwyAppDataBrowser *browser;
    GwyContainer *container;
    gint id;

    browser = srcproxy->parent;
    gtk_tree_model_get(model, iter, MODEL_ID, &id, -1);

    if (!destproxy) {
        gwy_debug("Create a new file");
        container = gwy_container_new();
        gwy_app_data_browser_add(container);
    }
    else {
        gwy_debug("Create a new object in container %p", destproxy->container);
        container = destproxy->container;
    }

    switch (pageno) {
        case PAGE_CHANNELS:
        gwy_app_data_browser_copy_channel(srcproxy->container, id, container);
        break;

        case PAGE_GRAPHS:
        {
            GwyGraphModel *gmodel, *gmodel2;

            gtk_tree_model_get(model, iter, MODEL_OBJECT, &gmodel, -1);
            gmodel2 = gwy_graph_model_duplicate(gmodel);
            gwy_app_data_browser_add_graph_model(gmodel2, container, TRUE);
            g_object_unref(gmodel);
        }
        break;

        case PAGE_SPECTRA:
        {
            GwySpectra *spectra, *spectra2;

            gtk_tree_model_get(model, iter, MODEL_OBJECT, &spectra, -1);
            spectra2 = gwy_spectra_duplicate(spectra);
            gwy_app_data_browser_add_spectra(spectra2, container, TRUE);
            g_object_unref(spectra);
        }
        break;
    }

    if (!destproxy)
        g_object_unref(container);
}

static void
gwy_app_data_browser_copy_other(GtkTreeModel *model,
                                GtkTreeIter *iter,
                                GtkWidget *window,
                                GwyContainer *container)
{
    GwyDataView *data_view;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    GQuark srcquark, targetquark, destquark;
    GObject *object, *destobject;
    const gchar *srckey, *targetkey;
    GwyAppKeyType type;
    gint id;
    gchar *destkey;
    guint len;

    /* XXX: At this moment, the copying possibilities are fairly limited. */
    if (!GWY_IS_DATA_WINDOW(window))
        return;

    /* Source */
    gtk_tree_model_get(model, iter,
                       MODEL_ID, &srcquark,
                       MODEL_OBJECT, &object,
                       -1);
    if (!object)
        return;
    srckey = g_quark_to_string(srcquark);
    if (!srckey) {
        g_object_unref(object);
        return;
    }
    gwy_debug("DnD: key %08x <%s>, object %p <%s>\n",
               srcquark, srckey, object, G_OBJECT_TYPE_NAME(object));

    id = _gwy_app_analyse_data_key(srckey, &type, &len);
    /* XXX: At this moment, the copying possibilities are fairly limited. */
    if (id == -1 || type != KEY_IS_SELECT || !GWY_IS_SELECTION(object)) {
        g_object_unref(object);
        return;
    }

    /* Target */
    data_view  = gwy_data_window_get_data_view(GWY_DATA_WINDOW(window));
    layer = gwy_data_view_get_base_layer(data_view);
    targetkey = gwy_pixmap_layer_get_data_key(layer);
    targetquark = g_quark_from_string(targetkey);
    g_return_if_fail(targetquark);
    id = _gwy_app_analyse_data_key(targetkey, &type, NULL);
    g_return_if_fail(id >= 0 && type == KEY_IS_DATA);
    dfield = gwy_container_get_object(container, targetquark);
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));

    /* Destination */
    destkey = g_strdup_printf("/%d/select%s", id, srckey+len);
    destquark = g_quark_from_string(destkey);
    g_free(destkey);

    /* Avoid copies if source is the same as the target */
    if (!gwy_container_gis_object(container, destquark, &destobject)
        || destobject != object) {
        gdouble xmin, xmax, ymin, ymax;

        /* FIXME: It would be nice to check compatibility of units, but we have
         * no idea where the selection come from. */
        xmin = xmax = gwy_data_field_get_xoffset(dfield);
        ymin = ymax = gwy_data_field_get_yoffset(dfield);
        xmax += gwy_data_field_get_xreal(dfield);
        ymax += gwy_data_field_get_yreal(dfield);
        destobject = gwy_serializable_duplicate(G_OBJECT(object));
        gwy_selection_crop(GWY_SELECTION(destobject), xmin, ymin, xmax, ymax);
        if (gwy_selection_get_data(GWY_SELECTION(destobject), NULL))
            gwy_container_set_object(container, destquark, destobject);
        g_object_unref(destobject);
    }

    g_object_unref(object);
}

static void
gwy_app_data_browser_close_file(GwyAppDataBrowser *browser)
{
    g_return_if_fail(browser->current);
    gwy_app_data_browser_remove(browser->current->container);
}

static void
gwy_app_data_browser_page_changed(GwyAppDataBrowser *browser,
                                  G_GNUC_UNUSED GtkNotebookPage *useless_crap,
                                  gint pageno)
{
    GtkTreeSelection *selection;

    gwy_debug("Page changed to: %d", pageno);

    browser->active_page = pageno;
    selection
        = gtk_tree_view_get_selection(GTK_TREE_VIEW(browser->lists[pageno]));
    gwy_app_data_browser_selection_changed(selection, browser);
}

static gboolean
gwy_app_data_browser_deleted(GwyAppDataBrowser *browser)
{
    gwy_app_data_browser_hide_real(browser);

    return TRUE;
}

static void
gwy_app_data_browser_window_destroyed(GwyAppDataBrowser *browser)
{
    guint i;

    browser->window = NULL;
    browser->active_page = 0;
    browser->sensgroup = NULL;
    browser->filename = NULL;
    browser->notebook = NULL;
    for (i = 0; i < NPAGES; i++)
        browser->lists[i] = NULL;
}

static void
gwy_app_data_browser_shoot_object(GObject *button,
                                  GwyAppDataBrowser *browser)
{
    GwyAppDataProxy *proxy;
    GtkTreeView *treeview;
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint pageno;
    const gchar *action;

    g_return_if_fail(browser->current);

    action = g_object_get_data(button, "action");
    gwy_debug("action: %s", action);

    proxy = browser->current;
    pageno = browser->active_page;

    treeview = GTK_TREE_VIEW(browser->lists[pageno]);
    selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        g_warning("Nothing is selected");
        return;
    }

    if (gwy_strequal(action, "delete"))
        gwy_app_data_browser_delete_object(proxy, pageno, model, &iter);
    else if (gwy_strequal(action, "duplicate"))
        gwy_app_data_browser_copy_object(proxy, pageno, model, &iter, proxy);
    else if (gwy_strequal(action, "extract"))
        gwy_app_data_browser_copy_object(proxy, pageno, model, &iter, NULL);
    else
        g_warning("Unknown action <%s>", action);
}

static GtkWidget*
gwy_app_data_browser_construct_buttons(GwyAppDataBrowser *browser)
{
    static const struct {
        const gchar *stock_id;
        const gchar *tooltip;
        const gchar *action;
    }
    actions[] = {
        { GTK_STOCK_NEW,    N_("Extract to a new file"), "extract",   },
        { GTK_STOCK_COPY,   N_("Duplicate"),             "duplicate", },
        { GTK_STOCK_DELETE, N_("Delete"),                "delete",    },
    };

    GtkWidget *hbox, *button, *image;
    GtkTooltips *tips;
    guint i;

    tips = gwy_app_get_tooltips();
    hbox = gtk_hbox_new(TRUE, 0);

    for (i = 0; i < G_N_ELEMENTS(actions); i++) {
        image = gtk_image_new_from_stock(actions[i].stock_id,
                                         GTK_ICON_SIZE_LARGE_TOOLBAR);
        button = gtk_button_new();
        g_object_set_data(G_OBJECT(button), "action",
                          (gpointer)actions[i].action);
        gtk_tooltips_set_tip(tips, button, _(actions[i].tooltip), NULL);
        gtk_container_add(GTK_CONTAINER(button), image);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gwy_sensitivity_group_add_widget(browser->sensgroup, button,
                                         SENS_OBJECT);
        g_signal_connect(button, "clicked",
                         G_CALLBACK(gwy_app_data_browser_shoot_object),
                         browser);
    }

    return hbox;
}

static void
gwy_app_data_browser_construct_window(GwyAppDataBrowser *browser)
{
    GtkWidget *label, *box_page, *scwin, *vbox, *hbox, *button, *image;
    GtkTooltips *tips;

    tips = gwy_app_get_tooltips();

    browser->sensgroup = gwy_sensitivity_group_new();
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect_swapped(browser->window, "destroy",
                             G_CALLBACK(gwy_app_data_browser_window_destroyed),
                             browser);

    gtk_window_set_default_size(GTK_WINDOW(browser->window), 300, 300);
    gtk_window_set_title(GTK_WINDOW(browser->window), _("Data Browser"));
    gtk_window_set_role(GTK_WINDOW(browser->window), GWY_DATABROWSER_WM_ROLE);
    gwy_app_add_main_accel_group(GTK_WINDOW(browser->window));

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(browser->window), vbox);

    /* Filename row */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    /* Filename */
    browser->filename = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(browser->filename), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment(GTK_MISC(browser->filename), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(browser->filename), 4, 2);
    gtk_box_pack_start(GTK_BOX(hbox), browser->filename, TRUE, TRUE, 0);

    /* Close button */
    button = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON);
    gtk_container_add(GTK_CONTAINER(button), image);
    gtk_tooltips_set_tip(tips, button, _("Close file"), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gwy_sensitivity_group_add_widget(browser->sensgroup, button, SENS_FILE);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_app_data_browser_close_file),
                             browser);

    /* Notebook */
    browser->notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), browser->notebook, TRUE, TRUE, 0);

    /* Channels tab */
    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new(_("Channels"));
    gtk_notebook_append_page(GTK_NOTEBOOK(browser->notebook), box_page, label);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box_page), scwin, TRUE, TRUE, 0);

    browser->lists[PAGE_CHANNELS]
        = gwy_app_data_browser_construct_channels(browser);
    gtk_container_add(GTK_CONTAINER(scwin), browser->lists[PAGE_CHANNELS]);

    /* Graphs tab */
    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new(_("Graphs"));
    gtk_notebook_append_page(GTK_NOTEBOOK(browser->notebook), box_page, label);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box_page), scwin, TRUE, TRUE, 0);

    browser->lists[PAGE_GRAPHS]
        = gwy_app_data_browser_construct_graphs(browser);
    gtk_container_add(GTK_CONTAINER(scwin), browser->lists[PAGE_GRAPHS]);

    /* Single point spectra */
    box_page = gtk_vbox_new(FALSE, 0);
    label = gtk_label_new(_("Spectra"));
    gtk_notebook_append_page(GTK_NOTEBOOK(browser->notebook), box_page, label);

    scwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(box_page), scwin, TRUE, TRUE, 0);

    browser->lists[PAGE_SPECTRA]
        = gwy_app_data_browser_construct_spectra(browser);
    gtk_container_add(GTK_CONTAINER(scwin), browser->lists[PAGE_SPECTRA]);

    /* Buttons */
    hbox = gwy_app_data_browser_construct_buttons(browser);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    /* Finish */
    g_signal_connect_swapped(browser->notebook, "switch-page",
                             G_CALLBACK(gwy_app_data_browser_page_changed),
                             browser);
    g_signal_connect_swapped(browser->window, "delete-event",
                             G_CALLBACK(gwy_app_data_browser_deleted), browser);
    g_object_unref(browser->sensgroup);

    gtk_widget_show_all(vbox);
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
    GwyAppDataBrowser *browser;

    if (gwy_app_data_browser)
        return gwy_app_data_browser;

    own_key_quark
        = g_quark_from_static_string("gwy-app-data-browser-own-key");
    container_quark
        = g_quark_from_static_string("gwy-app-data-browser-container");
    page_id_quark
        = g_quark_from_static_string("gwy-app-data-browser-page-id");
    column_id_quark
        = g_quark_from_static_string("gwy-app-data-browser-column-id");
    filename_quark
        = g_quark_from_static_string("/filename");
    graph_window_quark
        = g_quark_from_static_string("gwy-app-data-browser-window-model");

    browser = g_new0(GwyAppDataBrowser, 1);
    gwy_app_data_browser = browser;

    return browser;
}

static void
gwy_app_data_browser_select_iter(GtkTreeView *treeview,
                                 GtkTreeIter *iter)
{
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection(treeview);
    gtk_tree_selection_select_iter(selection, iter);
}

static void
gwy_app_data_browser_restore_active(GtkTreeView *treeview,
                                    GwyAppDataList *list)
{
    GtkTreeIter iter;

    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(list->store));
    if (gwy_app_data_proxy_find_object(list->store, list->active, &iter))
        gwy_app_data_browser_select_iter(treeview, &iter);
}

static void
gwy_app_data_browser_switch_data(GwyContainer *data)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    guint i;

    browser = gwy_app_get_data_browser();
    if (!data) {
        if (browser->window) {
            for (i = 0; i < NPAGES; i++)
                gtk_tree_view_set_model(GTK_TREE_VIEW(browser->lists[i]), NULL);
            gtk_label_set_text(GTK_LABEL(browser->filename), NULL);
            gwy_app_data_browser_set_file_present(browser, FALSE);
        }
        browser->current = NULL;
        return;
    }

    if (browser->current && browser->current->container == data)
        return;

    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);
    if (proxy->finalize_id)
        return;

    browser->current = proxy;

    gwy_app_data_browser_update_filename(proxy);
    if (browser->window) {
        for (i = 0; i < NPAGES; i++)
            gwy_app_data_browser_restore_active
                          (GTK_TREE_VIEW(browser->lists[i]), &proxy->lists[i]);
        gwy_app_data_browser_set_file_present(browser, TRUE);
    }
}

static void
gwy_app_data_browser_select_object(GwyAppDataBrowser *browser,
                                   GwyAppDataProxy *proxy,
                                   guint pageno)
{
    GtkTreeView *treeview;
    GtkTreeIter iter;

    if (!browser->window)
        return;

    treeview = GTK_TREE_VIEW(browser->lists[pageno]);
    gwy_app_data_proxy_find_object(proxy->lists[pageno].store,
                                   proxy->lists[pageno].active, &iter);
    gwy_app_data_browser_select_iter(treeview, &iter);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(browser->notebook), pageno);
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
    GwyPixmapLayer *layer;
    GwyContainer *data, *olddata;
    const gchar *strkey;
    GwyAppKeyType type;
    gint i;

    browser = gwy_app_get_data_browser();
    olddata = browser->current ? browser->current->container : NULL;

    data = gwy_data_view_get_data(data_view);
    gwy_app_data_browser_switch_data(data);

    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);

    layer = gwy_data_view_get_base_layer(data_view);
    strkey = gwy_pixmap_layer_get_data_key(layer);
    i = _gwy_app_analyse_data_key(strkey, &type, NULL);
    g_return_if_fail(i >= 0 && type == KEY_IS_DATA);
    proxy->lists[PAGE_CHANNELS].active = i;

    gwy_app_data_browser_select_object(browser, proxy, PAGE_CHANNELS);
    gwy_app_widget_queue_manage(GTK_WIDGET(data_view), FALSE);
    _gwy_app_data_view_set_current(data_view);

    /* Restore the last used spectra.  If the reference is dangling, remove
     * it from the container. */
    {
        gboolean selected = FALSE;
        GwySpectra *spectra;
        gchar key[40];
        gint id;

        g_snprintf(key, sizeof(key), "/%d/data/sps-id", i);
        if (gwy_container_gis_int32_by_name(data, key, &id)) {
            GQuark quark;

            quark = gwy_app_get_spectra_key_for_id(id);
            if (gwy_container_gis_object(data, quark, &spectra)) {
                gwy_app_data_browser_select_spectra(spectra);
                selected = TRUE;
            }
            else
                gwy_container_remove_by_name(data, key);
        }
        /* We have to ensure NULL spectra selection is emitted when we
         * switch to data that have no spectra.  And generally whenever we
         * switch to another container, we make spectra from that container
         * active (or none). */
        if (!selected) {
            if (data != olddata) {
                GwyAppDataList *list = &proxy->lists[PAGE_SPECTRA];
                GtkTreeModel *model;
                GtkTreeIter iter;

                model = GTK_TREE_MODEL(list->store);
                if (gwy_app_data_proxy_find_object(list->store, list->active,
                                                   &iter)
                    || gtk_tree_model_get_iter_first(model, &iter)) {
                    gtk_tree_model_get(model, &iter,
                                       MODEL_OBJECT, &spectra,
                                       -1);
                    gwy_app_data_browser_select_spectra(spectra);
                    g_object_unref(spectra);
                }
                else {
                    _gwy_app_spectra_set_current(NULL);
                }
            }
        }
    }
}

static gboolean
gwy_app_data_browser_select_data_view2(GwyDataView *data_view)
{
    gwy_app_data_browser_select_data_view(data_view);
    return FALSE;
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
    i = _gwy_app_analyse_data_key(strkey, &type, NULL);
    g_return_if_fail(i >= 0 && type == KEY_IS_GRAPH);
    proxy->lists[PAGE_GRAPHS].active = i;

    gwy_app_data_browser_select_object(browser, proxy, PAGE_GRAPHS);
    gwy_app_widget_queue_manage(GTK_WIDGET(graph), FALSE);
    _gwy_app_graph_set_current(graph);
}

static gboolean
gwy_app_data_browser_select_graph2(GwyGraph *graph)
{
    gwy_app_data_browser_select_graph(graph);
    return FALSE;
}

/**
 * gwy_app_data_browser_select_spectra:
 * @spectra: A spectra object.
 *
 * Switches application data browser to display container of @spectra's data
 * and selects @spectra's data in the graph list.
 *
 * However, it is not actually supposed to work with spectra from a different
 * container than those of the currently active channel, so do not try that
 * for now.
 *
 * Since: 2.7
 **/
void
gwy_app_data_browser_select_spectra(GwySpectra *spectra)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GwyContainer *data;
    const gchar *strkey;
    GwyAppKeyType type;
    GQuark quark;
    gint i;

    data = g_object_get_qdata(G_OBJECT(spectra), container_quark);
    g_return_if_fail(data);
    gwy_app_data_browser_switch_data(data);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    g_return_if_fail(proxy);

    quark = GPOINTER_TO_UINT(g_object_get_qdata(G_OBJECT(spectra),
                                                own_key_quark));
    strkey = g_quark_to_string(quark);
    i = _gwy_app_analyse_data_key(strkey, &type, NULL);
    g_return_if_fail(i >= 0 && type == KEY_IS_SPECTRA);
    proxy->lists[PAGE_SPECTRA].active = i;

    gwy_app_data_browser_select_object(browser, proxy, PAGE_SPECTRA);
    _gwy_app_spectra_set_current(spectra);
}

static GwyAppDataProxy*
gwy_app_data_browser_select(GwyContainer *data,
                            gint id,
                            gint pageno,
                            GtkTreeIter *iter)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;

    gwy_app_data_browser_switch_data(data);

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    if (!gwy_app_data_proxy_find_object(proxy->lists[pageno].store, id,
                                        iter)) {
        g_warning("Cannot find object to select");
        return NULL;
    }

    proxy->lists[pageno].active = id;
    gwy_app_data_browser_select_object(browser, proxy, pageno);

    return proxy;

}

/**
 * gwy_app_data_browser_select_data_field:
 * @data: The container to select.
 * @id: Number (id) of the data field in @data to select.
 *
 * Makes a data field (channel) current in the data browser.
 **/
void
gwy_app_data_browser_select_data_field(GwyContainer *data,
                                       gint id)
{
    GwyAppDataProxy *proxy;
    GwyDataView *data_view;
    GtkTreeIter iter;

    proxy = gwy_app_data_browser_select(data, id, PAGE_CHANNELS, &iter);

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_CHANNELS].store), &iter,
                       MODEL_WIDGET, &data_view,
                       -1);
    if (data_view) {
        _gwy_app_data_view_set_current(data_view);
        g_object_unref(data_view);
    }
}

/**
 * gwy_app_data_browser_select_graph_model:
 * @data: The container to select.
 * @id: Number (id) of the graph model in @data to select.
 *
 * Makes a graph model (channel) current in the data browser.
 **/
void
gwy_app_data_browser_select_graph_model(GwyContainer *data,
                                        gint id)
{
    GwyAppDataProxy *proxy;
    GwyGraph *graph;
    GtkTreeIter iter;

    proxy = gwy_app_data_browser_select(data, id, PAGE_GRAPHS, &iter);

    gtk_tree_model_get(GTK_TREE_MODEL(proxy->lists[PAGE_GRAPHS].store), &iter,
                       MODEL_WIDGET, &graph,
                       -1);
    if (graph) {
        _gwy_app_graph_set_current(graph);
        g_object_unref(graph);
    }
}

static void
gwy_app_data_list_reset_visibility(GwyAppDataProxy *proxy,
                                   GwyAppDataList *list,
                                   SetVisibleFunc set_visible,
                                   gboolean visible)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    model = GTK_TREE_MODEL(list->store);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            set_visible(proxy, &iter, visible);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

static void
gwy_app_data_list_reconstruct_visibility(GwyAppDataProxy *proxy,
                                         GwyAppDataList *list,
                                         SetVisibleFunc set_visible)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GObject *object;
    GQuark quark;
    const gchar *strkey;
    gchar key[48];
    gboolean visible;

    model = GTK_TREE_MODEL(list->store);
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            visible = FALSE;
            gtk_tree_model_get(model, &iter, MODEL_OBJECT, &object, -1);
            quark = GPOINTER_TO_UINT(g_object_get_qdata(object, own_key_quark));
            strkey = g_quark_to_string(quark);
            g_snprintf(key, sizeof(key), "%s/visible", strkey);
            gwy_container_gis_boolean_by_name(proxy->container, key, &visible);
            set_visible(proxy, &iter, visible);
            g_object_unref(object);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

/**
 * gwy_app_data_browser_reset_visibility:
 * @data: A data container.
 * @reset_type: Type of visibility reset.
 *
 * Resets visibility of all data objects in a container.
 *
 * Returns: %TRUE if anything is visible after the reset.
 **/
gboolean
gwy_app_data_browser_reset_visibility(GwyContainer *data,
                                      GwyVisibilityResetType reset_type)
{
    static const SetVisibleFunc set_visible[NPAGES] = {
        &gwy_app_data_proxy_channel_set_visible,
        &gwy_app_data_proxy_graph_set_visible,
        NULL,
    };

    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy = NULL;
    GwyAppDataList *list;
    gboolean visible;
    gint i;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);

    if (gui_disabled)
        return FALSE;

    if ((browser = gwy_app_data_browser))
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);

    if (!proxy) {
        g_critical("Data container is unknown to data browser.");
        return FALSE;
    }

    if (reset_type == GWY_VISIBILITY_RESET_RESTORE
        || reset_type == GWY_VISIBILITY_RESET_DEFAULT) {
        for (i = 0; i < NPAGES; i++) {
            if (set_visible[i])
                gwy_app_data_list_reconstruct_visibility(proxy,
                                                         &proxy->lists[i],
                                                         set_visible[i]);
        }
        if (gwy_app_data_proxy_visible_count(proxy))
            return TRUE;

        /* For RESTORE, we are content even with nothing being displayed */
        if (reset_type == GWY_VISIBILITY_RESET_RESTORE)
            return FALSE;

        /* Attempt to show something. FIXME: Crude. */
        for (i = 0; i < NPAGES; i++) {
            GtkTreeModel *model;
            GtkTreeIter iter;

            if (!set_visible[i])
                continue;

            list = &proxy->lists[i];
            model = GTK_TREE_MODEL(list->store);
            if (!gtk_tree_model_get_iter_first(model, &iter))
                continue;

            set_visible[i](proxy, &iter, TRUE);
        }

        return FALSE;
    }

    if (reset_type == GWY_VISIBILITY_RESET_HIDE_ALL)
        visible = FALSE;
    else if (reset_type == GWY_VISIBILITY_RESET_SHOW_ALL)
        visible = TRUE;
    else {
        g_critical("Wrong reset_type value");
        return FALSE;
    }

    for (i = 0; i < NPAGES; i++) {
        if (set_visible[i])
            gwy_app_data_list_reset_visibility(proxy, &proxy->lists[i],
                                               set_visible[i], visible);
    }

    return visible && gwy_app_data_proxy_visible_count(proxy);
}

/**
 * gwy_app_data_browser_add:
 * @data: A data container.
 *
 * Adds a data container to the application data browser.
 *
 * The data browser takes a reference on the container so you can release
 * yours.
 **/
void
gwy_app_data_browser_add(GwyContainer *data)
{
    g_return_if_fail(GWY_IS_CONTAINER(data));

    gwy_app_data_browser_get_proxy(gwy_app_get_data_browser(), data, TRUE);
}

/**
 * gwy_app_data_browser_remove:
 * @data: A data container.
 *
 * Removed a data container from the application data browser.
 **/
void
gwy_app_data_browser_remove(GwyContainer *data)
{
    GwyAppDataProxy *proxy;

    proxy = gwy_app_data_browser_get_proxy(gwy_app_get_data_browser(), data,
                                           FALSE);
    g_return_if_fail(proxy);

    gwy_app_data_proxy_destroy_all_3d(proxy);
    gwy_app_data_browser_reset_visibility(proxy->container,
                                          GWY_VISIBILITY_RESET_HIDE_ALL);
    g_return_if_fail(gwy_app_data_proxy_visible_count(proxy) == 0);
    gwy_app_data_proxy_finalize(proxy);
}

static void
gwy_app_data_merge_gather(gpointer key,
                          G_GNUC_UNUSED gpointer value,
                          gpointer user_data)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GList **ids = (GList**)user_data;
    GwyAppKeyType type;
    gint id, pageno;

    id = _gwy_app_analyse_data_key(g_quark_to_string(quark), &type, NULL);
    switch (type) {
        case KEY_IS_DATA:
        pageno = PAGE_CHANNELS;
        break;

        case KEY_IS_GRAPH:
        pageno = PAGE_GRAPHS;
        break;

        case KEY_IS_SPECTRA:
        pageno = PAGE_SPECTRA;
        break;

        default:
        return;
        break;
    }
    gwy_debug("adding %d to page %d", id, pageno);
    ids[pageno] = g_list_prepend(ids[pageno], GINT_TO_POINTER(id));
}

static void
gwy_app_data_merge_copy_1(gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GValue *gvalue = (GValue*)value;
    GHashTable **map = (GHashTable**)user_data;
    GwyAppKeyType type;
    gpointer idp, id2p;
    gint id;

    id = _gwy_app_analyse_data_key(g_quark_to_string(quark), &type, NULL);
    idp = GINT_TO_POINTER(id);
    switch (type) {
        case KEY_IS_DATA:
        if (!g_hash_table_lookup_extended(map[PAGE_CHANNELS], idp, NULL, &id2p))
            goto fail;
        quark = gwy_app_get_data_key_for_id(GPOINTER_TO_INT(id2p));
        break;

        case KEY_IS_GRAPH:
        if (!g_hash_table_lookup_extended(map[PAGE_GRAPHS], idp, NULL, &id2p))
            goto fail;
        quark = gwy_app_get_graph_key_for_id(GPOINTER_TO_INT(id2p));
        break;

        case KEY_IS_SPECTRA:
        if (!g_hash_table_lookup_extended(map[PAGE_SPECTRA], idp, NULL, &id2p))
            goto fail;
        quark = gwy_app_get_spectra_key_for_id(GPOINTER_TO_INT(id2p));
        break;

        default:
        /* Handle these in gwy_app_data_merge_copy_2() */
        return;
        break;
    }
    gwy_container_set_object((GwyContainer*)map[NPAGES], quark,
                             g_value_get_object(gvalue));
    return;

fail:
    g_warning("%s does not map to any new location", g_quark_to_string(quark));
}

static void
gwy_app_data_merge_copy_2(gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GValue *gvalue = (GValue*)value;
    GHashTable **map = (GHashTable**)user_data;
    GwyContainer *dest;
    const gchar *strkey;
    GwyAppKeyType type;
    gpointer idp, id2p;
    gint id, id2;
    guint len;
    gboolean visibility = FALSE;
    gchar buf[80];

    strkey = g_quark_to_string(quark);
    if (gwy_strequal(strkey, "/0/graph/lastid"))
        return;

    /* Handle visibility by stripping "/visible" from the key before analysis */
    if (g_str_has_suffix(strkey, "/visible")) {
        gchar *vstrkey;

        vstrkey = g_strndup(strkey, strlen(strkey) - strlen("/visible"));
        id = _gwy_app_analyse_data_key(vstrkey, &type, &len);
        g_free(vstrkey);
        visibility = TRUE;
    }
    else
        id = _gwy_app_analyse_data_key(strkey, &type, &len);

    if (type == KEY_IS_FILENAME)
        return;
    if (id < 0)
        goto fail;

    idp = GINT_TO_POINTER(id);
    dest = (GwyContainer*)map[NPAGES];

    /* Visibilty */
    switch (type) {
        case KEY_IS_DATA:
        if (visibility) {
            if (!g_hash_table_lookup_extended(map[PAGE_CHANNELS], idp, NULL,
                                              &id2p))
                goto fail;
            quark = gwy_app_get_data_key_for_id(GPOINTER_TO_INT(id2p));
            g_snprintf(buf, sizeof(buf), "%s/visible",
                       g_quark_to_string(quark));
            if (g_value_get_boolean(gvalue))
                gwy_container_set_boolean_by_name(dest, buf, TRUE);
        }
        return;
        break;

        case KEY_IS_GRAPH:
        if (visibility) {
            if (!g_hash_table_lookup_extended(map[PAGE_GRAPHS], idp, NULL,
                                              &id2p))
                goto fail;
            quark = gwy_app_get_graph_key_for_id(GPOINTER_TO_INT(id2p));
            g_snprintf(buf, sizeof(buf), "%s/visible",
                       g_quark_to_string(quark));
            if (g_value_get_boolean(gvalue))
                gwy_container_set_boolean_by_name(dest, buf, TRUE);
        }
        return;
        break;

        case KEY_IS_SPECTRA:
        if (visibility) {
            if (!g_hash_table_lookup_extended(map[PAGE_SPECTRA], idp, NULL,
                                              &id2p))
                goto fail;
            quark = gwy_app_get_spectra_key_for_id(GPOINTER_TO_INT(id2p));
            g_snprintf(buf, sizeof(buf), "%s/visible",
                       g_quark_to_string(quark));
            if (g_value_get_boolean(gvalue))
                gwy_container_set_boolean_by_name(dest, buf, TRUE);
        }
        return;
        break;

        default:
        /* Pass */
        break;
    }

    if (!g_hash_table_lookup_extended(map[PAGE_CHANNELS], idp, NULL, &id2p))
        goto fail;
    id2 = GPOINTER_TO_INT(id2p);

    switch (type) {
        case KEY_IS_MASK:
        quark = gwy_app_get_mask_key_for_id(id2);
        gwy_container_set_object(dest, quark, g_value_get_object(gvalue));
        break;

        case KEY_IS_SHOW:
        quark = gwy_app_get_show_key_for_id(id2);
        gwy_container_set_object(dest, quark, g_value_get_object(gvalue));
        break;

        case KEY_IS_SPS_REF:
        id = g_value_get_int(gvalue);
        idp = GINT_TO_POINTER(id);
        /* Ignore references to nonexistent sps ids silently */
        if (g_hash_table_lookup_extended(map[PAGE_SPECTRA], idp, NULL, &id2p)) {
            g_snprintf(buf, sizeof(buf), "/%d/data/sps-is", id2);
            id2 = GPOINTER_TO_INT(id2p);
            gwy_container_set_int32_by_name(dest, buf, id2);
        }
        break;

        case KEY_IS_TITLE:
        g_snprintf(buf, sizeof(buf), "/%d/data/title", id2);
        gwy_container_set_string_by_name(dest, buf, g_value_dup_string(gvalue));
        break;

        case KEY_IS_PALETTE:
        g_snprintf(buf, sizeof(buf), "/%d/base/palette", id2);
        gwy_container_set_string_by_name(dest, buf, g_value_dup_string(gvalue));
        break;

        case KEY_IS_MASK_COLOR:
        g_snprintf(buf, sizeof(buf), "/%d/mask%s", id2, strkey + len);
        gwy_container_set_double_by_name(dest, buf, g_value_get_double(gvalue));
        break;

        case KEY_IS_SELECT:
        g_snprintf(buf, sizeof(buf), "/%d/data/select%s", id2, strkey + len);
        gwy_container_set_object_by_name(dest, buf, g_value_get_object(gvalue));
        break;

        case KEY_IS_RANGE_TYPE:
        g_snprintf(buf, sizeof(buf), "/%d/base/range-type", id2);
        gwy_container_set_enum_by_name(dest, buf, g_value_get_int(gvalue));
        break;

        case KEY_IS_RANGE:
        g_snprintf(buf, sizeof(buf), "/%d/base%s", id2, strkey + len);
        gwy_container_set_double_by_name(dest, buf, g_value_get_double(gvalue));
        break;

        case KEY_IS_REAL_SQUARE:
        g_snprintf(buf, sizeof(buf), "/%d/data/realsquare", id2);
        gwy_container_set_boolean_by_name(dest, buf,
                                          g_value_get_boolean(gvalue));
        break;

        case KEY_IS_META:
        g_snprintf(buf, sizeof(buf), "/%d/meta", id2);
        gwy_container_set_object_by_name(dest, buf, g_value_get_object(gvalue));
        break;

        case KEY_IS_3D_SETUP:
        g_snprintf(buf, sizeof(buf), "/%d/3d/setup", id2);
        gwy_container_set_object_by_name(dest, buf, g_value_get_object(gvalue));
        break;

        case KEY_IS_3D_LABEL:
        g_snprintf(buf, sizeof(buf), "/%d/3d%s", id2, strkey + len);
        gwy_container_set_object_by_name(dest, buf, g_value_get_object(gvalue));
        break;

        case KEY_IS_3D_PALETTE:
        g_snprintf(buf, sizeof(buf), "/%d/3d/palette", id2);
        gwy_container_set_string_by_name(dest, buf, g_value_dup_string(gvalue));
        break;

        case KEY_IS_3D_MATERIAL:
        g_snprintf(buf, sizeof(buf), "/%d/3d/material", id2);
        gwy_container_set_string_by_name(dest, buf, g_value_dup_string(gvalue));
        break;

        default:
        goto fail;
        break;
    }
    return;

fail:
    g_warning("%s does not map to any new location, cannot map it "
              "generically because the current key organization is a mess",
              strkey);
}

static gint
compare_int(gconstpointer a,
            gconstpointer b)
{
    gint ia, ib;

    ia = GPOINTER_TO_INT(a);
    ib = GPOINTER_TO_INT(b);
    if (ia < ib)
        return -1;
    if (ia > ib)
        return 1;
    return 0;
}

/**
 * gwy_app_data_browser_merge:
 * @data: A data container, not managed by the data browser.
 *
 * Merges the data from a data container to the current one.
 *
 * Since: 2.7
 **/
void
gwy_app_data_browser_merge(GwyContainer *container)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GList *ids[NPAGES], *l;
    GHashTable *map[NPAGES+1];
    gint last, pageno;

    g_return_if_fail(GWY_IS_CONTAINER(container));
    browser = gwy_app_get_data_browser();

    proxy = gwy_app_data_browser_get_proxy(browser, container, FALSE);
    if (proxy) {
        g_critical("Live files cannot be merged");
        return;
    }
    proxy = browser->current;
    if (!proxy) {
        g_warning("There is no current data to merge to");
        gwy_app_data_browser_add(container);
        return;
    }

    /* Build a map from container ids to destination ids */
    memset(&ids[0], 0, NPAGES*sizeof(GList*));
    gwy_container_foreach(container, NULL, gwy_app_data_merge_gather, &ids[0]);
    for (pageno = 0; pageno < NPAGES; pageno++) {
        gwy_debug("page %d", pageno);
        last = proxy->lists[pageno].last;
        map[pageno] = g_hash_table_new(g_direct_hash, g_direct_equal);
        ids[pageno] = g_list_sort(ids[pageno], compare_int);
        for (l = ids[pageno]; l; l = g_list_next(l)) {
            last++;
            g_hash_table_insert(map[pageno], l->data, GINT_TO_POINTER(last));
            gwy_debug("mapping %d -> %d", GPOINTER_TO_INT(l->data), last);
        }
        g_list_free(ids[pageno]);
    }

    /* Perform the transfer */
    map[NPAGES] = (GHashTable*)proxy->container;
    gwy_container_foreach(container, NULL, gwy_app_data_merge_copy_1, &map[0]);
    gwy_container_foreach(container, NULL, gwy_app_data_merge_copy_2, &map[0]);
    gwy_app_data_browser_reset_visibility(proxy->container,
                                          GWY_VISIBILITY_RESET_RESTORE);
}

/**
 * gwy_app_data_browser_set_keep_invisible:
 * @data: A data container.
 * @keep_invisible: %TRUE to retain @data in the browser even when it becomes
 *                  inaccessible, %FALSE to dispose of it.
 *
 * Sets data browser behaviour for inaccessible data.
 *
 * Normally, when all visual objects belonging to a file are closed the
 * container is removed from the data browser and dereferenced, leading to
 * its finalization.  By setting @keep_invisible to %TRUE the container can be
 * made to sit in the browser indefinitely.
 **/
void
gwy_app_data_browser_set_keep_invisible(GwyContainer *data,
                                        gboolean keep_invisible)
{
    GwyAppDataProxy *proxy;

    proxy = gwy_app_data_browser_get_proxy(gwy_app_get_data_browser(), data,
                                           FALSE);
    g_return_if_fail(proxy);
    proxy->keep_invisible = keep_invisible;
}

/**
 * gwy_app_data_browser_get_keep_invisible:
 * @data: A data container.
 *
 * Gets data browser behaviour for inaccessible data.
 *
 * Returns: See gwy_app_data_browser_set_keep_invisible().
 **/
gboolean
gwy_app_data_browser_get_keep_invisible(GwyContainer *data)
{
    GwyAppDataProxy *proxy;

    proxy = gwy_app_data_browser_get_proxy(gwy_app_get_data_browser(), data,
                                           FALSE);
    g_return_val_if_fail(proxy, FALSE);

    return proxy->keep_invisible;
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
    GwyAppDataList *list;
    GtkTreeIter iter;
    gchar key[24];

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), -1);
    g_return_val_if_fail(!data || GWY_IS_CONTAINER(data), -1);

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    if (!proxy) {
        g_critical("Data container is unknown to data browser.");
        return -1;
    }

    list = &proxy->lists[PAGE_CHANNELS];
    g_snprintf(key, sizeof(key), "/%d/data", list->last + 1);
    /* This invokes "item-changed" callback that will finish the work.
     * Among other things, it will update proxy->lists[PAGE_CHANNELS].last. */
    gwy_container_set_object_by_name(proxy->container, key, dfield);

    if (showit && !gui_disabled) {
        gwy_app_data_proxy_find_object(list->store, list->last, &iter);
        gwy_app_data_proxy_channel_set_visible(proxy, &iter, TRUE);
    }

    return list->last;
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
    GwyAppDataList *list;
    GtkTreeIter iter;
    GQuark quark;

    g_return_val_if_fail(GWY_IS_GRAPH_MODEL(gmodel), -1);
    g_return_val_if_fail(!data || GWY_IS_CONTAINER(data), -1);

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    if (!proxy) {
        g_critical("Data container is unknown to data browser.");
        return -1;
    }

    list = &proxy->lists[PAGE_GRAPHS];
    quark = gwy_app_get_graph_key_for_id(list->last + 1);
    /* This invokes "item-changed" callback that will finish the work.
     * Among other things, it will update proxy->lists[PAGE_GRAPHS].last. */
    gwy_container_set_object(proxy->container, quark, gmodel);

    if (showit && !gui_disabled) {
        gwy_app_data_proxy_find_object(list->store, list->last, &iter);
        gwy_app_data_proxy_graph_set_visible(proxy, &iter, TRUE);
    }

    return list->last;
}

/**
 * gwy_app_data_browser_add_spectra:
 * @spectra: A spectra object to add.
 * @data: A data container to add @gmodel to.
 *        It can be %NULL to add the spectra to current data container.
 * @showit: %TRUE to display it immediately, %FALSE to just add it.
 *
 * Adds a spectra object to a data container.
 *
 * Returns: The id of the spectra object in the container.
 *
 * Since: 2.7
 **/
gint
gwy_app_data_browser_add_spectra(GwySpectra *spectra,
                                 GwyContainer *data,
                                 gboolean showit)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GwyAppDataList *list;
    GtkTreeIter iter;
    gchar key[32];

    g_return_val_if_fail(GWY_IS_SPECTRA(spectra), -1);
    g_return_val_if_fail(!data || GWY_IS_CONTAINER(data), -1);

    browser = gwy_app_get_data_browser();
    if (data)
        proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    else
        proxy = browser->current;
    if (!proxy) {
        g_critical("Data container is unknown to data browser.");
        return -1;
    }

    list = &proxy->lists[PAGE_SPECTRA];
    g_snprintf(key, sizeof(key), "%s/%d", SPECTRA_PREFIX, list->last + 1);
    /* This invokes "item-changed" callback that will finish the work.
     * Among other things, it will update proxy->lists[PAGE_SPECTRA].last. */
    gwy_container_set_object_by_name(proxy->container, key, spectra);

    if (showit && !gui_disabled) {
        gwy_app_data_proxy_find_object(list->store, list->last, &iter);
        /* FIXME */
        g_warning("Cannot make spectra visible");
        /* gwy_app_data_proxy_spectra_set_visible(proxy, &iter, TRUE); */
    }

    return list->last;
}

static GQuark
gwy_app_get_any_key_for_id(gint id,
                           const gchar *format,
                           guint nquarks,
                           GQuark *quarks)
{
    GQuark q;

    g_return_val_if_fail(id >= 0, 0);
    if (id < nquarks && quarks[id])
        return quarks[id];

    {
        gchar key[48];

        g_snprintf(key, sizeof(key), format, id);
        q = g_quark_from_string(key);
    }

    if (id < nquarks)
        quarks[id] = q;

    return q;
}

/**
 * gwy_app_get_data_key_for_id:
 * @id: Data number in container.
 *
 * Calculates data field quark identifier from its id.
 *
 * Returns: The quark key identifying mask number @id.
 **/
GQuark
gwy_app_get_data_key_for_id(gint id)
{
    static GQuark quarks[16] = { 0, };

    return gwy_app_get_any_key_for_id(id, "/%d/data",
                                      G_N_ELEMENTS(quarks), quarks);
}

/**
 * gwy_app_get_mask_key_for_id:
 * @id: Data number in container.
 *
 * Calculates mask field quark identifier from its id.
 *
 * Returns: The quark key identifying mask number @id.
 **/
GQuark
gwy_app_get_mask_key_for_id(gint id)
{
    static GQuark quarks[16] = { 0, };

    return gwy_app_get_any_key_for_id(id, "/%d/mask",
                                      G_N_ELEMENTS(quarks), quarks);
}

/**
 * gwy_app_get_show_key_for_id:
 * @id: Data number in container.
 *
 * Calculates presentation field quark identifier from its id.
 *
 * Returns: The quark key identifying presentation number @id.
 **/
GQuark
gwy_app_get_show_key_for_id(gint id)
{
    static GQuark quarks[16] = { 0, };

    return gwy_app_get_any_key_for_id(id, "/%d/show",
                                      G_N_ELEMENTS(quarks), quarks);
}

/**
 * gwy_app_get_graph_key_for_id:
 * @id: Graph number in container.
 *
 * Calculates graph model quark identifier from its id.
 *
 * Returns: The quark key identifying graph model number @id.
 *
 * Since: 2.7
 **/
GQuark
gwy_app_get_graph_key_for_id(gint id)
{
    static GQuark quarks[16] = { 0, };

    return gwy_app_get_any_key_for_id(id, GRAPH_PREFIX "/%d",
                                      G_N_ELEMENTS(quarks), quarks);
}

/**
 * gwy_app_get_spectra_key_for_id:
 * @id: Spectra number in container.
 *
 * Calculates spectra quark identifier from its id.
 *
 * Returns: The quark key identifying spectra number @id.
 *
 * Since: 2.7
 **/
GQuark
gwy_app_get_spectra_key_for_id(gint id)
{
    static GQuark quarks[16] = { 0, };

    return gwy_app_get_any_key_for_id(id, SPECTRA_PREFIX "/%d",
                                      G_N_ELEMENTS(quarks), quarks);
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
    g_snprintf(key, sizeof(key), "/%d/data/title", id);
    gwy_container_set_string_by_name(data, key, title);
}

static const gchar*
gwy_app_data_browser_figure_out_channel_title(GwyContainer *data,
                                              gint channel)
{
    const guchar *title = NULL;
    static gchar buf[128];

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    g_return_val_if_fail(channel >= 0, NULL);

    g_snprintf(buf, sizeof(buf), "/%d/data/title", channel);
    gwy_container_gis_string_by_name(data, buf, &title);
    if (!title) {
        g_snprintf(buf, sizeof(buf), "/%d/data/untitled", channel);
        gwy_container_gis_string_by_name(data, buf, &title);
    }
    /* Support 1.x titles */
    if (!title)
        gwy_container_gis_string_by_name(data, "/filename/title", &title);

    if (title)
        return title;

    g_snprintf(buf, sizeof(buf), _("Unknown channel %d"), channel + 1);
    return buf;
}

/**
 * gwy_app_get_data_field_title:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Gets a data channel title.
 *
 * This function should return a reasoanble title for untitled channels,
 * channels with old titles, channels with and without a file, etc.
 *
 * Returns: The channel title as a newly allocated string.
 **/
gchar*
gwy_app_get_data_field_title(GwyContainer *data,
                             gint id)
{
    return g_strdup(gwy_app_data_browser_figure_out_channel_title(data, id));
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
 * it exists (no reference is added), or cleared to %NULL if no such object
 * exists.
 *
 * Quark arguments are set to the corresponding key even if no such object is
 * actually present (use object arguments to check for object presence) but the
 * location where it would be stored is known.  This is common with
 * presentations and masks.  They are be set to 0 if no corresponding location
 * exists -- for example, when the current mask key is requested but the
 * current data contains no channel (or there is no current data at all).
 *
 * The rules for id arguments are similar to quarks, except they are set to -1
 * to indicate undefined result.
 *
 * The current objects can change due to user interaction even during the
 * execution of modal dialogs (typically used by modules).  Therefore to
 * achieve consistency one has to ask for the complete set of current objects
 * at once.
 **/
void
gwy_app_data_browser_get_current(GwyAppWhat what,
                                 ...)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *current = NULL;
    GwyAppDataList *channels = NULL, *graphs = NULL, *spectras = NULL;
    GtkTreeIter iter;
    GObject *object, **otarget;
    /* Cache the current object by type */
    GObject *dfield = NULL, *gmodel = NULL, *spectra = NULL;
    GQuark quark, *qtarget;
    gint *itarget;
    va_list ap;

    if (!what)
        return;

    va_start(ap, what);
    browser = gwy_app_data_browser;
    if (browser) {
        current = browser->current;
        if (current) {
            channels = &current->lists[PAGE_CHANNELS];
            graphs = &current->lists[PAGE_GRAPHS];
            spectras = &current->lists[PAGE_SPECTRA];
        }
    }

    do {
        switch (what) {
            case GWY_APP_CONTAINER:
            otarget = va_arg(ap, GObject**);
            *otarget = current ? G_OBJECT(current->container) : NULL;
            break;

            case GWY_APP_DATA_VIEW:
            otarget = va_arg(ap, GObject**);
            *otarget = NULL;
            if (channels
                && gwy_app_data_proxy_find_object(channels->store,
                                                  channels->active, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(channels->store), &iter,
                                   MODEL_WIDGET, otarget,
                                   -1);
                if (*otarget)
                    g_object_unref(*otarget);
            }
            break;

            case GWY_APP_GRAPH:
            otarget = va_arg(ap, GObject**);
            *otarget = NULL;
            if (graphs
                && gwy_app_data_proxy_find_object(graphs->store,
                                                  graphs->active, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(graphs->store), &iter,
                                   MODEL_WIDGET, otarget,
                                   -1);
                if (*otarget)
                    g_object_unref(*otarget);
            }
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
                && gwy_app_data_proxy_find_object(channels->store,
                                                  channels->active, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(channels->store), &iter,
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
                    quark = gwy_app_get_show_key_for_id(channels->active);
                    gwy_container_gis_object(current->container, quark,
                                             otarget);
                }
                break;

                case GWY_APP_SHOW_FIELD_KEY:
                qtarget = va_arg(ap, GQuark*);
                *qtarget = 0;
                if (dfield)
                    *qtarget = gwy_app_get_show_key_for_id(channels->active);
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
                && gwy_app_data_proxy_find_object(graphs->store,
                                                  graphs->active, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(graphs->store), &iter,
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

            case GWY_APP_SPECTRA:
            case GWY_APP_SPECTRA_KEY:
            case GWY_APP_SPECTRA_ID:
            if (!spectra
                && current
                && gwy_app_data_proxy_find_object(spectras->store,
                                                  spectras->active, &iter)) {
                gtk_tree_model_get(GTK_TREE_MODEL(spectras->store), &iter,
                                   MODEL_OBJECT, &object, -1);
                spectra = object;
                g_object_unref(object);
            }
            switch (what) {
                case GWY_APP_SPECTRA:
                otarget = va_arg(ap, GObject**);
                *otarget = spectra;
                break;

                case GWY_APP_SPECTRA_KEY:
                qtarget = va_arg(ap, GQuark*);
                *qtarget = 0;
                if (spectra)
                    *qtarget = GPOINTER_TO_UINT(g_object_get_qdata
                                                     (spectra, own_key_quark));
                break;

                case GWY_APP_SPECTRA_ID:
                itarget = va_arg(ap, gint*);
                *itarget = spectra ? spectras->active : -1;
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

static gint*
gwy_app_data_list_get_object_ids(GwyContainer *data,
                                 guint pageno,
                                 const gchar *titleglob)
{
    GPatternSpec *pattern = NULL;
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gint *ids;
    gint n;

    browser = gwy_app_get_data_browser();
    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    if (!proxy) {
        g_warning("Nothing is known about Container %p", data);
        /* Returning NULL would likely make the caller crash, avoid that. */
        ids = g_new(gint, 1);
        ids[0] = -1;
        return ids;
    }

    if (titleglob)
        pattern = g_pattern_spec_new(titleglob);

    model = GTK_TREE_MODEL(proxy->lists[pageno].store);
    n = gtk_tree_model_iter_n_children(model, NULL);
    ids = g_new(gint, n+1);
    if (n) {
        n = 0;
        gtk_tree_model_get_iter_first(model, &iter);
        do {
            gboolean ok = FALSE;

            gtk_tree_model_get(model, &iter, MODEL_ID, ids + n, -1);
            if (pattern) {
                if (pageno == PAGE_CHANNELS) {
                    const gchar *title
                        = gwy_app_data_browser_figure_out_channel_title(data,
                                                                        ids[n]);
                    ok = g_pattern_match_string(pattern, title);
                }
                else if (pageno == PAGE_GRAPHS || pageno == PAGE_SPECTRA) {
                    GObject *object;
                    gchar *title;

                    gtk_tree_model_get(model, &iter, MODEL_OBJECT, &object, -1);
                    g_object_get(object, "title", &title, NULL);
                    ok = g_pattern_match_string(pattern, title);
                    g_free(title);
                    g_object_unref(object);
                }
            }
            else
                ok = TRUE;

            if (ok)
                n++;
        } while (gtk_tree_model_iter_next(model, &iter));
    }
    ids[n] = -1;

    if (pattern)
        g_pattern_spec_free(pattern);

    return ids;
}

/**
 * gwy_app_data_browser_get_data_ids:
 * @data: A data container managed by the data-browser.
 *
 * Gets the list of all channels in a data container.
 *
 * Returns: A newly allocated array with channel ids, -1 terminated.
 **/
gint*
gwy_app_data_browser_get_data_ids(GwyContainer *data)
{
    return gwy_app_data_list_get_object_ids(data, PAGE_CHANNELS, NULL);
}

/**
 * gwy_app_data_browser_get_graph_ids:
 * @data: A data container managed by the data-browser.
 *
 * Gets the list of all graphs in a data container.
 *
 * Returns: A newly allocated array with graph ids, -1 terminated.
 **/
gint*
gwy_app_data_browser_get_graph_ids(GwyContainer *data)
{
    return gwy_app_data_list_get_object_ids(data, PAGE_GRAPHS, NULL);
}

/**
 * gwy_app_data_browser_get_spectra_ids:
 * @data: A data container managed by the data-browser.
 *
 * Gets the list of all spectra in a data container.
 *
 * Returns: A newly allocated array with spectrum ids, -1 terminated.
 *
 * Since: 2.7
 **/
gint*
gwy_app_data_browser_get_spectra_ids(GwyContainer *data)
{
    return gwy_app_data_list_get_object_ids(data, PAGE_SPECTRA, NULL);
}

/**
 * gwy_app_find_window_for_channel:
 * @data: A data container to find window for.
 * @id: Data channel id.  It can be -1 to find any data window displaying
 *      a channel from @data.
 *
 * Finds the window displaying a data channel.
 *
 * Returns: The window if found, %NULL if no data window displays the
 *          requested channel.
 **/
GtkWindow*
gwy_app_find_window_for_channel(GwyContainer *data,
                                gint id)
{
    GtkWidget *data_view = NULL, *data_window;
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GwyAppDataList *list;
    GtkTreeModel *model;
    GtkTreeIter iter;

    browser = gwy_app_data_browser;
    if (!browser)
        return NULL;

    proxy = gwy_app_data_browser_get_proxy(browser, data, FALSE);
    if (!proxy)
        return NULL;

    list = &proxy->lists[PAGE_CHANNELS];
    model = GTK_TREE_MODEL(list->store);
    if (id >= 0) {
        if (!gwy_app_data_proxy_find_object(list->store, id, &iter))
            return NULL;

        gtk_tree_model_get(model, &iter, MODEL_WIDGET, &data_view, -1);
    }
    else {
        if (!gtk_tree_model_get_iter_first(model, &iter))
            return NULL;

        do {
            gtk_tree_model_get(model, &iter, MODEL_WIDGET, &data_view, -1);
            if (data_view)
                break;
        } while (gtk_tree_model_iter_next(model, &iter));
    }

    if (!data_view)
        return NULL;

    data_window = gtk_widget_get_ancestor(data_view, GWY_TYPE_DATA_WINDOW);
    g_object_unref(data_view);

    return data_window ? GTK_WINDOW(data_window) : NULL;
}

static void
gwy_app_data_selection_gather(G_GNUC_UNUSED gpointer key,
                              gpointer value,
                              gpointer user_data)
{
    GObject *selection;
    GSList **l = (GSList**)user_data;

    if (!G_VALUE_HOLDS_OBJECT(value))
        return;

    selection = g_value_get_object(value);
    if (GWY_IS_SELECTION(selection)) {
        g_object_ref(selection);
        *l = g_slist_prepend(*l, selection);
    }
}

/**
 * gwy_app_data_clear_selections:
 * @data: A data container.
 * @id: Data channel id.
 *
 * Clears all selections associated with a data channel.
 *
 * This is the preferred selection handling after changes in data geometry
 * as they have generally unpredictable effects on selections.  Selection
 * should not be removed because this is likely to make the current tool stop
 * working.
 **/
void
gwy_app_data_clear_selections(GwyContainer *data,
                              gint id)
{
    gchar buf[28];
    GSList *list = NULL, *l;

    g_snprintf(buf, sizeof(buf), "/%d/select", id);
    /* Afraid of chain reactions when selections are changed inside
     * gwy_container_foreach(), gather them first, then clear */
    gwy_container_foreach(data, buf, &gwy_app_data_selection_gather, &list);
    for (l = list; l; l = g_slist_next(l)) {
        gwy_selection_clear(GWY_SELECTION(l->data));
        gwy_object_unref(l->data);
    }
    g_slist_free(list);
}

/**
 * gwy_app_data_browser_foreach:
 * @function: Function to run on each data container.
 * @user_data: Data to pass as second argument of @function.
 *
 * Calls a function for each data container managed by data browser.
 **/
void
gwy_app_data_browser_foreach(GwyAppDataForeachFunc function,
                             gpointer user_data)
{
    GwyAppDataBrowser *browser;
    GwyAppDataProxy *proxy;
    GList *proxies, *l;

    g_return_if_fail(function);

    browser = gwy_app_data_browser;
    if (!browser)
        return;

    /* The copy is necessary as even innocent functions can move a proxy to
     * list head. */
    proxies = g_list_copy(browser->proxy_list);
    for (l = proxies; l; l = g_list_next(l)) {
        proxy = (GwyAppDataProxy*)l->data;
        function(proxy->container, user_data);
    }
    g_list_free(proxies);
}

/**
 * gwy_app_sync_data_items:
 * @source: Source container.
 * @dest: Target container (may be identical to source).
 * @from_id: Data number to copy items from.
 * @to_id: Data number to copy items to.
 * @delete_too: %TRUE to delete items in target if source does not contain
 *              them, %FALSE to copy only.
 * @...: 0-terminated list of #GwyDataItem values defining the items to copy.
 *
 * Synchronizes auxiliary data items between data containers.
 **/
void
gwy_app_sync_data_items(GwyContainer *source,
                        GwyContainer *dest,
                        gint from_id,
                        gint to_id,
                        gboolean delete_too,
                        ...)
{
    /* FIXME: copy ALL selections */
    static const gchar *sel_keys[] = {
        "point", "pointer", "line", "rectangle", "ellipse",
    };

    GwyDataItem what;
    gchar key_from[40];
    gchar key_to[40];
    const guchar *name;
    GwyRGBA rgba;
    guint enumval;
    gboolean boolval;
    GObject *obj;
    gdouble dbl;
    va_list ap;
    guint i;

    g_return_if_fail(GWY_IS_CONTAINER(source));
    g_return_if_fail(GWY_IS_CONTAINER(dest));
    g_return_if_fail(from_id >= 0 && to_id >= 0);
    if (source == dest && from_id == to_id)
        return;

    va_start(ap, delete_too);
    while ((what = va_arg(ap, GwyDataItem))) {
        switch (what) {
            case GWY_DATA_ITEM_GRADIENT:
            g_snprintf(key_from, sizeof(key_from), "/%d/base/palette", from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/base/palette", to_id);
            if (gwy_container_gis_string_by_name(source, key_from, &name))
                gwy_container_set_string_by_name(dest, key_to, g_strdup(name));
            else if (delete_too)
                gwy_container_remove_by_name(dest, key_to);
            break;

            case GWY_DATA_ITEM_MASK_COLOR:
            g_snprintf(key_from, sizeof(key_from), "/%d/mask", from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/mask", to_id);
            if (gwy_rgba_get_from_container(&rgba, source, key_from))
                gwy_rgba_store_to_container(&rgba, dest, key_to);
            else if (delete_too)
                gwy_rgba_remove_from_container(dest, key_to);
            break;

            case GWY_DATA_ITEM_TITLE:
            g_snprintf(key_from, sizeof(key_from), "/%d/data/title", from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/data/title", to_id);
            if (gwy_container_gis_string_by_name(source, key_from, &name))
                gwy_container_set_string_by_name(dest, key_to, g_strdup(name));
            else if (delete_too)
                gwy_container_remove_by_name(dest, key_to);
            break;

            case GWY_DATA_ITEM_RANGE:
            g_snprintf(key_from, sizeof(key_from), "/%d/base/min", from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/base/min", to_id);
            if (gwy_container_gis_double_by_name(source, key_from, &dbl))
                gwy_container_set_double_by_name(dest, key_to, dbl);
            else if (delete_too)
                gwy_container_remove_by_name(dest, key_to);
            g_snprintf(key_from, sizeof(key_from), "/%d/base/max", from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/base/max", to_id);
            if (gwy_container_gis_double_by_name(source, key_from, &dbl)) {
                gwy_container_set_double_by_name(dest, key_to, dbl);
            }
            else if (delete_too)
                gwy_container_remove_by_name(dest, key_to);
            case GWY_DATA_ITEM_RANGE_TYPE:
            g_snprintf(key_from, sizeof(key_from), "/%d/base/range-type",
                       from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/base/range-type", to_id);
            if (gwy_container_gis_enum_by_name(source, key_from, &enumval))
                gwy_container_set_enum_by_name(dest, key_to, enumval);
            else if (delete_too)
                gwy_container_remove_by_name(dest, key_to);
            break;

            case GWY_DATA_ITEM_REAL_SQUARE:
            g_snprintf(key_from, sizeof(key_from), "/%d/data/realsquare",
                       from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/data/realsquare", to_id);
            if (gwy_container_gis_boolean_by_name(source, key_from, &boolval)
                && boolval)
                gwy_container_set_boolean_by_name(dest, key_to, boolval);
            else if (delete_too)
                gwy_container_remove_by_name(dest, key_to);
            break;

            case GWY_DATA_ITEM_META:
            g_snprintf(key_from, sizeof(key_from), "/%d/meta", from_id);
            g_snprintf(key_to, sizeof(key_to), "/%d/meta", to_id);
            if (gwy_container_gis_object_by_name(source, key_from, &obj)) {
                obj = gwy_serializable_duplicate(obj);
                gwy_container_set_object_by_name(dest, key_to, obj);
                g_object_unref(obj);
            }
            else if (delete_too)
                gwy_container_remove_by_name(dest, key_to);
            break;

            case GWY_DATA_ITEM_SELECTIONS:
            for (i = 0; i < G_N_ELEMENTS(sel_keys); i++) {
                g_snprintf(key_from, sizeof(key_from), "/%d/select/%s",
                           from_id, sel_keys[i]);
                g_snprintf(key_to, sizeof(key_to), "/%d/select/%s",
                           to_id, sel_keys[i]);
                if (gwy_container_gis_object_by_name(source, key_from, &obj)) {
                    obj = gwy_serializable_duplicate(obj);
                    gwy_container_set_object_by_name(dest, key_to, obj);
                    g_object_unref(obj);
                }
                else if (delete_too)
                    gwy_container_remove_by_name(dest, key_to);
            }
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }
    va_end(ap);
}

/**
 * gwy_app_data_browser_copy_channel:
 * @source: Source container.
 * @id: Data channel id.
 * @dest: Target container (may be identical to source).
 *
 * Copies a channel including all auxiliary data.
 *
 * Returns: The id of the copy.
 **/
gint
gwy_app_data_browser_copy_channel(GwyContainer *source,
                                  gint id,
                                  GwyContainer *dest)
{
    GwyDataField *dfield;
    GQuark key;
    gint newid;

    g_return_val_if_fail(GWY_IS_CONTAINER(source), -1);
    g_return_val_if_fail(GWY_IS_CONTAINER(dest), -1);
    key = gwy_app_get_data_key_for_id(id);
    dfield = gwy_container_get_object(source, key);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), -1);

    dfield = gwy_data_field_duplicate(dfield);
    newid = gwy_app_data_browser_add_data_field(dfield, dest, TRUE);
    g_object_unref(dfield);

    key = gwy_app_get_mask_key_for_id(id);
    if (gwy_container_gis_object(source, key, &dfield)) {
        dfield = gwy_data_field_duplicate(dfield);
        key = gwy_app_get_mask_key_for_id(newid);
        gwy_container_set_object(dest, key, dfield);
        g_object_unref(dfield);
    }

    key = gwy_app_get_show_key_for_id(id);
    if (gwy_container_gis_object(source, key, &dfield)) {
        dfield = gwy_data_field_duplicate(dfield);
        key = gwy_app_get_show_key_for_id(newid);
        gwy_container_set_object(dest, key, dfield);
        g_object_unref(dfield);
    }

    gwy_app_sync_data_items(source, dest, id, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            GWY_DATA_ITEM_META,
                            GWY_DATA_ITEM_TITLE,
                            GWY_DATA_ITEM_SELECTIONS,
                            0);

    return newid;
}

/**
 * gwy_app_data_browser_show:
 *
 * Shows the data browser window.
 *
 * If the window does not exist, it is created.
 **/
void
gwy_app_data_browser_show(void)
{
    GwyContainer *settings;

    settings = gwy_app_settings_get();
    gwy_container_set_boolean_by_name(settings, "/app/data-browser/visible",
                                      TRUE);
    gwy_app_data_browser_restore();
}

/**
 * gwy_app_data_browser_restore:
 *
 * Restores the data browser window.
 *
 * The data browser window is always created (if it does not exist).
 * If it should be visible according to settings, is shown at the saved
 * position.  Otherwise it is kept hidden until gwy_app_data_browser_show().
 **/
void
gwy_app_data_browser_restore(void)
{
    GwyAppDataBrowser *browser;
    GwyContainer *settings;
    gboolean visible = TRUE;

    if (gui_disabled)
        return;

    browser = gwy_app_get_data_browser();
    if (!browser->window)
        gwy_app_data_browser_construct_window(browser);

    settings = gwy_app_settings_get();
    gwy_container_gis_boolean_by_name(settings, "/app/data-browser/visible",
                                      &visible);

    if (visible)
        gwy_app_data_browser_show_real(browser);
}

static void
gwy_app_data_browser_show_real(GwyAppDataBrowser *browser)
{
    GtkWindow *window;

    window = GTK_WINDOW(browser->window);

    gwy_app_restore_window_position(window, "/app/data-browser", FALSE);
    gtk_widget_show_all(browser->window);
    gtk_window_present(window);
    gwy_app_restore_window_position(window, "/app/data-browser", FALSE);
}

static void
gwy_app_data_browser_hide_real(GwyAppDataBrowser *browser)
{
    GwyContainer *settings;

    if (!browser || !browser->window || !GTK_WIDGET_VISIBLE(browser->window))
        return;

    gwy_app_save_window_position(GTK_WINDOW(browser->window),
                                 "/app/data-browser", TRUE, TRUE);

    settings = gwy_app_settings_get();
    gwy_container_set_boolean_by_name(settings, "/app/data-browser/visible",
                                      FALSE);
    gtk_widget_hide(browser->window);
}

/**
 * gwy_app_data_browser_shut_down:
 *
 * Releases data browser resources and saves its state.
 **/
void
gwy_app_data_browser_shut_down(void)
{
    GwyAppDataBrowser *browser;
    guint i;

    browser = gwy_app_data_browser;
    if (!browser)
        return;

    if (browser->window && GTK_WIDGET_VISIBLE(browser->window))
        gwy_app_save_window_position(GTK_WINDOW(browser->window),
                                     "/app/data-browser", TRUE, TRUE);

    /* XXX: EXIT-CLEAN-UP */
    /* This clean-up is only to make sure we've got the references right.
     * Remove in production version. */
    while (browser->proxy_list) {
        browser->current = (GwyAppDataProxy*)browser->proxy_list->data;
        browser->current->keep_invisible = FALSE;
        gwy_app_data_browser_close_file(browser);
    }

    if (browser->window) {
        for (i = 0; i < NPAGES; i++)
            gtk_tree_view_set_model(GTK_TREE_VIEW(browser->lists[i]), NULL);
    }
}

/************** FIXME: where this belongs to? ***************************/

enum { BITS_PER_SAMPLE = 8 };

static GwyDataField*
make_thumbnail_field(GwyDataField *dfield,
                     gint *width,
                     gint *height)
{
    gint xres, yres;
    gdouble scale;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    scale = MAX(xres/(gdouble)*width, yres/(gdouble)*height);
    if (scale > 1.0) {
        xres = xres/scale;
        yres = yres/scale;
        xres = CLAMP(xres, 2, *width);
        yres = CLAMP(yres, 2, *height);
        dfield = gwy_data_field_new_resampled(dfield, xres, yres,
                                              GWY_INTERPOLATION_NNA);
    }
    else
        g_object_ref(dfield);

    *width = xres;
    *height = yres;

    return dfield;
}

static GdkPixbuf*
render_data_thumbnail(GwyDataField *dfield,
                      const gchar *gradname,
                      GwyLayerBasicRangeType range_type,
                      gint width,
                      gint height,
                      gdouble *pmin,
                      gdouble *pmax)
{
    GwyDataField *render_field;
    GdkPixbuf *pixbuf;
    GwyGradient *gradient;
    gdouble min, max;

    gradient = gwy_gradients_get_gradient(gradname);
    gwy_resource_use(GWY_RESOURCE(gradient));

    render_field = make_thumbnail_field(dfield, &width, &height);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, BITS_PER_SAMPLE,
                            width, height);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));

    switch (range_type) {
        case GWY_LAYER_BASIC_RANGE_FULL:
        gwy_pixbuf_draw_data_field(pixbuf, render_field, gradient);
        break;

        case GWY_LAYER_BASIC_RANGE_FIXED:
        min = pmin ? *pmin : gwy_data_field_get_min(render_field);
        max = pmax ? *pmax : gwy_data_field_get_max(render_field);
        gwy_pixbuf_draw_data_field_with_range(pixbuf, render_field, gradient,
                                              min, max);
        break;

        case GWY_LAYER_BASIC_RANGE_AUTO:
        gwy_data_field_get_autorange(render_field, &min, &max);
        gwy_pixbuf_draw_data_field_with_range(pixbuf, render_field, gradient,
                                              min, max);
        break;

        case GWY_LAYER_BASIC_RANGE_ADAPT:
        gwy_pixbuf_draw_data_field_adaptive(pixbuf, render_field, gradient);
        break;

        default:
        g_warning("Bad range type: %d", range_type);
        gwy_pixbuf_draw_data_field(pixbuf, render_field, gradient);
        break;
    }
    g_object_unref(render_field);

    gwy_resource_release(GWY_RESOURCE(gradient));

    return pixbuf;
}

static GdkPixbuf*
render_mask_thumbnail(GwyDataField *dfield,
                      const GwyRGBA *color,
                      gint width,
                      gint height)
{
    GwyDataField *render_field;
    GdkPixbuf *pixbuf;

    render_field = make_thumbnail_field(dfield, &width, &height);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, BITS_PER_SAMPLE,
                            width, height);
    gwy_pixbuf_draw_data_field_as_mask(pixbuf, render_field, color);
    g_object_unref(render_field);

    return pixbuf;
}

/**
 * gwy_app_get_channel_thumbnail:
 * @data: A data container.
 * @id: Data channel id.
 * @max_width: Maximum width of the created pixbuf, it must be at least 2.
 * @max_height: Maximum height of the created pixbuf, it must be at least 2.
 *
 * Creates a channel thumbnail.
 *
 * Returns: A newly created pixbuf with channel thumbnail.  It keeps the aspect
 *          ratio of the data field while not exceeding @max_width and
 *          @max_height.
 **/
GdkPixbuf*
gwy_app_get_channel_thumbnail(GwyContainer *data,
                              gint id,
                              gint max_width,
                              gint max_height)
{
    GwyDataField *dfield, *mfield = NULL, *sfield = NULL;
    GwyLayerBasicRangeType range_type = GWY_LAYER_BASIC_RANGE_FULL;
    const guchar *gradient = NULL;
    GdkPixbuf *pixbuf, *mask;
    gdouble min, max;
    gboolean min_set = FALSE, max_set = FALSE;
    GwyRGBA color;
    gchar key[48];

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    g_return_val_if_fail(id >= 0, NULL);
    g_return_val_if_fail(max_width > 1 && max_height > 1, NULL);

    if (!gwy_container_gis_object(data, gwy_app_get_data_key_for_id(id),
                                  &dfield))
        return NULL;

    gwy_container_gis_object(data, gwy_app_get_mask_key_for_id(id), &mfield);
    gwy_container_gis_object(data, gwy_app_get_show_key_for_id(id), &sfield);

    g_snprintf(key, sizeof(key), "/%d/base/palette", id);
    gwy_container_gis_string_by_name(data, key, &gradient);

    if (sfield)
        pixbuf = render_data_thumbnail(sfield, gradient,
                                       GWY_LAYER_BASIC_RANGE_FULL,
                                       max_width, max_height, NULL, NULL);
    else {
        g_snprintf(key, sizeof(key), "/%d/base/range-type", id);
        gwy_container_gis_enum_by_name(data, key, &range_type);
        if (range_type == GWY_LAYER_BASIC_RANGE_FIXED) {
            g_snprintf(key, sizeof(key), "/%d/base/min", id);
            min_set = gwy_container_gis_double_by_name(data, key, &min);
            g_snprintf(key, sizeof(key), "/%d/base/max", id);
            max_set = gwy_container_gis_double_by_name(data, key, &max);
        }
        /* Make thumbnails of images with defects nicer */
        if (range_type == GWY_LAYER_BASIC_RANGE_FULL)
            range_type = GWY_LAYER_BASIC_RANGE_AUTO;

        pixbuf = render_data_thumbnail(dfield, gradient, range_type,
                                       max_width, max_height,
                                       min_set ? &min : NULL,
                                       max_set ? &max : NULL);
    }

    if (mfield) {
        g_snprintf(key, sizeof(key), "/%d/mask", id);
        if (!gwy_rgba_get_from_container(&color, data, key))
            gwy_rgba_get_from_container(&color, gwy_app_settings_get(),
                                        "/mask");
        mask = render_mask_thumbnail(mfield, &color, max_width, max_height);
        gdk_pixbuf_composite(mask, pixbuf,
                             0, 0,
                             gdk_pixbuf_get_width(pixbuf),
                             gdk_pixbuf_get_height(pixbuf),
                             0, 0,
                             1.0, 1.0,
                             GDK_INTERP_NEAREST,
                             255);
        g_object_unref(mask);
    }

    return pixbuf;
}

/* Keep this private.  We cannot render graphs completely off-screen. */
G_GNUC_UNUSED static GdkPixbuf*
gwy_app_get_graph_thumbnail(GwyContainer *data,
                            gint id,
                            gint max_width,
                            gint max_height)
{
    GwyGraphModel *gmodel;

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);
    g_return_val_if_fail(id >= 0, NULL);
    g_return_val_if_fail(max_width > 1 && max_height > 1, NULL);

    if (!gwy_container_gis_object(data, gwy_app_get_graph_key_for_id(id),
                                  &gmodel))
        return NULL;

    /* FIXME: I do not know how to implement this w/o showing windows on
     * screen... */

    return NULL;
}

/**
 * gwy_app_data_browser_get_gui_enabled:
 *
 * Reports whether creation of windows by the data-browser is enabled.
 *
 * Returns: %TRUE if the data-browser is permitted to create windows, %FALSE
 *          if it is not.
 *
 * Since: 2.21
 **/
gboolean
gwy_app_data_browser_get_gui_enabled(void)
{
    return !gui_disabled;
}

/**
 * gwy_app_data_browser_set_gui_enabled:
 * @setting: %TRUE to enable creation of widgets by the data-browser,
 *           %FALSE to disable it.
 *
 * Globally enables or disables creation of widgets by the data-browser.
 *
 * By default, the data-browser creates windows for data objects automatically,
 * for instance when reconstructing view of a loaded file, after a module
 * function creates a new channel or graph or when it is explicitly asked so
 * by gwy_app_data_browser_show_3d().  Non-GUI applications that run module
 * functions usually wish to disable GUI.
 *
 * If GUI is disabled the data browser never creates windows showing data
 * objects and also gwy_app_data_browser_show() becomes no-op.
 *
 * Disabling GUI after widgets have been already created is a bad idea.
 * Hence you should do so before loading files or calling module functions.
 *
 * Since: 2.21
 **/
void
gwy_app_data_browser_set_gui_enabled(gboolean setting)
{
    GwyAppDataBrowser *browser;

    browser = gwy_app_data_browser;
    if (!gui_disabled && !setting && browser && browser->window) {
        g_warning("Disabling GUI when widgets have been already constructed. "
                  "This does not really work.");
        gtk_widget_hide(browser->window);
    }

    gui_disabled = !setting;

}

/**
 * gwy_app_data_browser_find_data_by_title:
 * @data: A data container managed by the data-browser.
 * @titleglob: Pattern, as used by #GPatternSpec, to match the channel titles
 *             against.
 *
 * Gets the list of all channels in a data container whose titles match the
 * specified pattern.
 *
 * Returns: A newly allocated array with channel ids, -1 terminated.
 *
 * Since: 2.21
 **/
gint*
gwy_app_data_browser_find_data_by_title(GwyContainer *data,
                                        const gchar *titleglob)
{
    return gwy_app_data_list_get_object_ids(data, PAGE_CHANNELS, titleglob);
}

/**
 * gwy_app_data_browser_find_graphs_by_title:
 * @data: A data container managed by the data-browser.
 * @titleglob: Pattern, as used by #GPatternSpec, to match the graph titles
 *             against.
 *
 * Gets the list of all graphs in a data container whose titles match the
 * specified pattern.
 *
 * Returns: A newly allocated array with graph ids, -1 terminated.
 *
 * Since: 2.21
 **/
gint*
gwy_app_data_browser_find_graphs_by_title(GwyContainer *data,
                                          const gchar *titleglob)
{
    return gwy_app_data_list_get_object_ids(data, PAGE_GRAPHS, titleglob);
}

/**
 * gwy_app_data_browser_find_spectra_by_title:
 * @data: A data container managed by the data-browser.
 * @titleglob: Pattern, as used by #GPatternSpec, to match the spectra titles
 *             against.
 *
 * Gets the list of all spectra in a data container whose titles match the
 * specified pattern.
 *
 * Returns: A newly allocated array with spectra ids, -1 terminated.
 *
 * Since: 2.21
 **/
gint*
gwy_app_data_browser_find_spectra_by_title(GwyContainer *data,
                                           const gchar *titleglob)
{
    return gwy_app_data_list_get_object_ids(data, PAGE_SPECTRA, titleglob);
}

static void
gwy_app_data_browser_notify_watch(GList *watchers,
                                  GwyContainer *container,
                                  gint id)
{
    g_printerr("NOTIFY WATCH %p %d\n", container, id);
    while (watchers) {
        GwyAppWatcherData *wdata = (GwyAppWatcherData*)watchers->data;
        wdata->function(container, id, wdata->user_data);
        watchers = g_list_next(watchers);
    }
}

/*
 * FIXME:
 * GwyAppDataWatchFunc needs a `what-happened' argument because when a file is
 * removed from data-browser the individual channels are not removed from it
 * (and we cannot remove them because the container can be still owned by
 * something else).  So it is necessary to notify the watcher that an object
 * is removed although it is, in fact, present.
 */
static gulong
gwy_app_data_browser_add_watch(GList **watchers,
                               GwyAppDataWatchFunc function,
                               gpointer user_data)
{
    GwyAppWatcherData *wdata;

    g_return_val_if_fail(function, 0);
    wdata = g_new(GwyAppWatcherData, 1);
    wdata->function = function;
    wdata->user_data = user_data;
    wdata->id = ++watcher_id;
    *watchers = g_list_append(*watchers, wdata);

    return wdata->id;
}

static void
gwy_app_data_browser_remove_watch(GList **watchers,
                                  gulong id)
{
    GList *l;

    for (l = *watchers; l; l = g_list_next(l)) {
        GwyAppWatcherData *wdata = (GwyAppWatcherData*)l->data;

        if (wdata->id == id) {
            *watchers = g_list_delete_link(*watchers, l);
            return;
        }
    }
    g_warning("Cannot find watch with id %lu.", id);
}

/**
 * gwy_app_data_browser_add_channel_watch:
 * @function: Function to call when a channel changes.
 * @user_data: User data to pass to @function.
 *
 * Adds a watch function called when a channel changes.
 *
 * The function is called whenever a channel is added, removed, its data
 * changes or its metadata such as the title changes.  If a channel is removed
 * it no longer exists when the function is called.
 *
 * Returns: The id of the added watch func that can be used to remove it later
 *          using gwy_app_data_browser_remove_channel_watch().
 *
 * Since 2.21.
 **/
gulong
gwy_app_data_browser_add_channel_watch(GwyAppDataWatchFunc function,
                                       gpointer user_data)
{
    return gwy_app_data_browser_add_watch(&channel_watchers,
                                          function, user_data);
}

/**
 * gwy_app_data_browser_remove_channel_watch:
 * @id: Watch function id, as returned by
 *      gwy_app_data_browser_add_channel_watch().
 *
 * Removes a channel watch function.
 *
 * Since: 2.21
 **/
void
gwy_app_data_browser_remove_channel_watch(gulong id)
{
    gwy_app_data_browser_remove_watch(&channel_watchers, id);
}

/*
gulong
gwy_app_data_browser_add_graph_watch(GwyAppDataWatchFunc function,
                                     gpointer user_data)
{
    return gwy_app_data_browser_add_watch(&graph_watchers,
                                          function, user_data);
}

void
gwy_app_data_browser_remove_graph_watch(gulong id)
{
    gwy_app_data_browser_remove_watch(&graph_watchers, id);
}

gulong
gwy_app_data_browser_add_spectra_watch(GwyAppDataWatchFunc function,
                                       gpointer user_data)
{
    return gwy_app_data_browser_add_watch(&spectra_watchers,
                                          function, user_data);
}

void
gwy_app_data_browser_remove_spectra_watch(gulong id)
{
    gwy_app_data_browser_remove_watch(&spectra_watchers, id);
}
*/

/************************** Documentation ****************************/

/**
 * SECTION:data-browser
 * @title: data-browser
 * @short_description: Data browser
 **/

/**
 * GwyAppDataForeachFunc:
 * @data: A data container managed by the data-browser.
 * @user_data: User data passed to gwy_app_data_browser_foreach().
 *
 * Type of function passed to gwy_app_data_browser_foreach().
 **/

/**
 * GwyAppDataWatchFunc:
 * @data: A data container managed by the data-browser.
 * @id: Object (channel) id in the container.
 * @user_data: User data passed to gwy_app_data_browser_add_channel_watch().
 *
 * Type of function passed to gwy_app_data_browser_add_channel_watch().
 *
 * Since: 2.21
 **/

/**
 * GwyVisibilityResetType:
 * @GWY_VISIBILITY_RESET_DEFAULT: Restore visibilities from container and if
 *                                nothing would be visible, make an arbitrary
 *                                data object visible.
 * @GWY_VISIBILITY_RESET_RESTORE: Restore visibilities from container.
 * @GWY_VISIBILITY_RESET_SHOW_ALL: Show all data objects.
 * @GWY_VISIBILITY_RESET_HIDE_ALL: Hide all data objects.  This normally
 *                                 makes the file inaccessible.
 *
 * Data object visibility reset type.
 *
 * The precise behaviour of @GWY_VISIBILITY_RESET_DEFAULT may be subject of
 * further changes.  It indicates the wish to restore saved visibilities
 * and do something reasonable when there are no visibilities to restore.
 **/

/**
 * GwyAppWhat:
 * @GWY_APP_CONTAINER: Data container (corresponds to files).
 * @GWY_APP_DATA_VIEW: Data view widget (shows a channel).
 * @GWY_APP_GRAPH: Graph widget (shows a graph model).
 * @GWY_APP_DATA_FIELD: Data field (channel).
 * @GWY_APP_DATA_FIELD_KEY: Quark corresponding to the data field (channel).
 * @GWY_APP_DATA_FIELD_ID: Number (id) of the data field (channel) in its
 *                         container.
 * @GWY_APP_MASK_FIELD: Mask data field.
 * @GWY_APP_MASK_FIELD_KEY: Quark corresponding to the mask field.
 * @GWY_APP_SHOW_FIELD: Presentation data field.
 * @GWY_APP_SHOW_FIELD_KEY: Quark corresponding to the presentation field.
 * @GWY_APP_GRAPH_MODEL: Graph model.
 * @GWY_APP_GRAPH_MODEL_KEY: Quark corresponding to the graph model.
 * @GWY_APP_GRAPH_MODEL_ID: Number (id) of the graph model in its container.
 * @GWY_APP_SPECTRA: Single point spectra.
 * @GWY_APP_SPECTRA_KEY: Quark corresponding to the single point spectra.
 * @GWY_APP_SPECTRA_ID: Number (id) of the the single point spectra in its
 *                      container.
 *
 * Types of current objects that can be requested with
 * gwy_app_data_browser_get_current().
 **/

/**
 * GwyDataItem:
 * @GWY_DATA_ITEM_GRADIENT: Color gradient.
 * @GWY_DATA_ITEM_PALETTE: An alias of @GWY_DATA_ITEM_GRADIENT.
 * @GWY_DATA_ITEM_MASK_COLOR: Mask color components.
 * @GWY_DATA_ITEM_TITLE: Channel title.
 * @GWY_DATA_ITEM_RANGE: Range type and range boundaries.
 * @GWY_DATA_ITEM_RANGE_TYPE: Range type.
 * @GWY_DATA_ITEM_REAL_SQUARE: Physical/pixel aspect ratio mode.
 * @GWY_DATA_ITEM_SELECTIONS: Data selections.
 * @GWY_DATA_ITEM_META: Metadata.
 *
 * Auxiliary channel data type.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
