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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "datafield.h"
#include "stats.h"
#include "correct.h"

static gdouble      unrotate_refine_correction   (GwyDataLine *derdist,
                                                  guint m,
                                                  gdouble phi);
static void         compute_fourier_coeffs       (gint nder,
                                                  const gdouble *der,
                                                  guint symmetry,
                                                  gdouble *st,
                                                  gdouble *ct);

/**
 * gwy_data_field_correct_laplace_iteration:
 * @data_field: Data field to be corrected.
 * @mask_field: Mask of places to be corrected.
 * @buffer_field: Initialized to same size as mask and data.
 * @error: Maximum change within last step.
 * @corfactor: Correction factor within step.
 *
 * Performs one interation of Laplace data correction.
 *
 * Tries to remove all the points in mask off the data by using
 * iterative method similar to solving heat flux equation.
 *
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
    if (!buffer_field)
        buffer_field = gwy_data_field_new_alike(data_field, TRUE);
    else if (buffer_field->xres != xres || buffer_field->yres != yres) {
        gwy_data_field_resample(buffer_field, xres, yres,
                                GWY_INTERPOLATION_NONE);
    }
    gwy_data_field_area_copy(data_field, buffer_field,
                             0, 0,
                             buffer_field->xres, buffer_field->yres,
                             0, 0);

    /*set boundary condition for masked boundary data */
    for (i = 0; i < xres; i++) {
        if (mask_field->data[i] != 0)
            buffer_field->data[i] = buffer_field->data[i + 2*xres];
        if (mask_field->data[i + xres*(yres - 1)] != 0)
            buffer_field->data[i + xres*(yres - 1)]
                = buffer_field->data[i + xres*(yres - 3)];
    }
    for (i = 0; i < yres; i++) {
        if (mask_field->data[xres*i] != 0)
            buffer_field->data[xres*i] = buffer_field->data[2 + xres*i];
        if (mask_field->data[xres - 1 + xres*i] != 0)
            buffer_field->data[xres - 1 + xres*i]
                = buffer_field->data[xres - 3 + xres*i];
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

    gwy_data_field_area_copy(buffer_field, data_field,
                             0, 0,
                             buffer_field->xres, buffer_field->yres,
                             0, 0);

}

/**
 * gwy_data_field_mask_outliers:
 * @data_field: A data field.
 * @mask_field: A data field to be filled with mask.
 * @thresh: Threshold value.
 *
 * Creates mask of data that are above or below @thresh*sigma from average
 * height.
 *
 * Sigma denotes root-mean square deviation
 * of heights. This criterium corresponds
 * to usual Gaussian distribution outliers detection for @thresh = 3.
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

    gwy_data_field_invalidate(mask_field);
}

/**
 * gwy_data_field_correct_average:
 * @data_field: A data field.
 * @mask_field: Mask of places to be corrected.
 *
 * Fills data under mask with average value.
 *
 * Simply puts average value of all the @data_field values into
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

    gwy_data_field_invalidate(mask_field);
}

/**
 * gwy_data_field_unrotate_find_corrections:
 * @derdist: Angular derivation distribution (normally obrained from
 *           gwy_data_field_slope_distribution()).
 * @correction: Corrections for particular symmetry types will be stored
 *              here (indexed by GwyPlaneSymmetry). @correction[0] contains
 *              the most probable correction.  All angles are in radians.
 *
 * Finds rotation corrections.
 *
 * Rotation correction is computed for for all symmetry types.
 * In addition an estimate is made about the prevalent one.
 *
 * Returns: The estimate type of prevalent symmetry.
 **/
GwyPlaneSymmetry
gwy_data_field_unrotate_find_corrections(GwyDataLine *derdist,
                                         gdouble *correction)
{
    static const guint symm[] = { 2, 3, 4, 6 };
    GwyPlaneSymmetry guess, t;
    gint nder;
    gsize j, m;
    gdouble avg, max, total, phi;
    const gdouble *der;
    gdouble sint[G_N_ELEMENTS(symm)], cost[G_N_ELEMENTS(symm)];

    nder = gwy_data_line_get_res(derdist);
    der = gwy_data_line_get_data_const(derdist);
    avg = gwy_data_line_get_avg(derdist);
    gwy_data_line_add(derdist, -avg);

    guess = GWY_SYMMETRY_AUTO;
    max = -G_MAXDOUBLE;
    for (j = 0; j < G_N_ELEMENTS(symm); j++) {
        m = symm[j];
        compute_fourier_coeffs(nder, der, m, sint+j, cost+j);
        phi = atan2(-sint[j], cost[j]);
        total = sqrt(sint[j]*sint[j] + cost[j]*cost[j]);

        gwy_debug("sc%d = (%f, %f), total%d = (%f, %f)",
                  m, sint[j], cost[j], m, total, 180.0/G_PI*phi);

        phi /= 2*G_PI*m;
        phi = unrotate_refine_correction(derdist, m, phi);
        t = sizeof("Die, die GCC warning!");
        /*
         *             range from             smallest possible
         *  symmetry   compute_correction()   range                ratio
         *    m        -1/2m .. 1/2m
         *
         *    2        -1/4  .. 1/4           -1/8  .. 1/8         1/2
         *    3        -1/6  .. 1/6           -1/12 .. 1/12        1/2
         *    4        -1/8  .. 1/8           -1/8  .. 1/8 (*)     1
         *    6        -1/12 .. 1/12          -1/12 .. 1/12        1
         *
         *  (*) not counting rhombic
         */
        switch (m) {
            case 2:
            t = GWY_SYMMETRY_PARALLEL;
            /* align with any x or y */
            if (phi >= 0.25/m)
                phi -= 0.5/m;
            else if (phi <= -0.25/m)
                phi += 0.5/m;
            correction[t] = phi;
            total /= 1.25;
            break;

            case 3:
            t = GWY_SYMMETRY_TRIANGULAR;
            /* align with any x or y */
            if (phi >= 0.125/m)
                phi -= 0.25/m;
            else if (phi <= -0.125/m)
                phi += 0.25/m;
            correction[t] = phi;
            break;

            case 4:
            t = GWY_SYMMETRY_SQUARE;
            correction[t] = phi;
            /* decide square/rhombic */
            phi += 0.5/m;
            if (phi > 0.5/m)
                phi -= 1.0/m;
            t = GWY_SYMMETRY_RHOMBIC;
            correction[t] = phi;
            if (fabs(phi) > fabs(correction[GWY_SYMMETRY_SQUARE]))
                t = GWY_SYMMETRY_SQUARE;
            total /= 1.4;
            break;

            case 6:
            t = GWY_SYMMETRY_HEXAGONAL;
            correction[t] = phi;
            break;

            default:
            g_assert_not_reached();
            break;
        }

        if (total > max) {
            max = total;
            guess = t;
        }
    }
    gwy_data_line_add(derdist, avg);
    g_assert(guess != GWY_SYMMETRY_AUTO);
    gwy_debug("SELECTED: %d", guess);
    correction[GWY_SYMMETRY_AUTO] = correction[guess];

    for (j = 0; j < GWY_SYMMETRY_LAST; j++) {
        gwy_debug("FINAL %d: (%f, %f)", j, correction[j], 360*correction[j]);
        correction[j] *= 2.0*G_PI;
    }

    return guess;
}

