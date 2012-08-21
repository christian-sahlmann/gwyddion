/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2012 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/linestats.h>
#include <libprocess/filters.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libprocess/grains.h>

#if GLIB_CHECK_VERSION(2, 10, 0)
#define ONE G_GUINT64_CONSTANT(1)
#else
#define ONE G_GINT64_CONSTANT(1U)
#endif

typedef struct {
    gint i;
    gint j;
} GridPoint;

typedef struct {
    guint size;
    guint len;
    GridPoint *points;
} PixelQueue;

typedef struct {
    gdouble xa;
    gdouble ya;
    gdouble xb;
    gdouble yb;
    gdouble r2;
} Edge;

typedef struct {
    guint size;
    guint len;
    Edge *edges;
} EdgeQueue;

typedef struct {
    gdouble x;
    gdouble y;
    gdouble R2;
    guint size;   /* For candidate sorting. */
} InscribedDisc;

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

enum { NDIRECTIONS = 12 };

static const gdouble shift_directions[NDIRECTIONS*2] = {
    1.0, 0.0,
    0.9914448613738104, 0.1305261922200516,
    0.9659258262890683, 0.2588190451025207,
    0.9238795325112867, 0.3826834323650898,
    0.8660254037844387, 0.5,
    0.7933533402912352, 0.6087614290087207,
    0.7071067811865476, 0.7071067811865475,
    0.6087614290087207, 0.7933533402912352,
    0.5,                0.8660254037844386,
    0.3826834323650898, 0.9238795325112867,
    0.2588190451025207, 0.9659258262890683,
    0.1305261922200517, 0.9914448613738104,
};

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
    gwy_data_field_filter_sobel(masky, GWY_ORIENTATION_VERTICAL);

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
 * Removes all grains below specified area.
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
    /* Avoid some no-op work below. */
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
    /* Avoid some no-op work below. */
    grain_kill[0] = FALSE;

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
 * gwy_data_field_grains_remove_touching_border:
 * @grain_field: Field of marked grains (mask).
 *
 * Removes all grains that touch field borders.
 *
 * Since: 2.30
 **/
void
gwy_data_field_grains_remove_touching_border(GwyDataField *grain_field)
{
    gint i, xres, yres, ngrains;
    gdouble *data;
    gint *grains;
    gboolean *touching;

    g_return_if_fail(GWY_IS_DATA_FIELD(grain_field));

    xres = grain_field->xres;
    yres = grain_field->yres;
    data = grain_field->data;

    grains = g_new0(gint, xres*yres);
    ngrains = gwy_data_field_number_grains(grain_field, grains);

    /* Remember grains that touch any border. */
    touching = g_new0(gboolean, ngrains + 1);
    for (i = 0; i < xres; i++)
        touching[grains[i]] = TRUE;
    for (i = 1; i < yres-1; i++) {
        touching[grains[i*xres]] = TRUE;
        touching[grains[i*xres + xres-1]] = TRUE;
    }
    for (i = 0; i < xres; i++)
        touching[grains[(yres-1)*xres + i]] = TRUE;
    /* Avoid some no-op work below. */
    touching[0] = FALSE;

    /* Remove grains. */
    for (i = 0; i < xres*yres; i++) {
        if (touching[grains[i]])
            data[i] = 0;
    }
    for (i = 1; i <= ngrains; i++) {
        if (touching[i]) {
            gwy_data_field_invalidate(grain_field);
            break;
        }
    }

    g_free(grains);
    g_free(touching);
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
    gint *mygrains = NULL;

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

    if (!distribution)
        distribution = gwy_data_line_new(1, 1.0, FALSE);

    gwy_data_line_distribution(values, distribution, 0.0, 0.0, FALSE, nstats);

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
    gint initpos, gno;

    g_return_if_fail(grains[pos]);

    g_array_set_size(vertices, 0);
    initpos = pos;
    gno = grains[pos];
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

static gdouble
grain_convex_hull_area(GArray *vertices, gdouble dx, gdouble dy)
{
    const GridPoint *a = &g_array_index(vertices, GridPoint, 0),
                    *b = &g_array_index(vertices, GridPoint, 1),
                    *c = &g_array_index(vertices, GridPoint, 2);
    gdouble s = 0.0;
    guint i;

    g_return_val_if_fail(vertices->len >= 4, 0.0);

    for (i = 2; i < vertices->len; i++) {
        gdouble bx = b->j - a->j, by = b->i - a->i,
                cx = c->j - a->j, cy = c->i - a->i;
        s += 0.5*(bx*cy - by*cx);
        b = c;
        c++;
    }

    return dx*dy*s;
}

static void
grain_convex_hull_centre(GArray *vertices,
                         gdouble dx, gdouble dy,
                         gdouble *centrex, gdouble *centrey)
{
    const GridPoint *a = &g_array_index(vertices, GridPoint, 0),
                    *b = &g_array_index(vertices, GridPoint, 1),
                    *c = &g_array_index(vertices, GridPoint, 2);
    gdouble s = 0.0, xc = 0.0, yc = 0.0;
    guint i;

    g_return_if_fail(vertices->len >= 4);

    for (i = 2; i < vertices->len; i++) {
        gdouble bx = b->j - a->j, by = b->i - a->i,
                cx = c->j - a->j, cy = c->i - a->i;
        gdouble s1 = bx*cy - by*cx;
        xc += s1*(a->j + b->j + c->j);
        yc += s1*(a->i + b->i + c->i);
        s += s1;
        b = c;
        c++;
    }
    *centrex = xc*dx/(3.0*s);
    *centrey = yc*dy/(3.0*s);
}

static gdouble
minimize_circle_radius(InscribedDisc *circle, GArray *vertices,
                       gdouble dx, gdouble dy)
{
    const GridPoint *v = (const GridPoint*)vertices->data;
    gdouble x = circle->x, y = circle->y, r2best = 0.0;
    guint n = vertices->len;

    while (n--) {
        gdouble deltax = dx*v->j - x, deltay = dy*v->i - y;
        gdouble r2 = deltax*deltax + deltay*deltay;

        if (r2 > r2best)
            r2best = r2;

        v++;
    }

    return r2best;
}

static void
improve_circumscribed_circle(InscribedDisc *circle, GArray *vertices,
                             gdouble dx, gdouble dy)
{
    gdouble eps = 1.0, improvement, qgeom = sqrt(dx*dy);

    do {
        InscribedDisc best = *circle;
        guint i;

        improvement = 0.0;
        for (i = 0; i < NDIRECTIONS; i++) {
            InscribedDisc cand;
            gdouble sx = eps*qgeom*shift_directions[2*i],
                    sy = eps*qgeom*shift_directions[2*i + 1];

            cand.size = circle->size;

            cand.x = circle->x + sx;
            cand.y = circle->y + sy;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x - sy;
            cand.y = circle->y + sx;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x - sx;
            cand.y = circle->y - sy;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;

            cand.x = circle->x + sy;
            cand.y = circle->y - sx;
            if ((cand.R2 = minimize_circle_radius(&cand, vertices, dx, dy))
                < best.R2)
                best = cand;
        }
        if (best.R2 < circle->R2) {
            improvement = (best.R2 - circle->R2)/(dx*dy);
            *circle = best;
        }
        else {
            eps *= 0.5;
        }
    } while (eps > 1e-3 || improvement > 1e-3);
}

static inline void
pixel_queue_add(PixelQueue *queue,
                gint i, gint j)
{
    if (G_UNLIKELY(queue->len == queue->size)) {
        queue->size = MAX(2*queue->size, 16);
        queue->points = g_renew(GridPoint, queue->points, queue->size);
    }

    queue->points[queue->len].i = i;
    queue->points[queue->len].j = j;
    queue->len++;
}

static guint*
grain_maybe_realloc(guint *grain, guint w, guint h, guint *grainsize)
{
    if (w*h > *grainsize) {
        g_free(grain);
        *grainsize = w*h;
        grain = g_new(guint, *grainsize);
    }
    return grain;
}

static guint*
extract_upsampled_square_pixel_grain(const guint *grains, guint xres, guint gno,
                                     const gint *bbox,
                                     guint *grain, guint *grainsize,
                                     guint *widthup, guint *heightup,
                                     gdouble dx, gdouble dy)
{
    gint col = bbox[0], row = bbox[1], w = bbox[2], h = bbox[3];
    guint w2 = 2*w, h2 = 2*h;
    guint i, j;

    /* Do not bother with nearly square pixels and upsample also 2×2. */
    if (fabs(log(dy/dx)) < 0.05) {
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        for (i = 0; i < h; i++) {
            guint k2 = w2*(2*i);
            guint k = (i + row)*xres + col;
            for (j = 0; j < w; j++, k++, k2 += 2) {
                guint v = (grains[k] == gno) ? G_MAXUINT : 0;
                grain[k2] = v;
                grain[k2+1] = v;
                grain[k2 + w2] = v;
                grain[k2 + w2+1] = v;
            }
        }
    }
    else if (dy < dx) {
        /* Horizontal upsampling, precalculate index map to use in each row. */
        guint *indices;
        w2 = GWY_ROUND(dx/dy*w2);
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        indices = (guint*)g_slice_alloc(w2*sizeof(guint));
        for (j = 0; j < w2; j++) {
            gint jj = GWY_ROUND(0.5*j*dy/dx);
            indices[j] = CLAMP(jj, 0, (gint)w-1);
        }
        for (i = 0; i < h; i++) {
            guint k = (i + row)*xres + col;
            guint k2 = w2*(2*i);
            for (j = 0; j < w2; j++) {
                guint v = (grains[k + indices[j]] == gno) ? G_MAXUINT : 0;
                grain[k2 + j] = v;
                grain[k2 + w2 + j] = v;
            }
        }
        g_slice_free1(w2*sizeof(guint), indices);
    }
    else {
        /* Vertical upsampling, rows are 2× scaled copies but uneven. */
        h2 = GWY_ROUND(dy/dx*h2);
        grain = grain_maybe_realloc(grain, w2, h2, grainsize);
        for (i = 0; i < h2; i++) {
            guint k, k2 = i*w2;
            gint ii = GWY_ROUND(0.5*i*dx/dy);
            ii = CLAMP(ii, 0, (gint)h-1);
            k = (ii + row)*xres + col;
            for (j = 0; j < w; j++) {
                guint v = (grains[k + j] == gno) ? G_MAXUINT : 0;
                grain[k2 + 2*j] = v;
                grain[k2 + 2*j + 1] = v;
            }
        }
    }

    *widthup = w2;
    *heightup = h2;
    return grain;
}

/* Init @queue with all Von Neumann-neighbourhood boundary pixels. */
static void
init_erosion(guint *grain,
             gint width, gint height,
             PixelQueue *queue)
{
    gint i, j, k;

    queue->len = 0;
    k = 0;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++, k++) {
            if (!grain[k])
                continue;

            if (!i || !grain[k - width]
                || !j || !grain[k - 1]
                || j == width-1 || !grain[k + 1]
                || i == height-1 || !grain[k + width]) {
                grain[k] = 1;
                pixel_queue_add(queue, i, j);
            }
        }
    }
}

