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

#define GWY_DATA_FIELD_RAW_ACCESS
#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libprocess/correlation.h>

/**
 * gwy_data_field_get_correlation_score:
 * @data_field: A data field.
 * @kernel_field: Kernel to correlate data field with.
 * @col: Upper-left column position in the data field.
 * @row: Upper-left row position in the data field.
 * @kernel_col: Upper-left column position in kernel field.
 * @kernel_row: Upper-left row position in kernel field.
 * @kernel_width: Width of kernel field area.
 * @kernel_height: Heigh of kernel field area.
 *
 * Calculates a correlation score in one point.
 *
 * Correlation window size is given
 * by @kernel_col, @kernel_row, @kernel_width, @kernel_height,
 * postion of the correlation window on data is given by
 * @col, @row.
 *
 * If anything fails (data too close to boundary, etc.),
 * function returns -1.0 (none correlation)..
 *
 * Returns: Correlation score (between -1.0 and 1.0). Value 1.0 denotes
 *          maximum correlation, -1.0 none correlation.
 **/
gdouble
gwy_data_field_get_correlation_score(GwyDataField *data_field,
                                     GwyDataField *kernel_field,
                                     gint col,
                                     gint row,
                                     gint kernel_col,
                                     gint kernel_row,
                                     gint kernel_width,
                                     gint kernel_height)
{
    gint xres, yres, kxres, kyres, i, j;
    gdouble rms1, rms2, avg1, avg2, sumpoints, score;
    gdouble *data, *kdata;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), -1.0);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(kernel_field), -1.0);

    xres = data_field->xres;
    yres = data_field->yres;
    kxres = kernel_field->xres;
    kyres = kernel_field->yres;
    kernel_width = kernel_width;
    kernel_height = kernel_height;

    /* correlation request outside kernel */
    if (kernel_col > kxres || kernel_row > kyres)
        return -1;

    /* correlation request outside data field */
    if (col < 0 || row < 0
        || col + kernel_width > xres
        || row + kernel_height > yres)
        return -1;
    if (kernel_col < 0
        || kernel_row < 0
        || kernel_col + kernel_width > kxres
        || kernel_row + kernel_height > kyres)
        return -1;

    avg1 = gwy_data_field_area_get_avg(data_field, NULL,
                                       col, row, kernel_width, kernel_height);
    avg2 = gwy_data_field_area_get_avg(kernel_field, NULL,
                                       kernel_col, kernel_row,
                                       kernel_width, kernel_height);
    rms1 = gwy_data_field_area_get_rms(data_field, NULL,
                                       col, row, kernel_width, kernel_height);
    rms2 = gwy_data_field_area_get_rms(kernel_field, NULL,
                                       kernel_col, kernel_row,
                                       kernel_width, kernel_height);

    score = 0;
    sumpoints = kernel_width * kernel_height;
    data = data_field->data;
    kdata = kernel_field->data;
    for (j = 0; j < kernel_height; j++) {   /* row */
        for (i = 0; i < kernel_width; i++) {   /* col */
            score += (data[(i + col) + xres*(j + row)] - avg1)
                      * (kdata[(i + kernel_col) + kxres*(j + kernel_row)]
                         - avg2);
        }
    }
    score /= rms1 * rms2 * sumpoints;

    return score;
}

/**
 * calculate_normalization:
 * @data_field: A data field.
 * @kernel_width: Width of kernel field.
 * @kernel_height: Heigh of kernel field.
 *
 * Precalculate data field area normalizations.
 *
 * The returned array has dimensions
 * (width - kernel_width + 1)x(height - kernel_height + 1)
 * and it is stored by rows as
 * avg, rms, avg, rms, ...
 *
 * Returns: A newly allocated array.
 **/
