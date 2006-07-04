/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/data-browser.h>
#include <app/datachooser.h>

/*****************************************************************************
 *
 * Declaration.  Do not make it public yet.
 *
 *****************************************************************************/

struct _GwyDataChooser {
    GtkComboBox parent_instance;

    GtkTreeModel *filter;
    GtkListStore *store;

    GwyDataChooserFilterFunc filter_func;
    gpointer filter_data;
    GtkDestroyNotify filter_destroy;

    GtkTreeIter none_iter;
    gchar *none_label;
};

struct _GwyDataChooserClass {
    GtkComboBoxClass parent_class;
};

/*****************************************************************************
 *
 * Implementation.
 *
 *****************************************************************************/

/* To avoid "row-changed" when values are actually filled in.  Filling rows
 * inside a cell renderer causes an obscure Gtk+ crash. */
typedef struct {
    GdkPixbuf *thumb;
    gchar *name;
} Proxy;

enum {
    MODEL_COLUMN_CONTAINER,
    MODEL_COLUMN_ID,
    MODEL_COLUMN_PROXY,
    MODEL_NCOLUMNS
};

static void     gwy_data_chooser_finalize       (GObject *object);
static void     gwy_data_chooser_destroy        (GtkObject *object);
static gboolean gwy_data_chooser_is_visible     (GtkTreeModel *model,
                                                 GtkTreeIter *iter,
                                                 gpointer data);
static void     gwy_data_chooser_choose_whatever(GwyDataChooser *chooser);

G_DEFINE_TYPE(GwyDataChooser, gwy_data_chooser, GTK_TYPE_COMBO_BOX)

static void
gwy_data_chooser_class_init(GwyDataChooserClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    object_class->destroy = gwy_data_chooser_destroy;

    gobject_class->finalize = gwy_data_chooser_finalize;
}

static void
gwy_data_chooser_finalize(GObject *object)
{
    GwyDataChooser *chooser;

    chooser = GWY_DATA_CHOOSER(object);

    g_free(chooser->none_label);
    if (chooser->filter_destroy)
        chooser->filter_destroy(chooser->filter_data);

    G_OBJECT_CLASS(gwy_data_chooser_parent_class)->finalize(object);
}

static void
gwy_data_chooser_free_proxies(GtkListStore *store)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    Proxy *proxy;

    model = GTK_TREE_MODEL(store);
    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        gtk_tree_model_get(model, &iter, MODEL_COLUMN_PROXY, &proxy, -1);
        gwy_object_unref(proxy->thumb);
        g_free(proxy->name);
        g_free(proxy);
    } while (gtk_tree_model_iter_next(model, &iter));
}

static void
gwy_data_chooser_destroy(GtkObject *object)
{
    GwyDataChooser *chooser;
    GtkComboBox *combo;
    GtkTreeModel *model;

    chooser = GWY_DATA_CHOOSER(object);
    combo = GTK_COMBO_BOX(object);
    model = gtk_combo_box_get_model(combo);
    if (model) {
        gwy_data_chooser_free_proxies(chooser->store);
        gtk_combo_box_set_model(combo, NULL);
        gwy_object_unref(chooser->filter);
        gwy_object_unref(chooser->store);
    }

    GTK_OBJECT_CLASS(gwy_data_chooser_parent_class)->destroy(object);
}

static void
gwy_data_chooser_init(GwyDataChooser *chooser)
{
    GtkTreeModelFilter *filter;
    GtkComboBox *combo;
    GtkTreeIter iter;
    Proxy *proxy;

    chooser->store = gtk_list_store_new(MODEL_NCOLUMNS,
                                        GWY_TYPE_CONTAINER,
                                        G_TYPE_INT,
                                        G_TYPE_POINTER);
    chooser->filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(chooser->store),
                                                NULL);
    filter = GTK_TREE_MODEL_FILTER(chooser->filter);
    gtk_tree_model_filter_set_visible_func(filter,
                                           gwy_data_chooser_is_visible, chooser,
                                           NULL);

    /* Create `none' row */
    proxy = g_new0(Proxy, 1);
    /* XXX: size */
    proxy->thumb = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 20, 20);
    gdk_pixbuf_fill(proxy->thumb, 0x00000000);
    proxy->name = g_strdup(_("None"));
    gtk_list_store_insert_with_values(chooser->store, &iter, 0,
                                      MODEL_COLUMN_ID, -1,
                                      MODEL_COLUMN_PROXY, proxy,
                                      -1);

    combo = GTK_COMBO_BOX(chooser);
    gtk_combo_box_set_model(combo, chooser->filter);
    gtk_combo_box_set_wrap_width(combo, 1);
}

