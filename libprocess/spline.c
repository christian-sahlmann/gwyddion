/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/spline.h>

#define PointXY GwyTriangulationPointXY

typedef enum {
    CURVE_RECURSE_OUTPUT_X_Y,
    CURVE_RECURSE_OUTPUT_T_L,
} GwySplineRecurseOutputType;

typedef struct {
    gdouble ux;
    gdouble uy;
    gdouble vx;
    gdouble vy;
} ControlPoint;

struct _GwySpline {
    /* Properties set from outside. */
    GArray *points;
    gdouble slackness;
    gboolean closed;

    /* Cached data.  These change whenever anything above changes.  */
    gboolean natural_sampling_valid;
    GArray *control_points;
    GArray *natural_points;
    gdouble length;

    /* These cache the last result of gwy_spline_sample() and become invalid
     * whenever anything above changes or gwy_spline_sample() is called for
     * a different number of points.  */
    gboolean fixed_sampling_valid;
    guint nfixed;
    GArray *fixed_samples;
};

static void gwy_spline_invalidate(GwySpline *spline);

/**
 * gwy_spline_new:
 *
 * Creates a new empty spline curve.
 *
 * You need to set the curve points using gwy_spline_set_points() before any
 * sampling along the curve.  Alternatively, use gwy_spline_from_points()
 * to construct the spline already with some points.
 *
 * Returns: A newly created spline curve.
 *
 * Since: 2.45
 **/
GwySpline*
gwy_spline_new(void)
{
    GwySpline *spline = g_slice_new0(GwySpline);

    spline->points = g_array_new(FALSE, FALSE, sizeof(PointXY));
    spline->slackness = 1.0;
    spline->control_points = g_array_new(FALSE, FALSE, sizeof(ControlPoint));
    spline->natural_points = g_array_new(FALSE, FALSE, sizeof(PointXY));
    spline->fixed_samples = g_array_new(FALSE, FALSE, sizeof(PointXY));

    return spline;
}

/**
 * gwy_spline_free:
 * @spline: A spline curve.
 *
 * Frees a spline curve and all associated resources.
 *
 * Since: 2.45
 **/
void
gwy_spline_free(GwySpline *spline)
{
    g_return_if_fail(spline);
    g_array_free(spline->fixed_samples, TRUE);
    g_array_free(spline->natural_points, TRUE);
    g_array_free(spline->control_points, TRUE);
    g_array_free(spline->points, TRUE);
    g_slice_free(GwySpline, spline);
}

/**
 * gwy_spline_from_points:
 * @xy: Array of points in plane the curve will pass through.
 * @n: Number of points in @xy[].
 *
 * Creates a new spline curve passing through given points.
 *
 * Returns: A newly created spline curve.
 *
 * Since: 2.45
 **/
GwySpline*
gwy_spline_from_points(const PointXY *xy,
                       guint n)
{
    GwySpline *spline = gwy_spline_new();
    gwy_spline_set_points(spline, xy, n);
    return spline;
}

/**
 * gwy_spline_get_npoints:
 * @spline: A spline curve.
 *
 * Gets the number of points of a spline curve.
 *
 * Returns: The number of XY points defining the curve.
 *
 * Since: 2.45
 **/
guint
gwy_spline_get_npoints(GwySpline *spline)
{
    return spline->points->len;
}

/**
 * gwy_spline_get_points:
 * @spline: A spline curve.
 *
 * Gets the coordinates of spline curve points.
 *
 * Returns: Coordinates of the XY points defining the curve.  The returned
 *          array is owned by @spline, must not be modified and is only
 *          guaranteed to exist so long as the spline is not modified nor
 *          destroyed.
 *
 * Since: 2.45
 **/
const PointXY*
gwy_spline_get_points(GwySpline *spline)
{
    return (PointXY*)spline->points->data;
}

/**
 * gwy_spline_get_slackness:
 * @spline: A spline curve.
 *
 * Gets the slackness parameter of a spline curve.
 *
 * See gwy_spline_set_slackness() for discussion.
 *
 * Returns: The slackness parameter value.
 *
 * Since: 2.45
 **/
gdouble
gwy_spline_get_slackness(GwySpline *spline)
{
    return spline->slackness;
}

/**
 * gwy_spline_get_closed:
 * @spline: A spline curve.
 *
 * Reports whether a spline curve is closed or not.
 *
 * See gwy_spline_set_closed() for discussion.
 *
 * Returns: %TRUE if @spline is closed, %FALSE if it is open-ended.
 *
 * Since: 2.45
 **/
gboolean
gwy_spline_get_closed(GwySpline *spline)
{
    return spline->closed;
}

/**
 * gwy_spline_set_points:
 * @xy: Array of points in plane the curve will pass through.
 * @n: Number of points in @xy[].
 *
 * Sets the coordinates of XY points a spline curve should pass through.
 *
 * It is possible to pass @n=0 to make the spline empty (@xy can be NULL then)
 * but such spline may not be sampled.
 *
 * Since: 2.45
 **/
