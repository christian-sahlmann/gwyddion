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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_NULL_STORE_H__
#define __GWY_NULL_STORE_H__

#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define GWY_TYPE_NULL_STORE            (gwy_null_store_get_type())
#define GWY_NULL_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_NULL_STORE, GwyNullStore))
#define GWY_NULL_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_NULL_STORE, GwyNullStoreClass))
#define GWY_IS_NULL_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_NULL_STORE))
#define GWY_IS_NULL_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_NULL_STORE))
#define GWY_NULL_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_NULL_STORE, GwyNullStoreClass))

typedef struct _GwyNullStore       GwyNullStore;
typedef struct _GwyNullStoreClass  GwyNullStoreClass;

struct _GwyNullStore {
    GObject parent_instance;

    gpointer model;
    GDestroyNotify model_destroy;

    guint n;
    gint stamp;

    gpointer reserved1;
};

struct _GwyNullStoreClass {
    GObjectClass parent_class;

    void (*reserved1)(void);
};


GType         gwy_null_store_get_type     (void) G_GNUC_CONST;
GwyNullStore* gwy_null_store_new          (guint n);
guint         gwy_null_store_get_n_rows   (GwyNullStore *store);
void          gwy_null_store_set_n_rows   (GwyNullStore *store,
                                           guint n);
gpointer      gwy_null_store_get_model    (GwyNullStore *store);
void          gwy_null_store_set_model    (GwyNullStore *store,
                                           gpointer model,
                                           GDestroyNotify destroy);
void          gwy_null_store_row_changed  (GwyNullStore *store,
                                           guint i);
gboolean      gwy_null_store_iter_is_valid(GwyNullStore *store,
                                           GtkTreeIter *iter);

G_END_DECLS

#endif /* __GWY_NULL_STORE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

