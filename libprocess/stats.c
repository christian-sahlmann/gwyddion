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

/* Private DataLine functions */
void            _gwy_data_line_initialize        (GwyDataLine *a,
                                                  gint res, gdouble real,
                                                  gboolean nullme);
void            _gwy_data_line_free              (GwyDataLine *a);

static gdouble  square_area                      (GwyDataField *data_field,
                                                  gint ulcol, gint ulrow,
                                                  gint brcol, gint brrow);

/**
 * gwy_data_field_get_max:
 * @data_field: A data field.
 *
 * Finds maximum value of a data field.
 *
 * Returns: The maximum value.
 **/
gdouble
gwy_data_field_get_max(GwyDataField *a)
{
    gint i;
    gdouble max = a->data[0];
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (max < *p)
            max = *p;
    }
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
 * Finds maximum value in a rectangular part of a data field.
 *
 * Returns: The maximum value.
 *
 * Since: 1.2:
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

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            if (max < *drow)
                max = *drow;
            drow++;
        }
    }

    return max;
}

gdouble
gwy_data_field_get_area_max(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_max(a, ulcol, ulrow,
                                       brcol - ulcol, brrow - ulrow);
}

/**
 * gwy_data_field_get_min:
 * @data_field: A data field.
 *
 * Finds minimum value of a data field.
 *
 * Returns: The minimum value.
 **/
gdouble
gwy_data_field_get_min(GwyDataField *a)
{
    gint i;
    gdouble min = a->data[0];
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++) {
        if (min > *p)
            min = *p;
    }
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
 * Finds minimum value in a rectangular part of a data field.
 *
 * Returns: The minimum value.
 *
 * Since: 1.2
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

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++) {
            if (min > *drow)
                min = *drow;
            drow++;
        }
    }

    return min;
}

