/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
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

/*
 *  Clean-up, rewrite from C++, and modification for Gwyddion by David
 *  Necas (Yeti), 2004.
 *  E-mail: yeti@gwyddion.net.
 */

#ifndef __GWY_NLFIT_H__
#define __GWY_NLFIT_H__

G_BEGIN_DECLS

#include <glib.h>

typedef enum {
    GWY_NLFIT_PRESET_GAUSSIAN          = 0,
    GWY_NLFIT_PRESET_GAUSSIAN_PSDF     = 1,
    GWY_NLFIT_PRESET_GAUSSIAN_ACF      = 2,
    GWY_NLFIT_PRESET_GAUSSIAN_HHCF     = 3,
    GWY_NLFIT_PRESET_EXPONENTIAL       = 4,
    GWY_NLFIT_PRESET_EXPONENTIAL_PSDF  = 5,
    GWY_NLFIT_PRESET_EXPONENTIAL_ACF   = 6,
    GWY_NLFIT_PRESET_EXPONENTIAL_HHCF  = 7,
    GWY_NLFIT_PRESET_POLY_0            = 8,
    GWY_NLFIT_PRESET_POLY_1            = 9,
    GWY_NLFIT_PRESET_POLY_2            = 10,
    GWY_NLFIT_PRESET_POLY_3            = 11
} GwyNLFitPresetType;


typedef  gdouble (*GwyNLFitFunc)(gdouble x,
                                 gint n_param,
                                 const gdouble *param,
                                 gpointer user_data,
                                 gboolean *fres);

typedef  void (*GwyNLFitDerFunc)(gint i,
                                 const gdouble *x,
                                 gint n_param,
                                 gdouble *param,
                                 GwyNLFitFunc fmarq,
                                 gpointer user_data,
                                 gdouble *deriv,
                                 gboolean *dres);

typedef void (*GwyNLFitGuessFunc)(gdouble *x,
                                  gdouble *y,
                                  gint n_dat,
                                  gdouble *param,
                                  gpointer user_data,
                                  gboolean *fres
                                  );

typedef void (*GwyNLFitParamScaleFunc)(gdouble *param,
                                         gdouble xscale,
                                         gdouble yscale,
                                         gint dir);

typedef void (*GwyNLFitWeightFunc)(gdouble *x,
                                   gdouble *y,
                                   gint n_dat,
                                   gdouble *weight,
                                   gpointer user_data);

typedef struct {
    const char *name;
    const char *unit;
    double default_init;
} GwyNLFitParam;

typedef struct {
    gchar *function_name;
    gchar *function_equation;
    GwyNLFitFunc function;
    GwyNLFitDerFunc function_derivation;
    GwyNLFitGuessFunc function_guess;
    GwyNLFitParamScaleFunc parameter_scale;
    GwyNLFitWeightFunc set_default_weights;
    gint nparams;
    const GwyNLFitParam *param;
} GwyNLFitPresetFunction;

typedef struct _GwyNLFitter GwyNLFitter;

struct _GwyNLFitter {
    GwyNLFitFunc fmarq;  /* fitting function */
    GwyNLFitDerFunc dmarq;  /* fitting function derivations */
    gint maxiter;  /* max number of iterations */
    gboolean eval;  /* success? */
    gdouble *covar; /* covariance matrix  */
    gdouble dispersion; /* dispersion */
    gdouble mfi;    /* fitting parameters --
                       fi, snizeni, zvyseni lambda, minimalni lambda */
    gdouble mdec;
    gdouble minc;
    gdouble mtol;
};

GwyNLFitter*   gwy_math_nlfit_new                (GwyNLFitFunc ff,
                                                  GwyNLFitDerFunc df);
void           gwy_math_nlfit_free               (GwyNLFitter *nlfit);
gdouble        gwy_math_nlfit_fit                (GwyNLFitter *nlfit,
                                                  gint n_dat,
                                                  const gdouble *x,
                                                  const gdouble *y,
                                                  const gdouble *weight,
                                                  gint n_param,
                                                  gdouble *param,
                                                  gpointer user_data);
gdouble        gwy_math_nlfit_fit_with_fixed     (GwyNLFitter *nlfit,
                                                  gint n_dat,
                                                  const gdouble *x,
                                                  const gdouble *y,
                                                  const gdouble *weight,
                                                  gint n_param,
                                                  gdouble *param,
                                                  const gboolean *fixed_param,
                                                  gpointer user_data);
gint           gwy_math_nlfit_get_max_iterations (GwyNLFitter *nlfit);
void           gwy_math_nlfit_set_max_iterations (GwyNLFitter *nlfit,
                                                  gint maxiter);
gdouble        gwy_math_nlfit_get_dispersion     (GwyNLFitter *nlfit);
gdouble        gwy_math_nlfit_get_correlations   (GwyNLFitter *nlfit,
                                                  gint par1,
                                                  gint par2);
gdouble        gwy_math_nlfit_get_sigma          (GwyNLFitter *nlfit,
                                                  gint par);

void           gwy_math_nlfit_derive             (gint i,
                                                  const gdouble *x,
                                                  gint n_param,
                                                  gdouble *param,
                                                  GwyNLFitFunc ff,
                                                  gpointer user_data,
                                                  gdouble *deriv,
                                                  gboolean *dres);

const GwyNLFitPresetFunction* gwy_math_nlfit_get_preset(GwyNLFitPresetType type)
    G_GNUC_CONST;

gdouble gwy_math_nlfit_get_function_value(GwyNLFitPresetFunction* function,
                                          gdouble *params,
                                          gdouble x);

gchar *gwy_math_nlfit_get_function_name(GwyNLFitPresetFunction* function);

gchar *gwy_math_nlfit_get_function_equation(GwyNLFitPresetFunction* function);

gchar *gwy_math_nlfit_get_function_param_name(GwyNLFitPresetFunction* function,
                                              gint param);

gdouble gwy_math_nlfit_get_function_param_default(GwyNLFitPresetFunction* function,
                                                  gint param);

gint gwy_math_nlfit_get_function_nparams(GwyNLFitPresetFunction* function);

GwyNLFitter* gwy_math_nlfit_fit_preset(GwyNLFitPresetFunction* function,
                               gint n_dat,
                               const gdouble *x,
                               const gdouble *y,
                               gint n_param,
                               gdouble *param,
                               gdouble *err,
                               const gboolean *fixed_param,
                               gpointer user_data);
  
#endif /* __GWY_NFLIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
