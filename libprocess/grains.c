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
#include <glib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "filters.h"
#include "arithmetic.h"
#include "datafield.h"

/*local functions*/
static gint step_by_one                   (GwyDataField *data_field,
                                           gint *rcol,
                                           gint *rrow);
static void drop_step                     (GwyDataField *data_field,
                                           GwyDataField *water_field,
                                           gdouble dropsize);
static void drop_minima                   (GwyDataField *water_field,
                                           GwyDataField *min_field,
                                           gint threshval);
static void process_mask                  (GwyDataField *grain_field,
                                           gint col,
                                           gint row);
static void wdrop_step                    (GwyDataField *data_field,
                                           GwyDataField *min_field,
                                           GwyDataField *water_field,
                                           GwyDataField *grain_field,
                                           gdouble dropsize);
static void mark_grain_boundaries         (GwyDataField *grain_field);
static gint number_grains                 (GwyDataField *mask_field,
                                           gint *grains);
static gint* gwy_data_field_fill_grain    (GwyDataField *dfield,
                                           gint col,
                                           gint row,
                                           gint *nindices);
static gint gwy_data_field_fill_one_grain (GwyDataField *dfield,
                                           gint col,
                                           gint row,
                                           gint *visited,
                                           gint grain_no,
                                           gint *listv,
                                           gint *listh);

/**
 * gwy_data_field_grains_mark_height:
 * @data_field: Data to be used for marking.
 * @grain_field: Data field to store the resulting mask to.
 * @threshval: Relative height threshold, in percents.
 * @below: If %TRUE, data below threshold are marked, otherwise data above
 *         threshold are marked.
 *
 * Marks data that are above/below height threshold.
 **/
void
gwy_data_field_grains_mark_height(GwyDataField *data_field,
                                  GwyDataField *grain_field, gdouble threshval,
                                  gboolean below)
{
    gdouble min, max;

    gwy_data_field_area_copy(data_field, grain_field,
                             0, 0, data_field->xres, data_field->yres, 0, 0);
    min = gwy_data_field_get_min(grain_field);
    max = gwy_data_field_get_max(grain_field);
    if (below)
        gwy_data_field_threshold(grain_field,
                                 min + threshval*(max - min)/100.0, 1, 0);
    else
        gwy_data_field_threshold(grain_field,
                                 min + threshval*(max - min)/100.0, 0, 1);

    gwy_data_field_invalidate(grain_field);
}

/**
 * gwy_data_field_grains_mark_slope:
 * @data_field: Data to be used for marking.
 * @grain_field: Data field to store the resulting mask to.
 * @threshval: Relative slope threshold, in percents.
 * @below: If %TRUE, data below threshold are marked, otherwise data above
 *         threshold are marked.
 *
 * Marks data that are above/below slope threshold.
 **/
void
gwy_data_field_grains_mark_slope(GwyDataField *data_field,
                                 GwyDataField *grain_field, gdouble threshval,
                                 gboolean below)
{
    gdouble min, max;

    gwy_data_field_area_copy(data_field, grain_field,
                             0, 0, data_field->xres, data_field->yres, 0, 0);
    gwy_data_field_area_filter_laplacian(grain_field, 0, 0,
                                         data_field->xres, data_field->yres);

    min = gwy_data_field_get_min(grain_field);
    max = gwy_data_field_get_max(grain_field);
    if (below)
        gwy_data_field_threshold(grain_field,
                                 min + threshval*(max - min)/100.0, 1, 0);
    else
        gwy_data_field_threshold(grain_field,
                                 min + threshval*(max - min)/100.0, 0, 1);

    gwy_data_field_invalidate(grain_field);
}

/**
 * gwy_data_field_grains_mark_curvature:
 * @data_field: Data to be used for marking.
 * @grain_field: Data field to store the resulting mask to.
 * @threshval: Relative curvature threshold, in percents.
 * @below: If %TRUE, data below threshold are marked, otherwise data above
 *         threshold are marked.
 *
 * Marks data that are above/below curvature threshold.
 **/
