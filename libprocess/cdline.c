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
#include "gwyprocessinternal.h"
#include <libgwyddion/gwynlfit.h>

typedef gdouble (*GwyCDLineFitFunc)(gdouble x,
                                    gint n_param,
                                    const gdouble *param,
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
    gint power_x;
    gint power_y;
    double default_init;
} GwyCDLineParam;

struct _GwyCDLineBuiltin {
    const gchar *function_name;
    const gchar *group_name;
    const gchar *function_definition;
    GwyCDLineFitFunc function;
    GwyCDLineCDFunc function_fit;
    gint nparams;
    const GwyCDLineParam *param;
};

static GwyCDLine*
gwy_cdline_new_static(const GwyCDLineBuiltin *data);

G_DEFINE_TYPE(GwyCDLine, gwy_cdline, GWY_TYPE_RESOURCE)

static void
get_linestatpars(const gdouble *y, gint ndat,
                 gint from, gint to,
                 gdouble *avg, gdouble *sigma)
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
    err[0] = hypot(err[2], err[3]);
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

    param[0] = param[2] - param[3];

    err[0] = hypot(err[2], err[3]);
    err[1] = -1;
    *fres = TRUE;
}

static gdouble
func_edgeheight(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *param,
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
            || (i > (imax + iwidth/3) && i < (imax + iwidth))) {       /* /3 */
            param[1] += y[i];
            err[1] += y[i] * y[i];
            nout++;
        }
    }

    err[1] = sqrt(fabs(err[1] - param[1] * param[1]/nout)/nout);
    param[1] /= (gdouble)nout;

    param[0] = param[2] - param[1];

    err[0] = hypot(err[2], err[1]);
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
            || (i > (imin + iwidth/3) && i < (imin + iwidth))) {       /* /3 */
            param[1] += y[i];
            err[1] += y[i] * y[i];
            nout++;
        }
    }
    err[1] = sqrt(fabs(err[1] - param[1] * param[1]/nout)/nout);
    param[1] /= (gdouble)nout;

    param[0] = param[2] - param[1];

    err[0] = hypot(err[2], err[1]);
    err[3] = err[4] = -1;

    *fres = TRUE;
}

static gdouble
func_stepheight(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *param,
                G_GNUC_UNUSED gpointer user_data,
                G_GNUC_UNUSED gboolean *fres)
{
    if (x > param[3] && x < param[4])
        return param[2];
    else
        return param[1];
}

static gdouble
    func_circle_down(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *param,
                G_GNUC_UNUSED gpointer user_data,
                G_GNUC_UNUSED gboolean *fres)
{
  if ( (param[0]*param[0] - (x-param[1])*(x-param[1])) > 0.0)
     return param[2] - sqrt(param[0]*param[0] - (x-param[1])*(x-param[1]));
  else
     return param[2];
}

static void
cd_circle_down(const gdouble *x,
               const gdouble *y,
               gint n_dat,
               gdouble *param,
               gdouble *err,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
  gdouble x1, x2, x3, y11, y2, y3, rx, ry, res; /* y1 already used somewhere... */
  gint i, m;
  GwyNLFitter *fitter;
  GwyNLFitFunc ff;
  gdouble par_init[3];
  gdouble par_res[3];
  gboolean par_fix[3];

  m = n_dat/2;
  if (x[0] != x[n_dat-1]){
     x1 = x[0]; y11 = y[0];
     x2 = x[m]; y2 = y[m];
     x3 = x[n_dat-1]; y3 = y[n_dat-1];
  }
  else{
     x1 = x[m]; y11 = y[m];
     x2 = x[n_dat-1]; y2 = y[n_dat-1];
     x3 = x[0]; y3 = y[0];
  }
  ry = ( (x1-x3)*((x2-x1)*(x2-x1)+(y2-y11)*(y2-y11)) + (x2-x1)*((x3-x1)*(x3-x1)+(y3-y11)*(y3-y11)) )/
      ( 2*((x2-x1)*(y3-y11)-(x3-x1)*(y2-y11)) );
  rx = ( (x3-x1)*(x3-x1)+(y3-y11)*(y3-y11)+2*ry*(y3-y11) )/(-2*(x3-x1));
  par_init[0] = sqrt(rx*rx+ry*ry); /*r*/
  par_init[1] = x1-rx; /*x0*/
  par_init[2] = y11+ry; /*y0*/
  for (i=0;i<3;i++){
      par_fix[i] = FALSE;
      par_res[i] = par_init[i];
  }
  ff = func_circle_down;
  fitter = gwy_math_nlfit_new(ff, gwy_math_nlfit_derive);
  res = gwy_math_nlfit_fit_full(fitter, n_dat, x, y, NULL, 3, par_res, par_fix, NULL, NULL);
  if (res > 0.0){
    if (par_res[0] < 0.0)
       par_res[0] *= -1;
    for (i=0;i<3;i++){
      param[i] = par_res[i];
      err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }
    *fres = TRUE;
  }
  else
    *fres = FALSE;
}

