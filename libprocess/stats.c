/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libprocess/grains.h>
#include <libprocess/inttrans.h>

/* Cache operations */
#define CVAL(datafield, b)  ((datafield)->cache[GWY_DATA_FIELD_CACHE_##b])
#define CBIT(b)             (1 << GWY_DATA_FIELD_CACHE_##b)
#define CTEST(datafield, b) ((datafield)->cached & CBIT(b))

/**
 * gwy_data_field_get_max:
 * @data_field: A data field.
 *
 * Finds the maximum value of a data field.
 *
 * This quantity is cached.
 *
 * Returns: The maximum value.
 **/
gdouble
gwy_data_field_get_max(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), -G_MAXDOUBLE);
    gwy_debug("%s", CTEST(data_field, MAX) ? "cache" : "lame");

    if (!CTEST(data_field, MAX)) {
        const gdouble *p;
        gdouble max;
        gint i;

        max = data_field->data[0];
        p = data_field->data;
        for (i = data_field->xres * data_field->yres; i; i--, p++) {
            if (G_UNLIKELY(max < *p))
                max = *p;
        }
        CVAL(data_field, MAX) = max;
        data_field->cached |= CBIT(MAX);
    }

    return CVAL(data_field, MAX);
}


/**
 * gwy_data_field_area_get_max:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds the maximum value in a rectangular part of a data field.
 *
 * Returns: The maximum value.  When the number of samples to calculate
 *          maximum of is zero, -%G_MAXDOUBLE is returned.
 **/
gdouble
gwy_data_field_area_get_max(GwyDataField *dfield,
                            GwyDataField *mask,
                            gint col, gint row,
                            gint width, gint height)
{
    gint i, j;
    gdouble max = -G_MAXDOUBLE;
    const gdouble *datapos, *mpos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), max);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         max);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         max);
    if (!width || !height)
        return max;

    if (mask) {
        datapos = dfield->data + row*dfield->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (G_UNLIKELY(max < *drow) && *mrow > 0.0)
                    max = *drow;
                drow++;
                mrow++;
            }
        }

        return max;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_max(dfield);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            if (G_UNLIKELY(max < *drow))
                max = *drow;
            drow++;
        }
    }

    return max;
}

/**
 * gwy_data_field_get_min:
 * @data_field: A data field.
 *
 * Finds the minimum value of a data field.
 *
 * This quantity is cached.
 *
 * Returns: The minimum value.
 **/
gdouble
gwy_data_field_get_min(GwyDataField *data_field)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), -G_MAXDOUBLE);
    gwy_debug("%s", CTEST(data_field, MIN) ? "cache" : "lame");

    if (!CTEST(data_field, MIN)) {
        gdouble min;
        const gdouble *p;
        gint i;

        min = data_field->data[0];
        p = data_field->data;
        for (i = data_field->xres * data_field->yres; i; i--, p++) {
            if (G_UNLIKELY(min > *p))
                min = *p;
        }
        CVAL(data_field, MIN) = min;
        data_field->cached |= CBIT(MIN);
    }

    return CVAL(data_field, MIN);
}


/**
 * gwy_data_field_area_get_min:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds the minimum value in a rectangular part of a data field.
 *
 * Returns: The minimum value.  When the number of samples to calculate
 *          minimum of is zero, -%G_MAXDOUBLE is returned.
 **/
gdouble
gwy_data_field_area_get_min(GwyDataField *dfield,
                            GwyDataField *mask,
                            gint col, gint row,
                            gint width, gint height)
{
    gint i, j;
    gdouble min = G_MAXDOUBLE;
    const gdouble *datapos, *mpos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), min);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         min);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         min);
    if (!width || !height)
        return min;

    if (mask) {
        datapos = dfield->data + row*dfield->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (G_UNLIKELY(min > *drow) && *mrow > 0.0)
                    min = *drow;
                drow++;
                mrow++;
            }
        }

        return min;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_min(dfield);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            if (G_UNLIKELY(min > *drow))
                min = *drow;
            drow++;
        }
    }

    return min;
}

/**
 * gwy_data_field_get_min_max:
 * @data_field: A data field.
 * @min: Location to store minimum to.
 * @max: Location to store maximum to.
 *
 * Finds minimum and maximum values of a data field.
 **/
void
gwy_data_field_get_min_max(GwyDataField *data_field,
                           gdouble *min,
                           gdouble *max)
{
    gboolean need_min = FALSE, need_max = FALSE;
    gdouble min1, max1;
    const gdouble *p;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    if (min) {
        if (CTEST(data_field, MIN))
            *min = CVAL(data_field, MIN);
        else
            need_min = TRUE;
    }
    if (max) {
        if (CTEST(data_field, MAX))
            *max = CVAL(data_field, MAX);
        else
            need_max = TRUE;
    }

    if (!need_max && !need_max)
        return;
    else if (!need_min) {
        *max = gwy_data_field_get_max(data_field);
        return;
    }
    else if (!need_max) {
        *min = gwy_data_field_get_min(data_field);
        return;
    }

    min1 = data_field->data[0];
    max1 = data_field->data[0];
    p = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++) {
        if (G_UNLIKELY(min1 > *p))
            min1 = *p;
        if (G_UNLIKELY(max1 < *p))
            max1 = *p;
    }

    *min = min1;
    *max = max1;
    CVAL(data_field, MIN) = min1;
    CVAL(data_field, MAX) = max1;
    data_field->cached |= CBIT(MIN) | CBIT(MAX);
}

/**
 * gwy_data_field_area_get_min_max:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @min: Location to store minimum to.
 * @max: Location to store maximum to.
 *
 * Finds minimum and maximum values in a rectangular part of a data field.
 **/
void
gwy_data_field_area_get_min_max(GwyDataField *data_field,
                                GwyDataField *mask,
                                gint col, gint row,
                                gint width, gint height,
                                gdouble *min,
                                gdouble *max)
{
    gdouble min1 = G_MAXDOUBLE, max1 = -G_MAXDOUBLE;
    const gdouble *datapos, *mpos;
    gint i, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                               && mask->xres == data_field->xres
                               && mask->yres == data_field->yres));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    if (!width || !height) {
        if (min)
            *min = min1;
        if (max)
            *max = max1;
        return;
    }

    if (!min && !max)
        return;

    if (mask) {
        if (!min) {
            *max = gwy_data_field_area_get_max(data_field, mask,
                                               col, row, width, height);
            return;
        }
        if (!max) {
            *min = gwy_data_field_area_get_min(data_field, mask,
                                               col, row, width, height);
            return;
        }

        datapos = data_field->data + row*data_field->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*data_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (G_UNLIKELY(min1 > *drow) && *mrow > 0.0)
                    min1 = *drow;
                if (G_UNLIKELY(max1 < *drow) && *mrow > 0.0)
                    max1 = *drow;
                drow++;
                mrow++;
            }
        }
        *min = min1;
        *max = max1;

        return;
    }

    if (col == 0 && width == data_field->xres
        && row == 0 && height == data_field->yres) {
        gwy_data_field_get_min_max(data_field, min, max);
        return;
    }

    if (!min) {
        *max = gwy_data_field_area_get_max(data_field, NULL,
                                           col, row, width, height);
        return;
    }
    if (!max) {
        *min = gwy_data_field_area_get_min(data_field, NULL,
                                           col, row, width, height);
        return;
    }

    datapos = data_field->data + row*data_field->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*data_field->xres;

        for (j = 0; j < width; j++) {
            if (G_UNLIKELY(min1 > *drow))
                min1 = *drow;
            if (G_UNLIKELY(max1 < *drow))
                max1 = *drow;
            drow++;
        }
    }

    *min = min1;
    *max = max1;
}

/**
 * gwy_data_field_get_sum:
 * @data_field: A data field.
 *
 * Sums all values in a data field.
 *
 * This quantity is cached.
 *
 * Returns: The sum of all values.
 **/
gdouble
gwy_data_field_get_sum(GwyDataField *data_field)
{
    gint i;
    gdouble sum = 0;
    const gdouble *p;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), sum);

    gwy_debug("%s", CTEST(data_field, SUM) ? "cache" : "lame");
    if (CTEST(data_field, SUM))
        return CVAL(data_field, SUM);

    p = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++)
        sum += *p;

    CVAL(data_field, SUM) = sum;
    data_field->cached |= CBIT(SUM);

    return sum;
}

/**
 * gwy_data_field_area_get_sum:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Sums values of a rectangular part of a data field.
 *
 * Returns: The sum of all values inside area.
 **/
gdouble
gwy_data_field_area_get_sum(GwyDataField *dfield,
                            GwyDataField *mask,
                            gint col, gint row,
                            gint width, gint height)
{
    gint i, j;
    gdouble sum = 0;
    const gdouble *datapos, *mpos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);

    if (mask) {
        datapos = dfield->data + row*dfield->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (*mrow > 0.0)
                    sum += *drow;
                drow++;
                mrow++;
            }
        }

        return sum;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_sum(dfield);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++)
            sum += *(drow++);
    }

    return sum;
}

/**
 * gwy_data_field_get_avg:
 * @data_field: A data field
 *
 * Computes average value of a data field.
 *
 * This quantity is cached.
 *
 * Returns: The average value.
 **/
gdouble
gwy_data_field_get_avg(GwyDataField *data_field)
{
    return gwy_data_field_get_sum(data_field)/((data_field->xres
                                                * data_field->yres));
}

/**
 * gwy_data_field_area_get_avg:
 * @data_field: A data field
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes average value of a rectangular part of a data field.
 *
 * Returns: The average value.
 **/
gdouble
gwy_data_field_area_get_avg(GwyDataField *dfield,
                            GwyDataField *mask,
                            gint col, gint row,
                            gint width, gint height)
{
    gint i, j, nn;
    gdouble sum = 0;
    const gdouble *datapos, *mpos;

    if (!mask) {
        return gwy_data_field_area_get_sum(dfield, mask,
                                           col, row,
                                           width, height)/(width*height);
    }

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(mask)
                         && mask->xres == dfield->xres
                         && mask->yres == dfield->yres,
                         sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);

    datapos = dfield->data + row*dfield->xres + col;
    mpos = mask->data + row*mask->xres + col;
    nn = 0;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;
        const gdouble *mrow = mpos + i*mask->xres;

        for (j = 0; j < width; j++) {
            if (*mrow > 0.0) {
                sum += *drow;
                nn++;
            }
            drow++;
            mrow++;
        }
    }

    return sum/nn;
}

/**
 * gwy_data_field_get_rms:
 * @data_field: A data field.
 *
 * Computes root mean square value of a data field.
 *
 * This quantity is cached.
 *
 * Returns: The root mean square value.
 **/
