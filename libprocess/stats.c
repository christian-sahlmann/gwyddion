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

#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"
#include "level.h"
#include "stats.h"
#include "linestats.h"

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
 * Returns: The maximum value.
 **/
gdouble
gwy_data_field_get_max(GwyDataField *data_field)
{
    gint i;
    gdouble max;
    gdouble *p;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), -G_MAXDOUBLE);

    gwy_debug("%s", CTEST(data_field, MAX) ? "cache" : "lame");
    if (CTEST(data_field, MAX))
        return CVAL(data_field, MAX);

    max = data_field->data[0];
    p = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++) {
        if (G_UNLIKELY(max < *p))
            max = *p;
    }
    CVAL(data_field, MAX) = max;
    data_field->cached |= CBIT(MAX);

    return max;
}


/**
 * gwy_data_field_area_get_max:
 * @data_field: A data field
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds the maximum value in a rectangular part of a data field.
 *
 * Returns: The maximum value.
 **/
gdouble
gwy_data_field_area_get_max(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    gint i, j;
    gdouble max = -G_MAXDOUBLE;
    gdouble *datapos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), max);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         max);
    if (!width || !height)
        return max;

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_max(dfield);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

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
 * Returns: The minimum value.
 **/
gdouble
gwy_data_field_get_min(GwyDataField *data_field)
{
    gint i;
    gdouble min;
    gdouble *p;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), G_MAXDOUBLE);

    gwy_debug("%s", CTEST(data_field, MIN) ? "cache" : "lame");
    if (CTEST(data_field, MIN))
        return CVAL(data_field, MIN);

    min = data_field->data[0];
    p = data_field->data;
    for (i = data_field->xres * data_field->yres; i; i--, p++) {
        if (G_UNLIKELY(min > *p))
            min = *p;
    }
    CVAL(data_field, MIN) = min;
    data_field->cached |= CBIT(MIN);

    return min;
}


/**
 * gwy_data_field_area_get_min:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 *
 * Finds the minimum value in a rectangular part of a data field.
 *
 * Returns: The minimum value.
 **/
gdouble
gwy_data_field_area_get_min(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    gint i, j;
    gdouble min = G_MAXDOUBLE;
    gdouble *datapos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), min);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         min);
    if (!width || !height)
        return min;

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_min(dfield);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            if (G_UNLIKELY(min > *drow))
                min = *drow;
            drow++;
        }
    }

    return min;
}

/**
 * gwy_data_field_get_sum:
 * @data_field: A data field.
 *
 * Sums all values in a data field.
 *
 * Returns: The sum of all values.
 **/
gdouble
gwy_data_field_get_sum(GwyDataField *data_field)
{
    gint i;
    gdouble sum = 0;
    gdouble *p;

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
                            gint col, gint row, gint width, gint height)
{
    gint i, j;
    gdouble sum = 0;
    gdouble *datapos;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_sum(dfield);

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

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
 * Returns: The average value.
 **/
gdouble
gwy_data_field_get_avg(GwyDataField *data_field)
{
    return gwy_data_field_get_sum(data_field)/((gdouble)(data_field->xres
                                                         * data_field->yres));
}

/**
 * gwy_data_field_area_get_avg:
 * @data_field: A data field
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
                            gint col, gint row, gint width, gint height)
{
    return gwy_data_field_area_get_sum(dfield, col, row,
                                       width, height)/(width*height);
}

/**
 * gwy_data_field_get_rms:
 * @data_field: A data field.
 *
 * Computes root mean square value of a data field.
 *
 * Returns: The root mean square value.
 **/
gdouble
gwy_data_field_get_rms(GwyDataField *data_field)
{
    gint i, n;
    gdouble rms = 0.0, sum, sum2;
    gdouble *p;

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
                            gint col, gint row, gint width, gint height)
{
    gint i, j, n;
    gdouble rms = 0.0, sum2 = 0.0;
    gdouble sum;
    gdouble *datapos;

    if (width == 0 || height == 0)
        return rms;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), rms);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         rms);
    if (!width || !height)
        return rms;

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_rms(dfield);

    sum = gwy_data_field_area_get_sum(dfield, col, row, width, height);
    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            sum2 += (*drow) * (*drow);
            *drow++;
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

    min = gwy_data_field_get_min(data_field);
    max = gwy_data_field_get_max(data_field);
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
    gdouble *p = data_field->data;
    gdouble nn = data_field->xres * data_field->yres;
    gdouble dif, myavg, myrms;

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
                              gint col, gint row, gint width, gint height,
                              gdouble *avg,
                              gdouble *ra,
                              gdouble *rms,
                              gdouble *skew,
                              gdouble *kurtosis)
{
    gint i, j;
    gdouble c_sz2, c_sz3, c_sz4, c_abs1;
    gdouble nn, dif, myavg, myrms;
    gdouble *datapos;

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    g_return_if_fail(col >= 0 && row >= 0
                     && width > 0 && height > 0
                     && col + width <= dfield->xres
                     && row + height <= dfield->yres);

    nn = width*height;
    c_sz2 = c_sz3 = c_sz4 = c_abs1 = 0;

    myavg = gwy_data_field_area_get_avg(dfield, col, row, width, height);
    if (avg)
        *avg = myavg;

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            dif = (*(drow++) - myavg);
            c_abs1 += fabs(dif);
            c_sz2 += dif*dif;
            c_sz3 += dif*dif*dif;
            c_sz4 += dif*dif*dif*dif;
        }
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
}

