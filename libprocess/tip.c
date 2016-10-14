/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libprocess/grains.h>
#include <libprocess/morph_lib.h>

/* INTERPOLATION: New (not applicable). */

/* Update when new parameter type is added. */
enum {
    NPARAMTYPES = 6
};

typedef struct {
    const gchar *tip_name;
    const gchar *group_name;
    GwyTipModelFunc func;
    GwyTipGuessFunc guess;
    gint nparams;
    const GwyTipParamType *params;
} GwyTipModelPresetReal;

static void
guess_symmetrical(GwyDataField *data,
                  gdouble height,
                  gdouble radius,
                  gdouble angle,
                  gint *xres,
                  gint *yres)
{
    gdouble xreal;
    gint xpix;

    /* radius*cos(angle) is the maximum lateral dimension of the spherical
     * part of the tip – for sane angles it takes little height so we do
     * not take that into accout; height*tan(angle) is the actual width from
     * the sloped part. */
    xreal = height*tan(angle) + radius*cos(angle);
    xpix = gwy_data_field_rtoi(data, xreal);
    xpix = 2*xpix + 1;
    xpix = CLAMP(xpix, 5, 1201);

    *xres = xpix;
    *yres = xpix;
}

static void
pyramid_guess(GwyDataField *data,
              gdouble height,
              gdouble radius,
              gdouble *params,
              gint *xres,
              gint *yres)
{
    guess_symmetrical(data, height, radius, params[1], xres, yres);
}

static void
contact_guess(GwyDataField *data,
              gdouble height,
              gdouble radius,
              G_GNUC_UNUSED gdouble *params,
              gint *xres,
              gint *yres)
{
    guess_symmetrical(data, height, radius, atan(sqrt(2)), xres, yres);
}

static void
noncontact_guess(GwyDataField *data,
                 gdouble height,
                 gdouble radius,
                 G_GNUC_UNUSED gdouble *params,
                 gint *xres,
                 gint *yres)
{
    guess_symmetrical(data, height, radius, 70.0*G_PI/180.0, xres, yres);
}

static void
parabola_guess(GwyDataField *data,
               gdouble height,
               gdouble radius,
               G_GNUC_UNUSED gdouble *params,
               gint *xres,
               gint *yres)
{
    gdouble xreal = sqrt(2*height*radius);
    gint xpix = gwy_data_field_rtoi(data, xreal);

    xpix = 2*xpix + 1;
    xpix = CLAMP(xpix, 5, 1201);

    *xres = xpix;
    *yres = xpix;
}

static void
ell_parabola_guess(GwyDataField *data,
                   gdouble height,
                   gdouble radius,
                   gdouble *params,
                   gint *xres,
                   gint *yres)
{
    gdouble r1 = radius*sqrt(params[2]), r2 = radius/sqrt(params[2]);
    parabola_guess(data, height, fmax(r1, r2), params, xres, yres);
}

static void
cone_guess(GwyDataField *data,
           gdouble height,
           gdouble radius,
           gdouble *params,
           gint *xres,
           gint *yres)
{
    guess_symmetrical(data, height, radius, params[1], xres, yres);
}

static void
delta_guess(G_GNUC_UNUSED GwyDataField *data,
            G_GNUC_UNUSED gdouble height,
            G_GNUC_UNUSED gdouble radius,
            G_GNUC_UNUSED gdouble *params,
            gint *xres, gint *yres)
{
    *xres = 21;
    *yres = 21;
}

static void
create_pyramid(GwyDataField *tip, gdouble alpha, gint n, gdouble theta)
{
    gint col, row;
    gdouble rcol, rrow;
    gdouble scol, srow;
    gdouble ccol, crow;
    gdouble phi, phic;
    gdouble vm, radius;
    gdouble height;
    gdouble nangle;
    gdouble ca, sa, ir;
    gdouble add;

    if (n == 3)
        add = G_PI/6;
    else
        add = G_PI/4;

    add += theta;
    radius = sqrt((tip->xres/2)*(tip->xres/2)+(tip->yres/2)*(tip->yres/2));
    nangle = G_PI/n;
    height = gwy_data_field_itor(tip, radius)*cos(nangle)/tan(alpha);

    scol = tip->xres/2;
    srow = tip->yres/2;

    ca = cos(add);
    sa = sin(add);
    ir = 1.0/(radius*cos(G_PI/n));
    for (col = 0; col < tip->xres; col++) {
        for (row = 0; row < tip->yres; row++) {
            ccol = col - scol;
            crow = row - srow;
            rcol = -ccol*ca + crow*sa;
            rrow = ccol*sa + crow*ca;
            phi = atan2(rrow, rcol) + G_PI;
            phic = floor(phi/(2*G_PI/n))*2*G_PI/n + G_PI/n;
            vm = rcol*cos(phic) + rrow*sin(phic);
            tip->data[col + tip->xres*row] = height*(1 + vm*ir);
        }
    }

    gwy_data_field_invalidate(tip);
}

static void
round_pyramid(GwyDataField *tip, gdouble angle, gint n, gdouble ballradius)
{
    gdouble center_x, center_y, center_z;
    gdouble height = gwy_data_field_get_max(tip);
    gint col, row;
    gdouble dcol, drow;
    gdouble sphere, radius;
    gdouble beta, zd;

    radius = sqrt((tip->xres/2)*(tip->xres/2) + (tip->yres/2)*(tip->yres/2));
    beta = atan(gwy_data_field_itor(tip, radius)/height);
    center_x = tip->xreal/2;
    center_y = tip->yreal/2;
    beta = atan(tan(angle)/cos(G_PI/n));
    center_z = height - ballradius/sin(beta);
    gwy_debug("z:%g, height=%g, ballradius=%g, cosbeta=%g, beta=%g "
              "(%g deg of %g deg)\n",
              center_z, height, ballradius, cos(beta), beta,
              beta*180/G_PI, angle*180/G_PI);
    for (row = 0; row < tip->yres; row++) {
        gdouble *datarow = tip->data + tip->xres*row;

        for (col = 0; col < tip->xres; col++) {
            if (datarow[col] > (center_z + ballradius*sin(beta))) {
                dcol = gwy_data_field_itor(tip, col) - center_x;
                drow = gwy_data_field_jtor(tip, row) - center_y;
                sphere = (ballradius*ballradius - dcol*dcol - drow*drow);
                zd = G_LIKELY(sphere >= 0) ? sqrt(sphere) : 0.0;
                datarow[col] = MIN(datarow[col], center_z + zd);
            }
        }
    }

    gwy_data_field_invalidate(tip);
}

