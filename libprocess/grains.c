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
#include <libprocess/linestats.h>
#include <libprocess/filters.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/grains.h>

typedef struct {
    gint i;
    gint j;
} GridPoint;

/* Watershed iterator */
typedef struct {
    GwyComputationState cs;
    GwyDataField *data_field;
    GwyDataField *grain_field;
    gint locate_steps;
    gint locate_thresh;
    gdouble locate_dropsize;
    gint wshed_steps;
    gdouble wshed_dropsize;
    gboolean prefilter;
    gboolean below;
    gint internal_i;
    GwyDataField *min;
    GwyDataField *water;
    GwyDataField *mark_dfield;
} GwyWatershedState;

static gboolean step_by_one                  (GwyDataField *data_field,
                                              gint *rcol,
                                              gint *rrow);
static void     drop_step                    (GwyDataField *data_field,
                                              GwyDataField *water_field,
                                              gdouble dropsize);
static void     drop_minima                  (GwyDataField *water_field,
                                              GwyDataField *min_field,
                                              gint threshval);
static void     process_mask                 (GwyDataField *grain_field,
                                              gint col,
                                              gint row);
static void     wdrop_step                   (GwyDataField *data_field,
                                              GwyDataField *min_field,
                                              GwyDataField *water_field,
                                              GwyDataField *grain_field,
                                              gdouble dropsize);
static void     mark_grain_boundaries        (GwyDataField *grain_field);
static gint*    gwy_data_field_fill_grain    (GwyDataField *data_field,
                                              gint col,
                                              gint row,
                                              gint *nindices);
static gint     gwy_data_field_fill_one_grain(gint xres,
                                              gint yres,
                                              const gint *data,
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
                                  GwyDataField *grain_field,
                                  gdouble threshval,
                                  gboolean below)
{
    gdouble min, max;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(grain_field));

    gwy_data_field_copy(data_field, grain_field, FALSE);
    gwy_data_field_get_min_max(grain_field, &min, &max);
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
                                 GwyDataField *grain_field,
                                 gdouble threshval,
                                 gboolean below)
{
    gdouble min, max;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(grain_field));

    gwy_data_field_copy(data_field, grain_field, FALSE);
    gwy_data_field_filter_laplacian(grain_field);

    gwy_data_field_get_min_max(grain_field, &min, &max);
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

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(grain_field));

    xres = data_field->xres;
    yres = data_field->yres;

    masky = gwy_data_field_duplicate(data_field);
    gwy_data_field_copy(data_field, grain_field, FALSE);
    gwy_data_field_filter_sobel(grain_field, GWY_ORIENTATION_HORIZONTAL);
    gwy_data_field_filter_sobel(masky, GWY_ORIENTATION_HORIZONTAL);

    gdata = grain_field->data;
    for (i = 0; i < xres*yres; i++)
        gdata[i] = hypot(gdata[i], masky->data[i]);

    gwy_data_field_get_min_max(grain_field, &min, &max);
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
                                     gint locate_steps,
                                     gint locate_thresh,
                                     gdouble locate_dropsize,
                                     gint wshed_steps,
                                     gdouble wshed_dropsize,
                                     gboolean prefilter,
                                     gboolean below)
{
    GwyDataField *min, *water, *mark_dfield;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(grain_field));

    xres = data_field->xres;
    yres = data_field->yres;

    min = gwy_data_field_new_alike(data_field, TRUE);
    water = gwy_data_field_new_alike(data_field, TRUE);
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
    gwy_data_field_copy(data_field, mark_dfield, FALSE);
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
 * gwy_data_field_grains_watershed_init:
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
 * Initializes the watershed algorithm.
 *
 * This iterator reports its state as #GwyWatershedStateType.
 *
 * Returns: A new watershed iterator.
 **/
GwyComputationState*
gwy_data_field_grains_watershed_init(GwyDataField *data_field,
                                     GwyDataField *grain_field,
                                     gint locate_steps,
                                     gint locate_thresh,
                                     gdouble locate_dropsize,
                                     gint wshed_steps,
                                     gdouble wshed_dropsize,
                                     gboolean prefilter,
                                     gboolean below)
{
    GwyWatershedState *state;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(grain_field), NULL);

    state = g_new0(GwyWatershedState, 1);

    state->cs.state = GWY_WATERSHED_STATE_INIT;
    state->cs.fraction = 0.0;
    state->data_field = g_object_ref(data_field);
    state->grain_field = g_object_ref(grain_field);
    state->locate_steps = locate_steps;
    state->locate_thresh = locate_thresh;
    state->locate_dropsize = locate_dropsize;
    state->wshed_steps = wshed_steps;
    state->wshed_dropsize = wshed_dropsize;
    state->prefilter = prefilter;
    state->below = below;
    state->internal_i = 0;

    return (GwyComputationState*)state;
}

/**
 * gwy_data_field_grains_watershed_iteration:
 * @state: Watershed iterator.
 *
 * Performs one iteration of the watershed algorithm.
 *
 * Fields @state and progress @fraction of watershed state are updated
 * (fraction is calculated for each phase individually).  Once @state
 * becomes %GWY_WATERSHED_STATE_FINISHED, the calculation is finised.
 *
 * A watershed iterator can be created with
 * gwy_data_field_grains_watershed_init().  When iteration ends, either
 * by finishing or being aborted, gwy_data_field_grains_watershed_finalize()
 * must be called to release allocated resources.
 **/