static gdouble
    func_circle_up(gdouble x,
                   G_GNUC_UNUSED gint n_param,
                   const gdouble *param,
                   G_GNUC_UNUSED gpointer user_data,
                   G_GNUC_UNUSED gboolean *fres)
{
  if ( (param[0]*param[0] - (x-param[1])*(x-param[1])) > 0.0)
    return param[2] + sqrt(param[0]*param[0] - (x-param[1])*(x-param[1]));
  else
    return param[2];
}

static void
cd_circle_up(const gdouble *x,
             const gdouble *y,
             gint n_dat,
             gdouble *param,
             gdouble *err,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
  gdouble x1, x2, x3, y11, y2, y3, rx, ry, res;
  gint i, m;
  GwyNLFitter *fitter;
  GwyNLFitFunc ff;
  gdouble par_init[3];
  gdouble par_res[3];
  gboolean par_fix[3];

  m = n_dat/2;
  if (x[0] != x[n_dat-1]){
      x1 = x[0]; y11 = y[0];
      x2 = x[m]; y2 = y[m];
      x3 = x[n_dat-1]; y3 = y[n_dat-1];
  }
  else{
    x1 = x[m]; y11 = y[m];
    x2 = x[n_dat-1]; y2 = y[n_dat-1];
    x3 = x[0]; y3 = y[0];
  }

  ry = ( (x1-x3)*((x2-x1)*(x2-x1)+(y2-y11)*(y2-y11)) + (x2-x1)*((x3-x1)*(x3-x1)+(y3-y11)*(y3-y11)) )/
        ( 2*((x2-x1)*(y3-y11)-(x3-x1)*(y2-y11)) );
  rx = ( (x3-x1)*(x3-x1)+(y3-y11)*(y3-y11)+2*ry*(y3-y11) )/(-2*(x3-x1));
  par_init[0] = sqrt(rx*rx+ry*ry); /*r*/
  par_init[1] = x1-rx; /*x0*/
  par_init[2] = y11+ry; /*y0*/
  for (i=0;i<3;i++){
    par_fix[i] = FALSE;
    par_res[i] = par_init[i];
  }
  ff = func_circle_up;
  fitter = gwy_math_nlfit_new(ff, gwy_math_nlfit_derive);
  res = gwy_math_nlfit_fit_full(fitter, n_dat, x, y, NULL, 3, par_res, par_fix, NULL, NULL);
  if (res > 0.0){
    for (i=0;i<3;i++){
      param[i] = par_res[i];
      err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }
    *fres = TRUE;
  }
  else
    *fres = FALSE;
}

/************************** cdlines ****************************/

static const GwyCDLineParam stepheight_pars[] = {
   { "h",             0, 1, 1, },
   { "y<sub>1</sub>", 0, 1, 2, },
   { "y<sub>2</sub>", 0, 1, 2, },
   { "x<sub>1</sub>", 1, 0, 3, },
   { "x<sub>2</sub>", 1, 0, 4, },
};

static const GwyCDLineParam edgeheight_pars[] = {
   { "h",             0, 1, 1, },
   { "x",             1, 0, 2, },
   { "y<sub>1</sub>", 0, 1, 2, },
   { "y<sub>2</sub>", 0, 1, 2, },
};

static const GwyCDLineParam circle_pars[] = {
  { "r",             0, 1, 1, },
  { "x<sub>0</sub>", 1, 0, 2, },
  { "y<sub>0</sub>", 0, 1, 2, },
};

