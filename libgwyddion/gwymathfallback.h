/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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

#ifndef __GWY_MATH_FALLBACK_H__
#define __GWY_MATH_FALLBACK_H__

#include <math.h>
#include <glib/gutils.h>
#include <gwyconfig.h>

G_BEGIN_DECLS

/* This is necessary to fool gtk-doc that ignores static inline functions */
#define _GWY_STATIC_INLINE static inline

_GWY_STATIC_INLINE double gwy_math_fallback_pow10(double x);
_GWY_STATIC_INLINE double gwy_math_fallback_cbrt (double x);
_GWY_STATIC_INLINE double gwy_math_fallback_hypot(double x, double y);
_GWY_STATIC_INLINE double gwy_math_fallback_acosh(double x);
_GWY_STATIC_INLINE double gwy_math_fallback_asinh(double x);
_GWY_STATIC_INLINE double gwy_math_fallback_atanh(double x);

#undef _GWY_STATIC_INLINE

static inline double
gwy_math_fallback_pow10(double x)
{
    return pow(10.0, x);
}

static inline double
gwy_math_fallback_cbrt(double x)
{
    return pow(x, 1.0/3.0);
}

static inline double
gwy_math_fallback_hypot(double x, double y)
{
    return sqrt(x*x + y*y);
}

static inline double
gwy_math_fallback_acosh(double x)
{
    return log(x + sqrt(x*x - 1.0));
}

static inline double
gwy_math_fallback_asinh(double x)
{
    return log(x + sqrt(x*x + 1.0));
}

static inline double
gwy_math_fallback_atanh(double x)
{
    return log((1.0 + x)/(1.0 - x));
}

#ifndef GWY_MATH_NAMESPACE_CLEAN

#ifndef GWY_HAVE_POW10
#define pow10 gwy_math_fallback_pow10
#endif  /* GWY_HAVE_POW10 */

#ifndef GWY_HAVE_CBRT
#define cbrt gwy_math_fallback_cbrt
#endif  /* GWY_HAVE_CBRT */

#ifndef GWY_HAVE_HYPOT
#define hypot gwy_math_fallback_hypot
#endif  /* GWY_HAVE_HYPOT */

#ifndef GWY_HAVE_ACOSH
#define acosh gwy_math_fallback_acosh
#endif  /* GWY_HAVE_ACOSH */

#ifndef GWY_HAVE_ASINH
#define asinh gwy_math_fallback_asinh
#endif  /* GWY_HAVE_ASINH */

#ifndef GWY_HAVE_ATANH
#define atanh gwy_math_fallback_atanh
#endif  /* GWY_HAVE_ATANH */

#endif /* GWY_MATH_NAMESPACE_CLEAN */

G_END_DECLS

#endif /* __GWY_MATH_FALLBACK_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
