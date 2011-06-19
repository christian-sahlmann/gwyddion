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
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include "gwyprocessinternal.h"

static gint thin_data_field(GwyDataField *data_field);

/**
 * gwy_data_field_normalize:
 * @data_field: A data field.
 *
 * Normalizes data in a data field to range 0.0 to 1.0.
 *
 * It is equivalent to gwy_data_field_renormalize(@data_field, 1.0, 0.0);
 *
 * If @data_field is filled with only one value, it is changed to 0.0.
 **/
void
gwy_data_field_normalize(GwyDataField *data_field)
{
    gdouble min, max;
    gdouble *p;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    gwy_data_field_get_min_max(data_field, &min, &max);
    if (min == max) {
        gwy_data_field_clear(data_field);
        return;
    }
    if (!min) {
        if (max != 1.0)
            gwy_data_field_multiply(data_field, 1.0/max);
        return;
    }

    /* The general case */
    max -= min;
    xres = data_field->xres;
    yres = data_field->yres;
    for (i = xres*yres, p = data_field->data; i; i--, p++)
        *p = (*p - min)/max;

    /* We can transform stats */
    data_field->cached &= CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                          | CBIT(MED) | CBIT(ARF) | CBIT(ART);
    CVAL(data_field, MIN) = 0.0;
    CVAL(data_field, MAX) = 1.0;
    CVAL(data_field, SUM) /= (CVAL(data_field, SUM) - xres*yres*min)/max;
    CVAL(data_field, RMS) /= max;
    CVAL(data_field, MED) = (CVAL(data_field, MED) - min)/max;
    CVAL(data_field, ART) = (CVAL(data_field, ART) - min)/max;
    CVAL(data_field, ARF) = (CVAL(data_field, ARF) - min)/max;
}

/**
 * gwy_data_field_renormalize:
 * @data_field: A data field.
 * @range: New data interval size.
 * @offset: New data interval offset.
 *
 * Transforms data in a data field with first linear function to given range.
 *
 * When @range is positive, the new data range is (@offset, @offset+@range);
 * when @range is negative, the new data range is (@offset-@range, @offset).
 * In neither case the data are flipped, negative range only means different
 * selection of boundaries.
 *
 * When @range is zero, this method is equivalent to
 * gwy_data_field_fill(@data_field, @offset).
 **/
void
gwy_data_field_renormalize(GwyDataField *data_field,
                           gdouble range,
                           gdouble offset)
{
    gdouble min, max, v;
    gdouble *p;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    if (!range) {
        gwy_data_field_fill(data_field, offset);
        return;
    }

    gwy_data_field_get_min_max(data_field, &min, &max);
    if (min == max) {
        gwy_data_field_fill(data_field, offset);
        return;
    }

    if ((range > 0 && min == offset && min + range == max)
        || (range < 0 && max == offset && min - range == max))
        return;

    /* The general case */
    xres = data_field->xres;
    yres = data_field->yres;

    if (range > 0) {
        max -= min;
        for (i = xres*yres, p = data_field->data; i; i--, p++)
            *p = (*p - min)/max*range + offset;

        /* We can transform stats */
        data_field->cached &= CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                              | CBIT(MED);
        CVAL(data_field, MIN) = offset;
        CVAL(data_field, MAX) = offset + range;
        v = CVAL(data_field, SUM);
        CVAL(data_field, SUM) = (v - xres*yres*min)/max*range
                                + offset*xres*yres;
        CVAL(data_field, RMS) = CVAL(data_field, RMS)/max*range;
        CVAL(data_field, MED) = (CVAL(data_field, MED) - min)/max*range
                                + offset;
        /* FIXME: we can recompute ARF and ART too */
    }
    else {
        min = max - min;
        for (i = xres*yres, p = data_field->data; i; i--, p++)
            *p = (max - *p)/min*range + offset;

        /* We can transform stats */
        data_field->cached &= CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                              | CBIT(MED);
        CVAL(data_field, MIN) = offset + range;
        CVAL(data_field, MAX) = offset;
        v = CVAL(data_field, SUM);
        CVAL(data_field, SUM) = (xres*yres*max - v)/min*range
                                + offset*xres*yres;
        CVAL(data_field, RMS) = CVAL(data_field, RMS)/min*(-range);
        CVAL(data_field, MED) = (max - CVAL(data_field, MED))/min*range
                                + offset;
        /* FIXME: we can recompute ARF and ART too */
    }
}

/**
 * gwy_data_field_threshold:
 * @data_field: A data field.
 * @threshval: Threshold value.
 * @bottom: Lower replacement value.
 * @top: Upper replacement value.
 *
 * Tresholds values of a data field.
 *
 * Values smaller than @threshold are set to value @bottom, values higher
 * than @threshold or equal to it are set to value @top
 *
 * Returns: The total number of values above threshold.
 **/
gint
gwy_data_field_threshold(GwyDataField *data_field,
                         gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, n, tot = 0;
    gdouble *p = data_field->data;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);

    n = data_field->xres * data_field->yres;
    for (i = n; i; i--, p++) {
        if (*p < threshval)
            *p = bottom;
        else {
            *p = top;
            tot++;
        }
    }

    /* We can precompute stats */
    data_field->cached = CBIT(MIN) | CBIT(MAX) | CBIT(SUM) | CBIT(RMS)
                         | CBIT(MED);
    CVAL(data_field, MIN) = MIN(top, bottom);
    CVAL(data_field, MAX) = MAX(top, bottom);
    CVAL(data_field, SUM) = tot*top + (n - tot)*bottom;
    CVAL(data_field, RMS) = (top - bottom)*(top - bottom)
                            * tot/(gdouble)n * (n - tot)/(gdouble)n;
    /* FIXME: may be incorrect for tot == n/2(?) */
    CVAL(data_field, MED) = tot > n/2 ? top : bottom;

    return tot;
}


/**
 * gwy_data_field_area_threshold:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @threshval: Threshold value.
 * @bottom: Lower replacement value.
 * @top: Upper replacement value.
 *
 * Tresholds values of a rectangular part of a data field.
 *
 * Values smaller than @threshold are set to value @bottom, values higher
 * than @threshold or equal to it are set to value @top
 *
 * Returns: The total number of values above threshold.
 **/
gint
gwy_data_field_area_threshold(GwyDataField *data_field,
                              gint col, gint row, gint width, gint height,
                              gdouble threshval, gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;
    gdouble *drow;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         0);

    for (i = 0; i < height; i++) {
        drow = data_field->data + (row + i)*data_field->xres + col;

        for (j = 0; j < width; j++) {
            if (*drow < threshval)
                *drow = bottom;
            else {
                *drow = top;
                tot++;
            }
        }
    }
    gwy_data_field_invalidate(data_field);

    return tot;
}

/**
 * gwy_data_field_clamp:
 * @data_field: A data field.
 * @bottom: Lower limit value.
 * @top: Upper limit value.
 *
 * Limits data field values to a range.
 *
 * Returns: The number of changed values, i.e., values that were outside
 *          [@bottom, @top].
 **/
gint
gwy_data_field_clamp(GwyDataField *data_field,
                     gdouble bottom, gdouble top)
{
    gint i, tot = 0;
    gdouble *p = data_field->data;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(bottom <= top, 0);

    for (i = data_field->xres * data_field->yres; i; i--, p++) {
        if (*p < bottom) {
            *p = bottom;
            tot++;
        }
        else if (*p > top) {
            *p = top;
            tot++;
        }
    }
    if (tot) {
        /* We can precompute stats */
        data_field->cached &= CBIT(MIN) | CBIT(MAX) | CBIT(MED);
        CVAL(data_field, MIN) = MAX(bottom, CVAL(data_field, MIN));
        CVAL(data_field, MAX) = MIN(top, CVAL(data_field, MAX));
        if (CTEST(data_field, MED)
            && (CVAL(data_field, MED) < bottom || CVAL(data_field, MED) > top))
            data_field->cached &= ~CBIT(MED);
    }

    return tot;
}

/**
 * gwy_data_field_area_clamp:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @bottom: Lower limit value.
 * @top: Upper limit value.
 *
 * Limits values in a rectangular part of a data field to a range.
 *
 * Returns: The number of changed values, i.e., values that were outside
 *          [@bottom, @top].
 **/