void
gwy_data_field_grains_watershed_iteration(GwyComputationState *cstate)
{
    GwyWatershedState *state = (GwyWatershedState*)cstate;

    if (state->cs.state == GWY_WATERSHED_STATE_INIT) {
        state->min = gwy_data_field_new_alike(state->data_field, TRUE);
        state->water = gwy_data_field_new_alike(state->data_field, TRUE);
        state->mark_dfield = gwy_data_field_duplicate(state->data_field);
        if (state->below)
            gwy_data_field_multiply(state->mark_dfield, -1.0);
        if (state->prefilter)
            gwy_data_field_filter_median(state->mark_dfield, 6);

        gwy_data_field_resample(state->grain_field,
                                state->data_field->xres,
                                state->data_field->yres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_clear(state->grain_field);

        state->cs.state = GWY_WATERSHED_STATE_LOCATE;
        state->internal_i = 0;
        state->cs.fraction = 0.0;
    }
    else if (state->cs.state == GWY_WATERSHED_STATE_LOCATE) {
        if (state->internal_i < state->locate_steps) {
            drop_step(state->mark_dfield, state->water, state->locate_dropsize);
            state->internal_i += 1;
            state->cs.fraction = (gdouble)state->internal_i/state->locate_steps;
        }
        else {
            state->cs.state = GWY_WATERSHED_STATE_MIN;
            state->internal_i = 0;
            state->cs.fraction = 0.0;
        }
    }
    else if (state->cs.state == GWY_WATERSHED_STATE_MIN) {
        drop_minima(state->water, state->min, state->locate_thresh);
        state->cs.state = GWY_WATERSHED_STATE_WATERSHED;
        state->internal_i = 0;
        state->cs.fraction = 0.0;
    }
    else if (state->cs.state == GWY_WATERSHED_STATE_WATERSHED) {
        if (state->internal_i == 0) {
            gwy_data_field_copy(state->data_field, state->mark_dfield, FALSE);
            if (state->below)
                gwy_data_field_multiply(state->mark_dfield, -1.0);
        }
        if (state->internal_i < state->wshed_steps) {
            wdrop_step(state->mark_dfield, state->min, state->water,
                       state->grain_field, state->wshed_dropsize);
            state->internal_i += 1;
            state->cs.fraction = (gdouble)state->internal_i/state->wshed_steps;
        }
        else {
            state->cs.state = GWY_WATERSHED_STATE_MARK;
            state->internal_i = 0;
            state->cs.fraction = 0.0;
        }
    }
    else if (state->cs.state == GWY_WATERSHED_STATE_MARK) {
        mark_grain_boundaries(state->grain_field);
        state->cs.state = GWY_WATERSHED_STATE_FINISHED;
        state->cs.fraction = 1.0;
    }
    else if (state->cs.state == GWY_WATERSHED_STATE_FINISHED)
        return;

    gwy_data_field_invalidate(state->grain_field);
}

/**
 * gwy_data_field_grains_watershed_finalize:
 * @state: Watershed iterator.
 *
 * Destroys a watershed iterator, freeing all resources.
 **/
void
gwy_data_field_grains_watershed_finalize(GwyComputationState *cstate)
{
    GwyWatershedState *state = (GwyWatershedState*)cstate;

    gwy_object_unref(state->min);
    gwy_object_unref(state->water);
    gwy_object_unref(state->mark_dfield);
    gwy_object_unref(state->data_field);
    gwy_object_unref(state->grain_field);
    g_free(state);
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
gwy_data_field_grains_remove_by_size(GwyDataField *grain_field,
                                     gint size)
{
    gint i, xres, yres, ngrains;
    gdouble *data;
    gint *grain_size;
    gint *grains;

    g_return_if_fail(GWY_IS_DATA_FIELD(grain_field));

    xres = grain_field->xres;
    yres = grain_field->yres;
    data = grain_field->data;

    grains = g_new0(gint, xres*yres);
    ngrains = gwy_data_field_number_grains(grain_field, grains);

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

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(grain_field));

    xres = grain_field->xres;
    yres = grain_field->yres;
    data = grain_field->data;

    threshval = gwy_data_field_get_min(data_field)
                + threshval*(gwy_data_field_get_max(data_field)
                             - gwy_data_field_get_min(data_field))/100.0;

    grains = g_new0(gint, xres*yres);
    ngrains = gwy_data_field_number_grains(grain_field, grains);

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
 * @data_field: Data field used for marking.  For some quantities its values
 *              are not used, but units and physical dimensions are always
 *              taken from it.
 * @grain_field: Data field (mask) of marked grains.  Note if you pass
 *               non-%NULL @grains all grain information is taken from it and
 *               @grain_field can be even %NULL then.
 * @distribution: Data line to store grain distribution to.
 * @grains: Grain numbers filled with gwy_data_field_number_grains() if you
 *          have it, or %NULL (the function then finds grain numbers itself
 *          which is not efficient for repeated use on the same grain field).
 * @ngrains: The number of grains as returned by
 *           gwy_data_field_number_grains().  Ignored in @grains is %NULL.
 * @quantity: The quantity to calculate.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Computes distribution of requested grain characteristics.
 *
 * Puts number of grains vs. grain value data into @distribution, units, scales
 * and offsets of @distribution are updated accordingly.
 *
 * Returns: A data line with the distribution: @distribution itself if it was
 *          not %NULL, otherwise a newly created #GwyDataLine caller must
 *          destroy.  If there are no grains, %NULL is returned and
 *          @distribution is not changed.
 **/
GwyDataLine*
gwy_data_field_grains_get_distribution(GwyDataField *data_field,
                                       GwyDataField *grain_field,
                                       GwyDataLine *distribution,
                                       gint ngrains,
                                       const gint *grains,
                                       GwyGrainQuantity quantity,
                                       gint nstats)
{
    GwyDataLine *values;
    GwySIUnit *xyunit, *zunit, *lineunit;
    gint i, j;
    gint *mygrains = NULL;
    gdouble min, max, s;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), FALSE);
    g_return_val_if_fail(grains || GWY_IS_DATA_FIELD(grain_field), FALSE);
    g_return_val_if_fail(!grain_field
                         || (grain_field->xres == data_field->xres
                             && grain_field->yres == data_field->yres), FALSE);
    g_return_val_if_fail(!distribution || GWY_IS_DATA_LINE(distribution),
                         FALSE);

    /* Calculate raw statistics */
    if (!grains) {
        grains = mygrains = g_new0(gint, grain_field->xres*grain_field->yres);
        ngrains = gwy_data_field_number_grains(grain_field, mygrains);
    }
    if (!ngrains) {
        g_free(mygrains);
        return NULL;
    }

    values = gwy_data_line_new(ngrains + 1, 1.0, FALSE);
    gwy_data_field_grains_get_values(data_field, values->data,
                                     ngrains, grains, quantity);
    g_free(mygrains);

    /* Find reasonable binning */
    min = gwy_data_line_part_get_min(values, 1, ngrains + 1);
    max = gwy_data_line_part_get_max(values, 1, ngrains + 1);
    if (min > 0.0 && min <= 0.1*max)
        min = 0.0;
    else if (max < 0.0 && max >= 0.1*min)
        max = 0.0;
    if (nstats < 1) {
        nstats = floor(3.49*cbrt(ngrains) + 0.5);
        nstats = MAX(nstats, 2);
    }
    gwy_debug("min: %g, max: %g, nstats: %d", min, max, nstats);
    s = (max - min)/(nstats - 1e-9);

    /* Fill histogram */
    if (distribution) {
        gwy_data_line_resample(distribution, nstats, GWY_INTERPOLATION_NONE);
        gwy_data_line_clear(distribution);
    }
    else
        distribution = gwy_data_line_new(nstats, 1.0, TRUE);

    if (max > min) {
        for (i = 1; i <= ngrains; i++) {
            j = (gint)((values->data[i] - min)/s);
            j = GWY_CLAMP(j, 0, nstats-1);
            distribution->data[j]++;
        }
    }
    else {
        if (max)
            s = fabs(max)/2.0;
        else
            s = 1.0;  /* whatever */

        max += s;
        min -= s;
        distribution->data[nstats/2] = ngrains;
    }
    g_object_unref(values);

    /* Set proper units and scales */
    gwy_data_line_set_real(distribution, max - min);
    gwy_data_line_set_offset(distribution, min);

    xyunit = gwy_data_field_get_si_unit_xy(data_field);
    zunit = gwy_data_field_get_si_unit_z(data_field);
    lineunit = gwy_data_line_get_si_unit_x(distribution);
    switch (quantity) {
        case GWY_GRAIN_VALUE_PROJECTED_AREA:
        case GWY_GRAIN_VALUE_SURFACE_AREA:
        gwy_si_unit_power(xyunit, 2, lineunit);
        break;

        case GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE:
        case GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS:
        case GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH:
        case GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE:
        case GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE:
        gwy_serializable_clone(G_OBJECT(xyunit), G_OBJECT(lineunit));
        break;

        case GWY_GRAIN_VALUE_MINIMUM:
        case GWY_GRAIN_VALUE_MAXIMUM:
        case GWY_GRAIN_VALUE_MEAN:
        case GWY_GRAIN_VALUE_MEDIAN:
        gwy_serializable_clone(G_OBJECT(zunit), G_OBJECT(lineunit));
        break;

        case GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE:
        case GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE:
        gwy_si_unit_set_from_string(lineunit, NULL);
        break;

        default:
        g_warning("Wrong grain quantity %d", (gint)quantity);
        break;
    }
    lineunit = gwy_data_line_get_si_unit_y(distribution);
    gwy_si_unit_set_from_string(lineunit, NULL);

    return distribution;
}