/**
 * gwy_data_field_get_line_stat_function:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @ulcol: Upper-left corner column index.
 * @ulrow: Upper-left corner row index.
 * @brcol: Bottom-right corner column index + 1.
 * @brrow: Bottom-right column row index + 1.
 * @type: The type of distribution to compute.
 * @orientation: Orientation to compute the distribution in.
 * @interpolation: Interpolation to use (unused for some functions).
 * @windowing: Windowing type to use (unused for some functions).
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, @data_field's resolution is used.
 *
 * Computes a statistical distribution of data field values.
 *
 * Returns: Normally %FALSE; %TRUE when @data_field is too small.  The return
 *          value should be ignored.
 **/
gboolean
gwy_data_field_get_line_stat_function(GwyDataField *data_field,
                                      GwyDataLine *target_line,
                                      gint ulcol, gint ulrow,
                                      gint brcol, gint brrow,
                                      GwySFOutputType type,
                                      GwyOrientation orientation,
                                      GwyInterpolationType interpolation,
                                      GwyWindowingType windowing, gint nstats)
{
    gint i, k, j, size;
    GwyDataLine *hlp_line;
    GwyDataLine *hlp_tarline;
    gdouble min = G_MAXDOUBLE, max = -G_MAXDOUBLE, val, realsize;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), FALSE);
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    /* FIXME: shouldn't size be difference + 1 ? */
    if (orientation == GWY_ORIENTATION_HORIZONTAL)
        size = brcol - ulcol;
    else
        size = brrow - ulrow;

    if (size < 10
        || (type == GWY_SF_OUTPUT_PSDF && size < 64))
        return FALSE;

    /* precompute settings if necessary */
    if (type == GWY_SF_OUTPUT_DH || type == GWY_SF_OUTPUT_CDH) {
        if (ulrow == 0 && ulcol == 0
            && brrow == data_field->xres-1 && brcol == data_field->yres - 1) {
            min = gwy_data_field_get_min(data_field);
            max = gwy_data_field_get_max(data_field);
        }
        else {
            min = gwy_data_field_area_get_min(data_field,
                                              ulcol, ulrow,
                                              brcol-ulcol, brrow-ulrow);
            max = gwy_data_field_area_get_max(data_field,
                                              ulcol, ulrow,
                                              brcol-ulcol, brrow-ulrow);
        }
    }
    else if (type == GWY_SF_OUTPUT_DA || type == GWY_SF_OUTPUT_CDA) {
        if (orientation == GWY_ORIENTATION_HORIZONTAL) {
            for (i = ulcol; i < brcol; i++)
                for (j = ulrow; j < brrow; j++) {
                    val = gwy_data_field_get_xder(data_field, i, j);
                    if (min > val)
                        min = val;
                    if (max < val)
                        max = val;
                }
        }
        else if (orientation == GWY_ORIENTATION_VERTICAL) {
            for (i = ulcol; i < brcol; i++) {
                for (j = ulrow; j < brrow; j++) {
                    val = gwy_data_field_get_yder(data_field, i, j);
                    if (min > val)
                        min = val;
                    if (max < val)
                        max = val;
                }
            }
        }
    }

    /*average over profiles*/
    if (orientation == GWY_ORIENTATION_HORIZONTAL) {
        realsize = gwy_data_field_jtor(data_field, size);
        hlp_line = gwy_data_line_new(size, realsize, FALSE);
        hlp_tarline = gwy_data_line_new(size, realsize, FALSE);

        if (nstats <= 0) {
            nstats = size;
        }
        if (type == GWY_SF_OUTPUT_DH || type == GWY_SF_OUTPUT_DA
            || type == GWY_SF_OUTPUT_CDA || type == GWY_SF_OUTPUT_CDH) {
            gwy_data_line_resample(target_line, nstats, interpolation);
            size = nstats;
        }
        else {
            gwy_data_line_resample(target_line, size, interpolation);
        }
        gwy_data_line_fill(target_line, 0.0);

        for (k = ulrow; k < brrow; k++) {
            gwy_data_field_get_row_part(data_field, hlp_line, k, ulcol, brcol);

            switch (type) {
                case GWY_SF_OUTPUT_DH:
                gwy_data_line_dh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDH:
                gwy_data_line_cdh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_DA:
                gwy_data_line_da(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDA:
                gwy_data_line_cda(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_ACF:
                gwy_data_line_acf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_HHCF:
                gwy_data_line_hhcf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_PSDF:
                gwy_data_line_psdf(hlp_line, hlp_tarline, windowing,
                                   interpolation);
                gwy_data_line_resample(hlp_tarline, size, interpolation);
                break;

                default:
                g_assert_not_reached();
                break;
            }
            for (j = 0; j < size; j++) { /*size*/
                target_line->data[j] += hlp_tarline->data[j]
                                        /((gdouble)(brrow-ulrow));
            }
            target_line->real = hlp_tarline->real;
        }
        g_object_unref(hlp_line);
        g_object_unref(hlp_tarline);

    }
    else if (orientation == GWY_ORIENTATION_VERTICAL) {
        realsize = gwy_data_field_itor(data_field, size);
        hlp_line = gwy_data_line_new(size, realsize, FALSE);
        hlp_tarline = gwy_data_line_new(size, realsize, FALSE);

        if (nstats <= 0) {
            nstats = size;
        }
        if (type == GWY_SF_OUTPUT_DH || type == GWY_SF_OUTPUT_DA
            || type == GWY_SF_OUTPUT_CDA || type == GWY_SF_OUTPUT_CDH) {
            gwy_data_line_resample(target_line, nstats, interpolation);
            size = nstats;
        }
        else {
            gwy_data_line_resample(target_line, size, interpolation);
        }
        gwy_data_line_fill(target_line, 0.0);

        for (k = ulcol; k < brcol; k++) {
            gwy_data_field_get_column_part(data_field,
                                           hlp_line, k, ulrow, brrow);

            switch (type) {
                case GWY_SF_OUTPUT_DH:
                gwy_data_line_dh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDH:
                gwy_data_line_cdh(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_DA:
                gwy_data_line_da(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_CDA:
                gwy_data_line_cda(hlp_line, hlp_tarline, min, max, nstats);
                break;

                case GWY_SF_OUTPUT_ACF:
                gwy_data_line_acf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_HHCF:
                gwy_data_line_hhcf(hlp_line, hlp_tarline);
                break;

                case GWY_SF_OUTPUT_PSDF:
                gwy_data_line_psdf(hlp_line, hlp_tarline, windowing,
                                   interpolation);
                gwy_data_line_resample(hlp_tarline, size, interpolation);
                break;

                default:
                g_assert_not_reached();
                break;
            }
            for (j = 0; j < size; j++) {
                target_line->data[j] += hlp_tarline->data[j]
                                        /((gdouble)(brcol-ulcol));
            }
            target_line->real = hlp_tarline->real;
        }
        g_object_unref(hlp_line);
        g_object_unref(hlp_tarline);

    }

    return TRUE;
}

static inline gdouble
square_area2(gdouble z1, gdouble z2, gdouble z3, gdouble z4,
             gdouble x, gdouble y)
{
    gdouble c;

    c = (z1 + z2 + z3 + z4)/2.0;

    return (sqrt(1 + (z1 - z2)*(z1 - z2)/x + (z1 + z2 - c)*(z1 + z2 - c)/y)
            + sqrt(1 + (z2 - z3)*(z2 - z3)/y + (z2 + z3 - c)*(z2 + z3 - c)/x)
            + sqrt(1 + (z3 - z4)*(z3 - z4)/x + (z3 + z4 - c)*(z3 + z4 - c)/y)
            + sqrt(1 + (z1 - z4)*(z1 - z4)/y + (z1 + z4 - c)*(z1 + z4 - c)/x));
}

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

    return (sqrt(1 + 2*(z1*z1 + z2*z2)/q)
            + sqrt(1 + 2*(z2*z2 + z3*z3)/q)
            + sqrt(1 + 2*(z3*z3 + z4*z4)/q)
            + sqrt(1 + 2*(z4*z4 + z1*z1)/q));
}

/**
 * gwy_data_field_get_surface_area:
 * @data_field: A data field.
 *
 * Computes surface area of a data field.
 *
 * Returns: surface area
 **/
gdouble
gwy_data_field_get_surface_area(GwyDataField *data_field)
{
    gint i, j, xres;
    gdouble x, y, q, sum = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), sum);

    gwy_debug("%s", CTEST(data_field, ARE) ? "cache" : "lame");
    if (CTEST(data_field, ARE))
        return CVAL(data_field, ARE);

    xres = data_field->xres;
    x = data_field->xreal/data_field->xres;
    y = data_field->yreal/data_field->yres;
    q = x*y;
    x = x*x;
    y = y*y;

    if (fabs(log(x/y)) < 1e-7) {
        for (i = 1; i < data_field->yres; i++) {
            gdouble *r = data_field->data + xres*i;

            for (j = 1; j < xres; j++)
                sum += square_area1(r[j], r[j-1], r[j-xres], r[j-xres-1], q);
        }
    }
    else {
        for (i = 1; i < data_field->yres; i++) {
            gdouble *r = data_field->data + xres*i;

            for (j = 1; j < xres; j++)
                sum += square_area2(r[j], r[j-1], r[j-xres], r[j-xres-1], x, y);
        }
    }

    sum *= q/4;
    CVAL(data_field, ARE) = sum;
    data_field->cached |= CBIT(ARE);

    return sum;
}

