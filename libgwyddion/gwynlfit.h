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

#ifndef __GWY_NLFIT_H__
#define __GWY_NLFIT_H__

G_BEGIN_DECLS

#include <glib.h>

typedef  gdouble (*GwyNLFitFunc)(gdouble x,
                                 gint n_par,
                                 gdouble *params,
                                 gpointer user_data,
                                 gboolean *fres);

typedef  void (*GwyNLFitDerFunc)(gint i,
                                 gdouble *dat_x,
                                 gint n_par,
                                 gdouble *params,
                                 GwyNLFitFunc fmarq,
                                 gpointer user_data,
                                 gdouble *deriv,
                                 gboolean *dres);

typedef struct _GwyNLFitState GwyNLFitState;

struct _GwyNLFitState {
    GwyNLFitFunc fmarq;  /*fitovaci funkce*/
    GwyNLFitDerFunc   dmarq;  /*derivacni funkce*/
    gint maxiter; /*maximalne iteraci*/
    gboolean eval;  /*uspesnost vyhodnoceni*/
    gdouble *covar; /*kovariancni matice*/
    gdouble dispersion; /*disperze = rezidualni soucet/(data-parametry)*/
    gdouble mfi;    /*parametry fitovani -- fi, snizeni, zvyseni lambda, minimalni lambda*/
    gdouble mdec;
    gdouble minc;
    gdouble mtol;
};

gdouble gwy_math_nlfit_fit(GwyNLFitState *ms,
                           gint n_dat,
                           gdouble *x,
                           gdouble *y,
                           gdouble *weight,
                           gint n_par,
                           gdouble *param,
                           gpointer user_data);

void gwy_math_nlfit_derive(gint i,
                           gdouble *x,
                           gint n_par,
                           gdouble *param,
                           GwyNLFitFunc ff,
                           gpointer user_data,
                           gdouble *deriv,
                           gboolean *dres);

GwyNLFitState* gwy_math_nlfit_new(GwyNLFitFunc ff,
                                  GwyNLFitDerFunc df);

void           gwy_math_nlfit_free(GwyNLFitState *ms);

/* Pocita korelacni koeficient mezi parametry
 * jen pokud je nastavena korelacni matice a disperze
 */
gdouble gwy_math_nlfit_get_correlations(GwyNLFitState *ms,
                                        gint par1,
                                        gint par2);

/* Pocita chybu parametru
 * Jen pokud je nastavena korelacni matice
 */
gdouble gwy_math_nlfit_get_sigma(GwyNLFitState *ms, gint par);

#endif /* __GWY_NFLIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