gint
gwy_data_field_area_clamp(GwyDataField *data_field,
                          gint col, gint row,
                          gint width, gint height,
                          gdouble bottom, gdouble top)
{
    gint i, j, tot = 0;
    gdouble *drow;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         0);

    for (i = 0; i < height; i++) {
        drow = data_field->data + (row + i)*data_field->xres + col;

        for (j = 0; j < width; j++) {
            if (*drow < bottom) {
                *drow = bottom;
                tot++;
            }
            else if (*drow > top) {
                *drow = top;
                tot++;
            }
        }

    }
    if (tot)
        gwy_data_field_invalidate(data_field);

    return tot;
}

/**
 * gwy_data_field_area_gather:
 * @data_field: A data field.
 * @result: A data field to put the result to, it may be @data_field itself.
 * @buffer: A data field to use as a scratch area, its size must be at least
 *          @width*@height.  May be %NULL to allocate a private temporary
 *          buffer.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @hsize: Horizontal size of gathered area.  The area is centered around
 *         each sample if @hsize is odd, it extends one pixel more to the
 *         right if @hsize is even.
 * @vsize: Vertical size of gathered area.  The area is centered around
 *         each sample if @vsize is odd, it extends one pixel more down
 *         if @vsize is even.
 * @average: %TRUE to divide resulting sums by the number of involved samples
 *           to get averages instead of sums.
 *
 * Sums or averages values in reactangular areas around each sample in a data
 * field.
 *
 * When the gathered area extends out of calculation area, only samples from
 * their intersection are taken into the local sum (or average).
 *
 * There are no restrictions on values of @hsize and @vsize with regard to
 * @width and @height, but they have to be positive.
 *
 * The result is calculated by the means of two-dimensional rolling sums.
 * One one hand it means the calculation time depends linearly on
 * (@width + @hsize)*(@height + @vsize) instead of
 * @width*@hsize*@height*@vsize.  On the other hand it means absolute rounding
 * errors of all output values are given by the largest input values, that is
 * relative precision of results small in absolute value may be poor.
 **/
void
gwy_data_field_area_gather(GwyDataField *data_field,
                           GwyDataField *result,
                           GwyDataField *buffer,
                           gint hsize,
                           gint vsize,
                           gboolean average,
                           gint col, gint row,
                           gint width, gint height)
{
    const gdouble *srow, *trow;
    gdouble *drow;
    gint xres, yres, i, j, m;
    gint hs2p, hs2m, vs2p, vs2m;
    gdouble v;

    g_return_if_fail(hsize > 0 && vsize > 0);
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    xres = data_field->xres;
    yres = data_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= xres
                     && row + height <= yres);
    g_return_if_fail(GWY_IS_DATA_FIELD(result));
    g_return_if_fail(result->xres == xres && result->yres == yres);
    if (buffer) {
        g_return_if_fail(GWY_IS_DATA_FIELD(buffer));
        g_return_if_fail(buffer->xres*buffer->yres >= width*height);
        g_object_ref(buffer);
    }
    else
        buffer = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);

    /* Extension to the left and to the right (for asymmetric sizes extend
     * to the right more) */
    hs2m = (hsize - 1)/2;
    hs2p = hsize/2;
    vs2m = (vsize - 1)/2;
    vs2p = vsize/2;

    /* Row-wise sums */
    /* FIXME: This is inefficient, split the inner loops to explicitly
     * according to the conditions inside */
    for (i = 0; i < height; i++) {
        srow = data_field->data + (i + row)*xres + col;
        drow = buffer->data + i*width;

        /* Left half */
        drow[0] = 0.0;
        m = MIN(hs2p, width-1);
        for (j = 0; j <= m; j++)
            drow[0] += srow[j];
        for (j = 1; j < width/2; j++) {
            v = ((j + hs2p < width ? srow[j + hs2p] : 0.0)
                 - (j-1 - hs2m >= 0 ? srow[j-1 - hs2m] : 0.0));
            drow[j] = drow[j-1] + v;
        }

        /* Right half */
        drow[width-1] = 0.0;
        m = width-1 - MIN(hs2m, width-1);
        for (j = width-1; j >= m; j--)
            drow[width-1] += srow[j];
        for (j = width-2; j >= width/2; j--) {
            v = ((j - hs2m >= 0 ? srow[j - hs2m] : 0.0)
                 - (j+1 + hs2p < width ? srow[j+1 + hs2p] : 0.0));
            drow[j] = drow[j+1] + v;
        }
    }

    /* Column-wise sums (but iterate row-wise to access memory linearly) */
    /* Top half */
    drow = result->data + row*xres + col;
    for (j = 0; j < width; j++)
        drow[j] = 0.0;
    m = MIN(vs2p, height-1);
    for (i = 0; i <= m; i++) {
        srow = buffer->data + i*width;
        for (j = 0; j < width; j++)
            drow[j] += srow[j];
    }
    for (i = 1; i < height/2; i++) {
        drow = result->data + (i + row)*xres + col;
        if (i + vs2p < height) {
            srow = buffer->data + (i + vs2p)*width;
            if (i-1 - vs2m >= 0) {
                trow = buffer->data + (i-1 - vs2m)*width;
                for (j = 0; j < width; j++)
                    drow[j] = *(drow + j - xres) + (srow[j] - trow[j]);
            }
            else {
                for (j = 0; j < width; j++)
                    drow[j] = *(drow + j - xres) + srow[j];
            }
        }
        else {
            if (G_UNLIKELY(i-1 - vs2m >= 0)) {
                g_warning("Me thinks pure subtraction cannot occur.");
                trow = buffer->data + (i-1 - vs2m)*width;
                for (j = 0; j < width; j++)
                    drow[j] = *(drow + j - xres) - trow[j];
            }
            else {
                for (j = 0; j < width; j++)
                    drow[j] = *(drow + j - xres);
            }
        }
    }

    /* Bottom half */
    drow = result->data + (height-1 + row)*xres + col;
    for (j = 0; j < width; j++)
        drow[j] = 0.0;
    m = height-1 - MIN(vs2m, height-1);
    for (i = height-1; i >= m; i--) {
        srow = buffer->data + i*width;
        for (j = 0; j < width; j++)
            drow[j] += srow[j];
    }
    for (i = height-2; i >= height/2; i--) {
        drow = result->data + (i + row)*xres + col;
        if (i+1 + vs2p < height) {
            srow = buffer->data + (i+1 + vs2p)*width;
            if (G_LIKELY(i - vs2m >= 0)) {
                trow = buffer->data + (i - vs2m)*width;
                for (j = 0; j < width; j++)
                    drow[j] = drow[j + xres] + (trow[j] - srow[j]);
            }
            else {
                g_warning("Me thinks pure subtraction cannot occur.");
                for (j = 0; j < width; j++)
                    drow[j] = drow[j + xres] - srow[j];
            }
        }
        else {
            if (i - vs2m >= 0) {
                trow = buffer->data + (i - vs2m)*width;
                for (j = 0; j < width; j++)
                    drow[j] = drow[j + xres] + trow[j];
            }
            else {
                for (j = 0; j < width; j++)
                    drow[j] = drow[j + xres];
            }
        }
    }

    gwy_data_field_invalidate(result);
    gwy_data_field_invalidate(buffer);
    g_object_unref(buffer);

    if (!average)
        return;

    /* Divide sums by the numbers of pixels that entered them */
    for (i = 0; i < height; i++) {
        gint iw;

        if (i <= vs2m)
            iw = vs2p + 1 + i;
        else if (i >= height-1 - vs2p)
            iw = vs2m + height - i;
        else
            iw = vsize;
        iw = MIN(iw, height);

        for (j = 0; j < width; j++) {
            gint jw;

            if (j <= hs2m)
                jw = hs2p + 1 + j;
            else if (j >= width-1 - hs2p)
                jw = hs2m + width - j;
            else
                jw = hsize;
            jw = MIN(jw, width);

            result->data[(i + row)*xres + j + col] /= iw*jw;
        }
    }
}

