/* @(#) $Id$ */

#include <math.h>

#include <libgwyddion/gwymacros.h>
#include "gwymath.h"

#define ROUND(x) ((gint)floor((x) + 0.5))

/**
 * gwy_math_SI_prefix:
 * @magnitude: A power of 1000.
 *
 * Finds SI prefix corresponding to a given power of 1000.
 *
 * In fact, @magnitude doesn't have to be power of 1000, but then the result
 * is mostly meaningless.
 *
 * Returns: The SI unit prefix corresponding to @magnitude, "?" if @magnitude
 *          is outside of the SI prefix range.  The returned value must be
 *          considered constant and never modified or freed.
 **/
const gchar*
gwy_math_SI_prefix(gdouble magnitude)
{
    static const gchar *positive[] = {
        "", "k", "M", "G", "T", "P", "E", "Z", "Y"
    };
    static const gchar *negative[] = {
        "", "m", "µ", "n", "p", "f", "a", "z", "y"
    };
    static const gchar *unknown = "?";
    gint i;

    i = ROUND(log10(magnitude)/3.0);
    if (i >= 0 && i < (gint)G_N_ELEMENTS(positive))
        return positive[i];
    if (i <= 0 && -i < (gint)G_N_ELEMENTS(negative))
        return negative[-i];
    /* FIXME: the vertical ruler text placing routine can't reasonably
     * break things like 10<sup>-36</sup> to lines */
    g_warning("magnitude %g outside of prefix range.  FIXME!", magnitude);

    return unknown;
}

/**
 * gwy_math_humanize_numbers:
 * @unit: The smallest possible step.
 * @maximum: The maximum possible value.
 * @precision: A location to store printf() precession, if not %NULL.
 *
 * Find a human readable representation for a range of numbers.
 *
 * Returns: The magnitude i.e., a power of 1000.
 **/
gdouble
gwy_math_humanize_numbers(gdouble unit,
                          gdouble maximum,
                          gint *precision)
{
    gdouble lm, lu, mag, range, min;

    lm = log10(maximum);
    lu = log10(unit);
    mag = 3.0*floor((lm + lu)/6.0);
    if (precision) {
        range = lm - lu;
        if (range > 3.0)
            range = (range + 3.0)/2;
        min = lm - range;
        *precision = (min < mag) ? (gint)ceil(mag - min) : 0;
    }

    return exp(G_LN10*mag);
}

/**
 * gwy_math_find_nearest_line:
 * @x: X-coordinate of the point to search.
 * @y: Y-coordinate of the point to search.
 * @d2min: Where to store the squared minimal distance, or %NULL.
 * @n: The size of @coords, NOT the number of points or lines, must be
 *     a multiple of 4.
 * @coords: Line coordinates stored as x00, y00, x01, y01, x10, y10, etc.
 *
 * Find the line from @coords nearest to the point (@x, @y).
 *
 * Returns: The line number (i.e., the line starts at index equal to four
 *          times the return value). It may return -1 if (@x, @y) doesn't lie
 *          in the orthogonal stripe of any of the lines.
 **/
gint
gwy_math_find_nearest_line(gdouble x, gdouble y,
                           gdouble *d2min,
                           gint n, gdouble *coords)
{
    gint i, m;
    gdouble d2m = G_MAXDOUBLE;

    g_return_val_if_fail(n > 0, d2m);
    g_return_val_if_fail(coords, d2m);
    g_assert(n % 4 == 0);
    n >>= 2;
    n <<= 2;

    m = -1;
    for (i = 0; i < n; i++) {
        gdouble x0 = *(coords++);
        gdouble y0 = *(coords++);
        gdouble x1 = *(coords++);
        gdouble y1 = *(coords++);
        gdouble vx, vy, d;

        vx = y1 - y0;
        vy = x0 - x1;
        if (vx*(y - y0) < vy*(x - x0))
            continue;
        if (vx*(y1 - y) < vy*(x1 - x))
            continue;
        if (vx == 0.0 && vy == 0.0)
            continue;
        d = vx*(x - x0) + vy*(y - y0);
        d *= d/(vx*vx + vy*vy);
        if (d < d2m) {
            d2m = d;
            m = i;
        }
    }
    if (d2min)
      *d2min = d2m;

    return m;
}

/**
 * gwy_math_find_nearest_point:
 * @x: X-coordinate of the point to search.
 * @y: Y-coordinate of the point to search.
 * @d2min: Where to store the squared minimal distance, or %NULL.
 * @n: The size of @coords, NOT the number of points, must be
 *     a multiple of 2.
 * @coords: Point coordinates stored as x0, y0, x1, y1, x2, y2, etc.
 *
 * Find the point from @coords nearest to the point (@x, @y).
 *
 * Returns: The point number (i.e., the point starts at index equal to twice
 *          the return value).
 **/
gint
gwy_math_find_nearest_point(gdouble x, gdouble y,
                            gdouble *d2min,
                            gint n, gdouble *coords)
{
    gint i, m;
    gdouble d2m = G_MAXDOUBLE;

    g_return_val_if_fail(n > 0, d2m);
    g_return_val_if_fail(coords, d2m);
    g_assert(n % 2 == 0);
    n >>= 1;
    n <<= 1;

    m = 0;
    for (i = 0; i < n; i++) {
        gdouble xx = *(coords++);
        gdouble yy = *(coords++);
        gdouble d;

        d = (xx - x)*(xx - x) + (yy - y)*(yy - y);
        if (d < d2m) {
            d2m = d;
            m = i;
        }
    }
    if (d2min)
      *d2min = d2m;

    return m;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
