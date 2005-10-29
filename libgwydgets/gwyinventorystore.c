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
#include <libgwydgets/gwyinventorystore.h>

#define GWY_IMPLEMENT_TREE_MODEL(iface_init) \
    { \
        static const GInterfaceInfo gwy_tree_model_iface_info = { \
            (GInterfaceInitFunc)iface_init, NULL, NULL \
        }; \
        g_type_add_interface_static(g_define_type_id, \
                                    GTK_TYPE_TREE_MODEL, \
                                    &gwy_tree_model_iface_info); \
    }

static void      gwy_inventory_store_finalize       (GObject *object);
static void      gwy_inventory_store_tree_model_init(GtkTreeModelIface *iface);
static GtkTreeModelFlags gwy_inventory_store_get_flags (GtkTreeModel *model);
static gint      gwy_inventory_store_get_n_columns  (GtkTreeModel *model);
static GType     gwy_inventory_store_get_column_type(GtkTreeModel *model,
                                                     gint column);
static inline void gwy_inventory_store_update_iter  (GwyInventoryStore *store,
                                                     GtkTreeIter *iter);
static gboolean  gwy_inventory_store_get_tree_iter  (GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     GtkTreePath *path);
static GtkTreePath* gwy_inventory_store_get_path    (GtkTreeModel *model,
                                                     GtkTreeIter *iter);
static void      gwy_inventory_store_get_value      (GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     gint column,
                                                     GValue *value);
static gboolean  gwy_inventory_store_iter_next      (GtkTreeModel *model,
                                                     GtkTreeIter *iter);
static gboolean  gwy_inventory_store_iter_children  (GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     GtkTreeIter *parent);
static gboolean  gwy_inventory_store_iter_has_child (GtkTreeModel *model,
                                                     GtkTreeIter *iter);
static gint      gwy_inventory_store_iter_n_children(GtkTreeModel *model,
                                                     GtkTreeIter *iter);
static gboolean  gwy_inventory_store_iter_nth_child (GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     GtkTreeIter *parent,
                                                     gint n);
static gboolean  gwy_inventory_store_iter_parent    (GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     GtkTreeIter *child);
static void      gwy_inventory_store_row_changed    (GwyInventory *inventory,
                                                     guint i,
                                                     GwyInventoryStore *store);
static void      gwy_inventory_store_row_inserted   (GwyInventory *inventory,
                                                     guint i,
                                                     GwyInventoryStore *store);
static void      gwy_inventory_store_row_deleted    (GwyInventory *inventory,
                                                     guint i,
                                                     GwyInventoryStore *store);
static void      gwy_inventory_store_rows_reordered (GwyInventory *inventory,
                                                     gint *new_order,
                                                     GwyInventoryStore *store);
static gboolean  gwy_inventory_store_check_item     (gpointer key,
                                                     gpointer item,
                                                     gpointer user_data);

G_DEFINE_TYPE_EXTENDED
    (GwyInventoryStore, gwy_inventory_store, G_TYPE_OBJECT, 0,
     GWY_IMPLEMENT_TREE_MODEL(gwy_inventory_store_tree_model_init))

static void
gwy_inventory_store_class_init(GwyInventoryStoreClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_inventory_store_finalize;
}

static void
gwy_inventory_store_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags = gwy_inventory_store_get_flags;
    iface->get_n_columns = gwy_inventory_store_get_n_columns;
    iface->get_column_type = gwy_inventory_store_get_column_type;
    iface->get_iter = gwy_inventory_store_get_tree_iter;
    iface->get_path = gwy_inventory_store_get_path;
    iface->get_value = gwy_inventory_store_get_value;
    iface->iter_next = gwy_inventory_store_iter_next;
    iface->iter_children = gwy_inventory_store_iter_children;
    iface->iter_has_child = gwy_inventory_store_iter_has_child;
    iface->iter_n_children = gwy_inventory_store_iter_n_children;
    iface->iter_nth_child = gwy_inventory_store_iter_nth_child;
    iface->iter_parent = gwy_inventory_store_iter_parent;
}

static void
gwy_inventory_store_init(GwyInventoryStore *store)
{
    gwy_debug_objects_creation(G_OBJECT(store));
    store->stamp = g_random_int();
}