gdouble
gwy_data_field_get_area_min(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_min(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
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
gwy_data_field_get_sum(GwyDataField *a)
{
    gint i;
    gdouble sum = 0;
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        sum += *p;

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
 *
 * Since: 1.2
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

    datapos = dfield->data + row*dfield->xres + col;
    for (i = 0; i < height; i++) {
        gdouble *drow = datapos + i*dfield->xres;

        for (j = 0; j < width; j++)
            sum += *(drow++);
    }

    return sum;
}

gdouble
gwy_data_field_get_area_sum(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_sum(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
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
gwy_data_field_get_avg(GwyDataField *a)
{
    return gwy_data_field_get_sum(a)/((gdouble)(a->xres * a->yres));
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
 *
 * Since: 1.2.
 **/
gdouble
gwy_data_field_area_get_avg(GwyDataField *dfield,
                            gint col, gint row, gint width, gint height)
{
    return gwy_data_field_area_get_sum(dfield, col, row,
                                       width, height)/(width*height);
}

gdouble
gwy_data_field_get_area_avg(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_avg(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
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
gwy_data_field_get_rms(GwyDataField *a)
{
    gint i, n;
    gdouble rms, sum2 = 0;
    gdouble sum = gwy_data_field_get_sum(a);
    gdouble *p = a->data;

    for (i = a->xres * a->yres; i; i--, p++)
        sum2 += (*p)*(*p);

    n = a->xres * a->yres;
    rms = sqrt(fabs(sum2 - sum*sum/n)/n);

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
 *
 * Since: 1.2.
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

gdouble
gwy_data_field_get_area_rms(GwyDataField *a,
                            gint ulcol,
                            gint ulrow,
                            gint brcol,
                            gint brrow)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_rms(a, ulcol, ulrow,
                                       brcol-ulcol, brrow-ulrow);
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

void
gwy_data_field_get_area_stats(GwyDataField *data_field,
                              gint ulcol,
                              gint ulrow,
                              gint brcol,
                              gint brrow,
                              gdouble *avg,
                              gdouble *ra,
                              gdouble *rms,
                              gdouble *skew,
                              gdouble *kurtosis)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    gwy_data_field_area_get_stats(data_field, ulcol, ulrow,
                                  brcol-ulcol, brrow-ulrow,
                                  avg, ra, rms, skew, kurtosis);
}


/* FIXME: fix return value to boolean */
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
gint
gwy_data_field_get_line_stat_function(GwyDataField *data_field,
                                      GwyDataLine *target_line,
                                      gint ulcol, gint ulrow,
                                      gint brcol, gint brrow,
                                      GwySFOutputType type,
                                      GtkOrientation orientation,
                                      GwyInterpolationType interpolation,
                                      GwyWindowingType windowing, gint nstats)
{
    gint i, k, j, size;
    GwyDataLine *hlp_line;
    GwyDataLine *hlp_tarline;
    gdouble min = G_MAXDOUBLE, max = -G_MAXDOUBLE, val, realsize;

    gwy_debug("");
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    /* precompute settings if necessary */
    if (type == GWY_SF_OUTPUT_DH || type == GWY_SF_OUTPUT_CDH) {
        min = gwy_data_field_area_get_min(data_field,
                                          ulcol, ulrow,
                                          brcol-ulcol, brrow-ulrow);
        max = gwy_data_field_area_get_max(data_field,
                                          ulcol, ulrow,
                                          brcol-ulcol, brrow-ulrow);
    }
    else if (type == GWY_SF_OUTPUT_DA || type == GWY_SF_OUTPUT_CDA) {
        if (orientation == GTK_ORIENTATION_HORIZONTAL) {
            for (i = ulcol; i < brcol; i++)
                for (j = ulrow; j < brrow; j++) {
                    val = gwy_data_field_get_xder(data_field, i, j);
                    if (min > val)
                        min = val;
                    if (max < val)
                        max = val;
                }
        }
        else if (orientation == GTK_ORIENTATION_VERTICAL) {
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
    if (orientation == GTK_ORIENTATION_HORIZONTAL) {
        size = brcol-ulcol;
        if (size < 10) {
            g_warning("Field too small");
            return 0;
        }
        if (type == GWY_SF_OUTPUT_PSDF && size < 64) {
            g_warning("Field too small");
            return 0;
        }

        realsize = gwy_data_field_jtor(data_field, size);
        hlp_line = GWY_DATA_LINE(gwy_data_line_new(size, realsize, FALSE));
        hlp_tarline = GWY_DATA_LINE(gwy_data_line_new(size, realsize, FALSE));

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
    else if (orientation == GTK_ORIENTATION_VERTICAL) {
        size = brrow-ulrow;
        if (size < 10) {
            g_warning("Field too small");
            return 0;
        }

        realsize = gwy_data_field_itor(data_field, size);
        hlp_line = GWY_DATA_LINE(gwy_data_line_new(size, realsize, FALSE));
        hlp_tarline = GWY_DATA_LINE(gwy_data_line_new(size, realsize, FALSE));

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
    return 1;
}

/**
 * gwy_data_field_get_surface_area:
 * @data_field: A data field.
 * @interpolation: interpolation method
 *
 * Computes surface area of a data field.
 *
 * Returns: surface area
 **/
gdouble
gwy_data_field_get_surface_area(GwyDataField *a,
                                G_GNUC_UNUSED
                                GwyInterpolationType interpolation)
{
    gint i, j;
    gdouble sum = 0;

    for (i = 0; i < (a->xres - 1); i++) {
        for (j = 0; j < (a->yres - 1); j++)
            sum += square_area(a, i, j, i + 1, j + 1);
    }
    return sum;
}

/**
 * gwy_data_field_area_get_surface_area:
 * @data_field: A data field.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @interpolation: Interpolation method.
 *
 * Computes surface area of a rectangular part of a data field.
 *
 * Returns: The surface area.
 *
 * Since: 1.2.
 **/
gdouble
gwy_data_field_area_get_surface_area(GwyDataField *dfield,
                                     gint col, gint row,
                                     gint width, gint height,
                                     G_GNUC_UNUSED
                                     GwyInterpolationType interpolation)
{
    gint i, j;
    gdouble sum = 0.0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), sum);
    g_return_val_if_fail(col >= 0 && row >= 0
                         && width >= 0 && height >= 0
                         && col + width <= dfield->xres
                         && row + height <= dfield->yres,
                         sum);
    if (!width || !height)
        return sum;

    for (i = 0; i < height-1; i++) {
        for (j = 0; j < width-1; j++)
            sum += square_area(dfield, col+j, row+i, col+j+1, row+i+1);
    }

    return sum;
}

gdouble
gwy_data_field_get_area_surface_area(GwyDataField *a,
                                     gint ulcol,
                                     gint ulrow,
                                     gint brcol,
                                     gint brrow,
                                     GwyInterpolationType interpolation)
{
    if (ulcol > brcol)
        GWY_SWAP(gint, ulcol, brcol);
    if (ulrow > brrow)
        GWY_SWAP(gint, ulrow, brrow);

    return gwy_data_field_area_get_surface_area(a, ulcol, ulrow,
                                                brcol-ulcol, brrow-ulrow,
                                                interpolation);
}

/**
 * square_area:
 * @data_field: data
 * @ulcol: upper-left coordinate (in pixel units)
 * @ulrow: upper-left coordinate (in pixel units)
 * @brcol: bottom-right coordinate (in pixel units)
 * @brrow: bottom-right coordinate (in pixel units)
 *
 * Computes surface area of a rectangular part of a data field.
 *
 * Returns: Surface area (in real units).
 *
 * Since: 1.1
 **/
static gdouble
square_area(GwyDataField *data_field, gint ulcol, gint ulrow, gint brcol,
            gint brrow)
{
    gdouble x, z1, z2, z3, z4, a, b, c, d, e, f, s1, s2, sa, sb, res;

    /* FIXME: this does not work when x and y measures are different */
    x = data_field->xreal / data_field->xres;

    z1 = data_field->data[(ulcol) + data_field->xres * (ulrow)];
    z2 = data_field->data[(brcol) + data_field->xres * (ulrow)];
    z3 = data_field->data[(ulcol) + data_field->xres * (brrow)];
    z4 = data_field->data[(brcol) + data_field->xres * (brrow)];

    a = sqrt(x * x + (z1 - z2) * (z1 - z2));
    b = sqrt(x * x + (z1 - z3) * (z1 - z3));
    c = sqrt(x * x + (z3 - z4) * (z3 - z4));
    d = sqrt(x * x + (z2 - z4) * (z2 - z4));
    e = sqrt(2 * x * x + (z3 - z2) * (z3 - z2));
    f = sqrt(2 * x * x + (z4 - z1) * (z4 - z1));

    s1 = (a + b + e)/2;
    s2 = (c + d + e)/2;
    sa = sqrt(s1 * (s1 - a) * (s1 - b) * (s1 - e))
         + sqrt(s2 * (s2 - c) * (s2 - d) * (s2 - e));

    s1 = (a + d + f)/2;
    s2 = (c + b + f)/2;
    sb = sqrt(s1 * (s1 - a) * (s1 - d) * (s1 - f))
         + sqrt(s2 * (s2 - c) * (s2 - b) * (s2 - f));

    if (sa < sb)
        res = sa;
    else
        res = sb;

    return res;
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
 *
 * Since: 1.4.
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
    data = gwy_data_field_get_data(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
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
                der[iphi] += sqrt(bx*bx + by*by);
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
                der[iphi] += sqrt(bx*bx + by*by);
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
 *
 * Since: 1.7
 **/
gdouble
gwy_data_field_area_get_median(GwyDataField *dfield,
                               gint col, gint row, gint width, gint height)
{
    gdouble median = 0.0/0.0;  /* NaN */
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
 *
 * Since: 1.7
 **/
gdouble
gwy_data_field_get_median(GwyDataField *dfield)
{
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), 0.0/0.0);
    return gwy_data_field_area_get_median(dfield,
                                          0, 0, dfield->xres, dfield->yres);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