static gboolean
erode_4(guint *grain,
        gint width, gint height,
        guint id,
        const PixelQueue *inqueue,
        PixelQueue *outqueue)
{
    const GridPoint *ipt = inqueue->points;
    guint m;

    outqueue->len = 0;
    for (m = inqueue->len; m; m--, ipt++) {
        gint i = ipt->i, j = ipt->j, k = i*width + j;

        if (i && grain[k - width] == G_MAXUINT) {
            grain[k - width] = id+1;
            pixel_queue_add(outqueue, i-1, j);
        }
        if (j && grain[k - 1] == G_MAXUINT) {
            grain[k - 1] = id+1;
            pixel_queue_add(outqueue, i, j-1);
        }
        if (j < width-1 && grain[k + 1] == G_MAXUINT) {
            grain[k + 1] = id+1;
            pixel_queue_add(outqueue, i, j+1);
        }
        if (i < height-1 && grain[k + width] == G_MAXUINT) {
            grain[k + width] = id+1;
            pixel_queue_add(outqueue, i+1, j);
        }
    }

    return outqueue->len;
}

static gboolean
erode_8(guint *grain,
        gint width, gint height,
        guint id,
        const PixelQueue *inqueue,
        PixelQueue *outqueue)
{
    const GridPoint *ipt = inqueue->points;
    guint m;

    outqueue->len = 0;
    for (m = inqueue->len; m; m--, ipt++) {
        gint i = ipt->i, j = ipt->j, k = i*width + j;
        if (i && j && grain[k - width - 1] == G_MAXUINT) {
            grain[k - width - 1] = id+1;
            pixel_queue_add(outqueue, i-1, j-1);
        }
        if (i && grain[k - width] == G_MAXUINT) {
            grain[k - width] = id+1;
            pixel_queue_add(outqueue, i-1, j);
        }
        if (i && j < width-1 && grain[k - width + 1] == G_MAXUINT) {
            grain[k - width + 1] = id+1;
            pixel_queue_add(outqueue, i-1, j+1);
        }
        if (j && grain[k - 1] == G_MAXUINT) {
            grain[k - 1] = id+1;
            pixel_queue_add(outqueue, i, j-1);
        }
        if (j < width-1 && grain[k + 1] == G_MAXUINT) {
            grain[k + 1] = id+1;
            pixel_queue_add(outqueue, i, j+1);
        }
        if (i < height-1 && j && grain[k + width - 1] == G_MAXUINT) {
            grain[k + width - 1] = id+1;
            pixel_queue_add(outqueue, i+1, j-1);
        }
        if (i < height-1 && grain[k + width] == G_MAXUINT) {
            grain[k + width] = id+1;
            pixel_queue_add(outqueue, i+1, j);
        }
        if (i < height-1 && j < width-1 && grain[k + width + 1] == G_MAXUINT) {
            grain[k + width + 1] = id+1;
            pixel_queue_add(outqueue, i+1, j+1);
        }
    }

    return outqueue->len;
}

static gint
compare_candidates(gconstpointer a,
                   gconstpointer b)
{
    const InscribedDisc *da = (const InscribedDisc*)a;
    const InscribedDisc *db = (const InscribedDisc*)b;

    if (da->size < db->size)
        return -1;
    if (da->size > db->size)
        return 1;

    if (da->R2 < db->R2)
        return -1;
    if (da->R2 > db->R2)
        return 1;

    return 0;
}

static void
find_disc_centre_candidates(GArray *candidates,
                            PixelQueue *inqueue,
                            gdouble dx, gdouble dy,
                            gdouble centrex, gdouble centrey)
{
    guint m, n, start, end;

    /* Grain numbering for sparse areas, we reorder inqueue to contiguous
     * blocks, each getting the area number in candareas[]. */
    start = 0;
    end = 1;
    g_array_set_size(candidates, 0);
    for (m = 0; m < inqueue->len; m++) {
        GridPoint *mpt = inqueue->points + m;
        guint i = mpt->i, j = mpt->j;

        for (n = end; n < inqueue->len; n++) {
            GridPoint *npt = inqueue->points + n;
            guint ii = npt->i, jj = npt->j;

            if ((ii == i && (jj == j+1 || jj+1 == j))
                || (jj == j && (ii == i+1 || ii+1 == i))) {
                if (n != end)
                    GWY_SWAP(GridPoint, inqueue->points[end], *npt);
                end++;
            }
        }

        if (end == m+1 || end == inqueue->len) {
            InscribedDisc cand = { 0.0, 0.0, 0.0, 0 };

            for (n = start; n < end; n++) {
                const GridPoint *npt = inqueue->points + n;

                cand.x += npt->j;
                cand.y += npt->i;
                cand.size++;
            }
            cand.x = (cand.x/cand.size + 0.5)*dx;
            cand.y = (cand.y/cand.size + 0.5)*dy;
            /* Use R2 temporarily for distance from the entire grain centre;
             * this is only for sorting below. */
            cand.R2 = ((cand.x - centrex)*(cand.x - centrex)
                        + (cand.y - centrey)*(cand.y - centrey));
            g_array_append_val(candidates, cand);
            if (end == inqueue->len)
                break;
            start = end;
            end++;
        }
    }

    g_array_sort(candidates, &compare_candidates);
}

static inline void
edge_list_add(EdgeQueue *queue,
              gdouble xa, gdouble ya,
              gdouble xb, gdouble yb)
{
    if (G_UNLIKELY(queue->len == queue->size)) {
        queue->size = MAX(2*queue->size, 16);
        queue->edges = g_renew(Edge, queue->edges, queue->size);
    }

    queue->edges[queue->len].xa = xa;
    queue->edges[queue->len].ya = ya;
    queue->edges[queue->len].xb = xb;
    queue->edges[queue->len].yb = yb;
    queue->len++;
}

static void
find_all_edges(EdgeQueue *edges,
               const gint *grains, guint xres,
               guint gno, const gint *bbox,
               gdouble dx, gdouble dy)
{
    guint col = bbox[0], row = bbox[1], w = bbox[2], h = bbox[3];
    guint i, j;
    guint *vertices;

    edges->len = 0;

    vertices = g_slice_alloc((w + 1)*sizeof(guint));
    for (j = 0; j <= w; j++)
        vertices[j] = G_MAXUINT;

    for (i = 0; i <= h; i++) {
        guint k = (i + row)*xres + col;
        guint vertex = G_MAXUINT;

        for (j = 0; j <= w; j++, k++) {
            /*
             * 1 2
             * 3 4
             */
            guint g0 = i && j && grains[k - xres - 1] == gno;
            guint g1 = i && j < w && grains[k - xres] == gno;
            guint g2 = i < h && j && grains[k - 1] == gno;
            guint g3 = i < h && j < w && grains[k] == gno;
            guint g = g0 | (g1 << 1) | (g2 << 2) | (g3 << 3);

            if (g == 8 || g == 7) {
                g_assert(vertices[j] == G_MAXUINT);
                g_assert(vertex == G_MAXUINT);
                vertex = j;
                vertices[j] = i;
            }
            else if (g == 2 || g == 13) {
                g_assert(vertices[j] != G_MAXUINT);
                g_assert(vertex == G_MAXUINT);
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = j;
                vertices[j] = G_MAXUINT;
            }
            else if (g == 4 || g == 11) {
                g_assert(vertices[j] == G_MAXUINT);
                g_assert(vertex != G_MAXUINT);
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                vertex = G_MAXUINT;
                vertices[j] = i;
            }
            else if (g == 1 || g == 14) {
                g_assert(vertices[j] != G_MAXUINT);
                g_assert(vertex != G_MAXUINT);
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = G_MAXUINT;
                vertices[j] = G_MAXUINT;
            }
            else if (g == 6 || g == 9) {
                g_assert(vertices[j] != G_MAXUINT);
                g_assert(vertex != G_MAXUINT);
                edge_list_add(edges, dx*vertex, dy*i, dx*j, dy*i);
                edge_list_add(edges, dx*j, dy*vertices[j], dx*j, dy*i);
                vertex = j;
                vertices[j] = i;
            }
        }
        g_assert(vertex == G_MAXUINT);
    }
    for (j = 0; j <= w; j++) {
        g_assert(vertices[j] == G_MAXUINT);
    }

    g_slice_free1((w + 1)*sizeof(guint), vertices);
}