static void
gwy_inventory_store_finalize(GObject *object)
{
    GwyInventoryStore *store;

    store = GWY_INVENTORY_STORE(object);

    if (store->inventory) {
        g_signal_handler_disconnect(store->inventory, store->item_updated_id);
        g_signal_handler_disconnect(store->inventory, store->item_inserted_id);
        g_signal_handler_disconnect(store->inventory, store->item_deleted_id);
        g_signal_handler_disconnect(store->inventory,
                                    store->items_reordered_id);
    }
    gwy_object_unref(store->inventory);

    G_OBJECT_CLASS(gwy_inventory_store_parent_class)->finalize(object);
}

static GtkTreeModelFlags
gwy_inventory_store_get_flags(GtkTreeModel *model)
{
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), 0);
    return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gwy_inventory_store_get_n_columns(GtkTreeModel *model)
{
    GwyInventoryStore *store;
    gint n;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), 0);
    store = GWY_INVENTORY_STORE(model);
    store->item_type->get_traits(&n);
    /* +1 for "item" column */
    return n+1;
}

static GType
gwy_inventory_store_get_column_type(GtkTreeModel *model,
                                    gint column)
{
    GwyInventoryStore *store;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), 0);
    store = GWY_INVENTORY_STORE(model);

    /* Zeroth is item itself */
    if (!column)
        return G_TYPE_POINTER;

    return store->item_type->get_traits(NULL)[column-1];
}

static inline void
gwy_inventory_store_update_iter(GwyInventoryStore *store,
                                GtkTreeIter *iter)
{
    if (iter->stamp != store->stamp) {
        const gchar *name;
        gint i;

        name = store->item_type->get_name(iter->user_data);
        i = gwy_inventory_get_item_position(store->inventory, name);

        iter->stamp = store->stamp;
        iter->user_data2 = GUINT_TO_POINTER(i);
    }
}

static gboolean
gwy_inventory_store_get_tree_iter(GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  GtkTreePath *path)
{
    GwyInventoryStore *store;
    gint i;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), FALSE);
    g_return_val_if_fail(gtk_tree_path_get_depth(path) > 0, FALSE);
    store = GWY_INVENTORY_STORE(model);

    i = gtk_tree_path_get_indices(path)[0];

    if (i >= gwy_inventory_get_n_items(store->inventory))
        return FALSE;

    /* GwyInventoryStore has presistent iters, because it uses item pointers
     * themselves as @user_data and an item pointer is valid as long as the
     * item exists.
     *
     * This always works but needs a round trip item -> name -> position ->
     * position+1 -> item to get next iter.  So we also store item position
     * in @user_data2 and use that directly if inventory has not changed.
     * If it has changed, we can update it using the slower method.
     *
     * To sum it up:
     * @stamp: Corresponds to store's @stamp, but does not have to match.
     * @user_data: Pointer to item.
     * @user_data2: Position in inventory.
     */
    iter->stamp = store->stamp;
    iter->user_data = gwy_inventory_get_nth_item(store->inventory, i);
    iter->user_data2 = GUINT_TO_POINTER(i);

    return TRUE;
}

static GtkTreePath*
gwy_inventory_store_get_path(GtkTreeModel *model,
                             GtkTreeIter *iter)
{
    GwyInventoryStore *store;
    GtkTreePath *path;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), NULL);

    store = GWY_INVENTORY_STORE(model);
    gwy_inventory_store_update_iter(store, iter);
    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, GPOINTER_TO_UINT(iter->user_data2));

    return path;
}

static void
gwy_inventory_store_get_value(GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gint column,
                              GValue *value)
{
    GwyInventoryStore *store;

    g_return_if_fail(GWY_IS_INVENTORY_STORE(model));
    store = GWY_INVENTORY_STORE(model);

    if (!column) {
        g_value_init(value, G_TYPE_POINTER);
        g_value_set_pointer(value, iter->user_data);
        return;
    }

    store->item_type->get_trait_value(iter->user_data, column-1, value);
}

static gboolean
gwy_inventory_store_iter_next(GtkTreeModel *model,
                              GtkTreeIter *iter)
{
    GwyInventoryStore *store;
    gint i;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), FALSE);
    store = GWY_INVENTORY_STORE(model);
    gwy_inventory_store_update_iter(store, iter);

    i = GPOINTER_TO_UINT(iter->user_data2) + 1;
    iter->user_data = gwy_inventory_get_nth_item(store->inventory, i);
    iter->user_data2 = GUINT_TO_POINTER(i);
    return iter->user_data != NULL;
}

