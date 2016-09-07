/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
 *  Copyright (C) 2004-2016 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef __GWY_NLFIT_H__
#define __GWY_NLFIT_H__

#include <glib.h>
#include <libgwyddion/gwyutils.h>

G_BEGIN_DECLS

typedef  gdouble (*GwyNLFitFunc)(gdouble x,
                                 gint nparam,
                                 const gdouble *param,
                                 gpointer user_data,
                                 gboolean *success);

typedef  void (*GwyNLFitDerFunc)(gdouble x,
                                 gint nparam,
                                 const gdouble *param,
                                 const gboolean *fixed_param,
                                 GwyNLFitFunc func,
                                 gpointer user_data,
                                 gdouble *der,
                                 gboolean *success);

typedef  gdouble (*GwyNLFitIdxFunc)(guint i,
                                    const gdouble *param,
                                    gpointer user_data,
                                    gboolean *success);

typedef  void (*GwyNLFitIdxDiffFunc)(guint i,
                                     const gdouble *param,
                                     const gboolean *fixed_param,
                                     GwyNLFitIdxFunc func,
                                     gpointer user_data,
                                     gdouble *der,
                                     gboolean *success);

typedef struct _GwyNLFitter GwyNLFitter;

struct _GwyNLFitter {
    GwyNLFitFunc fmarq;  /* fitting function */
    GwyNLFitDerFunc dmarq;  /* fitting function derivations */
    gint maxiter;  /* max number of iterations */
    gboolean eval;  /* success? */
    gdouble *covar; /* covariance matrix  */
    gdouble dispersion; /* dispersion */
    gdouble mfi;    /* fitting parameters --
                       fi, decrease, increase of lambda, minimum lambda */
    gdouble mdec;
    gdouble minc;
    gdouble mtol;
};

GwyNLFitter*    gwy_math_nlfit_new               (GwyNLFitFunc func,
                                                  GwyNLFitDerFunc diff);
GwyNLFitter*    gwy_math_nlfit_new_idx           (GwyNLFitIdxFunc func,
                                                  GwyNLFitIdxDiffFunc diff);
void            gwy_math_nlfit_free              (GwyNLFitter *nlfit);
gdouble         gwy_math_nlfit_fit               (GwyNLFitter *nlfit,
                                                  gint ndata,
                                                  const gdouble *x,
                                                  const gdouble *y,
                                                  gint nparam,
                                                  gdouble *param,
                                                  gpointer user_data);
gdouble         gwy_math_nlfit_fit_full          (GwyNLFitter *nlfit,
                                                  gint ndata,
                                                  const gdouble *x,
                                                  const gdouble *y,
                                                  const gdouble *weight,
                                                  gint nparam,
                                                  gdouble *param,
                                                  const gboolean *fixed_param,
                                                  const gint *link_map,
                                                  gpointer user_data);
gdouble         gwy_math_nlfit_fit_idx           (GwyNLFitter *nlfit,
                                                  guint ndata,
                                                  guint nparam,
                                                  gdouble *param,
                                                  gpointer user_data);
gdouble         gwy_math_nlfit_fit_idx_full      (GwyNLFitter *nlfit,
                                                  guint ndata,
                                                  guint nparam,
                                                  gdouble *param,
                                                  const gboolean *fixed_param,
                                                  const gint *link_map,
                                                  gpointer user_data);
gint            gwy_math_nlfit_get_max_iterations(GwyNLFitter *nlfit);
void            gwy_math_nlfit_set_max_iterations(GwyNLFitter *nlfit,
                                                  gint maxiter);
gboolean        gwy_math_nlfit_succeeded         (GwyNLFitter *nlfit);
gdouble         gwy_math_nlfit_get_dispersion    (GwyNLFitter *nlfit);
gdouble         gwy_math_nlfit_get_correlations  (GwyNLFitter *nlfit,
                                                  gint par1,
                                                  gint par2);
gdouble         gwy_math_nlfit_get_sigma         (GwyNLFitter *nlfit,
                                                  gint par);
void            gwy_math_nlfit_set_callbacks     (GwyNLFitter *nlfit,
                                                  GwySetFractionFunc set_fraction,
                                                  GwySetMessageFunc set_message);

void            gwy_math_nlfit_diff              (gdouble x,
                                                  gint nparam,
                                                  const gdouble *param,
                                                  const gboolean *fixed_param,
                                                  GwyNLFitFunc func,
                                                  gpointer user_data,
                                                  gdouble *der,
                                                  gboolean *success);
void            gwy_math_nlfit_derive            (gdouble x,
                                                  gint nparam,
                                                  const gdouble *param,
                                                  const gboolean *fixed_param,
                                                  GwyNLFitFunc func,
                                                  gpointer user_data,
                                                  gdouble *der,
                                                  gboolean *success);
void            gwy_math_nlfit_diff_idx          (guint i,
                                                  gint nparam,
                                                  const gdouble *param,
                                                  const gboolean *fixed_param,
                                                  GwyNLFitIdxFunc func,
                                                  gpointer user_data,
                                                  gdouble *der,
                                                  gboolean *success);

G_END_DECLS

#endif /* __GWY_NFLIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
