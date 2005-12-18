/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
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
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwynlfitpreset.h>
#include "gwyddioninternal.h"

typedef void (*GwyNLFitGuessFunc)(gint n_dat,
                                  const gdouble *x,
                                  const gdouble *y,
                                  gdouble *param,
                                  gboolean *fres);

typedef void (*GwyNLFitParamScaleFunc)(gdouble *param,
                                       gdouble xscale,
                                       gdouble yscale,
                                       gint dir);

typedef void (*GwyNLFitWeightFunc)(gint n_dat,
                                   const gdouble *x,
                                   const gdouble *y,
                                   gdouble *weight);

typedef struct {
    const char *name;
} GwyNLFitParam;

struct _GwyNLFitPresetBuiltin {
    const gchar *name;
    const gchar *formula;
    GwyNLFitFunc function;
    GwyNLFitDerFunc derive;
    GwyNLFitGuessFunc guess;
    GwyNLFitParamScaleFunc scale_params;
    GwyNLFitWeightFunc set_default_weights;
    gint nparams;
    const GwyNLFitParam *param;
};

static GwyNLFitPreset*
gwy_nlfit_preset_new_static(const GwyNLFitPresetBuiltin *data);

G_DEFINE_TYPE(GwyNLFitPreset, gwy_nlfit_preset, GWY_TYPE_RESOURCE)

/*********************** gaussian *****************************/
static gdouble
gauss_func(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    gdouble c;

    if (b[3] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = (x - b[0])/b[3];
    return b[2] * exp(-c*c/2) + b[1];
}

static void
gauss_guess(gint n_dat,
            const gdouble *x,
            const gdouble *y,
            gdouble *param,
            gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = G_MAXDOUBLE;
    param[2] = -G_MAXDOUBLE;
    for (i = 0; i < n_dat; i++) {
        param[0] += x[i]/(gdouble)n_dat;
        if (param[1] > y[i])
            param[1] = y[i];
        if (param[2] < y[i])
            param[2] = y[i];
    }
    param[2] -= param[1];

    param[3] = (x[n_dat-1] - x[0])/4;

    *fres = TRUE;
}

static void
gauss_scale(gdouble *param,
            gdouble xscale,
            gdouble yscale,
            gint dir)
{
    if (dir == 1) {
        param[0] /= xscale;
        param[1] /= yscale;
        param[2] /= yscale;
        param[3] /= xscale;

    }
    else {
        param[0] *= xscale;
        param[1] *= yscale;
        param[2] *= yscale;
        param[3] *= xscale;
    }

}

/******************** gaussian PSDF ***************************/
static gdouble
gauss_psdf_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    gdouble c;

    if (b[1] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = x*b[1];

    return b[0]*b[0]*b[1]/(2.0*GWY_SQRT_PI) * exp(-c*c/4);
}

static void
gauss_psdf_scale(gdouble *param,
                 gdouble xscale,
                 gdouble yscale,
                 gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale*xscale);
        param[1] *= xscale;
    }
    else {
        param[0] *= sqrt(yscale*xscale);
        param[1] /= xscale;
    }
}

static void
gauss_psdf_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;

    param[1] = 50/x[n_dat-1];

    param[0] = 0;
    for (i = 0; i < n_dat; i++)
        param[0] += x[1]*y[i];

    *fres = param[0] >= 0;
    param[0] = sqrt(param[0]);
}


