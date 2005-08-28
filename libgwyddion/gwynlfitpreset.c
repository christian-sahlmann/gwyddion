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
#include <libgwyddion/gwynlfitpreset.h>

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

struct _GwyNLFitPreset {
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

/*********************** gaussian *****************************/
static gdouble
fit_gauss(gdouble x,
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
guess_gauss(gint n_dat,
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
scale_gauss(gdouble *param,
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
fit_gauss_psdf(gdouble x,
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

    return b[0]*b[0]*b[1]/(2.0*sqrt(G_PI)) * exp(-c*c/4);
}

static void
scale_gauss_psdf(gdouble *param,
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
guess_gauss_psdf(gint n_dat,
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
fit_gauss_hhcf(gdouble x,
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
guess_gauss_hhcf(gint n_dat,
                 const gdouble *x,
                 const gdouble *y,
                 gdouble *param,
                 gboolean *fres)
{
    gint i;

    param[0] = 0;
    for (i = (n_dat/2); i < n_dat; i++)
    {
        param[0] += y[i]/(n_dat/2);
    }
    param[0] = sqrt(param[0]);
    param[1] = x[n_dat-1]/50;

    *fres = TRUE;
}

static void
scale_gauss_hhcf(gdouble *param,
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
fit_gauss_acf(gdouble x,
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
guess_gauss_acf(gint n_dat,
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
scale_gauss_acf(gdouble *param,
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
fit_exp(gdouble x,
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
guess_exp(gint n_dat,
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
scale_exp(gdouble *param,
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
fit_exp_psdf(gdouble x,
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
    c = x*b[1];

    return b[0]*b[0]*b[1]/(2.0*sqrt(G_PI)) / (1 + c*c);
}

static void
guess_exp_psdf(gint n_dat,
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
scale_exp_psdf(gdouble *param,
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
fit_exp_hhcf(gdouble x,
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
guess_exp_hhcf(gint n_dat,
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
scale_exp_hhcf(gdouble *param,
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
fit_exp_acf(gdouble x,
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
guess_exp_acf(gint n_dat,
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
scale_exp_acf(gdouble *param,
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
fit_poly_0(G_GNUC_UNUSED gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0];
}

static void
guess_poly_0(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 0, param);
    *fres = TRUE;
}

static void
scale_poly_0(gdouble *param,
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
fit_poly_1(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*b[1];
}

static void
guess_poly_1(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 1, param);
    *fres = TRUE;
}

static void
scale_poly_1(gdouble *param,
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
fit_poly_2(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*(b[1] + x*b[2]);
}

static void
guess_poly_2(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 2, param);
    *fres = TRUE;
}

static void
scale_poly_2(gdouble *param,
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
fit_poly_3(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           gboolean *fres)
{
    *fres = TRUE;

    return b[0] + x*(b[1] + x*(b[2] + x*b[3]));
}

static void
guess_poly_3(gint n_dat,
             const gdouble *x,
             const gdouble *y,
             gdouble *param,
             gboolean *fres)
{
    gwy_math_fit_polynom(n_dat, x, y, 3, param);
    *fres = TRUE;
}

static void
scale_poly_3(gdouble *param,
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
fit_square(gdouble x,
           G_GNUC_UNUSED gint n_param,
           const gdouble *b,
           G_GNUC_UNUSED gpointer user_data,
           G_GNUC_UNUSED gboolean *fres)
{
    /* FIXME: fix this to the square wave the sum converges to */
    gint i;
    gdouble val, amplitude, shift;

    amplitude = (b[3] - b[2])/1.6;
    shift = b[2];
    val = 0;
    for (i = 1; i < 20;) {

        val += (1.0/i) * sin(2.0 * i * G_PI * (x - b[1])/b[0]);
        i += 2;
    }

    return amplitude * val + (b[3] - b[2])/2 + shift;
}

static void
guess_square(gint n_dat,
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
scale_square(gdouble *param,
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

static const GwyNLFitParam gaussexp_pars[]= {
   { "x<sub>0</sub>" },
   { "y<sub>0</sub>" },
   { "a" },
   { "b" },
};

static const GwyNLFitParam gaussexp_two_pars[]= {
   { "\xcf\x83" },
   { "T" },
};


static const GwyNLFitParam poly0_pars[]= {
   { "a" },
};

static const GwyNLFitParam poly1_pars[]= {
   { "a" },
   { "b" },
};

static const GwyNLFitParam poly2_pars[]= {
   { "a" },
   { "b" },
   { "c" },
};

static const GwyNLFitParam poly3_pars[]= {
   { "a" },
   { "b" },
   { "c" },
   { "d" },
};

static const GwyNLFitParam square_pars[] = {
    { "T" },
    { "s" },
    { "y<sub>1</sub>" },
    { "y<sub>2</sub>" },
};

static const GwyNLFitPreset fitting_presets[] = {
    {
        "Gaussian",
        "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>))<sup>2</sup>)",
        &fit_gauss,
        NULL,
        &guess_gauss,
        &scale_gauss,
        NULL,
        4,
        gaussexp_pars,
    },
    {
        "Gaussian (PSDF)",
        "f(x) = (\xcf\x83<sup>2</sup>T)/(2)*exp(-(x*T/2)<sup>2</sup>)",
        &fit_gauss_psdf,
        NULL,
        &guess_gauss_psdf,
        &scale_gauss_psdf,
        NULL,
        2,
        gaussexp_two_pars,
    },
    {
        "Gaussian (ACF)",
        "f(x) = \xcf\x83<sup>2</sup>exp(-(x/T)<sup>2</sup>)",
        &fit_gauss_acf,
        NULL,
        &guess_gauss_acf,
        &scale_gauss_acf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
    },
    {
        "Gaussian (HHCF)",
        "f(x) =  2*\xcf\x83<sup>2</sup>(1 - exp(-(x/T)<sup>2</sup>))",
        &fit_gauss_hhcf,
        NULL,
        &guess_gauss_hhcf,
        &scale_gauss_hhcf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
    },
    {
        "Exponential",
        "f(x) = y<sub>0</sub> + a*exp(-(b*(x-x<sub>0</sub>)))",
        &fit_exp,
        NULL,
        &guess_exp,
        &scale_exp,
        NULL,
        4,
        gaussexp_pars,
    },
    {
        "Exponential (PSDF)",
        "f(x) = (\xcf\x83<sup>2</sup>T)/(2)/(1+((x/T)<sup>2</sup>))))",
        &fit_exp_psdf,
        NULL,
        &guess_exp_psdf,
        &scale_exp_psdf,
        NULL,
        2,
        gaussexp_two_pars,
    },
    {
        "Exponential (ACF)",
        "f(x) = \xcf\x83<sup>2</sup>exp(-(x/T))",
        &fit_exp_acf,
        NULL,
        &guess_exp_acf,
        &scale_exp_acf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
    },
    {
        "Exponential (HHCF)",
        "f(x) =  2*\xcf\x83<sup>2</sup>(1 - exp(-(x/T)))",
        &fit_exp_hhcf,
        NULL,
        &guess_exp_hhcf,
        &scale_exp_hhcf,
        &weights_linear_decrease,
        2,
        gaussexp_two_pars,
    },
    {
        "Polynom (order 0)",
        "f(x) = a",
        &fit_poly_0,
        NULL,
        &guess_poly_0,
        &scale_poly_0,
        NULL,
        1,
        poly0_pars,
    },
    {
        "Polynom (order 1)",
        "f(x) = a + b*x",
        &fit_poly_1,
        NULL,
        &guess_poly_1,
        &scale_poly_1,
        NULL,
        2,
        poly1_pars,
    },
    {
        "Polynom (order 2)",
        "f(x) = a + b*x + c*x<sup>2</sup>",
        &fit_poly_2,
        NULL,
        &guess_poly_2,
        &scale_poly_2,
        NULL,
        3,
        poly2_pars,
    },
    {
        "Polynom (order 3)",
        "f(x) = a + b*x + c*x<sup>2</sup> + d*x<sup>3</sup>",
        &fit_poly_3,
        NULL,
        &guess_poly_3,
        &scale_poly_3,
        NULL,
        4,
        poly3_pars,
    },
    {
        "Square wave",
        "f(x) = sum{(1/i) * sin(2*Pi*(i+s)*/T)}",
        &fit_square,
        NULL,
        &guess_square,
        &scale_square,
        NULL,
        4,
        square_pars,
    },
};

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
    return preset->function(x, preset->nparams, params, NULL, fres);
}

/* XXX */
const gchar*
_gwy_nlfit_preset_get_name(GwyNLFitPreset* preset)
{
    return preset->name;
}

/**
 * gwy_math_nlfit_get_preset_formula:
 * @preset: A NL fitter function preset.
 *
 * Returns function formula of @preset (with Pango markup).
 *
 * Returns: The preset function formula.
 **/
const gchar*
gwy_nlfit_preset_get_formula(GwyNLFitPreset* preset)
{
    return preset->formula;
}

/**
 * gwy_math_nlfit_get_preset_param_name:
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

    g_return_val_if_fail(param >= 0 && param < preset->nparams, NULL);
    par = preset->param + param;

    return par->name;
}

/**
 * gwy_math_nlfit_get_preset_nparams:
 * @preset: A NL fitter function preset.
 *
 * Return the number of parameters of @preset.
 *
 * Returns: The number of function parameters.
 **/
gint
gwy_nlfit_preset_get_nparams(GwyNLFitPreset* preset)
{
    return preset->nparams;
}

void
gwy_nlfit_preset_guess(GwyNLFitPreset *preset,
                       gint n_dat,
                       const gdouble *x,
                       const gdouble *y,
                       gdouble *params,
                       gboolean *fres)
{
    preset->guess(n_dat, x, y, params, fres);
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

    /*use numerical derivation if necessary*/
    if (fitter) {
        /* XXX */
        g_return_val_if_fail(fitter->fmarq == preset->function, NULL);
    }
    else
        fitter = gwy_math_nlfit_new(preset->function,
                                    preset->derive ? preset->derive
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
    preset->scale_params(param, xscale, yscale, 1);

    /*load default weights for given function type*/
    if (preset->set_default_weights) {
        weight = g_new(gdouble, n_dat);
        preset->set_default_weights(n_dat, xsc, ysc, weight);
    }

    ok = gwy_math_nlfit_fit_full(fitter, n_dat, xsc, ysc, weight,
                                 preset->nparams, param,
                                 fixed_param, NULL, preset) >= 0.0;

    if (ok && err) {
        for (i = 0; i < preset->nparams; i++)
            err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }
    /*recompute parameters to be scaled as original data*/
    preset->scale_params(param, xscale, yscale, -1);
    if (ok)
        preset->scale_params(err, xscale, yscale, -1);

    g_free(ysc);
    g_free(xsc);
    g_free(weight);

    return fitter;
}

static const gchar*
gwy_nlfit_preset_get_item_name(gpointer item)
{
    return ((GwyNLFitPreset*)item)->name;
}


static const GType*
gwy_nlfit_preset_get_traits(gint *ntraits)
{
    static const GType traits_types[] = { G_TYPE_STRING };

    if (ntraits)
        *ntraits = G_N_ELEMENTS(traits_types);

    return traits_types;
}

static const gchar*
gwy_nlfit_preset_get_trait_name(gint i)
{
    static const gchar* trait_names[] = { "name" };

    g_return_val_if_fail(i >= 0 && i < G_N_ELEMENTS(trait_names), NULL);
    return trait_names[i];
}

static void
gwy_nlfit_preset_get_trait_value(gpointer item,
                                 gint i,
                                 GValue *value)
{
    GwyNLFitPreset *preset = (GwyNLFitPreset*)item;

    switch (i) {
        case 0:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_string(value, preset->name);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/**
 * gwy_nlfit_presets:
 *
 * Gets inventory with all the nlfit_presets.
 *
 * Returns: Gradient inventory.
 **/
GwyInventory*
gwy_nlfit_presets(void)
{
    static GwyInventory *inventory = NULL;

    if (!inventory) {
        GwyInventoryItemType nlfit_preset_item_type = {
            0,
            NULL,
            NULL,
            &gwy_nlfit_preset_get_item_name,
            NULL,
            NULL,
            NULL,
            NULL,
            &gwy_nlfit_preset_get_traits,
            &gwy_nlfit_preset_get_trait_name,
            &gwy_nlfit_preset_get_trait_value,
        };
        inventory = gwy_inventory_new_from_array(&nlfit_preset_item_type,
                                                 sizeof(GwyNLFitPreset),
                                                 G_N_ELEMENTS(fitting_presets),
                                                 fitting_presets);
    }
    return inventory;
    /*return GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_NLFIT_PRESET))->inventory;*/
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