static gdouble
maximize_disc_radius(InscribedDisc *disc, Edge *edges, guint n)
{
    gdouble x = disc->x, y = disc->y, r2best = HUGE_VAL;

    while (n--) {
        gdouble rax = edges->xa - x, ray = edges->ya - y,
                rbx = edges->xb - x, rby = edges->yb - y,
                deltax = edges->xb - edges->xa, deltay = edges->yb - edges->ya;
        gdouble ca = -(deltax*rax + deltay*ray),
                cb = deltax*rbx + deltay*rby;

        if (ca <= 0.0)
            edges->r2 = rax*rax + ray*ray;
        else if (cb <= 0.0)
            edges->r2 = rbx*rbx + rby*rby;
        else {
            gdouble tx = cb*rax + ca*rbx, ty = cb*ray + ca*rby, D = ca + cb;
            edges->r2 = (tx*tx + ty*ty)/(D*D);
        }

        if (edges->r2 < r2best)
            r2best = edges->r2;
        edges++;
    }

    return r2best;
}

static guint
filter_relevant_edges(EdgeQueue *edges, gdouble r2, gdouble eps)
{
    Edge *edge = edges->edges, *enear = edges->edges;
    gdouble limit = sqrt(r2) + 4.0*eps + 0.5;
    guint i;

    limit *= limit;
    for (i = edges->len; i; i--, edge++) {
        if (edge->r2 <= limit) {
            if (edge != enear)
                GWY_SWAP(Edge, *edge, *enear);
            enear++;
        }
    }

    return enear - edges->edges;
}

static void
improve_inscribed_disc(InscribedDisc *disc, EdgeQueue *edges, guint dist)
{
    gdouble eps = 0.5 + 0.25*(dist > 4) + 0.25*(dist > 16), improvement;

    do {
        InscribedDisc best;
        guint i, nr;

        disc->R2 = maximize_disc_radius(disc, edges->edges, edges->len);
        best = *disc;
        nr = filter_relevant_edges(edges, best.R2, eps);

        improvement = 0.0;
        for (i = 0; i < NDIRECTIONS; i++) {
            InscribedDisc cand;
            gdouble sx = eps*shift_directions[2*i],
                    sy = eps*shift_directions[2*i + 1];

            cand.size = disc->size;

            cand.x = disc->x + sx;
            cand.y = disc->y + sy;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x - sy;
            cand.y = disc->y + sx;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x - sx;
            cand.y = disc->y - sy;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;

            cand.x = disc->x + sy;
            cand.y = disc->y - sx;
            if ((cand.R2 = maximize_disc_radius(&cand, edges->edges, nr))
                > best.R2)
                best = cand;
        }

        if (best.R2 > disc->R2) {
            improvement = sqrt(best.R2) - sqrt(disc->R2);
            *disc = best;
        }
        else {
            eps *= 0.5;
        }
    } while (eps > 1e-3 || improvement > 1e-3);
}

static gdouble
grain_volume_laplace(GwyDataField *data_field,
                     const gint *grains,
                     gint gno,
                     const gint *bound)
{
    GwyDataField *grain, *mask, *buffer;
    gint xres, yres, col, row, w, h, i, j, k, ns;
    gdouble v, s, maxerr, error, vol;
    const gdouble *d;
    gdouble *m, *g;

    xres = data_field->xres;
    yres = data_field->yres;

    /* Caulcate extended boundaries */
    w = bound[2];
    col = bound[0];
    if (col > 0) {
        col--;
        w++;
    }
    if (col + w < xres)
        w++;

    h = bound[3];
    row = bound[1];
    if (row > 0) {
        row--;
        h++;
    }
    if (row + h < yres)
        h++;

    /* Create the mask for laplace iteration and calculate a suitable starting
     * value to fill the grain with */
    grain = gwy_data_field_area_extract(data_field, col, row, w, h);
    mask = gwy_data_field_new_alike(grain, TRUE);

    g = grain->data;
    m = mask->data;
    d = data_field->data + row*xres + col;

    s = maxerr = 0.0;
    ns = 0;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            k = (i + row)*xres + j + col;
            if (grains[k] == gno) {
                m[i*w + j] = 1.0;
                if (i > 0 && !grains[k - xres]) {
                    v = g[i*w + j - w];
                    s += v;
                    maxerr += v*v;
                    ns++;
                }
                if (j > 0 && !grains[k - 1]) {
                    v = g[i*w + j - 1];
                    s += v;
                    maxerr += v*v;
                    ns++;
                }
                if (j + 1 < w && !grains[k + 1]) {
                    v = g[i*w + j + 1];
                    s += v;
                    maxerr += v*v;
                    ns++;
                }
                if (i + 1 < h && !grains[k + xres]) {
                    v = g[i*w + j + w];
                    s += v;
                    maxerr += v*v;
                    ns++;
                }
            }
        }
    }
    g_assert(ns > 0);
    s /= ns;
    maxerr = 0.01*sqrt(fabs(maxerr/ns - s*s));

    /* Fill with the starting value */
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            k = (i + row)*xres + j + col;
            if (grains[k] == gno)
                g[i*w + j] = s;
        }
    }

    /* Iterate to get basis (background) */
    if (maxerr) {
        buffer = gwy_data_field_new_alike(grain, FALSE);
        for (i = 0; i < 500; i++) {
            gwy_data_field_correct_laplace_iteration(grain, mask, buffer,
                                                     0.2, &error);
            if (error <= maxerr)
                break;
        }
        g_object_unref(buffer);
    }
    g_object_unref(mask);

    /* Calculate the volume between data and basis */
    vol = 0.0;
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            k = (i + row)*xres + j + col;
            if (grains[k] == gno) {
                gint im, ip, jm, jp;

                im = (i > 0) ? i-1 : i;
                ip = (i < h-1) ? i+1 : i;
                jm = (j > 0) ? j-1 : j;
                jp = (j < w-1) ? j+1 : j;

                vol += 52.0*(d[i*xres + j] - g[i*w + j])
                       + 10.0*(d[im*xres + j] - g[im*w + j]
                               + d[i*xres + jm] - g[i*w + jm]
                               + d[i*xres + jp] - g[i*w + jp]
                               + d[ip*xres + j] - g[ip*w + j])
                       + (d[im*xres + jm] - g[im*w + jm]
                          + d[im*xres + jp] - g[im*w + jp]
                          + d[ip*xres + jm] - g[ip*w + jm]
                          + d[ip*xres + jp] - g[ip*w + jp]);
            }
        }
    }
    g_object_unref(grain);

    return vol;
}

/**
 * gwy_data_field_grains_get_values:
 * @data_field: Data field used for marking.  For some quantities its values
 *              are not used, but its dimensions determine the dimensions of
 *              @grains.
 * @values: Array of size @ngrains+1 to put grain values to.  It can be
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
 * individual grain (0th item of @values which does not correspond to any grain
 * will be overwritten with an arbitrary value and should be ignored).
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
    gdouble *allvalues[1];

    if (!values)
        values = g_new(gdouble, ngrains + 1);

    allvalues[0] = values;
    gwy_data_field_grains_get_quantities(data_field, allvalues,
                                         &quantity, 1, ngrains, grains);
    return values;
}

static gdouble*
ensure_buffer(GwyGrainQuantity quantity,
              gdouble **quantity_data,
              guint ngrains,
              gdouble fillvalue,
              GList **buffers)
{
    gdouble *buf, *b;
    guint gno;

    if (quantity_data[quantity]) {
        buf = quantity_data[quantity];
        if (!fillvalue)
            gwy_clear(buf, ngrains + 1);
    }
    else {
        if (fillvalue)
            buf = g_new(gdouble, ngrains + 1);
        else
            buf = g_new0(gdouble, ngrains + 1);
        *buffers = g_list_prepend(*buffers, buf);
    }
    if (fillvalue) {
        for (gno = ngrains+1, b = buf; gno; gno--)
            *(b++) = fillvalue;
    }

    return buf;
}

/* Note all coordinates are pixel-wise, not real.  For linear and quadratic,
 * the origin is always the grain centre. */
