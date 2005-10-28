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
#include <libprocess/datafield.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>

/**
 * gwy_data_field_get_correlation_score:
 * @data_field: A data field.
 * @kernel_field: Kernel to correlate data field with.
 * @ulcol: Upper-left column position in the data field.
 * @ulrow: Upper-left row position in the data field.
 * @kernel_ulcol: Upper-left column position in kernel field.
 * @kernel_ulrow: Upper-left row position in kernel field.
 * @kernel_brcol: Bottom-right column position in kernel field.
 * @kernel_brrow: Bottom-right row position in kernel field.
 *
 * Computes single correlation score.
 *
 * Correlation window size is given
 * by @kernel_ulcol, @kernel_ulrow, @kernel_brcol, @kernel_brrow,
 * postion of the correlation window on data is given by
 * @ulcol, @ulrow.
 *
 * If anything fails (data too close to boundary, etc.),
 * function returns -1 (none correlation).
 *
 * Returns: Correlation score (between -1 and 1). Number 1 denotes
 *          maximum correlation, -1 none correlation.
 **/
gdouble
gwy_data_field_get_correlation_score(GwyDataField *data_field,
                                     GwyDataField *kernel_field, gint ulcol,
                                     gint ulrow, gint kernel_ulcol,
                                     gint kernel_ulrow, gint kernel_brcol,
                                     gint kernel_brrow)
{
    gint xres, yres, kxres, kyres, i, j;
    gint kwidth, kheight;
    gdouble rms1, rms2, avg1, avg2, sumpoints, score;
    gdouble *data, *kdata;

    g_return_val_if_fail(data_field != NULL && kernel_field != NULL, -1);

    if (kernel_ulcol > kernel_brcol)
        GWY_SWAP(gint, kernel_ulcol, kernel_brcol);

    if (kernel_ulrow > kernel_brrow)
        GWY_SWAP(gint, kernel_ulrow, kernel_brrow);


    xres = data_field->xres;
    yres = data_field->yres;
    kxres = kernel_field->xres;
    kyres = kernel_field->yres;
    kwidth = kernel_brcol - kernel_ulcol;
    kheight = kernel_brrow - kernel_ulrow;

    if (kwidth <= 0 || kheight <= 0) {
        g_warning("Correlation kernel has nonpositive size.");
        return -1;
    }

    /*correlation request outside kernel */
    if (kernel_brcol > kxres || kernel_brrow > kyres)
        return -1;
    /*correlation request outside data field */
    if (ulcol < 0 || ulrow < 0 || (ulcol + kwidth) > xres
        || (ulrow + kheight) > yres)
        return -1;
    if (kernel_ulcol < 0 || kernel_ulrow < 0 || (kernel_ulcol + kwidth) > kxres
        || (kernel_ulrow + kheight) > kyres)
        return -1;

    avg1 = gwy_data_field_area_get_avg(data_field, ulcol, ulrow,
                                       kwidth, kheight);
    avg2 = gwy_data_field_area_get_avg(kernel_field, kernel_ulcol, kernel_ulrow,
                                       kernel_brcol - kernel_ulcol,
                                       kernel_brrow - kernel_ulrow);
    rms1 = gwy_data_field_area_get_rms(data_field, ulcol, ulrow,
                                       kwidth, kheight);
    rms2 = gwy_data_field_area_get_rms(kernel_field, kernel_ulcol, kernel_ulrow,
                                       kernel_brcol - kernel_ulcol,
                                       kernel_brrow - kernel_ulrow);

    score = 0;
    sumpoints = kwidth * kheight;
    data = data_field->data;
    kdata = kernel_field->data;
    for (j = 0; j < kheight; j++) {   /* row */
        for (i = 0; i < kwidth; i++) {   /* col */
            score += (data[(i + ulcol) + xres*(j + ulrow)] - avg1)
                      * (kdata[(i + kernel_ulcol) + kxres*(j + kernel_ulrow)]
                         - avg2);
        }
    }
    score /= rms1 * rms2 * sumpoints;

    return score;
}

/**
 * gwy_data_field_correlate:
 * @data_field: A data field.
 * @kernel_field: Correlation kernel.
 * @score: Data field to store correlation scores to.
 *
 * Computes correlation score for all positions in a data field.
 *
 * Correlation score is compute for all points in data field @data_field
 * and full size of correlation kernel @kernel_field.
 **/