/* See stats.c for description, this function calculates twice `contribution
 * of one corner' (the twice is to move multiplications from inner loops) */
static inline gdouble
square_area2w_1c(gdouble z1, gdouble z2, gdouble z4, gdouble c,
                 gdouble x, gdouble y)
{
    return sqrt(1.0 + (z1 - z2)*(z1 - z2)/x + (z1 + z2 - c)*(z1 + z2 - c)/y)
            + sqrt(1.0 + (z1 - z4)*(z1 - z4)/y + (z1 + z4 - c)*(z1 + z4 - c)/x);
}

/* See stats.c for description, this function calculates 24x `contribution
 * of one corner' (the 24x is to move multiplications from inner loops) */
static inline gdouble
square_volumew_1c(gdouble z1, gdouble z2, gdouble z4, gdouble c)
{
    return 3.0*z1 + z2 + z4 + c;
}

/**
 * find_grain_convex_hull:
 * @xres: The number of columns in @grains.
 * @yres: The number of rows in @grains.
 * @grains: Grain numbers filled with gwy_data_field_number_grains().
 * @pos: Position of the top-left vertex of grain's convex hull.
 * @vertices: Array to fill with vertices.
 *
 * Finds vertices of a grain's convex hull.
 *
 * The grain is identified by @pos which must lie in a grain.
 *
 * The positions are returned as indices to vertex grid.  NB: The size of the
 * grid is (@xres + 1)*(@yres + 1), not @xres*@yres.
 *
 * The method is a bit naive, some atan2() calculations could be easily saved.
 **/
static void
find_grain_convex_hull(gint xres, gint yres,
                       const gint *grains,
                       gint pos,
                       GArray *vertices)
{
    enum { RIGHT = 0, DOWN, LEFT, UP } newdir, dir;
    const GridPoint *cur, *mid, *prev;
    GridPoint v;
    gdouble phi, phim;
    gint initpos, gno, n;

    g_return_if_fail(grains[pos]);

    g_array_set_size(vertices, 0);
    initpos = pos;
    gno = grains[pos];
    n = xres + 1;
    v.i = pos/xres;
    v.j = pos % xres;
    g_array_append_val(vertices, v);
    newdir = RIGHT;

    do {
        dir = newdir;
        switch (dir) {
            case RIGHT:
            v.j++;
            if (v.i > 0 && v.j < xres && grains[(v.i-1)*xres + v.j] == gno)
                newdir = UP;
            else if (v.j < xres && grains[v.i*xres + v.j] == gno)
                newdir = RIGHT;
            else
                newdir = DOWN;
            break;

            case DOWN:
            v.i++;
            if (v.j < xres && v.i < yres && grains[v.i*xres + v.j] == gno)
                newdir = RIGHT;
            else if (v.i < yres && grains[v.i*xres + v.j-1] == gno)
                newdir = DOWN;
            else
                newdir = LEFT;
            break;

            case LEFT:
            v.j--;
            if (v.i < yres && v.j > 0 && grains[v.i*xres + v.j-1] == gno)
                newdir = DOWN;
            else if (v.j > 0 && grains[(v.i-1)*xres + v.j-1] == gno)
                newdir = LEFT;
            else
                newdir = UP;
            break;

            case UP:
            v.i--;
            if (v.j > 0 && v.i > 0 && grains[(v.i-1)*xres + v.j-1] == gno)
                newdir = LEFT;
            else if (v.i > 0 && grains[(v.i-1)*xres + v.j] == gno)
                newdir = UP;
            else
                newdir = RIGHT;
            break;

            default:
            g_assert_not_reached();
            break;
        }

        /* When we turn right, the previous point is a potential vertex, and
         * it can also supersed previous vertices. */
        if (newdir == (dir + 1) % 4) {
            g_array_append_val(vertices, v);
            while (vertices->len > 2) {
                cur = &g_array_index(vertices, GridPoint, vertices->len-1);
                mid = &g_array_index(vertices, GridPoint, vertices->len-2);
                prev = &g_array_index(vertices, GridPoint, vertices->len-3);
                phi = atan2(cur->i - mid->i, cur->j - mid->j);
                phim = atan2(mid->i - prev->i, mid->j - prev->j);
                phi = fmod(phi - phim + 4.0*G_PI, 2.0*G_PI);
                /* This should be fairly save as (a) not real harm is done
                 * when we have an occasional extra vertex (b) the greatest
                 * possible angle is G_PI/2.0 */
                if (phi > 1e-12 && phi < G_PI)
                    break;

                /* Get rid of mid, it is in a locally concave part */
                g_array_index(vertices, GridPoint, vertices->len-2) = *cur;
                g_array_set_size(vertices, vertices->len-1);
            }
        }
    } while (v.i*xres + v.j != initpos);

    /* The last point is duplicated first point */
    g_array_set_size(vertices, vertices->len-1);
}

