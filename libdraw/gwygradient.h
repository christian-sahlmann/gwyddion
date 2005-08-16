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

#ifndef __GWY_GRADIENT_H__
#define __GWY_GRADIENT_H__

#include <glib-object.h>
#include <libgwyddion/gwyresource.h>
#include <libdraw/gwyrgba.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRADIENT                  (gwy_gradient_get_type())
#define GWY_GRADIENT(obj)                  (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRADIENT, GwyGradient))
#define GWY_GRADIENT_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRADIENT, GwyGradientClass))
#define GWY_IS_GRADIENT(obj)               (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRADIENT))
#define GWY_IS_GRADIENT_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRADIENT))
#define GWY_GRADIENT_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRADIENT, GwyGradientClass))

typedef struct _GwyGradient      GwyGradient;
typedef struct _GwyGradientClass GwyGradientClass;

typedef struct {
    gdouble x;
    GwyRGBA color;
} GwyGradientPoint;

struct _GwyGradient {
    GwyResource parent_instance;

    gboolean favourite;
    gboolean boolean1;
    GArray *points;
    guchar *pixels;
    GdkPixbuf *pixbuf;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyGradientClass {
    GwyResourceClass parent_class;

    GHashTable *gradients;

    void (*data_changed)(GwyGradient *gradient);

    gpointer reserved1;
    gpointer reserved2;
};

GType             gwy_gradient_get_type              (void) G_GNUC_CONST;
void              gwy_gradient_get_color             (GwyGradient *gradient,
                                                      gdouble x,
                                                      GwyRGBA *color);
const guchar*     gwy_gradient_get_samples           (GwyGradient *gradient,
                                                      gint *nsamples);
guchar*           gwy_gradient_sample                (GwyGradient *gradient,
                                                      gint nsamples,
                                                      guchar *samples);
gint              gwy_gradient_get_npoints           (GwyGradient *gradient);
GwyGradientPoint  gwy_gradient_get_point             (GwyGradient *gradient,
                                                      gint index_);
void              gwy_gradient_set_point             (GwyGradient *gradient,
                                                      gint index_,
                                                      const GwyGradientPoint *point);
void              gwy_gradient_set_point_color       (GwyGradient *gradient,
                                                      gint index_,
                                                      const GwyRGBA *color);
void              gwy_gradient_insert_point          (GwyGradient *gradient,
                                                      gint index_,
                                                      GwyGradientPoint *point);
gint              gwy_gradient_insert_point_sorted   (GwyGradient *gradient,
                                                      GwyGradientPoint *point);
void              gwy_gradient_delete_point          (GwyGradient *gradient,
                                                      gint index_);
void              gwy_gradient_reset                 (GwyGradient *gradient);
const
GwyGradientPoint* gwy_gradient_get_points            (GwyGradient *gradient,
                                                      gint *npoints);
void              gwy_gradient_set_points            (GwyGradient *gradient,
                                                      gint npoints,
                                                      const GwyGradientPoint *points);
void              gwy_gradient_set_from_samples      (GwyGradient *gradient,
                                                      gint nsamples,
                                                      guchar *samples);

GwyInventory*     gwy_gradients                      (void);

#define gwy_gradients_get_gradient(name) \
    gwy_inventory_get_item_or_default(gwy_gradients(), name)

G_END_DECLS

#endif /*__GWY_GRADIENT_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
