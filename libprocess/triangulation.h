/*
 *  @(#) $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GWY_PROCESS_TRIANGULATION_H__
#define __GWY_PROCESS_TRIANGULATION_H__ 1

#include <libprocess/datafield.h>

G_BEGIN_DECLS

#define GWY_TRIANGULATION_NONE G_MAXUINT

typedef struct {
    gdouble x;
    gdouble y;
} GwyTriangulationPointXY;

typedef struct {
    gdouble x;
    gdouble y;
    gdouble z;
} GwyTriangulationPointXYZ;

typedef struct {
    guint npoints;
    guint size;
    const guint *index;
    const guint *neighbours;
} GwyTriangulationData;

#define GWY_TYPE_TRIANGULATION            (gwy_triangulation_get_type())
#define GWY_TRIANGULATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_TRIANGULATION, GwyTriangulation))
#define GWY_TRIANGULATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_TRIANGULATION, GwyTriangulationClass))
#define GWY_IS_TRIANGULATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_TRIANGULATION))
#define GWY_IS_TRIANGULATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_TRIANGULATION))
#define GWY_TRIANGULATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_TRIANGULATION, GwyTriangulationClass))

typedef struct _GwyTriangulation      GwyTriangulation;
typedef struct _GwyTriangulationClass GwyTriangulationClass;

struct _GwyTriangulation {
    GObject parent_instance;
};

struct _GwyTriangulationClass {
    GObjectClass parent_class;
};

GType                 gwy_triangulation_get_type   (void)                                      G_GNUC_CONST;
GwyTriangulation*     gwy_triangulation_new        (void);
gboolean              gwy_triangulation_triangulate(GwyTriangulation *triangulation,
                                                    guint npoints,
                                                    gconstpointer points,
                                                    gsize point_size);
gboolean              gwy_triangulation_interpolate(GwyTriangulation *triangulation,
                                                    GwyInterpolationType interpolation,
                                                    GwyDataField *dfield);
void                  gwy_triangulation_data_free  (GwyTriangulationData *triangulation_data);
GwyTriangulationData* gwy_triangulation_delaunay   (GwyTriangulation *triangulation);
GwyTriangulationData* gwy_triangulation_boundary   (GwyTriangulation *triangulation);
GwyTriangulationData* gwy_triangulation_voronoi    (GwyTriangulation *triangulation,
                                                    guint *nvpoints,
                                                    const GwyTriangulationPointXY **vpoints);

G_END_DECLS

#endif /* __GWY_PROCESS_TRIANGULATION_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