/******************* gaussian HHCF ********************************/
static gdouble
gauss_hhcf_func(gdouble x,
                G_GNUC_UNUSED gint n_param,
                const gdouble *b,
                G_GNUC_UNUSED gpointer user_data,
                gboolean *fres)
{
    gdouble c;

    if (b[1] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = x/b[1];

    return 2*b[0]*b[0] * (1 - exp(-c*c));
}

static void
gauss_hhcf_guess(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;

    param[0] = 0;
    for (i = (n_dat/2); i < n_dat; i++) {
        param[0] += y[i]/(n_dat/2);
    }
    param[0] = sqrt(param[0]);
    param[1] = x[n_dat-1]/50;

    *fres = TRUE;
}

static void
gauss_hhcf_scale(gdouble *param,
                 gdouble xscale,
                 gdouble yscale,
                 gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}

/****************    gaussian ACF  *****************************************/
static gdouble
gauss_acf_func(gdouble x,
               G_GNUC_UNUSED gint n_param,
               const gdouble *b,
               G_GNUC_UNUSED gpointer user_data,
               gboolean *fres)
{
    gdouble c;

    if (b[1] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = x/b[1];

    return b[0]*b[0] * exp(-c*c);
}

static void
gauss_acf_guess(gint n_dat,
                const gdouble *x,
                const gdouble *y,
                gdouble *param,
                gboolean *fres)
{
    param[0] = sqrt(y[1]);
    param[1] = x[n_dat-1]/50;

    *fres = y[1] >= 0;
}

static void
gauss_acf_scale(gdouble *param,
                gdouble xscale,
                gdouble yscale,
                gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}


/**************** exponential ************************************/
static gdouble
exp_func(gdouble x,
         G_GNUC_UNUSED gint n_param,
         const gdouble *b,
         G_GNUC_UNUSED gpointer user_data,
         gboolean *fres)
{
    gdouble c;

    if (b[3] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = (x - b[0])/b[3];

    return b[2] * exp(-c/2) + b[1];
}

static void
exp_guess(gint n_dat,
          const gdouble *x,
          const gdouble *y,
          gdouble *param,
          gboolean *fres)
{
    gint i;

    param[0] = 0;
    param[1] = G_MAXDOUBLE;
    param[2] = -G_MAXDOUBLE;
    for (i = 0; i < n_dat; i++) {
        param[0] += x[i]/(gdouble)n_dat;
        if (param[1] > y[i])
            param[1] = y[i];
        if (param[2] < y[i])
            param[2] = y[i];
    }
    param[2] -= param[1];

    param[3] = (x[n_dat-1] - x[0])/4;

    *fres = TRUE;
}

static void
exp_scale(gdouble *param,
          gdouble xscale,
          gdouble yscale,
          gint dir)
{
    if (dir == 1) {
        param[0] /= xscale;
        param[1] /= yscale;
        param[2] /= yscale;
        param[3] /= xscale;

    }
    else {
        param[0] *= xscale;
        param[1] *= yscale;
        param[2] *= yscale;
        param[3] *= xscale;
    }

}

/**************** exponential PSDF **************************/
static gdouble
exp_psdf_func(gdouble x,
              G_GNUC_UNUSED gint n_param,
              const gdouble *b,
              G_GNUC_UNUSED gpointer user_data,
              gboolean *fres)
{
    gdouble c;

    if (b[1] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = x*b[1];

    return b[0]*b[0]*b[1]/(2.0*GWY_SQRT_PI)/(1.0 + c*c);
}

static void
exp_psdf_guess(gint n_dat,
               const gdouble *x,
               const gdouble *y,
               gdouble *param,
               gboolean *fres)
{
    gint i;

    param[1] = 50/x[n_dat-1];

    param[0] = 0;
    for (i = 0; i < n_dat; i++)
        param[0] += x[1]*y[i];

    *fres = param[0] >= 0;
    param[0] = sqrt(param[0]);
}

static void
exp_psdf_scale(gdouble *param,
               gdouble xscale,
               gdouble yscale,
               gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale*xscale);
        param[1] *= xscale;
    }
    else {
        param[0] *= sqrt(yscale*xscale);
        param[1] /= xscale;
    }
}

/***************** exponential HHCF ********************************/
static gdouble
exp_hhcf_func(gdouble x,
              G_GNUC_UNUSED gint n_param,
              const gdouble *b,
              G_GNUC_UNUSED gpointer user_data,
              gboolean *fres)
{
    gdouble c;

    if (b[1] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = x/b[1];

    return 2*b[0]*b[0] * (1 - exp(-c));
}

static void
exp_hhcf_guess(gint n_dat,
               const gdouble *x,
               const gdouble *y,
               gdouble *param,
               gboolean *fres)
{
    gint i;

    param[0] = 0;
    for (i = (n_dat/2); i < n_dat; i++)
        param[0] += y[i]/(n_dat/2);

    *fres = param[0] >= 0;
    param[0] = sqrt(param[0]);
    param[1] = x[n_dat-1]/50;
}

static void
exp_hhcf_scale(gdouble *param,
               gdouble xscale,
               gdouble yscale,
               gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}


/*************** exponential ACF ************************************/
static gdouble
exp_acf_func(gdouble x,
             G_GNUC_UNUSED gint n_param,
             const gdouble *b,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
    gdouble c;

    if (b[1] == 0) {
        *fres = FALSE;
        return 0;
    }
    *fres = TRUE;
    c = x/b[1];

    return b[0]*b[0] * exp(-c);
}

static void
exp_acf_guess(gint n_dat,
              const gdouble *x,
              const gdouble *y,
              gdouble *param,
              gboolean *fres)
{
    param[0] = sqrt(y[1]);
    param[1] = x[n_dat-1]/50;

    *fres = y[1] >= 0;
}

static void
exp_acf_scale(gdouble *param,
              gdouble xscale,
              gdouble yscale,
              gint dir)
{
    if (dir == 1) {
        param[0] /= sqrt(yscale);
        param[1] /= xscale;
    }
    else {
        param[0] *= sqrt(yscale);
        param[1] *= xscale;
    }
}


/**************   polynomial 0th order ********************************/
static gdouble
poly_0_func(G_GNUC_UNUSED gdouble x,
            G_GNUC_UNUSED gint n_param,
            const gdouble *b,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    *fres = TRUE;

    return b[0];
}

static void
poly_0_guess(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 0, param);
    *fres = TRUE;
}

static void
poly_0_scale(gdouble *param,
             G_GNUC_UNUSED gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1)
        param[0] /= yscale;
    else
        param[0] *= yscale;
}


/*************** polynomial 1st order ********************************/
static gdouble
poly_1_func(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*b[1];
}

static void
poly_1_guess(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 1, param);
    *fres = TRUE;
}

