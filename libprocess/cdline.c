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
#include <stdio.h>

static void
cd_ustepheight(gdouble *x,
            gdouble *y,
            gint n_dat,
            gdouble *param,
            gdouble *err,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    gint i;
    gint nstep;
    gdouble max, val;
    gint imax, iwidth;
    gint nlow, nup;

    nstep = n_dat/20;
    iwidth = n_dat/10;
    if (nstep<1) nstep = 1;
    
    max = -G_MAXDOUBLE;
    for (i=nstep; i<(n_dat - 2*nstep); i++)
    {
        val = ((y[i+nstep] - y[i])/(x[i+nstep] - x[i]));
        if (max < val) {
            max = val;
            imax = i + nstep/2;
            param[1] = (x[i+nstep] + x[i])/2.0;
        }
    }
   
    param[2] = param[3] = 0;
    nlow=0; 
    nup = 0;
    for (i=0; i<n_dat; i++)
    {
        if (i<(imax - iwidth/2))
        {
            param[2] += y[i];
            nlow++;
        }
        else if (i>(imax + iwidth/2))
        {
            param[3] += y[i];
            nup++;
        }
    }
    param[2]/=(gdouble)nlow;
    param[3]/=(gdouble)nup;

    param[0] = param[3] - param[2];
   
    err[0] = err[1] = err[2] = err[3] = 0;
    *fres = TRUE;

}

static void
cd_lstepheight(gdouble *x,
            gdouble *y,
            gint n_dat,
            gdouble *param,
            gdouble *err,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    gint i;
    gint nstep;
    gdouble min, val;
    gint imin, iwidth;
    gint nlow, nup;

    nstep = n_dat/20;
    iwidth = n_dat/10;
    if (nstep<1) nstep = 1;
    
    min = G_MAXDOUBLE;
    for (i=nstep; i<(n_dat - 2*nstep); i++)
    {
        val = ((y[i+nstep] - y[i])/(x[i+nstep] - x[i]));
        if (min > val) {
            min = val;
            imin = i + nstep/2;
            param[1] = (x[i+nstep] + x[i])/2.0;
        }
    }
   
    param[2] = param[3] = 0;
    nlow=0; 
    nup = 0;
    for (i=0; i<n_dat; i++)
    {
        if (i<(imin - iwidth/2))
        {
            param[2] += y[i];
            nlow++;
        }
        else if (i>(imin + iwidth/2))
        {
            param[3] += y[i];
            nup++;
        }
    }
    param[2]/=(gdouble)nlow;
    param[3]/=(gdouble)nup;

    param[0] = param[3] - param[2];
   
    err[0] = err[1] = err[2] = err[3] = 0;
    *fres = TRUE;

}

static gdouble
func_stepheight(gdouble x, gint n_param, gdouble *param, gpointer user_data, gboolean *fres)
{
    if (x<param[1]) return param[2];
    else return param[3];
}


static void
cd_rlineheight(gdouble *x,
            gdouble *y,
            gint n_dat,
            gdouble *param,
            gdouble *err,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    gint i;
    gint nstep;
    gdouble max, min, val;
    gint imax, imin, iwidth;
    gint nin, nout;


    nstep = n_dat/20;
    if (nstep<1) nstep = 1;
    
    max = -G_MAXDOUBLE;
    min = G_MAXDOUBLE;
    
    for (i=nstep; i<(n_dat - 2*nstep); i++)
    {
        val = ((y[i+nstep] - y[i])/(x[i+nstep] - x[i]));
        if (min > val) {
            min = val;
            imin = i + nstep/2;
            param[3] = (x[i+nstep] + x[i])/2.0;
        }
    }
     
    for (i=imin; i<(n_dat - 2*nstep); i++)
    {
        val = ((y[i+nstep] - y[i])/(x[i+nstep] - x[i]));
        if (max < val) {
            max = val;
            imax = i + nstep/2;
            param[4] = (x[i+nstep] + x[i])/2.0;
        }
    }
   
    param[1] = param[2] = 0;
    nin=0; 
    nout = 0;
    iwidth = imax - imin;
    for (i=0; i<n_dat; i++)
    {
        if (i>(imin+iwidth/4) && i<(imax-iwidth/4))
        {
            param[2] += y[i];
            nin++;
        }
        else if ((i<(imin-iwidth/4) && i>(imin-3*iwidth/4))
                 || (i>(imax+iwidth/4) && i<(imax+3*iwidth/4)))
        {
            param[1] += y[i];
            nout++;
        }
    }
    param[2]/=(gdouble)nin;
    param[1]/=(gdouble)nout;

    param[0] = param[2] - param[1];
   
    err[0] = err[1] = err[2] = err[3] = err[4] = 0;
    *fres = TRUE;

}

