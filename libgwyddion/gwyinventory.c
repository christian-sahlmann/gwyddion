/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <stdlib.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include "gwyinventory.h"
#include "gwyserializable.h"
#include "gwywatchable.h"

#define GWY_INVENTORY_TYPE_NAME "GwyInventory"

enum {
    ITEM_INSERTED,
    ITEM_DELETED,
    ITEM_UPDATED,
    ITEMS_REORDERED,
    LAST_SIGNAL
};

typedef struct {
    gpointer p;
    guint i;
} ArrayItem;

static void     gwy_inventory_class_init         (GwyInventoryClass *klass);
static void     gwy_inventory_init               (GwyInventory *container);
static void     gwy_inventory_finalize           (GObject *object);
static void     gwy_inventory_reindex            (GwyInventory *inventory);
static gint     gwy_inventory_compare_indices    (gint *a,
                                                  gint *b,
                                                  GwyInventory *inventory);

static guint gwy_inventory_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
gwy_inventory_get_type(void)
{
    static GType gwy_inventory_type = 0;

    if (!gwy_inventory_type) {
        static const GTypeInfo gwy_inventory_info = {
            sizeof(GwyInventoryClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_inventory_class_init,
            NULL,
            NULL,
            sizeof(GwyInventory),
            0,
            (GInstanceInitFunc)gwy_inventory_init,
            NULL,
        };

        gwy_debug("");
        gwy_inventory_type = g_type_register_static(G_TYPE_OBJECT,
                                                    GWY_INVENTORY_TYPE_NAME,
                                                    &gwy_inventory_info,
                                                    0);
    }

    return gwy_inventory_type;
}

static void
gwy_inventory_class_init(GwyInventoryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_inventory_finalize;

/**
 * GwyInventory::item_inserted:
 * @gwyinventory: The #GwyInventory which received the signal.
 * @arg1: Position an item was inserted at.
 * @user_data: User data set when the signal handler was connected.
 *
 * The ::item_inserted signal is emitted when an item is inserted into
 * an inventory.
 **/
    gwy_inventory_signals[ITEM_INSERTED] =
        g_signal_new("item_inserted",
                     GWY_TYPE_INVENTORY,
                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                     G_STRUCT_OFFSET(GwyInventoryClass, item_inserted),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT,
                     G_TYPE_NONE, 1,
                     G_TYPE_UINT);

/**
 * GwyInventory::item_deleted:
 * @gwyinventory: The #GwyInventory which received the signal.
 * @arg1: Position an item was deleted from.
 * @user_data: User data set when the signal handler was connected.
 *
 * The ::item_deleted signal is emitted when an item is deleted from
 * an inventory.
 **/
    gwy_inventory_signals[ITEM_INSERTED] =
        g_signal_new("item_deleted",
                     GWY_TYPE_INVENTORY,
                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                     G_STRUCT_OFFSET(GwyInventoryClass, item_deleted),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT,
                     G_TYPE_NONE, 1,
                     G_TYPE_UINT);

/**
 * GwyInventory::item_updated:
 * @gwyinventory: The #GwyInventory which received the signal.
 * @arg1: Position of updated item.
 * @user_data: User data set when the signal handler was connected.
 *
 * The ::item_updated signal is emitted when an item in an inventory
 * is updated.
 **/
    gwy_inventory_signals[ITEM_UPDATED] =
        g_signal_new("item_updated",
                     GWY_TYPE_INVENTORY,
                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                     G_STRUCT_OFFSET(GwyInventoryClass, item_updated),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__UINT,
                     G_TYPE_NONE, 1,
                     G_TYPE_UINT);

/**
 * GwyInventory::items_reordered:
 * @gwyinventory: The #GwyInventory which received the signal.
 * @arg1: Position of updated item.
 * @user_data: User data set when the signal handler was connected.
 *
 * The ::items_reordered signal is emitted when item in an inventory
 * are reordered.
 **/
    gwy_inventory_signals[ITEMS_REORDERED] =
        g_signal_new("items_reordered",
                     GWY_TYPE_INVENTORY,
                     G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                     G_STRUCT_OFFSET(GwyInventoryClass, items_reordered),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1,
                     G_TYPE_POINTER);
}

static void
gwy_inventory_init(GwyInventory *inventory)
{
    gwy_debug("");
    gwy_debug_objects_creation((GObject*)inventory);
}

static void
gwy_inventory_finalize(GObject *object)
{
    GwyInventory *inventory = (GwyInventory*)object;
    guint i;

    gwy_debug("");

    if (inventory->default_key)
        g_string_free(inventory->default_key, TRUE);
    if (inventory->hash)
        g_hash_table_destroy(inventory->hash);
    if (inventory->idx)
        g_array_free(inventory->idx, TRUE);
    if (inventory->items) {
        for (i = 0; i < inventory->items->len; i++)
            g_object_unref(g_array_index(inventory->items, ArrayItem, i).p);
        g_array_free(inventory->items, TRUE);
    }

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_inventory_new:
 * @item_type: Type of items the inventory will contain.
 *
 * Creates a new inventory.
 *
 * Returns: The newly created inventory.
 **/
GwyInventory*
gwy_inventory_new(const GwyItemType *item_type)
{
    return gwy_inventory_new_filled(item_type, 0, NULL);
}

/**
 * gwy_inventory_new_filled:
 * @item_type: Type of items the inventory will contain.
 * @nitems: The number of pointers in @items.
 * @items: Item pointers to fill the newly created inventory with.
 *
 * Creates a new inventory and fills it with items.
 *
 * Returns: The newly created inventory.
 **/
GwyInventory*
gwy_inventory_new_filled(const GwyItemType *item_type,
                         guint nitems,
                         gpointer *items)
{
    GwyInventory *inventory;
    guint i;

    gwy_debug("");
    g_return_val_if_fail(item_type, NULL);
    g_return_val_if_fail(items || !nitems, NULL);

    inventory = g_object_new(GWY_TYPE_INVENTORY, NULL);

    inventory->item_type = *item_type;
    if (item_type->type) {
        inventory->is_simple = !G_TYPE_IS_CLASSED(item_type->type);
        inventory->can_make_copies = GWY_IS_SERIALIZABLE(item_type->type);
    }
    else {
        inventory->is_simple = TRUE;
        inventory->can_make_copies = FALSE;
    }

    inventory->is_sorted = (item_type->compare != NULL);
    inventory->items = g_array_sized_new(FALSE, FALSE, sizeof(ArrayItem),
                                         nitems);
    inventory->idx = g_array_sized_new(FALSE, FALSE, sizeof(guint), nitems);
    inventory->hash = g_hash_table_new(g_str_hash, g_str_equal);

    for (i = 0; i < nitems; i++) {
        ArrayItem aitem;

        aitem.p = items[i];
        aitem.i = i;
        g_array_append_val(inventory->items, aitem);
        g_array_append_val(inventory->idx, i);
        if (inventory->is_sorted && i)
            inventory->is_sorted = (item_type->compare(items[i-1], items[i])
                                    < 0);

        g_hash_table_insert(inventory->hash,
                            (gpointer)item_type->get_name(items[i]),
                            GUINT_TO_POINTER(i+1));
    }
    if (!inventory->is_simple) {
        for (i = 0; i < nitems; i++)
            g_object_ref(items[i]);
    }

    return inventory;
}


/**
 * gwy_inventory_get_n_items:
 * @inventory: An inventory.
 *
 * Returns the number of items in an inventory.
 *
 * Returns: The number of items.
 **/
guint
gwy_inventory_get_n_items(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), 0);
    return inventory->items->len;
}