/* params[0]...number of sides, params[1]...slope angle */
static void
pyramid(GwyDataField *tip,
        G_GNUC_UNUSED gdouble height,
        gdouble radius,
        gdouble rotation,
        gdouble *params)
{
    create_pyramid(tip, params[1], params[0], rotation);
    round_pyramid(tip, params[1], params[0], radius);
}

static void
contact(GwyDataField *tip,
        G_GNUC_UNUSED gdouble height,
        gdouble radius,
        gdouble rotation,
        G_GNUC_UNUSED gdouble *params)
{
    gdouble angle = G_PI/2 - atan(sqrt(2));
    create_pyramid(tip, angle, 4, rotation);
    round_pyramid(tip, angle, 4, radius);
}

static void
noncontact(GwyDataField *tip,
           G_GNUC_UNUSED gdouble height,
           gdouble radius,
           gdouble rotation,
           G_GNUC_UNUSED gdouble *params)
{
    gdouble angle = G_PI/2 - atan(sqrt(2));
    create_pyramid(tip, angle, 3, rotation);
    round_pyramid(tip, angle, 3, radius);
}

static void
parabola(GwyDataField *tip,
         G_GNUC_UNUSED gdouble height,
         gdouble radius,
         G_GNUC_UNUSED gdouble rotation,
         G_GNUC_UNUSED gdouble *params)
{
    gdouble a = 0.5/radius;
    gint col, row;
    gdouble scol, srow;
    gdouble x, y, r2, z0;

    scol = tip->xres/2;
    srow = tip->yres/2;
    x = gwy_data_field_jtor(tip, scol);
    z0 = 2.0*a*x*x;

    for (row = 0; row < tip->yres; row++) {
        y = gwy_data_field_jtor(tip, row - srow);
        for (col = 0; col < tip->xres; col++) {
            x = gwy_data_field_itor(tip, col - scol);
            r2 = x*x + y*y;
            tip->data[col + tip->xres*row] = z0 - a*r2;
        }
    }

    gwy_data_field_invalidate(tip);
}

/* params[2]...anisotropy */
static void
ell_parabola(GwyDataField *tip,
             G_GNUC_UNUSED gdouble height,
             gdouble radius,
             gdouble rotation,
             gdouble *params)
{
    gdouble a1 = 0.5/radius/sqrt(params[2]), a2 = 0.5/radius*sqrt(params[2]);
    gint col, row;
    gdouble scol, srow;
    gdouble x, y, xx, yy, ca = cos(rotation), sa = sin(rotation);

    scol = tip->xres/2;
    srow = tip->yres/2;
    x = gwy_data_field_jtor(tip, scol);

    for (row = 0; row < tip->yres; row++) {
        y = gwy_data_field_jtor(tip, row - srow);
        for (col = 0; col < tip->xres; col++) {
            x = gwy_data_field_itor(tip, col - scol);
            xx = x*ca - y*sa;
            yy = x*sa + y*ca;
            tip->data[col + tip->xres*row] = -(a1*xx*xx + a2*yy*yy);
        }
    }

    gwy_data_field_invalidate(tip);
    gwy_data_field_add(tip, -gwy_data_field_get_min(tip));
}

/* params[1]...slope angle */
static void
cone(GwyDataField *tip,
     G_GNUC_UNUSED gdouble height,
     gdouble radius,
     G_GNUC_UNUSED gdouble rotation,
     gdouble *params)
{
    gdouble angle = params[1];
    gint col, row;
    gdouble scol, srow;
    gdouble x, y, br2, r2, z0, ta;

    scol = tip->xres/2;
    srow = tip->yres/2;

    z0 = radius/sin(angle);
    br2 = radius*cos(angle);
    br2 *= br2;
    ta = 1.0/tan(angle);

    for (row = 0; row < tip->yres; row++) {
        y = gwy_data_field_jtor(tip, row - srow);
        for (col = 0; col < tip->xres; col++) {
            x = gwy_data_field_itor(tip, col - scol);
            r2 = x*x + y*y;
            if (r2 < br2)
                tip->data[col + tip->xres*row] = sqrt(radius*radius - r2);
            else
                tip->data[col + tip->xres*row] = z0 - ta*sqrt(r2);
        }
    }

    gwy_data_field_invalidate(tip);
    gwy_data_field_add(tip, -gwy_data_field_get_min(tip));
}

static void
delta(GwyDataField *tip, gdouble height,
      G_GNUC_UNUSED gdouble radius,
      G_GNUC_UNUSED gdouble rotation,
      G_GNUC_UNUSED gdouble *params)
{
    gwy_data_field_clear(tip);
    tip->data[tip->xres/2 + tip->xres*(tip->yres/2)] = height;
    gwy_data_field_invalidate(tip);
}

static const GwyTipParamType pyramid_params[] = {
    GWY_TIP_PARAM_RADIUS,
    GWY_TIP_PARAM_ROTATION,
    GWY_TIP_PARAM_NSIDES,
    GWY_TIP_PARAM_SLOPE,
};

static const GwyTipParamType contact_params[] = {
    GWY_TIP_PARAM_RADIUS,
    GWY_TIP_PARAM_ROTATION,
};

static const GwyTipParamType noncontact_params[] = {
    GWY_TIP_PARAM_RADIUS,
    GWY_TIP_PARAM_ROTATION,
};