/**
 * grain_maximum_bound:
 * @vertices: Convex hull vertex list.
 * @qx: Scale (pixel size) in x-direction.
 * @qy: Scale (pixel size) in y-direction.
 * @vx: Location to store vector x component to.
 * @vy: Location to store vector y component to.
 *
 * Given a list of integer convex hull vertices, return the vector between
 * the two most distance vertices.
 *
 * FIXME: This is a blatantly naive O(n^2) algorithm.
 **/
static void
grain_maximum_bound(GArray *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    const GridPoint *a, *x;
    gdouble vm, v, dx, dy;
    guint g1, g2;

    vm = -G_MAXDOUBLE;
    for (g1 = 0; g1 < vertices->len; g1++) {
        a = &g_array_index(vertices, GridPoint, g1);
        for (g2 = g1 + 1; g2 < vertices->len; g2++) {
            x = &g_array_index(vertices, GridPoint, g2);
            dx = qx*(x->j - a->j);
            dy = qy*(x->i - a->i);
            v = dx*dx + dy*dy;
            if (v > vm) {
                vm = v;
                *vx = dx;
                *vy = dy;
            }
        }
    }
}

/**
 * grain_minimum_bound:
 * @vertices: Convex hull vertex list.
 * @qx: Scale (pixel size) in x-direction.
 * @qy: Scale (pixel size) in y-direction.
 * @vx: Location to store vector x component to.
 * @vy: Location to store vector y component to.
 *
 * Given a list of integer convex hull vertices, return the vector
 * corresponding to the minimum linear projection.
 *
 * FIXME: This is a blatantly naive O(n^2) algorithm.
 **/
static void
grain_minimum_bound(GArray *vertices,
                    gdouble qx, gdouble qy,
                    gdouble *vx, gdouble *vy)
{
    const GridPoint *a, *b, *x;
    gdouble vm, vm1, v, s, b2, bx, by, dx, dy, vx1, vy1;
    guint g1, g1p, g2;

    g_return_if_fail(vertices->len >= 3);

    vm = G_MAXDOUBLE;
    for (g1 = 0; g1 < vertices->len; g1++) {
        a = &g_array_index(vertices, GridPoint, g1);
        g1p = (g1 + 1) % vertices->len;
        b = &g_array_index(vertices, GridPoint, g1p);
        bx = qx*(b->j - a->j);
        by = qy*(b->i - a->i);
        b2 = bx*bx + by*by;
        vm1 = vx1 = vy1 = -G_MAXDOUBLE;
        for (g2 = 0; g2 < vertices->len; g2++) {
            x = &g_array_index(vertices, GridPoint, g2);
            dx = qx*(x->j - a->j);
            dy = qy*(x->i - a->i);
            s = (dx*bx + dy*by)/b2;
            dx -= s*bx;
            dy -= s*by;
            v = dx*dx + dy*dy;
            if (v > vm1) {
                vm1 = v;
                vx1 = dx;
                vy1 = dy;
            }
        }
        if (vm1 < vm) {
            vm = vm1;
            *vx = vx1;
            *vy = vy1;
        }
    }
}

/**
 * gwy_data_field_grains_get_values:
 * @data_field: Data field used for marking.  For some quantities its values
 *              are not used, but its dimensions determine the dimensions of
 *              @grains.
 * @values: An array of size @ngrains+1 to put grain values to.  It can be
 *          %NULL to allocate and return a new array.
 * @grains: Grain numbers filled with gwy_data_field_number_grains().
 * @ngrains: The number of grains as returned by
 *           gwy_data_field_number_grains().
 * @quantity: The quantity to calculate.
 *
 * Calculates characteristics of grains.
 *
 * This is a bit low-level function, see also
 * gwy_data_field_grains_get_distribution().
 *
 * The array @values will be filled with the requested grain value for each
 * individual grain (0th item of @values which do not correspond to any grain
 * is overwritten with an arbitrary value and should be ignored).
 *
 * The grain numbers serve as indices in @values.  Therefore as long as the
 * same @grains is used, the same position in @values corresponds to the same
 * particular grain.  This enables one for instance to calculate grain sizes
 * and grain heights and then correlate them.
 *
 * Returns: @values itself if it was not %NULL, otherwise a newly allocated
 *          array that caller has to free.
 **/