/**
 * gwy_inventory_can_make_copies:
 * @inventory: An inventory.
 *
 * Returns whether an inventory can create new items itself.
 *
 * The prerequistie is that item type is a serializable object.  It enables
 * functions like gwy_inventory_new_item_as_copy().
 *
 * Returns: %TRUE if inventory can create new items itself.
 **/
gboolean
gwy_inventory_can_make_copies(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), FALSE);
    return inventory->can_make_copies;
}

/**
 * gwy_inventory_get_item_type:
 * @inventory: An inventory.
 *
 * Returns the type of item an inventory holds.
 *
 * Returns: The item type.  It is owned by inventory and must not be modified
 *          or freed.
 **/
const GwyItemType*
gwy_inventory_get_item_type(GwyInventory *inventory)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    return &inventory->item_type;
}

/**
 * gwy_inventory_item_exists:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Checks if an item exists in an inventory.
 *
 * Returns: %TRUE if item exists in inventory.
 **/
gboolean
gwy_inventory_item_exists(GwyInventory *inventory,
                          const gchar *name)
{
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), FALSE);
    return g_hash_table_lookup(inventory->hash, name) != NULL;
}

/**
 * gwy_inventory_get_item:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Looks up an item in an inventory.
 *
 * Returns: Item called @name, or %NULL if there is no such item.
 **/
