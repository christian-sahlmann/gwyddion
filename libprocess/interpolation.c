/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/interpolation.h>

enum { SUPPORT_LENGTH_MAX = 4 };

static const gdouble synth_func_values_bspline3[] = {
    2.0/3.0, 1.0/6.0,
};

static const gdouble synth_func_values_omoms3[] = {
    13.0/21.0, 4.0/21.0,
};

static inline void
gwy_interpolation_get_weights(gdouble x,
                              GwyInterpolationType interpolation,
                              gdouble *w)
{
    g_return_if_fail(x >= 0.0 && x <= 1.0);

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        /* Don't really care. */
        break;

        /* Silently use first order B-spline instead of NN for symmetry */
        case GWY_INTERPOLATION_ROUND:
        if (x < 0.5) {
            w[0] = 1.0;
            w[1] = 0.0;
        }
        else if (x > 0.5) {
            w[0] = 0.0;
            w[1] = 1.0;
        }
        else
            w[0] = w[1] = 0.5;
        break;

        case GWY_INTERPOLATION_LINEAR:
        w[0] = 1.0 - x;
        w[1] = x;
        break;

        case GWY_INTERPOLATION_KEY:
        w[0] = (-0.5 + (1.0 - x/2.0)*x)*x;
        w[1] = 1.0 + (-2.5 + 1.5*x)*x*x;
        w[2] = (0.5 + (2.0 - 1.5*x)*x)*x;
        w[3] = (-0.5 + x/2.0)*x*x;
        break;

        case GWY_INTERPOLATION_BSPLINE:
        w[0] = (1.0 - x)*(1.0 - x)*(1.0 - x)/6.0;
        w[1] = 2.0/3.0 - x*x*(1.0 - x/2.0);
        w[2] = (1.0/3.0 + x*(1.0 + x*(1.0 - x)))/2.0;
        w[3] = x*x*x/6.0;
        break;

        case GWY_INTERPOLATION_OMOMS:
        w[0] = 4.0/21.0 + (-11.0/21.0 + (0.5 - x/6.0)*x)*x;
        w[1] = 13.0/21.0 + (1.0/14.0 + (-1.0 + x/2.0)*x)*x;
        w[2] = 4.0/21.0 + (3.0/7.0 + (0.5 - x/2.0)*x)*x;
        w[3] = (1.0/42.0 + x*x/6.0)*x;
        break;

        case GWY_INTERPOLATION_NNA:
        if (x == 0.0) {
            w[0] = w[2] = w[3] = 0.0;
            w[1] = 1.0;
        }
        else if (x == 1.0) {
            w[0] = w[1] = w[3] = 0.0;
            w[2] = 1.0;
        }
        else {
            w[0] = x + 1.0;
            w[1] = x;
            w[2] = 1.0 - x;
            w[3] = 2.0 - x;
            w[0] = 1.0/(w[0]*w[0]);
            w[0] *= w[0];
            w[1] = 1.0/(w[1]*w[1]);
            w[1] *= w[1];
            w[2] = 1.0/(w[2]*w[2]);
            w[2] *= w[2];
            w[3] = 1.0/(w[3]*w[3]);
            w[3] *= w[3];
            x = w[0] + w[1] + w[2] + w[3];
            w[0] /= x;
            w[1] /= x;
            w[2] /= x;
            w[3] /= x;
        }
        break;

        case GWY_INTERPOLATION_SCHAUM:
        w[0] = -x*(x - 1.0)*(x - 2.0)/6.0;
        w[1] = (x*x - 1.0)*(x - 2.0)/2.0;
        w[2] = -x*(x + 1.0)*(x - 2.0)/2.0;
        w[3] = x*(x*x - 1.0)/6.0;
        break;

        default:
        g_return_if_reached();
        break;
    }
}
/**
 * gwy_interpolation_get_dval:
 * @x: requested value coordinate
 * @x1_: x coordinate of first value
 * @y1_: y coordinate of first value
 * @x2_: x coordinate of second value
 * @y2_: y coordinate of second value
 * @interpolation: interpolation type
 *
 * This function uses two-point interpolation
 * methods to get interpolated value between
 * two arbitrary data points.
 *
 * Returns: interpolated value
 **/