static void
calculate_grain_aux(GwyDataField *data_field,
                    const gint *grains,
                    guint ngrains,
                    gint *sizes, gint *boundpos,
                    gdouble *min, gdouble *max,
                    gdouble *xvalue, gdouble *yvalue, gdouble *zvalue,
                    gdouble *linear, gdouble *quadratic)
{
    guint xres, yres, i, j, k, n, gno, nn;
    gdouble z;
    const gdouble *d;
    const gint *g;
    gdouble *t;

    xres = data_field->xres;
    yres = data_field->yres;
    nn = xres*yres;

    if (sizes) {
        for (k = nn, g = grains; k; k--, g++) {
            gno = *g;
            sizes[gno]++;
        }
    }
    if (boundpos) {
        for (k = 0, g = grains; k < nn; k++, g++) {
            gno = *g;
            if (boundpos[gno] == -1)
                boundpos[gno] = k;
        }
    }
    if (min) {
        for (k = nn, g = grains, d = data_field->data; k; k--, g++, d++) {
            gno = *g;
            z = *d;
            if (z < min[gno])
                min[gno] = z;
        }
    }
    if (max) {
        for (k = nn, g = grains, d = data_field->data; k; k--, g++, d++) {
            gno = *g;
            z = *d;
            if (z > max[gno])
                max[gno] = z;
        }
    }
    if (zvalue) {
        g_assert(sizes);
        for (k = nn, g = grains, d = data_field->data; k; k--, g++, d++) {
            gno = *g;
            z = *d;
            zvalue[gno] += z;
        }
        for (gno = 0; gno <= ngrains; gno++) {
            n = sizes[gno];
            zvalue[gno] /= n;
        }
    }
    if (xvalue) {
        g_assert(sizes);
        g = grains;
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++, g++) {
                gno = *g;
                xvalue[gno] += j;
            }
        }
        for (gno = 0; gno <= ngrains; gno++) {
            n = sizes[gno];
            xvalue[gno] /= n;
        }
    }
    if (yvalue) {
        g_assert(sizes);
        g = grains;
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++, g++) {
                gno = *g;
                yvalue[gno] += i;
            }
        }
        for (gno = 0; gno <= ngrains; gno++) {
            n = sizes[gno];
            yvalue[gno] /= n;
        }
    }
    if (linear) {
        g_assert(xvalue && yvalue);
        g = grains;
        d = data_field->data;
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++, g++, d++) {
                gdouble x, y;

                gno = *g;
                t = linear + 5*gno;
                x = j - xvalue[gno];
                y = i - yvalue[gno];
                z = *d;
                *(t++) += x*x;
                *(t++) += x*y;
                *(t++) += y*y;
                *(t++) += x*z;
                *t += y*z;
            }
        }
    }
    if (quadratic) {
        g_assert(xvalue && yvalue);
        g = grains;
        d = data_field->data;
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++, g++, d++) {
                gdouble x, y, xx, yy, xy;

                gno = *g;
                t = quadratic + 12*gno;
                x = j - xvalue[gno];
                y = i - yvalue[gno];
                xx = x*x;
                xy = x*y;
                yy = y*y;
                z = *d;
                *(t++) += xx*x;
                *(t++) += xx*y;
                *(t++) += x*yy;
                *(t++) += y*yy;
                *(t++) += xx*xx;
                *(t++) += xx*xy;
                *(t++) += xx*yy;
                *(t++) += xy*yy;
                *(t++) += yy*yy;
                *(t++) += xx*z;
                *(t++) += xy*z;
                *t += yy*z;
            }
        }
    }
}

/**
 * gwy_data_field_grains_get_quantities:
 * @data_field: Data field used for marking.  For some quantities its values
 *              are not used, but its dimensions determine the dimensions of
 *              @grains.
 * @values: Array of @nquantities pointers to blocks of length @ngrains+1 to
 *          put the calculated grain values to.  Each block corresponds to one
 *          requested quantity.  %NULL can be passed to allocate and return a
 *          new array.
 * @quantities: Array of @nquantities items that specify the requested
 *              #GwyGrainQuantity to put to corresponding items in @values.
 *              Quantities can repeat.
 * @nquantities: The number of requested different grain values.
 * @grains: Grain numbers filled with gwy_data_field_number_grains().
 * @ngrains: The number of grains as returned by
 *           gwy_data_field_number_grains().
 *
 * Calculates multiple characteristics of grains simultaneously.
 *
 * See gwy_data_field_grains_get_values() for some discussion.  This function
 * is more efficient if several grain quantities need to be calculated since
 * gwy_data_field_grains_get_values() can do lot of repeated work in such case.
 *
 * Returns: @values itself if it was not %NULL, otherwise a newly allocated
 *          array that caller has to free with g_free(), including the
 *          contained arrays.
 *
 * Since: 2.22
 **/