gdouble*
gwy_data_field_grains_get_values(GwyDataField *data_field,
                                 gdouble *values,
                                 gint ngrains,
                                 const gint *grains,
                                 GwyGrainQuantity quantity)
{
    const gdouble *data;
    gdouble *tmp;
    gint *sizes, *pos;
    gdouble q, qh, qv;
    gint xres, yres, i, j, nn;
    GArray *vertices;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(grains, NULL);

    if (!values)
        values = g_new(gdouble, ngrains + 1);

    xres = data_field->xres;
    yres = data_field->yres;
    nn = xres*yres;
    gwy_debug("ngrains: %d, nn: %d", ngrains, nn);
    qh = gwy_data_field_get_xmeasure(data_field);
    qv = gwy_data_field_get_ymeasure(data_field);

    data = data_field->data;
    switch (quantity) {
        case GWY_GRAIN_VALUE_PROJECTED_AREA:
        case GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE:
        case GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS:
        /* Find sizes */
        sizes = g_new0(gint, ngrains + 1);
        for (i = 0; i < nn; i++)
            sizes[grains[i]]++;
        /* q is the area of one pixel */
        q = qh*qv;
        switch (quantity) {
            case GWY_GRAIN_VALUE_PROJECTED_AREA:
            for (i = 0; i <= ngrains; i++)
                values[i] = q*sizes[i];
            break;

            case GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS:
            q /= G_PI;
            case GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE:
            for (i = 0; i <= ngrains; i++)
                values[i] = sqrt(q*sizes[i]);
            break;

            default:
            /* Die, die, GCC! */
            break;
        }
        g_free(sizes);
        break;

        case GWY_GRAIN_VALUE_MINIMUM:
        for (i = 0; i <= ngrains; i++)
            values[i] = G_MAXDOUBLE;
        for (i = 0; i < nn; i++) {
            if (data[i] < values[grains[i]])
                values[grains[i]] = data[i];
        }
        break;

        case GWY_GRAIN_VALUE_MAXIMUM:
        for (i = 0; i <= ngrains; i++)
            values[i] = -G_MAXDOUBLE;
        for (i = 0; i < nn; i++) {
            if (data[i] > values[grains[i]])
                values[grains[i]] = data[i];
        }
        break;

        case GWY_GRAIN_VALUE_MEAN:
        sizes = g_new0(gint, ngrains + 1);
        memset(values, 0, (ngrains + 1)*sizeof(gdouble));
        for (i = 0; i < nn; i++) {
            values[grains[i]] += data[i];
            sizes[grains[i]]++;
        }
        for (i = 0; i <= ngrains; i++)
            values[i] /= sizes[i];
        g_free(sizes);
        break;

        case GWY_GRAIN_VALUE_MEDIAN:
        /* Find sizes */
        sizes = g_new0(gint, 2*(ngrains + 1));
        pos = sizes + ngrains+1;
        for (i = 0; i < nn; i++)
            sizes[grains[i]]++;
        /* Find cumulative sizes (we care only about grains, ignore the
         * outside-grains area) */
        for (i = 2; i <= ngrains; i++)
            sizes[i] += sizes[i-1];
        sizes[0] = 0;
        tmp = g_new(gdouble, sizes[ngrains]);
        /* Find where each grain starts in tmp sorted by grain # */
        for (i = 1; i <= ngrains; i++)
            pos[i] = sizes[i-1];
        /* Sort values by grain # to tmp */
        for (i = 0; i < nn; i++) {
            if ((j = grains[i])) {
                tmp[pos[j]] = data[i];
                pos[j]++;
            }
        }
        /* Find medians of each block */
        for (i = 1; i <= ngrains; i++)
            values[i] = gwy_math_median(sizes[i] - sizes[i-1],
                                        tmp + sizes[i-1]);
        /* Finalize */
        g_free(sizes);
        g_free(tmp);
        break;

        case GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH:
        memset(values, 0, (ngrains + 1)*sizeof(gdouble));
        q = hypot(qh, qv);
        for (i = 0; i <= yres; i++) {
            for (j = 0; j <= xres; j++) {
                gint g1, g2, g3, g4, f;

                /* Hope compiler will optimize this mess... */
                g1 = (i > 0 && j > 0) ? grains[i*xres + j - xres - 1] : 0;
                g2 = (i > 0 && j < xres) ? grains[i*xres + j - xres] : 0;
                g3 = (i < yres && j > 0) ? grains[i*xres + j - 1] : 0;
                g4 = (i < yres && j < xres) ? grains[i*xres + j] : 0;
                f = (g1 > 0) + (g2 > 0) + (g3 > 0) + (g4 > 0);
                if (f == 0 || f == 4)
                    continue;

                if (f == 1 || f == 3) {
                    /* Try to avoid too many if-thens by using the fact they
                     * are all either zero or an identical value */
                    values[g1 | g2 | g3 | g4] += q/2.0;
                }
                else if (g1 && g4) {
                    /* This works for both g1 == g4 and g1 != g4 */
                    values[g1] += q/2.0;
                    values[g4] += q/2.0;
                }
                else if (g2 && g3) {
                    /* This works for both g2 == g3 and g2 != g3 */
                    values[g1] += q/2.0;
                    values[g4] += q/2.0;
                }
                else if (g1 == g2)
                    values[g1 | g3] += qh;
                else if (g1 == g3)
                    values[g1 | g2] += qv;
                else {
                    g_assert_not_reached();
                }
            }
        }
        break;

        case GWY_GRAIN_VALUE_SURFACE_AREA:
        memset(values, 0, (ngrains + 1)*sizeof(gdouble));
        q = qh*qv/8.0;
        qh = qh*qh;
        qv = qv*qv;
        /* Every contribution is calculated twice -- for each pixel (vertex)
         * participating to a particular triangle */
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gint ix, ipx, imx, jp, jm, gno;
                gdouble c;

                ix = i*xres;
                if (!(gno = grains[ix + j]))
                    continue;

                imx = (i > 0) ? ix-xres : ix;
                ipx = (i < yres-1) ? ix+xres : ix;
                jm = (j > 0) ? j-1 : j;
                jp = (j < yres-1) ? j+1 : j;

                c = (data[ix + j] + data[ix + jm]
                     + data[imx + jm] + data[imx + j])/2.0;

                values[gno] += square_area2w_1c(data[ix + j], data[ix + jm],
                                                data[imx + j], c, qh, qv);
                values[gno] += square_area2w_1c(data[ix + j], data[ix + jp],
                                                data[imx + j], c, qh, qv);
                values[gno] += square_area2w_1c(data[ix + j], data[ix + jm],
                                                data[ipx + j], c, qh, qv);
                values[gno] += square_area2w_1c(data[ix + j], data[ix + jp],
                                                data[ipx + j], c, qh, qv);
            }
        }
        for (i = 1; i <= ngrains; i++)
            values[i] *= q;
        break;

        case GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE:
        case GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE:
        case GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE:
        case GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE:
        /* Find a one convex hull point of each grain (particularly that
         * with the lowest i*xres+j) */
        pos = g_new(gint, ngrains + 1);
        for (i = 0; i <= ngrains; i++)
            pos[i] = -1;
        for (i = 0; i < nn; i++) {
            if (pos[grains[i]] == -1)
                pos[grains[i]] = i;
        }
        /* Find the complete convex hulls */
        vertices = g_array_new(FALSE, FALSE, sizeof(GridPoint));
        for (i = 1; i <= ngrains; i++) {
            gdouble dx, dy;

            find_grain_convex_hull(xres, yres, grains, pos[i], vertices);
            switch (quantity) {
                case GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE:
                grain_maximum_bound(vertices, qh, qv, &dx, &dy);
                values[i] = hypot(dx, dy);
                break;

                case GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE:
                grain_maximum_bound(vertices, qh, qv, &dx, &dy);
                values[i] = atan2(-dy, dx);
                if (values[i] <= -G_PI/2.0)
                    values[i] += G_PI;
                else if (values[i] > G_PI/2.0)
                    values[i] -= G_PI;
                break;

                case GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE:
                grain_minimum_bound(vertices, qh, qv, &dx, &dy);
                values[i] = hypot(dx, dy);
                break;

                case GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE:
                grain_minimum_bound(vertices, qh, qv, &dx, &dy);
                values[i] = atan2(-dy, dx);
                if (values[i] <= -G_PI/2.0)
                    values[i] += G_PI;
                else if (values[i] > G_PI/2.0)
                    values[i] -= G_PI;
                break;

                default:
                g_assert_not_reached();
                break;
            }
        }
        /* Finalize */
        g_array_free(vertices, TRUE);
        g_free(pos);
        break;

        case GWY_GRAIN_VALUE_VOLUME_0:
        case GWY_GRAIN_VALUE_VOLUME_MIN:
        /* Yay! We can recurse to calculate the minima! */
        if (quantity == GWY_GRAIN_VALUE_VOLUME_MIN) {
            tmp = g_new(gdouble, ngrains + 1);
            gwy_data_field_grains_get_values(data_field, tmp, ngrains, grains,
                                             GWY_GRAIN_VALUE_MINIMUM);
        }
        else
            tmp = NULL;

        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gint ix, ipx, imx, jp, jm, gno;
                gdouble c;

                ix = i*xres;
                if (!(gno = grains[ix + j]))
                    continue;

                imx = (i > 0) ? ix-xres : ix;
                ipx = (i < yres-1) ? ix+xres : ix;
                jm = (j > 0) ? j-1 : j;
                jp = (j < yres-1) ? j+1 : j;

                c = (data[ix + j] + data[ix + jm]
                     + data[imx + jm] + data[imx + j])/4.0;

                values[gno] = 0.0;
                values[gno] += square_volumew_1c(data[ix + j], data[ix + jm],
                                                 data[imx + j], c);
                values[gno] += square_volumew_1c(data[ix + j], data[ix + jp],
                                                 data[imx + j], c);
                values[gno] += square_volumew_1c(data[ix + j], data[ix + jm],
                                                 data[ipx + j], c);
                values[gno] += square_volumew_1c(data[ix + j], data[ix + jp],
                                                 data[ipx + j], c);

                /* We know the basis would appear with total weight -24 so
                 * don't bother subtracting it from individual heights */
                if (tmp)
                    values[gno] -= 24.0*tmp[gno];
            }
        }
        q = qh*qv/24.0;
        for (i = 1; i <= ngrains; i++)
            values[i] *= q;
        /* Finalize */
        g_free(tmp);
        break;

        case GWY_GRAIN_VALUE_VOLUME_LAPLACE:
        g_warning("Implement me!");
        break;

        default:
        g_warning("Wrong grain quantity %d", (gint)quantity);
        memset(values, 0, (ngrains + 1)*sizeof(gdouble));
        break;
    }

    return values;
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