void
gwy_data_field_grains_mark_curvature(GwyDataField *data_field,
                                     GwyDataField *grain_field,
                                     gdouble threshval,
                                     gboolean below)
{
    GwyDataField *masky;
    gdouble *gdata;
    gint i;
    gdouble xres, yres, min, max;

    xres = data_field->xres;
    yres = data_field->yres;

    masky = GWY_DATA_FIELD(gwy_data_field_new(xres, yres,
                                              data_field->xreal,
                                              data_field->yreal,
                                              FALSE));

    gwy_data_field_area_copy(data_field, grain_field, 0, 0, xres, yres, 0, 0);
    gwy_data_field_area_copy(data_field, masky, 0, 0, xres, yres, 0, 0);
    gwy_data_field_area_filter_sobel(grain_field, GTK_ORIENTATION_HORIZONTAL,
                                     0, 0, xres, yres);
    gwy_data_field_area_filter_sobel(masky, GTK_ORIENTATION_HORIZONTAL,
                                     0, 0, xres, yres);

    gdata = grain_field->data;
    for (i = 0; i < xres*yres; i++)
        gdata[i] = sqrt(gdata[i]*gdata[i] + masky->data[i]*masky->data[i]);

    min = gwy_data_field_get_min(grain_field);
    max = gwy_data_field_get_max(grain_field);
    if (below)
        gwy_data_field_threshold(grain_field,
                                 min + threshval*(max - min)/100.0, 1, 0);
    else
        gwy_data_field_threshold(grain_field,
                                 min + threshval*(max - min)/100.0, 0, 1);

    g_object_unref(masky);
    gwy_data_field_invalidate(grain_field);
}

/**
 * gwy_data_field_grains_mark_watershed:
 * @data_field: Data to be used for marking.
 * @grain_field: Result of marking (mask).
 * @locate_steps: Locating algorithm steps.
 * @locate_thresh: Locating algorithm threshold.
 * @locate_dropsize: Locating drop size.
 * @wshed_steps: Watershed steps.
 * @wshed_dropsize: Watershed drop size.
 * @prefilter: Use prefiltering.
 * @below: If %TRUE, valleys are marked, otherwise mountains are marked.
 *
 * Performs watershed algorithm.
 **/