gdouble**
gwy_data_field_grains_get_quantities(GwyDataField *data_field,
                                     gdouble **values,
                                     const GwyGrainQuantity *quantities,
                                     guint nquantities,
                                     guint ngrains,
                                     const gint *grains)
{
    /* The number of built-in quantities. */
    enum { NQ = 41 };
    enum {
        NEED_SIZES = 1 << 0,
        NEED_BOUNDPOS = 1 << 1,
        NEED_MIN = 1 << 2,
        NEED_MAX = 1 << 3,
        NEED_XVALUE = (1 << 4) | NEED_SIZES,
        NEED_YVALUE = (1 << 5) | NEED_SIZES,
        NEED_CENTRE = NEED_XVALUE | NEED_YVALUE,
        NEED_ZVALUE = (1 << 6) | NEED_SIZES,
        NEED_LINEAR = (1 << 7) | NEED_ZVALUE | NEED_CENTRE,
        NEED_QUADRATIC = (1 << 8) | NEED_LINEAR,
        INVALID = G_MAXUINT
    };
    static const guint need_aux[NQ] = {
        NEED_SIZES,                   /* projected area */
        NEED_SIZES,                   /* equiv square side */
        NEED_SIZES,                   /* equiv disc radius */
        0,                            /* surface area */
        NEED_MAX,                     /* maximum */
        NEED_MIN,                     /* minimum */
        NEED_ZVALUE,                  /* mean */
        NEED_SIZES,                   /* median */
        INVALID,
        NEED_MIN | NEED_MAX,          /* half-height area */
        0,                            /* flat boundary length */
        INVALID,
        NEED_BOUNDPOS,                /* min bounding size */
        NEED_BOUNDPOS,                /* min bounding direction */
        NEED_BOUNDPOS,                /* max bounding size */
        NEED_BOUNDPOS,                /* max bounding direction */
        NEED_XVALUE,                  /* centre x */
        NEED_YVALUE,                  /* centre y */
        0,                            /* volume, 0-based */
        NEED_MIN,                     /* volume, min-based */
        0,                            /* volume, Laplace-based */
        INVALID,
        INVALID,
        NEED_LINEAR,                  /* slope theta */
        NEED_LINEAR,                  /* slope phi */
        0,                            /* boundary minimum */
        0,                            /* boundary maximum */
        NEED_QUADRATIC,               /* curvature centre x */
        NEED_QUADRATIC,               /* curvature centre y */
        NEED_QUADRATIC,               /* curvature centre z */
        NEED_QUADRATIC,               /* curvature invrad 1 */
        NEED_QUADRATIC,               /* curvature invrad 2 */
        NEED_QUADRATIC,               /* curvature direction 1 */
        NEED_QUADRATIC,               /* curvature direction 2 */
        NEED_CENTRE,                  /* inscribed disc radius */
        NEED_CENTRE,                  /* inscribed disc centre x */
        NEED_CENTRE,                  /* inscribed disc centre y */
        NEED_BOUNDPOS,                /* convex hull area */
        NEED_BOUNDPOS,                /* circumcircle radius */
        NEED_BOUNDPOS,                /* circumcircle centre x */
        NEED_BOUNDPOS,                /* circumcircle centre y */
    };

    gdouble *quantity_data[NQ];
    gboolean seen[NQ];
    GList *l, *buffers = NULL;
    guint *sizes = NULL;
    gint *boundpos = NULL;
    gdouble *xvalue = NULL, *yvalue = NULL, *zvalue = NULL,
            *min = NULL, *max = NULL,
            *linear = NULL, *quadratic = NULL;
    const gdouble *d;
    gdouble *p;
    gdouble qh, qv, qarea, qdiag, qgeom;
    guint xres, yres, i, j, k, nn, gno;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), NULL);
    g_return_val_if_fail(grains, NULL);
    if (!nquantities)
        return values;
    g_return_val_if_fail(quantities, NULL);

    if (!values) {
        values = g_new(gdouble*, nquantities);
        for (i = 0; i < nquantities; i++)
            values[i] = g_new0(gdouble, ngrains + 1);
    }
    else {
        for (i = 0; i < nquantities; i++)
            gwy_clear(values[i], ngrains + 1);
    }

    xres = data_field->xres;
    yres = data_field->yres;
    nn = xres*yres;
    gwy_debug("ngrains: %d, nn: %d", ngrains, nn);

    /* Figure out which quantities are requested. */
    gwy_clear(quantity_data, NQ);
    for (i = 0; i < nquantities; i++) {
        GwyGrainQuantity quantity = quantities[i];

        if ((guint)quantity >= NQ || need_aux[quantity] == INVALID) {
            g_warning("Invalid built-in grain quantity number %u.", quantity);
            continue;
        }
        /* Take the first if the same quantity is requested multiple times.
         * We will deal with this later. */
        if (!quantity_data[quantity])
            quantity_data[quantity] = values[i];
    }

    /* Figure out the auxiliary data to calculate.  Do this after we gathered
     * all quantities as some auxiliary data are in fact quantities too. */
    for (i = 0; i < nquantities; i++) {
        GwyGrainQuantity quantity = quantities[i];
        guint need;

        if ((guint)quantity >= NQ || need_aux[quantity] == INVALID)
            continue;

        need = need_aux[quantity];
        /* Integer data */
        if ((need & NEED_SIZES) && !sizes) {
            sizes = g_new0(guint, ngrains + 1);
            buffers = g_list_prepend(buffers, sizes);
        }
        if ((need & NEED_BOUNDPOS) && !boundpos) {
            boundpos = g_new(gint, ngrains + 1);
            buffers = g_list_prepend(buffers, boundpos);
            for (gno = 0; gno <= ngrains; gno++)
                boundpos[gno] = -1;
        }
        /* Floating point data that coincide with some quantity.  An array
         * is allocated only if the corresponding quantity is not requested.
         * Otherwise we use the supplied array. */
        if (need & NEED_MIN)
            min = ensure_buffer(GWY_GRAIN_VALUE_MINIMUM, quantity_data,
                                ngrains, G_MAXDOUBLE, &buffers);
        if (need & NEED_MAX)
            max = ensure_buffer(GWY_GRAIN_VALUE_MAXIMUM, quantity_data,
                                ngrains, -G_MAXDOUBLE, &buffers);
        if (need & NEED_XVALUE)
            xvalue = ensure_buffer(GWY_GRAIN_VALUE_CENTER_X, quantity_data,
                                   ngrains, 0.0, &buffers);
        if (need & NEED_YVALUE)
            yvalue = ensure_buffer(GWY_GRAIN_VALUE_CENTER_Y, quantity_data,
                                   ngrains, 0.0, &buffers);
        if (need & NEED_ZVALUE)
            zvalue = ensure_buffer(GWY_GRAIN_VALUE_MEAN, quantity_data,
                                   ngrains, 0.0, &buffers);
        /* Complex floating point data */
        if ((need & NEED_LINEAR) && !linear) {
            linear = g_new0(gdouble, 5*(ngrains + 1));
            buffers = g_list_prepend(buffers, linear);
        }
        if ((need & NEED_QUADRATIC) && !quadratic) {
            quadratic = g_new0(gdouble, 12*(ngrains + 1));
            buffers = g_list_prepend(buffers, quadratic);
        }
    }

    /* Calculate auxiliary quantities (in pixel lateral coordinates) */
    calculate_grain_aux(data_field, grains, ngrains, sizes, boundpos,
                        min, max, xvalue, yvalue, zvalue, linear, quadratic);

    d = data_field->data;
    qh = gwy_data_field_get_xmeasure(data_field);
    qv = gwy_data_field_get_ymeasure(data_field);
    qdiag = hypot(qh, qv);
    qarea = qh*qv;
    qgeom = sqrt(qarea);

    /* Calculate specific requested quantities */
    if ((p = quantity_data[GWY_GRAIN_VALUE_PROJECTED_AREA])) {
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = qarea*sizes[gno];
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE])) {
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = sqrt(qarea*sizes[gno]);
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS])) {
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = sqrt(qarea/G_PI*sizes[gno]);
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_SURFACE_AREA])) {
        gdouble qh2 = qh*qh, qv2 = qv*qv;

        gwy_clear(p, ngrains + 1);
        /* Every contribution is calculated twice -- for each pixel (vertex)
         * participating to a particular triangle */
        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gint ix, ipx, imx, jp, jm;
                gdouble c;

                ix = i*xres;
                if (!(gno = grains[ix + j]))
                    continue;

                imx = (i > 0) ? ix-xres : ix;
                ipx = (i < yres-1) ? ix+xres : ix;
                jm = (j > 0) ? j-1 : j;
                jp = (j < xres-1) ? j+1 : j;

                c = (d[ix + j] + d[ix + jm] + d[imx + jm] + d[imx + j])/2.0;
                p[gno] += square_area2w_1c(d[ix + j], d[ix + jm],
                                           d[imx + j], c, qh2, qv2);

                c = (d[ix + j] + d[ix + jp] + d[imx + jp] + d[imx + j])/2.0;
                p[gno] += square_area2w_1c(d[ix + j], d[ix + jp],
                                           d[imx + j], c, qh2, qv2);

                c = (d[ix + j] + d[ix + jm] + d[ipx + jm] + d[ipx + j])/2.0;
                p[gno] += square_area2w_1c(d[ix + j], d[ix + jm],
                                           d[ipx + j], c, qh2, qv2);

                c = (d[ix + j] + d[ix + jp] + d[ipx + jp] + d[ipx + j])/2.0;
                p[gno] += square_area2w_1c(d[ix + j], d[ix + jp],
                                           d[ipx + j], c, qh2, qv2);
            }
        }
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] *= qarea/8.0;
    }
    /* GWY_GRAIN_VALUE_MINIMUM is calculated directly. */
    /* GWY_GRAIN_VALUE_MAXIMUM is calculated directly. */
    /* GWY_GRAIN_VALUE_MEAN is calculated directly. */
    if ((p = quantity_data[GWY_GRAIN_VALUE_MEDIAN])) {
        guint *csizes = g_new0(guint, ngrains + 1);
        guint *pos = g_new0(guint, ngrains + 1);
        gdouble *tmp;

        /* Find cumulative sizes (we care only about grains, ignore the
         * outside-grains area) */
        csizes[0] = 0;
        csizes[1] = sizes[1];
        for (gno = 2; gno <= ngrains; gno++)
            csizes[gno] = sizes[gno] + csizes[gno-1];

        tmp = g_new(gdouble, csizes[ngrains]);
        /* Find where each grain starts in tmp sorted by grain # */
        for (gno = 1; gno <= ngrains; gno++)
            pos[gno] = csizes[gno-1];
        /* Sort values by grain # to tmp */
        for (k = 0; k < nn; k++) {
            if ((gno = grains[k])) {
                tmp[pos[gno]] = d[k];
                pos[gno]++;
            }
        }
        /* Find medians of each block */
        for (gno = 1; gno <= ngrains; gno++)
            p[gno] = gwy_math_median(csizes[gno] - csizes[gno-1],
                                     tmp + csizes[gno-1]);
        /* Finalize */
        g_free(csizes);
        g_free(pos);
        g_free(tmp);
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_HALF_HEIGHT_AREA])) {
        gdouble *zhalf;
        guint *zhsizes;

        /* Find the grain half-heights, i.e. (z_min + z_max)/2, first */
        zhalf = g_new(gdouble, ngrains + 1);
        for (gno = 0; gno <= ngrains; gno++)
            zhalf[gno] = (min[gno] + max[gno])/2.0;
        /* Calculate the area of pixels above the half-heights */
        zhsizes = g_new0(gint, ngrains + 1);
        for (k = 0; k < nn; k++) {
            gno = grains[k];
            if (d[k] >= zhalf[gno])
                zhsizes[gno]++;
        }
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = qarea*zhsizes[gno];
        /* Finalize */
        g_free(zhalf);
        g_free(zhsizes);
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH])) {
        gwy_clear(p, ngrains + 1);
        /* Note the cycles go to xres and yres inclusive as we calculate the
         * boundary, not pixel interiors. */
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
                    p[g1 | g2 | g3 | g4] += qdiag/2.0;
                }
                else if (g1 && g4) {
                    /* This works for both g1 == g4 and g1 != g4 */
                    p[g1] += qdiag/2.0;
                    p[g4] += qdiag/2.0;
                }
                else if (g2 && g3) {
                    /* This works for both g2 == g3 and g2 != g3 */
                    p[g2] += qdiag/2.0;
                    p[g3] += qdiag/2.0;
                }
                else if (g1 == g2)
                    p[g1 | g3] += qh;
                else if (g1 == g3)
                    p[g1 | g2] += qv;
                else {
                    g_assert_not_reached();
                }
            }
        }
    }
    if (quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MINIMUM]
        || quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM]) {
        gdouble *pmin = quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MINIMUM];
        gdouble *pmax = quantity_data[GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM];

        if (pmin) {
            for (gno = 0; gno <= ngrains; gno++)
                pmin[gno] = G_MAXDOUBLE;
        }
        if (pmax) {
            for (gno = 0; gno <= ngrains; gno++)
                pmax[gno] = -G_MAXDOUBLE;
        }

        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gdouble z;

                /* Processing of the none-grain boundary is waste of time. */
                if (!(gno = grains[i*xres + j]))
                    continue;

                if (i && j && i < yres-1 && j < xres - 1
                    && grains[(i - 1)*xres + j] == gno
                    && grains[i*xres + j - 1] == gno
                    && grains[i*xres + j + 1] == gno
                    && grains[(i + 1)*xres + j] == gno)
                    continue;

                z = d[i*xres + j];
                if (pmin && z < pmin[gno])
                    pmin[gno] = z;
                if (pmax && z > pmax[gno])
                    pmax[gno] = z;
            }
        }
    }
    if (quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE]
        || quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE]
        || quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE]
        || quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE]
        || quantity_data[GWY_GRAIN_VALUE_CONVEX_HULL_AREA]
        || quantity_data[GWY_GRAIN_VALUE_CIRCUMCIRCLE_R]
        || quantity_data[GWY_GRAIN_VALUE_CIRCUMCIRCLE_X]
        || quantity_data[GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y]) {
        gdouble *psmin = quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE];
        gdouble *psmax = quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE];
        gdouble *pamin = quantity_data[GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE];
        gdouble *pamax = quantity_data[GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE];
        gdouble *achull = quantity_data[GWY_GRAIN_VALUE_CONVEX_HULL_AREA];
        gdouble *circcr = quantity_data[GWY_GRAIN_VALUE_CIRCUMCIRCLE_R];
        gdouble *circcx = quantity_data[GWY_GRAIN_VALUE_CIRCUMCIRCLE_X];
        gdouble *circcy = quantity_data[GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y];
        GArray *vertices;

        /* Find the complete convex hulls */
        vertices = g_array_new(FALSE, FALSE, sizeof(GridPoint));
        for (gno = 1; gno <= ngrains; gno++) {
            gdouble dx = qh, dy = qv;

            find_grain_convex_hull(xres, yres, grains, boundpos[gno], vertices);
            if (psmin || pamin) {
                grain_minimum_bound(vertices, qh, qv, &dx, &dy);
                if (psmin)
                    psmin[gno] = hypot(dx, dy);
                if (pamin) {
                    pamin[gno] = atan2(-dy, dx);
                    if (pamin[gno] <= -G_PI/2.0)
                        pamin[gno] += G_PI;
                    else if (pamin[gno] > G_PI/2.0)
                        pamin[gno] -= G_PI;
                }
            }
            if (psmax || pamax) {
                grain_maximum_bound(vertices, qh, qv, &dx, &dy);
                if (psmax)
                    psmax[gno] = hypot(dx, dy);
                if (pamax) {
                    pamax[gno] = atan2(-dy, dx);
                    if (pamax[gno] <= -G_PI/2.0)
                        pamax[gno] += G_PI;
                    else if (pamax[gno] > G_PI/2.0)
                        pamax[gno] -= G_PI;
                }
            }
            if (achull) {
                achull[gno] = grain_convex_hull_area(vertices, qh, qv);
            }
            if (circcr || circcx || circcy) {
                InscribedDisc circle = { 0.0, 0.0, 0.0, 0 };

                grain_convex_hull_centre(vertices, qh, qv,
                                         &circle.x, &circle.y);
                circle.R2 = minimize_circle_radius(&circle, vertices,
                                                   qh, qv);
                improve_circumscribed_circle(&circle, vertices, qh, qv);

                if (circcr)
                    circcr[gno] = sqrt(circle.R2);
                if (circcx)
                    circcx[gno] = circle.x + data_field->xoff;
                if (circcy)
                    circcy[gno] = circle.y + data_field->yoff;
            }
        }
        /* Finalize */
        g_array_free(vertices, TRUE);
    }
    /* XXX: This must go before GWY_GRAIN_VALUE_CENTER_X and
     * GWY_GRAIN_VALUE_CENTER_Y because we want them as pixel quantities. */
    if (quantity_data[GWY_GRAIN_VALUE_INSCRIBED_DISC_R]
        || quantity_data[GWY_GRAIN_VALUE_INSCRIBED_DISC_X]
        || quantity_data[GWY_GRAIN_VALUE_INSCRIBED_DISC_Y]) {
        gdouble *inscdr = quantity_data[GWY_GRAIN_VALUE_INSCRIBED_DISC_R];
        gdouble *inscdx = quantity_data[GWY_GRAIN_VALUE_INSCRIBED_DISC_X];
        gdouble *inscdy = quantity_data[GWY_GRAIN_VALUE_INSCRIBED_DISC_Y];
        gint *bbox;
        guint *grain = NULL;
        guint grainsize = 0;
        PixelQueue *inqueue = g_slice_new0(PixelQueue);
        PixelQueue *outqueue = g_slice_new0(PixelQueue);
        GArray *candidates = g_array_new(FALSE, FALSE, sizeof(InscribedDisc));
        EdgeQueue edges = { 0, 0, NULL };
        InscribedDisc *cand;

        /* FIXME: BBoxes are used for more than one grain quantity, make them
         * a common aux. */
        bbox = gwy_data_field_get_grain_bounding_boxes(data_field,
                                                       ngrains, grains, NULL);

        /*
         * For each grain:
         *    Extract it, find all boundary pixels.
         *    Use (octagnoal) erosion to find disc centre candidate(s).
         *    For each candidate:
         *       Find maximum disc that fits with this centre.
         *       By expanding/moving try to find a larger disc until we cannot
         *       improve it.
         */
        for (gno = 1; gno <= ngrains; gno++) {
            guint width, height, dist;
            gdouble dx, dy, centrex, centrey;
            guint w = bbox[4*gno + 2], h = bbox[4*gno + 3];
            gdouble xoff = qh*bbox[4*gno] + data_field->xoff,
                    yoff = qv*bbox[4*gno + 1] + data_field->yoff;
            guint ncand;

            /* If the grain is rectangular, calculate the disc directly.
             * Large rectangular grains are rare but the point is to catch
             * grains with width of height of 1 here. */
            if (sizes[gno] == w*h) {
                dx = 0.5*w*qh;
                dy = 0.5*h*qv;
                if (inscdr)
                    inscdr[gno] = 0.999*MIN(dx, dy);
                if (inscdx)
                    inscdx[gno] = dx + xoff;
                if (inscdy)
                    inscdy[gno] = dy + yoff;
                continue;
            }

            /* Upsampling twice combined with octagonal erosion has the nice
             * property that we get candidate pixels in places such as corners
             * or junctions of one-pixel thin lines. */
            grain = extract_upsampled_square_pixel_grain(grains, xres, gno,
                                                         bbox + 4*gno,
                                                         grain, &grainsize,
                                                         &width, &height,
                                                         qh, qv);
            /* Size of upsamples pixel in original pixel coordinates.  Normally
             * equal to 1/2 and always approximately 1:1. */
            dx = w*(qh/qgeom)/width;
            dy = h*(qv/qgeom)/height;
            /* Grain centre in squeezed pixel coordinates within the bbox. */
            centrex = (xvalue[gno] + 0.5)*(qh/qgeom);
            centrey = (yvalue[gno] + 0.5)*(qv/qgeom);

            init_erosion(grain, width, height, inqueue);
            dist = 1;
            while (TRUE) {
                if (!erode_8(grain, width, height, dist, inqueue, outqueue))
                    break;
                GWY_SWAP(PixelQueue*, inqueue, outqueue);
                dist++;

                if (!erode_4(grain, width, height, dist, inqueue, outqueue))
                    break;
                GWY_SWAP(PixelQueue*, inqueue, outqueue);
                dist++;
            }
#if 0
            for (i = 0; i < height; i++) {
                for (j = 0; j < width; j++) {
                    if (!grain[i*width + j])
                        g_printerr("..");
                    else
                        g_printerr("%02u", grain[i*width + j]);
                    g_printerr("%c", j == width-1 ? '\n' : ' ');
                }
            }
#endif
            /* Now inqueue is always non-empty and contains max-distance
             * pixels of the upscaled grain. */
            find_disc_centre_candidates(candidates, inqueue, dx, dy,
                                        centrex, centrey);

            /* Try a few first candidates for the inscribed disc centre. */
            ncand = MIN(7, candidates->len);
            for (i = 0; i < ncand; i++) {
                cand = &g_array_index(candidates, InscribedDisc, i);
                find_all_edges(&edges, grains, xres, gno, bbox + 4*gno,
                               qh/qgeom, qv/qgeom);
                improve_inscribed_disc(cand, &edges, dist);
            }

            cand = &g_array_index(candidates, InscribedDisc, 0);
            for (i = 1; i < ncand; i++) {
                if (g_array_index(candidates, InscribedDisc, i).R2 > cand->R2)
                    cand = &g_array_index(candidates, InscribedDisc, i);
            }

            if (inscdr)
                inscdr[gno] = sqrt(cand->R2 * qarea);
            if (inscdx)
                inscdx[gno] = cand->x*qgeom + xoff;
            if (inscdy)
                inscdy[gno] = cand->y*qgeom + yoff;
        }

        g_free(bbox);
        g_free(grain);
        g_free(inqueue->points);
        g_free(outqueue->points);
        g_slice_free(PixelQueue, inqueue);
        g_slice_free(PixelQueue, outqueue);
        g_free(edges.edges);
        g_array_free(candidates, TRUE);
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_CENTER_X])) {
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = qh*(p[gno] + 0.5) + data_field->xoff;
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_CENTER_Y])) {
        for (gno = 0; gno <= ngrains; gno++)
            p[gno] = qv*(p[gno] + 0.5) + data_field->yoff;
    }
    if (quantity_data[GWY_GRAIN_VALUE_VOLUME_0]
        || quantity_data[GWY_GRAIN_VALUE_VOLUME_MIN]) {
        gdouble *pv0 = quantity_data[GWY_GRAIN_VALUE_VOLUME_0];
        gdouble *pvm = quantity_data[GWY_GRAIN_VALUE_VOLUME_MIN];

        if (pv0)
            gwy_clear(pv0, ngrains + 1);
        if (pvm)
            gwy_clear(pvm, ngrains + 1);

        for (i = 0; i < yres; i++) {
            for (j = 0; j < xres; j++) {
                gint ix, ipx, imx, jp, jm;
                gdouble v;

                ix = i*xres;
                if (!(gno = grains[ix + j]))
                    continue;

                imx = (i > 0) ? ix-xres : ix;
                ipx = (i < yres-1) ? ix+xres : ix;
                jm = (j > 0) ? j-1 : j;
                jp = (j < xres-1) ? j+1 : j;

                v = (52.0*d[ix + j] + 10.0*(d[imx + j] + d[ix + jm]
                                            + d[ix + jp] + d[ipx + j])
                     + (d[imx + jm] + d[imx + jp] + d[ipx + jm] + d[ipx + jp]));

                /* We know the basis would appear with total weight -96 so
                 * don't bother subtracting it from individual heights */
                if (pv0)
                    pv0[gno] += v;
                if (pvm)
                    pvm[gno] += v - 96.0*min[gno];
            }
        }
        if (pv0) {
            for (gno = 1; gno <= ngrains; gno++)
                pv0[gno] *= qarea/96.0;
        }
        if (pvm) {
            for (gno = 1; gno <= ngrains; gno++)
                pvm[gno] *= qarea/96.0;
        }
    }
    if ((p = quantity_data[GWY_GRAIN_VALUE_VOLUME_LAPLACE])) {
        gint *bbox;

        gwy_clear(p, ngrains + 1);
        /* Fail gracefully when there is one big `grain' over all data.
         * FIXME: Is this correct?  The grain can touch all sides but still
         * have an exterior. */
        bbox = gwy_data_field_get_grain_bounding_boxes(data_field,
                                                       ngrains, grains, NULL);
        if (ngrains == 1
            && (bbox[4] == 0 && bbox[5] == 0
                && bbox[6] == xres && bbox[7] == yres)) {
            g_warning("Cannot interpolate from exterior of the grain when it "
                      "has no exterior.");
        }
        else {
            for (gno = 1; gno <= ngrains; gno++)
                p[gno] = qarea/96.0*grain_volume_laplace(data_field, grains,
                                                         gno, bbox + 4*gno);
        }
        g_free(bbox);
    }
    if (quantity_data[GWY_GRAIN_VALUE_SLOPE_THETA]
        || quantity_data[GWY_GRAIN_VALUE_SLOPE_PHI]) {
        gdouble *ptheta = quantity_data[GWY_GRAIN_VALUE_SLOPE_THETA];
        gdouble *pphi = quantity_data[GWY_GRAIN_VALUE_SLOPE_PHI];

        for (gno = 1; gno <= ngrains; gno++) {
            gdouble xx, yy, xy, xz, yz, det, bx, by;
            gdouble *lin = linear + 5*gno;

            xx = lin[0];
            xy = lin[1];
            yy = lin[2];
            xz = lin[3];
            yz = lin[4];
            det = xx*yy - xy*xy;
            if (det) {
                bx = (xz*yy - xy*yz)/(qh*det);
                by = (yz*xx - xy*xz)/(qv*det);
                if (ptheta)
                    ptheta[gno] = atan(hypot(bx, by));
                if (pphi)
                    pphi[gno] = atan2(by, -bx);
            }
            else {
                if (ptheta)
                    ptheta[gno] = 0.0;
                if (pphi)
                    pphi[gno] = 0.0;
            }
        }
    }
    if (quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_X]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Y]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Z]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE1]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE2]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE1]
        || quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE2]) {
        gdouble *px = quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_X];
        gdouble *py = quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Y];
        gdouble *pz = quantity_data[GWY_GRAIN_VALUE_CURVATURE_CENTER_Z];
        gdouble *pk1 = quantity_data[GWY_GRAIN_VALUE_CURVATURE1];
        gdouble *pk2 = quantity_data[GWY_GRAIN_VALUE_CURVATURE2];
        gdouble *pa1 = quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE1];
        gdouble *pa2 = quantity_data[GWY_GRAIN_VALUE_CURVATURE_ANGLE2];
        gdouble mx = sqrt(qh/qv), my = sqrt(qv/qh);

        for (gno = 1; gno <= ngrains; gno++) {
            /* a:
             *  0 [<1>
             *  1  <x>   <x²>
             *  3  <y>   <xy>   <y²>
             *  6  <x²>  <x³>   <x²y>  <x⁴>
             * 10  <xy>  <x²y>  <xy²>  <x³y>   <x²y²>
             * 15  <y²>  <xy²>  <y³>   <x²y²>  <xy³>   <y⁴>]
             * b: [<z>  <xz>  <yz>  <x²z>  <xyz>  <y²z>]
             */
            gdouble a[21], b[6];
            gdouble *lin = linear + 5*gno, *quad = quadratic + 12*gno;
            guint n = sizes[gno];

            if (n >= 6) {
                a[0] = n;
                a[1] = a[3] = 0.0;
                a[2] = a[6] = lin[0];
                a[4] = a[10] = lin[1];
                a[5] = a[15] = lin[2];
                a[7] = quad[0];
                a[8] = a[11] = quad[1];
                a[9] = quad[4];
                a[12] = a[16] = quad[2];
                a[13] = quad[5];
                a[14] = a[18] = quad[6];
                a[17] = quad[3];
                a[19] = quad[7];
                a[20] = quad[8];
                if (gwy_math_choleski_decompose(6, a)) {
                    b[0] = n*zvalue[gno];
                    b[1] = lin[3];
                    b[2] = lin[4];
                    b[3] = quad[9];
                    b[4] = quad[10];
                    b[5] = quad[11];
                    gwy_math_choleski_solve(6, a, b);
                    /* Get pixel aspect ratio right while keeping pixel size
                     * around 1. */
                    b[1] /= mx;
                    b[2] /= my;
                    b[3] /= mx*mx;
                    b[5] /= my*my;
                }
                else
                    n = 0;
            }

            /* Recycle a[] for the curvature parameters. */
            if (n >= 6)
                gwy_math_curvature(b, a+0, a+1, a+2, a+3, a+4, a+5, a+6);
            else {
                a[0] = a[1] = a[2] = a[4] = a[5] = 0.0;
                a[3] = G_PI/2.0;
                a[6] = zvalue[gno];
            }
            if (pk1)
                pk1[gno] = a[0]/(qgeom*qgeom);
            if (pk2)
                pk2[gno] = a[1]/(qgeom*qgeom);
            if (pa1)
                pa1[gno] = a[2];
            if (pa2)
                pa2[gno] = a[3];
            if (px)
                px[gno] = qgeom*a[4] + xvalue[gno];
            if (py)
                py[gno] = qgeom*a[5] + yvalue[gno];
            if (pz)
                pz[gno] = a[6];
        }
    }

    /* Copy quantity values to all other instances of the same quantity in
     * @values. */
    gwy_clear(seen, NQ);
    for (i = 0; i < nquantities; i++) {
        GwyGrainQuantity quantity = quantities[i];

        if ((guint)quantity >= NQ || need_aux[quantity] == INVALID)
            continue;

        if (seen[quantity]) {
            memcpy(values[i], quantity_data[quantity],
                   (ngrains + 1)*sizeof(gdouble));
        }
        seen[quantity] = TRUE;
    }

    /* Finalize */
    for (l = buffers; l; l = g_list_next(l))
        g_free(l->data);
    g_list_free(buffers);

    return values;
}