gpointer
gwy_inventory_get_item(GwyInventory *inventory,
                       const gchar *name)
{
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    if ((i = GPOINTER_TO_UINT(g_hash_table_lookup(inventory->hash, name))))
        return g_array_index(inventory->items, ArrayItem, i-1).p;
    else
        return NULL;
}

/**
 * gwy_inventory_get_item_or_default:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Looks up an item in an inventory, eventually falling back to default.
 *
 * The lookup order is: item of requested name, default item (if set), first
 * inventory item, %NULL (can happen only when inventory is empty).
 *
 * Returns: Item called @name, or default item.
 **/
gpointer
gwy_inventory_get_item_or_default(GwyInventory *inventory,
                                  const gchar *name)
{
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    if ((i = GPOINTER_TO_UINT(g_hash_table_lookup(inventory->hash, name))))
        return g_array_index(inventory->items, ArrayItem, i-1).p;
    if (inventory->has_default
        && (i = GPOINTER_TO_UINT(g_hash_table_lookup(inventory->hash,
                                                     inventory->default_key))))
        return g_array_index(inventory->items, ArrayItem, i-1).p;

    if (inventory->items->len)
        return g_array_index(inventory->items, ArrayItem, 0).p;
    return NULL;
}

/**
 * gwy_inventory_get_nth_item:
 * @inventory: An inventory.
 * @n: Item position.
 *
 * Returns item on given position in an inventory.
 *
 * Returns: Item at given position.
 **/
gpointer
gwy_inventory_get_nth_item(GwyInventory *inventory,
                           guint n)
{
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    g_return_val_if_fail(n < inventory->items->len, NULL);
    i = g_array_index(inventory->idx, guint, n);

    return g_array_index(inventory->items, ArrayItem, i).p;
}

/**
 * gwy_inventory_get_item_position:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Finds position of an item in an inventory.
 *
 * Returns: Item position, or -1 if there is no such item.
 **/
guint
gwy_inventory_get_item_position(GwyInventory *inventory,
                                const gchar *name)
{
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), -1);
    if (!(i = GPOINTER_TO_UINT(g_hash_table_lookup(inventory->hash, name))))
        return (guint)-1;

    if (inventory->needs_reindex)
        gwy_inventory_reindex(inventory);

    return g_array_index(inventory->items, ArrayItem, i-1).i;
}

/**
 * gwy_inventory_reindex:
 * @inventory: An inventory.
 *
 * Updates @i members of @inventory->items to reflect item positions.
 *
 * Note positions in hash are 1-based (to allow %NULL work as no-such-item),
 * but position in @items and @idx are 0-based.
 **/
static void
gwy_inventory_reindex(GwyInventory *inventory)
{
    guint i, n;

    for (i = 0; i < inventory->idx->len; i++) {
        n = g_array_index(inventory->idx, guint, i);
        g_array_index(inventory->items, ArrayItem, n).i = n;
    }

    inventory->needs_reindex = FALSE;
}

/**
 * gwy_inventory_foreach:
 * @inventory: An inventory.
 * @function: A function to call on each item.
 * @user_data: Data passed to @function.
 *
 * Calls a function on each item of an inventory, in order.
 *
 * @function's first argument is item position (transformed with
 * GUINT_TO_POINTER()), second is item pointer, and the last is @user_data.
 **/
void
gwy_inventory_foreach(GwyInventory *inventory,
                      GHFunc function,
                      gpointer user_data)
{
    guint i;

    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    g_return_if_fail(function);
    for (i = 0; i < inventory->idx->len; i++)
        function(GUINT_TO_POINTER(i),
                 g_array_index(inventory->items, ArrayItem, i).p,
                 user_data);
}

/**
 * gwy_inventory_get_default_item:
 * @inventory: An inventory.
 *
 * Returns the default item of an inventory.
 *
 * Returns: The default item.  If there is no default item, %NULL is returned.
 **/
gpointer
gwy_inventory_get_default_item(GwyInventory *inventory)
{
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    if (!inventory->has_default)
        return NULL;

    if (!(i = GPOINTER_TO_UINT(g_hash_table_lookup(inventory->hash,
                                                   inventory->default_key))))
        return NULL;

    return g_array_index(inventory->items, ArrayItem, i-1).p;
}

/**
 * gwy_inventory_set_default_item:
 * @inventory: An inventory.
 * @name: Item name, pass %NULL to unset default item.
 *
 * Sets the default of an inventory.
 *
 * Item @name must already exist in the inventory.
 **/