static gboolean
gwy_inventory_store_iter_children(GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  GtkTreeIter *parent)
{
    GwyInventoryStore *store;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), FALSE);
    store = GWY_INVENTORY_STORE(model);

    if (parent)
        return FALSE;

    if (gwy_inventory_get_n_items(store->inventory)) {
        iter->stamp = store->stamp;
        iter->user_data = gwy_inventory_get_nth_item(store->inventory, 0);
        iter->user_data2 = GUINT_TO_POINTER(0);
        return TRUE;
    }
    return FALSE;
}

static gboolean
gwy_inventory_store_iter_has_child(G_GNUC_UNUSED GtkTreeModel *model,
                                   G_GNUC_UNUSED GtkTreeIter *iter)
{
    return FALSE;
}

static gint
gwy_inventory_store_iter_n_children(GtkTreeModel *model,
                                    GtkTreeIter *iter)
{
    GwyInventoryStore *store;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), -1);
    store = GWY_INVENTORY_STORE(model);

    if (iter)
        return 0;

    return gwy_inventory_get_n_items(store->inventory);
}

static gboolean
gwy_inventory_store_iter_nth_child(GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   GtkTreeIter *parent,
                                   gint n)
{
    GwyInventoryStore *store;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), FALSE);
    store = GWY_INVENTORY_STORE(model);

    if (parent)
        return FALSE;

    if (n < gwy_inventory_get_n_items(store->inventory)) {
        iter->stamp = store->stamp;
        iter->user_data = gwy_inventory_get_nth_item(store->inventory, n);
        iter->user_data2 = GUINT_TO_POINTER(n);
        return TRUE;
    }
    return FALSE;
}

static gboolean
gwy_inventory_store_iter_parent(G_GNUC_UNUSED GtkTreeModel *model,
                                G_GNUC_UNUSED GtkTreeIter *iter,
                                G_GNUC_UNUSED GtkTreeIter *child)
{
    return FALSE;
}

static void
gwy_inventory_store_row_changed(GwyInventory *inventory,
                                guint i,
                                GwyInventoryStore *store)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    iter.user_data = gwy_inventory_get_nth_item(inventory, i);
    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
    gtk_tree_path_free(path);
}

static void
gwy_inventory_store_row_inserted(GwyInventory *inventory,
                                 guint i,
                                 GwyInventoryStore *store)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    store->stamp++;
    iter.user_data = gwy_inventory_get_nth_item(inventory, i);
    iter.user_data2 = GUINT_TO_POINTER(i);
    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, &iter);
    gtk_tree_path_free(path);
}

static void
gwy_inventory_store_row_deleted(G_GNUC_UNUSED GwyInventory *inventory,
                                guint i,
                                GwyInventoryStore *store)
{
    GtkTreePath *path;

    store->stamp++;
    path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, i);
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
    gtk_tree_path_free(path);
}

static void
gwy_inventory_store_rows_reordered(G_GNUC_UNUSED GwyInventory *inventory,
                                   gint *new_order,
                                   GwyInventoryStore *store)
{
    GtkTreePath *path;

    store->stamp++;
    path = gtk_tree_path_new();
    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(store), path, NULL, new_order);
    gtk_tree_path_free(path);
}

/**
 * gwy_inventory_store_new:
 * @inventory: An inventory.
 *
 * Creates a new #GtkTreeModel wrapper around a #GwyInventory.
 *
 * Returns: The newly created inventory store.
 **/
GwyInventoryStore*
gwy_inventory_store_new(GwyInventory *inventory)
{
    const GwyInventoryItemType *item_type;
    GwyInventoryStore *store;
    gulong i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    item_type = gwy_inventory_get_item_type(inventory);
    g_return_val_if_fail(item_type->get_name, NULL);
    g_return_val_if_fail(item_type->get_traits, NULL);
    g_return_val_if_fail(item_type->get_trait_value, NULL);

    g_object_ref(inventory);
    store = g_object_new(GWY_TYPE_INVENTORY_STORE, NULL);
    store->inventory = inventory;
    store->item_type = item_type;

    i = g_signal_connect(inventory, "item-updated",
                         G_CALLBACK(gwy_inventory_store_row_changed), store);
    store->item_updated_id = i;
    i = g_signal_connect(inventory, "item-inserted",
                         G_CALLBACK(gwy_inventory_store_row_inserted), store);
    store->item_inserted_id = i;
    i = g_signal_connect(inventory, "item-deleted",
                         G_CALLBACK(gwy_inventory_store_row_deleted), store);
    store->item_deleted_id = i;
    i = g_signal_connect(inventory, "items-reordered",
                         G_CALLBACK(gwy_inventory_store_rows_reordered), store);
    store->items_reordered_id = i;

    return store;
}

