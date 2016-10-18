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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwyshapefitpreset.h>
#include "gwyddioninternal.h"

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

typedef struct {
    gboolean have_mean;
    gboolean have_circle;
    gboolean have_zrange;
    gboolean have_zstats;
    /* Plain mean values */
    gdouble xm;
    gdouble ym;
    /* Circumscribed circle. */
    gdouble xc;
    gdouble yc;
    gdouble r;
    /* Value range. */
    gdouble zmin;
    gdouble zmax;
    /* Simple value stats. */
    gdouble zmean;
    gdouble zrms;
    gdouble zskew;
} FitShapeEstimateCache;

typedef gdouble (*FitShapeXYFunc)(gdouble x, gdouble y,
                                  const gdouble *param);

typedef gboolean (*FitShapeEstimate)(const GwyXYZ *xyz,
                                     guint n,
                                     gdouble *param,
                                     FitShapeEstimateCache *estimcache);

typedef gdouble (*FitShapeCalcParam)(const gdouble *param);
typedef gdouble (*FitShapeCalcError)(const gdouble *param,
                                     const gdouble *param_err,
                                     const gdouble *correl);

typedef struct {
    const char *name;
    gint power_xy;
    gint power_z;
    GwyNLFitParamFlags flags;
} FitShapeParam;

typedef struct {
    const char *name;
    gint power_xy;
    gint power_z;
    GwyNLFitParamFlags flags;
    FitShapeCalcParam calc;
    FitShapeCalcError calc_err;
} FitShapeSecondary;

typedef struct {
    const gchar *name;
    gboolean needs_same_units;
    FitShapeXYFunc function;
    GwyNLFitIdxFunc fit_function;
    FitShapeEstimate estimate;
    FitShapeEstimate initialise;
    guint nparams;
    guint nsecondary;
    const FitShapeParam *param;
    const FitShapeSecondary *secondary;
} FitShapeFunc;

struct _GwyShapeFitPresetPrivate {
    const FitShapeFunc *builtin;
};

typedef struct _GwyShapeFitPresetPrivate ShapeFitPreset;

/*
static GwyShapeFitPreset*
gwy_shape_fit_preset_new_static(const GwyShapeFitPresetBuiltin *data);
*/

G_DEFINE_TYPE(GwyShapeFitPreset, gwy_shape_fit_preset, GWY_TYPE_RESOURCE)

static void
gwy_shape_fit_preset_class_init(GwyShapeFitPresetClass *klass)
{
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    g_type_class_add_private(klass, sizeof(ShapeFitPreset));

    parent_class = GWY_RESOURCE_CLASS(gwy_shape_fit_preset_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);
    res_class->name = "shapefitpresets";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    gwy_inventory_forget_order(res_class->inventory);
}

static void
gwy_shape_fit_preset_init(GwyShapeFitPreset *preset)
{
    gwy_debug_objects_creation(G_OBJECT(preset));
    preset->priv = G_TYPE_INSTANCE_GET_PRIVATE(preset,
                                               GWY_TYPE_SHAPE_FIT_PRESET,
                                               ShapeFitPreset);
}

#if 0
static GwyShapeFitPreset*
gwy_shape_fit_preset_new_static(const GwyShapeFitPresetBuiltin *data)
{
    GwyShapeFitPreset *preset;

    preset = g_object_new(GWY_TYPE_SHAPE_FIT_PRESET, "is-const", TRUE, NULL);
    preset->builtin = data;
    g_string_assign(GWY_RESOURCE(preset)->name, data->name);

    return preset;
}
#endif

void
_gwy_shape_fit_preset_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyShapeFitPreset *preset;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_SHAPE_FIT_PRESET);

#if 0
    for (i = 0; i < G_N_ELEMENTS(fitting_presets); i++) {
        preset = gwy_shape_fit_preset_new_static(fitting_presets + i);
        gwy_inventory_insert_item(klass->inventory, preset);
        g_object_unref(preset);
    }
