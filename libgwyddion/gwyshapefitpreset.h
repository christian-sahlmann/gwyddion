/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

#ifndef __GWY_SHAPE_FIT_PRESET_H__
#define __GWY_SHAPE_FIT_PRESET_H__

#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfit.h>
#include <libgwyddion/gwyresource.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

#define GWY_TYPE_SHAPE_FIT_PRESET             (gwy_shape_fit_preset_get_type())
#define GWY_SHAPE_FIT_PRESET(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SHAPE_FIT_PRESET, GwyShapeFitPreset))
#define GWY_SHAPE_FIT_PRESET_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_SHAPE_FIT_PRESET, GwyShapeFitPresetClass))
#define GWY_IS_SHAPE_FIT_PRESET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SHAPE_FIT_PRESET))
#define GWY_IS_SHAPE_FIT_PRESET_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_SHAPE_FIT_PRESET))
#define GWY_SHAPE_FIT_PRESET_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SHAPE_FIT_PRESET, GwyShapeFitPresetClass))

typedef enum {
    GWY_NLFIT_PARAM_ANGLE  = 1 << 0,
    GWY_NLFIT_PARAM_ABSVAL = 1 << 1,
} GwyNLFitParamFlags;

typedef struct _GwyShapeFitPreset      GwyShapeFitPreset;
typedef struct _GwyShapeFitPresetClass GwyShapeFitPresetClass;

struct _GwyShapeFitPreset {
    GwyResource parent_instance;
    struct _GwyShapeFitPresetPrivate *priv;
};

struct _GwyShapeFitPresetClass {
    GwyResourceClass parent_class;
};

GType              gwy_shape_fit_preset_get_type           (void)                         G_GNUC_CONST;
guint              gwy_shape_fit_preset_get_nparams        (GwyShapeFitPreset *preset);
const gchar*       gwy_shape_fit_preset_get_param_name     (GwyShapeFitPreset *preset,
                                                            guint i);
GwyNLFitParamFlags gwy_shape_fit_preset_get_param_flags    (GwyShapeFitPreset *preset,
                                                            guint i);
GwySIUnit*         gwy_shape_fit_preset_get_param_units    (GwyShapeFitPreset *preset,
                                                            guint i,
                                                            GwySIUnit *siunit_xy,
                                                            GwySIUnit *siunit_z);
guint              gwy_shape_fit_preset_get_nsecondary     (GwyShapeFitPreset *preset);
const gchar*       gwy_shape_fit_preset_get_secondary_name (GwyShapeFitPreset *preset,
                                                            guint i);
GwyNLFitParamFlags gwy_shape_fit_preset_get_secondary_flags(GwyShapeFitPreset *preset,
                                                            guint i);
gdouble            gwy_shape_fit_preset_get_secondary_value(GwyShapeFitPreset *preset,
                                                            guint i,
                                                            const gdouble *param);
gdouble            gwy_shape_fit_preset_get_secondary_error(GwyShapeFitPreset *preset,
                                                            guint i,
                                                            const gdouble *param,
                                                            const gdouble *error,
                                                            const gdouble *correl);
GwySIUnit*         gwy_shape_fit_preset_get_secondary_units(GwyShapeFitPreset *preset,
                                                            guint i,
                                                            GwySIUnit *siunit_xy,
                                                            GwySIUnit *siunit_z);
gboolean           gwy_shape_fit_preset_setup              (GwyShapeFitPreset *preset,
                                                            const GwyXYZ *points,
                                                            guint n,
                                                            gdouble *params);
gboolean           gwy_shape_fit_preset_guess              (GwyShapeFitPreset *preset,
                                                            const GwyXYZ *points,
                                                            guint n,
                                                            gdouble *params);
gdouble            gwy_shape_fit_preset_get_value          (GwyShapeFitPreset *preset,
                                                            gdouble x,
                                                            gdouble y,
                                                            const gdouble *params,
                                                            gboolean *fres);
gboolean           gwy_shape_fit_preset_calculate_z        (GwyShapeFitPreset *preset,
                                                            const GwyXYZ *points,
                                                            gdouble *z,
                                                            guint n,
                                                            const gdouble *params);
gboolean           gwy_shape_fit_preset_calculate_xyz      (GwyShapeFitPreset *preset,
                                                            GwyXYZ *points,
                                                            guint n,
                                                            const gdouble *params);
GwyNLFitter*       gwy_shape_fit_preset_fit                (GwyShapeFitPreset *preset,
                                                            const GwyXYZ *points,
                                                            guint n,
                                                            gdouble *params,
                                                            gdouble *err,
                                                            const gboolean *fixed_param);
GwyInventory*      gwy_shape_fit_presets                   (void);

G_END_DECLS

#endif /* __GWY_SHAPE_FIT_PRESET_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