gdouble
gwy_data_field_get_rms(GwyDataField *data_field)
{
    gint i, n;
    gdouble rms = 0.0, sum, sum2;
    const gdouble *p;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), rms);

    gwy_debug("%s", CTEST(data_field, RMS) ? "cache" : "lame");
    if (CTEST(data_field, RMS))
        return CVAL(data_field, RMS);

    sum = gwy_data_field_get_sum(data_field);
    sum2 = 0.0;
    p = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++)
        sum2 += (*p)*(*p);

    n = data_field->xres * data_field->yres;
    rms = sqrt(fabs(sum2 - sum*sum/n)/n);

    CVAL(data_field, RMS) = rms;
    data_field->cached |= CBIT(RMS);

    return rms;
}

/**
 * gwy_data_field_area_get_rms:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes root mean square value of a rectangular part of a data field.
 *
 * Returns: The root mean square value.
 **/
gdouble
gwy_data_field_area_get_rms(GwyDataField *dfield,
                            GwyDataField *mask,
                            gint col, gint row,
                            gint width, gint height)
{
    gint i, j, n;
    gdouble rms = 0.0, sum2 = 0.0;
    gdouble sum;
    const gdouble *datapos, *mpos;

    if (width == 0 || height == 0)
        return rms;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), rms);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         rms);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         rms);
    if (!width || !height)
        return rms;

    if (mask) {
        sum = gwy_data_field_area_get_sum(dfield, mask,
                                          col, row, width, height);
        datapos = dfield->data + row*dfield->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (*mrow > 0.0)
                    sum2 += (*drow) * (*drow);
                drow++;
                mrow++;
            }
        }

        n = width*height;
        rms = sqrt(fabs(sum2 - sum*sum/n)/n);

        return rms;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_rms(dfield);

    sum = gwy_data_field_area_get_sum(dfield, NULL, col, row, width, height);
    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        const gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            sum2 += (*drow) * (*drow);
            drow++;
        }
    }

    n = width*height;
    rms = sqrt(fabs(sum2 - sum*sum/n)/n);

    return rms;
}

/**
 * gwy_data_field_get_autorange:
 * @data_field: A data field.
 * @from: Location to store range start.
 * @to: Location to store range end.
 *
 * Computes value range with outliers cut-off.
 *
 * The purpose of this function is to find a range is suitable for false color
 * mapping.  The precise method how it is calculated is unspecified and may be
 * subject to changes.
 *
 * However, it is guaranteed minimum <= @from <= @to <= maximum.
 *
 * This quantity is cached.
 **/
void
gwy_data_field_get_autorange(GwyDataField *data_field,
                             gdouble *from,
                             gdouble *to)
{
    enum { AR_NDH = 512 };
    guint dh[AR_NDH];
    gdouble min, max, rmin, rmax, q;
    gdouble *p;
    guint i, n, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    gwy_debug("%s", CTEST(data_field, ARF) ? "cache" : "lame");
    if ((!from || CTEST(data_field, ARF))
        && (!to || CTEST(data_field, ART))) {
        if (from)
            *from = CVAL(data_field, ARF);
        if (to)
            *to = CVAL(data_field, ART);
        return;
    }

    gwy_data_field_get_min_max(data_field, &min, &max);
    if (min == max) {
        rmin = min;
        rmax = max;
    }
    else {
        max += 1e-6*(max - min);
        q = AR_NDH/(max - min);

        n = data_field->xres*data_field->yres;
        memset(dh, 0, AR_NDH*sizeof(guint));
        for (i = n, p = data_field->data; i; i--, p++) {
            j = (*p - min)*q;    /* rounding toward zero is ok here */
            dh[j]++;
        }

        j = 0;
        for (i = j = 0; dh[i] < 5e-2*n/AR_NDH && j < 2e-2*n; i++)
            j += dh[i];
        rmin = min + i/q;

        j = 0;
        for (i = AR_NDH-1, j = 0; dh[i] < 5e-2*n/AR_NDH && j < 2e-2*n; i--)
            j += dh[i];
        rmax = min + (i + 1)/q;
    }

    if (from)
        *from = rmin;
    if (to)
        *to = rmax;

    CVAL(data_field, ARF) = rmin;
    CVAL(data_field, ART) = rmax;
    data_field->cached |= CBIT(ARF) | CBIT(ART);
}

/**
 * gwy_data_field_get_stats:
 * @data_field: A data field.
 * @avg: Where average height value of the surface should be stored, or %NULL.
 * @ra: Where average value of irregularities should be stored, or %NULL.
 * @rms: Where root mean square value of irregularities (Rq) should be stored,
 *       or %NULL.
 * @skew: Where skew (symmetry of height distribution) should be stored, or
 *        %NULL.
 * @kurtosis: Where kurtosis (peakedness of height ditribution) should be
 *            stored, or %NULL.
 *
 * Computes basic statistical quantities of a data field.
 **/
void
gwy_data_field_get_stats(GwyDataField *data_field,
                         gdouble *avg,
                         gdouble *ra,
                         gdouble *rms,
                         gdouble *skew,
                         gdouble *kurtosis)
{
    gint i;
    gdouble c_sz2, c_sz3, c_sz4, c_abs1;
    const gdouble *p = data_field->data;
    gdouble nn = data_field->xres * data_field->yres;
    gdouble dif, myavg, myrms;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    c_sz2 = c_sz3 = c_sz4 = c_abs1 = 0;

    myavg = gwy_data_field_get_avg(data_field);
    if (avg)
        *avg = myavg;

    for (i = nn; i; i--, p++) {
        dif = (*p - myavg);
        c_abs1 += fabs(dif);
        c_sz2 += dif*dif;
        c_sz3 += dif*dif*dif;
        c_sz4 += dif*dif*dif*dif;

    }

    myrms = c_sz2/nn;
    if (ra)
        *ra = c_abs1/nn;
    if (skew)
        *skew = c_sz3/pow(myrms, 1.5)/nn;
    if (kurtosis)
        *kurtosis = c_sz4/(myrms)/(myrms)/nn - 3;
    if (rms)
        *rms = sqrt(myrms);

    if (!CTEST(data_field, RMS)) {
        CVAL(data_field, RMS) = sqrt(myrms);
        data_field->cached |= CBIT(RMS);
    }
}

/**
 * gwy_data_field_area_get_stats:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @avg: Where average height value of the surface should be stored, or %NULL.
 * @ra: Where average value of irregularities should be stored, or %NULL.
 * @rms: Where root mean square value of irregularities (Rq) should be stored,
 *       or %NULL.
 * @skew: Where skew (symmetry of height distribution) should be stored, or
 *        %NULL.
 * @kurtosis: Where kurtosis (peakedness of height ditribution) should be
 *            stored, or %NULL.
 *
 * Computes basic statistical quantities of a rectangular part of a data field.
 **/
void
gwy_data_field_area_get_stats(GwyDataField *dfield,
                              GwyDataField *mask,
                              gint col, gint row,
                              gint width, gint height,
                              gdouble *avg,
                              gdouble *ra,
                              gdouble *rms,
                              gdouble *skew,
                              gdouble *kurtosis)
{
    gdouble c_sz2, c_sz3, c_sz4, c_abs1;
    gdouble nn, dif, myavg, myrms;
    const gdouble *datapos, *mpos;
    gint i, j;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                               && mask->xres == dfield->xres
                               && mask->yres == dfield->yres));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= dfield->xres
                     && row + height <= dfield->yres);

    c_sz2 = c_sz3 = c_sz4 = c_abs1 = 0;

    if (mask) {
        myavg = gwy_data_field_area_get_avg(dfield, mask,
                                            col, row, width, height);
        datapos = dfield->data + row*dfield->xres + col;
        mpos = mask->data + row*mask->xres + col;
        nn = 0;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (*mrow > 0.0) {
                    dif = *drow - myavg;
                    c_abs1 += fabs(dif);
                    c_sz2 += dif*dif;
                    c_sz3 += dif*dif*dif;
                    c_sz4 += dif*dif*dif*dif;
                    nn++;
                }
                drow++;
                mrow++;
            }
        }
    }
    else {
        nn = width*height;
        myavg = gwy_data_field_area_get_avg(dfield, NULL,
                                            col, row, width, height);
        datapos = dfield->data + row*dfield->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;

            for (j = 0; j < width; j++) {
                dif = *(drow++) - myavg;
                c_abs1 += fabs(dif);
                c_sz2 += dif*dif;
                c_sz3 += dif*dif*dif;
                c_sz4 += dif*dif*dif*dif;
            }
        }
    }

    myrms = c_sz2/nn;
    if (avg)
        *avg = myavg;
    if (ra)
        *ra = c_abs1/nn;
    if (skew)
        *skew = c_sz3/pow(myrms, 1.5)/nn;
    if (kurtosis)
        *kurtosis = c_sz4/(myrms)/(myrms)/nn - 3;
    if (rms)
        *rms = sqrt(myrms);
}

/**
 * gwy_data_field_area_count_in_range:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @below: Upper bound to compare data to.  The number of samples less
 *         than or equal to @below is stored in @nbelow.
 * @above: Lower bound to compare data to.  The number of samples greater
 *         than or equal to @above is stored in @nabove.
 * @nbelow: Location to store the number of samples less than or equal
 *          to @below, or %NULL.
 * @nabove: Location to store the number of samples greater than or equal
 *          to @above, or %NULL.
 *
 * Counts data samples in given range.
 *
 * No assertion is made about the values of @above and @below, in other words
 * @above may be larger than @below.  To count samples in an open interval
 * instead of a closed interval, exchange @below and @above and then subtract
 * the @nabove and @nbelow from @width*@height to get the complementary counts.
 *
 * With this trick the common task of counting positive values can be
 * realized:
 * <informalexample><programlisting>
 * gwy_data_field_area_count_in_range(data_field, NULL,
 *                                    col, row, width, height,
 *                                    0.0, 0.0, &amp;count, NULL);
 * count = width*height - count;
 * </programlisting></informalexample>
 **/
void
gwy_data_field_area_count_in_range(GwyDataField *data_field,
                                   GwyDataField *mask,
                                   gint col, gint row,
                                   gint width, gint height,
                                   gdouble below,
                                   gdouble above,
                                   gint *nbelow,
                                   gint *nabove)
{
    const gdouble *datapos, *mpos;
    gint i, j, na, nb;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                               && mask->xres == data_field->xres
                               && mask->yres == data_field->yres));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    if (!nabove && !nbelow)
        return;

    na = nb = 0;
    if (mask) {
        datapos = data_field->data + row*data_field->xres + col;
        mpos = mask->data + row*mask->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*data_field->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (*mrow > 0.0) {
                    if (*drow >= above)
                        na++;
                    if (*drow <= below)
                        nb++;
                }
                drow++;
                mrow++;
            }
        }
    }
    else {
        datapos = data_field->data + row*data_field->xres + col;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*data_field->xres;

            for (j = 0; j < width; j++) {
                if (*drow >= above)
                    na++;
                if (*drow <= below)
                    nb++;
                drow++;
            }
        }
    }

    if (nabove)
        *nabove = na;
    if (nbelow)
        *nbelow = nb;
}