static const GwyTipParamType delta_params[] = {
    GWY_TIP_PARAM_HEIGHT,
};

static const GwyTipParamType parabola_params[] = {
    GWY_TIP_PARAM_RADIUS,
};

static const GwyTipParamType cone_params[] = {
    GWY_TIP_PARAM_RADIUS,
    GWY_TIP_PARAM_SLOPE,
};

static const GwyTipParamType ell_parabola_params[] = {
    GWY_TIP_PARAM_RADIUS,
    GWY_TIP_PARAM_ROTATION,
    GWY_TIP_PARAM_ANISOTROPY,
};

/* Must match the GwyTipTyp enum! */
static const GwyTipModelPresetReal tip_presets[] = {
    {
        N_("Pyramid"),
        N_("Pyramidal"),
        &pyramid, &pyramid_guess,
        G_N_ELEMENTS(pyramid_params), pyramid_params,
    },
    {
        N_("Contact"),
        N_("Pyramidal"),
        &contact, &contact_guess,
        G_N_ELEMENTS(contact_params), contact_params,
    },
    {
        N_("Noncontact"),
        N_("Pyramidal"),
        &noncontact, &noncontact_guess,
        G_N_ELEMENTS(noncontact_params), noncontact_params,
    },
    {
        N_("Delta function"),
        N_("Analytical"),
        &delta, &delta_guess,
        G_N_ELEMENTS(delta_params), delta_params,
    },
    {
        N_("Parabola"),
        N_("Symmetric"),
        &parabola, &parabola_guess,
        G_N_ELEMENTS(parabola_params), parabola_params,
    },
    {
        N_("Cone"),
        N_("Symmetric"),
        &cone, &cone_guess,
        G_N_ELEMENTS(cone_params), cone_params,
    },
    {
        N_("Ellptical parabola"),
        N_("Asymmetric"),
        &ell_parabola, &ell_parabola_guess,
        G_N_ELEMENTS(ell_parabola_params), ell_parabola_params,
    },
};

/**
 * gwy_tip_model_get_npresets:
 *
 * Find number of actual tip model presets.
 *
 * Returns: Number of presets.
 **/
gint
gwy_tip_model_get_npresets(void)
{
    return (gint)G_N_ELEMENTS(tip_presets);
}

/**
 * gwy_tip_model_get_preset:
 * @preset_id: Preset identifier.
 *
 * Get data related to tip preset.
 *
 * Returns: Chosen preset data.
 **/
const GwyTipModelPreset*
gwy_tip_model_get_preset(gint preset_id)
{
    g_return_val_if_fail(preset_id >= 0
                         && preset_id < (gint)G_N_ELEMENTS(tip_presets),
                         NULL);

    return (const GwyTipModelPreset*)(tip_presets + preset_id);
}

/**
 * gwy_tip_model_get_preset_by_name:
 * @name: Name of tip (e. g. "contact").
 *
 * Get data related to preset with specified name.
 *
 * Returns: Chosen preset data.
 **/
const GwyTipModelPreset*
gwy_tip_model_get_preset_by_name(const gchar *name)
{
    guint i;

    g_return_val_if_fail(name, NULL);
    for (i = 0; i < G_N_ELEMENTS(tip_presets); i++) {
        if (gwy_strequal(name, tip_presets[i].tip_name))
            return (GwyTipModelPreset*)(tip_presets + i);
    }

    return NULL;
}

/**
 * gwy_tip_model_get_preset_id:
 * @preset: Tip model preset.
 *
 * Get preset identifier within all presets.
 *
 * Returns: Preset id.
 **/
gint
gwy_tip_model_get_preset_id(const GwyTipModelPreset* preset)
{
    g_return_val_if_fail(preset, 0);
    return (const GwyTipModelPresetReal*)preset - tip_presets;
}

/**
 * gwy_tip_model_get_preset_tip_name:
 * @preset: Tip model preset.
 *
 * Get name of the preset (e. g. "contact").
 *
 * Returns: Preset name.
 **/
const gchar*
gwy_tip_model_get_preset_tip_name(const GwyTipModelPreset* preset)
{
    g_return_val_if_fail(preset, NULL);
    return preset->tip_name;
}

/**
 * gwy_tip_model_get_preset_group_name:
 * @preset: Tip model preset.
 *
 * Get group name of preset (e. g. "analytical".)
 *
 * Returns: Preset group name.
 **/
const gchar*
gwy_tip_model_get_preset_group_name(const GwyTipModelPreset* preset)
{
    g_return_val_if_fail(preset, NULL);
    return preset->group_name;
}

/**
 * gwy_tip_model_get_preset_nparams:
 * @preset: Tip model preset.
 *
 * Get number of tip preset parameters.
 *
 * <warning>In versions prior to 2.47 the function alwas returned zero and thus
 * was useless.  You had to know the what parameters each model had and always
 * pass a maximum-nparams-sized array with only the interesting elements
 * containing your parameters.  Since version 2.47 it returns the true number
 * of parameters used in functions such as gwy_tip_model_preset_create() and
 * gwy_tip_model_preset_create_for_zrange().  It does not return the number of
 * parameters the old functions take.  They behave exactly as before.</warning>
 *
 * Returns: Number of parameters.
 **/
gint
gwy_tip_model_get_preset_nparams(const GwyTipModelPreset* preset)
{
    g_return_val_if_fail(preset, 0);
    return preset->nparams;
}

/**
 * gwy_tip_model_get_preset_params:
 * @preset: Tip model preset.
 *
 * Gets the list of parameters of a tip model preset.
 *
 * All tip models have parameters from a predefined set given by the
 * #GwyTipParamType enum.
 *
 * Note further items may be in principle added to the set in the future so
 * you may want to avoid tip models that have parameters with an unknown
 * (higher than known) id.
 *
 * Returns: List of all tip model parameter ids in ascending order.  The array
 *          is owned by the library and must not be modified nor freed.
 *
 * Since: 2.47
 **/
