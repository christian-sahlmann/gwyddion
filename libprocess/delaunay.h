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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#ifndef __GWY_PROCESS_DELAUNAY_H__
#define __GWY_PROCESS_DELAUNAY_H__ 1

#include <libprocess/datafield.h>

G_BEGIN_DECLS

#define GWY_DELAUNAY_NONE G_MAXUINT

typedef struct {
    gdouble x;
    gdouble y;
} GwyDelaunayPointXY;

typedef struct {
    gdouble x;
    gdouble y;
    gdouble z;
} GwyDelaunayPointXYZ;

typedef struct {
    guint npoints;
    guint nsize;
    guint blen;
    guint nvpoints;
    guint nvoronoi;
    guint *index;
    guint *neighbours;
    guint *boundary;
    guint *bindex;
    GwyDelaunayPointXY *vpoints;
    guint *vindex;
    guint *voronoi;
} GwyDelaunayTriangulation;

GwyDelaunayTriangulation* gwy_delaunay_triangulate(guint npoints,
                                                   gconstpointer points,
                                                   gsize point_size);

void gwy_delaunay_triangulation_free(GwyDelaunayTriangulation *triangulation);

gboolean gwy_delaunay_interpolate(GwyDelaunayTriangulation *triangulation,
                                  gconstpointer points,
                                  gsize point_size,
                                  GwyInterpolationType interpolation,
                                  GwyDataField *dfield);

G_END_DECLS

#endif /* __GWY_PROCESS_DELAUNAY_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