#endif
    gwy_inventory_restore_order(klass->inventory);

    /* The presets added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

/**
 * gwy_shape_fit_preset_get_nparams:
 * @preset: A 3D geometrical shape fitting function.
 *
 * Reports the number of parameters of a 3D geometrical shape fitter preset.
 *
 * Returns: The number of function parameters.
 *
 * Since: 2.47
 **/
guint
gwy_shape_fit_preset_get_nparams(GwyShapeFitPreset* preset)
{
    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), 0);
    return preset->priv->builtin->nparams;
}

/**
 * gwy_shape_fit_preset_get_param_name:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Parameter number.
 *
 * Gets the name of a fitting parameter of a 3D geometrical shape fitter
 * preset.
 *
 * The name may contain Pango markup.
 *
 * Returns: The name of the @i-th parameter.
 *
 * Since: 2.47
 **/
const gchar*
gwy_shape_fit_preset_get_param_name(GwyShapeFitPreset* preset,
                                    guint i)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), "");
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nparams, "");

    return builtin->param[i].name;
}

/**
 * gwy_shape_fit_preset_get_param_flags:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Parameter number.
 *
 * Gets the properties of a fitting parameter of a 3D geometrical shape fitter
 * preset.
 *
 * Returns: The flags of the @i-th parameter.
 *
 * Since: 2.47
 **/
GwyNLFitParamFlags
gwy_shape_fit_preset_get_param_flags(GwyShapeFitPreset* preset,
                                     guint i)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), 0);
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nparams, 0);

    return builtin->param[i].flags;
}

/**
 * gwy_shape_fit_preset_get_param_units:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Parameter number.
 * @siunit_xy: SI unit of lateral coordinates.
 * @siunit_z: SI unit of values.
 *
 * Derives the SI unit of a fitting parameter from the units of abscissa and
 * ordinate.
 *
 * Note that angle parameters are by default in radians and thus unitless.
 * If you want to convert them to degrees for presentation to the user you must
 * do it explicitly.
 *
 * Returns: A newly created #GwySIUnit with the units of the @i-th parameter.
 *
 * Since: 2.47
 **/
GwySIUnit*
gwy_shape_fit_preset_get_param_units(GwyShapeFitPreset *preset,
                                     guint i,
                                     GwySIUnit *siunit_xy,
                                     GwySIUnit *siunit_z)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit_xy), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit_z), NULL);
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nparams, NULL);

    return gwy_si_unit_power_multiply(siunit_xy, builtin->param[i].power_xy,
                                      siunit_z, builtin->param[i].power_z,
                                      NULL);
}

/**
 * gwy_shape_fit_preset_get_nsecondary:
 * @preset: A 3D geometrical shape fitting function.
 *
 * Reports the number of secondary (derived) quantities of a 3D geometrical
 * shape fitter preset.
 *
 * Returns: The number of secondary quantities.
 *
 * Since: 2.47
 **/
guint
gwy_shape_fit_preset_get_nsecondary(GwyShapeFitPreset *preset)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), 0);
    builtin = preset->priv->builtin;
    return builtin->nsecondary;
}

/**
 * gwy_shape_fit_preset_get_secondary_name:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Secondary quantity number.
 *
 * Gets the name of a secondary (derived) quantity of a 3D geometrical shape
 * fitter preset.
 *
 * The name may contain Pango markup.
 *
 * Returns: The name of the @i-th secondary quantity.
 *
 * Since: 2.47
 **/
const gchar*
gwy_shape_fit_preset_get_secondary_name(GwyShapeFitPreset *preset,
                                        guint i)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), "");
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nsecondary, "");
    return builtin->secondary[i].name;
}

/**
 * gwy_shape_fit_preset_get_secondary_flags:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Secondary quantity number.
 *
 * Gets the properties of a secondary (derived) quantity of a 3D geometrical
 * shape fitter preset.
 *
 * Returns: The flags of the @i-th secondary quantity.
 *
 * Since: 2.47
 **/
GwyNLFitParamFlags
gwy_shape_fit_preset_get_secondary_flags(GwyShapeFitPreset *preset,
                                         guint i)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), 0);
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nsecondary, 0);
    return builtin->secondary[i].flags;
}