/**
 * gwy_data_field_area_dh:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates distribution of heights in a rectangular part of data field.
 **/
void
gwy_data_field_area_dh(GwyDataField *data_field,
                       GwyDataField *mask,
                       GwyDataLine *target_line,
                       gint col, gint row,
                       gint width, gint height,
                       gint nstats)
{
    GwySIUnit *fieldunit, *lineunit;
    gdouble min, max;
    const gdouble *drow, *mrow;
    gint i, j, k, nn;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                               && mask->xres == data_field->xres
                               && mask->yres == data_field->yres));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 1 && height >= 1
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    if (mask) {
        nn = 0;
        for (i = 0; i < height; i++) {
            mrow = mask->data + (i + row)*mask->xres + col;
            for (j = 0; j < width; j++) {
                if (mrow[i])
                    nn++;
            }
        }
    }
    else
        nn = width*height;

    if (nstats < 1) {
        nstats = floor(3.49*cbrt(nn) + 0.5);
        nstats = MAX(nstats, 2);
    }

    gwy_data_line_resample(target_line, nstats, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);
    gwy_data_field_area_get_min_max(data_field, mask,
                                    col, row, width, height,
                                    &min, &max);

    /* Set proper units */
    fieldunit = gwy_data_field_get_si_unit_z(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_serializable_clone(G_OBJECT(fieldunit), G_OBJECT(lineunit));
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_set_from_string(lineunit, NULL);

    /* Handle border cases */
    if (min == max) {
        gwy_data_line_set_real(target_line, min ? max : 1.0);
        target_line->data[0] = nstats/gwy_data_line_get_real(target_line);
        return;
    }

    /* Calculate height distribution */
    gwy_data_line_set_real(target_line, max - min);
    gwy_data_line_set_offset(target_line, min);
    if (mask) {
        for (i = 0; i < height; i++) {
            drow = data_field->data + (i + row)*data_field->xres + col;
            mrow = mask->data + (i + row)*mask->xres + col;

            for (j = 0; j < width; j++) {
                if (mrow[j]) {
                    k = (gint)((drow[j] - min)/(max - min)*nstats);
                    /* Fix rounding errors */
                    if (G_UNLIKELY(k >= nstats))
                        k = nstats-1;

                    target_line->data[k] += 1;
                }
            }
        }
    }
    else {
        for (i = 0; i < height; i++) {
            drow = data_field->data + (i + row)*data_field->xres + col;

            for (j = 0; j < width; j++) {
                k = (gint)((drow[j] - min)/(max - min)*nstats);
                /* Fix rounding errors */
                if (G_UNLIKELY(k >= nstats))
                    k = nstats-1;

                target_line->data[k] += 1;
            }
        }
    }

    /* Normalize integral to 1 */
    gwy_data_line_multiply(target_line, nstats/(max - min)/nn);
}

/**
 * gwy_data_field_dh:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates distribution of heights in a data field.
 **/
void
gwy_data_field_dh(GwyDataField *data_field,
                  GwyDataLine *target_line,
                  gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_dh(data_field, NULL, target_line,
                           0, 0, data_field->xres, data_field->yres,
                           nstats);
}

/**
 * gwy_data_field_area_cdh:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates cumulative distribution of heights in a rectangular part of data
 * field.
 **/
void
gwy_data_field_area_cdh(GwyDataField *data_field,
                        GwyDataField *mask,
                        GwyDataLine *target_line,
                        gint col, gint row,
                        gint width, gint height,
                        gint nstats)
{
    gwy_data_field_area_dh(data_field, mask, target_line,
                           col, row, width, height,
                           nstats);
    gwy_data_line_cumulate(target_line);
    gwy_data_line_multiply(target_line, gwy_data_line_itor(target_line, 1));
}

/**
 * gwy_data_field_cdh:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates cumulative distribution of heights in a data field.
 **/
void
gwy_data_field_cdh(GwyDataField *data_field,
                   GwyDataLine *target_line,
                   gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_dh(data_field, NULL, target_line,
                           0, 0, data_field->xres, data_field->yres,
                           nstats);
    gwy_data_line_cumulate(target_line);
    gwy_data_line_multiply(target_line, gwy_data_line_itor(target_line, 1));
}

/**
 * gwy_data_field_area_da:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @orientation: Orientation to compute the slope distribution in.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates distribution of slopes in a rectangular part of data field.
 **/
void
gwy_data_field_area_da(GwyDataField *data_field,
                       GwyDataLine *target_line,
                       gint col, gint row,
                       gint width, gint height,
                       GwyOrientation orientation,
                       gint nstats)
{
    GwySIUnit *lineunit;
    GwyDataField *der;
    const gdouble *drow;
    gdouble *derrow;
    gdouble q;
    gint xres, yres, i, j, size;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    xres = data_field->xres;
    yres = data_field->yres;
    size = (orientation == GWY_ORIENTATION_HORIZONTAL) ? width : height;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 1 && height >= 1
                     && size >= 2
                     && col + width <= xres
                     && row + height <= yres);

    /* Create a temporary data field from horizontal/vertical derivations
     * and then simply use gwy_data_field_dh().
     * XXX: Should not such a thing exist as a public method? */
    der = gwy_data_field_new(width, height,
                             data_field->xreal*width/xres,
                             data_field->yreal*height/yres,
                             FALSE);

    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        q = xres/data_field->xreal;
        /* Instead of testing border columns in each gwy_data_field_get_xder()
         * call, special-case them explicitely */
        for (i = 0; i < height; i++) {
            drow = data_field->data + (i + row)*xres + col;
            derrow = der->data + i*width;

            derrow[0] = drow[1] - drow[0];
            for (j = 1; j < width-1; j++)
                derrow[j] = (drow[j+1] - drow[j-1])/2.0;
            if (width > 1)
                derrow[j] = drow[width-1] - drow[width-2];
        }
        break;

        case GWY_ORIENTATION_VERTICAL:
        q = yres/data_field->yreal;
        /* Instead of testing border rows in each gwy_data_field_get_yder()
         * call, special-case them explicitely */
        drow = data_field->data + row*xres + col;
        derrow = der->data;
        for (j = 0; j < width; j++)
            derrow[j] = *(drow + j+xres) - *(drow + j);

        for (i = 1; i < height-1; i++) {
            drow = data_field->data + (i + row)*xres + col;
            derrow = der->data + i*width;

            for (j = 0; j < width; j++)
                derrow[j] = (*(drow + j+xres) - *(drow + j-xres))/2.0;
        }

        if (height > 1) {
            drow = data_field->data + (row + height-1)*xres + col;
            derrow = der->data + (height-1)*width;
            for (j = 0; j < width; j++)
                derrow[j] = *(drow + j) - *(drow + j-xres);
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gwy_data_field_dh(der, target_line, nstats);
    /* Fix derivation normalization.  At the same time we have to multiply
     * target_line values with inverse factor to keep integral intact */
    gwy_data_line_set_real(target_line, q*gwy_data_line_get_real(target_line));
    gwy_data_line_set_offset(target_line,
                             q*gwy_data_line_get_offset(target_line));
    gwy_data_line_multiply(target_line, 1.0/q);
    g_object_unref(der);

    /* Set proper units */
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_si_unit_divide(gwy_data_field_get_si_unit_z(data_field),
                       gwy_data_field_get_si_unit_xy(data_field),
                       lineunit);
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_set_from_string(lineunit, NULL);
}

/**
 * gwy_data_field_da:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @orientation: Orientation to compute the slope distribution in.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates distribution of slopes in a data field.
 **/
void
gwy_data_field_da(GwyDataField *data_field,
                  GwyDataLine *target_line,
                  GwyOrientation orientation,
                  gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_da(data_field, target_line,
                           0, 0, data_field->xres, data_field->yres,
                           orientation, nstats);
}

/**
 * gwy_data_field_area_cda:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @orientation: Orientation to compute the slope distribution in.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates cumulative distribution of slopes in a rectangular part of data
 * field.
 **/
void
gwy_data_field_area_cda(GwyDataField *data_field,
                        GwyDataLine *target_line,
                        gint col, gint row,
                        gint width, gint height,
                        GwyOrientation orientation,
                        gint nstats)
{
    gwy_data_field_area_da(data_field, target_line,
                           col, row, width, height,
                           orientation, nstats);
    gwy_data_line_cumulate(target_line);
    gwy_data_line_multiply(target_line, gwy_data_line_itor(target_line, 1));
}

/**
 * gwy_data_field_cda:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @orientation: Orientation to compute the slope distribution in.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates cumulative distribution of slopes in a data field.
 **/
void
gwy_data_field_cda(GwyDataField *data_field,
                   GwyDataLine *target_line,
                   GwyOrientation orientation,
                   gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_da(data_field, target_line,
                           0, 0, data_field->xres, data_field->yres,
                           orientation, nstats);
    gwy_data_line_cumulate(target_line);
    gwy_data_line_multiply(target_line, gwy_data_line_itor(target_line, 1));
}

#ifdef HAVE_FFTW3
typedef void (*GwyFFTAreaFunc)(fftw_plan plan,
                               GwyDataLine *din,
                               GwyDataLine *dout,
                               GwyDataLine *target_line);

static inline void
do_fft_acf(fftw_plan plan,
           GwyDataLine *din,
           GwyDataLine *dout,
           GwyDataLine *target_line)
{
    gdouble *in, *out;
    gint j, width, res;

    width = target_line->res;
    res = din->res;
    in = din->data;
    out = dout->data;

    memset(in + width, 0, (res - width)*sizeof(gdouble));

    fftw_execute(plan);
    in[0] = out[0]*out[0];
    for (j = 1; j < (res + 1)/2; j++)
        in[j] = in[res-j] = out[j]*out[j] + out[res-j]*out[res-j];
    if (!(res % 2))
        in[res/2] = out[res/2]*out[res/2];

    fftw_execute(plan);
    for (j = 0; j < width; j++)
        target_line->data[j] += out[j]/(width - j);
}