/**
 * gwy_data_chooser_set_active:
 * @chooser: A data chooser.
 * @data: Container to select, %NULL to select none (if the chooser contains
 *        `none' item).
 * @id: Id of particular data to select in @data.
 *
 * Selects a data in a data chooser.
 *
 * Returns: %TRUE if selected item was set.
 **/
gboolean
gwy_data_chooser_set_active(GwyDataChooser *chooser,
                            GwyContainer *data,
                            gint id)
{
    GwyContainer *container;
    GtkComboBox *combo;
    GtkTreeIter iter;
    gint dataid;

    g_return_val_if_fail(GWY_IS_DATA_CHOOSER(chooser), FALSE);

    if (!gtk_tree_model_get_iter_first(chooser->filter, &iter))
        return FALSE;

    combo = GTK_COMBO_BOX(chooser);
    if (!data) {
        if (chooser->none_label) {
            gtk_combo_box_set_active_iter(combo, &iter);
            return TRUE;
        }
        return FALSE;
    }

    do {
        gtk_tree_model_get(chooser->filter, &iter,
                           MODEL_COLUMN_CONTAINER, &container,
                           MODEL_COLUMN_ID, &dataid,
                           -1);
        g_object_unref(container);
        if (container == data && dataid == id) {
            gtk_combo_box_set_active_iter(combo, &iter);
            return TRUE;
        }
    } while (gtk_tree_model_iter_next(chooser->filter, &iter));

    return FALSE;
}

/**
 * gwy_data_chooser_get_active:
 * @chooser: A data chooser.
 * @id: Location to store selected data id to (may be %NULL).
 *
 * Gets the selected item in a data chooser.
 *
 * Returns: The container selected data lies in, %NULL if nothing is selected
 *          or `none' item is selected.
 **/
GwyContainer*
gwy_data_chooser_get_active(GwyDataChooser *chooser,
                            gint *id)
{
    GwyContainer *container;
    GtkComboBox *combo;
    GtkTreeIter iter;
    gint dataid;

    g_return_val_if_fail(GWY_IS_DATA_CHOOSER(chooser), NULL);

    combo = GTK_COMBO_BOX(chooser);
    if (!gtk_combo_box_get_active_iter(combo, &iter))
        return NULL;

    gtk_tree_model_get(chooser->filter, &iter,
                       MODEL_COLUMN_CONTAINER, &container,
                       MODEL_COLUMN_ID, &dataid,
                       -1);
    if (container)
        g_object_unref(container);

    if (id)
        *id = dataid;

    return container;
}

static gboolean
gwy_data_chooser_is_visible(GtkTreeModel *model,
                            GtkTreeIter *iter,
                            gpointer data)
{
    GwyDataChooser *chooser = (GwyDataChooser*)data;
    GwyContainer *container;
    guint id;

    gtk_tree_model_get(model, iter,
                       MODEL_COLUMN_CONTAINER, &container,
                       MODEL_COLUMN_ID, &id,
                       -1);

    /* Handle `none' explicitly */
    if (!container)
        return chooser->none_label != NULL;

    g_object_unref(container);
    if (!chooser->filter_func)
        return TRUE;

    return chooser->filter_func(container, id, chooser->filter_data);
}

/**
 * gwy_data_chooser_set_filter:
 * @chooser: A data chooser.
 * @filter: The filter function.
 * @user_data: The data passed to @filter.
 * @destroy: Destroy notifier of @user_data or %NULL.
 *
 * Sets the filter applied to a data chooser.
 *
 * The display of an item corresponding to no data is controlled by
 * gwy_data_chooser_set_none(), @filter function is only called for real data.
 **/
void
gwy_data_chooser_set_filter(GwyDataChooser *chooser,
                            GwyDataChooserFilterFunc filter,
                            gpointer user_data,
                            GtkDestroyNotify destroy)
{
    g_return_if_fail(GWY_IS_DATA_CHOOSER(chooser));

    if (chooser->filter_destroy)
        chooser->filter_destroy(chooser->filter_data);

    chooser->filter_func = filter;
    chooser->filter_data = user_data;
    chooser->filter_destroy = destroy;

    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(chooser->filter));
    gwy_data_chooser_choose_whatever(chooser);
}