void
gwy_data_field_correlate(GwyDataField *data_field, GwyDataField *kernel_field,
                         GwyDataField *score, GwyCorrelationType method)
{

    gint xres, yres, kxres, kyres, i, j, fftxres, fftyres;
    GwyDataField *data_in_re, *data_out_re, *data_out_im;
    GwyDataField *kernel_in_re, *kernel_out_re, *kernel_out_im;
    gdouble norm;

    g_return_if_fail(data_field != NULL && kernel_field != NULL);

    xres = data_field->xres;
    yres = data_field->yres;
    kxres = kernel_field->xres;
    kyres = kernel_field->yres;

    if (kxres <= 0 || kyres <= 0) {
        g_warning("Correlation kernel has nonpositive size.");
        return;
    }

    switch (method)
    {
        case GWY_CORRELATION_NORMAL:
        gwy_data_field_fill(score, -1);
        /*correlation request outside kernel */
        if (kxres > xres || kyres > yres) {
            return;
        }

        for (i = (kxres/2); i < (xres - kxres/2); i++) {        /*col */
            for (j = (kyres/2); j < (yres - kyres/2); j++) {    /*row */
                score->data[i + xres * j] =
                    gwy_data_field_get_correlation_score(data_field, kernel_field,
                                                     i - kxres/2,
                                                     j - kyres/2, 0, 0, kxres,
                                                     kyres);
            }
        }
        break;

        case GWY_CORRELATION_FFT:
        case GWY_CORRELATION_POC:
        fftxres = gwy_data_field_get_fft_res(xres);
        fftyres = gwy_data_field_get_fft_res(yres);
        data_in_re = gwy_data_field_duplicate(data_field);
        kernel_in_re = gwy_data_field_new_alike(data_field, TRUE);
        gwy_data_field_area_copy(kernel_field, kernel_in_re,
                                 0, 0, kernel_field->xres, kernel_field->yres,
                                 kernel_in_re->xres/2 - kernel_field->xres/2,
                                 kernel_in_re->yres/2 - kernel_field->yres/2);
        gwy_data_field_resample(data_in_re, fftxres, fftyres, GWY_INTERPOLATION_BILINEAR);
        gwy_data_field_resample(kernel_in_re, fftxres, fftyres, GWY_INTERPOLATION_BILINEAR);
        gwy_data_field_resample(score, fftxres, fftyres, GWY_INTERPOLATION_NONE);

        data_out_re = gwy_data_field_new_alike(data_in_re, TRUE);
        data_out_im = gwy_data_field_new_alike(data_in_re, TRUE);
        kernel_out_re = gwy_data_field_new_alike(data_in_re, TRUE);
        kernel_out_im = gwy_data_field_new_alike(data_in_re, TRUE);

        gwy_data_field_2dfft_real(data_in_re, data_out_re, data_out_im, GWY_WINDOWING_NONE,
                                  GWY_TRANSFORM_DIRECTION_FORWARD,
                                  GWY_INTERPOLATION_BILINEAR, FALSE, FALSE);
        gwy_data_field_2dfft_real(kernel_in_re, kernel_out_re, kernel_out_im, GWY_WINDOWING_NONE,
                                  GWY_TRANSFORM_DIRECTION_FORWARD,
                                  GWY_INTERPOLATION_BILINEAR, FALSE, FALSE);

        for (i = 0; i < fftxres*fftyres; i++)
        {
            /*NOTE: now we construct new "complex field" from data and kernel fields, just to save memory*/
            data_in_re->data[i] = data_out_re->data[i]*kernel_out_re->data[i]
                + data_out_im->data[i]*kernel_out_im->data[i];
            kernel_in_re->data[i] = -data_out_re->data[i]*kernel_out_im->data[i]
                + data_out_im->data[i]*kernel_out_re->data[i];
            if (method == GWY_CORRELATION_POC) {
                norm = sqrt(data_in_re->data[i]*data_in_re->data[i] + kernel_in_re->data[i]*kernel_in_re->data[i]);
                data_in_re->data[i] /= norm;
                kernel_in_re->data[i] /= norm;
            }
        }
        gwy_data_field_2dfft(data_in_re, kernel_in_re, score, data_out_im, GWY_WINDOWING_NONE,
                                  GWY_TRANSFORM_DIRECTION_BACKWARD,
                                  GWY_INTERPOLATION_BILINEAR, FALSE, FALSE);
        gwy_data_field_2dfft_humanize(score);
        //gwy_data_field_resample(score, data_field->xres, data_field->yres, GWY_INTERPOLATION_BILINEAR);

        /*TODO compute it and put to score field*/
        g_object_unref(data_in_re);
        g_object_unref(data_out_re);
        g_object_unref(data_out_im);
        g_object_unref(kernel_in_re);
        g_object_unref(kernel_out_re);
        g_object_unref(kernel_out_im);
        break;
    }

    gwy_data_field_invalidate(score);
}