/**
 * gwy_grain_quantity_needs_same_units:
 * @quantity: A grain quantity.
 *
 * Tests whether a grain quantity is defined only when lateral and value
 * units match.
 *
 * Returns: %TRUE if @quantity is meaningless when lateral and value units
 *          differ, %FALSE if it is always defined.
 *
 * Since: 2.7
 **/
gboolean
gwy_grain_quantity_needs_same_units(GwyGrainQuantity quantity)
{
    enum {
        no_same_units = ((ONE << GWY_GRAIN_VALUE_PROJECTED_AREA)
                         | (ONE << GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE)
                         | (ONE << GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS)
                         | (ONE << GWY_GRAIN_VALUE_MAXIMUM)
                         | (ONE << GWY_GRAIN_VALUE_MINIMUM)
                         | (ONE << GWY_GRAIN_VALUE_MEAN)
                         | (ONE << GWY_GRAIN_VALUE_MEDIAN)
                         | (ONE << GWY_GRAIN_VALUE_HALF_HEIGHT_AREA)
                         | (ONE << GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH)
                         | (ONE << GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE)
                         | (ONE << GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE)
                         | (ONE << GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE)
                         | (ONE << GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE)
                         | (ONE << GWY_GRAIN_VALUE_CENTER_X)
                         | (ONE << GWY_GRAIN_VALUE_CENTER_Y)
                         | (ONE << GWY_GRAIN_VALUE_VOLUME_0)
                         | (ONE << GWY_GRAIN_VALUE_VOLUME_MIN)
                         | (ONE << GWY_GRAIN_VALUE_VOLUME_LAPLACE)
                         | (ONE << GWY_GRAIN_VALUE_SLOPE_PHI)
                         | (ONE << GWY_GRAIN_VALUE_CURVATURE_CENTER_X)
                         | (ONE << GWY_GRAIN_VALUE_CURVATURE_CENTER_Y)
                         | (ONE << GWY_GRAIN_VALUE_CURVATURE_CENTER_Z)
                         | (ONE << GWY_GRAIN_VALUE_CURVATURE_ANGLE1)
                         | (ONE << GWY_GRAIN_VALUE_CURVATURE_ANGLE2)
                         | (ONE << GWY_GRAIN_VALUE_INSCRIBED_DISC_R)
                         | (ONE << GWY_GRAIN_VALUE_INSCRIBED_DISC_X)
                         | (ONE << GWY_GRAIN_VALUE_INSCRIBED_DISC_Y)
                         | (ONE << GWY_GRAIN_VALUE_CONVEX_HULL_AREA)
                         | (ONE << GWY_GRAIN_VALUE_CIRCUMCIRCLE_R)
                         | (ONE << GWY_GRAIN_VALUE_CIRCUMCIRCLE_X)
                         | (ONE << GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y)),
        same_units = ((ONE << GWY_GRAIN_VALUE_SLOPE_THETA)
                      | (ONE << GWY_GRAIN_VALUE_SURFACE_AREA)
                      | (ONE << GWY_GRAIN_VALUE_CURVATURE1)
                      | (ONE << GWY_GRAIN_VALUE_CURVATURE2))
    };

    if ((ONE << quantity) & no_same_units)
        return FALSE;
    if ((ONE << quantity) & same_units)
        return TRUE;
    g_return_val_if_reached(FALSE);
}