static void
poly_1_scale(gdouble *param,
             gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] /= yscale/xscale;
    }
    else {
        param[0] *= yscale;
        param[1] *= yscale/xscale;
    }
}

/************* polynomial 2nd order **********************************/
static gdouble
poly_2_func(gdouble x,
            G_GNUC_UNUSED gint n_param,
            const gdouble *b,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*(b[1] + x*b[2]);
}

static void
poly_2_guess(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 2, param);
    *fres = TRUE;
}

static void
poly_2_scale(gdouble *param,
             gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] /= yscale/xscale;
        param[2] /= yscale/xscale/xscale;
    }
    else {
        param[0] *= yscale;
        param[1] *= yscale/xscale;
        param[2] *= yscale/xscale/xscale;
    }
}

/************** polynomial 3rd order *****************************/
static gdouble
poly_3_func(gdouble x,
            G_GNUC_UNUSED gint n_param,
            const gdouble *b,
            G_GNUC_UNUSED gpointer user_data,
            gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*(b[1] + x*(b[2] + x*b[3]));
}

static void
poly_3_guess(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 3, param);
    *fres = TRUE;
}

static void
poly_3_scale(gdouble *param,
             gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] /= yscale/xscale;
        param[2] /= yscale/xscale/xscale;
        param[3] /= yscale/xscale/xscale/xscale;
    }
    else {
        param[0] *= yscale;
        param[1] *= yscale/xscale;
        param[2] *= yscale/xscale/xscale;
        param[3] *= yscale/xscale/xscale/xscale;
    }
}

/******************* square signal ********************************/
static gdouble
square_func(gdouble x,
            G_GNUC_UNUSED gint n_param,
            const gdouble *b,
            G_GNUC_UNUSED gpointer user_data,
            G_GNUC_UNUSED gboolean *fres)
{
    x = (x - b[1])/b[0];
    return (x - floor(x)) > 0.5 ? b[2] : b[3];
}

static void
square_guess(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gint i;
    gdouble min, max;

    param[0] = fabs(x[n_dat - 1] - x[0])/10.0;
    param[1] = 0;

    min = G_MAXDOUBLE;
    max = -G_MAXDOUBLE;
    for (i = 0; i < n_dat; i++) {
        if (min > y[i])
            min = y[i];
        if (max < y[i])
            max = y[i];
    }
    param[2] = min;
    param[3] = max;

    *fres = TRUE;
}

static void
square_scale(gdouble *param,
             gdouble xscale,
             gdouble yscale,
             gint dir)
{
    if (dir == 1) {
        param[0] /= xscale;
        param[1] /= xscale;
        param[2] /= yscale;
        param[3] /= yscale;
    }
    else {
        param[0] *= xscale;
        param[1] *= xscale;
        param[2] *= yscale;
        param[3] *= yscale;
    }
}

