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

    if (!inventory->is_simple) {
        gint i;

        /* FIXME */
        for (i = 0; i < inventory->items->len; i++)
            g_object_unref(g_array_index(inventory->items, GObject*, i));
    }
    g_array_free(inventory->items, TRUE);
    g_free(inventory->default_item_name);

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
    GwyInventory *inventory;

    gwy_debug("");
    g_return_val_if_fail(item_type, NULL);

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
    /* FIXME */
    inventory->items = g_array_new(FALSE, FALSE, sizeof(gpointer));

    return inventory;
}

/************************** Documentation ****************************/

/**
 * GwyInventory:
 *
 * The #GwyInventory struct contains private data only and should be accessed
 * using the functions below.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
