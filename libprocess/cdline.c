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
#include <libprocess/cdline.h>
#include <libgwyddion/gwydebugobjects.h>

typedef gdouble (*GwyCDLineFitFunc)(gdouble x,
                                    gint n_param,
                                    gdouble *param,
                                    gpointer user_data,
                                    gboolean *fres);

typedef void (*GwyCDLineCDFunc)(const gdouble *x,
                                const gdouble *y,
                                gint n_dat,
                                gdouble *param,
                                gdouble *err,
                                gpointer user_data,
                                gboolean *fres);


typedef struct {
    const char *name;
    const char *unit;
    double default_init;
} GwyCDLineParam;


struct _GwyCDLinePresetBuiltin {
    const gchar *function_name;
    const gchar *group_name;
    const gchar *function_formula;
    GwyCDLineFitFunc function;
    GwyCDLineCDFunc function_fit;
    gint nparams;
    const GwyCDLineParam *param;
};



static GwyCDLinePreset*
gwy_cdline_preset_new_static(const GwyCDLinePresetBuiltin *data);

G_DEFINE_TYPE(GwyCDLinePreset, gwy_cdline_preset, GWY_TYPE_RESOURCE)




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


static const GwyCDLinePresetBuiltin fitting_presets[] = {
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
 * gwy_cdline_get_preset_name:
 * @preset: A NL fitter function preset.
 *
 * Return preset name (its unique identifier).
 *
 * Returns: The preset name.
 **/
const gchar*
gwy_cdline_preset_get_name(const GwyCDLinePreset* preset)
{
    return preset->builtin->function_name;
}

/**
 * gwy_cdline_get_preset_formula:
 * @preset: A CD preset.
 *
 * Returns function formula of @preset (with Pango markup).
 *
 * Returns: The preset function formula.
 **/
const gchar*
gwy_cdline_preset_get_formula(const GwyCDLinePreset* preset)
{
    return preset->builtin->function_formula;
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
const gchar*
gwy_cdline_preset_get_param_name(const GwyCDLinePreset* preset,
                                     gint param)
{
    const GwyCDLineParam *par;

    g_return_val_if_fail(param >= 0 && param < preset->builtin->nparams, NULL);
    par = preset->builtin->param + param;

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
gwy_cdline_preset_get_param_default(const GwyCDLinePreset* preset,
                                        gint param)
{
    const GwyCDLineParam *par;

    g_return_val_if_fail(param >= 0 && param < preset->builtin->nparams, G_MAXDOUBLE);
    par = preset->builtin->param + param;

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
gwy_cdline_preset_get_nparams(const GwyCDLinePreset* preset)
{
    return preset->builtin->nparams;
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
    preset->builtin->function_fit(x, y, n_dat, param, err, user_data, &fres);
}


static void
gwy_cdline_preset_class_init(GwyCDLinePresetClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    parent_class = GWY_RESOURCE_CLASS(gwy_cdline_preset_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);

    res_class->name = "cdlinepresets";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_forget_order(res_class->inventory);
}

static void
gwy_cdline_preset_init(GwyCDLinePreset *preset)
{
    gwy_debug_objects_creation(G_OBJECT(preset));
}

static GwyCDLinePreset*
gwy_cdline_preset_new_static(const GwyCDLinePresetBuiltin *data)
{
    GwyCDLinePreset *preset;

    preset = g_object_new(GWY_TYPE_CDLINE_PRESET, "is-const", TRUE, NULL);
    preset->builtin = data;
    g_string_assign(GWY_RESOURCE(preset)->name, data->function_name);

    return preset;
}

void
_gwy_cdline_preset_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyCDLinePreset *preset;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_CDLINE_PRESET);

    for (i = 0; i < G_N_ELEMENTS(fitting_presets); i++) {
        preset = gwy_cdline_preset_new_static(fitting_presets + i);
        gwy_inventory_insert_item(klass->inventory, preset);
        g_object_unref(preset);
    }
    gwy_inventory_restore_order(klass->inventory);

    /* The presets added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}



/**
 * gwy_cdline_presets:
 *
 * Gets inventory with all the CDLine presets.
 *
 * Returns: CDLine preset inventory.
 **/
GwyInventory*
gwy_cdline_presets(void)
{
    return
        GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_CDLINE_PRESET))->inventory;
}



/************************** Documentation ****************************/

/**
 * SECTION:cdline
 * @title: cdline
 * @short_description: Critical dimension
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
