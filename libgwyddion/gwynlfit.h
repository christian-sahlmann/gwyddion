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

typedef  gdouble (*GwyNLFitFunc)(gdouble x,
                                 gint n_par,
                                 gdouble *param,
                                 gpointer user_data,
                                 gboolean *fres);

typedef  void (*GwyNLFitDerFunc)(gint i,
                                 gdouble *x,
                                 gint n_par,
                                 gdouble *param,
                                 GwyNLFitFunc fmarq,
                                 gpointer user_data,
                                 gdouble *deriv,
                                 gboolean *dres);

typedef struct _GwyNLFitState GwyNLFitState;

struct _GwyNLFitState {
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

GwyNLFitState* gwy_math_nlfit_new                (GwyNLFitFunc ff,
                                                  GwyNLFitDerFunc df);
void           gwy_math_nlfit_free               (GwyNLFitState *nlfit);
gdouble        gwy_math_nlfit_fit                (GwyNLFitState *nlfit,
                                                  gint n_dat,
                                                  gdouble *x,
                                                  gdouble *y,
                                                  gdouble *weight,
                                                  gint n_par,
                                                  gdouble *param,
                                                  gpointer user_data);
gint           gwy_math_nlfit_get_max_iterations (GwyNLFitState *nlfit);
void           gwy_math_nlfit_set_max_iterations (GwyNLFitState *nlfit,
                                                  gint maxiter);
gdouble        gwy_math_nlfit_get_dispersion     (GwyNLFitState *nlfit);
gdouble        gwy_math_nlfit_get_correlations   (GwyNLFitState *nlfit,
                                                  gint par1,
                                                  gint par2);
gdouble        gwy_math_nlfit_get_sigma          (GwyNLFitState *nlfit,
                                                  gint par);

void           gwy_math_nlfit_derive             (gint i,
                                                  gdouble *x,
                                                  gint n_par,
                                                  gdouble *param,
                                                  GwyNLFitFunc ff,
                                                  gpointer user_data,
                                                  gdouble *deriv,
                                                  gboolean *dres);
#endif /* __GWY_NFLIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