static gdouble*
calculate_normalization(GwyDataField *data_field,
                        gint kernel_width, gint kernel_height)
{
    gdouble *sum_sum2, *avg_rms, *row;
    const gdouble *drow;
    gint i, j, w, h, p;

    w = data_field->xres - kernel_width + 1;
    h = data_field->yres - kernel_height + 1;
    g_return_val_if_fail(w > 0 && h > 0, NULL);

    /* Row-wise averages */
    sum_sum2 = g_new(gdouble, 2*w*data_field->yres);
    for (i = 0; i < data_field->yres; i++) {
        row = sum_sum2 + 2*i*w;
        drow = data_field->data + i*data_field->xres;
        row[0] = row[1] = 0.0;
        for (j = 0; j < kernel_width; j++) {
            row[0] += drow[j];
            row[1] += drow[j]*drow[j];
        }
        for (j = kernel_width; j < data_field->xres; j++) {
            p = j - kernel_width;
            row[2*(p + 1) + 0] = row[2*p + 0] + drow[j] - drow[p];
            row[2*(p + 1) + 1] = row[2*p + 1] + (drow[j]*drow[j]
                                                 - drow[p]*drow[p]);
        }
    }

    /* Column-wise averages */
    avg_rms = g_new(gdouble, 2*w*h);
    row = avg_rms;
    for (j = 0; j < 2*w; j++)
        row[j] = 0.0;
    for (i = 0; i < kernel_height; i++) {
        drow = sum_sum2 + 2*i*w;
        for (j = 0; j < 2*w; j++)
            row[j] += drow[j];
    }
    for (i = kernel_height; i < data_field->yres; i++) {
        p = i - kernel_height;
        row = avg_rms + 2*p*w;
        drow = sum_sum2 + 2*p*w;
        for (j = 0; j < 2*w; j++)
            row[2*w + j] = row[j] + drow[2*w*kernel_height + j] - drow[j];
    }

    g_free(sum_sum2);

    for (i = 0; i < w*h; i++) {
        avg_rms[2*i] /= kernel_width*kernel_height;
        avg_rms[2*i + 1] = (avg_rms[2*i + 1]/kernel_width/kernel_height
                            - avg_rms[2*i]*avg_rms[2*i]);
        avg_rms[2*i + 1] = sqrt(MAX(avg_rms[2*i + 1], 0.0));
    }

    return avg_rms;
}

/**
 * gwy_data_field_get_raw_correlation_score:
 * @data_field: A data field.
 * @kernel_field: Kernel to correlate data field with.
 * @col: Upper-left column position in the data field.
 * @row: Upper-left row position in the data field.
 * @kernel_col: Upper-left column position in kernel field.
 * @kernel_row: Upper-left row position in kernel field.
 * @kernel_width: Width of kernel field area.
 * @kernel_height: Heigh of kernel field area.
 * @data_avg: Mean value of the effective data field area.
 * @data_rms: Mean value of the effective kernel field area.
 *
 * Calculates a raw correlation score in one point.
 *
 * See gwy_data_field_get_correlation_score() for description.  This function
 * is useful if you know the mean values and rms.
 *
 * To obtain the score, divide the returned value with the product of rms of
 * data field area and rms of the kernel.
 *
 * Returns: Correlation score (normalized to multiple of kernel and data
 *          area rms).
 **/