const GwyTipParamType*
gwy_tip_model_get_preset_params(const GwyTipModelPreset* preset)
{
    g_return_val_if_fail(preset, NULL);
    return ((const GwyTipModelPresetReal*)preset)->params;
}

/* Take the real parameters the function actually has and fill the full-sized
 * arrays (including ignored parameters) the functions want to take. */
static void
params_to_old_params(const GwyTipModelPreset *preset,
                     const gdouble *params,
                     gdouble *height, gdouble *radius, gdouble *rotation,
                     gdouble *oldparams)
{
    const GwyTipModelPresetReal *rpreset = (const GwyTipModelPresetReal*)preset;
    guint i;

    *height = *radius = *rotation = 0.0;
    gwy_clear(oldparams, NPARAMTYPES-3);

    for (i = 0; i < rpreset->nparams; i++) {
        GwyTipParamType paramid = rpreset->params[i];

        if (paramid == GWY_TIP_PARAM_HEIGHT)
            *height = params[i];
        else if (paramid == GWY_TIP_PARAM_RADIUS)
            *radius = params[i];
        else if (paramid == GWY_TIP_PARAM_ROTATION)
            *rotation = params[i];
        else {
            g_assert((guint)paramid < NPARAMTYPES);
            oldparams[paramid-3] = params[i];
        }
    }
}

/**
 * gwy_tip_model_preset_create:
 * @preset: Tip model preset.
 * @tip: Data field to fill with the tip model.
 * @params: Parameters of the tip model.
 *
 * Fills a data field with a preset tip model.
 *
 * Both pixel and physical dimensions of the @tip data field are preserved by
 * this function.  Ensure that before using this function the @tip data field
 * has the same pixels as target data field you want to use the tip model with.
 *
 * The number of parameters is the true full number of parameters as reported
 * by gwy_tip_model_get_preset_nparams() and gwy_tip_model_get_preset_params().
 * And only those parameters are passed in @params.
 *
 * Since: 2.47
 **/
void
gwy_tip_model_preset_create(const GwyTipModelPreset* preset,
                            GwyDataField *tip,
                            const gdouble *params)
{
    gdouble old_params[NPARAMTYPES - 3];
    gdouble height, radius, rotation;

    g_return_if_fail(GWY_IS_DATA_FIELD(tip));
    g_return_if_fail(preset);

    params_to_old_params(preset, params,
                         &height, &radius, &rotation, old_params);
    preset->func(tip, height, radius, rotation, old_params);
}

static gboolean
check_tip_zrange(GwyDataField *tip, gdouble zrange,
                 gdouble *zrange_leftright, gdouble *zrange_topbottom,
                 guint *can_shrink_width, guint *can_shrink_height)
{
    GwyDataField *mask;
    gdouble max = gwy_data_field_get_max(tip);
    guint xres = tip->xres, yres = tip->yres, left, right, up, down;
    gdouble hmax, vmax;

    vmax = fmax(gwy_data_field_area_get_max(tip, NULL, 0, 0, xres, 1),
                gwy_data_field_area_get_max(tip, NULL, 0, yres-1, xres, 1));
    *zrange_topbottom = max - vmax;
    hmax = fmax(gwy_data_field_area_get_max(tip, NULL, 0, 0, 1, yres),
                gwy_data_field_area_get_max(tip, NULL, xres-1, 0, 1, yres));
    *zrange_leftright = max - hmax;
    *can_shrink_height = *can_shrink_width = 0;

    if (!(fmax(*zrange_topbottom, *zrange_leftright) >= zrange))
        return FALSE;

    mask = gwy_data_field_duplicate(tip);
    gwy_data_field_threshold(mask, max - zrange, 0.0, 1.0);
    if (gwy_data_field_grains_autocrop(mask, TRUE, &left, &right, &up, &down)) {
        /* There were some empty rows, i.e. rows with too small values. */
        *can_shrink_width = left;
        *can_shrink_height = up;
    }
    g_object_unref(mask);

    return TRUE;
}

/**
 * gwy_tip_model_preset_create_for_zrange:
 * @preset: Tip model preset.
 * @tip: Data field to fill with the tip model.
 * @zrange: Range of height values in the data determining the required height
 *          of the tip model.
 * @square: %TRUE to enforce a square data field (with @xres and @yres equal).
 * @params: Parameters of the tip model.
 *
 * Fills a data field with a preset tip model, resizing it to make it suitable
 * for the given value range.
 *
 * The dimensions of a pixel in @tip is preserved by this function.  Ensure
 * that before using this function the @tip data field has the same pixels as
 * target data field you want to use the tip model with.
 *
 * However, its dimensions will generally be changed to ensure it is optimal
 * for @zrange.  This means it is guaranteed the height difference between the
 * apex and any border pixel in @tip is at least @zrange, while simultaneously
 * the smallest such difference is not much larger than @zrange.
 *
 * The number of parameters is the true full number of parameters as reported
 * by gwy_tip_model_get_preset_nparams() and gwy_tip_model_get_preset_params().
 * And only those parameters are passed in @params.
 *
 * Since: 2.47
 **/