/******************* power function ********************************/
static gdouble
power_func(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    if (x < 0.0) {
        *fres = FALSE;
        return 0;
    }
    if (x == 0.0 && b[3] <= 0.0) {
        *fres = FALSE;
        return 0;
    }

    *fres = TRUE;
    return b[0] + b[1]*pow(x, b[2]);
}

static void
power_guess(gint n_dat,
            const gdouble *x,
            const gdouble *y,
            gdouble *param,
            gboolean *fres)
{
    gint i;
    gdouble q, la, lb, c1, c2;

    if (n_dat < 4 || x[0] <= 0.0) {
        *fres = FALSE;
        return;
    }

    i = ROUND(sqrt(n_dat-1));
    i = CLAMP(i, 2, n_dat-2);
    la = log(x[1]/x[i]);
    lb = log(x[n_dat-1]/x[i]);
    q = (y[n_dat-1] - y[i])/(y[i] - y[1]);
    if (fabs(q) < 0.4)
        param[2] = -2.0*(q*la + lb)/(q*la*la + lb*lb);
    else {
        c1 = log(q)/lb;
        c2 = -log(q)/la;
        if (c1 + c2 <= 0.0)
            param[2] = c2;
        else
            param[2] = c1;
    }

    q = (y[n_dat-1] - y[i])*(y[i] - y[1]);
    q /= (pow(x[n_dat - 1], param[2]) - pow(x[i], param[2]))
         *(pow(x[i], param[2]) - pow(x[1], param[2]));
    param[1] = sqrt(fabs(q));

    param[0] = (y[0] + y[i] + y[n_dat-1]
                - param[1]*(pow(x[0], param[2]) + pow(x[i], param[2])
                            + pow(x[n_dat-1], param[2])))/3.0;

    *fres = TRUE;
}

static void
power_scale(gdouble *param,
            gdouble xscale,
            gdouble yscale,
            gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] /= yscale/pow(xscale, param[2]);
    }
    else {
        param[0] *= yscale;
        param[1] *= yscale/pow(xscale, param[2]);
    }
}

/******************* lorentzian ********************************/
static gdouble
lorentz_func(gdouble x,
             G_GNUC_UNUSED gint n_param,
             const gdouble *b,
             G_GNUC_UNUSED gpointer user_data,
             gboolean *fres)
{
    *fres = TRUE;
    return b[0]/(b[1]*b[1] + (x - b[2])*(x - b[2]));
}

static void
lorentz_guess(gint n_dat,
              const gdouble *x,
              const gdouble *y,
              gdouble *param,
              gboolean *fres)
{
    gint i, imin, imax;
    gdouble c0, c2p, c2m;

    imin = imax = 0;
    for (i = 1; i < n_dat; i++) {
        if (G_UNLIKELY(y[imax] < y[i]))
            imax = i;
        if (G_UNLIKELY(y[imin] < y[i]))
            imin = i;
    }

    c0 = c2p = c2m = 0.0;
    for (i = 0; i < n_dat; i++) {
        c0 += y[i];
        c2p += y[i]*(x[i] - x[imax])*(x[i] - x[imax]);
        c2m += y[i]*(x[i] - x[imin])*(x[i] - x[imin]);
    }
    c0 *= (x[n_dat-1] - x[0])/n_dat;
    c2p *= (x[n_dat-1] - x[0])/n_dat;
    c2m *= (x[n_dat-1] - x[0])/n_dat;

    param[0] = c2p/(c0/y[imax] + x[n_dat-1] - x[0]);
    if (param[0]*y[imax] > 0.0) {
        param[1] = sqrt(param[0]/y[imax]);
        param[2] = x[imax];
    }
    else {
        param[0] = c2m/(c0/y[imin] + x[n_dat-1] - x[0]);
        if (param[0]*y[imin] > 0.0) {
            param[1] = sqrt(param[0]/y[imin]);
            param[2] = x[imin];
        }
        else {
            *fres = FALSE;
            return;
        }
    }

    *fres = TRUE;
}

