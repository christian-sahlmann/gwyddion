/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*< private_header >*/

#ifndef __GWY_INTERNAL_H__
#define __GWY_INTERNAL_H__

G_BEGIN_DECLS

typedef void (*GwyNLFitGuessFunc)(gint n_dat,
                                  const gdouble *x,
                                  const gdouble *y,
                                  gdouble *param,
                                  gboolean *fres);

typedef void (*GwyNLFitParamScaleFunc)(GwyNLFitPreset *preset,
                                       gdouble *param,
                                       gdouble xscale,
                                       gdouble yscale,
                                       gint dir);

typedef GwySIUnit* (*GwyNLFitGetUnitFunc)(GwyNLFitPreset *preset,
                                          guint param,
                                          GwySIUnit *siunit_x,
                                          GwySIUnit *siunit_y);

typedef void (*GwyNLFitWeightFunc)(gint n_dat,
                                   const gdouble *x,
                                   const gdouble *y,
                                   gdouble *weight);

typedef struct {
    const char *name;
    gint power_x;
    gint power_y;
} GwyNLFitParam;

struct _GwyNLFitPresetBuiltin {
    const gchar *name;
    const gchar *formula;
    GwyNLFitFunc function;
    GwyNLFitDerFunc derive;
    GwyNLFitGuessFunc guess;
    GwyNLFitParamScaleFunc scale_params;
    GwyNLFitGetUnitFunc get_unit;
    GwyNLFitWeightFunc set_default_weights;
    guint nparams;
    const GwyNLFitParam *param;
};

G_GNUC_INTERNAL
void _gwy_nlfit_preset_class_setup_presets(void);

G_GNUC_INTERNAL
void _gwy_fd_curve_preset_class_setup_presets(void);

G_END_DECLS

#endif /* __GWY_INTERNAL_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

