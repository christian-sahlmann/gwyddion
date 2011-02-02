/*
 *  @(#) $Id: natural.h 11428 2010-10-18 08:34:36Z dn2010 $
 *  Copyright (C) 2009 Ross Hemsley, David Necas (Yeti), Petr Klapetek.
 *  E-mail: rh7223@bris.ac.uk, yeti@gwyddion.net, klapetek@gwyddion.net.
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

/*< private_header >*/

#ifndef __GWY_PROCESS_NATURAL_H__
#define __GWY_PROCESS_NATURAL_H__

#include <glib.h>

typedef struct _GwyDelaunayVertex GwyDelaunayVertex;
typedef struct _GwyDelaunayMesh GwyDelaunayMesh;

G_GNUC_INTERNAL
GwyDelaunayVertex *_gwy_delaunay_vertex_new(gdouble *x, gdouble *y, gdouble *z,
                   gdouble *u, gdouble *v, gdouble *w, gint n);

//void     _gwy_delaunay_vertex_free(GwyDelaunayVertex *ps);

G_GNUC_INTERNAL
GwyDelaunayMesh* _gwy_delaunay_mesh_new();

G_GNUC_INTERNAL
void             _gwy_delaunay_mesh_build(GwyDelaunayMesh *m, GwyDelaunayVertex* ps, gint n);

G_GNUC_INTERNAL
void     _gwy_delaunay_mesh_interpolate3_3(GwyDelaunayMesh *m, gdouble  x, gdouble  y, gdouble  z,
                        gdouble *u, gdouble *v, gdouble *w);

G_GNUC_INTERNAL
void     _gwy_delaunay_mesh_free(GwyDelaunayMesh *m);

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