static void
lorentz_scale(gdouble *param,
              gdouble xscale,
              gdouble yscale,
              gint dir)
{
    if (dir == 1) {
        param[0] /= yscale*xscale*xscale;
        param[1] /= xscale;
        param[2] /= xscale;
    }
    else {
        param[0] *= yscale*xscale*xscale;
        param[1] *= xscale;
        param[2] *= xscale;
    }
}

/******************* sinc ********************************/
static gdouble
sinc_func(gdouble x,
          G_GNUC_UNUSED gint n_param,
          const gdouble *b,
          G_GNUC_UNUSED gpointer user_data,
          gboolean *fres)
{
    *fres = TRUE;
    x *= b[1];
    if (x == 0.0)
        return b[0];
    return b[0]*sin(x)/x;
}

static void
sinc_guess(gint n_dat,
           const gdouble *x,
           const gdouble *y,
           gdouble *param,
           gboolean *fres)
{
    gint i;
    gdouble max, min, xmin;

    max = min = y[0];
    xmin = x[0];
    for (i = 1; i < n_dat; i++) {
        if (G_UNLIKELY(max < y[i]))
            max = y[i];
        if (G_UNLIKELY(min > y[i])) {
            min = y[i];
            xmin = x[i];
        }
    }

    param[1] = 4.493409457909064175307880927276/xmin;
    param[0] = max;
    *fres = TRUE;
}

static void
sinc_scale(gdouble *param,
           gdouble xscale,
           gdouble yscale,
           gint dir)
{
    if (dir == 1) {
        param[0] /= yscale;
        param[1] *= xscale;
    }
    else {
        param[0] *= yscale;
        param[1] /= xscale;
    }
}

/******************** preset default weights *************************/
static void
weights_linear_decrease(gint n_dat,
                        G_GNUC_UNUSED const gdouble *x,
                        G_GNUC_UNUSED const gdouble *y,
                        gdouble *weight)
{
    gint i;

    for (i = 0; i < n_dat; i++)
        weight[i] = 1 - (gdouble)i/(gdouble)n_dat;
}

/************************** presets ****************************/

static const GwyNLFitParam gaussexp_params[]= {
   { "x<sub>0</sub>" },
   { "y<sub>0</sub>" },
   { "a" },
   { "b" },
};

static const GwyNLFitParam gaussexp_two_params[]= {
   { "\xcf\x83" },
   { "T" },
};


static const GwyNLFitParam poly0_params[]= {
   { "a" },
};

static const GwyNLFitParam poly1_params[]= {
   { "a" },
   { "b" },
};

static const GwyNLFitParam poly2_params[]= {
   { "a" },
   { "b" },
   { "c" },
};

static const GwyNLFitParam poly3_params[]= {
   { "a" },
   { "b" },
   { "c" },
   { "d" },
};

static const GwyNLFitParam square_params[] = {
    { "T" },
    { "s" },
    { "t" },
    { "b" },
};