void
gwy_inventory_set_default_item(GwyInventory *inventory,
                               const gchar *name)
{
    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    if (!name) {
        inventory->has_default = FALSE;
        return;
    }

    if (!g_hash_table_lookup(inventory->hash, name)) {
        g_warning("Default item to be set not present in inventory");
        return;
    }

    if (!inventory->default_key)
        inventory->default_key = g_string_new(name);
    else
        g_string_assign(inventory->default_key, name);
}

/**
 * gwy_inventory_insert_item:
 * @inventory: An inventory.
 * @item: An item to insert.
 *
 * Inserts an item into an inventory.
 *
 * Item of the same name must not exist yet.
 *
 * If the inventory is sorted, item is inserted to keep order.  If the
 * inventory is unsorted, item is simply added to the end.
 *
 * Returns: @item, for convenience.
 **/
gpointer
gwy_inventory_insert_item(GwyInventory *inventory,
                          gpointer item)
{
    ArrayItem aitem;
    const gchar *name;
    guint m;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    g_return_val_if_fail(item, NULL);

    name = inventory->item_type.get_name(item);
    if (g_hash_table_lookup(inventory->hash, name)) {
        g_warning("Item already exists");
        return NULL;
    }

    if (!inventory->is_simple)
        g_object_ref((gpointer)item);

    /* Insert into index array */
    if (inventory->is_sorted) {
        gpointer mp;
        guint j0, j1;

        j0 = 0;
        j1 = inventory->idx->len-1;
        while (j1 - j0 > 1) {
            m = (j0 + j1 + 1)/2;
            mp = g_array_index(inventory->items, ArrayItem,
                               g_array_index(inventory->idx, guint, m)).p;
            if (inventory->item_type.compare(item, mp) >= 0)
                j0 = m;
            else
                j1 = m;
        }

        mp = g_array_index(inventory->items, ArrayItem,
                           g_array_index(inventory->idx, guint, j0)).p;
        if (inventory->item_type.compare(item, mp) < 0)
            m = j0;
        else {
            mp = g_array_index(inventory->items, ArrayItem,
                               g_array_index(inventory->idx, guint, j0)).p;
            if (inventory->item_type.compare(item, mp) < 0)
                m = j1;
            else
                m = j1+1;
        }

        g_array_insert_val(inventory->idx, m, inventory->items->len);
        inventory->needs_reindex = TRUE;
    }
    else {
        m = inventory->items->len;
        g_array_append_val(inventory->idx, inventory->items->len);
    }

    aitem.p = item;
    g_array_append_val(inventory->items, aitem);
    g_hash_table_insert(inventory->hash, (gpointer)name,
                        GUINT_TO_POINTER(inventory->items->len));

    g_signal_emit(inventory, gwy_inventory_signals[ITEM_INSERTED], 0, m);

    return item;
}

/**
 * gwy_inventory_insert_nth_item:
 * @inventory: An inventory.
 * @item: An item to insert.
 * @n: Position to insert @item to.
 *
 * Inserts an item to an explicit position in an inventory.
 *
 * Item of the same name must not exist yet.
 *
 * Returns: @item, for convenience.
 **/
gpointer
gwy_inventory_insert_nth_item(GwyInventory *inventory,
                              gpointer item,
                              guint n)
{
    ArrayItem aitem;
    const gchar *name;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    g_return_val_if_fail(item, NULL);
    g_return_val_if_fail(n <= inventory->items->len, NULL);

    name = inventory->item_type.get_name(item);
    if (g_hash_table_lookup(inventory->hash, name)) {
        g_warning("Item already exists");
        return NULL;
    }

    if (!inventory->is_simple)
        g_object_ref((gpointer)item);

    g_array_insert_val(inventory->idx, n, inventory->items->len);
    inventory->needs_reindex = TRUE;

    aitem.p = item;
    g_array_append_val(inventory->items, aitem);
    g_hash_table_insert(inventory->hash, (gpointer)name,
                        GUINT_TO_POINTER(inventory->items->len));

    if (inventory->is_sorted) {
        gpointer mp;

        if (n > 0) {
            mp = g_array_index(inventory->items, ArrayItem,
                               g_array_index(inventory->idx, guint, n-1)).p;
            if (inventory->item_type.compare(item, mp) < 0)
                inventory->is_sorted = FALSE;
        }
        if (n+1 < inventory->items->len) {
            mp = g_array_index(inventory->items, ArrayItem,
                               g_array_index(inventory->idx, guint, n+1)).p;
            if (inventory->item_type.compare(item, mp) > 0)
                inventory->is_sorted = FALSE;
        }
    }

    g_signal_emit(inventory, gwy_inventory_signals[ITEM_INSERTED], 0, n);

    return item;
}