static void
gwy_data_field_area_convolve_3x3(GwyDataField *data_field,
                                 const gdouble *kernel,
                                 gint col, gint row,
                                 gint width, gint height)
{
    gdouble *rm, *rc, *rp;
    gdouble t, v;
    gint xres, i, j;

    xres = data_field->xres;
    rp = data_field->data + row*xres + col;

    /* Special-case width == 1 to avoid complications below.  It's silly but
     * the API guarantees it. */
    if (width == 1) {
        t = rp[0];
        for (i = 0; i < height; i++) {
            rc = rp = data_field->data + (row + i)*xres + col;
            if (i < height-1)
                rp += xres;

            v = (kernel[0] + kernel[1] + kernel[2])*t
                + (kernel[3] + kernel[4] + kernel[5])*rc[0]
                + (kernel[6] + kernel[7] + kernel[8])*rp[0];
            t = rc[0];
            rc[0] = v;
        }
        gwy_data_field_invalidate(data_field);

        return;
    }

    rm = g_new(gdouble, width);
    memcpy(rm, rp, width*sizeof(gdouble));

    for (i = 0; i < height; i++) {
        rc = rp;
        if (i < height-1)
            rp += xres;
        v = (kernel[0] + kernel[1])*rm[0] + kernel[2]*rm[1]
            + (kernel[3] + kernel[4])*rc[0] + kernel[5]*rc[1]
            + (kernel[6] + kernel[7])*rp[0] + kernel[8]*rp[1];
        t = rc[0];
        rc[0] = v;
        if (i < height-1) {
            for (j = 1; j < width-1; j++) {
                v = kernel[0]*rm[j-1] + kernel[1]*rm[j] + kernel[2]*rm[j+1]
                    + kernel[3]*t + kernel[4]*rc[j] + kernel[5]*rc[j+1]
                    + kernel[6]*rp[j-1] + kernel[7]*rp[j] + kernel[8]*rp[j+1];
                rm[j-1] = t;
                t = rc[j];
                rc[j] = v;
            }
            v = kernel[0]*rm[j-1] + (kernel[1] + kernel[2])*rm[j]
                + kernel[3]*t + (kernel[4] + kernel[5])*rc[j]
                + kernel[6]*rp[j-1] + (kernel[7] + kernel[8])*rp[j];
        }
        else {
            for (j = 1; j < width-1; j++) {
                v = kernel[0]*rm[j-1] + kernel[1]*rm[j] + kernel[2]*rm[j+1]
                    + kernel[3]*t + kernel[4]*rc[j] + kernel[5]*rc[j+1]
                    + kernel[6]*t + kernel[7]*rc[j] + kernel[8]*rc[j+1];
                rm[j-1] = t;
                t = rc[j];
                rc[j] = v;
            }
            v = kernel[0]*rm[j-1] + (kernel[1] + kernel[2])*rm[j]
                + kernel[3]*t + (kernel[4] + kernel[5])*rc[j]
                + kernel[6]*t + (kernel[7] + kernel[8])*rc[j];
        }
        rm[j-1] = t;
        rm[j] = rc[j];
        rc[j] = v;
    }

    g_free(rm);
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_area_convolve:
 * @data_field: A data field to convolve.  It must be at least as large as
 *              1/3 of @kernel_field in each dimension.
 * @kernel_field: Kenrel field to convolve @data_field with.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Convolves a rectangular part of a data field with given kernel.
 **/
void
gwy_data_field_area_convolve(GwyDataField *data_field,
                             GwyDataField *kernel_field,
                             gint col, gint row,
                             gint width, gint height)
{
    gint xres, yres, kxres, kyres, i, j, m, n, ii, jj;
    GwyDataField *hlp_df;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(kernel_field));

    xres = data_field->xres;
    yres = data_field->yres;
    kxres = kernel_field->xres;
    kyres = kernel_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= xres
                     && row + height <= yres);

    if (kxres == 3 && kyres == 3) {
        gwy_data_field_area_convolve_3x3(data_field, kernel_field->data,
                                         col, row, width, height);
        return;
    }

    hlp_df = gwy_data_field_new(width, height, 1.0, 1.0, TRUE);
    for (i = row; i < row + height; i++) {
        for (j = col; j < col + width; j++) {
            for (m = -kyres/2; m < kyres - kyres/2; m++) {
                ii = i + m;
                if (G_UNLIKELY(ii < 0))
                    ii = -ii-1;
                else if (G_UNLIKELY(ii >= yres))
                    ii = 2*yres-1 - ii;

                for (n = -kxres/2; n < kxres - kxres/2; n++) {
                    jj = j + n;
                    if (G_UNLIKELY(jj < 0))
                        jj = -jj-1;
                    else if (G_UNLIKELY(jj >= xres))
                        jj = 2*xres-1 - jj;

                    hlp_df->data[(i - row)*width + (j - col)]
                        += data_field->data[ii*xres + jj]
                           * kernel_field->data[kxres*(m + kyres/2)
                                                + n + kxres/2];
                }
            }
        }
    }
    gwy_data_field_area_copy(hlp_df, data_field, 0, 0, width, height, col, row);
    g_object_unref(hlp_df);

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_convolve:
 * @data_field: A data field to convolve.  It must be at least as large as
 *              1/3 of @kernel_field in each dimension.
 * @kernel_field: Kenrel field to convolve @data_field with.
 *
 * Convolves a data field with given kernel.
 **/
void
gwy_data_field_convolve(GwyDataField *data_field,
                        GwyDataField *kernel_field)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_convolve(data_field, kernel_field,
                                 0, 0, data_field->xres, data_field->yres);
}

static void
gwy_data_field_area_hconvolve(GwyDataField *data_field,
                              GwyDataLine *kernel_line,
                              gint col, gint row,
                              gint width, gint height)
{
    gint kres, mres, k0, i, j, k, pos;
    const gdouble *kernel;
    gdouble *buf, *drow;
    gdouble d;

    kres = kernel_line->res;
    kernel = kernel_line->data;
    mres = 2*width;
    k0 = (kres/2 + 1)*mres;
    buf = g_new(gdouble, kres);

    for (i = 0; i < height; i++) {
        drow = data_field->data + (row + i)*data_field->xres + col;
        /* Initialize with a triangluar sums, mirror-extend */
        gwy_clear(buf, kres);
        for (j = 0; j < kres; j++) {
            k = (j - kres/2 + k0) % mres;
            d = drow[k < width ? k : mres-1 - k];
            for (k = 0; k <= j; k++)
                buf[k] += kernel[j - k]*d;
        }
        pos = 0;
        /* Middle part and tail with mirror extension again, we do some
         * O(1/2*k^2) of useless work here by not separating the tail */
        for (j = 0; j < width; j++) {
            drow[j] = buf[pos];
            buf[pos] = 0.0;
            pos = (pos + 1) % kres;
            k = (j + kres - kres/2 + k0) % mres;
            d = drow[G_LIKELY(k < width) ? k : mres-1 - k];
            for (k = pos; k < kres; k++)
                buf[k] += kernel[kres-1 - (k - pos)]*d;
            for (k = 0; k < pos; k++)
                buf[k] += kernel[pos-1 - k]*d;
        }
    }

    g_free(buf);
}

static void
gwy_data_field_area_vconvolve(GwyDataField *data_field,
                              GwyDataLine *kernel_line,
                              gint col, gint row,
                              gint width, gint height)
{
    gint kres, xres, mres, k0, i, j, k, pos;
    const gdouble *kernel;
    gdouble *buf, *dcol;
    gdouble d;

    kres = kernel_line->res;
    kernel = kernel_line->data;
    xres = data_field->xres;
    mres = 2*height;
    k0 = (kres/2 + 1)*mres;
    buf = g_new(gdouble, kres);

    /* This looks like a bad memory access pattern.  And for small kernels it
     * indeed is (we should iterate row-wise and directly calculate the sums).
     * For large kernels this is mitigated by the maximum possible amount of
     * work done per a data field access. */
    for (j = 0; j < width; j++) {
        dcol = data_field->data + row*xres + (col + j);
        /* Initialize with a triangluar sums, mirror-extend */
        gwy_clear(buf, kres);
        for (i = 0; i < kres; i++) {
            k = (i - kres/2 + k0) % mres;
            d = dcol[k < height ? k*xres : (mres-1 - k)*xres];
            for (k = 0; k <= i; k++)
                buf[k] += kernel[i - k]*d;
        }
        pos = 0;
        /* Middle part and tail with mirror extension again, we do some
         * O(1/2*k^2) of useless work here by not separating the tail */
        for (i = 0; i < height; i++) {
            dcol[i*xres] = buf[pos];
            buf[pos] = 0.0;
            pos = (pos + 1) % kres;
            k = (i + kres - kres/2 + k0) % mres;
            d = dcol[G_LIKELY(k < height) ? k*xres : (mres-1 - k)*xres];
            for (k = pos; k < kres; k++)
                buf[k] += kernel[kres-1 - (k - pos)]*d;
            for (k = 0; k < pos; k++)
                buf[k] += kernel[pos-1 - k]*d;
        }
    }

    g_free(buf);
}

