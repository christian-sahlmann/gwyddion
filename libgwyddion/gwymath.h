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

/* TODO: Remove in 3.0.  This is kept included only because we have
 * traditionally defined unprefix cbrt() et al. if the platform did not
 * provide it. */
#include <libgwyddion/gwymathfallback.h>

G_BEGIN_DECLS

#ifndef GWY_DISABLE_DEPRECATED
#define ROUND(x) ((gint)floor((x) + 0.5))
#endif

#define GWY_ROUND(x) ((gint)floor((x) + 0.5))

#define GWY_SQRT3 1.73205080756887729352744634150587236694280525381038
#define GWY_SQRT_PI 1.77245385090551602729816748334114518279754945612237

gdouble  gwy_math_humanize_numbers     (gdouble unit,
                                        gdouble maximum,
                                        gint *precision);
gboolean gwy_math_is_in_polygon        (gdouble x,
                                        gdouble y,
                                        const gdouble* poly,
                                        guint n);
gint     gwy_math_find_nearest_line    (gdouble x,
                                        gdouble y,
                                        gdouble *d2min,
                                        gint n,
                                        const gdouble *coords,
                                        const gdouble *metric);
gint     gwy_math_find_nearest_point   (gdouble x,
                                        gdouble y,
                                        gdouble *d2min,
                                        gint n,
                                        const gdouble *coords,
                                        const gdouble *metric);
gdouble* gwy_math_lin_solve            (gint n,
                                        const gdouble *matrix,
                                        const gdouble *rhs,
                                        gdouble *result);
gdouble* gwy_math_lin_solve_rewrite    (gint n,
                                        gdouble *matrix,
                                        gdouble *rhs,
                                        gdouble *result);
gboolean gwy_math_tridiag_solve_rewrite(gint n,
                                        gdouble *d,
                                        const gdouble *a,
                                        const gdouble *b,
                                        gdouble *rhs);
gdouble* gwy_math_fit_polynom          (gint ndata,
                                        const gdouble *xdata,
                                        const gdouble *ydata,
                                        gint n,
                                        gdouble *coeffs);
gboolean gwy_math_choleski_decompose   (gint n,
                                        gdouble *matrix);
void     gwy_math_choleski_solve       (gint n,
                                        const gdouble *decomp,
                                        gdouble *rhs);
guint    gwy_math_curvature            (const gdouble *coeffs,
                                        gdouble *kappa1,
                                        gdouble *kappa2,
                                        gdouble *phi1,
                                        gdouble *phi2,
                                        gdouble *xc,
                                        gdouble *yc,
                                        gdouble *zc);
gdouble  gwy_math_median               (gsize n,
                                        gdouble *array);
void     gwy_math_sort                 (gsize n,
                                        gdouble *array);

G_END_DECLS

#endif /* __GWY_MATH_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
