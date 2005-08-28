/*
 *  @(#) $Id$
 *  Copyright (C) 2000-2003 Martin Siler.
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_NLFIT_PRESET_H__
#define __GWY_NLFIT_PRESET_H__

#include <libgwyddion/gwynlfit.h>
#include <libgwyddion/gwyinventory.h>

G_BEGIN_DECLS

/* TODO: Resourcify someday.  But first we need clean API. */
typedef struct _GwyNLFitPreset GwyNLFitPreset;

/* XXX: Remove from API because it will be a GwyResource method */
#define gwy_nlfit_preset_get_name(f) _gwy_nlfit_preset_get_name(f)
const gchar* _gwy_nlfit_preset_get_name     (GwyNLFitPreset *preset);

gdouble      gwy_nlfit_preset_get_value     (GwyNLFitPreset *preset,
                                             gdouble x,
                                             const gdouble *params,
                                             gboolean *fres);
const gchar* gwy_nlfit_preset_get_formula   (GwyNLFitPreset *preset);
gint         gwy_nlfit_preset_get_nparams   (GwyNLFitPreset *preset);
const gchar* gwy_nlfit_preset_get_param_name(GwyNLFitPreset *preset,
                                             gint param);
void         gwy_nlfit_preset_guess         (GwyNLFitPreset *preset,
                                             gint n_dat,
                                             const gdouble *x,
                                             const gdouble *y,
                                             gdouble *params,
                                             gboolean *fres);
GwyNLFitter* gwy_nlfit_preset_fit           (GwyNLFitPreset *preset,
                                             GwyNLFitter *fitter,
                                             gint n_dat,
                                             const gdouble *x,
                                             const gdouble *y,
                                             gdouble *param,
                                             gdouble *err,
                                             const gboolean *fixed_param);

GwyInventory* gwy_nlfit_presets             (void);

G_END_DECLS

#endif /* __GWY_NFLIT_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
