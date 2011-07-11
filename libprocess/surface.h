/*
 *  @(#) $Id: surface.h 6698 2006-09-28 21:50:13Z yeti-dn $
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_SURFACE_H__
#define __GWY_SURFACE_H__

#include <libgwyddion/gwysiunit.h>
#include <libprocess/datafield.h>
#include <libprocess/gwyprocessenums.h>

G_BEGIN_DECLS

typedef enum {
    GWY_SURFACE_REGULARIZE_PREVIEW,
} GwySurfaceRegularizeType;

#define GWY_TYPE_SURFACE            (gwy_surface_get_type())
#define GWY_SURFACE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SURFACE, GwySurface))
#define GWY_SURFACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SURFACE, GwySurfaceClass))
#define GWY_IS_SURFACE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SURFACE))
#define GWY_IS_SURFACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SURFACE))
#define GWY_SURFACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SURFACE, GwySurfaceClass))

/* Cache operations */
#define GWY_SURFACE_CVAL(arg, bit)  ((arg)->cache[GWY_SURFACE_CACHE_##bit])
#define GWY_SURFACE_CBIT(bit)       (1 << GWY_SURFACE_CACHE_##bit)
#define GWY_SURFACE_CTEST(arg, bit) ((arg)->cached & GWY_SURFACE_CBIT(bit))

typedef enum {
    GWY_SURFACE_CACHE_MIN = 0,
    GWY_SURFACE_CACHE_MAX,
    GWY_SURFACE_CACHE_AVG,
    GWY_SURFACE_CACHE_RMS,
    GWY_SURFACE_CACHE_MSQ,  /* Not implemented yet */
    GWY_SURFACE_CACHE_MED,
    GWY_SURFACE_CACHE_ARF,  /* Not implemented yet */
    GWY_SURFACE_CACHE_ART,  /* Not implemented yet */
    GWY_SURFACE_CACHE_ARE,  /* Not implemented yet */
    GWY_SURFACE_CACHE_SIZE
} GwySurfaceCached;

typedef struct _GwyXYZ          GwyXYZ;
typedef struct _GwySurface      GwySurface;
typedef struct _GwySurfaceClass GwySurfaceClass;

struct _GwyXYZ {
	gdouble x;
	gdouble y;
	gdouble z;
};
	


struct _GwySurface {
    GObject parent_instance;
    
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;

    GwySIUnit *si_unit_xy;
    GwySIUnit *si_unit_z;

    gboolean cached_range;
    guint32 cached;
    gdouble cache[GWY_SURFACE_CACHE_SIZE];

    guint32 n;
    GwyXYZ *data;
};

struct _GwySurfaceClass {
    GObjectClass parent_class;

    void (*data_changed)(GwySurface *surface);
    void (*reserved1)(void);
};

#define gwy_surface_invalidate(surface) (((surface)->cached = 0), ((surface)->cached_range = FALSE))

#define gwy_surface_duplicate(surface) \
        (GWY_SURFACE(gwy_serializable_duplicate(G_OBJECT(surface))))

GType             gwy_surface_get_type  (void) G_GNUC_CONST;

GwySurface*       gwy_surface_new                    (gdouble xmin,
                                                      gdouble xmax,
                                                      gdouble ymin,
                                                      gdouble ymax,
                                                      guint32 n,
                                                      gboolean nullme);
GwySurface*       gwy_surface_new_alike              (const GwySurface *model,
                                                      gboolean nullme);
void              gwy_surface_data_changed           (GwySurface *surface);
void              gwy_surface_copy                   (GwySurface *src,
                                                      GwySurface *dest,
                                                      gboolean nondata_too);
GwySurface*       gwy_surface_new_part               (const GwySurface *surface,
                                                      gdouble xfrom,
                                                      gdouble xto,
                                                      gdouble yfrom,
                                                      gdouble yto);
void              copy_field_to_surface              (const GwyDataField *field,
                                                      GwySurface *surface);
GwySurface*       gwy_surface_new_from_field         (const GwyDataField *datafield);
void              gwy_surface_set_from_field         (GwySurface *surface,
                                                      const GwyDataField *field);
GwyDataField*     gwy_surface_regularize_full        (GwySurface *surface,
                                                      GwySurfaceRegularizeType method,
                                                      guint xres, guint yres);
GwyDataField*     gwy_surface_regularize             (GwySurface *surface,
                                                      GwySurfaceRegularizeType method,
                                                      gdouble xfrom, gdouble xto,
                                                      gdouble yfrom, gdouble yto,
                                                      guint xres, guint yres);
GwySIValueFormat* gwy_surface_get_format_xy          (GwySurface *surface,
                                                      GwySIUnitFormatStyle style,
                                                      GwySIValueFormat *format);
GwySIValueFormat* gwy_surface_get_format_z           (GwySurface *surface,
                                                      GwySIUnitFormatStyle style,
                                                      GwySIValueFormat *format);


void
gwy_surface_print_info(GwySurface *surface, int n_values);


G_END_DECLS

#endif /* __GWY_DATAFIELD_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