static const GwyCDLineBuiltin cdlines[] = {
    {
        N_("Edge height (right)"),
        N_("Edge"),
        "cd_step.png",
        &func_edgeheight,
        &cd_uedgeheight,
        G_N_ELEMENTS(edgeheight_pars),
        edgeheight_pars
    },
    {
        N_("Edge height (left)"),
        N_("Edge"),
        "cd_rstep.png",
        &func_edgeheight,
        &cd_ledgeheight,
        G_N_ELEMENTS(edgeheight_pars),
        edgeheight_pars
    },
    {
        N_("Step height (positive)"),  /* ISO 5436 */
        N_("Line"),
        "cd_line.png",
        &func_stepheight,
        &cd_stepheight,
        G_N_ELEMENTS(stepheight_pars),
        stepheight_pars
    },
    {
        N_("Step height (negative)"),
        N_("Line"),
        "cd_rline.png",
        &func_stepheight,
        &cd_rstepheight,
        G_N_ELEMENTS(stepheight_pars),
        stepheight_pars
    },
    {
        N_("Circle (down)"),
        N_("Circle"),
        "circle_down.png",
        &func_circle_down,
        &cd_circle_down,
        G_N_ELEMENTS(circle_pars),
        circle_pars
    },
    {
        N_("Circle (up)"),
        N_("Circle"),
        "circle_up.png",
        &func_circle_up,
        &cd_circle_up,
        G_N_ELEMENTS(circle_pars),
        circle_pars
    },
};

/**
 * gwy_cdline_get_name:
 * @cdline: A critical dimension evaluator.
 *
 * Return cdline name (its unique identifier).
 *
 * Returns: The cdline name.
 **/
const gchar*
gwy_cdline_get_name(GwyCDLine* cdline)
{
    g_return_val_if_fail(GWY_IS_CDLINE(cdline), "");
    return cdline->builtin->function_name;
}

/**
 * gwy_cdline_get_definition:
 * @cdline: A critical dimension evaluator.
 *
 * Gets the name of the image file with critical dimension evaluator
 * description.
 *
 * Returns: The cdline function definition.
 **/
const gchar*
gwy_cdline_get_definition(GwyCDLine* cdline)
{
    g_return_val_if_fail(GWY_IS_CDLINE(cdline), "");
    return cdline->builtin->function_definition;
}

/**
 * gwy_cdline_get_param_name:
 * @cdline: A NL evaluator function cdline.
 * @param: A parameter number.
 *
 * Returns the name of a critical dimension evaluator parameter.
 *
 * The name may contain Pango markup.
 *
 * Returns: The name of parameter @param.
 **/
const gchar*
gwy_cdline_get_param_name(GwyCDLine* cdline,
                                     gint param)
{
    const GwyCDLineParam *par;

    g_return_val_if_fail(GWY_IS_CDLINE(cdline), "");
    g_return_val_if_fail(param >= 0 && param < cdline->builtin->nparams, NULL);
    par = cdline->builtin->param + param;

    return par->name;
}

/**
 * gwy_cdline_get_param_default:
 * @cdline: A NL evaluator function cdline.
 * @param: A parameter number.
 *
 * Returns a constant default parameter value.
 *
 * Returns: The default parameter value, unrelated to the actual data fitted.
 *          It is worthless.
 **/
gdouble
gwy_cdline_get_param_default(GwyCDLine* cdline,
                             gint param)
{
    const GwyCDLineParam *par;

    g_return_val_if_fail(GWY_IS_CDLINE(cdline), 0.0);
    g_return_val_if_fail(param >= 0 && param < cdline->builtin->nparams,
                         G_MAXDOUBLE);
    par = cdline->builtin->param + param;

    return par->default_init;
}

/**
 * gwy_cdline_get_param_units:
 * @cdline: A critical dimension evaluator.
 * @param: A parameter number.
 * @siunit_x: SI unit of abscissa.
 * @siunit_y: SI unit of ordinate.
 *
 * Derives the SI unit of a critical dimension parameter from the units of
 * abscissa and ordinate.
 *
 * Returns: A newly created #GwySIUnit with the units of the parameter @param.
 *          If the units of @param are not representable as #GwySIUnit,
 *          the result is unitless (i.e. it will be presented as a mere
 *          number).
 *
 * Since: 2.5
 **/