void
gwy_data_field_grains_mark_watershed(GwyDataField *data_field,
                                     GwyDataField *grain_field,
                                     gint locate_steps, gint locate_thresh,
                                     gdouble locate_dropsize, gint wshed_steps,
                                     gdouble wshed_dropsize, gboolean prefilter,
                                     gboolean below)
{
    GwyDataField *min, *water, *mark_dfield;
    gint xres, yres, i;

    xres = data_field->xres;
    yres = data_field->yres;

    min = GWY_DATA_FIELD(gwy_data_field_new(xres, yres,
                                            data_field->xreal,
                                            data_field->yreal,
                                            TRUE));
    water = GWY_DATA_FIELD(gwy_data_field_new(xres, yres,
                                              data_field->xreal,
                                              data_field->yreal,
                                              TRUE));
    mark_dfield = gwy_data_field_duplicate(data_field);
    if (below)
        gwy_data_field_multiply(mark_dfield, -1.0);
    if (prefilter)
        gwy_data_field_filter_median(mark_dfield, 6);

    gwy_data_field_resample(grain_field, xres, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_clear(grain_field);

    /* odrop */
    for (i = 0; i < locate_steps; i++)
        drop_step(mark_dfield, water, locate_dropsize);
    drop_minima(water, min, locate_thresh);

    /* owatershed */
    gwy_data_field_area_copy(data_field, mark_dfield, 0, 0, xres, yres, 0, 0);
    if (below)
        gwy_data_field_multiply(mark_dfield, -1.0);
    for (i = 0; i < wshed_steps; i++)
        wdrop_step(mark_dfield, min, water, grain_field, wshed_dropsize);

    mark_grain_boundaries(grain_field);

    g_object_unref(min);
    g_object_unref(water);
    g_object_unref(mark_dfield);
    gwy_data_field_invalidate(grain_field);
}

/**
 * gwy_data_field_grains_watershed_iteration:
 * @data_field: Data to be used for marking.
 * @grain_field: Result of marking (mask).
 * @status : current status of the algorithm.
 * @locate_steps: Locating algorithm steps.
 * @locate_thresh: Locating algorithm threshold.
 * @locate_dropsize: Locating drop size.
 * @wshed_steps: Watershed steps.
 * @wshed_dropsize: Watershed drop size.
 * @prefilter: Use prefiltering.
 * @below: If %TRUE, valleys are marked, otherwise mountains are marked.
 *
 * Performs one iteration of the watershed algorithm.
 **/
void
gwy_data_field_grains_watershed_iteration(GwyDataField *data_field,
                                          GwyDataField *grain_field,
                                          GwyWatershedStatus *status,
                                          gint locate_steps, gint locate_thresh,
                                          gdouble locate_dropsize,
                                          gint wshed_steps,
                                          gdouble wshed_dropsize,
                                          gboolean prefilter,
                                          gboolean below)
{
    if (status->state == GWY_WSHED_INIT) {
        status->min = GWY_DATA_FIELD(gwy_data_field_new(data_field->xres,
                                                        data_field->yres,
                                                        data_field->xreal,
                                                        data_field->yreal,
                                                        TRUE));
        status->water = GWY_DATA_FIELD(gwy_data_field_new(data_field->xres,
                                                          data_field->yres,
                                                          data_field->xreal,
                                                          data_field->yreal,
                                                          TRUE));
        status->mark_dfield = gwy_data_field_duplicate(data_field);
        if (below)
            gwy_data_field_multiply(status->mark_dfield, -1.0);
        if (prefilter)
            gwy_data_field_filter_median(status->mark_dfield, 6);

        gwy_data_field_resample(grain_field, data_field->xres, data_field->yres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(grain_field);


        status->state = GWY_WSHED_LOCATE;
        status->internal_i = 0;
    }

    /* odrop */
    if (status->state == GWY_WSHED_LOCATE) {
        if (status->internal_i < locate_steps) {
            drop_step(status->mark_dfield, status->water, locate_dropsize);
            status->internal_i += 1;
        }
        else {
            status->state = GWY_WSHED_MIN;
            status->internal_i = 0;
        }
    }

    if (status->state == GWY_WSHED_MIN) {
        drop_minima(status->water, status->min, locate_thresh);
        status->state = GWY_WSHED_WSHED;
        status->internal_i = 0;
    }


    if (status->state == GWY_WSHED_WSHED) {
        if (status->internal_i == 0) {
            gwy_data_field_area_copy(data_field, status->mark_dfield, 0, 0,
                                     data_field->xres, data_field->yres, 0, 0);
            if (below)
                gwy_data_field_multiply(status->mark_dfield, -1.0);
        }
        if (status->internal_i < wshed_steps) {
            wdrop_step(status->mark_dfield, status->min, status->water,
                       grain_field, wshed_dropsize);
            status->internal_i += 1;
        }
        else {
            status->state = GWY_WSHED_MARK;
            status->internal_i = 0;
        }
    }

    if (status->state == GWY_WSHED_MARK) {
        mark_grain_boundaries(grain_field);

        g_object_unref(status->min);
        g_object_unref(status->water);
        g_object_unref(status->mark_dfield);

        status->state = GWY_WSHED_FINISHED;
    }
    gwy_data_field_invalidate(grain_field);
}

/**
 * gwy_data_field_grains_remove_grain:
 * @grain_field: Field of marked grains (mask).
 * @col: Column inside a grain.
 * @row: Row inside a grain.
 *
 * Removes one grain at given position.
 *
 * Returns: %TRUE if a grain was actually removed (i.e., (@col,@row) was
 *          inside a grain).
 *
 * Since: 1.7
 **/
gboolean
gwy_data_field_grains_remove_grain(GwyDataField *grain_field,
                                   gint col,
                                   gint row)
{
    gint *points;
    gint npoints = 0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(grain_field), FALSE);
    g_return_val_if_fail(col >= 0 && col < grain_field->xres, FALSE);
    g_return_val_if_fail(row >= 0 && row < grain_field->yres, FALSE);

    if (!grain_field->data[grain_field->xres*row + col])
        return FALSE;

    points = gwy_data_field_fill_grain(grain_field, col, row, &npoints);
    while (npoints) {
        npoints--;
        grain_field->data[points[npoints]] = 0.0;
    }
    g_free(points);
    gwy_data_field_invalidate(grain_field);

    return TRUE;
}

/**
 * gwy_data_field_grains_extract_grain:
 * @grain_field: Field of marked grains (mask).
 * @col: Column inside a grain.
 * @row: Row inside a grain.
 *
 * Removes all grains except that one at given position.
 *
 * If there is no grain at (@col, @row), all grains are removed.
 *
 * Returns: %TRUE if a grain remained (i.e., (@col,@row) was inside a grain).
 *
 * Since: 1.8
 **/
gboolean
gwy_data_field_grains_extract_grain(GwyDataField *grain_field,
                                    gint col,
                                    gint row)
{
    gint *points;
    gint npoints = 0;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(grain_field), FALSE);
    g_return_val_if_fail(col >= 0 && col < grain_field->xres, FALSE);
    g_return_val_if_fail(row >= 0 && row < grain_field->yres, FALSE);

    if (!grain_field->data[grain_field->xres*row + col]) {
        gwy_data_field_clear(grain_field);
        return FALSE;
    }

    points = gwy_data_field_fill_grain(grain_field, col, row, &npoints);
    gwy_data_field_clear(grain_field);
    while (npoints) {
        npoints--;
        grain_field->data[points[npoints]] = 1.0;
    }
    g_free(points);
    gwy_data_field_invalidate(grain_field);

    return TRUE;
}