void
gwy_data_field_grains_splash_water(GwyDataField *data_field,
                                   GwyDataField *water,
                                   gint locate_steps,
                                   gdouble locate_dropsize)
{
    GwyDataField *min, *mark_dfield;
    gint xres, yres, i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

    xres = data_field->xres;
    yres = data_field->yres;

    min = gwy_data_field_new_alike(data_field, TRUE);
    mark_dfield = gwy_data_field_duplicate(data_field);

    /* odrop */
    gwy_data_field_clear(water);
    for (i = 0; i < locate_steps; i++)
        drop_step(mark_dfield, water, locate_dropsize);

    gwy_data_field_invalidate(water);
    g_object_unref(mark_dfield);
}


/****************************************************************************/
/*private functions*/

static gboolean
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
        return TRUE;
    }
    else if (a >= v && a >= b && a >= c && a >= d) {
        *rcol += 1;
        return FALSE;
    }
    else if (b >= v && b >= a && b >= c && b >= d) {
        *rcol -= 1;
        return FALSE;
    }
    else if (c >= v && c >= b && c >= a && c >= d) {
        *rrow += 1;
        return FALSE;
    }
    else {
        *rrow -= 1;
        return FALSE;
    }

    return FALSE;
}

static void
drop_step(GwyDataField *data_field, GwyDataField *water_field, gdouble dropsize)
{
    gint xres, yres, i;
    gint col, row;
    gboolean retval;

    xres = data_field->xres;
    yres = data_field->yres;

    for (i = 0; i < xres*yres; i++) {
        row = (gint)floor((gdouble)i/(gdouble)xres);
        col = i - xres*row;
        if (col == 0 || row == 0 || col == (xres - 1) || row == (yres - 1))
            continue;

        do {
            retval = step_by_one(data_field, &col, &row);
        } while (!retval);

        water_field->data[col + xres*row] += 1;
        data_field->data[col + xres*row] -= dropsize;

    }
    gwy_data_field_invalidate(water_field);
    gwy_data_field_invalidate(data_field);
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
    ngrains = gwy_data_field_number_grains(water_field, grains);
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
    gint xres, yres, ival[4], val, i;
    gboolean stat;
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
    stat = FALSE;
    for (i = 0; i < 4; i++) {
        if (val > 0 && ival[i] > 0 && ival[i] != val) {
            /*if some value already was there and the now one is different */
            stat = TRUE;
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
    data[col + xres*row] = stat ? -1 : val;
}

static void
wdrop_step(GwyDataField *data_field, GwyDataField *min_field,
           GwyDataField *water_field, GwyDataField *grain_field,
           gdouble dropsize)
{
    gint xres, yres, vcol, vrow, col, row, grain;
    gboolean retval;

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
            do {
                retval = step_by_one(data_field, &vcol, &vrow);
            } while (!retval);

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
    /* FIXME: it is not necessary to duplicate complete data field to check
     * a few boundary pixels. */
    buffer = gwy_data_field_duplicate(grain_field);
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

/* Merge grains i and j in map with full resolution */
static inline void
resolve_grain_map(gint *m, gint i, gint j)
{
    gint ii, jj, k;

    /* Find what i and j fully resolve to */
    for (ii = i; m[ii] != ii; ii = m[ii])
        ;
    for (jj = j; m[jj] != jj; jj = m[jj])
        ;
    k = MIN(ii, jj);

    /* Fix partial resultions to full */
    for (ii = m[i]; m[ii] != ii; ii = m[ii]) {
        m[i] = k;
        i = ii;
    }
    m[ii] = k;
    for (jj = m[j]; m[jj] != jj; jj = m[jj]) {
        m[j] = k;
        j = jj;
    }
    m[jj] = k;
}

/**
 * gwy_data_field_number_grains:
 * @mask_field: Data field containing positive values in grains, nonpositive
 *              in free space.
 * @grains: Zero-filled array of integers of equal size to @mask_field to put
 *          grain numbers to.  Empty space will be left 0, pixels inside a
 *          grain will be set to grain number.  Grains are numbered
 *          sequentially 1, 2, 3, ...
 *
 * Numbers grains in a mask data field.
 *
 * Returns: The number of last grain (note they are numbered from 1).
 **/
gint
gwy_data_field_number_grains(GwyDataField *mask_field,
                             gint *grains)
{
    const gdouble *data;
    gint xres, yres, i, j, grain_id, max_id, id;
    gint *m, *mm;

    xres = mask_field->xres;
    yres = mask_field->yres;
    data = mask_field->data;

    /* The max number of grains, reached with checkerboard pattern */
    m = g_new0(gint, (xres*yres + 1)/2 + 1);

    /* Number grains with simple unidirectional grain number propagation,
     * updating map m for later full grain join */
    max_id = 0;
    /* Special-case first row for which no top row exist to avoid testing
     * i > 0 below */
    grain_id = 0;
    for (j = 0; j < xres; j++) {
        if (data[j] > 0.0) {
            if (!grain_id) {
                grain_id = ++max_id;
                m[grain_id] = grain_id;
            }
            grains[j] = grain_id;
        }
        else
            grain_id = 0;
    }
    /* The rest of rows */
    for (i = 1; i < yres; i++) {
        grain_id = 0;
        for (j = 0; j < xres; j++) {
            if (data[i*xres + j] > 0.0) {
                /* Grain number is kept from left neighbour unless it does
                 * not exist (a new number is assigned) or a join with top
                 * neighbour occurs (m is updated) */
                if ((id = grains[(i - 1)*xres + j])) {
                    if (!grain_id)
                        grain_id = id;
                    else if (id != grain_id) {
                        resolve_grain_map(m, id, grain_id);
                        grain_id = m[id];
                    }
                }
                if (!grain_id) {
                    grain_id = ++max_id;
                    m[grain_id] = grain_id;
                }
                grains[i*xres + j] = grain_id;
            }
            else
                grain_id = 0;
        }
    }

    /* Resolve remianing grain number links in map */
    for (i = 1; i <= max_id; i++)
        m[i] = m[m[i]];

    /* Compactify grain numbers */
    mm = g_new0(gint, max_id + 1);
    id = 0;
    for (i = 1; i <= max_id; i++) {
        if (!mm[m[i]]) {
            id++;
            mm[m[i]] = id;
        }
        m[i] = mm[m[i]];
    }
    g_free(mm);

    /* Renumber grains (we make use of the fact m[0] = 0) */
    for (i = 0; i < xres*yres; i++)
        grains[i] = m[grains[i]];

    g_free(m);

    return id;
}

/**
 * gwy_data_field_get_grain_bounding_boxes:
 * @mask_field: Data field containing positive values in grains, nonpositive
 *              in free space.  However its contents is ignored as all
 *              grain information is taken from @grains (its dimensions
 *              determine the dimensions of @grains).
 * @ngrains: The number of grains as returned by
 *           gwy_data_field_number_grains().
 * @grains: Grain numbers filled with gwy_data_field_number_grains().
 * @bboxes: Array of size at least 4@ngrains to fill with grain bounding
 *          boxes.  The bounding boxes are stored as quadruples of indices:
 *          (xmin, ymin, width, height).  It can be %NULL to allocate a new
 *          array.
 *
 * Find bounding boxes of all grains.
 *
 * Returns: Either %bboxes (if it was not %NULL), or a newly allocated array
 *          of size 4@ngrains.
 *
 * Since: 2.3
 **/
gint*
gwy_data_field_get_grain_bounding_boxes(GwyDataField *mask_field,
                                        gint ngrains,
                                        const gint *grains,
                                        gint *bboxes)
{
    gint xres, yres, i, j, id;
    const gdouble *data;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(mask_field), NULL);
    g_return_val_if_fail(grains, NULL);

    xres = mask_field->xres;
    yres = mask_field->yres;
    data = mask_field->data;
    if (!bboxes)
        bboxes = g_new(gint, 4*ngrains);

    for (i = 0; i < ngrains; i++) {
        bboxes[4*i] = bboxes[4*i + 1] = G_MAXINT;
        bboxes[4*i + 2] = bboxes[4*i + 3] = -1;
    }

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            if ((id = grains[i*xres + j])) {
                id *= 4;
                if (j < bboxes[id])
                    bboxes[id] = j;
                if (i < bboxes[id + 1])
                    bboxes[id + 1] = i;
                if (j > bboxes[id + 2])
                    bboxes[id + 2] = j;
                if (i > bboxes[id + 3])
                    bboxes[id + 3] = i;
            }
        }
    }

    for (i = 0; i < ngrains; i++) {
        bboxes[4*i + 2] = bboxes[4*i + 2] - bboxes[4*i]     + 1;
        bboxes[4*i + 3] = bboxes[4*i + 3] - bboxes[4*i + 1] + 1;
    }

    return bboxes;
}