gdouble
gwy_interpolation_get_dval(gdouble x,
                           gdouble x1_, gdouble y1_,
                           gdouble x2_, gdouble y2_,
                           GwyInterpolationType interpolation)
{
    if (x1_ > x2_) {
        GWY_SWAP(gdouble, x1_, x2_);
        GWY_SWAP(gdouble, y1_, y2_);
    }

    switch (interpolation) {
        case GWY_INTERPOLATION_ROUND:
        if ((x - x1_) < (x2_ - x))
            return y1_;
        else
            return y2_;
        break;


        case GWY_INTERPOLATION_LINEAR:
        return y1_ + (x - x1_)/(x2_ - x1_)*(y2_ - y1_);
        break;

        default:
        g_warning("Interpolation not implemented yet.\n");
        break;
    }

    return 0.0;
}

/**
 * gwy_interpolation_get_dval_of_equidists:
 * @x: Possibily noninteger position in @data to get value at.
 * @data: Array of 4 values to interpolate between (see below).
 * @interpolation: Interpolation type to use.
 *
 * Computes interpolated value from 2 or 4 equidistant values.
 *
 * For %GWY_INTERPOLATION_NONE no @data value is actually used, and zero is
 * returned.
 *
 * For %GWY_INTERPOLATION_ROUND or %GWY_INTERPOLATION_LINEAR
 * it is enough to set middle two @data values, that to use @data in format
 * {0, data[i], data[i+1], 0} and function computes value at data[i+x]
 * (the outer values are not used).
 *
 * For four value interpolations you have to prepare @data as
 * {data[i-1], data[i], data[i+1], data[i+2]} and function again
 * returns value at data[i+x].
 *
 * Interpolation with non-interpolating bases are silently replaced with an
 * interpolating function with the same support size.  See
 * gwy_interpolation_interpolate_1d() for a function interpolating from
 * interpolation coefficients.
 *
 * Returns: Interpolated value.
 **/
gdouble
gwy_interpolation_get_dval_of_equidists(gdouble x,
                                        gdouble *data,
                                        GwyInterpolationType interpolation)
{
    gint l;
    gdouble w[SUPPORT_LENGTH_MAX];
    gdouble rest;

    x += 1.0;
    l = floor(x);
    rest = x - (gdouble)l;

    g_return_val_if_fail(x >= 1 && x < 2, 0.0);

    if (rest == 0)
        return data[l];

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0.0;
        break;

        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        gwy_interpolation_get_weights(rest, interpolation, w);
        return w[0]*data[l] + w[1]*data[l + 1];
        break;

        /* One cannot do B-spline and o-MOMS this way.  Read e.g.
         * `Interpolation Revisited' by Philippe Thevenaz for explanation.
         * Replace them with Key. */
        case GWY_INTERPOLATION_BSPLINE:
        case GWY_INTERPOLATION_OMOMS:
        interpolation = GWY_INTERPOLATION_KEY;
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        gwy_interpolation_get_weights(rest, interpolation, w);
        return w[0]*data[l - 1] + w[1]*data[l]
               + w[2]*data[l + 1] + w[3]*data[l + 2];
        break;

        default:
        g_return_val_if_reached(0.0);
        break;
    }
}

/**
 * gwy_interpolation_interpolate_1d:
 * @x: Position in interval [0,1) to get value at.
 * @coeff: Array of support-length size with interpolation coefficients
 *         (that are equal to data values for an interpolating basis).
 * @interpolation: Interpolation type to use.
 *
 * Interpolates a signle data point in one dimension.
 *
 * The interpolation basis support size can be obtained generically with
 * gwy_interpolation_get_support_size().
 *
 * Returns: Interpolated value.
 *
 * Since: 2.2
 **/
gdouble
gwy_interpolation_interpolate_1d(gdouble x,
                                 const gdouble *coeff,
                                 GwyInterpolationType interpolation)
{
    gdouble w[SUPPORT_LENGTH_MAX];
    gint i, suplen;
    gdouble v;

    g_return_val_if_fail(x >= 0.0 && x <= 1.0, 0.0);
    suplen = gwy_interpolation_get_support_size(interpolation);
    if (G_UNLIKELY(suplen == 0))
        return 0.0;
    g_return_val_if_fail(suplen > 0, 0.0);
    gwy_interpolation_get_weights(x, interpolation, w);

    v = 0.0;
    for (i = 0; i < suplen; i++)
        v += w[i]*coeff[i];

    return v;
}