/**
 * gwy_data_field_grains_remove_by_size:
 * @grain_field: Field of marked grains (mask).
 * @size: Grain area threshold, in square pixels.
 *
 * Removes all grain below specified area.
 **/
void
gwy_data_field_grains_remove_by_size(GwyDataField *grain_field, gint size)
{
    gint i, xres, yres, ngrains;
    gdouble *data;
    gint *grain_size;
    gint *grains;

    xres = grain_field->xres;
    yres = grain_field->yres;
    data = grain_field->data;

    grains = g_new0(gint, xres*yres);
    ngrains = number_grains(grain_field, grains);

    /* sum grain sizes */
    grain_size = g_new0(gint, ngrains + 1);
    for (i = 0; i < xres*yres; i++)
        grain_size[grains[i]]++;
    grain_size[0] = size;

    /* remove grains */
    for (i = 0; i < xres*yres; i++) {
        if (grain_size[grains[i]] < size)
            data[i] = 0;
    }
    for (i = 1; i <= ngrains; i++) {
        if (grain_size[i] < size) {
            gwy_data_field_invalidate(grain_field);
            break;
        }
    }

    g_free(grains);
    g_free(grain_size);
}

/**
 * gwy_data_field_grains_remove_by_height:
 * @data_field: Data to be used for marking
 * @grain_field: Field of marked grains (mask)
 * @threshval: Relative height threshold, in percents.
 * @below: If %TRUE, grains below threshold are removed, otherwise grains above
 *         threshold are removed.
 *
 * Removes grains that are higher/lower than given threshold value.
 **/
void
gwy_data_field_grains_remove_by_height(GwyDataField *data_field,
                                       GwyDataField *grain_field,
                                       gdouble threshval,
                                       gboolean below)
{
    gint i, xres, yres, ngrains;
    gdouble *data;
    gboolean *grain_kill;
    gint *grains;

    xres = grain_field->xres;
    yres = grain_field->yres;
    data = grain_field->data;

    threshval = gwy_data_field_get_min(data_field)
                + threshval*(gwy_data_field_get_max(data_field)
                             - gwy_data_field_get_min(data_field))/100.0;

    grains = g_new0(gint, xres*yres);
    ngrains = number_grains(grain_field, grains);

    /* find grains to remove */
    grain_kill = g_new0(gboolean, ngrains + 1);
    if (below) {
        for (i = 0; i < xres*yres; i++) {
            if (grains[i] && data_field->data[i] < threshval)
                grain_kill[grains[i]] = TRUE;
        }
    }
    else {
        for (i = 0; i < xres*yres; i++) {
            if (grains[i] && data_field->data[i] > threshval)
                grain_kill[grains[i]] = TRUE;
        }
    }

    /* remove them */
    for (i = 0; i < xres*yres; i++) {
        if (grain_kill[grains[i]])
            data[i] = 0;
    }
    for (i = 1; i <= ngrains; i++) {
        if (grain_kill[i]) {
            gwy_data_field_invalidate(grain_field);
            break;
        }
    }

    g_free(grains);
    g_free(grain_kill);
}