static inline void
do_fft_hhcf(fftw_plan plan,
            GwyDataLine *din,
            GwyDataLine *dout,
            GwyDataLine *target_line)
{
    gdouble *in, *out;
    gdouble sum;
    gint j, width, res;

    width = target_line->res;
    res = din->res;
    in = din->data;
    out = dout->data;

    sum = 0.0;
    for (j = 0; j < width; j++) {
        sum += in[j]*in[j] + in[width-1-j]*in[width-1-j];
        target_line->data[width-1-j] += sum*res/(j+1);
    }

    memset(in + width, 0, (res - width)*sizeof(gdouble));

    fftw_execute(plan);
    in[0] = out[0]*out[0];
    for (j = 1; j < (res + 1)/2; j++)
        in[j] = in[res-j] = out[j]*out[j] + out[res-j]*out[res-j];
    if (!(res % 2))
        in[res/2] = out[res/2]*out[res/2];

    fftw_execute(plan);
    for (j = 0; j < width; j++)
        target_line->data[j] -= 2*out[j]/(width - j);
}

static void
gwy_data_field_area_func_fft(GwyDataField *data_field,
                             GwyDataLine *target_line,
                             GwyFFTAreaFunc func,
                             gint col, gint row,
                             gint width, gint height,
                             GwyOrientation orientation,
                             GwyInterpolationType interpolation,
                             gint nstats)
{
    GwyDataLine *din, *dout;
    fftw_plan plan;
    gdouble *in, *out, *drow, *dcol;
    gint i, j, xres, yres, res = 0;
    gdouble avg;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    xres = data_field->xres;
    yres = data_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 1 && height >= 1
                     && col + width <= xres
                     && row + height <= yres);
    g_return_if_fail(orientation == GWY_ORIENTATION_HORIZONTAL
                     || orientation == GWY_ORIENTATION_VERTICAL);

    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        res = gwy_fft_find_nice_size(2*xres);
        gwy_data_line_resample(target_line, width, GWY_INTERPOLATION_NONE);
        break;

        case GWY_ORIENTATION_VERTICAL:
        res = gwy_fft_find_nice_size(2*yres);
        gwy_data_line_resample(target_line, height, GWY_INTERPOLATION_NONE);
        break;
    }
    gwy_data_line_clear(target_line);
    gwy_data_line_set_offset(target_line, 0.0);

    din = gwy_data_line_new(res, 1.0, FALSE);
    dout = gwy_data_line_new(res, 1.0, FALSE);
    in = gwy_data_line_get_data(din);
    out = gwy_data_line_get_data(dout);
    plan = fftw_plan_r2r_1d(res, in, out, FFTW_R2HC, FFTW_MEASURE);
    g_return_if_fail(plan);

    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        for (i = 0; i < height; i++) {
            drow = data_field->data + (i + row)*xres + col;
            avg = gwy_data_field_area_get_avg(data_field, NULL,
                                              col, row+i, width, 1);
            for (j = 0; j < width; j++)
                in[j] = drow[j] - avg;
            func(plan, din, dout, target_line);
        }
        gwy_data_line_set_real(target_line,
                               gwy_data_field_jtor(data_field, width));
        gwy_data_line_multiply(target_line, 1.0/(res*height));
        break;

        case GWY_ORIENTATION_VERTICAL:
        for (i = 0; i < width; i++) {
            dcol = data_field->data + row*xres + (i + col);
            avg = gwy_data_field_area_get_avg(data_field, NULL,
                                              col+i, row, 1, height);
            for (j = 0; j < height; j++)
                in[j] = dcol[j*xres] - avg;
            func(plan, din, dout, target_line);
        }
        gwy_data_line_set_real(target_line,
                               gwy_data_field_itor(data_field, height));
        gwy_data_line_multiply(target_line, 1.0/(res*width));
        break;
    }

    fftw_destroy_plan(plan);
    g_object_unref(din);
    g_object_unref(dout);

    if (nstats > 0)
        gwy_data_line_resample(target_line, nstats, interpolation);
}
#else  /* HAVE_FFTW3 */
typedef void (*GwyLameAreaFunc)(GwyDataLine *source,
                                GwyDataLine *target);

static void
gwy_data_field_area_func_lame(GwyDataField *data_field,
                              GwyDataLine *target_line,
                              GwyLameAreaFunc func,
                              gint col, gint row,
                              gint width, gint height,
                              GwyOrientation orientation,
                              GwyInterpolationType interpolation,
                              gint nstats)
{
    GwyDataLine *data_line, *tmp_line;
    gint i, j, xres, yres, size;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    xres = data_field->xres;
    yres = data_field->yres;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 1 && height >= 1
                     && col + width <= xres
                     && row + height <= yres);
    g_return_if_fail(orientation == GWY_ORIENTATION_HORIZONTAL
                     || orientation == GWY_ORIENTATION_VERTICAL);

    size = (orientation == GWY_ORIENTATION_HORIZONTAL) ? width : height;
    data_line = gwy_data_line_new(size, 1.0, FALSE);
    tmp_line = gwy_data_line_new(size, 1.0, FALSE);
    gwy_data_line_resample(target_line, size, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);
    gwy_data_line_set_offset(target_line, 0.0);

    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        for (i = 0; i < height; i++) {
            gwy_data_field_get_row_part(data_field, data_line, row+i,
                                        col, col+width);
            func(data_line, tmp_line);
            for (j = 0; j < width; j++)
                target_line->data[j] += tmp_line->data[j];
        }
        gwy_data_line_set_real(target_line,
                               gwy_data_field_jtor(data_field, width));
        gwy_data_line_multiply(target_line, 1.0/height);
        break;

        case GWY_ORIENTATION_VERTICAL:
        for (i = 0; i < width; i++) {
            gwy_data_field_get_column_part(data_field, data_line, col+i,
                                           row, row+height);
            func(data_line, tmp_line);
            for (j = 0; j < height; j++)
                target_line->data[j] += tmp_line->data[j];
        }
        gwy_data_line_set_real(target_line,
                               gwy_data_field_itor(data_field, height));
        gwy_data_line_multiply(target_line, 1.0/width);
        break;
    }

    g_object_unref(data_line);
    g_object_unref(tmp_line);

    if (nstats > 0)
        gwy_data_line_resample(target_line, nstats, interpolation);
}
#endif  /* HAVE_FFTW3 */

/**
 * gwy_data_field_area_acf:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @orientation: Orientation of lines (ACF is simply averaged over the
 *               other orientation).
 * @interpolation: Interpolation to use when @nstats is given and requires
 *                 resampling.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, @width (@height) is used.
 *
 * Calculates one-dimensional autocorrelation function of a rectangular part of
 * a data field.
 **/