/* FIXME: The reason why this is a separate function is that either there's
 * a devious bug in gwy_data_field_unrotate_find_corrections(), or MSVC
 * mysteriously miscompiles it.  The effect is that bogus numbers appear
 * in `total'.  Either way moving this code into a subroutine hides the
 * problem. */
static void
compute_fourier_coeffs(gint nder, const gdouble *der,
                       guint symmetry,
                       gdouble *st, gdouble *ct)
{
    guint i;
    gdouble q, sint, cost;

    q = 2*G_PI/nder*symmetry;
    sint = cost = 0.0;
    for (i = 0; i < nder; i++) {
        sint += sin(q*(i + 0.5))*der[i];
        cost += cos(q*(i + 0.5))*der[i];
    }

    *st = sint;
    *ct = cost;
}

/**
 * unrotate_refine_correction:
 * @derdist: Angular derivation distribution (as in Slope dist. graph).
 * @m: Symmetry.
 * @phi: Initial correction guess (in the range 0..1!).
 *
 * Compute correction assuming symmetry @m and initial guess @phi.
 *
 * Returns: The correction (again in the range 0..1!).
 **/
static gdouble
unrotate_refine_correction(GwyDataLine *derdist,
                           guint m, gdouble phi)
{
    gdouble sum, wsum;
    const gdouble *der;
    guint i, j, nder;

    nder = gwy_data_line_get_res(derdist);
    der = gwy_data_line_get_data_const(derdist);

    phi -= floor(phi) + 1.0;
    sum = wsum = 0.0;
    for (j = 0; j < m; j++) {
        gdouble low = (j + 5.0/6.0)/m - phi;
        gdouble high = (j + 7.0/6.0)/m - phi;
        gdouble s, w;
        guint ilow, ihigh;

        ilow = (guint)floor(low*nder);
        ihigh = (guint)floor(high*nder);
        gwy_debug("[%u] peak %u low = %f, high = %f, %u, %u",
                  m, j, low, high, ilow, ihigh);
        s = w = 0.0;
        for (i = ilow; i <= ihigh; i++) {
            s += (i + 0.5)*der[i % nder];
            w += der[i % nder];
        }

        s /= nder*w;
        gwy_debug("[%u] peak %u center: %f", m, j, 360*s);
        sum += (s - (gdouble)j/m)*w*w;
        wsum += w*w;
    }
    phi = sum/wsum;
    gwy_debug("[%u] FITTED phi = %f (%f)", m, phi, 360*phi);
    phi = fmod(phi + 1.0, 1.0/m);
    if (phi > 0.5/m)
        phi -= 1.0/m;
    gwy_debug("[%u] MINIMIZED phi = %f (%f)", m, phi, 360*phi);

    return phi;
}

/**
 * GwyPlaneSymmetry:
 * @GWY_SYMMETRY_AUTO: Automatic symmetry selection.
 * @GWY_SYMMETRY_PARALLEL: Parallel symmetry, there is one prevalent direction
 *                         (bilateral).
 * @GWY_SYMMETRY_TRIANGULAR: Triangular symmetry, there are three prevalent
 *                           directions (unilateral) by 120 degrees.
 * @GWY_SYMMETRY_SQUARE: Square symmetry, two prevalent directions (bilateral)
 *                       oriented approximately along image sides.
 * @GWY_SYMMETRY_RHOMBIC: Rhombic symmetry, two prevalent directions
 *                        (bilateral) oriented approximately along diagonals.
 * @GWY_SYMMETRY_HEXAGONAL: Hexagonal symmetry, three prevalent directions
 *                          (bilateral) by 120 degrees.
 * @GWY_SYMMETRY_LAST: The number of symmetries.
 *
 * Plane symmetry types for rotation correction.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