void
gwy_spline_set_points(GwySpline *spline,
                      const PointXY *xy,
                      guint n)
{
    GArray *points = spline->points;

    if (points->len == n
        && memcmp(xy, points->data, n*sizeof(PointXY)) == 0)
        return;

    g_array_set_size(spline->points, 0);
    g_array_append_vals(spline->points, xy, n);
    gwy_spline_invalidate(spline);
}

/**
 * gwy_spline_set_slackness:
 * @spline: A spline curve.
 * @slackness: New slackness parameter value from the range [0, 1].
 *
 * Sets the slackness parameter of a spline curve.
 *
 * The slackness parameter determines how taut or slack the curve is.
 *
 * The curve always passes through the given XY points.  For zero slackness
 * the curve is maximally taut, i.e. the shortest possible passing
 * through the points.  Such curve is formed by straight segments.  For
 * slackness of 1 the curve is a ‘free’ spline.  This is also the default.
 *
 * Since: 2.45
 **/
void
gwy_spline_set_slackness(GwySpline *spline,
                         gdouble slackness)
{
    if (spline->slackness == slackness)
        return;

    /* XXX: We may permit slackness > 1 for some interesting and possibly still
     * useful curves.  Up to approximately sqrt(2) seems reasonable. */
    if (!(slackness >= 0.0 && slackness <= 1.0)) {
        g_warning("Slackness parameter %g is out of bounds.", slackness);
        return;
    }
    spline->slackness = slackness;
    gwy_spline_invalidate(spline);
}

/**
 * gwy_spline_set_closed:
 * @spline: A spline curve.
 * @closed: %TRUE to make @spline closed, %FALSE to make it open-ended.
 *
 * Sets whether a spline curve is closed or open.
 *
 * In closed curve the last point is connected smoothly with the first point,
 * forming a cycle.  Note you should not repeat the point in the @xy array.
 * When a closed curve is sampled, the sampling starts from the first point
 * and continues beyond the last point until it gets close to the first point
 * again.
 *
 * An open curve begins with the first point and ends with the last point.  It
 * has zero curvature at these two points.
 *
 * Since: 2.45
 **/
void
gwy_spline_set_closed(GwySpline *spline,
                      gboolean closed)
{
    if (!spline->closed == !closed)
        return;

    spline->closed = !!closed;
    gwy_spline_invalidate(spline);
}

/**
 * gwy_spline_length:
 * @spline: A spline curve.
 *
 * Calculates the length of a spline curve.
 *
 * This is useful when you want to sample the curve with a specific step
 * (at least approximately).
 *
 * Note gwy_spline_sample() also returns the length.
 *
 * Returns: The curve length in whatever units the XY coordinates are expressed
 *          in.
 *
 * Since: 2.45
 **/
gdouble
gwy_spline_length(GwySpline *spline)
{
    if (spline->natural_sampling_valid)
        return spline->length;

    /* TODO */
    /* Here we set natural_sampling_valid, but not fixed_sampling_valid. */
    return 0.0;
}

/**
 * gwy_spline_sample:
 * @spline: A spline curve.
 * @xy: Array where the sampled point coordinates should be stored in.
 * @n: The number of samples to take.
 *
 * Samples uniformly a spline curve.
 *
 * This function calculates coordinates of points that lie on the spline curve
 * and are equidistant along it.  For open curves the first sampled point
 * coincides with the first given XY point and, similar, the last with the last.
 * For closed curves the first point again coincides with the first given XY
 * point but the last lies one sampling distance before the curve gets back
 * again to the first point.
 *
 * If you want to specify the sampling step instead of the number of samples
 * use gwy_spline_length() first to obtain the curve length and calculate @n
 * accordingly.
 *
 * A single-point curve always consists of a single point.  Hence all samples
 * lie in this point.  A two-point curve is always formed by straight segments,
 * in the case of a closed curve one going forward and the other back.  A
 * meaningful sampling requires @n at least 2, nevertheless, the function
 * permits also @n of one or zero.
 *
 * Returns: The curve length in whatever units the XY coordinates are expressed
 *          in.
 *
 * Since: 2.45
 **/
gdouble
gwy_spline_sample(GwySpline *spline,
                  GwyTriangulationPointXY *xy,
                  guint n)
{
    if (spline->fixed_sampling_valid && spline->nfixed == n) {
        memcpy(xy, spline->fixed_samples->data, n*sizeof(PointXY));
        return spline->length;
    }

    /* TODO */
    /* Here we ensure natural_sampling_valid, and set fixed_sampling_valid. */
    return 0.0;
}

static void
gwy_spline_invalidate(GwySpline *spline)
{
    spline->natural_sampling_valid = FALSE;
    spline->fixed_sampling_valid = FALSE;
}

/************************** Documentation ****************************/

/**
 * SECTION:spline
 * @title: GwySpline
 * @short_description: Sampling curves in plane
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