void
gwy_data_field_area_acf(GwyDataField *data_field,
                        GwyDataLine *target_line,
                        gint col, gint row,
                        gint width, gint height,
                        GwyOrientation orientation,
                        GwyInterpolationType interpolation,
                        gint nstats)
{
    GwySIUnit *fieldunit, *lineunit;

#ifdef HAVE_FFTW3
    gwy_data_field_area_func_fft(data_field, target_line,
                                 &do_fft_acf,
                                 col, row, width, height,
                                 orientation, interpolation, nstats);
#else
    gwy_data_field_area_func_lame(data_field, target_line,
                                  &gwy_data_line_acf,
                                  col, row, width, height,
                                  orientation, interpolation, nstats);
#endif  /* HAVE_FFTW3 */

    /* Set proper units */
    fieldunit = gwy_data_field_get_si_unit_xy(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_serializable_clone(G_OBJECT(fieldunit), G_OBJECT(lineunit));
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_power(gwy_data_field_get_si_unit_z(data_field), 2, lineunit);
}

/**
 * gwy_data_field_acf:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @orientation: Orientation of lines (ACF is simply averaged over the
 *               other orientation).
 * @interpolation: Interpolation to use when @nstats is given and requires
 *                 resampling.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, data field width (height) is used.
 *
 * Calculates one-dimensional autocorrelation function of a data field.
 **/
void
gwy_data_field_acf(GwyDataField *data_field,
                   GwyDataLine *target_line,
                   GwyOrientation orientation,
                   GwyInterpolationType interpolation,
                   gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_acf(data_field, target_line,
                            0, 0, data_field->xres, data_field->yres,
                            orientation, interpolation, nstats);
}

/**
 * gwy_data_field_area_hhcf:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @orientation: Orientation of lines (HHCF is simply averaged over the
 *               other orientation).
 * @interpolation: Interpolation to use when @nstats is given and requires
 *                 resampling.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, @width (@height) is used.
 *
 * Calculates one-dimensional autocorrelation function of a rectangular part of
 * a data field.
 **/
void
gwy_data_field_area_hhcf(GwyDataField *data_field,
                         GwyDataLine *target_line,
                         gint col, gint row,
                         gint width, gint height,
                         GwyOrientation orientation,
                         GwyInterpolationType interpolation,
                         gint nstats)
{
    GwySIUnit *fieldunit, *lineunit;

#ifdef HAVE_FFTW3
    gwy_data_field_area_func_fft(data_field, target_line, &do_fft_hhcf,
                                 col, row, width, height,
                                 orientation, interpolation, nstats);
#else
    gwy_data_field_area_func_lame(data_field, target_line,
                                  &gwy_data_line_hhcf,
                                  col, row, width, height,
                                  orientation, interpolation, nstats);
#endif  /* HAVE_FFTW3 */

    /* Set proper units */
    fieldunit = gwy_data_field_get_si_unit_xy(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_serializable_clone(G_OBJECT(fieldunit), G_OBJECT(lineunit));
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_power(gwy_data_field_get_si_unit_z(data_field), 2, lineunit);
}

/**
 * gwy_data_field_hhcf:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @orientation: Orientation of lines (HHCF is simply averaged over the
 *               other orientation).
 * @interpolation: Interpolation to use when @nstats is given and requires
 *                 resampling.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, data field width (height) is used.
 *
 * Calculates one-dimensional autocorrelation function of a data field.
 **/
void
gwy_data_field_hhcf(GwyDataField *data_field,
                    GwyDataLine *target_line,
                    GwyOrientation orientation,
                    GwyInterpolationType interpolation,
                    gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_hhcf(data_field, target_line,
                             0, 0, data_field->xres, data_field->yres,
                             orientation, interpolation, nstats);
}

/**
 * gwy_data_field_area_psdf:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @orientation: Orientation of lines (PSDF is simply averaged over the
 *               other orientation).
 * @interpolation: Interpolation to use when @nstats is given and requires
 *                 resampling (and possibly in FFT too).
 * @windowing: Windowing type to use.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, data field width (height) is used.
 *
 * Calculates one-dimensional power spectrum density function of a rectangular
 * part of a data field.
 **/
void
gwy_data_field_area_psdf(GwyDataField *data_field,
                         GwyDataLine *target_line,
                         gint col, gint row,
                         gint width, gint height,
                         GwyOrientation orientation,
                         GwyInterpolationType interpolation,
                         GwyWindowingType windowing,
                         gint nstats)
{
    GwyDataField *re_field, *im_field;
    GwySIUnit *xyunit, *zunit, *lineunit;
    gdouble *re, *im, *target;
    gint i, j, xres, yres, size;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    xres = data_field->xres;
    yres = data_field->yres;
    size = (orientation == GWY_ORIENTATION_HORIZONTAL) ? width : height;
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 1 && height >= 1
                     && size >= 4
                     && col + width <= xres
                     && row + height <= yres);
    g_return_if_fail(orientation == GWY_ORIENTATION_HORIZONTAL
                     || orientation == GWY_ORIENTATION_VERTICAL);

    if (nstats < 0)
        nstats = size/2;
    gwy_data_line_resample(target_line, size/2, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);
    gwy_data_line_set_offset(target_line, 0.0);

    re_field = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);
    im_field = gwy_data_field_new(width, height, 1.0, 1.0, FALSE);
    target = target_line->data;
    switch (orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        gwy_data_field_area_1dfft(data_field, NULL, re_field, im_field,
                                  col, row, width, height,
                                  orientation,
                                  windowing,
                                  GWY_TRANSFORM_DIRECTION_FORWARD,
                                  interpolation,
                                  TRUE, 2);
        re = re_field->data;
        im = im_field->data;
        for (i = 0; i < height; i++) {
            for (j = 0; j < size/2; j++)
                target[j] += re[i*width + j]*re[i*width + j]
                             + im[i*width + j]*im[i*width + j];
        }
        gwy_data_line_multiply(target_line,
                               data_field->xreal/xres/(2*G_PI*height));
        gwy_data_line_set_real(target_line, G_PI*xres/data_field->xreal);
        break;

        case GWY_ORIENTATION_VERTICAL:
        gwy_data_field_area_1dfft(data_field, NULL, re_field, im_field,
                                  col, row, width, height,
                                  orientation,
                                  windowing,
                                  GWY_TRANSFORM_DIRECTION_FORWARD,
                                  interpolation,
                                  TRUE, 2);
        re = re_field->data;
        im = im_field->data;
        for (i = 0; i < width; i++) {
            for (j = 0; j < size/2; j++)
                target[j] += re[j*width + i]*re[j*width + i]
                             + im[j*width + i]*im[j*width + i];
        }
        gwy_data_line_multiply(target_line,
                               data_field->yreal/yres/(2*G_PI*width));
        gwy_data_line_set_real(target_line, G_PI*yres/data_field->yreal);
        break;
    }

    gwy_data_line_resample(target_line, nstats, interpolation);

    g_object_unref(re_field);
    g_object_unref(im_field);

    /* Set proper units */
    xyunit = gwy_data_field_get_si_unit_xy(data_field);
    zunit = gwy_data_field_get_si_unit_z(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_si_unit_power(xyunit, -1, lineunit);
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_power(zunit, 2, lineunit);
    gwy_si_unit_multiply(lineunit, xyunit, lineunit);
}

/**
 * gwy_data_field_psdf:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @orientation: Orientation of lines (PSDF is simply averaged over the
 *               other orientation).
 * @interpolation: Interpolation to use when @nstats is given and requires
 *                 resampling (and possibly in FFT too).
 * @windowing: Windowing type to use.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, data field width (height) is used.
 *
 * Calculates one-dimensional power spectrum density function of a rectangular
 * part of a data field.
 **/
void
gwy_data_field_psdf(GwyDataField *data_field,
                    GwyDataLine *target_line,
                    GwyOrientation orientation,
                    GwyInterpolationType interpolation,
                    GwyWindowingType windowing,
                    gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_psdf(data_field, target_line,
                             0, 0, data_field->xres, data_field->yres,
                             orientation, interpolation, windowing, nstats);
}

/**
 * gwy_data_field_area_minkowski_volume:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates Minkowski volume functional of a rectangular part of a data
 * field.
 *
 * Volume functional is calculated as the number of values above each
 * threshold value (,white pixels`) divided by the total number of samples
 * in the area.  Is it's equivalent to 1-CDH.
 **/
void
gwy_data_field_area_minkowski_volume(GwyDataField *data_field,
                                     GwyDataLine *target_line,
                                     gint col, gint row,
                                     gint width, gint height,
                                     gint nstats)
{
    gwy_data_field_area_cdh(data_field, NULL, target_line,
                            col, row, width, height,
                            nstats);
    gwy_data_line_multiply(target_line, -1.0);
    gwy_data_line_add(target_line, 1.0);
}

/**
 * gwy_data_field_minkowski_volume:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates Minkowski volume functional of a data field.
 *
 * See gwy_data_field_area_minkowski_volume() for details.
 **/
void
gwy_data_field_minkowski_volume(GwyDataField *data_field,
                                GwyDataLine *target_line,
                                gint nstats)
{
    gwy_data_field_cdh(data_field, target_line, nstats);
    gwy_data_line_multiply(target_line, -1.0);
    gwy_data_line_add(target_line, 1.0);
}

/**
 * gwy_data_field_area_minkowski_boundary:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates Minkowski boundary functional of a rectangular part of a data
 * field.
 *
 * Boundary functional is calculated as the number of boundaries for each
 * threshold value (the number of pixel sides where of neighouring pixels is
 * ,white` and the other ,black`) divided by the total number of samples
 * in the area.
 **/
void
gwy_data_field_area_minkowski_boundary(GwyDataField *data_field,
                                       GwyDataLine *target_line,
                                       gint col, gint row,
                                       gint width, gint height,
                                       gint nstats)
{
    GwySIUnit *fieldunit, *lineunit;
    const gdouble *data;
    gdouble *line;
    gdouble min, max, q;
    gint xres, i, j, k, k0, kr, kd;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    if (nstats < 1) {
        nstats = floor(3.49*cbrt(width*height) + 0.5);
        nstats = MAX(nstats, 2);
    }

    gwy_data_line_resample(target_line, nstats, GWY_INTERPOLATION_NONE);
    gwy_data_line_clear(target_line);
    gwy_data_field_area_get_min_max(data_field, NULL,
                                    col, row, width, height,
                                    &min, &max);
    /* There are no boundaries on a totally flat sufrace */
    if (min == max || width == 0 || height == 0)
        return;

    xres = data_field->xres;
    q = nstats/(max - min);
    line = target_line->data;

    for (i = 0; i < height-1; i++) {
        kr = (gint)((data_field->data[i*xres + col] - min)*q);
        for (j = 0; j < width-1; j++) {
            data = data_field->data + (i + row)*xres + (col + j);

            k0 = kr;

            kr = (gint)((data[1] - min)*q);
            for (k = MAX(MIN(k0, kr), 0); k < MIN(MAX(k0, kr), nstats); k++)
                line[k] += 1;

            kd = (gint)((data[xres] - min)*q);
            for (k = MAX(MIN(k0, kd), 0); k < MIN(MAX(k0, kd), nstats); k++)
                line[k] += 1;
        }
    }

    gwy_data_line_multiply(target_line, 1.0/(width*height));
    gwy_data_line_set_real(target_line, max - min);
    gwy_data_line_set_offset(target_line, min);

    /* Set proper units */
    fieldunit = gwy_data_field_get_si_unit_z(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_serializable_clone(G_OBJECT(fieldunit), G_OBJECT(lineunit));
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_set_from_string(lineunit, NULL);
}

/**
 * gwy_data_field_minkowski_boundary:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates Minkowski boundary functional of a data field.
 *
 * See gwy_data_field_area_minkowski_boundary() for details.
 **/
void
gwy_data_field_minkowski_boundary(GwyDataField *data_field,
                                  GwyDataLine *target_line,
                                  gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_minkowski_boundary(data_field, target_line,
                                           0, 0,
                                           data_field->xres, data_field->yres,
                                           nstats);
}

/**
 * gwy_data_field_area_minkowski_euler:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates Minkowski connectivity functional (Euler characteristics) of
 * a rectangular part of a data field.
 *
 * Connectivity functional is calculated as the number connected areas of
 * pixels above threhsold (,white`) minus the number of connected areas of
 * pixels below threhsold (,black`) for each threshold value, divided by the
 * total number of samples in the area.
 **/
void
gwy_data_field_area_minkowski_euler(GwyDataField *data_field,
                                    GwyDataLine *target_line,
                                    gint col, gint row,
                                    gint width, gint height,
                                    gint nstats)
{
    GwySIUnit *fieldunit, *lineunit;
    GwyDataLine *tmp_line;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    g_return_if_fail(col >= 0 && row >= 0
                     && width >= 0 && height >= 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    if (nstats < 1) {
        nstats = floor(3.49*cbrt(width*height) + 0.5);
        nstats = MAX(nstats, 2);
    }

    gwy_data_line_resample(target_line, nstats, GWY_INTERPOLATION_NONE);
    tmp_line = gwy_data_line_new_alike(target_line, FALSE);

    gwy_data_field_area_grains_tgnd(data_field, target_line,
                                    col, row, width, height,
                                    FALSE, nstats);
    gwy_data_field_area_grains_tgnd(data_field, tmp_line,
                                    col, row, width, height,
                                    TRUE, nstats);

    for (i = 0; i < nstats; i++)
        target_line->data[i] -= tmp_line->data[nstats-1 - i];
    g_object_unref(tmp_line);

    gwy_data_line_multiply(target_line, 1.0/(width*height));
    gwy_data_line_invert(target_line, TRUE, FALSE);

    /* Set proper units */
    fieldunit = gwy_data_field_get_si_unit_z(data_field);
    lineunit = gwy_data_line_get_si_unit_x(target_line);
    gwy_serializable_clone(G_OBJECT(fieldunit), G_OBJECT(lineunit));
    lineunit = gwy_data_line_get_si_unit_y(target_line);
    gwy_si_unit_set_from_string(lineunit, NULL);
}

/**
 * gwy_data_field_minkowski_euler:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates Minkowski connectivity functional (Euler characteristics) of
 * a data field.
 *
 * See gwy_data_field_area_minkowski_euler() for details.
 **/
void
gwy_data_field_minkowski_euler(GwyDataField *data_field,
                               GwyDataLine *target_line,
                               gint nstats)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_minkowski_euler(data_field, target_line,
                                        0, 0,
                                        data_field->xres, data_field->yres,
                                        nstats);
}

/**
 * square_area1:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @q: One fourth of rectangle projected area (x-size * ysize).
 *
 * Calculates approximate area of a one square pixel.
 *
 * Returns: The area.
 **/
static inline gdouble
square_area1(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
             gdouble q)
{
    gdouble c;

    c = (z1 + z2 + z3 + z4)/4.0;
    z1 -= c;
    z2 -= c;
    z3 -= c;
    z4 -= c;

    return (sqrt(1.0 + 2.0*(z1*z1 + z2*z2)/q)
            + sqrt(1.0 + 2.0*(z2*z2 + z3*z3)/q)
            + sqrt(1.0 + 2.0*(z3*z3 + z4*z4)/q)
            + sqrt(1.0 + 2.0*(z4*z4 + z1*z1)/q));
}

/**
 * square_area1w:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @w1: Weight of first corner (0 or 1).
 * @w2: Weight of second corner (0 or 1).
 * @w3: Weight of third corner (0 or 1).
 * @w4: Weight of fourth corner (0 or 1).
 * @q: One fourth of rectangle projected area (x-size * ysize).
 *
 * Calculates approximate area of a one square pixel with some corners possibly
 * missing.
 *
 * Returns: The area.
 **/
static inline gdouble
square_area1w(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
              gint w1, gint w2, gint w3, gint w4,
              gdouble q)
{
    gdouble c;

    c = (z1 + z2 + z3 + z4)/4.0;
    z1 -= c;
    z2 -= c;
    z3 -= c;
    z4 -= c;

    return ((w1 + w2)*sqrt(1.0 + 2.0*(z1*z1 + z2*z2)/q)
            + (w2 + w3)*sqrt(1.0 + 2.0*(z2*z2 + z3*z3)/q)
            + (w3 + w4)*sqrt(1.0 + 2.0*(z3*z3 + z4*z4)/q)
            + (w4 + w1)*sqrt(1.0 + 2.0*(z4*z4 + z1*z1)/q))/2.0;
}

/**
 * square_area2:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @x: One fourth of square of rectangle width (x-size).
 * @y: One fourth of square of rectangle height (y-size).
 *
 * Calculates approximate area of a one general rectangular pixel.
 *
 * Returns: The area.
 **/
static inline gdouble
square_area2(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
             gdouble x, gdouble y)
{
    gdouble c;

    c = (z1 + z2 + z3 + z4)/2.0;

    return (sqrt(1.0 + (z1 - z2)*(z1 - z2)/x
                 + (z1 + z2 - c)*(z1 + z2 - c)/y)
            + sqrt(1.0 + (z2 - z3)*(z2 - z3)/y
                   + (z2 + z3 - c)*(z2 + z3 - c)/x)
            + sqrt(1.0 + (z3 - z4)*(z3 - z4)/x
                   + (z3 + z4 - c)*(z3 + z4 - c)/y)
            + sqrt(1.0 + (z1 - z4)*(z1 - z4)/y
                   + (z1 + z4 - c)*(z1 + z4 - c)/x));
}

/**
 * square_area2w:
 * @z1: Z-value in first corner.
 * @z2: Z-value in second corner.
 * @z3: Z-value in third corner.
 * @z4: Z-value in fourth corner.
 * @w1: Weight of first corner (0 or 1).
 * @w2: Weight of second corner (0 or 1).
 * @w3: Weight of third corner (0 or 1).
 * @w4: Weight of fourth corner (0 or 1).
 * @x: One fourth of square of rectangle width (x-size).
 * @y: One fourth of square of rectangle height (y-size).
 *
 * Calculates approximate area of a one general rectangular pixel with some
 * corners possibly missing.
 *
 * Returns: The area.
 **/
static inline gdouble
square_area2w(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
              gint w1, gint w2, gint w3, gint w4,
              gdouble x, gdouble y)
{
    gdouble c;

    c = (z1 + z2 + z3 + z4)/2.0;

    return ((w1 + w2)*sqrt(1.0 + (z1 - z2)*(z1 - z2)/x
                           + (z1 + z2 - c)*(z1 + z2 - c)/y)
            + (w2 + w3)*sqrt(1.0 + (z2 - z3)*(z2 - z3)/y
                             + (z2 + z3 - c)*(z2 + z3 - c)/x)
            + (w3 + w4)*sqrt(1.0 + (z3 - z4)*(z3 - z4)/x
                             + (z3 + z4 - c)*(z3 + z4 - c)/y)
            + (w4 + w1)*sqrt(1.0 + (z1 - z4)*(z1 - z4)/y
                             + (z1 + z4 - c)*(z1 + z4 - c)/x))/2.0;
}

/**
 * stripe_area1:
 * @n: The number of values in @r, @rr, @m.
 * @stride: Stride in @r, @rr, @m.
 * @r: Array of @n z-values of vertices, this row of vertices is considered
 *     inside.
 * @rr: Array of @n z-values of vertices, this row of vertices is considered
 *      outside.
 * @m: Mask for @r (@rr does not need mask since it has zero weight by
 *     definition), or %NULL to sum over all @r vertices.
 * @q: One fourth of rectangle projected area (x-size * ysize).
 *
 * Calculates approximate area of a half-pixel stripe.
 *
 * Returns: The area.
 **/
static gdouble
stripe_area1(gint n,
             gint stride,
             const gdouble *r,
             const gdouble *rr,
             const gdouble *m,
             gdouble q)
{
    gdouble sum = 0.0;
    gint j;

    if (m) {
        for (j = 0; j < n-1; j++)
            sum += square_area1w(r[j*stride], r[(j + 1)*stride],
                                 rr[(j + 1)*stride], rr[j*stride],
                                 m[j*stride] > 0.0, m[(j + 1)*stride] > 0.0,
                                 0, 0,
                                 q);
    }
    else {
        for (j = 0; j < n-1; j++)
            sum += square_area1w(r[j*stride], r[(j + 1)*stride],
                                 rr[(j + 1)*stride], rr[j*stride],
                                 1, 1, 0, 0,
                                 q);
    }

    return sum;
}

/**
 * stripe_area2:
 * @n: The number of values in @r, @rr, @m.
 * @stride: Stride in @r, @rr, @m.
 * @r: Array of @n z-values of vertices, this row of vertices is considered
 *     inside.
 * @rr: Array of @n z-values of vertices, this row of vertices is considered
 *      outside.
 * @m: Mask for @r (@rr does not need mask since it has zero weight by
 *     definition), or %NULL to sum over all @r vertices.
 * @x: One fourth of square of rectangle width (x-size).
 * @y: One fourth of square of rectangle height (y-size).
 *
 * Calculates approximate area of a half-pixel stripe.
 *
 * Returns: The area.
 **/
static gdouble
stripe_area2(gint n,
             gint stride,
             const gdouble *r,
             const gdouble *rr,
             const gdouble *m,
             gdouble x,
             gdouble y)
{
    gdouble sum = 0.0;
    gint j;

    if (m) {
        for (j = 0; j < n-1; j++)
            sum += square_area2w(r[j*stride], r[(j + 1)*stride],
                                 rr[(j + 1)*stride], rr[j*stride],
                                 m[j*stride] > 0.0, m[(j + 1)*stride] > 0.0,
                                 0, 0,
                                 x, y);
    }
    else {
        for (j = 0; j < n-1; j++)
            sum += square_area2w(r[j*stride], r[(j + 1)*stride],
                                 rr[(j + 1)*stride], rr[j*stride],
                                 1, 1, 0, 0,
                                 x, y);
    }

    return sum;
}

static gdouble
calculate_surface_area(GwyDataField *dfield,
                       GwyDataField *mask,
                       gint col, gint row,
                       gint width, gint height)
{
    const gdouble *r, *m, *dataul, *maskul;
    gint i, j, xres, yres, s;
    gdouble x, y, q, sum = 0.0;

    /* special cases */
    if (!width || !height)
        return sum;

    xres = dfield->xres;
    yres = dfield->yres;
    x = dfield->xreal/dfield->xres;
    y = dfield->yreal/dfield->yres;
    q = x*y;
    x = x*x;
    y = y*y;
    dataul = dfield->data + xres*row + col;

    if (mask) {
        maskul = mask->data + xres*row + col;
        if (fabs(log(x/y)) < 1e-7) {
            /* Inside */
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                m = maskul + xres*i;
                for (j = 0; j < width-1; j++)
                    sum += square_area1w(r[j], r[j+1],
                                         r[j+xres+1], r[j+xres],
                                         m[j] > 0.0, m[j+1] > 0.0,
                                         m[j+xres+1] > 0.0, m[j+xres] > 0.0,
                                         q);
            }

            /* Top row */
            s = !(row == 0);
            sum += stripe_area1(width, 1, dataul, dataul - s*xres, maskul, q);

            /* Bottom row */
            s = !(row + height == yres);
            sum += stripe_area1(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                maskul + xres*(height-1),
                                q);

            /* Left column */
            s = !(col == 0);
            sum += stripe_area1(height, xres, dataul, dataul - s, maskul, q);

            /* Right column */
            s = !(col + width == xres);
            sum += stripe_area1(height, xres,
                                dataul + width-1, dataul + width-1 + s,
                                maskul + width-1,
                                q);
        }
        else {
            /* Inside */
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                m = maskul + xres*i;
                for (j = 0; j < width-1; j++)
                    sum += square_area2w(r[j], r[j+1],
                                         r[j+xres+1], r[j+xres],
                                         m[j] > 0.0, m[j+1] > 0.0,
                                         m[j+xres+1] > 0.0, m[j+xres] > 0.0,
                                         x, y);
            }

            /* Top row */
            s = !(row == 0);
            sum += stripe_area2(width, 1, dataul, dataul - s*xres, maskul,
                                x, y);

            /* Bottom row */
            s = !(row + height == yres);
            sum += stripe_area2(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                maskul + xres*(height-1),
                                x, y);

            /* Left column */
            s = !(col == 0);
            sum += stripe_area2(height, xres, dataul, dataul - s, maskul, y, x);

            /* Right column */
            s = !(col + width == xres);
            sum += stripe_area2(height, xres,
                                dataul + width-1, dataul + width-1 + s,
                                maskul + width-1,
                                y, x);
        }

        /* Just take the four corner quater-pixels as flat.  */
        if (maskul[0])
            sum += 1.0;
        if (maskul[width-1])
            sum += 1.0;
        if (maskul[xres*(height-1)])
            sum += 1.0;
        if (maskul[xres*(height-1) + width-1])
            sum += 1.0;
    }
    else {
        if (fabs(log(x/y)) < 1e-7) {
            /* Inside */
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                for (j = 0; j < width-1; j++)
                    sum += square_area1(r[j], r[j+1], r[j+xres+1], r[j+xres],
                                        q);
            }

            /* Top row */
            s = !(row == 0);
            sum += stripe_area1(width, 1, dataul, dataul - s*xres, NULL, q);

            /* Bottom row */
            s = !(row + height == yres);
            sum += stripe_area1(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                NULL,
                                q);

            /* Left column */
            s = !(col == 0);
            sum += stripe_area1(height, xres, dataul, dataul - s, NULL, q);

            /* Right column */
            s = !(col + width == xres);
            sum += stripe_area1(height, xres,
                                dataul + width-1, dataul + width-1 + s, NULL,
                                q);
        }
        else {
            for (i = 0; i < height-1; i++) {
                r = dataul + xres*i;
                for (j = 0; j < width-1; j++)
                    sum += square_area2(r[j], r[j+1], r[j+xres+1], r[j+xres],
                                        x, y);
            }

            /* Top row */
            s = !(row == 0);
            sum += stripe_area2(width, 1, dataul, dataul - s*xres, NULL, x, y);

            /* Bottom row */
            s = !(row + height == yres);
            sum += stripe_area2(width, 1,
                                dataul + xres*(height-1),
                                dataul + xres*(height-1 + s),
                                NULL,
                                x, y);

            /* Left column */
            s = !(col == 0);
            sum += stripe_area2(height, xres, dataul, dataul - s, NULL, y, x);

            /* Right column */
            s = !(col + width == xres);
            sum += stripe_area2(height, xres,
                                dataul + width-1, dataul + width-1 + s, NULL,
                                y, x);
        }

        /* Just take the four corner quater-pixels as flat.  */
        sum += 4.0;
    }

    return sum*q/4;
}
/**
 * gwy_data_field_get_surface_area:
 * @data_field: A data field.
 *
 * Computes surface area of a data field.
 *
 * This quantity is cached.
 *
 * Returns: surface area
 **/