/**
 * gwy_data_field_area_grains_tgnd:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @below: If %TRUE, valleys are marked, otherwise mountains are marked.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates threshold grain number distribution.
 *
 * This is the number of grains for each of @nstats equidistant height
 * threshold levels.  For large @nstats this function is much faster than the
 * equivalent number of gwy_data_field_grains_mark_height().
 **/
void
gwy_data_field_area_grains_tgnd(GwyDataField *data_field,
                                GwyDataLine *target_line,
                                gint col, gint row,
                                gint width, gint height,
                                gboolean below,
                                gint nstats)
{
    gint *heights, *hindex, *nh, *grains, *listv, *listh, *m, *mm;
    gdouble min, max, q;
    gint i, j, k, h, n;
    gint grain_no, last_grain_no;
    guint msize;

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

    gwy_data_field_area_get_min_max(data_field, NULL,
                                    col, row, width, height,
                                    &min, &max);
    n = width*height;
    if (max == min || n == 0) {
        gwy_data_line_clear(target_line);
        return;
    }
    gwy_data_line_set_real(target_line, max - min);
    gwy_data_line_set_offset(target_line, min);

    /* Calculate discrete heights.
     *
     * The buckets are a bit unusual.  We want the distribution symmetric from
     * above- and below- viewpoint.  That means if we invert the input data
     * field, negate @below, and reverse the contents of @nh we want to get
     * the same distribution (except points that are excatly equal to some
     * bucked edge value, they get rounded to the other bucker).  This means
     * if nh[nstats-1] == n, then it must be nh[0] = 0. */
    heights = g_new(gint, n);
    q = (nstats - 1.0)/(max - min);
    for (i = 0; i < height; i++) {
        const gdouble *drow = data_field->data
                              + (i + row)*data_field->xres + col;

        if (below) {
            for (j = 0; j < width; j++) {
                k = (gint)((drow[j] - min)*q + 1);
                heights[i*width + j] = CLAMP(k, 0, nstats-1);
            }
        }
        else {
            for (j = 0; j < width; j++) {
                k = (gint)((max - drow[j])*q + 1);
                heights[i*width + j] = CLAMP(k, 0, nstats-1);
            }
        }
    }

    /* Calculate inverse discrete distribution function nh.  In other words,
     * group pixels of the same discrete height.  nh then holds indices in
     * hindex where each height starts.  The other half of nh serves as a
     * scratch space where we keep how many pixels we have in each group
     * so far.
     *
     * The purpose of all this is to scale well with nstats.  For small nstats
     * we use more memory and thus trash CPU more badly, but the big advantage
     * is that we do not scan complete @heights each iteration and only touch
     * the pixels that actually constitute the grains (and their neighbours).
     */
    nh = g_new0(gint, 2*nstats);
    for (i = 0; i < n; i++)
        nh[heights[i]]++;
    for (i = 1; i < nstats; i++)
        nh[i] += nh[i-1];
    for (i = nstats-1; i; i--)
        nh[i] = nh[i-1];
    nh[0] = 0;

    hindex = g_new(gint, n);
    for (i = 0; i < n; i++) {
        h = heights[i];
        hindex[nh[h] + nh[h + nstats]] = i;
        nh[h + nstats]++;
    }
    nh[nstats] = n;    /* To avoid special-casing in cycle limits below */

    grains = g_new0(gint, n);
    listv = g_new(gint, n/2 + 2);
    listh = g_new(gint, n/2 + 2);

    m = g_new(gint, 1);
    mm = m;
    msize = 0;

    /* Main iteration */
    last_grain_no = 0;
    target_line->data[0] = 0;
    for (h = 0; h < nstats; h++) {
        /* Mark new subgrains corresponding just to height h */
        grain_no = last_grain_no;
        gwy_debug("Height %d, number of old grains: %d", h, grain_no);
        for (i = nh[h]; i < nh[h+1]; i++) {
            j = hindex[i];
            if (!grains[j]) {
                grain_no++;
                gwy_data_field_fill_one_grain(width, height, heights,
                                              j % width, j/width,
                                              grains, grain_no, listv, listh);
            }
        }
        gwy_debug("new subgrains: %d", grain_no-last_grain_no);

        if (grain_no == last_grain_no) {
            gwy_debug("skipping empty height level");
            if (h)
                target_line->data[h] = target_line->data[h-1];
            continue;
        }

        /* Initialize grains number maps for merge scan */
        if (grain_no+1 > msize) {
            g_free(m);
            m = g_new(gint, 2*(grain_no+1));
            mm = m + grain_no+1;
        }
        for (i = 0; i <= grain_no; i++) {
            m[i] = i;
            mm[i] = 0;
        }

        /* Find grains that touch each other for merge.
         *
         * Grains that did not touch before don't touch now.  So we are only
         * interested in neighbours of pixels of new subgrains. */
        for (i = nh[h]; i < nh[h+1]; i++) {
            j = hindex[i];
            /* Left */
            if (j % width && grains[j-1]
                && m[grains[j]] != m[grains[j-1]])
                resolve_grain_map(m, grains[j], grains[j-1]);
            /* Right */
            if ((j+1) % width && grains[j+1]
                && m[grains[j]] != m[grains[j+1]])
                resolve_grain_map(m, grains[j], grains[j+1]);
            /* Up */
            if (j/width && grains[j-width]
                && m[grains[j]] != m[grains[j-width]])
                resolve_grain_map(m, grains[j], grains[j-width]);

            /* Down */
            if (j/width < height-1 && grains[j+width]
                && m[grains[j]] != m[grains[j+width]])
                resolve_grain_map(m, grains[j], grains[j+width]);
        }

        /* Resolve remianing grain number links in m */
        for (i = 1; i <= grain_no; i++)
            m[i] = m[m[i]];

        /* Compactify grain numbers */
        k = 0;
        for (i = 1; i <= grain_no; i++) {
            if (!mm[m[i]]) {
                k++;
                mm[m[i]] = k;
            }
            m[i] = mm[m[i]];
        }

#ifdef DEBUG
        for (i = 0; i <= grain_no; i++)
            g_printerr("%d[%d] ", m[i], i);
        g_printerr("\n");
#endif

        /* Renumber grains (we make use of the fact m[0] = 0).
         *
         * This is the only place where we have to scan complete data field.
         * Since grain numbers usually vary wildly and globally, we probably
         * can't avoid it. */
        for (i = 0; i < n; i++)
            grains[i] = m[grains[i]];

        /* The number of grains for this h */
        target_line->data[h] = k;
        last_grain_no = k;
    }

    g_free(m);
    g_free(listv);
    g_free(listh);
    g_free(grains);
    g_free(hindex);
    g_free(nh);
    g_free(heights);
}