/**
 * gwy_data_field_area_convolve_1d:
 * @data_field: A data field to convolve.  It must be at least as large as
 *              1/3 of @kernel_field in the corresponding dimension.
 * @kernel_line: Kernel line to convolve @data_field with.
 * @orientation: Filter orientation (%GWY_ORIENTATION_HORIZONTAL for
 *               row-wise convolution, %GWY_ORIENTATION_VERTICAL for
 *               column-wise convolution).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Convolves a rectangular part of a data field with given linear kernel.
 *
 * For large separable kernels it can be more efficient to use a sequence of
 * horizontal and vertical convolutions instead one 2D convolution.
 *
 * Since: 2.4
 **/
void
gwy_data_field_area_convolve_1d(GwyDataField *data_field,
                                GwyDataLine *kernel_line,
                                GwyOrientation orientation,
                                gint col, gint row,
                                gint width, gint height)
{
    gint kres;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(kernel_line));

    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    kres = kernel_line->res;
    if (kres == 1) {
        gwy_data_field_area_multiply(data_field, col, row, width, height,
                                     kernel_line->data[0]);
        return;
    }

    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        gwy_data_field_area_hconvolve(data_field, kernel_line,
                                      col, row, width, height);
        break;

        case GWY_ORIENTATION_VERTICAL:
        gwy_data_field_area_vconvolve(data_field, kernel_line,
                                      col, row, width, height);
        break;

        default:
        g_return_if_reached();
        break;
    }

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_convolve_1d:
 * @data_field: A data field to convolve.  It must be at least as large as
 *              1/3 of @kernel_field in the corresponding dimension.
 * @kernel_line: Kenrel line to convolve @data_field with.
 * @orientation: Filter orientation (see gwy_data_field_area_convolve_1d()).
 *
 * Convolves a data field with given linear kernel.
 *
 * Since: 2.4
 **/