static const GwyNLFitPresetBuiltin fitting_presets[] = {
    {
        "Gaussian",
        "<i>f</i>(<i>x</i>) "
            "= <i>y</i><sub>0</sub> "
            "+ <i>a</i> exp[\xe2\x88\x92(<i>x</i> "
            "\xe2\x88\x92 <i>x</i><sub>0</sub>)<sup>2</sup>"
            "/b<sup>2</sup>]",
        &gauss_func,
        NULL,
        &gauss_guess,
        &gauss_scale,
        NULL,
        G_N_ELEMENTS(gaussexp_params),
        gaussexp_params,
    },
    {
        "Gaussian (PSDF)",
        "<i>f</i>(<i>x</i>) "
            "= \xcf\x83<sup>2</sup><i>T</i>/2 "
            "exp[\xe2\x88\x92(<i>x</i><i>T</i>/2)<sup>2</sup>]",
        &gauss_psdf_func,
        NULL,
        &gauss_psdf_guess,
        &gauss_psdf_scale,
        NULL,
        G_N_ELEMENTS(gaussexp_two_params),
        gaussexp_two_params,
    },
    {
        "Gaussian (ACF)",
        "<i>f</i>(<i>x</i>) "
            "= \xcf\x83<sup>2</sup> "
            "exp[\xe2\x88\x92(<i>x</i>/<i>T</i>)<sup>2</sup>]",
        &gauss_acf_func,
        NULL,
        &gauss_acf_guess,
        &gauss_acf_scale,
        &weights_linear_decrease,
        G_N_ELEMENTS(gaussexp_two_params),
        gaussexp_two_params,
    },
    {
        "Gaussian (HHCF)",
        "<i>f</i>(<i>x</i>) "
            "= 2\xcf\x83<sup>2</sup>"
            "[1 \xe2\x88\x92 exp(\xe2\x88\x92(<i>x</i>/<i>T</i>)<sup>2</sup>)]",
        &gauss_hhcf_func,
        NULL,
        &gauss_hhcf_guess,
        &gauss_hhcf_scale,
        &weights_linear_decrease,
        G_N_ELEMENTS(gaussexp_two_params),
        gaussexp_two_params,
    },
    {
        "Exponential",
        "<i>f</i>(<i>x</i>) "
            "= <i>y</i><sub>0</sub> "
            "+ <i>a</i> exp[\xe2\x88\x92<i>b</i>(<i>x</i> "
            "\xe2\x88\x92 <i>x</i><sub>0</sub>)]",
        &exp_func,
        NULL,
        &exp_guess,
        &exp_scale,
        NULL,
        G_N_ELEMENTS(gaussexp_params),
        gaussexp_params,
    },
    {
        "Exponential (PSDF)",
        "<i>f</i>(<i>x</i>) "
            "= \xcf\x83<sup>2</sup><i>T</i>"
            "/[2\xcf\x80(1 + (<i>x</i>/<i>T</i>)<sup>2</sup>)]",
        &exp_psdf_func,
        NULL,
        &exp_psdf_guess,
        &exp_psdf_scale,
        NULL,
        G_N_ELEMENTS(gaussexp_two_params),
        gaussexp_two_params,
    },
    {
        "Exponential (ACF)",
        "<i>f</i>(<i>x</i>) "
            "= \xcf\x83<sup>2</sup> exp[\xe2\x88\x92<i>x</i>/<i>T</i>]",
        &exp_acf_func,
        NULL,
        &exp_acf_guess,
        &exp_acf_scale,
        &weights_linear_decrease,
        G_N_ELEMENTS(gaussexp_two_params),
        gaussexp_two_params,
    },
    {
        "Exponential (HHCF)",
        "<i>f</i>(<i>x</i>) "
            "= 2\xcf\x83<sup>2</sup>"
            "[1 \xe2\x88\x92 exp(\xe2\x88\x92<i>x</i>/<i>T</i>)]",
        &exp_hhcf_func,
        NULL,
        &exp_hhcf_guess,
        &exp_hhcf_scale,
        &weights_linear_decrease,
        G_N_ELEMENTS(gaussexp_two_params),
        gaussexp_two_params,
    },
    {
        "Polynom (order 0)",
        "<i>f</i>(<i>x</i>) = <i>a</i>",
        &poly_0_func,
        NULL,
        &poly_0_guess,
        &poly_0_scale,
        NULL,
        G_N_ELEMENTS(poly0_params),
        poly0_params,
    },
    {
        "Polynom (order 1)",
        "<i>f</i>(<i>x</i>) = <i>a</i> + <i>b</i><i>x</i>",
        &poly_1_func,
        NULL,
        &poly_1_guess,
        &poly_1_scale,
        NULL,
        G_N_ELEMENTS(poly1_params),
        poly1_params,
    },
    {
        "Polynom (order 2)",
        "<i>f</i>(<i>x</i>) "
            "= <i>a</i> + <i>b</i><i>x</i> + <i>c</i><i>x</i><sup>2</sup>",
        &poly_2_func,
        NULL,
        &poly_2_guess,
        &poly_2_scale,
        NULL,
        G_N_ELEMENTS(poly2_params),
        poly2_params,
    },
    {
        "Polynom (order 3)",
        "<i>f</i>(<i>x</i>) "
            "= <i>a</i> + <i>b</i><i>x</i> + <i>c</i><i>x</i><sup>2</sup> "
            "+ <i>d</i><i>x</i><sup>3</sup>",
        &poly_3_func,
        NULL,
        &poly_3_guess,
        &poly_3_scale,
        NULL,
        G_N_ELEMENTS(poly3_params),
        poly3_params,
    },
    {
        "Square wave",
        "<i>f</i>(<i>x</i>) "
            "= (<i>t</i> + <i>b</i>)/2 "
            "+ 2(<i>t</i> \xe2\x88\x92 <i>b</i>)"
            "/\xcf\x80 "
            " \xe2\x88\x91<sub>k</sub> "
            "sin(2π<i>k</i>(<i>x</i> \xe2\x88\x92 <i>s</i>)/<i>T</i>)/<i>k</i>",
        &square_func,
        NULL,
        &square_guess,
        &square_scale,
        NULL,
        G_N_ELEMENTS(square_params),
        square_params,
    },
    {
        "Power",
        "<i>f</i>(<i>x</i>) "
            "= <i>a</i> + <i>b</i><i>x</i><sup><i>c</i></sup>",
        &power_func,
        NULL,
        &power_guess,
        &power_scale,
        NULL,
        G_N_ELEMENTS(poly2_params),
        poly2_params,
    },
    {
        "Lorentzian",
        "<i>f</i>(<i>x</i>) "
            "= <i>a</i>/[<i>b</i><sup>2</sup> "
            "+ (<i>x</i> \xe2\x88\x92 <i>c</i>)<sup>2</sup>]",
        &lorentz_func,
        NULL,
        &lorentz_guess,
        &lorentz_scale,
        NULL,
        G_N_ELEMENTS(poly2_params),
        poly2_params,
    },
    {
        "Sinc",
        "<i>f</i>(<i>x</i>) "
            "= <i>a</i> sinc(<i>b</i><i>x</i>)",
        &sinc_func,
        NULL,
        &sinc_guess,
        &sinc_scale,
        NULL,
        G_N_ELEMENTS(poly1_params),
        poly1_params,
    },
};

