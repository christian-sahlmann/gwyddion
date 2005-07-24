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
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/dataline.h>
#include <libprocess/cdline.h>

/* INTERPOLATION: New (not applicable). */

/* Preset */
struct _GwyCDLineParam {
    const char *name;
    const char *unit;
    double default_init;
};

static void
get_linestatpars(const gdouble *y, gint ndat, gint from, gint to, gdouble *avg,
                 gdouble *sigma)
{
    gint i, n;

    if (from > to)
        GWY_SWAP(gint, from, to);

    *avg = 0;
    *sigma = 0;

    from = CLAMP(from, 0, ndat);
    to = CLAMP(to, 0, ndat);

    n = to - from;
    if (n <= 0)
        return;

    for (i = from; i < to; i++) {
        *avg += y[i];
        *sigma += y[i] * y[i];
    }

    *sigma = sqrt(fabs(*sigma - (*avg) * (*avg)/n)/n);
    *avg /= n;
}

static void
cd_uedgeheight(const gdouble *x,
               const gdouble *y,
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

    nstep = n_dat/20;
    iwidth = n_dat/8;
    if (nstep < 1)
        nstep = 1;

    max = -G_MAXDOUBLE;
    imax = nstep/2;
    for (i = nstep; i < (n_dat - 2 * nstep); i++) {
        val = ((y[i + nstep] - y[i])/(x[i + nstep] - x[i]));
        if (max < val) {
            max = val;
            imax = i + nstep/2;
            param[1] = (x[i + nstep] + x[i])/2.0;
        }
    }

    get_linestatpars(y, n_dat, 0, imax - iwidth/2, param + 2, err + 2);
    get_linestatpars(y, n_dat, imax + iwidth/2, n_dat, param + 3, err + 3);

    param[0] = param[3] - param[2];
    err[0] = sqrt(err[2] * err[2] + err[3] * err[3]);
    err[1] = -1;

    *fres = TRUE;

}

static void
cd_ledgeheight(const gdouble *x,
               const gdouble *y,
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

    nstep = n_dat/20;
    iwidth = n_dat/8;
    if (nstep < 1)
        nstep = 1;

    min = G_MAXDOUBLE;
    imin = nstep/2;
    for (i = nstep; i < (n_dat - 2 * nstep); i++) {
        val = ((y[i + nstep] - y[i])/(x[i + nstep] - x[i]));
        if (min > val) {
            min = val;
            imin = i + nstep/2;
            param[1] = (x[i + nstep] + x[i])/2.0;
        }
    }

    get_linestatpars(y, n_dat, 0, imin - iwidth/2, param + 2, err + 2);
    get_linestatpars(y, n_dat, imin + iwidth/2, n_dat, param + 3, err + 3);

    param[0] = param[3] - param[2];

    err[0] = sqrt(err[2] * err[2] + err[3] * err[3]);
    err[1] = -1;
    *fres = TRUE;

}

static gdouble
func_edgeheight(gdouble x,
                G_GNUC_UNUSED gint n_param,
                gdouble *param,
                G_GNUC_UNUSED gpointer user_data,
                G_GNUC_UNUSED gboolean *fres)
{
    if (x < param[1])
        return param[2];
    else
        return param[3];
}


static void
cd_rstepheight(const gdouble *x,
               const gdouble *y,
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
    gint nout;


    nstep = n_dat/20;
    if (nstep < 1)
        nstep = 1;

    max = -G_MAXDOUBLE;
    min = G_MAXDOUBLE;

    imax = imin = nstep/2;
    for (i = nstep; i < (n_dat - 2 * nstep); i++) {
        val = ((y[i + nstep] - y[i])/(x[i + nstep] - x[i]));
        if (min > val) {
            min = val;
            imin = i + nstep/2;
            param[3] = (x[i + nstep] + x[i])/2.0;
        }
    }

    for (i = imin; i < (n_dat - 2 * nstep); i++) {
        val = ((y[i + nstep] - y[i])/(x[i + nstep] - x[i]));
        if (max < val) {
            max = val;
            imax = i + nstep/2;
            param[4] = (x[i + nstep] + x[i])/2.0;
        }
    }
    iwidth = imax - imin;

    /*FIXME modidfied now (imin+iwidth/3, imax-iwidth/3) */
    get_linestatpars(y, n_dat, imin + iwidth/3, imax - iwidth/3, param + 2,
                     err + 2);

    param[1] = err[1] = 0;
    nout = 0;
    for (i = 0; i < n_dat; i++) {
        if ((i < (imin - iwidth/3) && i > (imin - iwidth))    /* /3 */
            ||(i > (imax + iwidth/3) && i < (imax + iwidth))) {       /* /3 */
            param[1] += y[i];
            err[1] += y[i] * y[i];
            nout++;
        }
    }

    err[1] = sqrt(fabs(err[1] - param[1] * param[1]/nout)/nout);
    param[1] /= (gdouble)nout;

    param[0] = param[2] - param[1];

    err[0] = sqrt(err[2] * err[2] + err[1] * err[1]);
    err[3] = err[4] = -1;
    *fres = TRUE;

}

