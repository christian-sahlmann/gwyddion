/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LIBGWY_SURFACE_STATISTICS_H__
#define __LIBGWY_SURFACE_STATISTICS_H__

#include <libprocess/surface.h>


G_BEGIN_DECLS

void    gwy_surface_min_max_full(GwySurface *surface,
                                 gdouble *min,
                                 gdouble *max);
void    gwy_surface_xrange_full (GwySurface *surface,
                                 gdouble *min,
                                 gdouble *max);
void    gwy_surface_yrange_full (GwySurface *surface,
                                 gdouble *min,
                                 gdouble *max);
gdouble gwy_surface_mean_full   (GwySurface *surface) G_GNUC_PURE;
gdouble gwy_surface_rms_full    (GwySurface *surface) G_GNUC_PURE;

G_END_DECLS

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
