/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"

/**
 * gwy_data_field_correct_laplace_iteration:
 * @data_field: data field to be corrected
 * @mask_field: mask of places to be corrected
 * @buffer_field: initialized to same size aa mask and data
 * @error: maximum change within last step
 * @corfactor: correction factor within step.
 *
 * Tries to remove all the points in mask off the data by using
 * iterative method similar to solving heat flux equation.
 * Use this function repeatedly until reasonable @error is reached.
 **/
void
gwy_data_field_correct_laplace_iteration(GwyDataField *data_field,
                                         GwyDataField *mask_field,
                                         GwyDataField *buffer_field,
                                         gdouble *error, gdouble *corfactor)
{
    gint xres, yres, i, j;
    gdouble cor;

    xres = data_field->xres;
    yres = data_field->yres;

    /*check buffer field */
    if (buffer_field == NULL) {
        buffer_field
            = (GwyDataField *)gwy_data_field_new(xres, yres, data_field->xreal,
                                                 data_field->yreal, TRUE);
    }
    if (buffer_field->xres != xres || buffer_field->yres != yres) {
        gwy_data_field_resample(buffer_field, xres, yres,
                                GWY_INTERPOLATION_NONE);
    }
    gwy_data_field_copy(data_field, buffer_field);

    /*set boundary condition for masked boundary data */
    for (i = 0; i < xres; i++) {
        if (mask_field->data[i] != 0)
            buffer_field->data[i] = buffer_field->data[i + 2*xres];
        if (mask_field->data[i + xres*(yres - 1)] != 0)
            buffer_field->data[i + xres*(yres - 1)] =
                buffer_field->data[i + xres*(yres - 3)];
    }
    for (i = 0; i < yres; i++) {
        if (mask_field->data[xres*i] != 0)
            buffer_field->data[xres*i] = buffer_field->data[2 + xres*i];
        if (mask_field->data[yres - 1 + xres*i] != 0)
            buffer_field->data[yres - 1 + xres*i] =
                buffer_field->data[yres - 3 + xres*i];
    }

    *error = 0;
    /*iterate */
    for (i = 1; i < (xres - 1); i++) {
        for (j = 1; j < (yres - 1); j++) {
            if (mask_field->data[i + xres*j] != 0) {
                cor = (*corfactor)*((data_field->data[i + 1 + xres*j]
                                     + data_field->data[i - 1 + xres*j]
                                     - 2*data_field->data[i + xres*j])
                                    + (data_field->data[i + xres*(j + 1)]
                                       + data_field->data[i + xres*(j - 1)]
                                       - 2*data_field->data[i + xres*j]));

                buffer_field->data[i + xres*j] += cor;
                if (fabs(cor) > (*error))
                    (*error) = fabs(cor);
            }
        }
    }

    gwy_data_field_copy(buffer_field, data_field);

}

/**
 * gwy_data_field_mask_outliers:
 * @data_field: data field
 * @mask_field: mask to be changed
 * @thresh: threshold value
 *
 * Creates mask of data that are above or below
 * thresh*sigma from average height. Sigma denotes root-mean square deviation
 * of heights. This criterium corresponds
 * to usual Gaussian distribution outliers detection for thresh = 3.
 **/
void
gwy_data_field_mask_outliers(GwyDataField *data_field,
                             GwyDataField *mask_field,
                             gdouble thresh)
{
    gdouble avg;
    gdouble criterium;
    gint i;

    avg = gwy_data_field_get_avg(data_field);
    criterium = gwy_data_field_get_rms(data_field) * thresh;

    for (i = 0; i < (data_field->xres * data_field->yres); i++) {
        if (fabs(data_field->data[i] - avg) > criterium)
            mask_field->data[i] = 1;
        else
            mask_field->data[i] = 0;
    }

}

/**
 * gwy_data_field_correct_average:
 * @data_field: data field
 * @mask_field: mask to be used for changes
 *
 * Function simply puts average value of all the @data_field into
 * points in @data_field lying under points where @mask_field values
 * are nonzero.
 **/