/**
 * gwy_interpolation_interpolate_2d:
 * @x: X-position in interval [0,1) to get value at.
 * @y: Y-position in interval [0,1) to get value at.
 * @rowstride: Row stride of @coeff.
 * @coeff: Array of support-length-squared size with interpolation coefficients
 *         (that are equal to data values for an interpolating basis).
 * @interpolation: Interpolation type to use.
 *
 * Interpolates a signle data point in two dimensions.
 *
 * Returns: Interpolated value.
 *
 * Since: 2.2
 **/
gdouble
gwy_interpolation_interpolate_2d(gdouble x,
                                 gdouble y,
                                 gint rowstride,
                                 const gdouble *coeff,
                                 GwyInterpolationType interpolation)
{
    gdouble wx[SUPPORT_LENGTH_MAX], wy[SUPPORT_LENGTH_MAX];
    gint i, j, suplen;
    gdouble v, vx;

    g_return_val_if_fail(x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0, 0.0);
    suplen = gwy_interpolation_get_support_size(interpolation);
    if (G_UNLIKELY(suplen == 0))
        return 0.0;
    g_return_val_if_fail(suplen > 0, 0.0);
    gwy_interpolation_get_weights(x, interpolation, wx);
    gwy_interpolation_get_weights(y, interpolation, wy);

    v = 0.0;
    for (i = 0; i < suplen; i++) {
        vx = 0.0;
        for (j = 0; j < suplen; j++)
            vx += coeff[i*rowstride + j]*wx[j];
        v += wy[i]*vx;
    }

    return v;
}

/**
 * deconvolve3_rows:
 * @width: Number of items in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including width).
 * @data: An array to deconvolve of size @width.
 * @buffer: Scratch space of at least @width items.
 * @a: Central convolution filter element.
 * @b: Side convolution filter element.
 *
 * Undoes the effect of mirror-extended with border value repeated (@b, @a, @b)
 * horizontal convolution filter on a two-dimensional array.  It can be also
 * used for one-dimensional arrays, pass @height=1, @rowstride=@width then.
 *
 * This function acts on a two-dimensional data array, accessing it at linearly
 * as possible for CPU cache utilization reasons.
 **/
static void
deconvolve3_rows(gint width,
                 gint height,
                 gint rowstride,
                 gdouble *data,
                 gdouble *buffer,
                 gdouble a,
                 gdouble b)
{
    gdouble *row;
    gdouble q;
    gint i, j;

    g_return_if_fail(height < 2 || rowstride >= width);
    g_return_if_fail(2.0*b < a);

    if (!height || !width)
        return;

    if (width == 1) {
        q = a + 2.0*b;
        for (i = 0; i < height; i++)
            data[i*rowstride] /= q;
        return;
    }
    if (width == 2) {
        q = a*(a + 2.0*b);
        for (i = 0; i < height; i++) {
            row = data + i*rowstride;
            buffer[0] = (a + b)/q*row[0] - b/q*row[1];
            row[1] = (a + b)/q*row[1] - b/q*row[0];
            row[0] = buffer[0];
        }
        return;
    }

    /* Special-case first item */
    buffer[0] = a + b;
    /* Inner items */
    for (j = 1; j < width-1; j++) {
        q = b/buffer[j-1];
        buffer[j] = a - q*b;
        data[j] -= q*data[j-1];
    }
    /* Special-case last item */
    q = b/buffer[j-1];
    buffer[j] = a + b*(1.0 - q);
    data[j] -= q*data[j-1];
    /* Go back */
    data[j] /= buffer[j];
    do {
        j--;
        data[j] = (data[j] - b*data[j+1])/buffer[j];
    } while (j > 0);

    /* Remaining rows */
    for (i = 1; i < height; i++) {
        row = data + i*rowstride;
        /* Forward */
        for (j = 1; j < width-1; j++)
            row[j] -= b*row[j-1]/buffer[j-1];
        row[j] -= b*row[j-1]/buffer[j-1];
        /* Back */
        row[j] /= buffer[j];
        do {
            j--;
            row[j] = (row[j] - b*row[j+1])/buffer[j];
        } while (j > 0);
    }
}

/**
 * deconvolve3_columns:
 * @width: Number of columns in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including width).
 * @data: A two-dimensional array of size @width*height to deconvolve.
 * @buffer: Scratch space of at least @height items.
 * @a: Central convolution filter element.
 * @b: Side convolution filter element.
 *
 * Undoes the effect of mirror-extended with border value repeated (@b, @a, @b)
 * vertical convolution filter on a two-dimensional array.
 *
 * This function acts on a two-dimensional data array, accessing it at linearly
 * as possible for CPU cache utilization reasons.
 **/