void
gwy_tip_model_preset_create_for_zrange(const GwyTipModelPreset* preset,
                                       GwyDataField *tip,
                                       gdouble zrange,
                                       gboolean square,
                                       const gdouble *params)
{
    gdouble old_params[NPARAMTYPES - 3];
    gdouble height, radius, rotation;
    gdouble dx, dy, zrange_lr, zrange_tb;
    guint xres_good, yres_good, redw, redh, iter;
    gboolean is_delta;
    gint xres, yres;

    g_return_if_fail(GWY_IS_DATA_FIELD(tip));
    g_return_if_fail(preset);

    is_delta = (gwy_tip_model_get_preset_id(preset) == GWY_TIP_DELTA);
    dx = gwy_data_field_get_xmeasure(tip);
    dy = gwy_data_field_get_ymeasure(tip);

    /* Create tip according to the guess function.  The guess function only
     * uses the dx and dy from the data field so we pass tip itself to it. */
    params_to_old_params(preset, params,
                         &height, &radius, &rotation, old_params);
    if (is_delta)
        xres = yres = 3;
    else
        preset->guess(tip, zrange, radius, old_params, &xres, &yres);
    if (square)
        xres = yres = MAX(xres, yres);

    gwy_data_field_resample(tip, xres, yres, GWY_INTERPOLATION_NONE);
    tip->xreal = xres*dx;
    tip->yreal = yres*dy;
    preset->func(tip, height, radius, rotation, old_params);
    if (is_delta)
        return;

    /* Enlarge the tip while it is too small. */
    iter = 0;
    while (!check_tip_zrange(tip, 1.1*zrange,
                             &zrange_lr, &zrange_tb, &redw, &redh)) {
        if (zrange_tb <= 1.1*zrange)
            yres = (4*yres/3 + 1) | 1;
        if (zrange_lr <= 1.1*zrange)
            xres = (4*xres/3 + 1) | 1;
        if (square)
            xres = yres = MAX(xres, yres);

        gwy_data_field_resample(tip, xres, yres, GWY_INTERPOLATION_NONE);
        tip->xreal = xres*dx;
        tip->yreal = yres*dy;
        preset->func(tip, height, radius, rotation, old_params);

        /* This means about 10 times larger tip than estimated. */
        if (iter++ == 8) {
            g_warning("Failed to guarantee zrange by enlagring tip model.");
            break;
        }
    }
    xres_good = xres;
    yres_good = yres;

    if (square)
        redw = redh = MIN(redw, redh);
    if (redw)
        redw--;
    if (redh)
        redh--;

    if (!MAX(redw, redh))
        return;

    /* The tip seems too large so try cropping it a bit. */
    xres -= 2*redw;
    yres -= 2*redh;
    gwy_data_field_resample(tip, xres, yres, GWY_INTERPOLATION_NONE);
    tip->xreal = xres*dx;
    tip->yreal = yres*dy;
    preset->func(tip, height, radius, rotation, old_params);
    if (check_tip_zrange(tip, 1.01*zrange,
                         &zrange_lr, &zrange_tb, &redw, &redh))
        return;

    /* It fails the check after size reduction so try two things: first adding
     * just one pixel to each side, then just reverting to the last know good
     * tip. */
    xres += 2;
    yres += 2;
    gwy_data_field_resample(tip, xres, yres, GWY_INTERPOLATION_NONE);
    tip->xreal = xres*dx;
    tip->yreal = yres*dy;
    preset->func(tip, height, radius, rotation, old_params);
    if (check_tip_zrange(tip, 1.01*zrange,
                         &zrange_lr, &zrange_tb, &redw, &redh))
        return;

    xres = xres_good;
    yres = yres_good;
    gwy_data_field_resample(tip, xres, yres, GWY_INTERPOLATION_NONE);
    tip->xreal = xres*dx;
    tip->yreal = yres*dy;
    preset->func(tip, height, radius, rotation, old_params);
}

static gint**
i_datafield_to_field(GwyDataField *datafield,
                     gboolean maxzero,
                     gdouble min,
                     gdouble step)
{
    gint **ret;
    gint col, row, xres;
    gdouble *data;
    gdouble max;

    max = maxzero ? gwy_data_field_get_max(datafield) : 0.0;

    xres = datafield->xres;
    data = datafield->data;
    ret = _gwy_morph_lib_iallocmatrix(datafield->yres, xres);
    for (row = 0; row < datafield->yres; row++) {
        for (col = 0; col < xres; col++)
            ret[row][col] = (gint)(((data[col + xres*row] - max) - min)/step);
    }

    return ret;
}

static GwyDataField*
i_field_to_datafield(gint **field,
                     GwyDataField *ret,
                     gdouble min,
                     gdouble step)
{
    gint col, row;

    for (row = 0; row < ret->yres; row++) {
        for (col = 0; col < ret->xres; col++) {
            ret->data[col + ret->xres*row] = (gdouble)field[row][col]*step
                                             + min;
        }
    }

    gwy_data_field_invalidate(ret);
    return ret;
}

static gint **
i_datafield_to_largefield(GwyDataField *datafield,
                          GwyDataField *tipfield,
                          gdouble min,
                          gdouble step)
{
    gint **ret;
    gint col, row;
    gint xres, yres, xnew, ynew;
    gint txr2, tyr2;
    gint minimum;
    gdouble *data;

    minimum = (gint)((gwy_data_field_get_min(datafield) - min)/step);
    xres = datafield->xres;
    yres = datafield->yres;
    xnew = xres + tipfield->xres;
    ynew = yres + tipfield->yres;
    txr2 = tipfield->xres/2;
    tyr2 = tipfield->yres/2;

    data = datafield->data;
    ret = _gwy_morph_lib_iallocmatrix(ynew, xnew);
    for (row = 0; row < ynew; row++) {
        for (col = 0; col < xnew; col++) {
            if (col >= txr2
                && col < xres + txr2
                && row >= tyr2
                && row < yres + tyr2)
                ret[row][col] = (gint)(((data[col - txr2 + xres*(row - tyr2)])
                                        - min) /step);
            else
                ret[row][col] = minimum;
        }
    }

    return ret;
}

static GwyDataField*
i_largefield_to_datafield(gint **field,
                          GwyDataField *ret,
                          GwyDataField *tipfield,
                          gdouble min,
                          gdouble step)
{
    gint col, row;
    gint xnew, ynew;
    gint txr2, tyr2;

    xnew = ret->xres + tipfield->xres;
    ynew = ret->yres + tipfield->yres;
    txr2 = tipfield->xres/2;
    tyr2 = tipfield->yres/2;

    for (row = 0; row < ynew; row++) {
        for (col = 0; col < xnew; col++) {
            if (col >= txr2
                && col < (ret->xres + txr2)
                && row >= tyr2
                && row < (ret->yres + tyr2)) {
                ret->data[col - txr2 + ret->xres*(row - tyr2)]
                    = field[row][col]*step + min;
            }
        }
    }

    gwy_data_field_invalidate(ret);
    return ret;
}