gdouble
gwy_data_field_get_surface_area(GwyDataField *data_field)
{
    gdouble area = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), area);

    gwy_debug("%s", CTEST(data_field, ARE) ? "cache" : "lame");
    if (CTEST(data_field, ARE))
        return CVAL(data_field, ARE);

    area = calculate_surface_area(data_field, NULL,
                                  0, 0, data_field->xres, data_field->yres);

    CVAL(data_field, ARE) = area;
    data_field->cached |= CBIT(ARE);

    return area;
}

/**
 * gwy_data_field_area_get_surface_area:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes surface area of a rectangular part of a data field.
 *
 * Returns: The surface area.
 **/
gdouble
gwy_data_field_area_get_surface_area(GwyDataField *data_field,
                                     GwyDataField *mask,
                                     gint col, gint row,
                                     gint width, gint height)
{
    gdouble area = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), area);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == data_field->xres
                                   && mask->yres == data_field->yres), area);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= data_field->xres
                         && row + height <= data_field->yres,
                         area);

    /* The result is the same, but it can be cached. */
    if (!mask
        && row == 0 && col == 0
        && width == data_field->xres && height == data_field->yres)
        return gwy_data_field_get_surface_area(data_field);

    return calculate_surface_area(data_field, mask, col, row, width, height);
}

