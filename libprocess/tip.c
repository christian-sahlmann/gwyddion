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
#include "datafield.h"
#include "morph_lib.h"

static void
pyramide_guess(GwyDataField *data,
               gdouble height,
               gdouble radius,
               gdouble *params,
               gint *xres,
               gint *yres)
{
    gdouble angle = params[1];
    gdouble xreal = 2*(height+radius)/tan(angle);
    gint xpix = gwy_data_field_rtoi(data, xreal);

    xpix = CLAMP(xpix, 10, 500);

    *xres = xpix;
    *yres = xpix;
}

static void
contact_guess(GwyDataField *data,
              gdouble height,
              gdouble radius,
              G_GNUC_UNUSED gdouble *params,
              gint *xres,
              gint *yres)
{
    gdouble angle = atan(sqrt(2));
    gdouble xreal = 2*(height+radius)/tan(angle);
    gint xpix = gwy_data_field_rtoi(data, xreal);

    xpix = CLAMP(xpix, 10, 500);

    *xres = xpix;
    *yres = xpix;

}
static void
noncontact_guess(GwyDataField *data,
                 gdouble height,
                 gdouble radius,
                 G_GNUC_UNUSED gdouble *params,
                 gint *xres,
                 gint *yres)
{
    gdouble angle = 70*G_PI/180;
    gdouble xreal = 2*(height+radius)/tan(angle);
    gint xpix = gwy_data_field_rtoi(data, xreal);

    xpix = CLAMP(xpix, 10, 500);

    *xres = xpix;
    *yres = xpix;
}

static void
delta_guess(G_GNUC_UNUSED GwyDataField *data,
            G_GNUC_UNUSED gdouble height,
            G_GNUC_UNUSED gdouble radius,
            G_GNUC_UNUSED gdouble *params,
            gint *xres, gint *yres)
{
    *xres = 20;
    *yres = 20;

}

static void
create_pyramide(GwyDataField *tip, gdouble alpha, gint n, gdouble theta)
{
    gint col, row;
    gdouble rcol, rrow;
    gdouble scol, srow;
    gdouble ccol, crow;
    gdouble phi, phic;
    gdouble vm, radius;
    gdouble height;
    gdouble nangle;
    gdouble add = G_PI/4;

    if (n == 3)
        add = G_PI/6;

    add += theta;
    radius = sqrt((tip->xres/2)*(tip->xres/2)+(tip->yres/2)*(tip->yres/2));
    nangle = G_PI/n;
    height = gwy_data_field_itor(tip, radius)*cos(nangle)/tan(alpha);

    scol = tip->xres/2;
    srow = tip->yres/2;

    for (col = 0; col < tip->xres; col++) {
        for (row = 0; row < tip->yres; row++) {
            ccol = col - scol;
            crow = row - srow;
            rcol = -ccol*cos(add) + crow*sin(add);
            rrow = ccol*sin(add) + crow*cos(add);
            phi = atan2(rrow, rcol) + G_PI;
            phic = floor(phi/(2*G_PI/n))*2*G_PI/n + G_PI/n;
            vm = rcol*cos(phic) + rrow*sin(phic);
            tip->data[col + tip->xres*row]
                = height*(1 + vm/(radius*cos(G_PI/n)));
        }
    }
}

static void
round_pyramide(GwyDataField *tip, gdouble angle, gint n, gdouble ballradius)
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
}

static void
pyramide(GwyDataField *tip,
         G_GNUC_UNUSED gdouble height,
         gdouble radius,
         gdouble rotation,
         gdouble *params)
{
    /*params[0]..number of sides, params[1]..angle*/
    create_pyramide(tip, params[1], params[0], rotation);
    round_pyramide(tip, params[1], params[0], radius);
}

static void
contact(GwyDataField *tip,
        G_GNUC_UNUSED gdouble height,
        gdouble radius,
        gdouble rotation,
        G_GNUC_UNUSED gdouble *params)
{
    gdouble angle = G_PI/2 - atan(sqrt(2));
    create_pyramide(tip, angle, 4, rotation);
    round_pyramide(tip, angle, 4, radius);
}