/**
 * gwy_shape_fit_preset_get_secondary_value:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Secondary quantity number.
 * @param: Values of fitting parameters for function @preset.
 *
 * Calculates the value of a secondary (derived) quantity of a 3D geometrical
 * shape fitter preset.
 *
 * Returns: The value of the @i-th secondary quantity.
 *
 * Since: 2.47
 **/
gdouble
gwy_shape_fit_preset_get_secondary_value(GwyShapeFitPreset *preset,
                                         guint i,
                                         const gdouble *param)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), 0.0);
    g_return_val_if_fail(param, 0.0);
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nsecondary, 0.0);
    return builtin->secondary[i].calc(param);
}

/**
 * gwy_shape_fit_preset_get_secondary_error:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Secondary quantity number.
 * @param: Values of fitting parameters for function @preset.
 * @error: Values of errors of fitting parameters for function @preset.
 * @correl: Parameter correlation matrix for function @preset (passed as lower
 *          triangular matrix).
 *
 * Calculates the error of a secondary (derived) quantity of a 3D geometrical
 * shape fitter preset.
 *
 * The error is calculated by numerical differentiation of the function and
 * applying the law of error propagation.
 *
 * Returns: The error of the @i-th secondary quantity.
 *
 * Since: 2.47
 **/
gdouble
gwy_shape_fit_preset_get_secondary_error(GwyShapeFitPreset *preset,
                                         guint i,
                                         const gdouble *param,
                                         const gdouble *error,
                                         const gdouble *correl)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), 0.0);
    g_return_val_if_fail(param, 0.0);
    g_return_val_if_fail(error, 0.0);
    g_return_val_if_fail(correl, 0.0);
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nsecondary, 0.0);
    return builtin->secondary[i].calc_err(param, error, correl);
}

/**
 * gwy_shape_fit_preset_get_secondary_units:
 * @preset: A 3D geometrical shape fitting function.
 * @i: Secondary quantity number.
 * @siunit_xy: SI unit of lateral coordinates.
 * @siunit_z: SI unit of values.
 *
 * Derives the SI unit of a secondary (derived) quantity from the units of
 * abscissa and ordinate.
 *
 * Note that angle parameters are by default in radians and thus unitless.
 * If you want to convert them to degrees for presentation to the user you must
 * do it explicitly.
 *
 * Returns: A newly created #GwySIUnit with the units of the @i-th secondary
 *          quantity.
 *
 * Since: 2.47
 **/
GwySIUnit*
gwy_shape_fit_preset_get_secondary_units(GwyShapeFitPreset *preset,
                                         guint i,
                                         GwySIUnit *siunit_xy,
                                         GwySIUnit *siunit_z)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit_xy), NULL);
    g_return_val_if_fail(GWY_IS_SI_UNIT(siunit_z), NULL);
    builtin = preset->priv->builtin;
    g_return_val_if_fail(i < builtin->nsecondary, NULL);

    return gwy_si_unit_power_multiply(siunit_xy, builtin->secondary[i].power_xy,
                                      siunit_z, builtin->secondary[i].power_z,
                                      NULL);
}

#if 0
/**
 * gwy_shape_fit_preset_guess:
 * @preset: A 3D geometrical shape fitting function.
 * @n_dat: The number of data points (number of items in @x and @y).
 * @x: Abscissa points.
 * @y: Ordinate points.
 * @params: The array to fill with estimated parameter values.  It has to be
 *          at least gwy_shape_fit_preset_get_nparams() long.
 * @fres: Set to %TRUE if succeeds, %FALSE on failure.
 *
 * Performs initial parameter estimate for a NL fitter.
 *
 * The initial estimate method depends on the function used.  There is no
 * absolute guarantee of quality, however if the data points approximately
 * match the fitted function the fit will typically converge from the returned
 * estimate.
 *
 * The parameters are filled also on failure, though just with some neutral
 * values that should not give raise to NaNs and infinities.
 *
 * Since: 2.47
 **/
void
gwy_shape_fit_preset_guess(GwyShapeFitPreset *preset,
                       gint n_dat,
                       const gdouble *x,
                       const gdouble *y,
                       gdouble *params,
                       gboolean *fres)
{
    g_return_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset));
    /* FIXME: builtin */
    preset->builtin->guess(n_dat, x, y, params, fres);
}