/**
 * gwy_grain_quantity_get_units:
 * @quantity: A grain quantity.
 * @siunitxy: Lateral SI unit of data.
 * @siunitz: Value SI unit of data.
 * @result: An SI unit to set to the units of @quantity.
 *          It can be %NULL, a new SI unit is created then and returned.
 *
 * Calculates the units of a grain quantity.
 *
 * Returns: When @result is %NULL, a newly creates SI unit that has to be
 *          dereferenced when no longer used later.  Otherwise @result itself
 *          is simply returned, its reference count is NOT increased.
 *
 * Since: 2.7
 **/
GwySIUnit*
gwy_grain_quantity_get_units(GwyGrainQuantity quantity,
                             GwySIUnit *siunitxy,
                             GwySIUnit *siunitz,
                             GwySIUnit *result)
{
    enum {
        coord_units = ((ONE << GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE)
                       | (ONE << GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS)
                       | (ONE << GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH)
                       | (ONE << GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE)
                       | (ONE << GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE)
                       | (ONE << GWY_GRAIN_VALUE_CENTER_X)
                       | (ONE << GWY_GRAIN_VALUE_CENTER_Y)
                       | (ONE << GWY_GRAIN_VALUE_CURVATURE_CENTER_X)
                       | (ONE << GWY_GRAIN_VALUE_CURVATURE_CENTER_Y)
                       | (ONE << GWY_GRAIN_VALUE_INSCRIBED_DISC_R)
                       | (ONE << GWY_GRAIN_VALUE_INSCRIBED_DISC_X)
                       | (ONE << GWY_GRAIN_VALUE_INSCRIBED_DISC_Y)
                       | (ONE << GWY_GRAIN_VALUE_CIRCUMCIRCLE_R)
                       | (ONE << GWY_GRAIN_VALUE_CIRCUMCIRCLE_X)
                       | (ONE << GWY_GRAIN_VALUE_CIRCUMCIRCLE_Y)),
        icoord_units = ((ONE << GWY_GRAIN_VALUE_CURVATURE1)
                       | (ONE << GWY_GRAIN_VALUE_CURVATURE2)),
        value_units = ((ONE << GWY_GRAIN_VALUE_MAXIMUM)
                       | (ONE << GWY_GRAIN_VALUE_MINIMUM)
                       | (ONE << GWY_GRAIN_VALUE_MEAN)
                       | (ONE << GWY_GRAIN_VALUE_MEDIAN)
                       | (ONE << GWY_GRAIN_VALUE_CURVATURE_CENTER_Z)),
        area_units = ((ONE << GWY_GRAIN_VALUE_PROJECTED_AREA)
                      | (ONE << GWY_GRAIN_VALUE_HALF_HEIGHT_AREA)
                      | (ONE << GWY_GRAIN_VALUE_SURFACE_AREA)
                      | (ONE << GWY_GRAIN_VALUE_CONVEX_HULL_AREA)),
        volume_units = ((ONE << GWY_GRAIN_VALUE_VOLUME_0)
                        | (ONE << GWY_GRAIN_VALUE_VOLUME_MIN)
                        | (ONE << GWY_GRAIN_VALUE_VOLUME_LAPLACE)),
        angle_units = ((ONE << GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE)
                       | (ONE << GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE)
                       | (ONE << GWY_GRAIN_VALUE_SLOPE_PHI)
                       | (ONE << GWY_GRAIN_VALUE_SLOPE_THETA)
                       | (ONE << GWY_GRAIN_VALUE_CURVATURE_ANGLE1)
                       | (ONE << GWY_GRAIN_VALUE_CURVATURE_ANGLE2))
    };

    g_return_val_if_fail(GWY_IS_SI_UNIT(siunitxy), result);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunitz), result);

    if ((ONE << quantity) & coord_units)
        return gwy_si_unit_power(siunitxy, 1, result);
    if ((ONE << quantity) & icoord_units)
        return gwy_si_unit_power(siunitxy, -1, result);
    if ((ONE << quantity) & value_units)
        return gwy_si_unit_power(siunitz, 1, result);
    if ((ONE << quantity) & area_units)
        return gwy_si_unit_power(siunitxy, 2, result);
    if ((ONE << quantity) & volume_units)
        return gwy_si_unit_power_multiply(siunitxy, 2, siunitz, 1, result);
    if ((ONE << quantity) & angle_units) {
        if (!result)
            return gwy_si_unit_new(NULL);
        gwy_si_unit_set_from_string(result, NULL);
        return result;
    }

    g_return_val_if_reached(result);
}