/**
 * gwy_data_chooser_get_none:
 * @chooser: A data chooser.
 *
 * Gets the label of the item corresponding to no data.
 *
 * Returns: The label corresponding to no data, an empty string for the default
 *          label and %NULL if the chooser does not display `none' item.
 **/
const gchar*
gwy_data_chooser_get_none(GwyDataChooser *chooser)
{
    g_return_val_if_fail(GWY_IS_DATA_CHOOSER(chooser), NULL);

    return chooser->none_label;
}

/**
 * gwy_data_chooser_set_none:
 * @chooser: A data chooser.
 * @none: Label to use for item corresponding to no data. Passing %NULL,
 *        disables such an item, an empty string enables it with
 *        the default label.
 *
 * Sets the label of the item corresponding to no data.
 **/
void
gwy_data_chooser_set_none(GwyDataChooser *chooser,
                          const gchar *none)
{
    gchar *old_none;
    GtkTreeIter iter;
    Proxy *proxy;

    g_return_if_fail(GWY_IS_DATA_CHOOSER(chooser));
    old_none = chooser->none_label;
    chooser->none_label = g_strdup(none);
    g_free(old_none);

    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(chooser->store), &iter);
    gtk_tree_model_get(GTK_TREE_MODEL(chooser->store), &iter,
                       MODEL_COLUMN_PROXY, &proxy,
                       -1);
    g_free(proxy->name);
    if (chooser->none_label && *chooser->none_label)
        proxy->name = g_strdup(chooser->none_label);
    else
        proxy->name = g_strdup(_("None"));
    gwy_list_store_row_changed(chooser->store, &iter, NULL, 0);

    gwy_data_chooser_choose_whatever(chooser);
}

/**
 * gwy_data_chooser_choose_whatever:
 * @chooser: A data chooser.
 *
 * Choose arbitrary item if none is active.
 **/
static void
gwy_data_chooser_choose_whatever(GwyDataChooser *chooser)
{
    GtkComboBox *combo;
    GtkTreeIter iter;

    combo = GTK_COMBO_BOX(chooser);
    if (gtk_combo_box_get_active_iter(combo, &iter))
        return;

    if (gtk_tree_model_get_iter_first(chooser->filter, &iter))
        gtk_combo_box_set_active_iter(combo, &iter);
}

/*****************************************************************************
 *
 * Channels.
 *
 *****************************************************************************/

static void
gwy_data_chooser_channels_fill(GwyContainer *data,
                               gpointer user_data)
{
    GtkListStore *store;
    GtkTreeIter iter;
    Proxy *proxy;
    gint *ids;
    gint i;

    store = GWY_DATA_CHOOSER(user_data)->store;
    ids = gwy_app_data_browser_get_data_ids(data);
    for (i = 0; ids[i] >= 0; i++) {
        gwy_debug("inserting %p %d", data, ids[i]);
        proxy = g_new0(Proxy, 1);
        gtk_list_store_insert_with_values(store, &iter, G_MAXINT,
                                          MODEL_COLUMN_CONTAINER, data,
                                          MODEL_COLUMN_ID, ids[i],
                                          MODEL_COLUMN_PROXY, proxy,
                                          -1);
    }
    g_free(ids);
}

static void
gwy_data_chooser_channels_render_name(G_GNUC_UNUSED GtkCellLayout *layout,
                                      GtkCellRenderer *renderer,
                                      GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
    GwyDataChooser *chooser;
    GwyContainer *container;
    Proxy *proxy;
    gint id;

    gtk_tree_model_get(model, iter, MODEL_COLUMN_PROXY, &proxy, -1);
    if (!proxy->name) {
        chooser = (GwyDataChooser*)data;
        gtk_tree_model_get(model, iter,
                           MODEL_COLUMN_CONTAINER, &container,
                           MODEL_COLUMN_ID, &id,
                           -1);
        proxy->name = gwy_app_get_data_field_title(container, id);
        g_object_unref(container);
    }
    g_object_set(renderer, "text", proxy->name, NULL);
}