static gdouble
gwy_data_field_get_raw_correlation_score(GwyDataField *data_field,
                                         GwyDataField *kernel_field,
                                         gint col,
                                         gint row,
                                         gint kernel_col,
                                         gint kernel_row,
                                         gint kernel_width,
                                         gint kernel_height,
                                         gdouble data_avg,
                                         gdouble kernel_avg)
{
    gint xres, yres, kxres, kyres, i, j;
    gdouble sumpoints, score;
    gdouble *data, *kdata;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), -1.0);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(kernel_field), -1.0);

    xres = data_field->xres;
    yres = data_field->yres;
    kxres = kernel_field->xres;
    kyres = kernel_field->yres;
    kernel_width = kernel_width;
    kernel_height = kernel_height;

    /* correlation request outside kernel */
    if (kernel_col > kxres || kernel_row > kyres)
        return -1;

    /* correlation request outside data field */
    if (col < 0 || row < 0
        || col + kernel_width > xres
        || row + kernel_height > yres)
        return -1;
    if (kernel_col < 0
        || kernel_row < 0
        || kernel_col + kernel_width > kxres
        || kernel_row + kernel_height > kyres)
        return -1;

    score = 0;
    sumpoints = kernel_width * kernel_height;
    data = data_field->data;
    kdata = kernel_field->data;
    for (j = 0; j < kernel_height; j++) {   /* row */
        for (i = 0; i < kernel_width; i++) {   /* col */
            score += (data[(i + col) + xres*(j + row)] - data_avg)
                      * (kdata[(i + kernel_col) + kxres*(j + kernel_row)]
                         - kernel_avg);
        }
    }
    score /= sumpoints;

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

        {
            gdouble *avg_rms;
            gdouble s, davg, drms, kavg, krms;
            gint w, h, p;

            kavg = gwy_data_field_get_avg(kernel_field);
            krms = gwy_data_field_get_rms(kernel_field);
            avg_rms = calculate_normalization(data_field, kxres, kyres);
            w = xres - kxres + 1;
            h = yres - kyres + 1;
            for (i = kyres/2; i < yres - (kyres - kyres/2); i++) {
                for (j = kxres/2; j < xres - (kxres - kxres/2); j++) {
                    p = (i - kyres/2)*w + (j - kxres/2);
                    davg = avg_rms[2*p + 0];
                    drms = avg_rms[2*p + 1];
                    s = gwy_data_field_get_raw_correlation_score(data_field,
                                                                 kernel_field,
                                                                 j - kxres/2,
                                                                 i - kyres/2,
                                                                 0, 0,
                                                                 kxres, kyres,
                                                                 davg, kavg);
                    score->data[i*xres + j] = s/(drms*krms);
                }
            }
            g_free(avg_rms);
        }
        break;

        case GWY_CORRELATION_FFT:
        case GWY_CORRELATION_POC:
        fftxres = gwy_fft_find_nice_size(xres);
        fftyres = gwy_fft_find_nice_size(yres);
        data_in_re = gwy_data_field_new_resampled(data_field,
                                                  fftxres, fftyres,
                                                  GWY_INTERPOLATION_BILINEAR);
        kernel_in_re = gwy_data_field_new_alike(data_field, TRUE);
        gwy_data_field_area_copy(kernel_field, kernel_in_re,
                                 0, 0, kernel_field->xres, kernel_field->yres,
                                 kernel_in_re->xres/2 - kernel_field->xres/2,
                                 kernel_in_re->yres/2 - kernel_field->yres/2);
        gwy_data_field_resample(kernel_in_re, fftxres, fftyres,
                                GWY_INTERPOLATION_BILINEAR);
        gwy_data_field_resample(score, fftxres, fftyres,
                                GWY_INTERPOLATION_NONE);

        data_out_re = gwy_data_field_new_alike(data_in_re, TRUE);
        data_out_im = gwy_data_field_new_alike(data_in_re, TRUE);
        kernel_out_re = gwy_data_field_new_alike(data_in_re, TRUE);
        kernel_out_im = gwy_data_field_new_alike(data_in_re, TRUE);

        gwy_data_field_2dfft(data_in_re, NULL, data_out_re, data_out_im,
                             GWY_WINDOWING_NONE,
                             GWY_TRANSFORM_DIRECTION_FORWARD,
                             GWY_INTERPOLATION_BILINEAR, FALSE, FALSE);
        gwy_data_field_2dfft(kernel_in_re, NULL, kernel_out_re, kernel_out_im,
                             GWY_WINDOWING_NONE,
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
        gwy_data_field_2dfft(data_in_re, kernel_in_re, score, data_out_im,
                             GWY_WINDOWING_NONE,
                             GWY_TRANSFORM_DIRECTION_BACKWARD,
                             GWY_INTERPOLATION_BILINEAR, FALSE, FALSE);
        gwy_data_field_2dfft_humanize(score);

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
 * gwy_data_field_correlate_init:
 * @state: Uninitialized correlation state.
 * @data_field: A data field.
 * @kernel_field: Kernel to correlate data field with.
 * @score: Data field to store correlation scores to.
 *
 * Initializes a correlation iterator.
 **/
void
gwy_data_field_correlate_init(GwyCorrelationState *state,
                              GwyDataField *data_field,
                              GwyDataField *kernel_field,
                              GwyDataField *score)
{
    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(kernel_field));
    g_return_if_fail(kernel_field->xres <= data_field->xres
                     && kernel_field->yres <= data_field->yres);
    g_return_if_fail(GWY_IS_DATA_FIELD(score));

    state->state = GWY_COMPUTATION_STATE_INIT;
    state->fraction = 0.0;
    state->data_field = g_object_ref(data_field);
    state->kernel_field = g_object_ref(kernel_field);
    state->score = g_object_ref(score);
    state->avg_rms = NULL;
}