/**
 * gwy_data_field_correlate_iteration:
 * @data_field: A data field.
 * @kernel_field: Kernel to correlate data field with.
 * @score: Data field to store correlation scores to.
 * @state: State of iteration.  It is updated to new state.
 * @iteration: Actual iteration row coordinate.
 *
 * Performs one iteration of correlation.
 **/
void
gwy_data_field_correlate_iteration(GwyDataField *data_field,
                                   GwyDataField *kernel_field,
                                   GwyDataField *score,
                                   GwyComputationStateType *state,
                                   gint *iteration)
{
    gint xres, yres, kxres, kyres, i, j;

    g_return_if_fail(data_field != NULL && kernel_field != NULL);

    xres = data_field->xres;
    yres = data_field->yres;
    kxres = kernel_field->xres;
    kyres = kernel_field->yres;

    if (kxres <= 0 || kyres <= 0) {
        g_warning("Correlation kernel has nonpositive size.");
        return;
    }
    /* correlation request outside kernel */
    if (kxres > xres || kyres > yres) {
        return;
    }

    if (*state == GWY_COMPUTATION_STATE_INIT) {
        gwy_data_field_fill(score, -1);
        *state = GWY_COMPUTATION_STATE_ITERATE;
        *iteration = 0;
    }
    else if (*state == GWY_COMPUTATION_STATE_ITERATE) {
        if (iteration == 0)
            i = (kxres/2);
        else
            i = *iteration;
        for (j = (kyres/2); j < (yres - kyres/2); j++) {    /*row */
            score->data[i + xres * j] =
                gwy_data_field_get_correlation_score(data_field, kernel_field,
                                                     i - kxres/2,
                                                     j - kyres/2,
                                                     0, 0, kxres, kyres);
        }
        *iteration = i + 1;
        if (*iteration == (xres - kxres/2 - 1))
            *state = GWY_COMPUTATION_STATE_FINISHED;
    }

    gwy_data_field_invalidate(score);
}

/**
 * gwy_data_field_crosscorrelate:
 * @data_field1: A data field.
 * @data_field2: A data field.
 * @x_dist: A data field to store x-distances to.
 * @y_dist: A data field to store y-distances to.
 * @score: Data field to store correlation scores to.
 * @search_width: Search area width.
 * @search_height: Search area height.
 * @window_width: Correlation window width.
 * @window_height: Correlation window height.
 *
 * Algorithm for matching two different images of the same object under changes.
 *
 * It does not use any special features
 * for matching. It simply searches for all points (with their neighbourhood)
 * of @data_field1 within @data_field2. Parameters @search_width and
 * @search_height
 * determine maimum area where to search for points. The area is cenetered
 * in the @data_field2 at former position of points at @data_field1.
 **/
void
gwy_data_field_crosscorrelate(GwyDataField *data_field1,
                              GwyDataField *data_field2, GwyDataField *x_dist,
                              GwyDataField *y_dist, GwyDataField *score,
                              gint search_width, gint search_height,
                              gint window_width,
                              gint window_height)
{
    gint xres, yres, i, j, m, n;
    gint imax, jmax;
    gdouble cormax, lscore;

    g_return_if_fail(data_field1 != NULL && data_field2 != NULL);

    xres = data_field1->xres;
    yres = data_field1->yres;

    g_return_if_fail(xres == data_field2->xres && yres == data_field2->yres);

    gwy_data_field_clear(x_dist);
    gwy_data_field_clear(y_dist);
    gwy_data_field_clear(score);

    /*iterate over all the points */
    for (i = (search_width/2); i < (xres - search_height/2); i++) {
        for (j = (search_height/2); j < (yres - search_height/2); j++) {
            /*iterate over search area in the second datafield */
            imax = i;
            jmax = j;
            cormax = -1;
            for (m = (i - search_width); m < i; m++) {
                for (n = (j - search_height); n < j; n++) {
                    lscore =
                        gwy_data_field_get_correlation_score(data_field1,
                                                             data_field2,
                                                             i-search_width/2,
                                                             j-search_height/2,
                                                             m, n,
                                                             m + search_width,
                                                             n + search_height);

                    /* add a little to score at exactly same point
                     * - to prevent problems on flat data */
                    if (m == (i - search_width/2)
                        && n == (j - search_height/2))
                        lscore *= 1.0001;

                    if (cormax < lscore) {
                        cormax = lscore;
                        imax = m + search_width/2;
                        jmax = n + search_height/2;
                    }
                }
            }
            score->data[i + xres * j] = cormax;
            x_dist->data[i + xres * j]
                = (gdouble)(imax - i)*data_field1->xreal/data_field1->xres;
            y_dist->data[i + xres * j]
                = (gdouble)(jmax - j)*data_field1->yreal/data_field1->yres;
        }
    }

    gwy_data_field_invalidate(score);
    gwy_data_field_invalidate(x_dist);
    gwy_data_field_invalidate(y_dist);
}