/**
 * gwy_data_field_grains_get_distribution:
 * @grain_field: Data field of marked grains (mask).
 * @distribution: Grain size distribution.
 *
 * Computes grain size distribution.
 *
 * Puts number of grains vs. grain size (in real units) data into
 * @distribution.  Grain size means grain side if it was square.
 **/
void
gwy_data_field_grains_get_distribution(GwyDataField *grain_field,
                                       GwyDataLine *distribution)
{
    gint i, xres, yres, ngrains, nhist;
    gint maxpnt;
    gint *grain_size;
    gint *grains;
    gdouble s, sigma;

    xres = grain_field->xres;
    yres = grain_field->yres;

    grains = g_new0(gint, xres*yres);
    ngrains = number_grains(grain_field, grains);
    if (!ngrains) {
        gwy_data_line_resample(distribution, 2, GWY_INTERPOLATION_NONE);
        gwy_data_line_fill(distribution, 0);
        gwy_data_line_set_real(distribution,
                               gwy_data_field_get_xres(grain_field));
        return;
    }

    /* sum grain sizes */
    grain_size = g_new0(gint, ngrains + 1);
    for (i = 0; i < xres*yres; i++)
        grain_size[grains[i]]++;
    g_free(grains);

    maxpnt = 0;
    s = sigma = 0.0;
    for (i = 1; i <= ngrains; i++) {
        if (maxpnt < grain_size[i])
            maxpnt = grain_size[i];
        s += sqrt(grain_size[i]);
        sigma += grain_size[i];
    }
    sigma = sqrt(ngrains*sigma - s*s)/ngrains;
    s = 3.49/pow((gdouble)ngrains, 1.0/3.0)*sigma;
    nhist = sqrt(maxpnt)/s + 1;

    gwy_data_line_resample(distribution, nhist, GWY_INTERPOLATION_NONE);
    gwy_data_line_fill(distribution, 0);
    for (i = 1; i <= ngrains; i++)
        distribution->data[(gint)(sqrt(grain_size[i])/s)] += 1;
    g_free(grain_size);

    gwy_data_line_set_real(distribution,
                           gwy_data_field_itor(grain_field, nhist));
}

/**
 * gwy_data_field_grains_add:
 * @grain_field: Field of marked grains (mask).
 * @add_field: Field of marked grains (mask) to be added.
 *
 * Adds @add_field grains to @grain_field.
 **/
void
gwy_data_field_grains_add(GwyDataField *grain_field, GwyDataField *add_field)
{
    gwy_data_field_max_of_fields(grain_field, grain_field, add_field);
}

/**
 * gwy_data_field_grains_intersect:
 * @grain_field:  field of marked grains (mask).
 * @intersect_field: Field of marked grains (mask).
 *
 * Performs intersection betweet two grain fields,
 * result is stored in @grain_field.
 **/
void
gwy_data_field_grains_intersect(GwyDataField *grain_field,
                                GwyDataField *intersect_field)
{
    gwy_data_field_min_of_fields(grain_field, grain_field, intersect_field);
}

/****************************************************************************/
/*private functions*/

/* FIXME: change to gboolean */
static gint
step_by_one(GwyDataField *data_field, gint *rcol, gint *rrow)
{
    gint xres, yres;
    gdouble a, b, c, d, v;

    xres = data_field->xres;
    yres = data_field->yres;

    if (*rcol < (xres - 1))
        a = data_field->data[*rcol + 1 + xres*(*rrow)];
    else
        a = -G_MAXDOUBLE;

    if (*rcol > 0)
        b = data_field->data[*rcol - 1 + xres*(*rrow)];
    else
        b = -G_MAXDOUBLE;

    if (*rrow < (yres - 1))
        c = data_field->data[*rcol + xres*(*rrow + 1)];
    else
        c = -G_MAXDOUBLE;

    if (*rrow > 0)
        d = data_field->data[*rcol + xres*(*rrow - 1)];
    else
        d = -G_MAXDOUBLE;

    v = data_field->data[(gint)(*rcol + xres*(*rrow))];

    if (v >= a && v >= b && v >= c && v >= d) {
        return 1;
    }
    else if (a >= v && a >= b && a >= c && a >= d) {
        *rcol += 1;
        return 0;
    }
    else if (b >= v && b >= a && b >= c && b >= d) {
        *rcol -= 1;
        return 0;
    }
    else if (c >= v && c >= b && c >= a && c >= d) {
        *rrow += 1;
        return 0;
    }
    else {
        *rrow -= 1;
        return 0;
    }

    return 0;
}


