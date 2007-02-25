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
#include <libprocess/datafield.h>
#include <libprocess/linestats.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libprocess/interpolation.h>

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
 * @corrfactor: Correction factor within step.
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
                                         gdouble corrfactor,
                                         gdouble *error)
{
    gint xres, yres, i, j;
    const gdouble *mask, *data;
    gdouble *buffer;
    gdouble cor, cf, err;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(mask_field));
    g_return_if_fail(GWY_IS_DATA_FIELD(buffer_field));
    g_return_if_fail(data_field->xres == mask_field->xres
                     && data_field->yres == mask_field->yres);

    xres = data_field->xres;
    yres = data_field->yres;

    /* check buffer field */
    if (buffer_field->xres != xres || buffer_field->yres != yres)
        gwy_data_field_resample(buffer_field, xres, yres,
                                GWY_INTERPOLATION_NONE);

    gwy_data_field_copy(data_field, buffer_field, FALSE);

    data = data_field->data;
    buffer = buffer_field->data;
    mask = mask_field->data;

    /* set boundary condition for masked boundary data */
    for (i = 0; i < xres; i++) {
        if (mask[i] > 0)
            buffer[i] = buffer[i + 2*xres];
        if (mask[i + xres*(yres - 1)] > 0)
            buffer[i + xres*(yres - 1)] = buffer[i + xres*(yres - 3)];
    }
    for (i = 0; i < yres; i++) {
        if (mask[xres*i] > 0)
            buffer[xres*i] = buffer[2 + xres*i];
        if (mask[xres - 1 + xres*i] > 0)
            buffer[xres - 1 + xres*i] = buffer[xres - 3 + xres*i];
    }

    /* iterate */
    err = 0.0;
    cf = corrfactor;
    for (i = 1; i < yres - 1; i++) {
        for (j = 1; j < xres - 1; j++) {
            if (mask[i*xres + j] > 0) {
                cor = cf*((data[(i - 1)*xres + j] + data[(i + 1)*xres + j]
                           - 2*data[i*xres + j])
                          + (data[i*xres + j - 1] + data[i*xres + j + 1]
                             - 2*data[i*xres + j]));

                buffer[i*xres + j] += cor;
                cor = fabs(cor);
                if (cor > err)
                    err = cor;
            }
        }
    }

    gwy_data_field_copy(buffer_field, data_field, FALSE);

    if (error)
        *error = err;
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
 * gwy_data_field_distort:
 * @source: Source data field.
 * @dest: Destination data field.
 * @invtrans: Inverse transform function, that is the transformation from
 *            new coordinates to old coordinates.   It gets
 *            (@j+0.5, @i+0.5), where @i and @j are the new row and column
 *            indices, passed as the input coordinates.  The output coordinates
 *            should follow the same convention.  Unless a special exterior
 *            handling is requires, the transform function does not need to
 *            concern itself with coordinates being outside of the data.
 * @user_data: Pointer passed as @user_data to @invtrans.
 * @interp: Interpolation type to use.
 * @exterior: Exterior pixels handling.
 * @fill_value: The value to use with @GWY_EXTERIOR_FIXED_VALUE.
 *
 * Distorts a data field in the horizontal plane.
 *
 * Note the transform function @invtrans is the inverse transform, in other
 * words it calculates the old coordinates from tne new coordinates (the
 * transform would not be uniquely defined the other way round).
 *
 * Since: 2.5
 **/
void
gwy_data_field_distort(GwyDataField *source,
                       GwyDataField *dest,
                       GwyCoordTransform2DFunc invtrans,
                       gpointer user_data,
                       GwyInterpolationType interp,
                       GwyExteriorType exterior,
                       gdouble fill_value)
{
    GwyDataField *coeffield;
    gdouble *data, *coeff;
    const gdouble *cdata;
    gint xres, yres, newxres, newyres;
    gint newi, newj, oldi, oldj, i, j, ii, jj, suplen, sf, st;
    gdouble x, y, v;
    gboolean vset, warned = FALSE;

    g_return_if_fail(GWY_IS_DATA_FIELD(source));
    g_return_if_fail(GWY_IS_DATA_FIELD(dest));
    g_return_if_fail(invtrans);

    suplen = gwy_interpolation_get_support_size(interp);
    g_return_if_fail(suplen > 0);
    coeff = g_newa(gdouble, suplen*suplen);
    sf = -((suplen - 1)/2);
    st = suplen/2;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    newxres = gwy_data_field_get_xres(dest);
    newyres = gwy_data_field_get_yres(dest);

    if (gwy_interpolation_has_interpolating_basis(interp))
        coeffield = g_object_ref(source);
    else {
        coeffield = gwy_data_field_duplicate(source);
        gwy_interpolation_resolve_coeffs_2d(xres, yres, xres,
                                            gwy_data_field_get_data(coeffield),
                                            interp);
    }

    data = gwy_data_field_get_data(dest);
    cdata = gwy_data_field_get_data_const(coeffield);

    for (newi = 0; newi < newyres; newi++) {
        for (newj = 0; newj < newxres; newj++) {
            invtrans(newj + 0.5, newi + 0.5, &x, &y, user_data);
            vset = FALSE;
            x -= 0.5;
            y -= 0.5;
            if (y > yres || x > xres || y < 0.0 || x < 0.0) {
                switch (exterior) {
                    case GWY_EXTERIOR_BORDER_EXTEND:
                    x = CLAMP(x, 0, xres);
                    y = CLAMP(y, 0, yres);
                    break;

                    case GWY_EXTERIOR_MIRROR_EXTEND:
                    /* Mirror extension is what the interpolation code does
                     * by default */
                    break;

                    case GWY_EXTERIOR_PERIODIC:
                    x = (x > 0) ? fmod(x, xres) : fmod(x, xres) + xres;
                    y = (y > 0) ? fmod(y, yres) : fmod(y, yres) + yres;
                    break;

                    case GWY_EXTERIOR_FIXED_VALUE:
                    v = fill_value;
                    vset = TRUE;
                    break;

                    case GWY_EXTERIOR_UNDEFINED:
                    continue;
                    break;

                    default:
                    if (!warned) {
                        g_warning("Unsupported exterior type, "
                                  "assuming undefined");
                        warned = TRUE;
                    }
                    continue;
                    break;
                }
            }
            if (!vset) {
                oldi = (gint)floor(y);
                y -= oldi;
                oldj = (gint)floor(x);
                x -= oldj;
                for (i = sf; i <= st; i++) {
                    ii = (oldi + i + 2*st*yres) % (2*yres);
                    if (G_UNLIKELY(ii >= yres))
                        ii = 2*yres-1 - ii;
                    for (j = sf; j <= st; j++) {
                        jj = (oldj + j + 2*st*xres) % (2*xres);
                        if (G_UNLIKELY(jj >= xres))
                            jj = 2*xres-1 - jj;
                        coeff[(i - sf)*suplen + j - sf] = cdata[ii*xres + jj];
                    }
                }
                v = gwy_interpolation_interpolate_2d(x, y, suplen, coeff,
                                                     interp);
            }
            data[newj + xres*newi] = v;
        }
    }

    g_object_unref(coeffield);
}

/************************** Documentation ****************************/

/**
 * SECTION:correct
 * @title: correct
 * @short_description: Data correction
 **/

/**
 * GwyCoordTransform2DFunc:
 * @x: Old x coordinate.
 * @y: Old y coordinate.
 * @px: Location to store new x coordinate.
 * @py: Location to store new y coordinate.
 * @user_data: User data passed to the caller function.
 *
 * The type of two-dimensional coordinate transform function.
 *
 * Since: 2.5
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