void
gwy_data_field_convolve_1d(GwyDataField *data_field,
                           GwyDataLine *kernel_line,
                           GwyOrientation orientation)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_convolve_1d(data_field, kernel_line, orientation,
                                    0, 0, data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_area_filter_mean:
 * @data_field: A data field to apply the filter to.
 * @size: Averaged area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with mean filter of size @size.
 *
 * This method is a simple gwy_data_field_area_gather() wrapper.
 **/
void
gwy_data_field_area_filter_mean(GwyDataField *data_field,
                                gint size,
                                gint col, gint row,
                                gint width, gint height)
{
    gwy_data_field_area_gather(data_field, data_field, NULL,
                               size, size, TRUE,
                               col, row, width, height);
}

/**
 * gwy_data_field_filter_mean:
 * @data_field: A data field to apply the filter to.
 * @size: Averaged area size.
 *
 * Filters a data field with mean filter of size @size.
 **/
void
gwy_data_field_filter_mean(GwyDataField *data_field,
                           gint size)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_mean(data_field, size, 0, 0,
                                    data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_area_filter_rms:
 * @data_field: A data field to apply RMS filter to.
 * @size: Area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with RMS filter of size @size.
 *
 * RMS filter computes root mean square in given area.
 **/
void
gwy_data_field_area_filter_rms(GwyDataField *data_field,
                               gint size,
                               gint col, gint row,
                               gint width, gint height)
{
    GwyDataField *avg2, *buffer;
    gint i, j;
    const gdouble *arow;
    gdouble *drow;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(size > 0);
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    if (size == 1) {
        gwy_data_field_clear(data_field);
        return;
    }

    avg2 = gwy_data_field_area_extract(data_field, col, row, width, height);
    for (i = 0; i < width*height; i++)
        avg2->data[i] *= avg2->data[i];

    buffer = gwy_data_field_new_alike(avg2, FALSE);
    gwy_data_field_area_gather(avg2, avg2, buffer,
                               size, size, TRUE,
                               0, 0, width, height);
    gwy_data_field_area_gather(data_field, data_field, buffer,
                               size, size, TRUE,
                               col, row, width, height);
    g_object_unref(buffer);

    for (i = 0; i < height; i++) {
        arow = avg2->data + i*width;
        drow = data_field->data + (i + row)*data_field->xres + col;
        for (j = 0; j < width; j++) {
            drow[j] = arow[j] - drow[j]*drow[j];
            drow[j] = sqrt(MAX(drow[j], 0.0));
        }
    }
    g_object_unref(avg2);

    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_filter_rms:
 * @data_field: A data field to apply RMS filter to.
 * @size: Area size.
 *
 * Filters a data field with RMS filter.
 **/
void
gwy_data_field_filter_rms(GwyDataField *data_field,
                          gint size)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_rms(data_field, size, 0, 0,
                                   data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_filter_canny:
 * @data_field: A data field to apply the filter to.
 * @threshold: Slope detection threshold (range 0..1).
 *
 * Filters a rectangular part of a data field with canny edge detector filter.
 **/
void
gwy_data_field_filter_canny(GwyDataField *data_field,
                            gdouble threshold)
{
    GwyDataField *sobel_horizontal;
    GwyDataField *sobel_vertical;
    gint i, j, k;
    gdouble angle;
    gboolean pass;
    gdouble *data;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    sobel_horizontal = gwy_data_field_duplicate(data_field);
    sobel_vertical = gwy_data_field_duplicate(data_field);

    gwy_data_field_filter_sobel(sobel_horizontal, GWY_ORIENTATION_HORIZONTAL);
    gwy_data_field_filter_sobel(sobel_vertical, GWY_ORIENTATION_VERTICAL);

    data = data_field->data;
    for (k = 0; k < (data_field->xres*data_field->yres); k++)
        data[k] = fabs(sobel_horizontal->data[k])
                  + fabs(sobel_vertical->data[k]);
    gwy_data_field_invalidate(data_field);

    threshold = gwy_data_field_get_min(data_field)
                + (gwy_data_field_get_max(data_field)
                   - gwy_data_field_get_min(data_field))*threshold;

    for (i = 0; i < data_field->yres; i++) {
        for (j = 0; j < data_field->xres; j++) {
            pass = FALSE;
            if (data[j + data_field->xres*i] > threshold
                && i > 0 && j > 0
                && i < (data_field->yres - 1)
                && j < (data_field->xres - 1)) {
                angle = atan2(sobel_vertical->data[j + data_field->xres*i],
                              sobel_horizontal->data[j + data_field->xres*i]);

                if (angle < 0.3925 || angle > 5.8875
                    || (angle > 2.7475 && angle < 3.5325)) {
                    if (data[j + 1 + data_field->xres*i] > threshold)
                        pass = TRUE;
                }
                else if ((angle > 1.178 && angle < 1.9632)
                         || (angle > 4.318 && angle < 5.1049)) {
                    if (data[j + 1 + data_field->xres*(i + 1)] > threshold)
                        pass = TRUE;
                }
                else {
                    if (data[j + data_field->xres*(i + 1)] > threshold)
                        pass = TRUE;
                }
            }
            /*we do not need sobel array more,
             * so use sobel_horizontal to store data results*/
            if (pass)
                sobel_horizontal->data[j + data_field->xres*i] = 1;
            else
                sobel_horizontal->data[j + data_field->xres*i] = 0;
        }
    }
    /*result is now in sobel_horizontal field*/
    gwy_data_field_copy(sobel_horizontal, data_field, FALSE);

    g_object_unref(sobel_horizontal);
    g_object_unref(sobel_vertical);

    /*thin the lines*/
    thin_data_field(data_field);
    gwy_data_field_invalidate(data_field);
}

 /**
 * gwy_data_field_area_filter_laplacian:
 * @data_field: A data field to apply the filter to.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with Laplacian filter.
 **/
void
gwy_data_field_area_filter_laplacian(GwyDataField *data_field,
                                     gint col, gint row,
                                     gint width, gint height)
{
    const gdouble laplace[] = {
        0,  1, 0,
        1, -4, 1,
        0,  1, 0,
    };

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_convolve_3x3(data_field, laplace,
                                     col, row, width, height);
}

/**
 * gwy_data_field_filter_laplacian:
 * @data_field: A data field to apply the filter to.
 *
 * Filters a data field with Laplacian filter.
 **/
void
gwy_data_field_filter_laplacian(GwyDataField *data_field)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_laplacian(data_field, 0, 0,
                                         data_field->xres, data_field->yres);
}

 /**
 * gwy_data_field_area_filter_laplacian_of_gaussians:
 * @data_field: A data field to apply the filter to.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with Laplacian of Gaussians filter.
 * 
 * Since: 2.23
 **/
void
gwy_data_field_area_filter_laplacian_of_gaussians(GwyDataField *data_field,
                                                  gint col, gint row,
                                                  gint width,
                                                  gint height)
{
	/* optimized mexican hat from Scharr's works */
	const gdouble laplacian_of_gaussians_data[] = {
		  1, -12,    3, -12,   1,
        -12,  78,  167,  78, -12,
          3, 167, -902, 167,   3,
        -12,  78,  167,  78, -12,
          1, -12,    3, -12,   1,
    };
    
    GwyDataField *laplacian_of_gaussians;
    gint i, j;

    laplacian_of_gaussians = gwy_data_field_new(5, 5, 5.0, 5.0, TRUE);
    for(i = 0; i < 5; i++)
        for(j = 0; j < 5; j++)
            gwy_data_field_set_val(laplacian_of_gaussians, j, i,
                                   laplacian_of_gaussians_data[i*5+j]);
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_convolve(data_field, laplacian_of_gaussians,
                                 col, row, width, height);

    g_object_unref(laplacian_of_gaussians);
}

/**
 * gwy_data_field_filter_laplacian_of_gaussians:
 * @data_field: A data field to apply the filter to.
 *
 * Filters a data field with Laplacian of Gaussians filter.
 * 
 * Since: 2.23
 **/
void
gwy_data_field_filter_laplacian_of_gaussians(GwyDataField *data_field)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_laplacian_of_gaussians(data_field, 0, 0,
                                                      data_field->xres,
                                                      data_field->yres);
}

/**
 * gwy_data_field_area_filter_sobel:
 * @data_field: A data field to apply the filter to.
 * @orientation: Filter orientation.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with Sobel filter.
 **/
void
gwy_data_field_area_filter_sobel(GwyDataField *data_field,
                                 GwyOrientation orientation,
                                 gint col, gint row,
                                 gint width, gint height)
{
    static const gdouble hsobel[] = {
        0.25, 0, -0.25,
        0.5,  0, -0.5,
        0.25, 0, -0.25,
    };
    static const gdouble vsobel[] = {
         0.25,  0.5,  0.25,
         0,     0,    0,
        -0.25, -0.5, -0.25,
    };

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (orientation == GWY_ORIENTATION_HORIZONTAL)
        gwy_data_field_area_convolve_3x3(data_field, hsobel,
                                         col, row, width, height);
    else
        gwy_data_field_area_convolve_3x3(data_field, vsobel,
                                         col, row, width, height);
}

/**
 * gwy_data_field_filter_sobel:
 * @data_field: A data field to apply the filter to.
 * @orientation: Filter orientation.
 *
 * Filters a data field with Sobel filter.
 **/
void
gwy_data_field_filter_sobel(GwyDataField *data_field,
                            GwyOrientation orientation)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_sobel(data_field, orientation, 0, 0,
                                     data_field->xres, data_field->yres);

}

/**
 * gwy_data_field_area_filter_prewitt:
 * @data_field: A data field to apply the filter to.
 * @orientation: Filter orientation.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with Prewitt filter.
 **/
void
gwy_data_field_area_filter_prewitt(GwyDataField *data_field,
                                   GwyOrientation orientation,
                                   gint col, gint row,
                                   gint width, gint height)
{
    static const gdouble hprewitt[] = {
        1.0/3.0, 0, -1.0/3.0,
        1.0/3.0, 0, -1.0/3.0,
        1.0/3.0, 0, -1.0/3.0,
    };
    static const gdouble vprewitt[] = {
         1.0/3.0,  1.0/3.0,  1.0/3.0,
         0,        0,        0,
        -1.0/3.0, -1.0/3.0, -1.0/3.0,
    };

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (orientation == GWY_ORIENTATION_HORIZONTAL)
        gwy_data_field_area_convolve_3x3(data_field, hprewitt,
                                         col, row, width, height);
    else
        gwy_data_field_area_convolve_3x3(data_field, vprewitt,
                                         col, row, width, height);
}

/**
 * gwy_data_field_filter_prewitt:
 * @data_field: A data field to apply the filter to.
 * @orientation: Filter orientation.
 *
 * Filters a data field with Prewitt filter.
 **/
void
gwy_data_field_filter_prewitt(GwyDataField *data_field,
                              GwyOrientation orientation)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_prewitt(data_field, orientation, 0, 0,
                                       data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_area_filter_dechecker:
 * @data_field: A data field to apply the filter to.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with 5x5 checker pattern removal
 * filter.
 *
 * Since: 2.1
 **/
void
gwy_data_field_area_filter_dechecker(GwyDataField *data_field,
                                     gint col, gint row,
                                     gint width, gint height)
{
    enum { size = 5 };
    static const gdouble checker[size*size] = {
         0.0,        1.0/144.0, -1.0/72.0,  1.0/144.0,  0.0,
         1.0/144.0, -1.0/18.0,   1.0/9.0,  -1.0/18.0,   1.0/144.0,
        -1.0/72.0,   1.0/9.0,    7.0/9.0,   1.0/9.0,   -1.0/72.0,
         1.0/144.0, -1.0/18.0,   1.0/9.0,  -1.0/18.0,   1.0/144.0,
         0.0,        1.0/144.0, -1.0/72.0,  1.0/144.0,  0.0,
    };
    GwyDataField *kernel;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    kernel = gwy_data_field_new(size, size, 1.0, 1.0, FALSE);
    memcpy(kernel->data, checker, sizeof(checker));
    gwy_data_field_area_convolve(data_field, kernel, col, row, width, height);
    g_object_unref(kernel);
}

/**
 * gwy_data_field_filter_dechecker:
 * @data_field: A data field to apply the filter to.
 *
 * Filters a data field with 5x5 checker pattern removal filter.
 *
 * Since: 2.1
 **/
void
gwy_data_field_filter_dechecker(GwyDataField *data_field)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_dechecker(data_field, 0, 0,
                                         data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_area_filter_gaussian:
 * @data_field: A data field to apply the filter to.
 * @sigma: The sigma parameter of the Gaussian.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with a Gaussian filter.
 *
 * The Gausian is normalized, i.e. it is sum-preserving.
 *
 * Since: 2.4
 **/
void
gwy_data_field_area_filter_gaussian(GwyDataField *data_field,
                                    gdouble sigma,
                                    gint col, gint row,
                                    gint width, gint height)
{
    GwyDataLine *kernel;
    gdouble x;
    gint res, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(sigma >= 0.0);
    if (sigma == 0.0)
        return;

    res = (gint)ceil(5.0*sigma);
    res = 2*res + 1;
    /* FIXME */
    i = 3*MIN(data_field->xres, data_field->yres);
    if (res > i) {
        res = i;
        if (res % 2 == 0)
            res--;
    }

    kernel = gwy_data_line_new(res, 1.0, FALSE);
    for (i = 0; i < res; i++) {
        x = i - (res - 1)/2.0;
        x /= sigma;
        kernel->data[i] = exp(-x*x/2.0);
    }
    gwy_data_line_multiply(kernel, 1.0/gwy_data_line_get_sum(kernel));
    gwy_data_field_area_convolve_1d(data_field, kernel,
                                    GWY_ORIENTATION_HORIZONTAL,
                                    col, row, width, height);
    gwy_data_field_area_convolve_1d(data_field, kernel,
                                    GWY_ORIENTATION_VERTICAL,
                                    col, row, width, height);
    g_object_unref(kernel);
}

/**
 * gwy_data_field_filter_gaussian:
 * @data_field: A data field to apply the filter to.
 * @sigma: The sigma parameter of the Gaussian.
 *
 * Filters a data field with a Gaussian filter.
 *
 * Since: 2.4
 **/
void
gwy_data_field_filter_gaussian(GwyDataField *data_field,
                               gdouble sigma)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_gaussian(data_field, sigma, 0, 0,
                                         data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_area_filter_median:
 * @data_field: A data field to apply the filter to.
 * @size: Size of area to take median of.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with median filter.
 **/
void
gwy_data_field_area_filter_median(GwyDataField *data_field,
                                  gint size,
                                  gint col, gint row,
                                  gint width, gint height)
{

    gint rowstride;
    gint i, j, k, len;
    gint xfrom, xto, yfrom, yto;
    gdouble *buffer, *data, *kernel;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(size > 0);
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    buffer = g_new(gdouble, width*height);
    kernel = g_new(gdouble, size*size);
    rowstride = data_field->xres;
    data = data_field->data + rowstride*row + col;

    for (i = 0; i < height; i++) {
        yfrom = MAX(0, i - (size-1)/2);
        yto = MIN(height-1, i + size/2);
        for (j = 0; j < width; j++) {
            xfrom = MAX(0, j - (size-1)/2);
            xto = MIN(width-1, j + size/2);
            len = xto - xfrom + 1;
            for (k = yfrom; k <= yto; k++)
                memcpy(kernel + len*(k - yfrom),
                       data + k*rowstride + xfrom,
                       len*sizeof(gdouble));
            buffer[i*width + j] = gwy_math_median(len*(yto - yfrom + 1),
                                                  kernel);
        }
    }

    g_free(kernel);
    for (i = 0; i < height; i++)
        memcpy(data + i*rowstride, buffer + i*width, width*sizeof(gdouble));
    g_free(buffer);
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_filter_median:
 * @data_field: A data field to apply the filter to.
 * @size: Size of area to take median of.
 *
 * Filters a data field with median filter.
 **/
void
gwy_data_field_filter_median(GwyDataField *data_field,
                             gint size)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_median(data_field, size, 0, 0,
                                      data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_area_filter_conservative:
 * @data_field: A data field to apply the filter to.
 * @size: Filtered area size.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with conservative denoise filter.
 **/
void
gwy_data_field_area_filter_conservative(GwyDataField *data_field,
                                        gint size,
                                        gint col, gint row,
                                        gint width, gint height)
{
    gint xres, yres, i, j, ii, jj;
    gdouble maxval, minval;
    gdouble *data;
    GwyDataField *hlp_df;

    gwy_debug("");
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(size > 0);
    xres = data_field->xres;
    yres = data_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= xres
                     && row + height <= yres);
    if (size == 1)
        return;
    if (size > width || size > height) {
        g_warning("Kernel size larger than field area size.");
        return;
    }

    hlp_df = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);

    data = data_field->data;
    for (i = 0; i < height; i++) {
        gint ifrom = MAX(0, i + row - (size-1)/2);
        gint ito = MIN(yres-1, i + row + size/2);

        for (j = 0; j < width; j++) {
            gint jfrom = MAX(0, j + col - (size-1)/2);
            gint jto = MIN(xres-1, j + col + size/2);

            maxval = -G_MAXDOUBLE;
            minval = G_MAXDOUBLE;
            for (ii = 0; ii <= ito - ifrom; ii++) {
                gdouble *drow = data + (ifrom + ii)*xres + jfrom;

                for (jj = 0; jj <= jto - jfrom; jj++) {
                    if (i + row == ii + ifrom && j + col == jj + jfrom)
                        continue;

                    if (drow[jj] < minval)
                        minval = drow[jj];
                    if (drow[jj] > maxval)
                        maxval = drow[jj];
                }
            }

            hlp_df->data[i*width + j] = CLAMP(data[(i + row)*xres + j + col],
                                              minval, maxval);
        }
    }
    /* fix bottom right corner for size == 2 */
    if (size == 2)
        hlp_df->data[height*width - 1] = data[(row + height - 1)*xres
                                              + col + width - 1];

    gwy_data_field_area_copy(hlp_df, data_field, 0, 0, width, height, col, row);
    g_object_unref(hlp_df);
    gwy_data_field_invalidate(data_field);
}

/**
 * gwy_data_field_filter_conservative:
 * @data_field: A data field to apply the filter to.
 * @size: Filtered area size.
 *
 * Filters a data field with conservative denoise filter.
 **/
void
gwy_data_field_filter_conservative(GwyDataField *data_field,
                                   gint size)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_conservative(data_field, size, 0, 0,
                                            data_field->xres, data_field->yres);
}

static inline gint
pixel_status(GwyDataField *data_field, gint i, gint j)
{
    if (data_field->data[j + data_field->xres * i] == 0)
        return 0;
    else
        return 1;
}

static gint
znzt_val(GwyDataField *data_field, gint i, gint j)
{
    gint ch = 0;
    gint pi[9];
    gint pj[9], k;

    pi[0] = i + 1;
    pi[1] = i;
    pi[2] = i - 1;
    pi[3] = i - 1;
    pi[4] = i - 1;
    pi[5] = i;
    pi[6] = i + 1;
    pi[7] = i + 1;
    pi[8] = i + 1;

    pj[0] = j + 1;
    pj[1] = j + 1;
    pj[2] = j + 1;
    pj[3] = j;
    pj[4] = j - 1;
    pj[5] = j - 1;
    pj[6] = j - 1;
    pj[7] = j;
    pj[8] = j + 1;

    for (k = 0; k < 8; k++)
        if (pixel_status(data_field, pi[k], pj[k]) == 0
            && pixel_status(data_field, pi[k + 1], pj[k + 1]) == 1)
            ch++;

    return ch;
}

static gint
nzn_val(GwyDataField *data_field, gint i, gint j)
{
    gint ch, ip, jp;

    ch = 0;
    for (ip = -1; ip <= 1; ip++) {
        for (jp = -1; jp <= 1; jp++) {
            if (!(ip == 0 && jp == 0))
                ch += pixel_status(data_field, i + ip, j + jp);
        }
    }

    return ch;
}

static gint
pixel_thinnable(GwyDataField *data_field, gint i, gint j)
{
    gint xres, yres;
    gint c1, c2, c3, c4;
    gdouble val;

    xres = data_field->xres;
    yres = data_field->yres;

    if (i <= 1 || j <= 1 || i >= (xres - 2) || (j >= yres - 2))
        return -1;

    c1 = c2 = c3 = c4 = 0;

    if (znzt_val(data_field, i, j) == 1)
        c1 = 1;
    val = nzn_val(data_field, i, j);

    if (val >= 2 && val <= 6)
        c2 = 1;

    if ((znzt_val(data_field, i + 1, j) != 1)
        || ((pixel_status(data_field, i, j + 1)
             * pixel_status(data_field, i, j - 1)
             * pixel_status(data_field, i + 1, j)) == 0))
        c3 = 1;

    if ((znzt_val(data_field, i, j + 1) != 1)
        || ((pixel_status(data_field, i, j + 1)
             * pixel_status(data_field, i - 1, j)
             * pixel_status(data_field, i + 1, j) == 0)))
        c4 = 1;

    if (c1 == 1 && c2 == 1 && c3 == 1 && c4 == 1)
        return 1;
    else
        return 0;

}

static gint
thinstep(GwyDataField *data_field,
         GwyDataField *buffer)
{
    gint i, j, ch;

    gwy_data_field_clear(buffer);
    ch = 0;
    for (i = 2; i < (data_field->yres - 1); i++) {
        for (j = 2; j < (data_field->xres - 1); j++) {
            if (pixel_status(data_field, i, j) == 1
                && pixel_thinnable(data_field, i, j) == 1) {
                ch++;
                buffer->data[j + data_field->xres * i] = 1;
            }
        }
    }
    for (i = 2; i < (data_field->yres - 1); i++) {
        for (j = 2; j < (data_field->xres - 1); j++) {
            if (buffer->data[j + data_field->xres * i] == 1)
                data_field->data[j + data_field->xres * i] = 0;
        }
    }
    gwy_data_field_invalidate(data_field);

    return ch;
}

static gint
thin_data_field(GwyDataField *data_field)
{
    GwyDataField *buffer;
    gint k, n;

    buffer = gwy_data_field_new_alike(data_field, FALSE);
    for (k = 0; k < 2000; k++) {
        n = thinstep(data_field, buffer);
        if (n == 0)
            break;
    }
    g_object_unref(buffer);

    return k;
}

/**
 * gwy_data_field_area_filter_minimum:
 * @data_field: A data field to apply minimum filter to.
 * @size: Neighbourhood size for minimum search.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with minimum filter.
 *
 * This operation is often called erosion filter.
 **/
void
gwy_data_field_area_filter_minimum(GwyDataField *data_field,
                                   gint size,
                                   gint col,
                                   gint row,
                                   gint width,
                                   gint height)
{
    GwyDataField *buffer, *buffer2;
    gint d, i, j, ip, ii, im, jp, jm;
    gint ep, em;  /* positive and negative excess */
    gdouble *buf, *buf2;
    gdouble v;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    g_return_if_fail(size > 0);
    if (size == 1)
        return;

    /* FIXME: does this silly case need an alternative implementation? */
    if (size/2 >= MIN(width, height)) {
        g_warning("Too large kernel size for too small area.");
        return;
    }

    buffer = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);
    buffer2 = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);
    buf = buffer->data;
    buf2 = buffer2->data;

    d = 1;
    gwy_data_field_area_copy(data_field, buffer, col, row, width, height, 0, 0);
    while (3*d < size) {
        for (i = 0; i < height; i++) {
            ii = i*width;
            im = MAX(i - d, 0)*width;
            ip = MIN(i + d, height-1)*width;
            for (j = 0; j < width; j++) {
                jm = MAX(j - d, 0);
                jp = MIN(j + d, width-1);

                v = MIN(buf[im + jm], buf[im + jp]);
                if (v > buf[im + j])
                    v = buf[im + j];
                if (v > buf[ii + jm])
                    v = buf[ii + jm];
                if (v > buf[ii + j])
                    v = buf[ii + j];
                if (v > buf[ip + j])
                    v = buf[ip + j];
                if (v > buf[ii + jp])
                    v = buf[ii + jp];
                if (v > buf[ip + jm])
                    v = buf[ip + jm];
                if (v > buf[ip + jp])
                    v = buf[ip + jp];

                buf2[ii + j] = v;
            }
        }
        /* XXX: This breaks the relation between buffer and buf */
        GWY_SWAP(gdouble*, buf, buf2);
        d *= 3;
    }


    /* Now we have to overlay the neighbourhoods carefully to get exactly
     * @size-sized squares.  There are two cases:
     * 1. @size <= 2*d, it's enough to take four corner representants
     * 2. @size > 2*d, it's necessary to take all nine representants
     */
    ep = size/2;
    em = (size - 1)/2;

    for (i = 0; i < height; i++) {
        ii = i*width;
        im = (MAX(i - em, 0) + d/2)*width;
        ip = (MIN(i + ep, height-1) - d/2)*width;

        for (j = 0; j < width; j++) {
            jm = MAX(j - em, 0) + d/2;
            jp = MIN(j + ep, width-1) - d/2;

            v = MIN(buf[im + jm], buf[im + jp]);
            if (2*d < size) {
                if (v > buf[im + j])
                    v = buf[im + j];
                if (v > buf[ii + jm])
                    v = buf[ii + jm];
                if (v > buf[ii + j])
                    v = buf[ii + j];
                if (v > buf[ii + jp])
                    v = buf[ii + jp];
                if (v > buf[ip + j])
                    v = buf[ip + j];
            }
            if (v > buf[ip + jm])
                v = buf[ip + jm];
            if (v > buf[ip + jp])
                v = buf[ip + jp];

            buf2[ii + j] = v;
        }
    }
    buffer->data = buf;
    buffer2->data = buf2;

    gwy_data_field_area_copy(buffer2, data_field,
                             0, 0, width, height, col, row);

    g_object_unref(buffer2);
    g_object_unref(buffer);
}

/**
 * gwy_data_field_filter_minimum:
 * @data_field: A data field to apply minimum filter to.
 * @size: Neighbourhood size for minimum search.
 *
 * Filters a data field with minimum filter.
 **/
void
gwy_data_field_filter_minimum(GwyDataField *data_field,
                              gint size)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_minimum(data_field, size, 0, 0,
                                       data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_area_filter_maximum:
 * @data_field: A data field to apply maximum filter to.
 * @size: Neighbourhood size for maximum search.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with maximum filter.
 *
 * This operation is often called dilation filter.
 **/
void
gwy_data_field_area_filter_maximum(GwyDataField *data_field,
                                   gint size,
                                   gint col,
                                   gint row,
                                   gint width,
                                   gint height)
{
    GwyDataField *buffer, *buffer2;
    gint d, i, j, ip, ii, im, jp, jm;
    gint ep, em;  /* positive and negative excess */
    gdouble *buf, *buf2;
    gdouble v;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    g_return_if_fail(size > 0);
    if (size == 1)
        return;

    /* FIXME: does this silly case need an alternative implementation? */
    if (size/2 >= MIN(width, height)) {
        g_warning("Too large kernel size for too small area.");
        return;
    }

    buffer = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);
    buffer2 = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);
    buf = buffer->data;
    buf2 = buffer2->data;

    d = 1;
    gwy_data_field_area_copy(data_field, buffer, col, row, width, height, 0, 0);
    while (3*d < size) {
        for (i = 0; i < height; i++) {
            ii = i*width;
            im = MAX(i - d, 0)*width;
            ip = MIN(i + d, height-1)*width;
            for (j = 0; j < width; j++) {
                jm = MAX(j - d, 0);
                jp = MIN(j + d, width-1);

                v = MAX(buf[im + jm], buf[im + jp]);
                if (v < buf[im + j])
                    v = buf[im + j];
                if (v < buf[ii + jm])
                    v = buf[ii + jm];
                if (v < buf[ii + j])
                    v = buf[ii + j];
                if (v < buf[ip + j])
                    v = buf[ip + j];
                if (v < buf[ii + jp])
                    v = buf[ii + jp];
                if (v < buf[ip + jm])
                    v = buf[ip + jm];
                if (v < buf[ip + jp])
                    v = buf[ip + jp];

                buf2[ii + j] = v;
            }
        }
        /* XXX: This breaks the relation between buffer and buf */
        GWY_SWAP(gdouble*, buf, buf2);
        d *= 3;
    }


    /* Now we have to overlay the neighbourhoods carefully to get exactly
     * @size-sized squares.  There are two cases:
     * 1. @size <= 2*d, it's enough to take four corner representants
     * 2. @size > 2*d, it's necessary to take all nine representants
     */
    ep = size/2;
    em = (size - 1)/2;

    for (i = 0; i < height; i++) {
        ii = i*width;
        im = (MAX(i - em, 0) + d/2)*width;
        ip = (MIN(i + ep, height-1) - d/2)*width;

        for (j = 0; j < width; j++) {
            jm = MAX(j - em, 0) + d/2;
            jp = MIN(j + ep, width-1) - d/2;

            v = MAX(buf[im + jm], buf[im + jp]);
            if (2*d < size) {
                if (v < buf[im + j])
                    v = buf[im + j];
                if (v < buf[ii + jm])
                    v = buf[ii + jm];
                if (v < buf[ii + j])
                    v = buf[ii + j];
                if (v < buf[ii + jp])
                    v = buf[ii + jp];
                if (v < buf[ip + j])
                    v = buf[ip + j];
            }
            if (v < buf[ip + jm])
                v = buf[ip + jm];
            if (v < buf[ip + jp])
                v = buf[ip + jp];

            buf2[ii + j] = v;
        }
    }
    buffer->data = buf;
    buffer2->data = buf2;

    gwy_data_field_area_copy(buffer2, data_field,
                             0, 0, width, height, col, row);

    g_object_unref(buffer2);
    g_object_unref(buffer);
}

