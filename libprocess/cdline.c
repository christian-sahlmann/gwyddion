/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Jindrich Bilek.
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
#include "dataline.h"
#include "cdline.h"

#define GWY_DATA_LINE_TYPE_NAME "GwyDataLine"


/* Preset */
typedef struct _GwyCDLineParam {
    const char *name;
    const char *unit;
    double default_init;
};



/*********************** gaussian *****************************/

static void
cd_stepheight(gdouble *x,
            gdouble *y,
            gint n_dat,
            gdouble *param,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{

    param[0] = param[1] = 0;
    *fres = TRUE;
}

static void
func_stepheight(gdouble x, gint n_param, gdouble *param, gpointer user_data, gboolean *fres)
{
    return 0;
}

/************************** presets ****************************/

static const GwyCDLineParam stepheight_pars[]= {
   {"x<sub>0</sub>", " ", 1 },
   {"y<sub>0</sub>", " ", 2 },
   {"a", " ", 3 },
   {"b", " ", 4 },
};


static const GwyCDLinePreset fitting_presets[] = {
    {
        "Step height",
        "Step",
        "ISO 42589",
        &func_stepheight,
        &cd_stepheight,
        4,
        stepheight_pars,
        NULL
    },
};

/**
 * gwy_math_cdline_get_npresets:
 *
 * Returns the number of available NL fitter presets.
 *
 * Returns: The number of presets.
 *
 * Since: 1.2.
 **/
gint
gwy_cdline_get_npresets(void)
{
    return (gint)G_N_ELEMENTS(fitting_presets);
}

/**
 * gwy_math_cdline_get_preset:
 * @preset_id: NL fitter preset number.
 *
 * Returns NL fitter preset number @preset_id.
 *
 * Presets are numbered sequentially from 0 to gwy_math_cdline_get_npresets()-1.
 * The numbers are not guaranteed to be constants, use preset names as unique
 * identifiers.
 *
 * Returns: Preset number @preset_id.  Note the returned value must not be
 *          modified or freed.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN GwyCDLinePreset*
gwy_cdline_get_preset(gint preset_id)
{
    g_return_val_if_fail(preset_id >= 0
                         && preset_id < (gint)G_N_ELEMENTS(fitting_presets),
                         NULL);

    return fitting_presets + preset_id;
}

/**
 * gwy_math_cdline_get_preset_by_name:
 * @name: NL fitter preset name.
 *
 * Returns NL fitter preset whose name is @name.
 *
 * Returns: Preset @name, %NULL if not found.  Note the returned value must
 *          not be modified or freed.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN GwyCDLinePreset*
gwy_cdline_get_preset_by_name(const gchar *name)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(fitting_presets); i++) {
        if (strcmp(name, fitting_presets[i].function_name) == 0)
            return fitting_presets + i;
    }
    return NULL;
}

/**
 * gwy_math_cdline_get_preset_id:
 * @preset: A NL fitter function preset.
 *
 * Returns the id of a NL fitter preset.
 *
 * Returns: The preset number.
 *
 * Since: 1.2.
 **/
gint
gwy_cdline_get_preset_id(const GwyCDLinePreset* preset)
{
    /* XXX: some sanity check? */
    return preset - fitting_presets;
}


/**
 * gwy_math_cdline_get_preset_name:
 * @preset: A NL fitter function preset.
 *
 * Return preset name (its unique identifier).
 *
 * Returns: The preset name.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN gchar*
gwy_cdline_get_preset_name(const GwyCDLinePreset* preset)
{
    return preset->function_name;
}

/**
 * gwy_math_cdline_get_preset_formula:
 * @preset: A NL fitter function preset.
 *
 * Returns function formula of @preset (with Pango markup).
 *
 * Returns: The preset function formula.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN gchar*
gwy_cdline_get_preset_formula(const GwyCDLinePreset* preset)
{
    return preset->function_formula;
}

/**
 * gwy_math_cdline_get_preset_param_name:
 * @preset: A NL fitter function preset.
 * @param: A parameter number.
 *
 * Returns the name of parameter number @param of preset @preset.
 *
 * The name may contain Pango markup.
 *
 * Returns: The name of parameter @param.
 *
 * Since: 1.2.
 **/
G_CONST_RETURN gchar*
gwy_cdline_get_preset_param_name(const GwyCDLinePreset* preset,
                                     gint param)
{
    const GwyCDLineParam *par;

    g_return_val_if_fail(param >= 0 && param < preset->nparams, NULL);
    par = preset->param + param;

    return par->name;
}

/**
 * gwy_math_cdline_get_preset_param_default:
 * @preset: A NL fitter function preset.
 * @param: A parameter number.
 *
 * Returns a suitable constant default parameter value.
 *
 * It is usually better to do an educated guess of initial parameter value.
 *
 * Returns: The default parameter value.
 *
 * Since: 1.2.
 **/
gdouble
gwy_cdline_get_preset_param_default(const GwyCDLinePreset* preset,
                                        gint param)
{
    const GwyCDLineParam *par;

    g_return_val_if_fail(param >= 0 && param < preset->nparams, G_MAXDOUBLE);
    par = preset->param + param;

    return par->default_init;
}

/**
 * gwy_math_cdline_get_preset_nparams:
 * @preset: A NL fitter function preset.
 *
 * Return the number of parameters of @preset.
 *
 * Returns: The number of function parameters.
 *
 * Since: 1.2.
 **/
gint
gwy_cdline_get_preset_nparams(const GwyCDLinePreset* preset)
{
    return preset->nparams;
}

/**
 * gwy_math_nlfit_fit_preset:
 * @preset: 
 * @n_dat: 
 * @x: 
 * @y: 
 * @n_param: 
 * @param: 
 * @err: 
 * @fixed_param: 
 * @user_data: 
 *
 * 
 *
 * Returns:
 *
 * Since: 1.2.
 **/
void
gwy_cdline_fit_preset(const GwyCDLinePreset* preset,
                          gint n_dat, const gdouble *x, const gdouble *y,
                          gint n_param,
                          gdouble *param, gdouble *err,
                          const gboolean *fixed_param,
                          gpointer user_data)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
