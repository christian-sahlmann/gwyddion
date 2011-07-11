/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

#include <glib.h>
#include <libgwyddion/gwymath.h>
#include "monte-carlo-unc.h"

/* Extend randomisation boundaries a bit beyond the caller-specified
 * rectangular area. */
static void
extend_area(GwyDataField *field,
            guint *col, guint *row,
            guint *width, guint *height,
            guint ext)
{
    guint xres = field->xres, yres = field->yres, n;

    n = MIN(*col, ext);
    *col -= n;
    *width += n;

    n = MIN(*col + *width + ext, xres);
    *width = n - *col;

    n = MIN(*row, ext);
    *row -= n;
    *height += n;

    n = MIN(*row + *height + ext, yres);
    *height = n - *row;
}

/* XXX: Replace with some actual random field generation according to the
 * uncertainties. */
static void
randomise_field_area(GwyDataField *destination,
                     GwyDataField *source,
                     guint col,
                     guint row,
                     guint width,
                     guint height,
                     GRand *rng,
                     const GwyFieldUncertainties *unc)
{
    gdouble q = 2.0*GWY_SQRT3*unc->sigma;
    const gdouble *src = source->data + row*source->xres + col;
    gdouble *dest = destination->data + row*destination->xres + col;
    guint i, j;

    for (i = 0; i < height; i++) {
        const gdouble *s = src + i*source->xres;
        gdouble *d = dest + i*destination->xres;

        for (j = width; j; j--, s++, d++) {
            *d = *s + q*(g_rand_double(rng) - 1.0);
        }
    }
}

/**
 * _gwy_data_field_unc_scalar:
 * @field: Data field with the original data.
 * @mask: Mask to use, if any.
 * @mode: Masking mode to use.
 * @fpart: Part of the field to process, or %NULL for a full field.
 * @params: Processing parameters passed to @func.
 * @results: Array of @nresults items to store the uncertainties of the
 *           @nresults scalars calculated by @func.
 * @nresults: The number of scalars function @func calculates.
 * @func: Data processing function to run on the data.
 * @unc: Uncertainties.
 *
 * Find the uncertainties of a scalar data field processing function using
 * Monte Carlo.
 **/
void
_gwy_data_field_unc_scalar(GwyDataField *field,
                           GwyDataField *mask,
                           GwyMaskingType mode,
                           const GwyFieldPart *fpart,
                           gpointer params,
                           gdouble *results,
                           guint nresults,
                           GwyDataFieldScalarFunc func,
                           const GwyFieldUncertainties *unc)
{
    guint niter = 10000;
    GwyDataField *workspace;
    gdouble *accum;
    guint width, height, row, col, iter, i;
    GRand *rng;

    /* XXX: This should be moved to some common helper function once it's
     * needed in several places, like is done in libgwy3. */
    if (fpart) {
        col = fpart->col;
        row = fpart->row;
        width = fpart->width;
        height = fpart->height;
    }
    else {
        row = col = 0;
        width = field->xres;
        height = field->yres;
    }
    if (!mask || mode == GWY_MASK_IGNORE) {
        mask = NULL;
        mode = GWY_MASK_IGNORE;
    }
    extend_area(field, &col, &row, &width, &height, 5);

    workspace = gwy_data_field_duplicate(field);
    accum = g_new0(gdouble, 2*nresults);
    rng = g_rand_new();

    for (iter = 0; iter < niter; iter++) {
        randomise_field_area(workspace, field,
                             col, row, width, height, rng, unc);
        func(workspace, mask, mode, fpart, params, results);
        for (i = 0; i < nresults; i++) {
            gdouble r = results[i];

            accum[2*i] += r;
            accum[2*i + 1] += r*r;
        }
    }

    for (i = 0; i < nresults; i++) {
        gdouble s = accum[2*i]/niter, s2 = accum[2*i + 1]/niter;
        results[i] = sqrt(MAX(s2 - s*s, 0.0));
    }

    g_rand_free(rng);
    g_free(accum);
    g_object_unref(workspace);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