static void
deconvolve3_columns(gint width,
                    gint height,
                    gint rowstride,
                    gdouble *data,
                    gdouble *buffer,
                    gdouble a,
                    gdouble b)
{
    gdouble *row;
    gdouble q;
    gint i, j;

    g_return_if_fail(height < 2 || rowstride >= width);
    g_return_if_fail(2.0*b < a);

    if (!height || !width)
        return;

    if (height == 1) {
        q = a + 2.0*b;
        for (j = 0; j < width; j++)
            data[j] /= q;
        return;
    }
    if (height == 2) {
        q = a*(a + 2.0*b);
        for (j = 0; j < width; j++) {
            buffer[0] = (a + b)/q*data[j] - b/q*data[rowstride + j];
            data[rowstride + j] = (a + b)/q*data[rowstride + j] - b/q*data[j];
            data[j] = buffer[0];
        }
        return;
    }

    /* Special-case first row */
    buffer[0] = a + b;
    /* Inner rows */
    for (i = 1; i < height-1; i++) {
        q = b/buffer[i-1];
        buffer[i] = a - q*b;
        row = data + (i - 1)*rowstride;
        for (j = 0; j < width; j++)
            row[rowstride + j] -= q*row[j];
    }
    /* Special-case last row */
    q = b/buffer[i-1];
    buffer[i] = a + b*(1.0 - q);
    row = data + (i - 1)*rowstride;
    for (j = 0; j < width; j++)
        row[rowstride + j] -= q*row[j];
    /* Go back */
    row += rowstride;
    for (j = 0; j < width; j++)
        row[j] /= buffer[i];
    do {
        i--;
        row = data + i*rowstride;
        for (j = 0; j < width; j++)
            row[j] = (row[j] - b*row[rowstride + j])/buffer[i];
    } while (i > 0);
}

/**
 * gwy_interpolation_has_interpolating_basis:
 * @interpolation: Interpolation type.
 *
 * Obtains the interpolating basis property of an inteprolation type.
 *
 * Interpolation types with inteprolating basis directly use data values
 * for interpolation.  For these types gwy_interpolation_resolve_coeffs_1d()
 * and gwy_interpolation_resolve_coeffs_2d() are no-op.
 *
 * Generalized interpolation types (with non-interpolation basis) require to
 * preprocess the data values to obtain interpolation coefficients first.  On
 * the ohter hand they typically offer much higher interpolation quality.
 *
 * Returns: %TRUE if the inteprolation type has interpolating basis,
 *          %FALSE if data values cannot be directly used for interpolation
 *          of this type.
 *
 * Since: 2.2
 **/
gboolean
gwy_interpolation_has_interpolating_basis(GwyInterpolationType interpolation)
{
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        return TRUE;
        break;

        case GWY_INTERPOLATION_BSPLINE:
        case GWY_INTERPOLATION_OMOMS:
        return FALSE;
        break;

        default:
        g_return_val_if_reached(FALSE);
        break;
    }
}

/**
 * gwy_interpolation_get_support_size:
 * @interpolation: Interpolation type.
 *
 * Obtains the basis support size for an interpolation type.
 *
 * Returns: The length of the support interval of the interpolation basis.
 *
 * Since: 2.2
 **/
gint
gwy_interpolation_get_support_size(GwyInterpolationType interpolation)
{
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0;
        break;

        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        return 2;
        break;

        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_BSPLINE:
        case GWY_INTERPOLATION_OMOMS:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        return 4;
        break;

        default:
        g_return_val_if_reached(-1);
        break;
    }
}

/**
 * gwy_interpolation_resolve_coeffs_1d:
 * @n: Number of points in @data.
 * @data: An array of data values.  It will be rewritten with the coefficients.
 * @interpolation: Interpolation type to prepare @data for.
 *
 * Transforms data values in a one-dimensional array to interpolation
 * coefficients.
 *
 * This function is no-op for interpolation types with finite-support
 * interpolating function.  Therefore you can also omit it and use the data
 * array directly for these interpolation types.
 *
 * Since: 2.2
 **/
