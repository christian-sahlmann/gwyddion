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

#ifndef __GWY_INVENTORY_STORE_H__
#define __GWY_INVENTORY_STORE_H__

#include <gtk/gtktreemodel.h>
#include <libgwyddion/gwyinventory.h>

G_BEGIN_DECLS

#define GWY_TYPE_INVENTORY_STORE            (gwy_inventory_store_get_type())
#define GWY_INVENTORY_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_INVENTORY_STORE, GwyInventoryStore))
#define GWY_INVENTORY_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_INVENTORY_STORE, GwyInventoryStoreClass))
#define GWY_IS_INVENTORY_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_INVENTORY_STORE))
#define GWY_IS_INVENTORY_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_INVENTORY_STORE))
#define GWY_INVENTORY_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_INVENTORY_STORE, GwyInventoryStoreClass))

typedef struct _GwyInventoryStore       GwyInventoryStore;
typedef struct _GwyInventoryStoreClass  GwyInventoryStoreClass;

struct _GwyInventoryStore
{
    GObject parent_instance;

    gint stamp;
    GwyInventory *inventory;

    /* Cached inventory properties */
    const GwyInventoryItemType *item_type;
};

struct _GwyInventoryStoreClass
{
    GObjectClass parent_class;
};


GType              gwy_inventory_store_get_type     (void) G_GNUC_CONST;
GwyInventoryStore* gwy_inventory_store_new          (GwyInventory *inventory);
GwyInventory*      gwy_inventory_store_get_inventory(GwyInventoryStore *store);
gint          gwy_inventory_store_get_column_by_name(GwyInventoryStore *store,
                                                     const gchar *name);
gboolean           gwy_inventory_store_iter_is_valid(GwyInventoryStore *store,
                                                     GtkTreeIter *iter);

G_END_DECLS

#endif /* __GWY_INVENTORY_STORE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