static GwyDataField*
get_right_tip_field(GwyDataField *tip,
                    GwyDataField *surface)
{
    gint xres, yres;

    xres = GWY_ROUND(tip->xreal/surface->xreal*surface->xres);
    xres = MAX(xres, 1);
    yres = GWY_ROUND(tip->yreal/surface->yreal*surface->yres);
    yres = MAX(yres, 1);

    return gwy_data_field_new_resampled(tip, xres, yres,
                                        GWY_INTERPOLATION_BSPLINE);
}

static inline gdouble
tip_convolve_interior(const gdouble *src, gint xres,
                      const gdouble *tip, gint txres, gint tyres)
{
    gdouble hmax = -G_MAXDOUBLE;
    gint i, j;

    for (i = 0; i < tyres; i++) {
        const gdouble *srcrow = src + i*xres;
        for (j = txres; j; j--) {
            gdouble h = *(srcrow++) + *(tip++);
            if (h > hmax)
                hmax = h;
        }
    }
    return hmax;
}

static inline gdouble
tip_convolve_border(const gdouble *src, gint xres, gint yres,
                    const gdouble *tip, gint txres, gint tyres,
                    gint j, gint i)
{
    gint ioff = tyres/2, joff = txres/2;
    gdouble hmax = -G_MAXDOUBLE;
    gint ii, jj;

    for (ii = 0; ii < tyres; ii++) {
        gint isrc = CLAMP(i + ii - ioff, 0, yres-1);
        for (jj = 0; jj < txres; jj++) {
            gint jsrc = CLAMP(j + jj - joff, 0, xres-1);
            gdouble h = src[isrc*xres + jsrc] + *(tip++);
            if (h > hmax)
                hmax = h;
        }
    }
    return hmax;
}

/**
 * gwy_tip_dilation:
 * @tip: Tip data.
 * @surface: Surface data.
 * @result: Data field where to store dilated surface to.
 * @set_fraction: Function that sets fraction to output (or %NULL).
 * @set_message: Function that sets message to output (or %NULL).
 *
 * Performs the tip convolution algorithm published by Villarrubia, which is
 * equivalent to morphological dilation operation.
 *
 * If the operation is aborted the size and contents of @result field is
 * undefined.
 *
 * Returns: Dilated surface data, i.e. @result, on success.  May return %NULL
 *          if aborted.
 **/
GwyDataField*
gwy_tip_dilation(GwyDataField *tip,
                 GwyDataField *surface,
                 GwyDataField *result,
                 GwySetFractionFunc set_fraction,
                 GwySetMessageFunc set_message)
{
    gint txres, tyres, xres, yres, ioff, joff, i, j;
    const gdouble *sdata, *tdata;
    gdouble *data;
    GwyDataField *mytip;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(tip), NULL);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(surface), NULL);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(result), NULL);

    if (set_message)
        set_message(_("Dilation..."));
    if (set_fraction)
        set_fraction(0.0);

    gwy_data_field_resample(result, surface->xres, surface->yres,
                            GWY_INTERPOLATION_NONE);
    gwy_data_field_invalidate(result);

    /* Preserve the surface height as original implementation does. */
    mytip = gwy_data_field_duplicate(tip);
    gwy_data_field_add(mytip, -gwy_data_field_get_max(mytip));

    txres = tip->xres;
    tyres = tip->yres;
    xres = surface->xres;
    yres = surface->yres;
    sdata = surface->data;
    tdata = mytip->data;
    data = result->data;
    ioff = tyres/2;
    joff = txres/2;

    for (i = 0; i < yres; i++) {
        gboolean row_inside = (i >= ioff && i + tyres-ioff <= yres);
        for (j = 0; j < xres; j++) {
            gboolean col_inside = (j >= joff && j + txres-joff <= xres);
            if (row_inside && col_inside) {
                const gdouble *src = sdata + (i - ioff)*xres + (j - joff);
                data[i*xres + j] = tip_convolve_interior(src, xres,
                                                         tdata, txres, tyres);
            }
            else {
                data[i*xres + j] = tip_convolve_border(sdata, xres, yres,
                                                       tdata, txres, tyres,
                                                       j, i);
            }
        }

        if (set_fraction && !set_fraction((i + 1.0)/yres)) {
            g_object_unref(mytip);
            return NULL;
        }
    }

    g_object_unref(mytip);
    return result;
}

static inline gdouble
tip_erode_interior(const gdouble *src, gint xres,
                   const gdouble *tip, gint txres, gint tyres)
{
    gdouble hmin = G_MAXDOUBLE;
    gint i, j;

    for (i = 0; i < tyres; i++) {
        const gdouble *srcrow = src + i*xres;
        for (j = txres; j; j--) {
            gdouble h = *(srcrow++) - *(tip++);
            if (h < hmin)
                hmin = h;
        }
    }
    return hmin;
}

static inline gdouble
tip_erode_border(const gdouble *src, gint xres, gint yres,
                 const gdouble *tip, gint txres, gint tyres,
                 gint j, gint i)
{
    gint ioff = tyres/2, joff = txres/2;
    gdouble hmin = G_MAXDOUBLE;
    gint ii, jj;

    for (ii = 0; ii < tyres; ii++) {
        gint isrc = CLAMP(i + ii - ioff, 0, yres-1);
        for (jj = 0; jj < txres; jj++) {
            gint jsrc = CLAMP(j + jj - joff, 0, xres-1);
            gdouble h = src[isrc*xres + jsrc] - *(tip++);
            if (h < hmin)
                hmin = h;
        }
    }
    return hmin;
}