static void
gwy_nlfit_preset_class_init(GwyNLFitPresetClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    parent_class = GWY_RESOURCE_CLASS(gwy_nlfit_preset_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);
    /* TODO: override more methods */

    res_class->name = "nlfitpresets";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_forget_order(res_class->inventory);
    /*
    gwy_inventory_set_default_item_name(res_class->inventory,
                                        GWY_NLFIT_PRESET_DEFAULT);
    res_class->dump = gwy_nlfit_preset_dump;
    res_class->parse = gwy_nlfit_preset_parse;
                                        */
}

static void
gwy_nlfit_preset_init(GwyNLFitPreset *preset)
{
    gwy_debug_objects_creation(G_OBJECT(preset));
}

static GwyNLFitPreset*
gwy_nlfit_preset_new_static(const GwyNLFitPresetBuiltin *data)
{
    GwyNLFitPreset *preset;

    preset = g_object_new(GWY_TYPE_NLFIT_PRESET, "is-const", TRUE, NULL);
    preset->builtin = data;
    g_string_assign(GWY_RESOURCE(preset)->name, data->name);

    return preset;
}

void
_gwy_nlfit_preset_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyNLFitPreset *preset;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_NLFIT_PRESET);

    for (i = 0; i < G_N_ELEMENTS(fitting_presets); i++) {
        preset = gwy_nlfit_preset_new_static(fitting_presets + i);
        gwy_inventory_insert_item(klass->inventory, preset);
        g_object_unref(preset);
    }
    gwy_inventory_restore_order(klass->inventory);

    /* The presets added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

/**
 * gwy_nlfit_preset_get_value:
 * @x: The point to compute value at.
 * @preset: A NL fitter function preset.
 * @params: Preset parameter values.
 * @fres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Return preset function value in point @x with parameters @params.
 *
 * Returns: The function value.
 **/
gdouble
gwy_nlfit_preset_get_value(GwyNLFitPreset *preset,
                           gdouble x,
                           const gdouble *params,
                           gboolean *fres)
{
    /* FIXME: builtin */
    return preset->builtin->function(x, preset->builtin->nparams, params,
                                     NULL, fres);
}

/**
 * gwy_nlfit_preset_get_formula:
 * @preset: A NL fitter function preset.
 *
 * Returns function formula of @preset (with Pango markup).
 *
 * Returns: The preset function formula.
 **/
const gchar*
gwy_nlfit_preset_get_formula(GwyNLFitPreset* preset)
{
    /* FIXME: builtin */
    return preset->builtin->formula;
}