static void
cd_stepheight(const gdouble *x,
              const gdouble *y,
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
    gint nout;


    nstep = n_dat/20;
    if (nstep < 1)
        nstep = 1;

    max = -G_MAXDOUBLE;
    min = G_MAXDOUBLE;
    imax = imin = nstep/2;

    for (i = nstep; i < (n_dat - 2 * nstep); i++) {
        val = ((y[i + nstep] - y[i])/(x[i + nstep] - x[i]));
        if (max < val) {
            max = val;
            imax = i + nstep/2;
            param[3] = (x[i + nstep] + x[i])/2.0;
        }
    }

    for (i = imax; i < (n_dat - 2 * nstep); i++) {
        val = ((y[i + nstep] - y[i])/(x[i + nstep] - x[i]));
        if (min > val) {
            min = val;
            imin = i + nstep/2;
            param[4] = (x[i + nstep] + x[i])/2.0;
        }
    }
    iwidth = imin - imax;

    /*FIXME: modified now (imax+iwidth/3, imin-iwidth/3) */
    get_linestatpars(y, n_dat, imax + iwidth/3, imin - iwidth/3, param + 2,
                     err + 2);

    param[1] = err[1] = 0;
    nout = 0;
    for (i = 0; i < n_dat; i++) {
        if ((i < (imax - iwidth/3) && i > (imax - iwidth))    /* /3 */
            ||(i > (imin + iwidth/3) && i < (imin + iwidth))) {       /* /3 */
            param[1] += y[i];
            err[1] += y[i] * y[i];
            nout++;
        }
    }
    err[1] = sqrt(fabs(err[1] - param[1] * param[1]/nout)/nout);
    param[1] /= (gdouble)nout;

    param[0] = param[2] - param[1];

    err[0] = sqrt(err[2] * err[2] + err[1] * err[1]);
    err[3] = err[4] = -1;

    *fres = TRUE;

}

static gdouble
func_stepheight(gdouble x,
                G_GNUC_UNUSED gint n_param,
                gdouble *param,
                G_GNUC_UNUSED gpointer user_data,
                G_GNUC_UNUSED gboolean *fres)
{
    if (x > param[3] && x < param[4])
        return param[2];
    else
        return param[1];
}

/************************** presets ****************************/

static const GwyCDLineParam stepheight_pars[]= {
   {"h", " ", 1 },
   {"y<sub>1</sub>", " ", 2 },
   {"y<sub>2</sub>", " ", 2 },
   {"x<sub>1</sub>", " ", 3 },
   {"x<sub>2</sub>", " ", 4 },
};

static const GwyCDLineParam edgeheight_pars[]= {
   {"h", " ", 1 },
   {"x", " ", 2 },
   {"y<sub>1</sub>", " ", 2 },
   {"y<sub>2</sub>", " ", 2 },
};


static const GwyCDLinePreset fitting_presets[] = {
    {
        "Edge height (right)",
        "Edge",
        "cd_step.png",
        &func_edgeheight,
        &cd_uedgeheight,
        4,
        edgeheight_pars,
        NULL
    },
    {
        "Edge height (left)",
        "Edge",
        "cd_rstep.png",
        &func_edgeheight,
        &cd_ledgeheight,
        4,
        edgeheight_pars,
        NULL
    },
    {
        "Step height (positive)", /*ISO 5436*/
        "Line",
        "cd_line.png",
        &func_stepheight,
        &cd_stepheight,
        5,
        stepheight_pars,
        NULL
    },
    {
        "Step height (negative)",
        "Line",
        "cd_rline.png",
        &func_stepheight,
        &cd_rstepheight,
        5,
        stepheight_pars,
        NULL
    },
};

/**
 * gwy_cdline_get_npresets:
 *
 * Returns the number of available critical dimension (CD)presets.
 *
 * Returns: The number of presets.
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
 **/
G_CONST_RETURN GwyCDLinePreset*
gwy_cdline_get_preset_by_name(const gchar *name)
{
    gsize i;

    for (i = 0; i < G_N_ELEMENTS(fitting_presets); i++) {
        if (gwy_strequal(name, fitting_presets[i].function_name))
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
 **/
void
gwy_cdline_fit_preset(const GwyCDLinePreset* preset,
                      gint n_dat, const gdouble *x, const gdouble *y,
                      G_GNUC_UNUSED gint n_param,
                      gdouble *param, gdouble *err,
                      G_GNUC_UNUSED const gboolean *fixed_param,
                      gpointer user_data)
{
    gboolean fres;
    fres = TRUE;
    preset->function_fit(x, y, n_dat, param, err, user_data, &fres);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