GwySIUnit*
gwy_cdline_get_param_units(GwyCDLine *cdline,
                           gint param,
                           GwySIUnit *siunit_x,
                           GwySIUnit *siunit_y)
{
    const GwyCDLineParam *par;

    g_return_val_if_fail(GWY_IS_CDLINE(cdline), NULL);
    g_return_val_if_fail(param >= 0 && param < cdline->builtin->nparams, NULL);
    par = cdline->builtin->param + param;

    return gwy_si_unit_power_multiply(siunit_x, par->power_x,
                                      siunit_y, par->power_y,
                                      NULL);
}

/**
 * gwy_cdline_get_nparams:
 * @cdline: A critical dimension evaluator.
 *
 * Return the number of parameters of @cdline.
 *
 * Returns: The number of function parameters.
 **/
gint
gwy_cdline_get_nparams(GwyCDLine* cdline)
{
    g_return_val_if_fail(GWY_IS_CDLINE(cdline), 0);
    return cdline->builtin->nparams;
}

/**
 * gwy_cdline_get_value:
 * @cdline: A critical dimension evaluator.
 * @x: The point to compute value at.
 * @params: Evaluator parameter values.
 * @fres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Calculates critical dimension function value in a single point with given
 * parameters.
 *
 * Returns: The function value.
 **/
gdouble
gwy_cdline_get_value(GwyCDLine *cdline,
                     gdouble x,
                     const gdouble *params,
                     gboolean *fres)
{
    return cdline->builtin->function(x, cdline->builtin->nparams, params,
                                     NULL, fres);
}

/**
 * gwy_cdline_fit:
 * @cdline: A critical dimension evaluator.
 * @n_dat: The number of data points (number of items in @x and @y).
 * @x: Abscissa points.
 * @y: Ordinate points.
 * @n_param: The number of parameters.  This argument is ignored as the
 *           evaluator knows how many parameters it has, it is safe to pass 0.
 * @params: Array to store fitted parameter values to.
 * @err: Array to store parameter errros to, may be %NULL.
 * @fixed_param: Which parameters should be treated as fixed.  It is ignored,
 *               pass %NULL.
 * @user_data: Ignored, pass %NULL.
 *
 * Performs a critical dimension evaulation (fit).
 **/
void
gwy_cdline_fit(GwyCDLine* cdline,
               gint n_dat,
               const gdouble *x,
               const gdouble *y,
               G_GNUC_UNUSED gint n_param,
               gdouble *params,
               gdouble *err,
               G_GNUC_UNUSED const gboolean *fixed_param,
               G_GNUC_UNUSED gpointer user_data)
{
    gboolean fres;

    g_return_if_fail(GWY_IS_CDLINE(cdline));
    fres = TRUE;
    cdline->builtin->function_fit(x, y, n_dat, params, err, NULL, &fres);
}

static void
gwy_cdline_class_init(GwyCDLineClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    parent_class = GWY_RESOURCE_CLASS(gwy_cdline_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);

    res_class->name = "cdlines";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_forget_order(res_class->inventory);
}

static void
gwy_cdline_init(GwyCDLine *cdline)
{
    gwy_debug_objects_creation(G_OBJECT(cdline));
}

static GwyCDLine*
gwy_cdline_new_static(const GwyCDLineBuiltin *data)
{
    GwyCDLine *cdline;

    cdline = g_object_new(GWY_TYPE_CDLINE, "is-const", TRUE, NULL);
    cdline->builtin = data;
    g_string_assign(GWY_RESOURCE(cdline)->name, data->function_name);

    return cdline;
}

void
_gwy_cdline_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyCDLine *cdline;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_CDLINE);

    for (i = 0; i < G_N_ELEMENTS(cdlines); i++) {
        cdline = gwy_cdline_new_static(cdlines + i);
        gwy_inventory_insert_item(klass->inventory, cdline);
        g_object_unref(cdline);
    }
    gwy_inventory_restore_order(klass->inventory);

    /* The cdlines added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

/**
 * gwy_cdlines:
 *
 * Gets inventory with all the critical dimension evaluators.
 *
 * Returns: Critical dimension evaluator inventory.
 **/
GwyInventory*
gwy_cdlines(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_CDLINE))->inventory;
}

/************************** Documentation ****************************/

/**
 * SECTION:cdline
 * @title: cdline
 * @short_description: Critical dimension
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