/**
 * gwy_inventory_store_get_inventory:
 * @store: An inventory store.
 *
 * Gets the inventory a inventory store wraps.
 *
 * Returns: The underlying inventory (its reference count is not increased).
 **/
GwyInventory*
gwy_inventory_store_get_inventory(GwyInventoryStore *store)
{
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), NULL);
    return store->inventory;
}

/**
 * gwy_inventory_store_get_column_by_name:
 * @store: An inventory store.
 * @name: Trait (column) name.
 *
 * Gets tree model column corresponding to a trait name.
 *
 * The underlying inventory must support trait names, except for @name
 * <literal>"item"</literal> which always works (and always maps to 0).
 *
 * Returns: The underlying inventory (its reference count is not increased).
 **/
gint
gwy_inventory_store_get_column_by_name(GwyInventoryStore *store,
                                       const gchar *name)
{
    const gchar* (*method)(gint);
    gint i, n;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), -1);
    if (gwy_strequal(name, "item"))
        return 0;

    g_return_val_if_fail(store->item_type->get_trait_name, -1);
    store->item_type->get_traits(&n);
    method = store->item_type->get_trait_name;

    for (i = 0; i < n; i++) {
        if (gwy_strequal(name, method(i)))
            return i+1;
    }
    return -1;
}

/**
 * gwy_inventory_store_get_iter:
 * @store: An inventory store.
 * @name: Item name.
 * @iter: Tree iterator to set to point to item named @name.
 *
 * Initializes a tree iterator to row corresponding to a inventory item.
 *
 * Returns: %TRUE if @iter is valid, that is the item exists, %FALSE if @iter
 *          was not set.
 **/
gboolean
gwy_inventory_store_get_iter(GwyInventoryStore *store,
                             const gchar *name,
                             GtkTreeIter *iter)
{
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), FALSE);
    g_return_val_if_fail(iter, FALSE);

    i = gwy_inventory_get_item_position(store->inventory, name);
    if (i == (guint)-1)
        return FALSE;

    iter->stamp = store->stamp;
    iter->user_data = gwy_inventory_get_nth_item(store->inventory, i);
    g_assert(iter->user_data);
    iter->user_data2 = GUINT_TO_POINTER(i);

    return TRUE;
}

/**
 * gwy_inventory_store_iter_is_valid:
 * @store: An inventory store.
 * @iter: A #GtkTreeIter.
 *
 * Checks if the given iter is a valid iter for this inventory store.
 *
 * <warning>This function is slow. Only use it for debugging and/or testing
 * purposes.</warning>
 *
 * Returns: %TRUE if the iter is valid, %FALSE if the iter is invalid.
 **/
gboolean
gwy_inventory_store_iter_is_valid(GwyInventoryStore *store,
                                  GtkTreeIter *iter)
{
    GtkTreeIter copy;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), FALSE);

    if (!iter || !iter->user_data)
        return FALSE;

    /* Make a copy because we use the iter as a scratch pad */
    copy = *iter;
    if (!gwy_inventory_find(store->inventory, gwy_inventory_store_check_item,
                            &copy))
        return FALSE;

    /* Iters with different stamps are valid if just @user_data matches item
     * pointer.  But if stamps match, @user_data2 must match item position */
    return iter->stamp != store->stamp || iter->user_data2 == copy.user_data2;
}

static gboolean
gwy_inventory_store_check_item(gpointer key,
                               gpointer item,
                               gpointer user_data)
{
    GtkTreeIter *iter = (GtkTreeIter*)user_data;

    if (iter->user_data != item)
        return FALSE;

    iter->user_data2 = key;
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyinventorystore
 * @title: GwyInventoryStore
 * @short_description: #GtkTreeModel wrapper around #GwyInventory
 * @see_also: #GwyInventory -- the actual data container,
 *            #GtkListStore, #GtkTreeStore -- Gtk+ tree model implementations
 *
 * #GwyInventoryStore is a simple adaptor class that wraps #GwyInventory in
 * #GtkTreeModel interface.  It is list-only and has persistent iterators.  It
 * offers no methods to manipulate items, this should be done on the underlying
 * inventory.
 *
 * #GwyInventoryStore maps inventory item traits to virtual #GtkTreeModel
 * columns.  Zeroth column is always of type %G_TYPE_POINTER and contains item
 * itself.  It exists even if item don't export any traits.  Columns from 1
 * onward are formed by item traits.  You can obtain column id of a named
 * item trait with gwy_inventory_store_get_column_by_name().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