static void
gwy_data_chooser_channels_render_icon(G_GNUC_UNUSED GtkCellLayout *layout,
                                      GtkCellRenderer *renderer,
                                      GtkTreeModel *model,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
    GwyDataChooser *chooser;
    GwyContainer *container;
    gint id, width, height;
    Proxy *proxy;

    /* FIXME */
    width = height = 20;

    gtk_tree_model_get(model, iter, MODEL_COLUMN_PROXY, &proxy, -1);
    if (!proxy->thumb) {
        chooser = (GwyDataChooser*)data;
        gtk_tree_model_get(model, iter,
                           MODEL_COLUMN_CONTAINER, &container,
                           MODEL_COLUMN_ID, &id,
                           -1);
        proxy->thumb = gwy_app_get_channel_thumbnail(container, id,
                                                     width, height);
        g_object_unref(container);
    }
    g_object_set(renderer, "pixbuf", proxy->thumb, NULL);
}

/**
 * gwy_data_chooser_new_channels:
 *
 * Creates a data chooser for data channels.
 *
 * Returns: A new channel chooser.  Nothing may be assumed about the type and
 *          properties of the returned widget as they can change in the future.
 **/
GtkWidget*
gwy_data_chooser_new_channels(void)
{
    GwyDataChooser *chooser;
    GtkCellRenderer *renderer;
    GtkCellLayout *layout;

    chooser = (GwyDataChooser*)g_object_new(GWY_TYPE_DATA_CHOOSER, NULL);
    gwy_app_data_browser_foreach(gwy_data_chooser_channels_fill, chooser);
    layout = GTK_CELL_LAYOUT(chooser);

    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(layout, renderer, FALSE);
    gtk_cell_layout_set_cell_data_func(layout, renderer,
                                       gwy_data_chooser_channels_render_icon,
                                       chooser, NULL);

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    gtk_cell_layout_pack_start(layout, renderer, TRUE);
    gtk_cell_layout_set_cell_data_func(layout, renderer,
                                       gwy_data_chooser_channels_render_name,
                                       chooser, NULL);

    gwy_data_chooser_choose_whatever(chooser);

    return (GtkWidget*)chooser;
}

/************************** Documentation ****************************/

/**
 * SECTION:datachooser
 * @title: GwyDataChooser
 * @short_description: Data object choosers
 *
 * #GwyDataChooser is an base data object chooser class.  Choosers for
 * particular data objects can be created with functions like
 * gwy_data_chooser_new_channels() and then manipulated through
 * #GwyDataChooser interface.
 *
 * The widget type used to implement choosers is not a part of the interface
 * and may be subject of future changes.  In any case #GwyDataChooser has
 * a <code>"changed"</code> signal emitted when the selected item changes.
 *
 * It is possible to offer only data objects matching some criteria.  For
 * example to offer only data fields compatible with another data field,
 * one can use:
 * <informalexample><programlisting>
 * GtkWidget *chooser;
 * GwyDataField *model;
 * <!-- Hello, gtk-doc! -->
 * model = ...;
 * chooser = gwy_data_chooser_new_channels(<!-- Hello, gtk-doc! -->);
 * gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
 *                             compatible_field_filter, model,
 *                             NULL);
 * </programlisting></informalexample>
 * where the filter function looks like
 * <informalexample><programlisting>
 * static gboolean
 * compatible_field_filter(GwyContainer *data,
 *                         gint id,
 *                         gpointer user_data)
 * {
 *     GwyDataField *model, *data_field;
 *     GQuark quark;
 *     <!-- Hello, gtk-doc! -->
 *     quark = gwy_app_get_data_key_for_id(id);
 *     data_field = gwy_container_get_object(data, quark);
 *     model = GWY_DATA_FIELD(user_data);
 *     return !gwy_data_field_check_compatibility
 *                          (data_field, model,
 *                           GWY_DATA_COMPATIBILITY_RES
 *                           | GWY_DATA_COMPATIBILITY_REAL
 *                           | GWY_DATA_COMPATIBILITY_LATERAL
 *                           | GWY_DATA_COMPATIBILITY_VALUE);
 * }
 * </programlisting></informalexample>
 **/

/**
 * GwyDataChooserFilterFunc:
 * @data: Data container.
 * @id: Id of particular data in @data.
 * @user_data: Data passed to gwy_data_chooser_set_filter().
 *
 * The type of data chooser filter function.
 *
 * Returns: %TRUE to display this data in the chooser, %FALSE to omit it.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