/**
 * gwy_data_field_filter_maximum:
 * @data_field: A data field to apply maximum filter to.
 * @size: Neighbourhood size for maximum search.
 *
 * Filters a data field with maximum filter.
 **/
void
gwy_data_field_filter_maximum(GwyDataField *data_field,
                              gint size)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_maximum(data_field, size, 0, 0,
                                       data_field->xres, data_field->yres);
}

/**
 * kuwahara_block:
 * @a: points to a 5x5 matrix (array of 25 doubles)
 *
 * Computes a new value of the center pixel according to the Kuwahara filter.
 *
 * Return: Filtered value.
 */
static gdouble
kuwahara_block(const gdouble *a)
{
   static const gint r1[] = { 0, 1, 2, 5, 6, 7, 10, 11, 12 };
   static const gint r2[] = { 2, 3, 4, 7, 8, 9, 12, 13, 14 };
   static const gint r3[] = { 12, 13, 14, 17, 18, 19, 22, 23, 24 };
   static const gint r4[] = { 10, 11, 12, 15, 16, 17, 20, 21, 22 };
   gdouble mean1 = 0.0, mean2 = 0.0, mean3 = 0.0, mean4 = 0.0;
   gdouble var1 = 0.0, var2 = 0.0, var3 = 0.0, var4 = 0.0;
   gint i;

   for (i = 0; i < 9; i++) {
       mean1 += a[r1[i]]/9.0;
       mean2 += a[r2[i]]/9.0;
       mean3 += a[r3[i]]/9.0;
       mean4 += a[r4[i]]/9.0;
       var1 += a[r1[i]]*a[r1[i]]/9.0;
       var2 += a[r2[i]]*a[r2[i]]/9.0;
       var3 += a[r3[i]]*a[r3[i]]/9.0;
       var4 += a[r4[i]]*a[r4[i]]/9.0;
   }

   var1 -= mean1 * mean1;
   var2 -= mean2 * mean2;
   var3 -= mean3 * mean3;
   var4 -= mean4 * mean4;

   if (var1 <= var2 && var1 <= var3 && var1 <= var4)
       return mean1;
   if (var2 <= var3 && var2 <= var4 && var2 <= var1)
       return mean2;
   if (var3 <= var4 && var3 <= var1 && var3 <= var2)
       return mean3;
   if (var4 <= var1 && var4 <= var2 && var4 <= var3)
       return mean4;
   return 0.0;
}

