/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
 *  Copyright (C) 2004,2005 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_NLFIT_H__
#define __GWY_NLFIT_H__

#include <glib.h>

G_BEGIN_DECLS

typedef  gdouble (*GwyNLFitFunc)(gdouble x,
                                 gint n_param,
                                 const gdouble *param,
                                 gpointer user_data,
                                 gboolean *fres);

typedef  void (*GwyNLFitDerFunc)(gdouble x,
                                 gint n_param,
                                 const gdouble *param,
                                 const gboolean *fixed_param,
                                 GwyNLFitFunc fmarq,
                                 gpointer user_data,
                                 gdouble *deriv,
                                 gboolean *dres);

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

GwyNLFitter*    gwy_math_nlfit_new               (GwyNLFitFunc ff,
                                                  GwyNLFitDerFunc df);
void            gwy_math_nlfit_free              (GwyNLFitter *nlfit);
gdouble         gwy_math_nlfit_fit               (GwyNLFitter *nlfit,
                                                  gint n_dat,
                                                  const gdouble *x,
                                                  const gdouble *y,
                                                  gint n_param,
                                                  gdouble *param,
                                                  gpointer user_data);
gdouble         gwy_math_nlfit_fit_full          (GwyNLFitter *nlfit,
                                                  gint n_dat,
                                                  const gdouble *x,
                                                  const gdouble *y,
                                                  const gdouble *weight,
                                                  gint n_param,
                                                  gdouble *param,
                                                  const gboolean *fixed_param,
                                                  const gint *link_map,
                                                  gpointer user_data);
gint            gwy_math_nlfit_get_max_iterations(GwyNLFitter *nlfit);
void            gwy_math_nlfit_set_max_iterations(GwyNLFitter *nlfit,
                                                  gint maxiter);
gdouble         gwy_math_nlfit_get_dispersion    (GwyNLFitter *nlfit);
gdouble         gwy_math_nlfit_get_correlations  (GwyNLFitter *nlfit,
                                                  gint par1,
                                                  gint par2);
gdouble         gwy_math_nlfit_get_sigma         (GwyNLFitter *nlfit,
                                                  gint par);

void            gwy_math_nlfit_derive            (gdouble x,
                                                  gint n_param,
                                                  const gdouble *param,
                                                  const gboolean *fixed_param,
                                                  GwyNLFitFunc ff,
                                                  gpointer user_data,
                                                  gdouble *deriv,
                                                  gboolean *dres);

G_END_DECLS

#endif /* __GWY_NFLIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