/**
 * gwy_data_field_slope_distribution:
 * @data_field: A data field.
 * @derdist: A data line to fill with angular slope distribution. Its
 *           resolution determines resolution of the distribution.
 * @kernel_size: If positive, local plane fitting will be used for slope
 *               computation; if nonpositive, plain central derivations
 *               will be used.
 *
 * Computes angular slope distribution.
 **/
void
gwy_data_field_slope_distribution(GwyDataField *dfield,
                                  GwyDataLine *derdist,
                                  gint kernel_size)
{
    GwySIUnit *lineunit;
    gdouble *data, *der;
    gdouble bx, by, phi;
    gint xres, yres, nder;
    gint col, row, iphi;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(GWY_IS_DATA_LINE(derdist));

    nder = gwy_data_line_get_res(derdist);
    der = gwy_data_line_get_data(derdist);
    data = dfield->data;
    xres = dfield->xres;
    yres = dfield->yres;
    memset(der, 0, nder*sizeof(gdouble));
    if (kernel_size > 0) {
        for (row = 0; row + kernel_size < yres; row++) {
            for (col = 0; col + kernel_size < xres; col++) {
                gwy_data_field_area_fit_plane(dfield, NULL, col, row,
                                              kernel_size, kernel_size,
                                              NULL, &bx, &by);
                phi = atan2(by, bx);
                iphi = (gint)floor(nder*(phi + G_PI)/(2.0*G_PI));
                iphi = CLAMP(iphi, 0, nder-1);
                der[iphi] += hypot(bx, by);
            }
        }
    }
    else {
        gdouble qx = xres/gwy_data_field_get_xreal(dfield);
        gdouble qy = yres/gwy_data_field_get_yreal(dfield);

        for (row = 1; row + 1 < yres; row++) {
            for (col = 1; col + 1 < xres; col++) {
                bx = data[row*xres + col + 1] - data[row*xres + col - 1];
                by = data[row*xres + xres + col] - data[row*xres - xres + col];
                phi = atan2(by*qy, bx*qx);
                iphi = (gint)floor(nder*(phi + G_PI)/(2.0*G_PI));
                iphi = CLAMP(iphi, 0, nder-1);
                der[iphi] += hypot(bx, by);
            }
        }
    }

    /* Set proper units */
    lineunit = gwy_data_line_get_si_unit_x(derdist);
    gwy_si_unit_set_from_string(lineunit, NULL);
    lineunit = gwy_data_line_get_si_unit_y(derdist);
    gwy_si_unit_divide(gwy_data_field_get_si_unit_z(dfield),
                       gwy_data_field_get_si_unit_xy(dfield),
                       lineunit);
}

/**
 * gwy_data_field_area_get_median:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Computes median value of a data field area.
 *
 * Returns: The median value.
 **/
gdouble
gwy_data_field_area_get_median(GwyDataField *dfield,
                               GwyDataField *mask,
                               gint col, gint row,
                               gint width, gint height)
{
    gdouble median = 0.0;
    const gdouble *datapos, *mpos;
    gdouble *buffer;
    gint i, j, nn;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), median);
    g_return_val_if_fail(!mask || (GWY_IS_DATA_FIELD(mask)
                                   && mask->xres == dfield->xres
                                   && mask->yres == dfield->yres),
                         median);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         median);
    if (!width || !height)
        return median;

    if (mask) {
        buffer = g_new(gdouble, width*height);
        datapos = dfield->data + row*dfield->xres + col;
        mpos = mask->data + row*mask->xres + col;
        nn = 0;
        for (i = 0; i < height; i++) {
            const gdouble *drow = datapos + i*dfield->xres;
            const gdouble *mrow = mpos + i*mask->xres;

            for (j = 0; j < width; j++) {
                if (*mrow > 0.0) {
                    buffer[nn] = *drow;
                    nn++;
                }
                drow++;
                mrow++;
            }
        }

        if (nn)
            median = gwy_math_median(nn, buffer);

        g_free(buffer);

        return median;
    }

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_median(dfield);

    buffer = g_new(gdouble, width*height);
    datapos = dfield->data + row*dfield->xres + col;
    if (height == 1 || (col == 0 && width == dfield->xres))
        memcpy(buffer, datapos, width*height*sizeof(gdouble));
    else {
        for (i = 0; i < height; i++)
            memcpy(buffer + i*width, datapos + i*dfield->xres,
                   width*sizeof(gdouble));
    }
    median = gwy_math_median(width*height, buffer);
    g_free(buffer);

    return median;
}

/**
 * gwy_data_field_get_median:
 * @data_field: A data field.
 *
 * Computes median value of a data field.
 *
 * This quantity is cached.
 *
 * Returns: The median value.
 **/
gdouble
gwy_data_field_get_median(GwyDataField *data_field)
{
    gint xres, yres;
    gdouble *buffer;
    gdouble med;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), 0.0);

    gwy_debug("%s", CTEST(data_field, MED) ? "cache" : "lame");
    if (CTEST(data_field, MED))
        return CVAL(data_field, MED);

    xres = data_field->xres;
    yres = data_field->yres;
    buffer = g_memdup(data_field->data, xres*yres*sizeof(gdouble));
    med = gwy_math_median(xres*yres, buffer);
    g_free(buffer);

    CVAL(data_field, MED) = med;
    data_field->cached |= CBIT(MED);

    return med;
}

/**
 * gwy_data_field_area_get_normal_coeffs:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @nx: Where x-component of average normal vector should be stored, or %NULL.
 * @ny: Where y-component of average normal vector should be stored, or %NULL.
 * @nz: Where z-component of average normal vector should be stored, or %NULL.
 * @normalize1: true to normalize the normal vector to 1, false to normalize
 *              the vector so that z-component is 1.
 *
 * Computes average normal vector of an area of a data field.
 **/
void
gwy_data_field_area_get_normal_coeffs(GwyDataField *data_field,
                                      gint col, gint row,
                                      gint width, gint height,
                                      gdouble *nx, gdouble *ny, gdouble *nz,
                                      gboolean normalize1)
{
    gint i, j;
    int ctr = 0;
    gdouble d1x, d1y, d1z, d2x, d2y, d2z, dcx, dcy, dcz, dd;
    gdouble sumdx = 0.0, sumdy = 0.0, sumdz = 0.0, sumw = 0.0;
    gdouble avgdx, avgdy, avgdz;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    /* This probably should not be enforced */
    /*
    g_return_if_fail(gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(a),
                                       gwy_data_field_get_si_unit_z(a)),
                     FALSE);
                     */

    for (i = col; i < col + width; i++) {
        for (j = row; j < row + height; j++) {
            d1x = 1.0;
            d1y = 0.0;
            d1z = gwy_data_field_get_xder(data_field, i, j);
            d2x = 0.0;
            d2y = 1.0;
            d2z = gwy_data_field_get_yder(data_field, i, j);
            /* Cross product = normal vector */
            dcx = d1y*d2z - d1z*d2y;
            dcy = d1z*d2x - d1x*d2z;
            dcz = d1x*d2y - d1y*d2x; /* Always 1 */
            /* Normalize and add */
            dd = sqrt(dcx*dcx + dcy*dcy + dcz*dcz);
            dcx /= dd;
            sumdx += dcx;
            dcy /= dd;
            sumdy += dcy;
            dcz /= dd;
            sumdz += dcz;
            sumw += 1.0/dd;
            ctr++;
        }
    }
    /* average dimensionless normal vector */
    if (normalize1) {
        /* normalize to 1 */
        avgdx = sumdx/ctr;
        avgdy = sumdy/ctr;
        avgdz = sumdz/ctr;
    }
    else {
        /* normalize for gwy_data_field_plane_level */
        avgdx = sumdx/sumw;
        avgdy = sumdy/sumw;
        avgdz = sumdz/sumw;
    }

    if (nx)
        *nx = avgdx;
    if (ny)
        *ny = avgdy;
    if (nz)
        *nz = avgdz;
}


/**
 * gwy_data_field_get_normal_coeffs:
 * @data_field: A data field.
 * @nx: Where x-component of average normal vector should be stored, or %NULL.
 * @ny: Where y-component of average normal vector should be stored, or %NULL.
 * @nz: Where z-component of average normal vector should be stored, or %NULL.
 * @normalize1: true to normalize the normal vector to 1, false to normalize
 *              the vector so that z-component is 1.
 *
 * Computes average normal vector of a data field.
 **/
