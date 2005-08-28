/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_MATH_H__
#define __GWY_MATH_H__

#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <math.h>

G_BEGIN_DECLS

#define ROUND(x) ((gint)floor((x) + 0.5))

gdouble      gwy_math_humanize_numbers   (gdouble unit,
                                          gdouble maximum,
                                          gint *precision);
gint         gwy_math_find_nearest_line  (gdouble x,
                                          gdouble y,
                                          gdouble *d2min,
                                          gint n,
                                          gdouble *coords);
gint         gwy_math_find_nearest_point (gdouble x,
                                          gdouble y,
                                          gdouble *d2min,
                                          gint n,
                                          gdouble *coords);
gdouble*     gwy_math_lin_solve          (gint n,
                                          const gdouble *matrix,
                                          const gdouble *rhs,
                                          gdouble *result);
gdouble*     gwy_math_lin_solve_rewrite  (gint n,
                                          gdouble *matrix,
                                          gdouble *rhs,
                                          gdouble *result);
gdouble*     gwy_math_fit_polynom        (gint ndata,
                                          const gdouble *xdata,
                                          const gdouble *ydata,
                                          gint n,
                                          gdouble *coeffs);
gboolean     gwy_math_choleski_decompose (gint n,
                                          gdouble *matrix);
void         gwy_math_choleski_solve     (gint n,
                                          const gdouble *decomp,
                                          gdouble *rhs);
gdouble      gwy_math_median             (gsize n,
                                          gdouble *array);
void         gwy_math_sort               (gsize n,
                                          gdouble *array);

G_END_DECLS

#endif /* __GWY_MATH_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
