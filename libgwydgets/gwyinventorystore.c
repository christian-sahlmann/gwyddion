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
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
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
static gboolean  gwy_inventory_store_get_iter       (GtkTreeModel *model,
                                                     GtkTreeIter *iter,
                                                     GtkTreePath *path);
static GtkTreePath* gwy_inventory_store_get_path       (GtkTreeModel *model,
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
static void      gwy_inventory_store_check_item     (gpointer key,
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
    iface->get_iter = gwy_inventory_store_get_iter;
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
    store->stamp = g_random_int();
}

static void
gwy_inventory_store_finalize(GObject *object)
{
    GwyInventoryStore *store;

    store = GWY_INVENTORY_STORE(object);

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
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), 0);
    return GWY_INVENTORY_STORE(model)->n_columns;
}

static GType
gwy_inventory_store_get_column_type(GtkTreeModel *model,
                                    gint column)
{
    GwyInventoryStore *store;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), 0);
    store = GWY_INVENTORY_STORE(model);
    g_return_val_if_fail(column < 0 || column >= store->n_columns, 0);

    /* Zeroth is item itself */
    if (!column)
        return G_TYPE_POINTER;

    return store->item_type->get_traits(NULL)[column-1];
}

static gboolean
gwy_inventory_store_get_iter(GtkTreeModel *model,
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

    iter->stamp = store->stamp;
    iter->user_data = gwy_inventory_get_nth_item(store->inventory, i);

    return TRUE;
}

static GtkTreePath*
gwy_inventory_store_get_path(GtkTreeModel *model,
                             GtkTreeIter *iter)
{
    GwyInventoryStore *store;
    GtkTreePath *path;
    const gchar *name;

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), NULL);
    store = GWY_INVENTORY_STORE(model);
    g_return_val_if_fail(iter->stamp == store->stamp, NULL);

    path = gtk_tree_path_new();
    name = store->item_type->get_name(iter->user_data);
    gtk_tree_path_append_index(path,
                               gwy_inventory_get_item_position(store->inventory,
                                                               name));

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
    g_return_if_fail(iter->stamp == store->stamp);
    g_return_if_fail(column < 0 || column >= store->n_columns);

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

    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(model), FALSE);
    store = GWY_INVENTORY_STORE(model);
    g_return_val_if_fail(iter->stamp == store->stamp, FALSE);

    iter->user_data = gwy_inventory_get_next_item(store->inventory,
                                                  iter->user_data);
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

    g_return_val_if_fail(iter->stamp == store->stamp, -1);
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

    iter.user_data = gwy_inventory_get_nth_item(inventory, i);
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
    GwyInventoryStore *store;
    const GwyInventoryItemType *item_type;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    item_type = gwy_inventory_get_item_type(inventory);
    g_return_val_if_fail(item_type->get_name, NULL);
    g_return_val_if_fail(item_type->get_traits, NULL);
    g_return_val_if_fail(item_type->get_trait_value, NULL);

    g_object_ref(inventory);
    store = g_object_new(GWY_TYPE_INVENTORY_STORE, NULL);
    store->item_type = item_type;
    item_type->get_traits(&store->n_columns);

    g_signal_connect(inventory, "item-updated",
                     G_CALLBACK(gwy_inventory_store_row_changed), store);
    g_signal_connect(inventory, "item-inserted",
                     G_CALLBACK(gwy_inventory_store_row_inserted), store);
    g_signal_connect(inventory, "item-deleted",
                     G_CALLBACK(gwy_inventory_store_row_deleted), store);
    g_signal_connect(inventory, "items-reordered",
                     G_CALLBACK(gwy_inventory_store_rows_reordered), store);

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
    g_return_val_if_fail(GWY_IS_INVENTORY_STORE(store), FALSE);

    if (!iter || !iter->user_data || iter->stamp != store->stamp)
        return FALSE;

    iter->user_data2 = NULL;
    gwy_inventory_foreach(store->inventory, gwy_inventory_store_check_item,
                          &iter);

    return iter->user_data == iter->user_data2;
}

static void
gwy_inventory_store_check_item(G_GNUC_UNUSED gpointer key,
                               gpointer item,
                               gpointer user_data)
{
    GtkTreeIter *iter = (GtkTreeIter*)user_data;

    if (iter->user_data == item) {
        if (iter->user_data2)
            g_warning("Item found multiple times");
        iter->user_data2 = item;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