void
gwy_data_field_get_normal_coeffs(GwyDataField *data_field,
                                 gdouble *nx, gdouble *ny, gdouble *nz,
                                 gboolean normalize1)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_get_normal_coeffs(data_field,
                                          0, 0,
                                          data_field->xres, data_field->yres,
                                          nx, ny, nz, normalize1);
}


/**
 * gwy_data_field_area_get_inclination:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @theta: Where theta angle (in radians) should be stored, or %NULL.
 * @phi: Where phi angle (in radians) should be stored, or %NULL.
 *
 * Calculates the inclination of the image (polar and azimuth angle).
 **/
void
gwy_data_field_area_get_inclination(GwyDataField *data_field,
                                    gint col, gint row,
                                    gint width, gint height,
                                    gdouble *theta,
                                    gdouble *phi)
{
    gdouble nx, ny, nz, nr;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);
    gwy_data_field_area_get_normal_coeffs(data_field,
                                          col, row, width, height,
                                          &nx, &ny, &nz, TRUE);

    nr = hypot(nx, ny);
    if (theta)
        *theta = atan2(nr, nz);
    if (phi)
        *phi = atan2(ny, nx);
}


/**
 * gwy_data_field_get_inclination:
 * @data_field: A data field.
 * @theta: Where theta angle (in radians) should be stored, or %NULL.
 * @phi: Where phi angle (in radians) should be stored, or %NULL.
 *
 * Calculates the inclination of the image (polar and azimuth angle).
 **/
void
gwy_data_field_get_inclination(GwyDataField *data_field,
                               gdouble *theta,
                               gdouble *phi)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_get_inclination(data_field,
                                        0, 0,
                                        data_field->xres, data_field->yres,
                                        theta,
                                        phi);
}

/**
 * gwy_data_field_area_get_line_stats:
 * @data_field: A data field.
 * @mask: Mask of values to take values into account, or %NULL for full
 *        @data_field.  Values equal to 0.0 and below cause corresponding
 *        @data_field samples to be ignored, values equal to 1.0 and above
 *        cause inclusion of corresponding @data_field samples.  The behaviour
 *        for values inside (0.0, 1.0) is undefined (it may be specified
 *        in the future).
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to the number of rows (columns).
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @quantity: The line quantity to calulate for each row (column).
 * @orientation: Line orientation.  For %GWY_ORIENTATION_HORIZONTAL each
 *               @target_line point corresponds to a row of the area,
 *               for %GWY_ORIENTATION_VERTICAL each @target_line point
 *               corresponds to a column of the area.
 *
 * Calculates a line quantity for each row or column in a data field area.
 *
 * Since: 2.2
 **/
void
gwy_data_field_area_get_line_stats(GwyDataField *data_field,
                                   GwyDataField *mask,
                                   GwyDataLine *target_line,
                                   gint col, gint row,
                                   gint width, gint height,
                                   GwyLineStatQuantity quantity,
                                   GwyOrientation orientation)
{
    GwyDataLine *buf;
    GwySIUnit *zunit, *xyunit, *lunit;
    gint i, j, xres, yres;
    const gdouble *data;
    gdouble *ldata;
    gdouble v;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_LINE(target_line));
    g_return_if_fail(!mask || GWY_IS_DATA_FIELD(mask));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= data_field->xres
                     && row + height <= data_field->yres);

    if (mask) {
        g_warning("Masking not implemented!");
        mask = NULL;
    }

    xres = data_field->xres;
    yres = data_field->yres;
    data = data_field->data + row*xres + col;

    if (orientation == GWY_ORIENTATION_VERTICAL) {
        gwy_data_line_resample(target_line, width, GWY_INTERPOLATION_NONE);
        ldata = target_line->data;

        if (!mask) {
            switch (quantity) {
                case GWY_LINE_STAT_MEAN:
                gwy_data_line_clear(target_line);
                for (i = 0; i < height; i++) {
                    for (j = 0; j < width; j++)
                        ldata[j] += data[i*xres + j];
                }
                gwy_data_line_multiply(target_line, 1.0/height);
                break;

                case GWY_LINE_STAT_MEDIAN:
                /* FIXME: Can we optimize this for linear memory access? */
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (j = 0; j < width; j++) {
                    gwy_data_field_get_column_part(data_field, buf,
                                                   j, row, row + height);
                    ldata[j] = gwy_math_median(width, buf->data);
                }
                g_object_unref(buf);
                break;

                case GWY_LINE_STAT_MINIMUM:
                gwy_data_field_get_row_part(data_field, target_line,
                                            row, col, col + width);
                for (i = 1; i < height; i++) {
                    for (j = 0; j < width; j++) {
                        if (data[i*xres + j] < ldata[j])
                            ldata[j] = data[i*xres + j];
                    }
                }
                break;

                case GWY_LINE_STAT_MAXIMUM:
                gwy_data_field_get_row_part(data_field, target_line,
                                            row, col, col + width);
                for (i = 1; i < height; i++) {
                    for (j = 0; j < width; j++) {
                        if (data[i*xres + j] > ldata[j])
                            ldata[j] = data[i*xres + j];
                    }
                }
                break;

                case GWY_LINE_STAT_RMS:
                /* FIXME: Optimize for linear memory access. */
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (j = 0; j < width; j++) {
                    gwy_data_field_get_column_part(data_field, buf,
                                                   j, row, row + height);
                    ldata[j] = gwy_data_line_get_rms(buf);
                }
                g_object_unref(buf);
                break;

                case GWY_LINE_STAT_LENGTH:
                /* FIXME: Optimize for linear memory access. */
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (j = 0; j < width; j++) {
                    gwy_data_field_get_column_part(data_field, buf,
                                                   j, row, row + height);
                    ldata[j] = gwy_data_line_get_length(buf);
                }
                g_object_unref(buf);
                break;

                case GWY_LINE_STAT_SLOPE:
                /* FIXME: Optimize for linear memory access. */
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (j = 0; j < width; j++) {
                    gwy_data_field_get_column_part(data_field, buf,
                                                   j, row, row + height);
                    gwy_data_line_get_line_coeffs(buf, NULL, &v);
                    ldata[j] = v*yres/data_field->yreal;
                }
                g_object_unref(buf);
                break;
            }
        }
        else {
        }

        gwy_data_line_set_offset(target_line,
                                 gwy_data_field_jtor(data_field, col));
        gwy_data_line_set_real(target_line,
                               gwy_data_field_jtor(data_field, width));
    }
    else {
        gwy_data_line_resample(target_line, height, GWY_INTERPOLATION_NONE);
        ldata = target_line->data;

        if (!mask) {
            switch (quantity) {
                case GWY_LINE_STAT_MEAN:
                gwy_data_line_clear(target_line);
                for (i = 0; i < height; i++) {
                    for (j = 0; j < width; j++)
                        ldata[i] += data[i*xres + j];
                }
                gwy_data_line_multiply(target_line, 1.0/height);
                break;

                case GWY_LINE_STAT_MEDIAN:
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (i = 0; i < height; i++) {
                    memcpy(buf->data, data + i*xres, width*sizeof(gdouble));
                    ldata[i] = gwy_math_median(width, buf->data);
                }
                g_object_unref(buf);
                break;

                case GWY_LINE_STAT_MINIMUM:
                for (i = 0; i < height; i++) {
                    v = data[i*xres];
                    for (j = 1; j < width; j++) {
                        if (data[i*xres + j] < v)
                            v = data[i*xres + j];
                    }
                    ldata[i] = v;
                }
                break;

                case GWY_LINE_STAT_MAXIMUM:
                for (i = 0; i < height; i++) {
                    v = data[i*xres];
                    for (j = 1; j < width; j++) {
                        if (data[i*xres + j] > v)
                            v = data[i*xres + j];
                    }
                    ldata[i] = v;
                }
                break;

                case GWY_LINE_STAT_RMS:
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (i = 0; i < height; i++) {
                    memcpy(buf->data, data + i*xres, width*sizeof(gdouble));
                    ldata[i] = gwy_data_line_get_rms(buf);
                }
                g_object_unref(buf);
                break;

                case GWY_LINE_STAT_LENGTH:
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (i = 0; i < height; i++) {
                    memcpy(buf->data, data + i*xres, width*sizeof(gdouble));
                    ldata[i] = gwy_data_line_get_length(buf);
                }
                g_object_unref(buf);
                break;

                case GWY_LINE_STAT_SLOPE:
                buf = gwy_data_line_new(width, 1.0, FALSE);
                for (i = 0; i < height; i++) {
                    memcpy(buf->data, data + i*xres, width*sizeof(gdouble));
                    gwy_data_line_get_line_coeffs(buf, NULL, &v);
                    ldata[i] = v*xres/data_field->xreal;
                }
                g_object_unref(buf);
                break;
            }
        }
        else {
        }

        gwy_data_line_set_offset(target_line,
                                 gwy_data_field_itor(data_field, row));
        gwy_data_line_set_real(target_line,
                               gwy_data_field_itor(data_field, height));
    }

    xyunit = gwy_data_field_get_si_unit_xy(data_field);
    zunit = gwy_data_field_get_si_unit_z(data_field);
    lunit = gwy_data_line_get_si_unit_y(target_line);
    switch (quantity) {
        case GWY_LINE_STAT_LENGTH:
        if (!gwy_si_unit_equal(xyunit, zunit))
            g_warning("Length makes no sense when lateral and value units "
                      "differ");
        case GWY_LINE_STAT_MEAN:
        case GWY_LINE_STAT_MEDIAN:
        case GWY_LINE_STAT_MINIMUM:
        case GWY_LINE_STAT_MAXIMUM:
        case GWY_LINE_STAT_RMS:
        gwy_serializable_clone(G_OBJECT(zunit), G_OBJECT(lunit));
        break;

        case GWY_LINE_STAT_SLOPE:
        gwy_si_unit_divide(zunit, xyunit, lunit);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

/**
 * gwy_data_field_get_line_stats:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to @data_field height (width).
 * @quantity: The line quantity to calulate for each row (column).
 * @orientation: Line orientation.  See gwy_data_field_area_get_line_stats().
 *
 * Calculates a line quantity for each row or column of a data field.
 *
 * Since: 2.2
 **/
void
gwy_data_field_get_line_stats(GwyDataField *data_field,
                              GwyDataLine *target_line,
                              GwyLineStatQuantity quantity,
                              GwyOrientation orientation)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_get_line_stats(data_field, NULL, target_line,
                                       0, 0,
                                       data_field->xres, data_field->yres,
                                       quantity, orientation);
}
/************************** Documentation ****************************/

/**
 * SECTION:stats
 * @title: stats
 * @short_description: Two-dimensional statistical functions
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