/**
 * gwy_shape_fit_preset_fit:
 * @preset: A 3D geometrical shape fitting function.
 * @fitter: A Marquardt-Levenberg nonlinear fitter already initialized for
 *          @preset's function, or %NULL.
 * @n_dat: The number of data points (number of items in @x and @y).
 * @x: Abscissa points.
 * @y: Ordinate points.
 * @params: Initial parameter estimate (the number of parameters depends on
 *          the fitted preset and it can be obtained with
 *          gwy_shape_fit_preset_get_nparams()).
 * @err: Array to store parameter errros to, may be %NULL.
 * @fixed_param: Which parameters should be treated as fixed (set
 *               corresponding element to %TRUE for them).  May be %NULL if
 *               all parameters are variable.
 *
 * Performs a nonlinear fit with a preset.
 *
 * See gwy_math_nlfit_fit_full() for details.
 *
 * Returns: Either @fitter itself, or a newly created fitter if it was %NULL.
 *
 * Since: 2.47
 **/
GwyNLFitter*
gwy_shape_fit_preset_fit(GwyShapeFitPreset *preset,
                     GwyNLFitter *fitter,
                     gint n_dat,
                     const gdouble *x,
                     const gdouble *y,
                     gdouble *param,
                     gdouble *err,
                     const gboolean *fixed_param)
{
    gdouble *weight = NULL;
    gboolean ok;
    gint i;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), NULL);
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

    /*load default weights for given function type*/
    if (preset->builtin->set_default_weights) {
        weight = g_new(gdouble, n_dat);
        preset->builtin->set_default_weights(n_dat, x, y, weight);
    }

    /* FIXME: builtin */
    ok = gwy_math_nlfit_fit_full(fitter, n_dat, x, y, weight,
                                 preset->builtin->nparams, param,
                                 fixed_param, NULL, preset) >= 0.0;

    if (ok && err && fitter->covar) {
    /* FIXME: builtin */
        for (i = 0; i < preset->builtin->nparams; i++)
            err[i] = gwy_math_nlfit_get_sigma(fitter, i);
    }

    g_free(weight);

    return fitter;
}
#endif

/**
 * gwy_shape_fit_presets:
 *
 * Gets inventory with all the 3D geometric shape fitting presets.
 *
 * Returns: 3D geometric shape fitting preset inventory.
 *
 * Since: 2.47
 **/
GwyInventory*
gwy_shape_fit_presets(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_SHAPE_FIT_PRESET))->inventory;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwynlfitpreset
 * @title: GwyShapeFitPreset
 * @short_description: 3D geometrical shape fitting functions
 * @see_also: #GwyNLFitter
 *
 * <link linkend="GwyNLFitter">Non-linear fitter</link> presets are predefined
 * fitting functions...
 *
 * As of version 2.47 the defined functions include:
 * <simplelist type='vert'>
 * <member><literal>"Gaussian"</literal></member>
 * <member><literal>"Gaussian (PSDF)"</literal></member>
 * <member><literal>"Gaussian (ACF)"</literal></member>
 * <member><literal>"Gaussian (HHCF)"</literal></member>
 * <member><literal>"Gaussian (RPSDF)"</literal></member>
 * <member><literal>"Exponential"</literal></member>
 * <member><literal>"Exponential (PSDF)"</literal></member>
 * <member><literal>"Exponential (ACF)"</literal></member>
 * <member><literal>"Exponential (HHCF)"</literal></member>
 * <member><literal>"Exponential (RPSDF)"</literal></member>
 * <member><literal>"Polynomial (order 0)"</literal></member>
 * <member><literal>"Polynomial (order 1)"</literal></member>
 * <member><literal>"Polynomial (order 2)"</literal></member>
 * <member><literal>"Polynomial (order 3)"</literal></member>
 * <member><literal>"Square wave"</literal></member>
 * <member><literal>"Power"</literal></member>
 * <member><literal>"Lorentzian"</literal></member>
 * <member><literal>"Sinc"</literal></member>
 * </simplelist>
 * ]|
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
