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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/surface-statistics.h>

#define CVAL GWY_SURFACE_CVAL
#define CBIT GWY_SURFACE_CBIT
#define CTEST GWY_SURFACE_CTEST

#define GWY_MAYBE_SET(pointer, value) \
    do { if (pointer) *(pointer) = (value); } while (0)

/**
 * gwy_surface_min_max_full:
 * @surface: A surface.
 * @min: (allow-none):
 *       Location to store the minimum value to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum value to, or %NULL.
 *
 * Finds the minimum and maximum value in a surface.
 *
 * If the surface is empty @min is set to a huge positive value and @max to
 * a huge negative value.
 *
 * The minimum and maximum of the entire surface are cached, see
 * gwy_surface_invalidate().
 **/
void
gwy_surface_min_max_full(GwySurface *surface,
                         gdouble *pmin,
                         gdouble *pmax)
{
    const GwyXYZ *p;
    guint i;    

    gdouble min = HUGE_VAL;
    gdouble max = -HUGE_VAL;

    g_return_if_fail(GWY_IS_SURFACE(surface));
    if (!pmin && !pmax)
        return;

    if (CTEST(surface, MIN) && CTEST(surface, MAX)) {
        GWY_MAYBE_SET(pmin, CVAL(surface, MIN));
        GWY_MAYBE_SET(pmax, CVAL(surface, MAX));
        return;
    }

    p = surface->data;
    for (i = surface->n; i; i--, p++) {
        if (p->z < min)
            min = p->z;
        if (p->z > max)
            max = p->z;
    }
    CVAL(surface, MIN) = min;
    CVAL(surface, MAX) = max;
    surface->cached |= CBIT(MIN) | CBIT(MAX);
    GWY_MAYBE_SET(pmin, min);
    GWY_MAYBE_SET(pmax, max);
}

static void
surface_ensure_range(GwySurface *surface)
{
    guint i;
    gdouble xmin = HUGE_VAL, ymin = HUGE_VAL;
    gdouble xmax = -HUGE_VAL, ymax = -HUGE_VAL;
    const GwyXYZ *p = surface->data;

    if (surface->cached_range)
        return;
    
    for (i = surface->n; i; i--, p++) {
        if (p->x < xmin)
            xmin = p->x;
        if (p->x > xmax)
            xmax = p->x;
        if (p->y < ymin)
            ymin = p->y;
        if (p->y > ymax)
            ymax = p->y;
    }

    surface->cached_range = TRUE;
    surface->xmin = xmin;
    surface->xmax = xmax;
    surface->ymin = ymin;
    surface->ymax = ymax;
}

/**
 * gwy_surface_xrange_full:
 * @surface: A surface.
 * @min: (allow-none):
 *       Location to store the minimum x-coordinate to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum x-coordinate to, or %NULL.
 *
 * Finds the minimum and maximum x-coordinates of an entire surface.
 *
 * If the surface is empty @min and @max are set to NaN.
 *
 * The bounding box of the entire surface is cached, see
 * gwy_surface_invalidate().
 **/
void
gwy_surface_xrange_full(GwySurface *surface,
                        gdouble *pmin,
                        gdouble *pmax)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    if (!pmin && !pmax)
        return;

    surface_ensure_range(surface);
    GWY_MAYBE_SET(pmin, surface->xmin);
    GWY_MAYBE_SET(pmax, surface->xmax);
}

/**
 * gwy_surface_yrange_full:
 * @surface: A surface.
 * @min: (allow-none):
 *       Location to store the minimum y-coordinate to, or %NULL.
 * @max: (allow-none):
 *       Location to store the maximum y-coordinate to, or %NULL.
 *
 * Finds the minimum and maximum y-coordinates of an entire surface.
 *
 * If the surface is empty @min and @max are set to NaN.
 *
 * The bounding box of the entire surface is cached, see
 * gwy_surface_invalidate().
 **/
void
gwy_surface_yrange_full(GwySurface *surface,
                        gdouble *pmin,
                        gdouble *pmax)
{
    g_return_if_fail(GWY_IS_SURFACE(surface));
    if (!pmin && !pmax)
        return;

    surface_ensure_range(surface);
    GWY_MAYBE_SET(pmin, surface->ymin);
    GWY_MAYBE_SET(pmax, surface->ymax);
}

/**
 * gwy_surface_mean_full:
 * @surface: A surface.
 *
 * Calculates the mean value of an entire surface.
 *
 * The mean value of the entire surface is cached, see
 * gwy_surface_invalidate().
 *
 * Returns: The mean value.  The mean value of an empty surface is NaN.
 **/
gdouble
gwy_surface_mean_full(GwySurface *surface)
{
    gdouble s = 0.0;
    guint i;
    const GwyXYZ *p;

    g_return_val_if_fail(GWY_IS_SURFACE(surface), NAN);
    if (G_UNLIKELY(!surface->n))
        return NAN;
    if (CTEST(surface, AVG))
        return CVAL(surface, AVG); 

    
    p = surface->data;
    for (i = surface->n; i; i--, p++)
        s += p->z;

    s /= surface->n;
    CVAL(surface, AVG) = s;
    surface->cached |= CBIT(AVG);

    return s;
}

/**
 * gwy_surface_rms_full:
 * @surface: A surface.
 *
 * Calculates the mean square value of an entire surface.
 *
 * The mean square value of the entire surface is cached, see
 * gwy_surface_invalidate().
 *
 * Returns: The mean square value.  The mean value square of an empty surface
 *          is zero.
 **/
gdouble
gwy_surface_rms_full(GwySurface *surface)
{
    gdouble mean, s=0.0;
    const GwyXYZ *p;
    guint i;

    g_return_val_if_fail(GWY_IS_SURFACE(surface), 0.0);
    if (G_UNLIKELY(!surface->n))
        return 0.0;
    if (CTEST(surface, RMS))
        return CVAL(surface, RMS);

    mean = gwy_surface_mean_full(surface);
    
    p = surface->data;
    for (i = surface->n; i; i--, p++)
        s += (p->z - mean)*(p->x - mean);

    s = sqrt(s/surface->n);
    CVAL(surface, RMS) = s;
    surface->cached |= CBIT(RMS);

    return s;
}

/**
 * SECTION: surface-statistics
 * @section_id: GwySurface-statistics
 * @title: GwySurface statistics
 * @short_description: Statistical characteristics of surfaces
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