void
gwy_interpolation_resolve_coeffs_1d(gint n,
                                    gdouble *data,
                                    GwyInterpolationType interpolation)
{
    gdouble *buffer;
    const gdouble *ab;

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        return;

        case GWY_INTERPOLATION_BSPLINE:
        ab = synth_func_values_bspline3;
        break;

        case GWY_INTERPOLATION_OMOMS:
        ab = synth_func_values_omoms3;
        break;

        default:
        g_return_if_reached();
        break;
    }

    buffer = g_new(gdouble, n);
    deconvolve3_rows(n, 1, n, data, buffer, ab[0], ab[1]);
    g_free(buffer);
}

/**
 * gwy_interpolation_resolve_coeffs_2d:
 * @width: Number of columns in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including @width).
 * @data: An array of data values.  It will be rewritten with the coefficients.
 * @interpolation: Interpolation type to prepare @data for.
 *
 * Transforms data values in a two-dimensional array to interpolation
 * coefficients.
 *
 * This function is no-op for interpolation types with finite-support
 * interpolating function.  Therefore you can also omit it and use the data
 * array directly for these interpolation types.
 *
 * Since: 2.2
 **/
void
gwy_interpolation_resolve_coeffs_2d(gint width,
                                    gint height,
                                    gint rowstride,
                                    gdouble *data,
                                    GwyInterpolationType interpolation)
{
    gdouble *buffer;
    const gdouble *ab;

    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        case GWY_INTERPOLATION_ROUND:
        case GWY_INTERPOLATION_LINEAR:
        case GWY_INTERPOLATION_KEY:
        case GWY_INTERPOLATION_NNA:
        case GWY_INTERPOLATION_SCHAUM:
        return;

        case GWY_INTERPOLATION_BSPLINE:
        ab = synth_func_values_bspline3;
        break;

        case GWY_INTERPOLATION_OMOMS:
        ab = synth_func_values_omoms3;
        break;

        default:
        g_return_if_reached();
        break;
    }

    buffer = g_new(gdouble, MAX(width, height));
    deconvolve3_rows(width, height, rowstride, data, buffer, ab[0], ab[1]);
    deconvolve3_columns(width, height, rowstride, data, buffer, ab[0], ab[1]);
    g_free(buffer);
}

/**
 * gwy_interpolation_resample_block_1d:
 * @length: Data block length.
 * @data: Data block to resample.
 * @newlength: Requested length after resampling.
 * @newdata: Array to put the resampled data to.
 * @interpolation: Interpolation type to use.
 * @preserve: %TRUE to preserve the content of @data, %FALSE to permit its
 *            overwriting with temporary data.
 *
 * Resamples a one-dimensional data array.
 *
 * This is a primitive operation, in most cases methods such as
 * gwy_data_line_new_resampled() provide more convenient interface.
 *
 * Since: 2.2
 **/
void
gwy_interpolation_resample_block_1d(gint length,
                                    gdouble *data,
                                    gint newlength,
                                    gdouble *newdata,
                                    GwyInterpolationType interpolation,
                                    gboolean preserve)
{
    gdouble *w, *coeffs = NULL;
    gdouble q, x0, x, v;
    gint i, ii, oldi, newi, suplen;
    gint sf, st;

    if (interpolation == GWY_INTERPOLATION_NONE)
        return;

    suplen = gwy_interpolation_get_support_size(interpolation);
    g_return_if_fail(suplen > 0);
    w = g_newa(gdouble, suplen);
    sf = -((suplen - 1)/2);
    st = suplen/2;

    if (!gwy_interpolation_has_interpolating_basis(interpolation)) {
        if (preserve)
            data = coeffs = g_memdup(data, length*sizeof(gdouble));
        gwy_interpolation_resolve_coeffs_1d(length, data, interpolation);
    }

    q = (gdouble)length/newlength;
    x0 = (q - 1.0)/2.0;
    for (newi = 0; newi < newlength; newi++) {
        x = q*newi + x0;
        oldi = (gint)floor(x);
        x -= oldi;
        gwy_interpolation_get_weights(x, interpolation, w);
        v = 0.0;
        for (i = sf; i <= st; i++) {
            ii = (oldi + i + 2*st*length) % (2*length);
            if (G_UNLIKELY(ii >= length))
                ii = 2*length-1 - ii;
            v += data[ii]*w[i - sf];
        }
        newdata[newi] = v;
    }

    g_free(coeffs);
}