/**
 * gwy_tip_erosion:
 * @tip: Tip data.
 * @surface: Surface to be eroded.
 * @result: Data field where to store dilated surface to.
 * @set_fraction: Function that sets fraction to output (or %NULL).
 * @set_message: Function that sets message to output (or %NULL).
 *
 * Performs surface reconstruction (erosion) algorithm published by
 * Villarrubia, which is equivalent to morphological erosion operation.
 *
 * If the operation is aborted the size and contents of @result field is
 * undefined.
 *
 * Returns: Reconstructed (eroded) surface, i.e. @result, on success.  May
 *          return %NULL if aborted.
 **/
GwyDataField*
gwy_tip_erosion(GwyDataField *tip,
                GwyDataField *surface,
                GwyDataField *result,
                GwySetFractionFunc set_fraction,
                GwySetMessageFunc set_message)
{
    gint txres, tyres, xres, yres, ioff, joff, i, j;
    const gdouble *sdata, *tdata;
    GwyDataField *mytip;
    gdouble *data;

    g_return_val_if_fail(GWY_IS_DATA_FIELD(tip), NULL);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(surface), NULL);
    g_return_val_if_fail(GWY_IS_DATA_FIELD(result), NULL);

    if (set_message)
        set_message(_("Erosion..."));
    if (set_fraction)
        set_fraction(0.0);

    gwy_data_field_resample(result, surface->xres, surface->yres,
                            GWY_INTERPOLATION_NONE);
    gwy_data_field_invalidate(result);

    /* Preserve the surface height as original implementation does. */
    mytip = gwy_data_field_duplicate(tip);
    gwy_data_field_invert(mytip, TRUE, TRUE, FALSE);
    gwy_data_field_add(mytip, -gwy_data_field_get_max(mytip));

    txres = tip->xres;
    tyres = tip->yres;
    xres = surface->xres;
    yres = surface->yres;
    sdata = surface->data;
    tdata = mytip->data;
    data = result->data;
    ioff = tyres/2;
    joff = txres/2;

    for (i = 0; i < yres; i++) {
        gboolean row_inside = (i >= ioff && i + tyres-ioff <= yres);
        for (j = 0; j < xres; j++) {
            gboolean col_inside = (j >= joff && j + txres-joff <= xres);
            if (row_inside && col_inside) {
                const gdouble *src = sdata + (i - ioff)*xres + (j - joff);
                data[i*xres + j] = tip_erode_interior(src, xres,
                                                      tdata, txres, tyres);
            }
            else {
                data[i*xres + j] = tip_erode_border(sdata, xres, yres,
                                                    tdata, txres, tyres,
                                                    j, i);
            }
        }

        if (set_fraction && !set_fraction((i + 1.0)/yres)) {
            g_object_unref(mytip);
            return NULL;
        }
    }

    g_object_unref(mytip);
    return result;
}

/**
 * gwy_tip_cmap:
 * @tip: Tip data.
 * @surface: Surface data.
 * @result: Data field to store ceratainty map data to.
 * @set_fraction: Function that sets fraction to output (or %NULL).
 * @set_message: Function that sets message to output (of %NULL).
 *
 * Performs certainty map algorithm published by Villarrubia. This function
 * converts all fields into form requested by "morph_lib.c" library, that is
 * almost identical with original Villarubia's library. Result certainty map
 * can be used as a mask of points where tip did not directly touch the
 * surface.
 *
 * Returns: Certainty map, i.e. @result, on success.  May return %NULL if
 *          aborted.
 **/
GwyDataField*
gwy_tip_cmap(GwyDataField *tip,
             GwyDataField *surface,
             GwyDataField *result,
             GwySetFractionFunc set_fraction,
             GwySetMessageFunc set_message)
{
    gint **ftip;
    gint **fsurface;
    gint **rsurface;
    gint **fresult;
    gint newx, newy;
    gdouble tipmin, surfacemin, step;
    GwyDataField *buffertip;

    /*if tip and surface have different spacings, make new, resampled tip*/
    buffertip = get_right_tip_field(tip, surface);
    /*invert tip (as necessary by dilation algorithm)*/
    gwy_data_field_invert(buffertip, TRUE, TRUE, FALSE);

    newx = surface->xres + buffertip->xres;
    newy = surface->yres + buffertip->yres;

    /*convert fields to integer arrays*/
    tipmin = gwy_data_field_get_min(buffertip);
    surfacemin = gwy_data_field_get_min(surface);
    step = (gwy_data_field_get_max(surface) - surfacemin)/10000;

    ftip = i_datafield_to_field(buffertip, TRUE, tipmin, step);
    fsurface = i_datafield_to_largefield(surface, buffertip, surfacemin, step);

    /*perform erosion as it is necessary parameter of certainty map algorithm*/
    rsurface = _gwy_morph_lib_ierosion(fsurface, newx, newy,
                                       ftip, buffertip->xres, buffertip->yres,
                                       buffertip->xres/2, buffertip->yres/2,
                                       set_fraction, set_message);
    if (!rsurface) {
        _gwy_morph_lib_ifreematrix(ftip);
        _gwy_morph_lib_ifreematrix(fsurface);
        g_object_unref(buffertip);
       return NULL;
    }

    /*find certanty map*/
    if (rsurface) {
        fresult = _gwy_morph_lib_icmap(fsurface, newx, newy,
                                       ftip, buffertip->xres, buffertip->yres,
                                       rsurface,
                                       buffertip->xres/2, buffertip->yres/2,
                                       set_fraction, set_message);
    }
    else
        fresult = NULL;

    /*convert result back*/
    if (fresult) {
        gwy_data_field_resample(result, surface->xres, surface->yres,
                                GWY_INTERPOLATION_NONE);
        result = i_largefield_to_datafield(fresult, result, buffertip,
                                           0.0, 1.0);
    }
    else
        result = NULL;

    g_object_unref(buffertip);
    _gwy_morph_lib_ifreematrix(ftip);
    _gwy_morph_lib_ifreematrix(fsurface);
    _gwy_morph_lib_ifreematrix(rsurface);
    if (fresult)
        _gwy_morph_lib_ifreematrix(fresult);

    return result;
}

