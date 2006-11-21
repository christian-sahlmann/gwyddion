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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/interpolation.h>

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


        case GWY_INTERPOLATION_BILINEAR:
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
 * @x: Noninteger part of requested x, that is a number from interval [0,1).
 * @data: Array of 4 values to interpolate between (see below).
 * @interpolation: Interpolation type to use.
 *
 * Computes interpolated value from 2 or 4 equidistant values.
 *
 * For %GWY_INTERPOLATION_NONE no @data value is actually used, and zero is
 * returned.
 *
 * For %GWY_INTERPOLATION_ROUND or %GWY_INTERPOLATION_BILINEAR
 * it is enough to set middle two @data values, that to use @data in format
 * {0, data[i], data[i+1], 0} and function computes value at data[i+x]
 * (the outer values are not used).
 *
 * For four value interpolations you have to prepare @data as
 * {data[i-1], data[i], data[i+1], data[i+2]} and function again
 * returns value at data[i+x].
 *
 * Returns: Interpolated value.
 **/
gdouble
gwy_interpolation_get_dval_of_equidists(gdouble x,
                                        gdouble *data,
                                        GwyInterpolationType interpolation)
{


    gint l;
    gdouble w1, w2, w3, w4;
    gdouble rest;

    x += 1.0;
    l = floor(x);
    rest = x - (gdouble)l;

    g_return_val_if_fail(x >= 1 && x < 2, 0.0);

    if (rest == 0)
        return data[l];

    /*simple (and fast) methods*/
    switch (interpolation) {
        case GWY_INTERPOLATION_NONE:
        return 0.0;

        case GWY_INTERPOLATION_ROUND:
        return data[(gint)(x + 0.5)];

        case GWY_INTERPOLATION_BILINEAR:
        return
            (1.0 - rest)*data[l] + rest*data[l+1];

        default:
        break;
    }

    switch (interpolation) {
        case GWY_INTERPOLATION_KEY:
        /* One cannot do B-spline and o-MOMS this way.  Read e.g.
         * `Interpolation Revisited' by Philippe Thevenaz for explanation.
         * Replace them with Key. */
        case GWY_INTERPOLATION_BSPLINE:
        case GWY_INTERPOLATION_OMOMS:
        w1 = (-0.5 + (1.0 - rest/2.0)*rest)*rest;
        w2 = 1.0 + (-2.5 + 1.5*rest)*rest*rest;
        w3 = (0.5 + (2.0 - 1.5*rest)*rest)*rest;
        w4 = (-0.5 + rest/2.0)*rest*rest;
        break;

        case GWY_INTERPOLATION_NNA:
        w1 = rest + 1.0;
        w2 = rest;
        w3 = 1.0 - rest;
        w4 = 2.0 - rest;
        w1 = 1/(w1*w1*w1*w1);
        w2 = 1/(w2*w2*w2*w2);
        w3 = 1/(w3*w3*w3*w3);
        w4 = 1/(w4*w4*w4*w4);
        return (w1*data[l-1] + w2*data[l]
                + w3*data[l+1] + w4*data[l+2])/(w1 + w2 + w3 + w4);

        case GWY_INTERPOLATION_SCHAUM:
        w1 = -rest*(rest - 1.0)*(rest - 2.0)/6.0;
        w2 = (rest*rest - 1.0)*(rest - 2.0)/2.0;
        w3 = -rest*(rest + 1.0)*(rest - 2.0)/2.0;
        w4 = rest*(rest*rest - 1.0)/6.0;
        break;

        default:
        g_assert_not_reached();
        w1 = w2 = w3 = w4 = 0.0;
        break;
    }

    return w1*data[l-1] + w2*data[l] + w3*data[l+1] + w4*data[l+2];
}

/**
 * deconvolve3_rows:
 * @width: The number of items in @data.
 * @height: The number of rows in @data.
 * @rowstride: The total row length (including width).
 * @data: An array to deconvolve of size @width.
 * @buffer: Scratch space of at least @width items.
 * @a: The central convolution filter element.
 * @b: The side convolution filter element.
 *
 * Undoes the effect of mirror-extended (@b, @a, @b) vertical convolution
 * filter on a two-dimensional array.  It can be also used for one-dimensional
 * arrays, pass @height=1, @rowstride=@width then.
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
    gdouble q, b2;
    gint i, j;

    g_return_if_fail(height < 2 || rowstride >= width);
    b2 = 2.0*b;
    g_return_if_fail(b2 < a);

    if (!height || !width)
        return;

    if (width == 1) {
        for (i = 0; i < height; i++)
            data[i*rowstride] /= (a + b2);
        return;
    }
    if (width == 2) {
        q = a*a - b2*b2;
        for (i = 0; i < height; i++) {
            row = data + i*rowstride;
            buffer[0] = a*row[0] - b2*row[1];
            row[1] = a*row[1] - b2*row[0];
            row[0] = buffer[0];
        }
        return;
    }

    /* Special-case first item */
    buffer[0] = a/2.0;
    data[0] /= 2.0;
    /* Inner items */
    for (j = 1; j < width-1; j++) {
        q = b/buffer[j-1];
        buffer[j] = a - q*b;
        data[j] -= q*data[j-1];
    }
    /* Special-case last item */
    q = b2/buffer[j-1];
    buffer[j] = a - q*b;
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
        row[0] /= 2.0;
        for (j = 1; j < width-1; j++)
            row[j] -= b*row[j-1]/buffer[j-1];
        row[j] -= b2*row[j-1]/buffer[j-1];
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
 * @width: The number of columns in @data.
 * @height: The number of rows in @data.
 * @rowstride: The total row length (including width).
 * @data: A two-dimensional array of size @width*height to deconvolve.
 * @buffer: Scratch space of at least @height items.
 * @a: The central convolution filter element.
 * @b: The side convolution filter element.
 *
 * Undoes the effect of mirror-extended (@b, @a, @b) vertical convolution
 * filter on a two-dimensional array.
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
    gdouble q, b2;
    gint i, j;

    g_return_if_fail(height < 2 || rowstride >= width);
    b2 = 2.0*b;
    g_return_if_fail(b2 < a);

    if (!height || !width)
        return;

    if (height == 1) {
        for (j = 0; j < width; j++)
            data[j] /= (a + b2);
        return;
    }
    if (height == 2) {
        q = a*a - b2*b2;
        for (j = 0; j < width; j++) {
            buffer[0] = a*data[j] - b2*data[rowstride + j];
            data[rowstride + j] = a*data[rowstride + j] - b2*data[j];
            data[j] = buffer[0];
        }
        return;
    }

    /* Special-case first row */
    buffer[0] = a/2.0;
    for (j = 0; j < width; j++)
        data[j] /= 2.0;
    /* Inner rows */
    for (i = 1; i < height-1; i++) {
        q = b/buffer[i-1];
        buffer[i] = a - q*b;
        row = data + (i - 1)*rowstride;
        for (j = 0; j < width; j++)
            row[rowstride + j] -= q*row[j];
    }
    /* Special-case last row */
    q = b2/buffer[i-1];
    buffer[i] = a - q*b;
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

/************************** Documentation ****************************/

/**
 * SECTION:interpolation
 * @title: interpolation
 * @short_description: General interpolation functions
 *
 * Data interpolation is usually pixel-like in Gwyddion, not function-like.
 * That means the contribution of individual data saples is preserved on
 * scaling.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
