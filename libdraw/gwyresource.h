/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_RESOURCE_H__
#define __GWY_RESOURCE_H__

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GWY_TYPE_RESOURCE                  (gwy_resource_get_type())
#define GWY_RESOURCE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RESOURCE, GwyResource))
#define GWY_RESOURCE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RESOURCE, GwyResourceClass))
#define GWY_IS_RESOURCE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RESOURCE))
#define GWY_IS_RESOURCE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RESOURCE))
#define GWY_RESOURCE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RESOURCE, GwyResourceClass))

typedef struct _GwyResource      GwyResource;
typedef struct _GwyResourceClass GwyResourceClass;

struct _GwyResource {
    GObject parent_instance;

    gint use_count;
    gchar *name;

    GdkPixbuf *pixbuf;
    gint pixbuf_use_count;

    gboolean is_const : 1;
    gboolean boolean1 : 1;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyResourceClass {
    GObjectClass parent_class;

    /* Traits */
    gint n_traits;
    GType *traits;

    /* Signals */
    void (*data_changed)(GwyResource *resource);

    /* Virtual table */
    void (*use)(GwyResource *resource);
    void (*unuse)(GwyResource *resource);
    void (*get_trait)(GwyResource *resource,
                      gint i,
                      GValue *value);
    GdkPixbuf* (*make_pixbuf)(GwyResource *resource);
    GString* (*dump)(GwyResource *resource);
    GwyResource* (*parse)(const gchar *text);

    gpointer reserved1;
    gpointer reserved2;
};

GType             gwy_resource_get_type              (void) G_GNUC_CONST;
const gchar*      gwy_resource_get_name              (GwyResource *resource);
gboolean          gwy_resource_get_is_modifiable     (GwyResource *resource);
const GType*      gwy_resource_class_get_traits      (GwyResourceClass *klass,
                                                      gint *ntraits);
void              gwy_resource_get_trait             (GwyResource *resource,
                                                      gint n,
                                                      GValue *value);
void              gwy_resource_ref                   (GwyResource *resource);
void              gwy_resource_unref                 (GwyResource *resource);
GdkPixbuf*        gwy_resource_ref_pixbuf            (GwyResource *resource);
void              gwy_resource_unref_pixbuf          (GwyResource *resource);
void              gwy_resource_data_changed          (GwyResource *resource);
GString*          gwy_resource_dump                  (GwyResource *resource);
GwyResource*      gwy_resource_parse                 (const gchar *text);

G_END_DECLS


#endif /*__GWY_RESOURCE_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