static void
drop_step(GwyDataField *data_field, GwyDataField *water_field, gdouble dropsize)
{
    gint xres, yres, i, retval;
    gint col, row;

    xres = data_field->xres;
    yres = data_field->yres;

    for (i = 0; i < xres*yres; i++) {
        retval = 0;
        row = (gint)floor((gdouble)i/(gdouble)xres);
        col = i - xres*row;
        if (col == 0 || row == 0 || col == (xres - 1) || row == (yres - 1))
            continue;

        do {
            retval = step_by_one(data_field, &col, &row);
        } while (retval == 0);

        water_field->data[col + xres*row] += 1;
        data_field->data[col + xres*row] -= dropsize;

    }
}

static void
drop_minima(GwyDataField *water_field, GwyDataField *min_field, gint threshval)
{
    gint xres, yres, i, j, ngrains;
    gint *grain_maxima, *grain_size;
    gdouble *data;
    gint *grains;

    xres = water_field->xres;
    yres = water_field->yres;
    data = water_field->data;

    grains = g_new0(gint, xres*yres);
    ngrains = number_grains(water_field, grains);
    grain_size = g_new0(gint, ngrains + 1);
    grain_maxima = g_new(gint, ngrains + 1);
    for (i = 1; i <= ngrains; i++)
        grain_maxima[i] = -1;

    /* sum grain sizes and find maxima */
    for (i = 0; i < xres*yres; i++) {
        j = grains[i];
        if (!j)
            continue;

        grain_size[j]++;
        if (grain_maxima[j] < 0
            || data[grain_maxima[j]] < data[i])
            grain_maxima[j] = i;
    }
    g_free(grains);

    /* mark maxima */
    for (i = 1; i <= ngrains; i++) {
        if (grain_size[i] <= threshval)
            continue;

        min_field->data[grain_maxima[i]] = i;
    }

    g_free(grain_maxima);
    g_free(grain_size);
}

static void
process_mask(GwyDataField *grain_field, gint col, gint row)
{
    gint xres, yres, ival[4], val, stat, /* FIXME: change to boolean */ i;
    gdouble *data;

    xres = grain_field->xres;
    yres = grain_field->yres;
    data = grain_field->data;

    if (col == 0 || row == 0 || col == (xres - 1) || row == (yres - 1)) {
        data[col + xres*row] = -1;
        return;
    }

    /*if this is grain or boundary, keep it */
    if (data[col + xres*row] != 0)
        return;

    /*if there is nothing around, do nothing */
    if ((fabs(data[col + 1 + xres*row]) + fabs(data[col - 1 + xres*row])
         + fabs(data[col + xres*(row + 1)]) + fabs(data[col + xres*(row - 1)]))
        == 0)
        return;

    /*now count the grain values around */
    ival[0] = data[col - 1 + xres*row];
    ival[1] = data[col + xres*(row - 1)];
    ival[2] = data[col + 1 + xres*row];
    ival[3] = data[col + xres*(row + 1)];

    val = 0;
    stat = 0;
    for (i = 0; i < 4; i++) {
        if (val > 0 && ival[i] > 0 && ival[i] != val) {
            /*if some value already was there and the now one is different */
            stat = 1;  /* FIXME: change to boolean */
            break;
        }
        else {
            /*ifthere is some value */
            if (ival[i] > 0) {
                val = ival[i];
            }
        }
    }

    /*it will be boundary or grain */
    /* FIXME: don't compare booleans */
    if (stat == 1)
        data[col + xres*row] = -1;
    else
        data[col + xres*row] = val;

}