/**
 * gwy_nlfit_preset_get_param_name:
 * @preset: A NL fitter function preset.
 * @param: A parameter number.
 *
 * Returns the name of parameter number @param of preset @preset.
 *
 * The name may contain Pango markup.
 *
 * Returns: The name of parameter @param.
 **/
const gchar*
gwy_nlfit_preset_get_param_name(GwyNLFitPreset* preset,
                                gint param)
{
    const GwyNLFitParam *par;

    /* FIXME: builtin */
    g_return_val_if_fail(param >= 0 && param < preset->builtin->nparams, NULL);
    par = preset->builtin->param + param;

    return par->name;
}

/**
 * gwy_nlfit_preset_get_nparams:
 * @preset: A NL fitter function preset.
 *
 * Return the number of parameters of @preset.
 *
 * Returns: The number of function parameters.
 **/
gint
gwy_nlfit_preset_get_nparams(GwyNLFitPreset* preset)
{
    /* FIXME: builtin */
    return preset->builtin->nparams;
}

void
gwy_nlfit_preset_guess(GwyNLFitPreset *preset,
                       gint n_dat,
                       const gdouble *x,
                       const gdouble *y,
                       gdouble *params,
                       gboolean *fres)
{
    /* FIXME: builtin */
    preset->builtin->guess(n_dat, x, y, params, fres);
}

GwyNLFitter*
gwy_nlfit_preset_fit(GwyNLFitPreset *preset,
                     GwyNLFitter *fitter,
                     gint n_dat,
                     const gdouble *x,
                     const gdouble *y,
                     gdouble *param,
                     gdouble *err,
                     const gboolean *fixed_param)
{
    gdouble xscale, yscale;
    gdouble *weight = NULL;
    gdouble *xsc, *ysc;  /* rescaled x and y */
    gboolean ok;
    gint i;

    /* FIXME: builtin */
    /*use numerical derivation if necessary*/
    if (fitter) {
        /* XXX */
        g_return_val_if_fail(fitter->fmarq == preset->builtin->function, NULL);
    }
    else
        fitter = gwy_math_nlfit_new(preset->builtin->function,
                                    preset->builtin->derive
                                        ? preset->builtin->derive
                                        : gwy_math_nlfit_derive);

    /* recompute data to be reasonably scaled */
    xscale = 0;
    yscale = 0;
    for (i = 0; i < n_dat; i++) {
        xscale += fabs(x[i]);
        yscale += fabs(y[i]);
    }
    xscale /= n_dat;
    yscale /= n_dat;
    xsc = g_new(gdouble, n_dat);
    ysc = g_new(gdouble, n_dat);
    for (i = 0; i < n_dat; i++) {
        xsc[i] = x[i]/xscale;
        ysc[i] = y[i]/yscale;

    }
    /* FIXME: builtin */
    preset->builtin->scale_params(param, xscale, yscale, 1);

    /*load default weights for given function type*/
    if (preset->builtin->set_default_weights) {
        weight = g_new(gdouble, n_dat);
        preset->builtin->set_default_weights(n_dat, xsc, ysc, weight);
    }

    /* FIXME: builtin */
    ok = gwy_math_nlfit_fit_full(fitter, n_dat, xsc, ysc, weight,
                                 preset->builtin->nparams, param,
                                 fixed_param, NULL, preset) >= 0.0;

    if (ok && err) {
    /* FIXME: builtin */
        for (i = 0; i < preset->builtin->nparams; i++)
            err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }
    /*recompute parameters to be scaled as original data*/
    /* FIXME: builtin */
    preset->builtin->scale_params(param, xscale, yscale, -1);
    if (ok)
        preset->builtin->scale_params(err, xscale, yscale, -1);

    g_free(ysc);
    g_free(xsc);
    g_free(weight);

    return fitter;
}

/**
 * gwy_nlfit_presets:
 *
 * Gets inventory with all the NLFit presets.
 *
 * Returns: NLFit preset inventory.
 **/
GwyInventory*
gwy_nlfit_presets(void)
{
    return
        GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_NLFIT_PRESET))->inventory;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwynlfitpreset
 * @title: GwyNLFitPreset
 * @short_description: NL fitter presets
 * @see_also: #GwyNLFitter
 **/

/**
 * GWY_SQRT3:
 *
 * The square root of 3.
 **/

/**
 * GWY_SQRT_PI:
 *
 * The square root of &pi;.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