/**
 * gwy_data_field_crosscorrelate_iteration:
 * @data_field1: A data field.
 * @data_field2: A data field.
 * @x_dist: A data field to store x-distances to.
 * @y_dist: A data field to store y-distances to.
 * @score: Data field to store correlation scores to.
 * @search_width: Search area width.
 * @search_height: Search area height.
 * @window_width: Correlation window width.
 * @window_height: Correlation window height.
 * @state: State of iteration.  It is updated to new state.
 * @iteration: Iteration of computation loop (within
 *             %GWY_COMPUTATION_STATE_ITERATE state).
 *
 * Matches two different images of the same object under changes.
 *
 * It does not use any special features
 * for matching. It simply searches for all points (with their neighbourhood)
 * of @data_field1 within @data_field2. Parameters @search_width and
 * @search_height
 * determine maimum area where to search for points. The area is cenetered
 * in the @data_field2 at former position of points at @data_field1.
 **/
void
gwy_data_field_crosscorrelate_iteration(GwyDataField *data_field1,
                                        GwyDataField *data_field2,
                                        GwyDataField *x_dist,
                                        GwyDataField *y_dist,
                                        GwyDataField *score,
                                        gint search_width, gint search_height,
                                        gint window_width,
                                        gint window_height,
                                        GwyComputationStateType * state,
                                        gint *iteration)
{
    gint xres, yres, i, j, m, n;
    gint imax, jmax;
    gdouble cormax, lscore;

    g_return_if_fail(data_field1 != NULL && data_field2 != NULL);

    xres = data_field1->xres;
    yres = data_field1->yres;

    g_return_if_fail(xres == data_field2->xres && yres == data_field2->yres);

    if (*state == GWY_COMPUTATION_STATE_INIT) {
        gwy_data_field_clear(x_dist);
        gwy_data_field_clear(y_dist);
        gwy_data_field_clear(score);
        *state = GWY_COMPUTATION_STATE_ITERATE;
        *iteration = 0;
    }
    else if (*state == GWY_COMPUTATION_STATE_ITERATE) {
        if (iteration == 0)
            i = (search_width/2);
        else
            i = *iteration;

        for (j = (search_height/2); j < (yres - search_height/2); j++) {
            /*iterate over search area in the second datafield */
            imax = i;
            jmax = j;
            cormax = -1;
            for (m = (i - search_width); m < i; m++) {
                for (n = (j - search_height); n < j; n++) {
                    lscore =
                        gwy_data_field_get_correlation_score(data_field1,
                                                             data_field2,
                                                             i-search_width/2,
                                                             j-search_height/2,
                                                             m, n,
                                                             m + search_width,
                                                             n + search_height);

                    /* add a little to score at exactly same point
                     * - to prevent problems on flat data */
                    if (m == (i - search_width/2)
                        && n == (j - search_height/2))
                        lscore *= 1.01;

                    if (cormax < lscore) {
                        cormax = lscore;
                        imax = m + search_width/2;
                        jmax = n + search_height/2;
                    }

                }
            }
            score->data[i + xres * j] = cormax;
            x_dist->data[i + xres * j]
                = (gdouble)(imax - i)*data_field1->xreal/data_field1->xres;
            y_dist->data[i + xres * j]
                = (gdouble)(jmax - j)*data_field1->yreal/data_field1->yres;

        }
        *iteration = i + 1;
        if (*iteration == (xres - search_height/2))
            *state = GWY_COMPUTATION_STATE_FINISHED;
    }

    gwy_data_field_invalidate(score);
    gwy_data_field_invalidate(x_dist);
    gwy_data_field_invalidate(y_dist);
}

/************************** Documentation ****************************/

/**
 * SECTION:correlation
 * @title: correlation
 * @short_description: Correlation and crosscorrelation
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