static void
noncontact(GwyDataField *tip,
           G_GNUC_UNUSED gdouble height,
           gdouble radius,
           gdouble rotation,
           G_GNUC_UNUSED gdouble *params)
{
    gdouble angle = G_PI/2 - atan(sqrt(2));
    create_pyramide(tip, angle, 3, rotation);
    round_pyramide(tip, angle, 3, radius);
}

static void
delta(GwyDataField *tip, gdouble height,
      G_GNUC_UNUSED gdouble radius,
      G_GNUC_UNUSED gdouble rotation,
      G_GNUC_UNUSED gdouble *params)
{
    gwy_data_field_fill(tip, 0);
    tip->data[tip->xres/2 + tip->xres*tip->yres/2] = height;
}

static const GwyTipModelPreset tip_presets[] = {
    {
        "Pyramide",
        "Pyramidal",
        &pyramide,
        &pyramide_guess,
        0
    },
    {
        "Contact",
        "Pyramidal",
        &contact,
        &contact_guess,
        0
    },
    {
        "Noncontact",
        "Pyramidal",
        &noncontact,
        &noncontact_guess,
        0
    },
     {
        "Delta function",
        "Analytical",
        &delta,
        &delta_guess,
        0
    },
};

/**
 * gwy_tip_model_get_npresets:
 *
 * Find number of actual tip model presets.
 *
 * Returns: number of presets
 **/
gint
gwy_tip_model_get_npresets(void)
{
    return (gint)G_N_ELEMENTS(tip_presets);
}

/**
 * gwy_tip_model_get_preset:
 * @preset_id: preset identifier
 *
 * Get data related to tip preset.
 *
 * Returns: chosen preset data.
 **/
G_CONST_RETURN GwyTipModelPreset*
gwy_tip_model_get_preset(gint preset_id)
{
    g_return_val_if_fail(preset_id >= 0
                         && preset_id < (gint)G_N_ELEMENTS(tip_presets),
                         NULL);

    return tip_presets + preset_id;
}

/**
 * gwy_tip_model_get_preset_by_name:
 * @name: name of tip (e. g. "contact")
 *
 * Get data related to preset with specified name.
 *
 * Returns: chosen preset data.
 **/
G_CONST_RETURN GwyTipModelPreset*
gwy_tip_model_get_preset_by_name(const gchar *name)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(tip_presets); i++) {
        if (strcmp(name, tip_presets[i].tip_name) == 0)
            return tip_presets + i;
    }
    return NULL;
}

/**
 * gwy_tip_model_get_preset_id:
 * @preset: tip model preset
 *
 * Get preset identifier within all presets.
 *
 * Returns: preset id.
 **/
gint
gwy_tip_model_get_preset_id(const GwyTipModelPreset* preset)
{
    return preset - tip_presets;
}

/**
 * gwy_tip_model_get_preset_tip_name:
 * @preset: tip model preset
 *
 * Get name of the preset (e. g. "contact").
 *
 * Returns: preset name.
 **/
G_CONST_RETURN gchar*
gwy_tip_model_get_preset_tip_name(const GwyTipModelPreset* preset)
{
    return preset->tip_name;
}

/**
 * gwy_tip_model_get_preset_group_name:
 * @preset: tip model preset
 *
 * Get group name of preset (e. g. "analytical".)
 *
 * Returns: preset group name
 **/
G_CONST_RETURN gchar*
gwy_tip_model_get_preset_group_name(const GwyTipModelPreset* preset)
{
    return preset->group_name;
}

/**
 * gwy_tip_model_get_preset_nparams:
 * @preset: tip model preset
 *
 * Get number of tip preset parameters.
 *
 * Returns: number of parameters.
 **/