/**
 * gwy_data_field_grains_add:
 * @grain_field: Field of marked grains (mask).
 * @add_field: Field of marked grains (mask) to be added.
 *
 * Adds @add_field grains to @grain_field.
 *
 * Note: This function is equivalent to
 * <literal>gwy_data_field_max_of_fields(grain_field, grain_field, add_field);</literal>
 * and it will be probably removed someday.
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
 *
 * Note: This function is equivalent to
 * <literal>gwy_data_field_min_of_fields(grain_field, grain_field, intersect_field);</literal>
 * and it will be probably removed someday.
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
    GwyDataField *mark_dfield;
    gint i;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));

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
 * @bboxes: Array of size at least 4*(@ngrains+1) to fill with grain bounding
 *          boxes (as usual zero does not correspond to any grain, grains
 *          start from 1). The bounding boxes are stored as quadruples of
 *          indices: (xmin, ymin, width, height).  It can be %NULL to allocate
 *          a new array.
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

    g_return_val_if_fail(GWY_IS_DATA_FIELD(mask_field), NULL);
    g_return_val_if_fail(grains, NULL);

    xres = mask_field->xres;
    yres = mask_field->yres;
    if (!bboxes)
        bboxes = g_new(gint, 4*(ngrains + 1));

    for (i = 1; i <= ngrains; i++) {
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

    for (i = 1; i <= ngrains; i++) {
        bboxes[4*i + 2] = bboxes[4*i + 2] + 1 - bboxes[4*i];
        bboxes[4*i + 3] = bboxes[4*i + 3] + 1 - bboxes[4*i + 1];
    }

    return bboxes;
}

/**
 * gwy_data_field_area_grains_tgnd:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to the requested width.
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
 * This function is a simple gwy_data_field_area_grains_tgnd_range() that
 * calculates the distribution in the full range.
 **/
void
gwy_data_field_area_grains_tgnd(GwyDataField *data_field,
                                GwyDataLine *target_line,
                                gint col, gint row,
                                gint width, gint height,
                                gboolean below,
                                gint nstats)
{
    gdouble min, max;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    gwy_data_field_area_get_min_max(data_field, NULL,
                                    col, row, width, height,
                                    &min, &max);
    gwy_data_field_area_grains_tgnd_range(data_field, target_line,
                                          col, row, width, height,
                                          min, max, below, nstats);
}

/**
 * gwy_data_field_area_grains_tgnd_range:
 * @data_field: A data field.
 * @target_line: A data line to store the distribution to.  It will be
 *               resampled to the requested width.
 * @col: Upper-left column coordinate.
 * @row: Upper-left row coordinate.
 * @width: Area width (number of columns).
 * @height: Area height (number of rows).
 * @min: Minimum threshold value.
 * @max: Maximum threshold value.
 * @below: If %TRUE, valleys are marked, otherwise mountains are marked.
 * @nstats: The number of samples to take on the distribution function.  If
 *          nonpositive, a suitable resolution is determined automatically.
 *
 * Calculates threshold grain number distribution in given height range.
 *
 * This is the number of grains for each of @nstats equidistant height
 * threshold levels.  For large @nstats this function is much faster than the
 * equivalent number of gwy_data_field_grains_mark_height() calls.
 **/
void
gwy_data_field_area_grains_tgnd_range(GwyDataField *data_field,
                                      GwyDataLine *target_line,
                                      gint col, gint row,
                                      gint width, gint height,
                                      gdouble min, gdouble max,
                                      gboolean below,
                                      gint nstats)
{
    gint *heights, *hindex, *nh, *grains, *listv, *listh, *m, *mm;
    gdouble q;
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