/**
 * gwy_data_field_area_get_surface_area:
 * @data_field: A data field.
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
gwy_data_field_area_get_surface_area(GwyDataField *dfield,
                                     gint col, gint row,
                                     gint width, gint height)
{
    gint i, j, xres;
    gdouble x, y, q, sum = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);
    if (!width || !height)
        return sum;

    if (col == 0 && width == dfield->xres
        && row == 0 && height == dfield->yres)
        return gwy_data_field_get_surface_area(dfield);

    xres = dfield->xres;
    x = dfield->xreal/dfield->xres;
    y = dfield->yreal/dfield->yres;
    q = x*y;
    x = x*x;
    y = y*y;

    if (fabs(log(x/y)) < 1e-7) {
        for (i = 1; i < height; i++) {
            gdouble *r = dfield->data + xres*(i + row) + col;

            for (j = 1; j < width; j++)
                sum += square_area1(r[j], r[j-1], r[j-xres], r[j-xres-1], q);
        }
    }
    else {
        for (i = 1; i < height; i++) {
            gdouble *r = dfield->data + xres*(i + row) + col;

            for (j = 1; j < width; j++)
                sum += square_area2(r[j], r[j-1], r[j-xres], r[j-xres-1], x, y);
        }
    }

    return sum*q/4;
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
    gdouble *data, *der;
    gdouble bx, by, phi;
    gint xres, yres, nder;
    gint col, row, iphi;

    nder = gwy_data_line_get_res(derdist);
    der = gwy_data_line_get_data(derdist);
    data = dfield->data;
    xres = dfield->xres;
    yres = dfield->yres;
    memset(der, 0, nder*sizeof(gdouble));
    if (kernel_size > 0) {
        for (row = 0; row + kernel_size < yres; row++) {
            for (col = 0; col + kernel_size < xres; col++) {
                gwy_data_field_area_fit_plane(dfield, col, row,
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
}

/**
 * gwy_data_field_area_get_median:
 * @data_field: A data field.
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
                               gint col, gint row, gint width, gint height)
{
    gdouble median = 0.0;
    gdouble *buffer, *datapos;
    gint i;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), median);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         median);
    if (!width || !height)
        return median;

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
            dcx /= dd; sumdx += dcx;
            dcy /= dd; sumdy += dcy;
            dcz /= dd; sumdz += dcz;
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
 * gwy_data_field_normalize:
 * @data_field: A data field.
 *
 * Normalizes data in a data field to range 0.0 to 1.0.
 *
 * If @data_field is filled with only one value, it is changed to 0.0.
 **/
void
gwy_data_field_normalize(GwyDataField *data_field)
{
    gdouble min, max;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    min = gwy_data_field_get_min(data_field);
    if (min)
        gwy_data_field_add(data_field, -min);
    max = gwy_data_field_get_max(data_field);
    if (max)
        gwy_data_field_multiply(data_field, 1.0/max);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
