/*
 *  @(#) $Id$
 *  Copyright (C) 2011-2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_SURFACE_H__
#define __GWY_SURFACE_H__

#include <libgwyddion/gwyserializable.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>

G_BEGIN_DECLS

#define GWY_TYPE_SURFACE \
    (gwy_surface_get_type())
#define GWY_SURFACE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SURFACE, GwySurface))
#define GWY_SURFACE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SURFACE, GwySurfaceClass))
#define GWY_IS_SURFACE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SURFACE))
#define GWY_IS_SURFACE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SURFACE))
#define GWY_SURFACE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SURFACE, GwySurfaceClass))

typedef struct _GwySurface      GwySurface;
typedef struct _GwySurfaceClass GwySurfaceClass;

struct _GwySurface {
    GObject g_object;
    struct _GwySurfacePrivate *priv;
    /*<public>*/
    guint n;
    GwyXYZ *data;
};

struct _GwySurfaceClass {
    GObjectClass g_object_class;

    void (*data_changed)(GwySurface *surface);
    /*< private >*/
    void (*reserved1)(void);
};

#define gwy_surface_duplicate(surface) \
    (GWY_SURFACE(gwy_serializable_duplicate(G_OBJECT(surface))))
#define gwy_surface_clone(dest, src) \
    (gwy_serializable_clone(G_OBJECT(dest), G_OBJECT(src)))

GType             gwy_surface_get_type           (void)                       G_GNUC_CONST;
GwySurface*       gwy_surface_new                (void);
GwySurface*       gwy_surface_new_sized          (guint n);
GwySurface*       gwy_surface_new_from_data      (const GwyXYZ *points,
                                                  guint n);
GwySurface*       gwy_surface_new_alike          (GwySurface *model);
GwySurface*       gwy_surface_new_part           (GwySurface *surface,
                                                  gdouble xfrom,
                                                  gdouble xto,
                                                  gdouble yfrom,
                                                  gdouble yto);
GwyXYZ*           gwy_surface_get_data           (GwySurface *surface);
const GwyXYZ*     gwy_surface_get_data_const     (GwySurface *surface);
guint             gwy_surface_get_npoints        (GwySurface *surface);
void              gwy_surface_data_changed       (GwySurface *surface);
void              gwy_surface_copy               (GwySurface *src,
                                                  GwySurface *dest);
void              gwy_surface_invalidate         (GwySurface *surface);
void              gwy_surface_set_from_data_field(GwySurface *surface,
                                                  GwyDataField *data_field);
GwySIUnit*        gwy_surface_get_si_unit_xy     (GwySurface *surface);
GwySIUnit*        gwy_surface_get_si_unit_z      (GwySurface *surface);
void              gwy_surface_set_si_unit_xy     (GwySurface *surface,
                                                  GwySIUnit *si_unit);
void              gwy_surface_set_si_unit_z      (GwySurface *surface,
                                                  GwySIUnit *si_unit);
GwySIValueFormat* gwy_surface_get_value_format_xy(GwySurface *surface,
                                                  GwySIUnitFormatStyle style,
                                                  GwySIValueFormat *format);
GwySIValueFormat* gwy_surface_get_value_format_z (GwySurface *surface,
                                                  GwySIUnitFormatStyle style,
                                                  GwySIValueFormat *format);
GwyXYZ            gwy_surface_get                (GwySurface *surface,
                                                  guint pos);
void              gwy_surface_set                (GwySurface *surface,
                                                  guint pos,
                                                  GwyXYZ point);
void              gwy_surface_get_xrange         (GwySurface *surface,
                                                  gdouble *min,
                                                  gdouble *max);
void              gwy_surface_get_yrange         (GwySurface *surface,
                                                  gdouble *min,
                                                  gdouble *max);
void              gwy_surface_get_min_max        (GwySurface *surface,
                                                  gdouble *min,
                                                  gdouble *max);
const GwyXYZ*     gwy_surface_get_data_full      (GwySurface *surface,
                                                  guint *n);
void              gwy_surface_set_data_full      (GwySurface *surface,
                                                  const GwyXYZ *points,
                                                  guint n);
gboolean          gwy_surface_xy_is_compatible   (GwySurface *surface,
                                                  GwySurface *othersurface);

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