/**
 * gwy_data_field_correlate_iteration:
 * @state: Correlation state.
 *
 * Performs one iteration of correlation.
 *
 * Fields @state and progress @fraction of correlation state are updated.
 * Once @state becomes %GWY_COMPUTATION_STATE_FINISHED, the calculation is
 * finised.
 *
 * Before the processing starts, @state must be initialized with
 * gwy_data_field_correlate_init().  When iteration ends, either
 * by finishing or being aborted, gwy_data_field_correlate_finalize()
 * must be called to release allocated resources.
 **/
void
gwy_data_field_correlate_iteration(GwyCorrelationState *state)
{
    gint xres, yres, kxres, kyres, w, h, p;
    gdouble s, davg, drms;

    xres = state->data_field->xres;
    yres = state->data_field->yres;
    kxres = state->kernel_field->xres;
    kyres = state->kernel_field->yres;

    if (state->state == GWY_COMPUTATION_STATE_INIT) {
        gwy_data_field_fill(state->score, -1);
        state->kavg = gwy_data_field_get_avg(state->kernel_field);
        state->krms = gwy_data_field_get_rms(state->kernel_field);
        state->avg_rms = calculate_normalization(state->data_field,
                                                 kxres, kyres);
        state->state = GWY_COMPUTATION_STATE_ITERATE;
        state->fraction = 0.0;
        state->i = kyres/2;
        state->j = kxres/2;
    }
    else if (state->state == GWY_COMPUTATION_STATE_ITERATE) {
        w = xres - kxres + 1;
        h = yres - kyres + 1;
        p = (state->i - kyres/2)*w + (state->j - kxres/2);

        davg = state->avg_rms[2*p + 0];
        drms = state->avg_rms[2*p + 1];
        s = gwy_data_field_get_raw_correlation_score(state->data_field,
                                                     state->kernel_field,
                                                     state->j - kxres/2,
                                                     state->i - kyres/2,
                                                     0, 0,
                                                     kxres, kyres,
                                                     davg, state->kavg);
        state->score->data[state->i*xres + state->j] = s/(drms*state->krms);

        state->j++;
        if (state->j == xres - (kxres - kxres/2)) {
            state->j = kxres/2;
            state->i++;
            if (state->i == yres - (kyres - kyres/2))
                state->state = GWY_COMPUTATION_STATE_FINISHED;
        }
        state->fraction += 1.0/(w*h);
    }
    gwy_data_field_invalidate(state->score);
}

/**
 * gwy_data_field_correlate_finalize:
 * @state: Correlation state.
 *
 * Frees all resources allocated by a correlation iterator.
 **/
void
gwy_data_field_correlate_finalize(GwyCorrelationState *state)
{
    state->state = GWY_COMPUTATION_STATE_FINISHED;
    state->fraction = 1.0;
    gwy_object_unref(state->data_field);
    gwy_object_unref(state->kernel_field);
    gwy_object_unref(state->score);
    g_free(state->avg_rms);
    state->avg_rms = NULL;
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