gint
gwy_tip_model_get_preset_nparams(const GwyTipModelPreset* preset)
{
    return preset->nparams;
}


static gdouble **
datafield_to_field(GwyDataField *datafield, gboolean maxzero)
{
    gdouble **ret;
    gint col, row;
    gdouble max;

    max = maxzero ? gwy_data_field_get_max(datafield) : 0.0;

    ret = _gwy_morph_lib_dallocmatrix(datafield->xres, datafield->yres);
    for (col = 0; col < datafield->xres; col++) {
        for (row = 0; row < datafield->yres; row++) {
            ret[col][row] = datafield->data[col + datafield->xres*row] - max;
        }
    }
    return ret;
}

static GwyDataField*
field_to_datafield(gdouble **field, GwyDataField *ret)
{
    gint col, row;
    for (col = 0; col < ret->xres; col++) {
        for (row = 0; row < ret->yres; row++) {
            ret->data[col  + ret->xres*row] = field[col][row];
        }
    }
    return ret;
}

static gint **
i_datafield_to_field(GwyDataField *datafield,
                     gboolean maxzero,
                     gdouble min,
                     gdouble step)
{
    gint **ret;
    gint col, row;
    gdouble max;

    max = maxzero ? gwy_data_field_get_max(datafield) : 0.0;

    ret = _gwy_morph_lib_iallocmatrix(datafield->xres, datafield->yres);
    for (col = 0; col < datafield->xres; col++) {
        for (row = 0; row < datafield->yres; row++) {
            ret[col][row] = (gint)(((datafield->data[col
                                     + datafield->xres*row] - max) - min)/step);
        }
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

    for (col = 0; col < ret->xres; col++) {
        for (row = 0; row < ret->yres; row++) {
            ret->data[col + ret->xres*row] = (gdouble)field[col][row]*step
                                             + min;
        }
    }
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
    gint xnew, ynew;
    gint minimum;

    minimum = (gint)((gwy_data_field_get_min(datafield) - min)/step);
    xnew = datafield->xres + tipfield->xres;
    ynew = datafield->yres + tipfield->yres;

    ret = _gwy_morph_lib_iallocmatrix(xnew, ynew);
    for (col = 0; col < xnew; col++) {
        for (row = 0; row < ynew; row++) {
            if (col >= tipfield->xres/2
                && col < (datafield->xres + tipfield->xres/2)
                && row >= tipfield->yres/2
                && row < (datafield->yres + tipfield->yres/2))
            ret[col][row] = (gint)(((datafield->data[col - tipfield->xres/2
                + datafield->xres*(row - tipfield->yres/2)]) - min)/step);
            else
                ret[col][row] = minimum;
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

    xnew = ret->xres + tipfield->xres;
    ynew = ret->yres + tipfield->yres;

    for (col = 0; col < xnew; col++) {
        for (row = 0; row < ynew; row++) {
            if (col >= tipfield->xres/2
                && col < (ret->xres + tipfield->xres/2)
                && row >= tipfield->yres/2
                && row < (ret->yres + tipfield->yres/2)) {
                ret->data[col - tipfield->xres/2 + ret->xres*(row - tipfield->yres/2)]
                    = field[col][row]*step + min;
            }
        }
    }
    return ret;
}

static GwyDataField*
get_right_tip_field(GwyDataField *tip,
                    GwyDataField *surface,
                    gboolean *freetip)
{
    GwyDataField *buffer;
    gdouble tipxstep, tipystep;
    gdouble surfxstep, surfystep;

    *freetip = FALSE;
    tipxstep = tip->xreal/tip->xres;
    surfxstep = surface->xreal/surface->xres;
    tipystep = tip->yreal/tip->yres;
    surfystep = surface->yreal/surface->yres;

    if (fabs(tipxstep/surfxstep - 1.0) > 0.01
        || fabs(tipystep/surfystep - 1.0) > 0.01) {
        buffer = GWY_DATA_FIELD(gwy_data_field_new(tip->xres, tip->yres,
                                                   tip->xreal, tip->yreal,
                                                   FALSE));
        gwy_data_field_copy(tip, buffer);

        gwy_data_field_resample(buffer, tip->xres/surfxstep*tipxstep,
                                tip->yres/surfystep*tipystep,
                                GWY_INTERPOLATION_BILINEAR);
        *freetip = TRUE;
        return buffer;
    }
    else
        return tip;
}

/**
 * gwy_tip_dilation:
 * @tip: tip data
 * @surface: surface data
 * @result: pointer where to store dilated surface data (allocated GwyDataField)
 * @set_fraction: function that sets fraction to output (or NULL)
 * @set_message: function that sets message to output (of NULL)
 *
 * Performs tip convolution (dilation) algorithm published by Villarrubia. This
 * function converts all fields into form requested by "morph_lib.c" library,
 * that is almost identical with original Villarubia's library.
 *
 * Returns: dilated surface data.
 **/
GwyDataField*
gwy_tip_dilation(GwyDataField *tip,
                 GwyDataField *surface,
                 GwyDataField *result,
                 GwySetFractionFunc set_fraction,
                 GwySetMessageFunc set_message)
{
    gdouble **ftip;
    gdouble **fsurface;
    gdouble **fresult;
    GwyDataField *buffertip;
    gboolean freetip;

    /*if tip and surface have different spacings, make new, resampled tip*/
    buffertip = get_right_tip_field(tip, surface, &freetip);
    /*invert tip (as necessary by dilation algorithm)*/
    gwy_data_field_invert(buffertip, TRUE, TRUE, FALSE);

    /*make auxiliary data arrays expected by Villarubia's algorithms*/
    ftip = datafield_to_field(buffertip, TRUE);
    fsurface = datafield_to_field(surface, FALSE);

    fresult = _gwy_morph_lib_ddilation(fsurface, surface->yres, surface->xres,
                                       ftip, buffertip->yres, buffertip->xres,
                                       buffertip->yres/2, buffertip->xres/2,
                                       set_fraction, set_message);

    /*convert result back from auxiliary array*/
    result = field_to_datafield(fresult, result);

    /*free auxiliary data arrays*/
    _gwy_morph_lib_dfreematrix(ftip, buffertip->xres);
    _gwy_morph_lib_dfreematrix(fsurface, surface->xres);
    _gwy_morph_lib_dfreematrix(fresult, result->xres);
    if (freetip)
        g_object_unref(buffertip);
    else
        gwy_data_field_invert(buffertip, TRUE, TRUE, FALSE);

    return result;
}

/**
 * gwy_tip_erosion:
 * @tip: tip data
 * @surface: surface to be eroded
 * @result: pointer where to store result data (allocated GwyDataField).
 * @set_fraction: function that sets fraction to output (or NULL)
 * @set_message: function that sets message to output (of NULL)
 *
 * Performs surface reconstruction (erosion) algorithm published by
 * Villarrubia. This function converts all fields into form requested by
 * "morph_lib.c" library, that is almost identical with original Villarubia's
 * library.
 *
 * Returns: reconstructed (eroded) surface.
 **/
GwyDataField*
gwy_tip_erosion(GwyDataField *tip,
                GwyDataField *surface,
                GwyDataField *result,
                GwySetFractionFunc set_fraction,
                GwySetMessageFunc set_message)
{
    gdouble **ftip;
    gdouble **fsurface;
    gdouble **fresult;
    GwyDataField *buffertip;
    gboolean freetip;

    /*if tip and surface have different spacings, make new, resampled tip*/
    buffertip = get_right_tip_field(tip, surface, &freetip);
    /*invert tip (as necessary by dilation algorithm)*/
    gwy_data_field_invert(buffertip, TRUE, TRUE, FALSE);

    /*make auxiliary data arrays expected by Villarubia's algorithms*/
    ftip = datafield_to_field(buffertip, TRUE);
    fsurface = datafield_to_field(surface, FALSE);

    fresult = _gwy_morph_lib_derosion(fsurface, surface->yres, surface->xres,
                                      ftip, buffertip->yres, buffertip->xres,
                                      buffertip->yres/2, buffertip->xres/2,
                                      set_fraction, set_message);

    /*convert result back from auxiliary array*/
    result = field_to_datafield(fresult, result);

    /*free auxiliary data arrays*/
    _gwy_morph_lib_dfreematrix(ftip, buffertip->xres);
    _gwy_morph_lib_dfreematrix(fsurface, surface->xres);
    _gwy_morph_lib_dfreematrix(fresult, result->xres);
    if (freetip)
        g_object_unref(buffertip);
    else
        gwy_data_field_invert(buffertip, TRUE, TRUE, FALSE);

    return result;
}

/**
 * gwy_tip_cmap:
 * @tip: tip data
 * @surface: surface data
 * @result: pointer where to store result ceratainty map data
 *          (allocated #GwyDataField)
 * @set_fraction: function that sets fraction to output (or %NULL)
 * @set_message: function that sets message to output (of %NULL)
 *
 * Performs certainty map algorithm published by Villarrubia. This function
 * converts all fields into form requested by "morph_lib.c" library, that is
 * almost identical with original Villarubia's library. Result certainty map
 * can be used as a mask of points where tip did not directly touch the
 * surface.
 *
 * Returns: certainty map
 **/
GwyDataField*
gwy_tip_cmap(GwyDataField *tip, GwyDataField *surface, GwyDataField *result,
              GwySetFractionFunc set_fraction, GwySetMessageFunc set_message)
{
    gint **ftip;
    gint **fsurface;
    gint **rsurface;
    gint **fresult;
    gint newx, newy;
    gdouble tipmin, surfacemin, step;
    GwyDataField *buffertip;
    gboolean freetip;

    newx = surface->xres + tip->xres;
    newy = surface->yres + tip->yres;

    /*if tip and surface have different spacings, make new, resampled tip*/
    buffertip = get_right_tip_field(tip, surface, &freetip);
    /*invert tip (as necessary by dilation algorithm)*/
    gwy_data_field_invert(buffertip, TRUE, TRUE, FALSE);

    /*convert fields to integer arrays*/
    tipmin = gwy_data_field_get_min(buffertip);
    surfacemin = gwy_data_field_get_min(surface);
    step = (gwy_data_field_get_max(surface)-surfacemin)/10000;

    ftip = i_datafield_to_field(buffertip, TRUE, tipmin, step);
    fsurface = i_datafield_to_largefield(surface, buffertip, surfacemin, step);

    /*perform erosion as it is necessary parameter of certainty map algorithm*/
    rsurface = _gwy_morph_lib_ierosion(fsurface, newy, newx,
                                       ftip, buffertip->yres, buffertip->xres,
                                       buffertip->yres/2, buffertip->xres/2,
                                       set_fraction, set_message);

    /*find certanty map*/
    fresult = _gwy_morph_lib_icmap(fsurface, newy, newx,
                                   ftip, buffertip->yres, buffertip->xres,
                                   rsurface,
                                   buffertip->yres/2, buffertip->xres/2,
                                   set_fraction, set_message);

    /*convert result back*/
    result = i_largefield_to_datafield(fresult, result, buffertip, 0.0, 1.0);

    _gwy_morph_lib_ifreematrix(ftip, buffertip->xres);
    _gwy_morph_lib_ifreematrix(fsurface, newx);
    _gwy_morph_lib_ifreematrix(rsurface, newx);
    _gwy_morph_lib_ifreematrix(fresult, result->xres);
    if (freetip)
        g_object_unref(buffertip);
    else
        gwy_data_field_invert(buffertip, TRUE, TRUE, FALSE);

    return result;
}

/**
 * gwy_tip_estimate_partial:
 * @tip: tip data to be refined (allocated)
 * @surface: surface data
 * @threshold: threshold for noise supression
 * @use_edges: whether use also edges of image
 * @set_fraction: function that sets fraction to output (or NULL)
 * @set_message: function that sets message to output (of NULL)
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
 * Returns: estimated tip.
 **/
GwyDataField*
gwy_tip_estimate_partial(GwyDataField *tip,
                         GwyDataField *surface,
                         gdouble threshold,
                         gboolean use_edges,
                         GwySetFractionFunc set_fraction,
                         GwySetMessageFunc set_message)
{
    gint **ftip;
    gint **fsurface;
    gdouble tipmin, surfacemin, step;


    if (set_message) {
        if (!set_message(N_("Converting fields")))
            return NULL;
    }
    tipmin = gwy_data_field_get_min(tip);
    surfacemin = gwy_data_field_get_min(surface);
    step = (gwy_data_field_get_max(surface)-surfacemin)/10000;

    ftip = i_datafield_to_field(tip, TRUE,  tipmin, step);
    fsurface = i_datafield_to_field(surface, FALSE, surfacemin, step);

    if (set_message) {
        if (!set_message(N_("Starting partial estimation")))
            return NULL;
    }
    _gwy_morph_lib_itip_estimate0(fsurface, surface->yres, surface->xres,
                                  tip->yres, tip->xres,
                                  tip->yres/2, tip->xres/2,
                                  ftip, threshold/step,
                                  use_edges, set_fraction, set_message);
    if (set_fraction) {
        if (!set_fraction(0))
            return NULL;
    }
    if (set_message)
        set_message(N_("Converting fields"));

    tip = i_field_to_datafield(ftip, tip, tipmin, step);
    gwy_data_field_add(tip, -gwy_data_field_get_min(tip));

    _gwy_morph_lib_ifreematrix(ftip, tip->xres);
    _gwy_morph_lib_ifreematrix(fsurface, surface->xres);
    return tip;
}


/**
 * gwy_tip_estimate_full:
 * @tip: tip data to be refined (allocated)
 * @surface: surface data
 * @threshold: threshold for noise supression
 * @use_edges: whether use also edges of image
 * @set_fraction: function that sets fraction to output (or NULL)
 * @set_message: function that sets message to output (of NULL)
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
 * Returns: estimated tip.
 **/
GwyDataField*
gwy_tip_estimate_full(GwyDataField *tip,
                      GwyDataField *surface,
                      gdouble threshold,
                      gboolean use_edges,
                      GwySetFractionFunc set_fraction,
                      GwySetMessageFunc set_message)
{
    gint **ftip;
    gint **fsurface;
    gdouble tipmin, surfacemin, step;

    if (set_message) {
        if (!set_message(N_("Converting fields")))
            return NULL;
    }
    tipmin = gwy_data_field_get_min(tip);
    surfacemin = gwy_data_field_get_min(surface);
    step = (gwy_data_field_get_max(surface)-surfacemin)/10000;

    ftip = i_datafield_to_field(tip, TRUE, tipmin, step);
    fsurface = i_datafield_to_field(surface, FALSE,  surfacemin, step);

    if (set_message) {
        if (!set_message(N_("Starting full estimation")))
            return NULL;
    }
    _gwy_morph_lib_itip_estimate(fsurface, surface->yres, surface->xres,
                                 tip->yres, tip->xres,
                                 tip->yres/2, tip->xres/2,
                                 ftip, threshold/step,
                                 use_edges, set_fraction, set_message);

    if (set_fraction) {
        if (!set_fraction(0))
            return NULL;
    }
    if (set_message)
        set_message(N_("Converting fields"));
    tip = i_field_to_datafield(ftip, tip, tipmin, step);
    gwy_data_field_add(tip, -gwy_data_field_get_min(tip));

    _gwy_morph_lib_ifreematrix(ftip, tip->xres);
    _gwy_morph_lib_ifreematrix(fsurface, surface->xres);
    return tip;
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