/**
 * gwy_inventory_restore_order:
 * @inventory: An inventory.
 *
 * Assures an inventory is sorted.
 **/
void
gwy_inventory_restore_order(GwyInventory *inventory)
{
    guint i;
    gint *new_order;

    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    if (inventory->is_sorted)
        return;

    if (!inventory->item_type.compare) {
        g_warning("Cannot sort inventory without any compare function");
        return;
    }

    /* Make sure old order is remembered in items.i fields */
    if (inventory->needs_reindex)
        gwy_inventory_reindex(inventory);
    g_array_sort_with_data(inventory->idx,
                           (GCompareDataFunc)gwy_inventory_compare_indices,
                           inventory);

    if (inventory->items->len > 1024)
        new_order = g_new(gint, inventory->items->len);
    else
        new_order = g_newa(gint, inventory->items->len);

    /* Fill new_order with indices: new_order[new_position] = old_position */
    for (i = 0; i < inventory->idx->len; i++)
        new_order[i] = g_array_index(inventory->items, ArrayItem,
                                     g_array_index(inventory->idx, guint, i)).i;
    inventory->needs_reindex = TRUE;
    inventory->is_sorted = TRUE;

    g_signal_emit(inventory, gwy_inventory_signals[ITEMS_REORDERED], 0,
                  new_order);

    if (inventory->items->len > 1024)
        g_free(new_order);
}

static gint
gwy_inventory_compare_indices(gint *a,
                              gint *b,
                              GwyInventory *inventory)
{
    gpointer pa, pb;

    pa = g_array_index(inventory->items, ArrayItem, *a).p;
    pb = g_array_index(inventory->items, ArrayItem, *b).p;
    return inventory->item_type.compare(pa, pb);
}

/**
 * gwy_inventory_delete_item:
 * @inventory: An inventory.
 * @name: Name of item to delete.
 *
 * Deletes an item from an inventory.
 *
 * Returns: %TRUE if item was deleted.
 **/
gboolean
gwy_inventory_delete_item(GwyInventory *inventory,
                          const gchar *name)
{
    gpointer mp;
    guint i, n;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), FALSE);
    if (!(i = GPOINTER_TO_UINT(g_hash_table_lookup(inventory->hash, name)))) {
        g_warning("Cannot delete nonexistent item");
        return FALSE;
    }

    /* FIXME: this makes removal of large number of items slow */
    if (inventory->needs_reindex)
        gwy_inventory_reindex(inventory);

    mp = g_array_index(inventory->items, ArrayItem, i-1).p;
    n = g_array_index(inventory->items, ArrayItem, i-1).i;
    g_hash_table_remove(inventory->hash, name);
    g_array_remove_index(inventory->idx, n);
    g_array_remove_index(inventory->items, i-1);

    inventory->needs_reindex = TRUE;
    g_object_unref(mp);

    g_signal_emit(inventory, gwy_inventory_signals[ITEM_DELETED], 0, n);

    return TRUE;
}

/**
 * gwy_inventory_delete_nth_item:
 * @inventory: An inventory.
 * @n: Position of @item to delete.
 *
 * Deletes an item on given position from an inventory.
 *
 * Returns: %TRUE if item was deleted.
 **/
gboolean
gwy_inventory_delete_nth_item(GwyInventory *inventory,
                              guint n)
{
    gpointer mp;
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), FALSE);
    g_return_val_if_fail(n <= inventory->items->len, FALSE);

    i = g_array_index(inventory->idx, guint, n);
    mp = g_array_index(inventory->items, ArrayItem, i).p;
    g_hash_table_remove(inventory->hash, inventory->item_type.get_name(mp));
    g_array_remove_index(inventory->idx, n);
    g_array_remove_index(inventory->items, i);

    inventory->needs_reindex = TRUE;
    g_object_unref(mp);

    g_signal_emit(inventory, gwy_inventory_signals[ITEM_DELETED], 0, n);

    return TRUE;
}

/************************** Documentation ****************************/

/**
 * GwyInventory:
 *
 * The #GwyInventory struct contains private data only and should be accessed
 * using the functions below.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