void
gwy_data_field_correct_average(GwyDataField *data_field,
                               GwyDataField *mask_field)
{
    gdouble avg;
    gint i;

    avg = gwy_data_field_get_avg(data_field);

    for (i = 0; i < (data_field->xres * data_field->yres); i++) {
        if (mask_field->data[i])
            data_field->data[i] = avg;
    }

}

/**
 * gwy_data_field_mark_scars:
 * @data_field: A data field to find scars in.
 * @scar_field: A data field to store the result to (it is resized to match
 *              @data_field).
 * @threshold_high: Miminum relative step for scar marking, must be positive.
 * @threshold_low: Definite relative step for scar marking, must be at least
 *                 equal to @threshold_high.
 * @min_scar_len: Minimum length of a scar, shorter ones are discarded
 *                (must be at least one).
 * @max_scar_width: Maximum width of a scar, must be at least one.
 *
 * Find and marks scars in a data field.
 *
 * Scars are linear horizontal defects, consisting of shifted values.
 * Zero or negative values in @scar_field siginify normal data, positive
 * values siginify samples that are part of a scar.
 *
 * Since: 1.4.
 **/
void
gwy_data_field_mark_scars(GwyDataField *data_field,
                          GwyDataField *scar_field,
                          gdouble threshold_high,
                          gdouble threshold_low,
                          gdouble min_scar_len,
                          gdouble max_scar_width)
{
    gint xres, yres, i, j, k;
    gdouble rms;
    gdouble *d, *m;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(scar_field));
    g_return_if_fail(max_scar_width >= 1 && max_scar_width <= 16);
    g_return_if_fail(min_scar_len >= 1);
    g_return_if_fail(threshold_low > 0.0);
    g_return_if_fail(threshold_high >= threshold_low);
    xres = gwy_data_field_get_xres(data_field);
    yres = gwy_data_field_get_yres(data_field);
    d = gwy_data_field_get_data(data_field);
    gwy_data_field_resample(scar_field, xres, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_fill(scar_field, 0.0);
    m = gwy_data_field_get_data(scar_field);

    if (min_scar_len > xres)
        return;
    max_scar_width = MIN(max_scar_width, yres - 2);

    /* compute `vertical rms' */
    rms = 0.0;
    for (i = 0; i < yres-1; i++) {
        gdouble *row = d + i*xres;

        for (j = 0; j < xres; j++) {
            gdouble z = row[j] - row[j + xres];

            rms += z*z;
        }
    }
    rms = sqrt(rms/(xres*yres));
    if (rms == 0.0)
        return;

    /* initial scar search */
    for (i = 0; i < yres - (max_scar_width + 1); i++) {
        for (j = 0; j < xres; j++) {
            gdouble top, bottom;
            gdouble *row = d + i*xres + j;

            bottom = row[0];
            top = row[xres];
            for (k = 1; k <= max_scar_width; k++) {
                bottom = MAX(row[0], row[xres*(k + 1)]);
                top = MIN(top, row[xres*k]);
                if (top - bottom >= threshold_low*rms)
                    break;
            }
            if (k <= max_scar_width) {
                gdouble *mrow = m + i*xres + j;

                while (k) {
                    mrow[k*xres] = (row[k*xres] - bottom)/rms;
                    k--;
                }
            }
        }
    }
    /* expand high threshold to neighbouring low threshold */
    for (i = 0; i < yres; i++) {
        gdouble *mrow = m + i*xres;

        for (j = 1; j < xres; j++) {
            if (mrow[j] >= threshold_low && mrow[j-1] >= threshold_high)
                mrow[j] = threshold_high;
        }
        for (j = xres-1; j > 0; j--) {
            if (mrow[j-1] >= threshold_low && mrow[j] >= threshold_high)
                mrow[j-1] = threshold_high;
        }
    }
    /* kill too short segments, clamping scar_field along the way */
    for (i = 0; i < yres; i++) {
        gdouble *mrow = m + i*xres;

        k = 0;
        for (j = 0; j < xres; j++) {
            if (mrow[j] >= threshold_high) {
                mrow[j] = 1.0;
                k++;
            }
            else if (k && k < min_scar_len) {
                while (k) {
                    mrow[j-k] = 0.0;
                    k--;
                }
                mrow[j] = 0.0;
            }
            else
                mrow[j] = 0.0;
        }
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