#define gwy_data_field_get_val_closest(d, col, row) \
  ((d)->data[CLAMP((row), 0, (d)->yres-1) * (d)->xres \
  + CLAMP((col), 0, (d)->xres-1)])

/**
 * gwy_data_field_area_filter_kuwahara:
 * @data_field: A data filed to apply Kuwahara filter to.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Filters a rectangular part of a data field with a Kuwahara
 * (edge-preserving smoothing) filter.
 **/
void
gwy_data_field_area_filter_kuwahara(GwyDataField *data_field,
                                    gint col, gint row,
                                    gint width, gint height)
{
    gint i, j, x, y, ctr;
    gdouble *buffer, *kernel;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    buffer = g_new(gdouble, width*height);
    kernel = g_new(gdouble, 25);

    /* TO DO: optimize for speed */
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {

            ctr = 0;
            for (y = -2; y <= 2; y++) {
                for (x = -2; x <= 2; x++)
                    kernel[ctr++] = gwy_data_field_get_val_closest(data_field,
                                                                   col + j + x,
                                                                   row + i + y);
            }
            buffer[i*width + j] = kuwahara_block(kernel);
        }
    }

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++)
            data_field->data[col + j + data_field->xres * (row + i)] =
                buffer[i*width + j];
    }

    g_free(kernel);
    g_free(buffer);
}