static void
wdrop_step(GwyDataField *data_field, GwyDataField *min_field,
           GwyDataField *water_field, GwyDataField *grain_field,
           gdouble dropsize)
{
    gint xres, yres, vcol, vrow, col, row, grain, retval;

    xres = data_field->xres;
    yres = data_field->yres;

    grain = 0;
    for (col = 0; col < xres; col++) {
        for (row = 0; row < yres; row++) {
            if (min_field->data[col + xres*row] > 0)
                grain_field->data[col + xres*row] = grain++;
        }
    }
    for (col = 1; col < xres - 1; col++) {
        for (row = 1; row < yres - 1; row++) {

            vcol = col;
            vrow = row;
            retval = 0;
            do {
                retval = step_by_one(data_field, &vcol, &vrow);
            } while (retval == 0);

            /*now, distinguish what to change at point vi, vj */
            process_mask(grain_field, vcol, vrow);
            water_field->data[vcol + xres*(vrow)] += 1;
            data_field->data[vcol + xres*(vrow)] -= dropsize;

        }
    }
}

static void
mark_grain_boundaries(GwyDataField *grain_field)
{
    gint xres, yres, col, row;
    GwyDataField *buffer;
    gdouble *data;

    xres = grain_field->xres;
    yres = grain_field->yres;
    buffer = GWY_DATA_FIELD(gwy_data_field_new(xres, yres,
                                               grain_field->xreal,
                                               grain_field->yreal,
                                               FALSE));
    gwy_data_field_copy(grain_field, buffer);
    data = buffer->data;

    for (col = 1; col < xres - 1; col++) {
        for (row = 1; row < yres - 1; row++) {
            if (data[col + xres*row] != data[col + 1 + xres*row]
                || data[col + xres*row] != data[col + xres*(row + 1)])
                grain_field->data[col + xres*row] = 0;
        }
    }
    g_object_unref(buffer);
}

/**
 * number_grains:
 * @mask_field: Data field containing positive values in grains, nonpositive
 *              in free space.
 * @grains: Zero-filled array of integers of equal size to @mask_field to put
 *          grain numbers to.  Empty space will be left 0, pixels inside a
 *          grain will be set to grain number.  Grains are numbered
 *          sequentially 1, 2, 3, ...
 *
 * Numbers grains in a mask data field:.
 *
 * Returns: The number of last grain (note they are numbered from 1).
 **/
static gint
number_grains(GwyDataField *mask_field,
              gint *grains)
{
    gint *listv, *listh;
    gint xres, yres, n, i, grain_no;

    xres = mask_field->xres;
    yres = mask_field->yres;

    n = xres*yres;
    listv = g_new(gint, n/2 + 2);
    listh = g_new(gint, n/2 + 2);

    grain_no = 0;

    for (i = 0; i < n; i++) {
        if (mask_field->data[i] && !grains[i]) {
            grain_no++;
            gwy_data_field_fill_one_grain(mask_field, i % xres, i/xres,
                                          grains, grain_no, listv, listh);
        }
    }

    g_free(listh);
    g_free(listv);

    return grain_no;
}

/**
 * gwy_data_field_fill_grain:
 * @dfield: A data field with zeroes in empty space and nonzeroes in grains.
 * @col: Column inside a grain.
 * @row: Row inside a grain.
 * @nindices: Where the number of points in the grain at (@col, @row) should
 *            be stored.
 *
 * Finds all the points belonging to the grain at (@col, @row).
 *
 * Returns: A newly allocated array of indices of grain points in @dfield's
 *          data, the size of the list is returned in @nindices.
 **/
static gint*
gwy_data_field_fill_grain(GwyDataField *dfield,
                          gint col, gint row, gint *nindices)
{
    gdouble *data;
    gint *visited, *listv, *listh;
    gint *indices;
    gint xres, yres, n, count;
    gint i, j;
    gint initial;

    data = dfield->data;
    xres = dfield->xres;
    yres = dfield->yres;
    initial = row*xres + col;
    g_return_val_if_fail(data[initial], NULL);

    /* check for a single point */
    if ((!col || data[initial - 1] <= 0)
        && (!row || data[initial - xres] <= 0)
        && (col + 1 == xres || data[initial + 1] <= 0)
        && (row + 1 == yres || data[initial + xres] <= 0)) {
        indices = g_new(gint, 1);

        indices[0] = initial;
        *nindices = 1;

        return indices;
    }

    n = xres*yres;
    visited = g_new0(gint, n);
    listv = g_new(gint, n/2 + 2);
    listh = g_new(gint, n/2 + 2);
    count = gwy_data_field_fill_one_grain(dfield, col, row,
                                          visited, 1, listv, listh);

    g_free(listh);
    g_free(listv);

    indices = g_new(gint, count);

    j = 0;
    for (i = 0; i < n; i++) {
        if (visited[i])
            indices[j++] = i;
    }
    g_free(visited);

    *nindices = count;
    return indices;
}

