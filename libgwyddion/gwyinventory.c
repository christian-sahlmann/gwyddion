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

#define ITEM(inventory, i) g_ptr_array_index((inventory)->items, (i))
#define HASH(inventory, n) \
    g_hash_table_lookup((inventory)->hash, \
                        GUINT_TO_POINTER(g_str_hash(name)))

static void     gwy_inventory_class_init         (GwyInventoryClass *klass);
static void     gwy_inventory_init               (GwyInventory *container);
static void     gwy_inventory_finalize           (GObject *object);

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

    gwy_debug("");

    if (inventory->hash)
        g_hash_table_destroy(inventory->hash);
    if (inventory->items)
        g_ptr_array_free(inventory->items, TRUE);

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
    inventory->items = g_ptr_array_sized_new(nitems);
    if (inventory->is_simple)
        inventory->hash = g_hash_table_new(&g_direct_hash, &g_direct_equal);
    else
        inventory->hash = g_hash_table_new_full(&g_direct_hash, &g_direct_equal,
                                                NULL, &g_object_unref);

    for (i = 0; i < nitems; i++) {
        g_ptr_array_add(inventory->items, items[i]);
        if (inventory->is_sorted && i)
            inventory->is_sorted = (item_type->compare(items[i-1], items[i])
                                    < 0);
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
    return HASH(inventory, name) != NULL;
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
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    return HASH(inventory, name);
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
    gpointer item;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    if ((item = HASH(inventory, name)))
        return item;
    if (inventory->has_default
        && (item = g_hash_table_lookup(inventory->hash,
                                       inventory->default_item_key)))
        return item;

    if (inventory->items->len)
        return ITEM(inventory, 0);
    else
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
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    g_return_val_if_fail(n < inventory->items->len, NULL);
    return ITEM(inventory, n);
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
    gpointer item;
    guint i;

    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), -1);
    if (!(item = HASH(inventory, name)))
        return -1;

    /* TODO: this is a linear search, even if it's a very fast linear
     * search.  For large sorted inventories we should implement bisection. */
    for (i = 0; i < inventory->items->len; i++) {
        if (ITEM(inventory, i) == item)
            return i;
    }
    g_return_val_if_reached(-1);
}

/**
 * gwy_inventory_foreach:
 * @inventory: An inventory.
 * @function: A function to call on each item.
 * @user_data: Data passed to @function.
 *
 * Calls a function on each item of an inventory.
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
    for (i = 0; i < inventory->items->len; i++)
        function(GUINT_TO_POINTER(i), ITEM(inventory, i), user_data);
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
    g_return_val_if_fail(GWY_IS_INVENTORY(inventory), NULL);
    if (!inventory->has_default)
        return NULL;
    return g_hash_table_lookup(inventory->hash, inventory->default_item_key);
}

/**
 * gwy_inventory_set_default_item:
 * @inventory: An inventory.
 * @name: Item name.
 *
 * Sets the default of an inventory.
 *
 * Item @name must already exist in the inventory.
 **/
void
gwy_inventory_set_default_item(GwyInventory *inventory,
                               const gchar *name)
{
    gpointer key;

    g_return_if_fail(GWY_IS_INVENTORY(inventory));
    key = GUINT_TO_POINTER(g_str_hash(name));
    if (key == inventory->default_item_key)
        return;

    if (!g_hash_table_lookup(inventory->hash, inventory->default_item_key))
        g_warning("New default item not present in inventory");
    else
        inventory->default_item_key = key;
}

/************************** Documentation ****************************/

/**
 * GwyInventory:
 *
 * The #GwyInventory struct contains private data only and should be accessed
 * using the functions below.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