/**
 * gwy_data_field_filter_kuwahara:
 * @data_field: A data field to apply Kuwahara filter to.
 *
 * Filters a data field with Kuwahara filter.
 **/
void
gwy_data_field_filter_kuwahara(GwyDataField *data_field)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_filter_kuwahara(data_field, 0, 0,
                                        data_field->xres, data_field->yres);
}

/**
 * gwy_data_field_shade:
 * @data_field: A data field.
 * @target_field: A data field to put the shade to.  It will be resized to
 *                match @data_field.
 * @theta: Shading angle (in radians, from north pole).
 * @phi: Shade orientation in xy plane (in radians, counterclockwise).
 *
 * Shades a data field.
 **/
void
gwy_data_field_shade(GwyDataField *data_field,
                     GwyDataField *target_field,
                     gdouble theta, gdouble phi)
{
    gint i, j;
    gdouble max, maxval, v;
    gdouble *data;

    gwy_data_field_resample(target_field, data_field->xres, data_field->yres,
                            GWY_INTERPOLATION_NONE);

    max = -G_MAXDOUBLE;
    data = target_field->data;
    for (i = 0; i < data_field->yres; i++) {

        for (j = 0; j < data_field->xres; j++) {
            v = -gwy_data_field_get_angder(data_field, j, i, phi);
            data[j + data_field->xres*i] = v;

            if (max < v)
                max = v;
        }
    }

    maxval = theta/max;
    for (i = 0; i < data_field->xres*data_field->yres; i++)
        data[i] = max - fabs(maxval - data[i]);

    gwy_data_field_invalidate(target_field);
}

void
gwy_data_field_filter_harris(GwyDataField *x_gradient,
                             GwyDataField *y_gradient,
                             GwyDataField *result,
                             gint neighbourhood,
                             gdouble alpha)
{

    gdouble pxx, pxy, pyy, det, trace, mult;
    gint height, width, i, j, ndata, k;
    GwyDataField *xkernel, *ykernel, *xx, *xy, *yy;
    gdouble *xdata, *ydata;
    gdouble sigma, vx, vy, u;

    g_return_if_fail(GWY_IS_DATA_FIELD(x_gradient));
    g_return_if_fail(GWY_IS_DATA_FIELD(y_gradient));
    g_return_if_fail(GWY_IS_DATA_FIELD(result));

    gwy_data_field_clear(result);
    g_return_if_fail(neighbourhood > 0);

    height = gwy_data_field_get_xres(x_gradient);
    width = gwy_data_field_get_yres(x_gradient);

    sigma = neighbourhood/5.0;
    ndata = neighbourhood*2;
    xkernel = gwy_data_field_new(ndata, 1, ndata, 1, FALSE);
    ykernel = gwy_data_field_new(1, ndata, 1, ndata, FALSE);
    xdata = gwy_data_field_get_data(xkernel);
    ydata = gwy_data_field_get_data(ykernel);

    mult = fabs(gwy_data_field_get_max(x_gradient)
                - gwy_data_field_get_min(x_gradient));
    mult += fabs(gwy_data_field_get_max(y_gradient)
                 - gwy_data_field_get_min(y_gradient));
    mult = 1.0/(mult*mult);

    xx = gwy_data_field_new_alike(result, TRUE);
    xy = gwy_data_field_new_alike(result, TRUE);
    yy = gwy_data_field_new_alike(result, TRUE);

    for (i = neighbourhood; i < height - neighbourhood; i++) {
         for (j = neighbourhood; j < width - neighbourhood; j++) {
             k = i*width + j;
             vx = x_gradient->data[k];
             vy = y_gradient->data[k];
             xx->data[k] = vx*vx*mult;
             xy->data[k] = vx*vy*mult;
             yy->data[k] = vy*vy*mult;
         }
    }

    for (k = 0; k < ndata; k++) {
        u = (k - ndata/2.0)/sigma;
        xdata[k] = ydata[k] = 0.5/G_PI/sigma/sigma*exp(-u*u/2.0);
    }
    gwy_data_field_convolve(xx, xkernel);
    gwy_data_field_convolve(xx, ykernel);
    gwy_data_field_convolve(xy, xkernel);
    gwy_data_field_convolve(xy, ykernel);
    gwy_data_field_convolve(yy, xkernel);
    gwy_data_field_convolve(yy, ykernel);

    for (i = neighbourhood; i < height - neighbourhood; i++) {
         for (j = neighbourhood; j < width - neighbourhood; j++) {
             k = i*width + j;
             pxx = xx->data[k];
             pxy = xy->data[k];
             pyy = yy->data[k];
             det = pxx*pyy - pxy*pxy;
             trace = pxx + pyy;
             result->data[k] = det - alpha*trace*trace;
          }
    }
    gwy_data_field_invalidate(result);
    g_object_unref(xkernel);
    g_object_unref(ykernel);
    g_object_unref(xx);
    g_object_unref(xy);
    g_object_unref(yy);
}

/************************** Documentation ****************************/

/**
 * SECTION:filters
 * @title: filters
 * @short_description: Convolution and other 2D data filters
 *
 * Filters are point-wise operations, such as thresholding, or generally local
 * operations producing a value based on the data in the vicinity of each
 * point: gradients, step detectors and convolutions.  Some simple common
 * point-wise operations, e.g. value inversion, are also found in base
 * #GwyDataField methods.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