static void
cd_lineheight(gdouble *x,
            gdouble *y,
            gint n_dat,
            gdouble *param,
            gdouble *err,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    gint i;
    gint nstep;
    gdouble max, min, val;
    gint imax, imin, iwidth;
    gint nin, nout;


    nstep = n_dat/20;
    if (nstep<1) nstep = 1;
    
    max = -G_MAXDOUBLE;
    min = G_MAXDOUBLE;
    for (i=nstep; i<(n_dat - 2*nstep); i++)
    {
        val = ((y[i+nstep] - y[i])/(x[i+nstep] - x[i]));
         if (max < val) {
            max = val;
            imax = i + nstep/2;
            param[3] = (x[i+nstep] + x[i])/2.0;
        }
    }
    
    for (i=imax; i<(n_dat - 2*nstep); i++)
    {
        val = ((y[i+nstep] - y[i])/(x[i+nstep] - x[i]));
        if (min > val) {
            min = val;
            imin = i + nstep/2;
            param[4] = (x[i+nstep] + x[i])/2.0;
        }
    }
   
    param[1] = param[2] = 0;
    nin=0; 
    nout = 0;
    iwidth = imin - imax;
    for (i=0; i<n_dat; i++)
    {
        if (i>(imax+iwidth/4) && i<(imin-iwidth/4))
        {
            param[2] += y[i];
            nin++;
        }
        else if ((i<(imax-iwidth/4) && i>(imax-3*iwidth/4))
                 || (i>(imin+iwidth/4) && i<(imin+3*iwidth/4)))
        {
            param[1] += y[i];
            nout++;
        }
    }
    param[2]/=(gdouble)nin;
    param[1]/=(gdouble)nout;

    param[0] = param[2] - param[1];
   
    err[0] = err[1] = err[2] = err[3] = err[4] = 0;
    *fres = TRUE;

}

static gdouble
func_lineheight(gdouble x, gint n_param, gdouble *param, gpointer user_data, gboolean *fres)
{
    if (x>param[3] && x<param[4]) return param[2];
    else return param[1];
}

/************************** presets ****************************/

static const GwyCDLineParam lineheight_pars[]= {
   {"h", " ", 1 },
   {"y<sub>1</sub>", " ", 2 },
   {"y<sub>2</sub>", " ", 2 },
   {"x<sub>1</sub>", " ", 3 },
   {"x<sub>2</sub>", " ", 4 },
};

static const GwyCDLineParam stepheight_pars[]= {
   {"h", " ", 1 },
   {"x", " ", 2 },
   {"y<sub>1</sub>", " ", 2 },
   {"y<sub>2</sub>", " ", 2 },
};


static const GwyCDLinePreset fitting_presets[] = {
    {
        "Step height (right)",
        "Step",
        "cd_step.png",
        &func_stepheight,
        &cd_ustepheight,
        4,
        stepheight_pars,
        NULL
    },
    {
        "Step height (left)",
        "Step",
        "cd_rstep.png",
        &func_stepheight,
        &cd_lstepheight,
        4,
        stepheight_pars,
        NULL
    }, 
    {
        "Line height (positive)",
        "Line",
        "cd_line.png",
        &func_lineheight,
        &cd_lineheight,
        5,
        lineheight_pars,
        NULL
    },
    {
        "Line height (negative)",
        "Line",
        "cd_rline.png",
        &func_lineheight,
        &cd_rlineheight,
        5,
        lineheight_pars,
        NULL
    },
};

/**
 * gwy_cdline_get_npresets:
 *
 * Returns the number of available critical dimension (CD)presets.
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
 * gwy_cdline_get_preset:
 * @preset_id: CD preset number.
 *
 * Returns: CD preset number @preset_id.
 *
 * Presets are numbered sequentially from 0 to gwy_cdline_get_npresets()-1.
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
 * gwy_cdline_get_preset_by_name:
 * @name: CDpreset name.
 *
 * Returns CD preset whose name is @name.
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
 * gwy_cdline_get_preset_id:
 * @preset: A CD preset.
 *
 * Returns the id of a CD preset.
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
 * gwy_cdline_get_preset_name:
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
 * gwy_cdline_get_preset_formula:
 * @preset: A CD preset.
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
 * gwy_cdline_get_preset_param_name:
 * @preset: A CD preset.
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
 * gwy_cdline_get_preset_param_default:
 * @preset: A CD preset.
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
 * gwy_cdline_get_preset_nparams:
 * @preset: A CD preset.
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
 * gwy_nlfit_fit_preset:
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
    gboolean fres;
    fres = TRUE;
    preset->function_fit(x, y, n_dat, param, err, user_data, &fres);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
