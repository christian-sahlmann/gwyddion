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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"
#include "stats.h"

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
 *
 * Since: 1.2
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
    for (i = 0; i < kwidth; i++) {      /*col */
        for (j = 0; j < kheight; j++) { /*row */
            score += (data_field->data[(i + ulcol) + xres * (j + ulrow)] - avg1)
                *
                (kernel_field->
                 data[(i + kernel_ulcol) + kxres * (j + kernel_ulrow)] - avg2);
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
 *
 * Since: 1.2
 **/
void
gwy_data_field_correlate(GwyDataField *data_field, GwyDataField *kernel_field,
                         GwyDataField *score)
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
 *
 * Since: 1.2
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
    /*correlation request outside kernel */
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

#if 0
/*iterate over search area in the second datafield */
static void
gwy_data_field_crosscorrelate_iter(GwyDataField *data_field1,
                                   GwyDataField *data_field2,
                                   gint search_width,
                                   gint search_height,
                                   gint i, gint j,
                                   gint *imax, gint *jmax,
                                   gdouble *cormax)
{
    gint m, n;
    gdouble lscore;

    *imax = i;
    *jmax = j;
    *cormax = -1;
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
             * to prevent problems on flat data */
            if (m == (i - search_width/2)
                && n == (j - search_height/2))
                lscore *= 1.0001;

            if (*cormax < lscore) {
                *cormax = lscore;
                *imax = m + search_width/2;
                *jmax = n + search_height/2;
            }
        }
    }
}
#endif

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
 *
 * Since: 1.2
 **/
void
gwy_data_field_crosscorrelate(GwyDataField *data_field1,
                              GwyDataField *data_field2, GwyDataField *x_dist,
                              GwyDataField *y_dist, GwyDataField *score,
                              gint search_width, gint search_height,
                              G_GNUC_UNUSED gint window_width,
                              G_GNUC_UNUSED gint window_height)
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
            x_dist->data[i + xres * j] = (gdouble)(imax - i)*data_field1->xreal/(gdouble)data_field1->xres;
            y_dist->data[i + xres * j] = (gdouble)(jmax - j)*data_field1->yreal/(gdouble)data_field1->yres;
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
 * @iteration: Iteration of computation loop (within GWY_COMPUTATION_STATE_ITERATE state).
 *
 * Algorithm for matching two different images of the same object under changes.
 *
 * It does not use any special features
 * for matching. It simply searches for all points (with their neighbourhood)
 * of @data_field1 within @data_field2. Parameters @search_width and
 * @search_height
 * determine maimum area where to search for points. The area is cenetered
 * in the @data_field2 at former position of points at @data_field1.
 *
 * Since: 1.2.
 **/
void
gwy_data_field_crosscorrelate_iteration(GwyDataField *data_field1,
                                        GwyDataField *data_field2,
                                        GwyDataField *x_dist,
                                        GwyDataField *y_dist,
                                        GwyDataField *score,
                                        gint search_width, gint search_height,
                                        G_GNUC_UNUSED gint window_width,
                                        G_GNUC_UNUSED gint window_height,
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