/**
 * gwy_data_field_fill_grain:
 * @data_field: A data field with zeroes in empty space and nonzeroes in grains.
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
gwy_data_field_fill_grain(GwyDataField *data_field,
                          gint col, gint row, gint *nindices)
{
    gint *data, *visited, *listv, *listh;
    gint *indices;
    gint xres, yres, n, count;
    gint i, j;
    gint initial;

    xres = data_field->xres;
    yres = data_field->yres;
    initial = row*xres + col;
    g_return_val_if_fail(data_field->data[initial], NULL);

    /* check for a single point */
    if ((!col || data_field->data[initial - 1] <= 0)
        && (!row || data_field->data[initial - xres] <= 0)
        && (col + 1 == xres || data_field->data[initial + 1] <= 0)
        && (row + 1 == yres || data_field->data[initial + xres] <= 0)) {
        indices = g_new(gint, 1);

        indices[0] = initial;
        *nindices = 1;

        return indices;
    }

    n = xres*yres;
    visited = g_new0(gint, n);
    data = g_new(gint, n);
    listv = g_new(gint, n/2 + 2);
    listh = g_new(gint, n/2 + 2);

    for (i = 0; i < n; i++)
        data[i] = data_field->data[i] > 0;

    count = gwy_data_field_fill_one_grain(xres, yres, data, col, row,
                                          visited, 1, listv, listh);

    g_free(listh);
    g_free(listv);
    g_free(data);

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
 * @xres: The number of columns in @data.
 * @yres: The number of rows in @data.
 * @data: Arbitrary integer data.  Grain is formed by values equal to the
 *        value at (@col, @row).
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
gwy_data_field_fill_one_grain(gint xres,
                              gint yres,
                              const gint *data,
                              gint col, gint row,
                              gint *visited,
                              gint grain_no,
                              gint *listv,
                              gint *listh)
{
    gint n, count;
    gint nh, nv;
    gint i, p, j;
    gint initial;
    gint look_for;

    g_return_val_if_fail(grain_no, 0);
    initial = row*xres + col;
    look_for = data[initial];

    /* check for a single point */
    visited[initial] = grain_no;
    count = 1;
    if ((!col || data[initial - 1] != look_for)
        && (!row || data[initial - xres] != look_for)
        && (col + 1 == xres || data[initial + 1] != look_for)
        && (row + 1 == yres || data[initial + xres] != look_for)) {

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
                    if (visited[j] || data[j] != look_for)
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
                    if (visited[j] || data[j] != look_for)
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
                    if (visited[j] || data[j] != look_for)
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
                    if (visited[j] || data[j] != look_for)
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

/************************** Documentation ****************************/

/**
 * SECTION:grains
 * @title: grains
 * @short_description: Grain detection and processing
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