static void
calculate_weights_for_rescale(gint oldn,
                              gint newn,
                              gint *positions,
                              gdouble *weights,
                              GwyInterpolationType interpolation)
{
    gint i, suplen;
    gdouble q, x0, x;

    suplen = gwy_interpolation_get_support_size(interpolation);
    q = (gdouble)oldn/newn;
    x0 = (q - 1.0)/2.0;
    for (i = 0; i < newn; i++) {
        x = q*i + x0;
        positions[i] = (gint)floor(x);
        x -= positions[i];
        gwy_interpolation_get_weights(x, interpolation, weights + suplen*i);
    }
}

/**
 * gwy_interpolation_resample_block_2d:
 * @width: Number of columns in @data.
 * @height: Number of rows in @data.
 * @rowstride: Total row length (including @width).
 * @data: Data block to resample.
 * @newwidth: Requested number of columns after resampling.
 * @newheight: Requested number of rows after resampling.
 * @newrowstride: Requested total row length after resampling (including
 *                @newwidth).
 * @newdata: Array to put the resampled data to.
 * @interpolation: Interpolation type to use.
 * @preserve: %TRUE to preserve the content of @data, %FALSE to permit its
 *            overwriting with temporary data.
 *
 * Resamples a two-dimensional data array.
 *
 * This is a primitive operation, in most cases methods such as
 * gwy_data_filed_new_resampled() provide more convenient interface.
 *
 * Since: 2.2
 **/
void
gwy_interpolation_resample_block_2d(gint width,
                                    gint height,
                                    gint rowstride,
                                    gdouble *data,
                                    gint newwidth,
                                    gint newheight,
                                    gint newrowstride,
                                    gdouble *newdata,
                                    GwyInterpolationType interpolation,
                                    gboolean preserve)
{
    gdouble *xw, *yw, *coeffs = NULL;
    gint *xp, *yp;
    gdouble v, vx;
    gint i, ii, oldi, newi, j, jj, oldj, newj, suplen;
    gint sf, st;

    if (interpolation == GWY_INTERPOLATION_NONE)
        return;

    suplen = gwy_interpolation_get_support_size(interpolation);
    g_return_if_fail(suplen > 0);
    sf = -((suplen - 1)/2);
    st = suplen/2;

    if (!gwy_interpolation_has_interpolating_basis(interpolation)) {
        if (preserve) {
            if (rowstride == width)
                data = coeffs = g_memdup(data, width*height*sizeof(gdouble));
            else {
                coeffs = g_new(gdouble, width*height);
                for (i = 0; i < height; i++) {
                    memcpy(coeffs + i*width,
                           data + i*rowstride,
                           width*sizeof(gdouble));
                }
                data = coeffs;
                rowstride = width;
            }
        }
        gwy_interpolation_resolve_coeffs_2d(width, height, rowstride,
                                            data, interpolation);
    }

    xw = g_new(gdouble, suplen*newwidth);
    yw = g_new(gdouble, suplen*newheight);
    xp = g_new(gint, newwidth);
    yp = g_new(gint, newheight);
    calculate_weights_for_rescale(width, newwidth, xp, xw, interpolation);
    calculate_weights_for_rescale(height, newheight, yp, yw, interpolation);
    for (newi = 0; newi < newheight; newi++) {
        oldi = yp[newi];
        for (newj = 0; newj < newwidth; newj++) {
            oldj = xp[newj];
            v = 0.0;
            for (i = sf; i <= st; i++) {
                ii = (oldi + i + 2*st*height) % (2*height);
                if (G_UNLIKELY(ii >= height))
                    ii = 2*height-1 - ii;
                vx = 0.0;
                for (j = sf; j <= st; j++) {
                    jj = (oldj + j + 2*st*width) % (2*width);
                    if (G_UNLIKELY(jj >= width))
                        jj = 2*width-1 - jj;
                    vx += data[ii*rowstride + jj]*xw[newj*suplen + j - sf];
                }
                v += vx*yw[newi*suplen + i - sf];
            }
            newdata[newi*newrowstride + newj] = v;
        }
    }
    g_free(yp);
    g_free(xp);
    g_free(yw);
    g_free(xw);

    g_free(coeffs);
}

/************************** Documentation ****************************/

/**
 * SECTION:interpolation
 * @title: interpolation
 * @short_description: General interpolation functions
 *
 * Data interpolation is usually pixel-like in Gwyddion, not function-like.
 * That means the contribution of individual data saples is preserved on
 * scaling (the area that <quote>belongs</quote> to all values is the same,
 * it is not reduced to half for edge pixels).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