/**
 * gwy_tip_estimate_partial:
 * @tip: Tip data to be refined (allocated).
 * @surface: Surface data.
 * @threshold: Threshold for noise supression.
 * @use_edges: Whether use also edges of image.
 * @count: Where to store the number of places that produced refinements to.
 * @set_fraction: Function that sets fraction to output (or %NULL).
 * @set_message: Function that sets message to output (or %NULL).
 *
 * Performs partial blind estimation algorithm published by Villarrubia. This
 * function converts all fields into form requested by "morph_lib.c" library,
 * that is almost identical with original Villarubia's library. Note that the
 * threshold value must be chosen sufficently high value to supress small
 * fluctulations due to noise (that would lead to very sharp tip) but
 * sufficiently low value to put algorithm at work. A value similar to 1/10000
 * of surface range can be good. Otherwise we recommend to start with zero
 * threshold and increase it slowly to observe changes and choose right value.
 *
 * Returns: Estimated tip.  May return %NULL if aborted.
 **/
GwyDataField*
gwy_tip_estimate_partial(GwyDataField *tip,
                         GwyDataField *surface,
                         gdouble threshold,
                         gboolean use_edges,
                         gint *count,
                         GwySetFractionFunc set_fraction,
                         GwySetMessageFunc set_message)
{
    gint **ftip;
    gint **fsurface;
    gdouble tipmin, surfacemin, step;
    gint cnt;

    if (set_message && !set_message(N_("Converting fields")))
        return NULL;

    tipmin = gwy_data_field_get_min(tip);
    surfacemin = gwy_data_field_get_min(surface);
    step = (gwy_data_field_get_max(surface)-surfacemin)/10000;

    ftip = i_datafield_to_field(tip, TRUE,  tipmin, step);
    fsurface = i_datafield_to_field(surface, FALSE, surfacemin, step);

    if (set_message && !set_message(N_("Starting partial estimation"))) {
        _gwy_morph_lib_ifreematrix(ftip);
        _gwy_morph_lib_ifreematrix(fsurface);
        return NULL;
    }

    cnt = _gwy_morph_lib_itip_estimate0(fsurface, surface->xres, surface->yres,
                                        tip->xres, tip->yres,
                                        tip->xres/2, tip->yres/2,
                                        ftip, threshold/step, use_edges,
                                        set_fraction, set_message);
    if (cnt == -1 || (set_fraction && !set_fraction(0.0))) {
        _gwy_morph_lib_ifreematrix(ftip);
        _gwy_morph_lib_ifreematrix(fsurface);
        return NULL;
    }
    gwy_debug("Converting fields");
    if (set_message)
        set_message(N_("Converting fields"));

    tip = i_field_to_datafield(ftip, tip, tipmin, step);
    gwy_data_field_add(tip, -gwy_data_field_get_min(tip));

    _gwy_morph_lib_ifreematrix(ftip);
    _gwy_morph_lib_ifreematrix(fsurface);
    if (count)
        *count = cnt;

    return tip;
}


/**
 * gwy_tip_estimate_full:
 * @tip: Tip data to be refined (allocated).
 * @surface: Surface data.
 * @threshold: Threshold for noise supression.
 * @use_edges: Whether use also edges of image.
 * @count: Where to store the number of places that produced refinements to.
 * @set_fraction: Function that sets fraction to output (or %NULL).
 * @set_message: Function that sets message to output (or %NULL).
 *
 * Performs full blind estimation algorithm published by Villarrubia. This
 * function converts all fields into form requested by "morph_lib.c" library,
 * that is almost identical with original Villarubia's library. Note that the
 * threshold value must be chosen sufficently high value to supress small
 * fluctulations due to noise (that would lead to very sharp tip) but
 * sufficiently low value to put algorithm at work. A value similar to 1/10000
 * of surface range can be good. Otherwise we recommend to start with zero
 * threshold and increase it slowly to observe changes and choose right value.
 *
 * Returns: Estimated tip.  May return %NULL if aborted.
 **/
GwyDataField*
gwy_tip_estimate_full(GwyDataField *tip,
                      GwyDataField *surface,
                      gdouble threshold,
                      gboolean use_edges,
                      gint *count,
                      GwySetFractionFunc set_fraction,
                      GwySetMessageFunc set_message)
{
    gint **ftip;
    gint **fsurface;
    gdouble tipmin, surfacemin, step;
    gint cnt;

    if (set_message && !set_message(N_("Converting fields")))
        return NULL;

    tipmin = gwy_data_field_get_min(tip);
    surfacemin = gwy_data_field_get_min(surface);
    step = (gwy_data_field_get_max(surface)-surfacemin)/10000;

    ftip = i_datafield_to_field(tip, TRUE, tipmin, step);
    fsurface = i_datafield_to_field(surface, FALSE,  surfacemin, step);

    if (set_message && !set_message(N_("Starting full estimation"))) {
        _gwy_morph_lib_ifreematrix(ftip);
        _gwy_morph_lib_ifreematrix(fsurface);
        return NULL;
    }

    cnt = _gwy_morph_lib_itip_estimate(fsurface, surface->xres, surface->yres,
                                       tip->xres, tip->yres,
                                       tip->xres/2, tip->yres/2,
                                       ftip, threshold/step, use_edges,
                                       set_fraction, set_message);
    if (cnt == -1 || (set_fraction && !set_fraction(0.0))) {
        _gwy_morph_lib_ifreematrix(ftip);
        _gwy_morph_lib_ifreematrix(fsurface);
        return NULL;
    }
    if (set_message)
        set_message(N_("Converting fields"));
    tip = i_field_to_datafield(ftip, tip, tipmin, step);
    gwy_data_field_add(tip, -gwy_data_field_get_min(tip));

    _gwy_morph_lib_ifreematrix(ftip);
    _gwy_morph_lib_ifreematrix(fsurface);
    if (count)
        *count = cnt;

    return tip;
}

/************************** Documentation ****************************/

/**
 * SECTION:tip
 * @title: tip
 * @short_description: SPM tip methods
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