/**
 * gwy_data_field_fill_one_grain:
 * @dfield: A data field with zeroes in empty space and nonzeroes in grains.
 * @col: Column inside a grain.
 * @row: Row inside a grain.
 * @visited: An array @col x @row that contain zeroes in empty space and yet
 *           unvisited grains.  Current grain will be filled with @grain_no.
 * @grain_no: Value to fill current grain with.
 * @listv: A working buffer of size at least @col x @row/2 + 2, its content is
 *         owerwritten.
 * @listh: A working buffer of size at least @col x @row/2 + 2, its content is
 *         owerwritten.
 *
 * Internal function to fill/number a one grain.
 *
 * The @visited, @listv, and @listh buffers are recyclable between calls so
 * they don't have to be allocated and freed for each grain, speeding up
 * sequential grain processing.  Generally, this function itself does not
 * allocate or free any memory.
 *
 * Returns: The number of pixels in the grain.
 **/
static gint
gwy_data_field_fill_one_grain(GwyDataField *dfield,
                              gint col, gint row,
                              gint *visited,
                              gint grain_no,
                              gint *listv,
                              gint *listh)
{
    gdouble *data;
    gint xres, yres, n, count;
    gint nh, nv;
    gint i, p, j;
    gint initial;

    g_return_val_if_fail(grain_no, 0);
    data = dfield->data;
    xres = dfield->xres;
    yres = dfield->yres;
    initial = row*xres + col;
    g_return_val_if_fail(data[initial], 0);

    /* check for a single point */
    visited[initial] = grain_no;
    count = 1;
    if ((!col || data[initial - 1] <= 0)
        && (!row || data[initial - xres] <= 0)
        && (col + 1 == xres || data[initial + 1] <= 0)
        && (row + 1 == yres || data[initial + xres] <= 0)) {

        return count;
    }

    n = xres*yres;
    listv[0] = listv[1] = initial;
    nv = 2;
    listh[0] = listh[1] = initial;
    nh = 2;

    while (nv) {
        /* go through vertical lines and expand them horizontally */
        for (i = 0; i < nv; i += 2) {
            for (p = listv[i]; p <= listv[i + 1]; p += xres) {
                gint start, stop;

                /* scan left */
                start = p - 1;
                stop = (p/xres)*xres;
                for (j = start; j >= stop; j--) {
                    if (visited[j] || data[j] <= 0)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j < start) {
                    listh[nh++] = j + 1;
                    listh[nh++] = start;
                }

                /* scan right */
                start = p + 1;
                stop = (p/xres + 1)*xres;
                for (j = start; j < stop; j++) {
                    if (visited[j] || data[j] <= 0)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j > start) {
                    listh[nh++] = start;
                    listh[nh++] = j - 1;
                }
            }
        }
        nv = 0;

        /* go through horizontal lines and expand them vertically */
        for (i = 0; i < nh; i += 2) {
            for (p = listh[i]; p <= listh[i + 1]; p++) {
                gint start, stop;

                /* scan up */
                start = p - xres;
                stop = p % xres;
                for (j = start; j >= stop; j -= xres) {
                    if (visited[j] || data[j] <= 0)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j < start) {
                    listv[nv++] = j + xres;
                    listv[nv++] = start;
                }

                /* scan down */
                start = p + xres;
                stop = p % xres + n;
                for (j = start; j < stop; j += xres) {
                    if (visited[j] || data[j] <= 0)
                        break;
                    visited[j] = grain_no;
                    count++;
                }
                if (j > start) {
                    listv[nv++] = start;
                    listv[nv++] = j - xres;
                }
            }
        }
        nh = 0;
    }

    return count;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
