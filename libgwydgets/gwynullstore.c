/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwydgets/gwynullstore.h>

#define GWY_IMPLEMENT_TREE_MODEL(iface_init) \
    { \
        static const GInterfaceInfo gwy_tree_model_iface_info = { \
            (GInterfaceInitFunc)iface_init, NULL, NULL \
        }; \
        g_type_add_interface_static(g_define_type_id, \
                                    GTK_TYPE_TREE_MODEL, \
                                    &gwy_tree_model_iface_info); \
    }

static void              gwy_null_store_finalize       (GObject *object);
static void            gwy_null_store_tree_model_init(GtkTreeModelIface *iface);
static GtkTreeModelFlags gwy_null_store_get_flags      (GtkTreeModel *model);
static gint              gwy_null_store_get_n_columns  (GtkTreeModel *model);
static GType             gwy_null_store_get_column_type(GtkTreeModel *model,
                                                        gint column);
static gboolean          gwy_null_store_get_tree_iter  (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreePath *path);
static GtkTreePath*      gwy_null_store_get_path       (GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static void              gwy_null_store_get_value      (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        gint column,
                                                        GValue *value);
static gboolean          gwy_null_store_iter_next      (GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gboolean          gwy_null_store_iter_children  (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *parent);
static gboolean          gwy_null_store_iter_has_child (GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gint              gwy_null_store_iter_n_children(GtkTreeModel *model,
                                                        GtkTreeIter *iter);
static gboolean          gwy_null_store_iter_nth_child (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *parent,
                                                        gint n);
static gboolean          gwy_null_store_iter_parent    (GtkTreeModel *model,
                                                        GtkTreeIter *iter,
                                                        GtkTreeIter *child);

G_DEFINE_TYPE_EXTENDED
    (GwyNullStore, gwy_null_store, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_TREE_MODEL(gwy_null_store_tree_model_init))

static void
gwy_null_store_class_init(GwyNullStoreClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_null_store_finalize;
}

static void
gwy_null_store_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = gwy_null_store_get_flags;
    iface->get_n_columns = gwy_null_store_get_n_columns;
    iface->get_column_type = gwy_null_store_get_column_type;
    iface->get_iter = gwy_null_store_get_tree_iter;
    iface->get_path = gwy_null_store_get_path;
    iface->get_value = gwy_null_store_get_value;
    iface->iter_next = gwy_null_store_iter_next;
    iface->iter_children = gwy_null_store_iter_children;
    iface->iter_has_child = gwy_null_store_iter_has_child;
    iface->iter_n_children = gwy_null_store_iter_n_children;
    iface->iter_nth_child = gwy_null_store_iter_nth_child;
    iface->iter_parent = gwy_null_store_iter_parent;
}

static void
gwy_null_store_init(GwyNullStore *store)
{
    gwy_debug_objects_creation(G_OBJECT(store));
    store->stamp = g_random_int();
}

static void
gwy_null_store_finalize(GObject *object)
{
    GwyNullStore *store;
    GDestroyNotify d;

    store = GWY_NULL_STORE(object);

    d = store->model_destroy;
    store->model_destroy = NULL;
    if (d)
        d(store->model);

    G_OBJECT_CLASS(gwy_null_store_parent_class)->finalize(object);
}

static GtkTreeModelFlags
gwy_null_store_get_flags(G_GNUC_UNUSED GtkTreeModel *model)
{
    return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gwy_null_store_get_n_columns(G_GNUC_UNUSED GtkTreeModel *model)
{
    return 1;
}

static GType
gwy_null_store_get_column_type(G_GNUC_UNUSED GtkTreeModel *model,
                               gint column)
{
    g_return_val_if_fail(column == 0, 0);

    return G_TYPE_UINT;
}

static gboolean
gwy_null_store_get_tree_iter(GtkTreeModel *model,
                             GtkTreeIter *iter,
                             GtkTreePath *path)
{
    GwyNullStore *store;
    guint i;

    g_return_val_if_fail(gtk_tree_path_get_depth(path) > 0, FALSE);
    store = GWY_NULL_STORE(model);

    i = gtk_tree_path_get_indices(path)[0];
    if (i >= store->n)
        return FALSE;

    /* GwyNullStore has of course presistent iters.
     *
     * @stamp is set upon initialization.
     * @user_data is the row index.
     * @user_data2 in unused.
     */
    iter->stamp = store->stamp;
    iter->user_data = GUINT_TO_POINTER(i);

    return TRUE;
}

static GtkTreePath*
gwy_null_store_get_path(G_GNUC_UNUSED GtkTreeModel *model,
                        GtkTreeIter *iter)
{
    GtkTreePath *path;

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, GPOINTER_TO_UINT(iter->user_data));

    return path;
}

static void
gwy_null_store_get_value(GtkTreeModel *model,
                         GtkTreeIter *iter,
                         gint column,
                         GValue *value)
{
    GwyNullStore *store;
    guint i;

    store = GWY_NULL_STORE(model);
    i = GPOINTER_TO_UINT(iter->user_data);

    g_return_if_fail(column == 0);
    g_return_if_fail(i < store->n);

    g_value_init(value, G_TYPE_UINT);
    g_value_set_uint(value, i);
}

static gboolean
gwy_null_store_iter_next(GtkTreeModel *model,
                         GtkTreeIter *iter)
{
    GwyNullStore *store;
    guint i;

    store = GWY_NULL_STORE(model);
    i = GPOINTER_TO_UINT(iter->user_data) + 1;
    iter->user_data = GUINT_TO_POINTER(i);

    return i < store->n;
}

static gboolean
gwy_null_store_iter_children(GtkTreeModel *model,
                             GtkTreeIter *iter,
                             GtkTreeIter *parent)
{
    GwyNullStore *store;

    store = GWY_NULL_STORE(model);
    if (!parent || !store->n)
        return FALSE;

    iter->stamp = store->stamp;
    iter->user_data = GUINT_TO_POINTER(0);

    return TRUE;
}

static gboolean
gwy_null_store_iter_has_child(G_GNUC_UNUSED GtkTreeModel *model,
                              G_GNUC_UNUSED GtkTreeIter *iter)
{
    return FALSE;
}

static gint
gwy_null_store_iter_n_children(GtkTreeModel *model,
                               GtkTreeIter *iter)
{
    GwyNullStore *store;

    store = GWY_NULL_STORE(model);
    if (iter)
        return 0;

    return store->n;
}

static gboolean
gwy_null_store_iter_nth_child(GtkTreeModel *model,
                              GtkTreeIter *iter,
                              GtkTreeIter *parent,
                              gint n)
{
    GwyNullStore *store;

    store = GWY_NULL_STORE(model);
    if (parent || (guint)n >= store->n)
        return FALSE;

    iter->stamp = store->stamp;
    iter->user_data = GUINT_TO_POINTER((guint)n);
    iter->user_data2 = NULL;

    return TRUE;
}

static gboolean
gwy_null_store_iter_parent(G_GNUC_UNUSED GtkTreeModel *model,
                           G_GNUC_UNUSED GtkTreeIter *iter,
                           G_GNUC_UNUSED GtkTreeIter *child)
{
    return FALSE;
}

/**
 * gwy_null_store_new:
 * @n: The initial number of rows.
 *
 * Creates a new #GtkTreeModel wrapper around nothing.
 *
 * Returns: The newly created null store.
 **/
GwyNullStore*
gwy_null_store_new(guint n)
{
    GwyNullStore *store;

    store = (GwyNullStore*)g_object_new(GWY_TYPE_NULL_STORE, NULL);
    store->n = n;

    return store;
}

/**
 * gwy_null_store_get_n_rows:
 * @store: A null store.
 *
 * Gets the number of imaginary rows in a null store.
 *
 * This is a convenience function, the same information can be obtained with
 * gtk_tree_model_iter_n_children().
 *
 * Returns: The number of rows.
 **/
guint
gwy_null_store_get_n_rows(GwyNullStore *store)
{
    g_return_val_if_fail(GWY_IS_NULL_STORE(store), 0);

    return store->n;
}

/**
 * gwy_null_store_set_n_rows:
 * @store: A null store.
 * @n: The new number of rows.
 *
 * Sets the number of imaginary rows in a null store.
 *
 * If the new number of rows is larger than the current one, rows will be
 * sequentially and virtually appended to the end of the store until the
 * requested number of rows is reached.
 *
 * Similarly, if the new number of rows is smaller then the current one, rows
 * will be sequentially and virtually deleted from the end of the store until
 * the requested number of rows is reached.
 *
 * Note for radical changes it is usually more useful to disconnect the model
 * from its view(s), change the number of rows, and then reconnect.
 **/
void
gwy_null_store_set_n_rows(GwyNullStore *store,
                          guint n)
{
    GtkTreeModel *model;
    GtkTreePath *path;
    GtkTreeIter iter;

    g_return_if_fail(GWY_IS_NULL_STORE(store));
    if (store->n == n)
        return;

    path = gtk_tree_path_new();
    model = GTK_TREE_MODEL(store);

    if (store->n > n) {
        while (store->n > n) {
            store->n--;
            gtk_tree_path_append_index(path, store->n);
            gtk_tree_model_row_deleted(model, path);
            gtk_tree_path_up(path);
        }
    }
    else {
        iter.stamp = store->stamp;
        while (store->n < n) {
            iter.user_data = GUINT_TO_POINTER(store->n);
            gtk_tree_path_append_index(path, store->n);
            store->n++;
            gtk_tree_model_row_inserted(model, path, &iter);
            gtk_tree_path_up(path);
        }
    }

    gtk_tree_path_free(path);
}

/**
 * gwy_null_store_get_model:
 * @store: A null store.
 *
 * Gets the model pointer of a null store.
 *
 * Returns: The pointer set with gwy_null_store_set_model().
 **/
gpointer
gwy_null_store_get_model(GwyNullStore *store)
{
    g_return_val_if_fail(GWY_IS_NULL_STORE(store), NULL);
    return store->model;
}

/**
 * gwy_null_store_set_model:
 * @store: A null store.
 * @model: Model pointer.
 * @destroy: Function to call on @model when it is replaced or the store is
 *           destroyed.
 *
 * Sets the model pointer of a null store.
 *
 * While the virtual integers in #GwyNullStore can be used directly, a null
 * store typically serves as an adaptor for array-like structures and its rows
 * are used as indices to these structures.  This helper method provides means
 * to attach such a structure to a null store in the common case.
 *
 * The store itself does not interpret nor access the attached data by any
 * means.  No signals are emitted in response to the model pointer change
 * either, particularly because it is expected to be set only once upon
 * creation (null stores are cheap).
 *
 * You are free to keep the model pointer at %NULL if these functions do not
 * suit your needs.
 **/
void
gwy_null_store_set_model(GwyNullStore *store,
                         gpointer model,
                         GDestroyNotify destroy)
{
    GDestroyNotify d;

    g_return_if_fail(GWY_IS_NULL_STORE(store));

    d = store->model_destroy;
    store->model_destroy = NULL;
    if (d)
        d(store->model);

    store->model = model;
    store->model_destroy = destroy;
}

/**
 * gwy_null_store_row_changed:
 * @store: A null store.
 * @i: A row to emit "row-changed" on.
 *
 * Emits "GtkTreeModel::row-changed" signal on a null store.
 *
 * This is a convenience method, with a bit more work the same effect can be
 * achieved with gtk_tree_model_row_changed().
 **/
void
gwy_null_store_row_changed(GwyNullStore *store,
                           guint i)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    g_return_if_fail(GWY_IS_NULL_STORE(store));
    g_return_if_fail(i < store->n);

    iter.stamp = store->stamp;
    iter.user_data = GUINT_TO_POINTER(i);

    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
    gtk_tree_path_free(path);
}

/**
 * gwy_null_store_iter_is_valid:
 * @store: A null store.
 * @iter: A #GtkTreeIter.
 *
 * Checks if the given iter is a valid iter for this null store.
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 **/
gboolean
gwy_null_store_iter_is_valid(GwyNullStore *store,
                             GtkTreeIter *iter)
{
    g_return_val_if_fail(GWY_IS_NULL_STORE(store), FALSE);

    if (!iter
        || iter->stamp != store->stamp
        || GPOINTER_TO_UINT(iter->user_data) >= store->n)
        return FALSE;

    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwynullstore
 * @title: GwyNullStore
 * @short_description: #GtkTreeModel wrapper around nothing
 * @see_also: #GwyInventoryStore -- #GtkTreeModel wrapper around #GwyInventory
 *
 * #GwyNullStore is a very simple class which pretends to be a #GtkTreeModel
 * with one column of type %G_TYPE_UINT whose values are equal to row numbers
 * (counted from 0).  In reality the column is purely virtual and the store
 * always takes up only a small constant amount of memory.
 *
 * The purpose of #GwyNullStore is to provide a low-overhead #GtkTreeModel
 * interface for array-like (and other indexed) data structures.
 *
 * A new null store can be created with gwy_null_store_new(), then number of
 * virtual rows can be controlled with gwy_null_store_set_n_rows().  For
 * convenience, a method to emit "row-changed" signal on a row by its index is
 * provided: gwy_null_store_row_changed().
 *
 * Since null stores often serve as wrappers around other data structures,
 * convenience methods to attach and obtain such a data are provided:
 * gwy_null_store_set_model(), gwy_null_store_get_model().
 *
 * A simple example to create a multiplication table with null storage:
 * <informalexample><programlisting>
 *  GtkWidget *treeview;
 *  GtkTreeViewColumn *column;
 *  GtkCellRenderer *renderer;
 *  GwyNullStore *store;
 *  gint i;
 *  <!-- Hello, gtk-doc! -->
 *  store = gwy_null_store_new(10);
 *  treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
 *  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), FALSE);
 *  <!-- Hello, gtk-doc! -->
 *  column = gtk_tree_view_column_new<!-- -->();
 *  for (i = 1; i <= 10; i++) {
 *      renderer = gtk_cell_renderer_text_new<!-- -->();
 *      g_object_set(renderer, "xalign", 1.0, "width-chars", 4, NULL);
 *      gtk_tree_view_column_pack_start(column, renderer, TRUE);
 *      gtk_tree_view_column_set_cell_data_func(column, renderer,
 *                                              multiply, GINT_TO_POINTER(i),
 *                                              NULL);
 *  }
 *  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
 * </programlisting></informalexample>
 *
 * The cell data function multiply<!-- -->() just multiplies the column number
 * with the number of (virtual) null store row:
 * <informalexample><programlisting>
 * static void
 * multiply(GtkTreeViewColumn *column,
 *          GtkCellRenderer *renderer,
 *          GtkTreeModel *model,
 *          GtkTreeIter *iter,
 *          gpointer data)
 * {
 *     gchar buf[20];
 *     gint i;
 *     <!-- Hello, gtk-doc! -->
 *     gtk_tree_model_get(model, iter, 0, &amp;i, -1);
 *     g_snprintf(buf, sizeof(buf), "&percnt;d", (i + 1)*GPOINTER_TO_INT(data));
 *     g_object_set(renderer, "text", buf, NULL);
 * }
 * </programlisting></informalexample>
 *
 * To extend the multiplication table to 20 rows, one only needs
 * <informalexample><programlisting>
 * gwy_null_store_set_n_rows(store, 20);
 * </programlisting></informalexample>
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
