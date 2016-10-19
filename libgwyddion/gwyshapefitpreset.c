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

#define DECLARE_SECONDARY(funcname,name) \
    static gdouble funcname##_calc_##name    (const gdouble *param); \
    static gdouble funcname##_calc_err_##name(const gdouble *param, \
                                              const gdouble *param_err, \
                                              const gdouble *correl);

#define DECLARE_SHAPE_FUNC(name) \
    static gdouble name##_func(gdouble x, \
                               gdouble y, \
                               const gdouble *param); \
    static gdouble name##_fitfunc(guint i, \
                                  const gdouble *param, \
                                  gpointer user_data, \
                                  gboolean *fres) \
    { \
        const GwyXYZ *xyz = ((const ShapeFitPreset*)user_data)->xyz; \
        *fres = TRUE; \
        return name##_func(xyz[i].x, xyz[i].y, param) - xyz[i].z; \
    } \
    static gboolean name##_estimate(const GwyXYZ *xyz, \
                                    guint n, \
                                    gdouble *param, \
                                    FitShapeEstimateCache *estimcache); \
    static gboolean name##_init(const GwyXYZ *xyz, \
                                guint n, \
                                gdouble *param, \
                                FitShapeEstimateCache *estimcache);

/* XXX: This is a dirty trick assuming sizeof(FitShapeSecondary) > sizeof(NULL)
 * so that we get zero nsecondary when name##_secondary is defined to NULL
 * and correct array size otherwise.  It should be safe because
 * FitShapeSecondary is a struct that contains at least two pointers plus other
 * stuff, but it is dirty anyway. */
#define SHAPE_FUNC_ITEM(name) \
    &name##_func, &name##_fitfunc, &name##_estimate, &name##_init, \
    G_N_ELEMENTS(name##_params), \
    sizeof(name##_secondary)/sizeof(FitShapeSecondary), \
    name##_params, \
    name##_secondary

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
    /* Context for the opaque function during fitting. */
    const GwyXYZ *xyz;
};

typedef struct _GwyShapeFitPresetPrivate ShapeFitPreset;

DECLARE_SHAPE_FUNC(sphere);

DECLARE_SECONDARY(sphere, R);

static const FitShapeParam sphere_params[] = {
   { "x<sub>0</sub>", 1,  0, 0, },
   { "y<sub>0</sub>", 1,  0, 0, },
   { "z<sub>0</sub>", 0,  1, 0, },
   { "C",             0, -1, 0, },
};

static const FitShapeSecondary sphere_secondary[] = {
   { "R", 0, 1, 0, sphere_calc_R, sphere_calc_err_R, },
};

static GwyShapeFitPreset*
gwy_shape_fit_preset_new_static(const FitShapeFunc *function);

G_DEFINE_TYPE(GwyShapeFitPreset, gwy_shape_fit_preset, GWY_TYPE_RESOURCE)

static const FitShapeFunc functions[] = {
    { N_("Sphere"),            TRUE,  SHAPE_FUNC_ITEM(sphere),     },
#if 0
    { N_("Grating (simple)"),  FALSE, SHAPE_FUNC_ITEM(grating),    },
    { N_("Grating (3-level)"), FALSE, SHAPE_FUNC_ITEM(grating3),   },
    { N_("Holes"),             FALSE, SHAPE_FUNC_ITEM(holes),      },
    { N_("Ring"),              FALSE, SHAPE_FUNC_ITEM(pring),      },
    { N_("Cylinder (lying)"),  TRUE,  SHAPE_FUNC_ITEM(cylinder),   },
    { N_("Gaussian"),          FALSE, SHAPE_FUNC_ITEM(gaussian),   },
    { N_("Lorentzian"),        FALSE, SHAPE_FUNC_ITEM(lorentzian), },
    { N_("Pyramid (diamond)"), FALSE, SHAPE_FUNC_ITEM(pyramidx),   },
    { N_("Parabolic bump"),    FALSE, SHAPE_FUNC_ITEM(parbump),   },
#endif
};

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

void
_gwy_shape_fit_preset_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyShapeFitPreset *preset;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_SHAPE_FIT_PRESET);

    for (i = 0; i < G_N_ELEMENTS(functions); i++) {
        preset = gwy_shape_fit_preset_new_static(functions + i);
        gwy_inventory_insert_item(klass->inventory, preset);
        g_object_unref(preset);
    }
    gwy_inventory_restore_order(klass->inventory);

    /* The presets added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

/**
 * gwy_shape_fit_preset_needs_same_units:
 * @preset: A 3D geometrical shape fitting function.
 *
 * Reports if a 3D geometrical shape fitter preset requires the same lateral
 * and value units.
 *
 * For instance, fitting a sphere is meaningless if the horizontal and
 * vertical radii would be different physical quantities.
 *
 * Returns: %TRUE if the function requires the same lateral and value units.
 *
 * Since: 2.47
 **/
gboolean
gwy_shape_fit_preset_needs_same_units(GwyShapeFitPreset *preset)
{
    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), FALSE);
    return preset->priv->builtin->needs_same_units;
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

/**
 * gwy_shape_fit_preset_setup:
 * @preset: A 3D geometrical shape fitting function.
 * @points: Array of XYZ data to fit.
 * @n: Number of data points.
 * @params: The array to fill with initialised parameter values.
 *
 * Initialises parameter values of a 3D geometrical shape fitter preset.
 *
 * The parameters are quickly set to reasonable values that roughly correspond
 * to the ranges of the data points.  They may serve as starting values for
 * manual experimentation but often will not be good enough as initial
 * parameter estimates for the fit.  See also gwy_shape_fit_preset_guess().
 *
 * Since: 2.47
 **/
void
gwy_shape_fit_preset_setup(GwyShapeFitPreset *preset,
                           const GwyXYZ *points,
                           guint n,
                           gdouble *params)
{
    FitShapeEstimateCache estimcache;
    const FitShapeFunc *builtin;

    g_return_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset));
    g_return_if_fail(points);
    g_return_if_fail(params);
    builtin = preset->priv->builtin;

    gwy_clear(&estimcache, 1);
    /* The function has a return value but it should always succeed. */
    builtin->initialise(points, n, params, &estimcache);
}

/**
 * gwy_shape_fit_preset_guess:
 * @preset: A 3D geometrical shape fitting function.
 * @points: Array of XYZ data to fit.
 * @n: Number of data points.
 * @params: The array to fill with initialised parameter values.
 *
 * Estimates parameter values of a 3D geometrical shape fitter preset.
 *
 * This function tries to find initial parameter estimates that are good enough
 * for the fit the converge.  Of course, it is not guaranteed it always
 * succeeds.  For some shapes it can be noticeably slower than
 * gwy_shape_fit_preset_setup().
 *
 * The estimate may not be deterministic.  For large point sets some estimates
 * are carried out using a randomly selected subset of points.
 *
 * If the function cannot find how the data points could correspond to the
 * preset geometrical shape it return %FALSE.  Parameter values are still set.
 * However, in this case they may be no better than from
 * gwy_shape_fit_preset_setup().
 *
 * Returns: %TRUE if the estimation succeeded, %FALSE if it failed.
 *
 * Since: 2.47
 **/
gboolean
gwy_shape_fit_preset_guess(GwyShapeFitPreset *preset,
                           const GwyXYZ *points,
                           guint n,
                           gdouble *params)
{
    FitShapeEstimateCache estimcache;
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), FALSE);
    g_return_val_if_fail(points, FALSE);
    g_return_val_if_fail(params, FALSE);
    builtin = preset->priv->builtin;

    gwy_clear(&estimcache, 1);
    return builtin->estimate(points, n, params, &estimcache);
}

/**
 * gwy_shape_fit_preset_get_value:
 * @preset: A 3D geometrical shape fitting function.
 * @x: X-coordinate.
 * @y: Y-coordinate.
 * @params: Fitting parameter values.
 *
 * Calculates the value of a 3D geometrical shape fitter preset in a single
 * point.
 *
 * If you want multiple values you should use either
 * gwy_shape_fit_preset_calculate_z() or gwy_shape_fit_preset_calculate_xyz()
 * instead of calling this function in a cycle.
 *
 * Returns: The calculated function value in (@x,@y).
 *
 * Since: 2.47
 **/
gdouble
gwy_shape_fit_preset_get_value(GwyShapeFitPreset *preset,
                               gdouble x,
                               gdouble y,
                               const gdouble *params)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), 0.0);
    g_return_val_if_fail(params, 0.0);
    builtin = preset->priv->builtin;
    return builtin->function(x, y, params);
}

/**
 * gwy_shape_fit_preset_calculate_z:
 * @preset: A 3D geometrical shape fitting function.
 * @points: Array of @n XYZ data defining the lateral coordinates.
 * @z: Array length @n to fill with calculated values.
 * @n: Number of items in @points and @z.
 * @params: Fitting parameter values.
 *
 * Calculates values of a 3D geometrical shape fitter preset in an array of
 * points.
 *
 * The z-coordinates in @points are ignored.  Only the lateral coordinates are
 * used.
 *
 * See also gwy_shape_fit_preset_calculate_xyz().
 *
 * Since: 2.47
 **/
void
gwy_shape_fit_preset_calculate_z(GwyShapeFitPreset *preset,
                                 const GwyXYZ *points,
                                 gdouble *z,
                                 guint n,
                                 const gdouble *params)
{
    const FitShapeFunc *builtin;
    FitShapeXYFunc func;
    guint i;

    g_return_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset));
    g_return_if_fail(params);
    if (!n)
        return;

    g_return_if_fail(points);
    g_return_if_fail(z);
    builtin = preset->priv->builtin;
    func = builtin->function;
    for (i = 0; i < n; i++)
        z[i] = func(points[i].x, points[i].y, params);
}

/**
 * gwy_shape_fit_preset_calculate_xyz:
 * @preset: A 3D geometrical shape fitting function.
 * @points: Array of @n XYZ data defining the lateral coordinates.  The
 *          z-coordinates will be filled with the calculated values.
 * @n: Number of items in @points.
 * @params: Fitting parameter values.
 *
 * Calculates values of a 3D geometrical shape fitter preset in an array of
 * points.
 *
 * See also gwy_shape_fit_preset_calculate_z().
 *
 * Since: 2.47
 **/
void
gwy_shape_fit_preset_calculate_xyz(GwyShapeFitPreset *preset,
                                   GwyXYZ *points,
                                   guint n,
                                   const gdouble *params)
{
    const FitShapeFunc *builtin;
    FitShapeXYFunc func;
    guint i;

    g_return_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset));
    g_return_if_fail(params);
    if (!n)
        return;

    g_return_if_fail(points);
    builtin = preset->priv->builtin;
    func = builtin->function;
    for (i = 0; i < n; i++)
        points[i].z = func(points[i].x, points[i].y, params);
}

/**
 * gwy_shape_fit_preset_create_fitter:
 * @preset: A 3D geometrical shape fitting function.
 *
 * Creates a non-linear least-squares fitter for a 3D geometrical shape.
 *
 * The created fitter will be of the opaque indexed data type, as created with
 * gwy_math_nlfit_new_idx().
 *
 * If you do not need to modify the fitter settings you can use
 * gwy_shape_fit_preset_fit() directly with %NULL fitter.
 *
 * Returns: A newly created fitter for @preset.
 *
 * Since: 2.47
 **/
GwyNLFitter*
gwy_shape_fit_preset_create_fitter(GwyShapeFitPreset *preset)
{
    const FitShapeFunc *builtin;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), NULL);
    builtin = preset->priv->builtin;
    return gwy_math_nlfit_new_idx(builtin->fit_function, NULL);
}

/**
 * gwy_shape_fit_preset_fit:
 * @preset: A 3D geometrical shape fitting function.
 * @fitter: A Marquardt-Levenberg nonlinear fitter already initialized for
 *          @preset's function, or %NULL.
 * @points: Array of @n XYZ data defining the lateral coordinates and values
 *          to fit.
 * @n: Number of items in @points.
 * @params: Fitting parameters filled with initial estimates (the fitting
 *          starts from the provided values).
 * @fixed_param: Which parameters should be treated as fixed (set
 *               corresponding element to %TRUE for them).  May be %NULL if
 *               all parameters are free.
 * @rss: Location to store the residual sum of squares, as returned by
 *       gwy_math_nlfit_fit_idx(), may be %NULL.
 *
 * Performs a non-linear least-squares fit with a 3D geometrical shape fitter.
 *
 * If you pass %NULL @fitter the function creates one for you and immediately
 * performs the fit.  If you want to modify the fitter settings beforehand or
 * set callback functions create it using gwy_shape_fit_preset_create_fitter()
 * and pass to this function.  The fitter must be created for the same preset.
 *
 * Additional quantities such as parameter errors or the correlation matrix can
 * be obtained from the fitter. See gwy_math_nlfit_fit_full() for details.
 *
 * Returns: Either @fitter itself, or a newly created fitter if it was %NULL.
 *
 * Since: 2.47
 **/
GwyNLFitter*
gwy_shape_fit_preset_fit(GwyShapeFitPreset *preset,
                         GwyNLFitter *fitter,
                         const GwyXYZ *points,
                         guint n,
                         gdouble *params,
                         const gboolean *fixed_param,
                         gdouble *rss)
{
    ShapeFitPreset *priv;
    const FitShapeFunc *builtin;
    gdouble myrss;

    g_return_val_if_fail(GWY_IS_SHAPE_FIT_PRESET(preset), NULL);
    g_return_val_if_fail(points, NULL);
    g_return_val_if_fail(params, NULL);
    if (!fitter)
        fitter = gwy_shape_fit_preset_create_fitter(preset);

    priv = preset->priv;
    priv->xyz = points;
    builtin = priv->builtin;
    myrss = gwy_math_nlfit_fit_idx_full(fitter, n, builtin->nparams,
                                        params, fixed_param, NULL, priv);
    priv->xyz = NULL;
    if (rss)
        *rss = myrss;

    return fitter;
}

static GwyShapeFitPreset*
gwy_shape_fit_preset_new_static(const FitShapeFunc *builtin)
{
    GwyShapeFitPreset *preset;

    preset = g_object_new(GWY_TYPE_SHAPE_FIT_PRESET, "is-const", TRUE, NULL);
    preset->priv->builtin = builtin;
    g_string_assign(GWY_RESOURCE(preset)->name, builtin->name);

    return preset;
}

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
    GTypeClass *klass = g_type_class_peek(GWY_TYPE_SHAPE_FIT_PRESET);
    return GWY_RESOURCE_CLASS(klass)->inventory;
}

/**************************************************************************
 *
 * General estimator helpers and math support functions.
 *
 **************************************************************************/

#ifdef HAVE_SINCOS
#define _gwy_sincos sincos
#else
static inline void
_gwy_sincos(gdouble x, gdouble *s, gdouble *c)
{
    *s = sin(x);
    *c = cos(x);
}
#endif

/* cosh(x) - 1, safe for small arguments */
static inline gdouble
gwy_coshm1(gdouble x)
{
    gdouble x2 = x*x;
    if (x2 > 3e-5)
        return cosh(x) - 1.0;
    return x2*(0.5 + x2/24.0);
}

#define DEFINE_PHI_CACHE(phi) \
    static gdouble phi##_last = 0.0, cphi_last = 1.0, sphi_last = 0.0

#define HANDLE_PHI_CACHE(phi) \
    do { \
        if (phi == phi##_last) { \
            cphi = cphi_last; \
            sphi = sphi_last; \
        } \
        else { \
            sincos(phi, &sphi, &cphi); \
            cphi_last = cphi; \
            sphi_last = sphi; \
            phi##_last = phi; \
        } \
    } while (0)

/* Mean value of xy point cloud (not necessarily centre, that depends on
 * the density). */
static void
mean_x_y(const GwyXYZ *xyz, guint n, gdouble *pxm, gdouble *pym,
         FitShapeEstimateCache *estimcache)
{
    gdouble xm = 0.0, ym = 0.0;
    guint i;

    if (estimcache && estimcache->have_mean) {
        gwy_debug("using cache %p", estimcache);
        *pxm = estimcache->xm;
        *pym = estimcache->ym;
        return;
    }

    if (!n) {
        *pxm = *pym = 0.0;
        return;
    }

    for (i = 0; i < n; i++) {
        xm += xyz[i].x;
        ym += xyz[i].y;
    }

    *pxm = xm/n;
    *pym = ym/n;

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_mean = TRUE;
        estimcache->xm = *pxm;
        estimcache->ym = *pym;
    }
}

/* Minimum and maximum of an array of values. */
static void
range_z(const GwyXYZ *xyz, guint n, gdouble *pmin, gdouble *pmax,
        FitShapeEstimateCache *estimcache)
{
    gdouble min, max;
    guint i;

    if (estimcache && estimcache->have_zrange) {
        gwy_debug("using cache %p", estimcache);
        *pmin = estimcache->zmin;
        *pmax = estimcache->zmax;
        return;
    }

    if (!n) {
        *pmin = *pmax = 0.0;
        return;
    }

    min = max = xyz[0].z;
    for (i = 1; i < n; i++) {
        if (xyz[i].z < min)
            min = xyz[i].z;
        if (xyz[i].z > max)
            max = xyz[i].z;
    }

    *pmin = min;
    *pmax = max;

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_zrange = TRUE;
        estimcache->zmin = *pmin;
        estimcache->zmax = *pmax;
    }
}

/* Simple stats of an array of values. */
static void
stat_z(const GwyXYZ *xyz, guint n,
       gdouble *zmean, gdouble *zrms, gdouble *zskew,
       FitShapeEstimateCache *estimcache)
{
    gdouble s = 0.0, s2 = 0.0, s3 = 0.0;
    guint i;

    if (estimcache && estimcache->have_zstats) {
        gwy_debug("using cache %p", estimcache);
        if (zmean)
            *zmean = estimcache->zmean;
        if (zrms)
            *zrms = estimcache->zrms;
        if (zskew)
            *zskew = estimcache->zskew;
        return;
    }

    if (!n) {
        if (zmean)
            *zmean = 0.0;
        if (zrms)
            *zrms = 0.0;
        if (zskew)
            *zskew = 0.0;
        return;
    }

    for (i = 0; i < n; i++)
        s += xyz[i].z;
    s /= n;

    for (i = 0; i < n; i++) {
        gdouble d = xyz[i].z - s;
        s2 += d*d;
        s3 += d*d*d;
    }

    if (s2) {
        s2 = sqrt(s2/n);
        s3 /= n*s2*s2*s2;
    }

    if (zmean)
        *zmean = s;
    if (zrms)
        *zrms = s2;
    if (zskew)
        *zskew = s3;

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_zstats = TRUE;
        estimcache->zmean = s;
        estimcache->zrms = s2;
        estimcache->zskew = s3;
    }
}

/* Approximately cicrumscribe a set of points by finding a containing
 * octagon. */
static void
circumscribe_x_y(const GwyXYZ *xyz, guint n,
                 gdouble *pxc, gdouble *pyc, gdouble *pr,
                 FitShapeEstimateCache *estimcache)
{
    gdouble min[4], max[4], r[4];
    guint i, j;

    if (estimcache && estimcache->have_circle) {
        gwy_debug("using cache %p", estimcache);
        *pxc = estimcache->xc;
        *pyc = estimcache->yc;
        *pr = estimcache->r;
        return;
    }

    if (!n) {
        *pxc = *pyc = 0.0;
        *pr = 1.0;
        return;
    }

    for (j = 0; j < 4; j++) {
        min[j] = G_MAXDOUBLE;
        max[j] = -G_MAXDOUBLE;
    }

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x, y = xyz[i].y;
        gdouble t[4] = { x, x+y, y, y-x };

        for (j = 0; j < 4; j++) {
            if (t[j] < min[j])
                min[j] = t[j];
            if (t[j] > max[j])
                max[j] = t[j];
        }
    }

    for (j = 0; j < 4; j++) {
        r[j] = sqrt(10.0)/3.0*(max[j] - min[j]);
        if (j % 2)
            r[j] /= G_SQRT2;
    }

    i = 0;
    for (j = 1; j < 4; j++) {
        if (r[j] > r[i])
            i = j;
    }

    *pr = 0.5*r[i];
    if (i % 2) {
        *pxc = (min[1] - min[3] + max[1] - max[3])/4.0;
        *pyc = (min[1] + min[3] + max[1] + max[3])/4.0;
    }
    else {
        *pxc = (min[0] + max[0])/2.0;
        *pyc = (min[2] + max[2])/2.0;
    }

    if (estimcache) {
        gwy_debug("filling cache %p", estimcache);
        estimcache->have_circle = TRUE;
        estimcache->xc = *pxc;
        estimcache->yc = *pyc;
        estimcache->r = *pr;
    }
}

/* Project xyz point cloud to a line rotated by angle phi anti-clockwise
 * from the horizontal line (x axis). */
static gdouble
projection_to_line(const GwyXYZ *xyz,
                   guint n,
                   gdouble phi,
                   gdouble xc, gdouble yc,
                   GwyDataLine *mean_line,
                   GwyDataLine *rms_line,
                   guint *counts)
{
    guint res = gwy_data_line_get_res(mean_line);
    gdouble *mean = gwy_data_line_get_data(mean_line);
    gdouble *rms = rms_line ? gwy_data_line_get_data(rms_line) : NULL;
    gdouble dx = gwy_data_line_get_real(mean_line)/res;
    gdouble off = gwy_data_line_get_offset(mean_line);
    gdouble c = cos(phi), s = sin(phi), total_ms = 0.0;
    guint i, total_n = 0;
    gint j;

    gwy_data_line_clear(mean_line);
    gwy_clear(counts, res);

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res) {
            mean[j] += xyz[i].z;
            counts[j]++;
        }
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            mean[j] /= counts[j];
        }
    }

    if (!rms_line)
        return 0.0;

    gwy_data_line_clear(rms_line);

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res)
            rms[j] += (xyz[i].z - mean[j])*(xyz[i].z - mean[j]);
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            total_ms += rms[j];
            rms[j] = sqrt(rms[j]/counts[j]);
            total_n += counts[j];
        }
    }

    return sqrt(total_ms/total_n);
}

/* Find direction along which projections capture best the shape, i.e. most
 * variance remains in the line-averaged data.  The returned angle is rotation
 * of the axis anti-clockwise with respect to the x-axis. */
static gdouble
estimate_projection_direction(const GwyXYZ *xyz, guint n,
                              FitShapeEstimateCache *estimcache)
{
    enum { NROUGH = 60, NFINE = 8 };

    GwyDataLine *mean_line, *rms_line;
    guint *counts;
    gdouble xc, yc, r, phi, alpha0, alpha_step, rms;
    gdouble best_rms = G_MAXDOUBLE, best_alpha = 0.0;
    guint iter, i, ni, res;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    res = (guint)floor(0.8*sqrt(n) + 1.0);

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    rms_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    for (iter = 0; iter < 6; iter++) {
        if (iter == 0) {
            ni = NROUGH;
            alpha_step = G_PI/ni;
            alpha0 = 0.0;
        }
        else {
            /* Choose the fine points so that we do not repeat calculation in
             * any of the rough points. */
            ni = NFINE;
            alpha0 = best_alpha - alpha_step*(NFINE - 1.0)/(NFINE + 1.0);
            alpha_step = 2.0*alpha_step/(NFINE + 1.0);
        }

        for (i = 0; i < ni; i++) {
            phi = alpha0 + i*alpha_step;
            rms = projection_to_line(xyz, n, phi, xc, yc,
                                     mean_line, rms_line, counts);
            gwy_debug("[%u] %g %g", iter, phi, rms);
            if (rms < best_rms) {
                best_rms = rms;
                best_alpha = phi;
            }
        }
    }

    g_object_unref(mean_line);
    g_object_unref(rms_line);
    g_free(counts);

    if (best_alpha > 0.5*G_PI)
        best_alpha += G_PI;

    return best_alpha;
}

/* Estimate projection direction, possibly on reduced data.  This is useful
 * when the estimator does not need reduced data for anything else. */
static gdouble
estimate_projection_direction_red(const GwyXYZ *xyz, guint n,
                                  FitShapeEstimateCache *estimcache)
{
    FitShapeEstimateCache estimcachered;
    guint nred = (guint)sqrt(n*(gdouble)NREDLIM);
    GwySurface *surface;
    gdouble phi;

    if (nred >= n)
        return estimate_projection_direction(xyz, n, estimcache);

    surface = gwy_surface_new_sized(nred);
    reduce_data_size(xyz, n, surface);

    /* Make sure caching still works for the reduced data. */
    gwy_clear(&estimcachered, 1);
    phi = estimate_projection_direction(gwy_surface_get_data_const(surface),
                                        nred, &estimcachered);

    g_object_unref(surface);

    return phi;
}

static void
data_line_shorten(GwyDataLine *dline, const guint *counts, guint threshold)
{
    guint res = gwy_data_line_get_res(dline);
    guint from = 0, to = res-1;
    gdouble off;

    while (to > from && counts[to] < threshold)
        to--;
    while (from < to && counts[from] < threshold)
        from++;

    off = (from*gwy_data_line_get_real(dline)/res
           + gwy_data_line_get_offset(dline));

    gwy_data_line_resize(dline, from, to+1);
    gwy_data_line_set_offset(dline, off);
}

/* Estimate the period of a periodic structure, knowing already the rotation.
 * The returned phase is such that if you subtract it from the rotated abscissa
 * value then the projection will have a positive peak (some kind of maximum)
 * centered around zero, whatever that means for specific grating-like
 * structures.  */
static gboolean
estimate_period_and_phase(const GwyXYZ *xyz, guint n,
                          gdouble phi, gdouble *pT, gdouble *poff,
                          FitShapeEstimateCache *estimcache)
{
    GwyDataLine *mean_line, *tmp_line;
    gdouble xc, yc, r, T, t, real, off, a_s, a_c, phi0, av, bv;
    const gdouble *mean, *tmp;
    guint *counts;
    guint res, i, ibest;
    gboolean found;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    /* Using more sqrt(n) than can make the sampling too sparse, causing noise
     * and oscillations. */
    res = (guint)floor(0.8*sqrt(n) + 1.0);

    *pT = r/4.0;
    *poff = 0.0;

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    tmp_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    projection_to_line(xyz, n, phi, xc, yc, mean_line, NULL, counts);
    data_line_shorten(mean_line, counts, 4);
    g_free(counts);

    res = gwy_data_line_get_res(mean_line);
    gwy_data_line_get_line_coeffs(mean_line, &av, &bv);
    gwy_data_line_line_level(mean_line, av, bv);
    gwy_data_line_psdf(mean_line, tmp_line,
                       GWY_WINDOWING_HANN, GWY_INTERPOLATION_LINEAR);
    tmp = gwy_data_line_get_data_const(tmp_line);

    found = FALSE;
    ibest = G_MAXUINT;
    for (i = 3; i < MIN(res/3, res-3); i++) {
        if (tmp[i] > tmp[i-2] && tmp[i] > tmp[i-1]
            && tmp[i] > tmp[i+1] && tmp[i] > tmp[i+2]) {
            if (ibest == G_MAXUINT || tmp[i] > tmp[ibest]) {
                found = TRUE;
                ibest = i;
            }
        }
    }
    if (!found)
        goto fail;

    T = *pT = 2.0*G_PI/gwy_data_line_itor(tmp_line, ibest);
    gwy_debug("found period %g", T);

    mean = gwy_data_line_get_data_const(mean_line);
    real = gwy_data_line_get_real(mean_line);
    off = gwy_data_line_get_offset(mean_line);
    a_s = a_c = 0.0;
    for (i = 0; i < res; i++) {
        t = off + real/res*(i + 0.5);
        a_s += sin(2*G_PI*t/T)*mean[i];
        a_c += cos(2*G_PI*t/T)*mean[i];
    }
    gwy_debug("a_s %g, a_c %g", a_s, a_c);

    phi0 = atan2(a_s, a_c);
    *poff = phi0*T/(2.0*G_PI) + xc*cos(phi) - yc*sin(phi);

fail:
    g_object_unref(mean_line);
    g_object_unref(tmp_line);

    return found;
}

/* For a shape that consists of a more or less flat base with some feature
 * on it, estimate the base plane (z0) and feature height (h).  The height
 * can be either positive or negative. */
static gboolean
estimate_feature_height(const GwyXYZ *xyz, guint n,
                        gdouble *pz0, gdouble *ph,
                        gdouble *px, gdouble *py,
                        FitShapeEstimateCache *estimcache)
{
    gdouble xm, ym, xc, yc, r, zmin, zmax;
    gdouble r2_large, r2_small;
    gdouble t, zbest, zmean_large = 0.0, zmean_small = 0.0;
    guint i, n_large = 0, n_small = 0;
    gboolean positive;

    if (!n) {
        *pz0 = *ph = 0.0;
        return FALSE;
    }

    range_z(xyz, n, &zmin, &zmax, estimcache);
    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    r2_large = 0.7*r*r;
    r2_small = 0.1*r*r;

    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        gdouble r2 = x*x + y*y;

        if (r2 <= r2_small) {
            zmean_small += xyz[i].z;
            n_small++;
        }
        else if (r2 >= r2_large) {
            zmean_large += xyz[i].z;
            n_large++;
        }
    }

    g_assert(n_large);   /* circumscribe_x_y() should ensure this. */
    zmean_large /= n_large;

    if (n_small) {
        zmean_small /= n_small;
        positive = (zmean_small >= zmean_large);
    }
    else
        positive = (fabs(zmean_large - zmin) <= fabs(zmean_large - zmax));

    t = zmax - zmin;
    if (positive) {
        *pz0 = zmin + 0.05*t;
        *ph = 0.9*t;
    }
    else {
        *pz0 = zmax - 0.05*t;
        *ph = -0.9*t;
    }

    xm = 0.0;
    ym = 0.0;
    if (n_small) {
        if (positive) {
            zbest = -G_MAXDOUBLE;
            for (i = 0; i < n; i++) {
                gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
                gdouble r2 = x*x + y*y;

                if (r2 <= r2_small && xyz[i].z > zbest) {
                    zbest = xyz[i].z;
                    xm = x;
                    ym = y;
                }
            }
        }
        else {
            zbest = G_MAXDOUBLE;
            for (i = 0; i < n; i++) {
                gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
                gdouble r2 = x*x + y*y;

                if (r2 <= r2_small && xyz[i].z < zbest) {
                    zbest = xyz[i].z;
                    xm = x;
                    ym = y;
                }
            }
        }
    }
    *px = xc + xm;
    *py = yc + ym;

    return TRUE;
}

static gboolean
common_bump_feature_init(const GwyXYZ *xyz, guint n,
                         gdouble *xc, gdouble *yc, gdouble *z0,
                         gdouble *height, gdouble *size,
                         gdouble *a, gdouble *phi,
                         FitShapeEstimateCache *estimcache)
{
    gdouble xm, ym, r, zmin, zmax;

    circumscribe_x_y(xyz, n, &xm, &ym, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);

    *xc = xm;
    *yc = ym;
    *z0 = zmin;
    *height = zmax - zmin;
    *size = r/3.0;
    *a = 1.0;
    *phi = 0.0;

    return TRUE;
}

static gboolean
common_bump_feature_estimate(const GwyXYZ *xyz, guint n,
                             gdouble *xc, gdouble *yc, gdouble *z0,
                             gdouble *height, gdouble *size,
                             gdouble *a, gdouble *phi,
                             FitShapeEstimateCache *estimcache)
{
    gdouble xm, ym, r;

    /* Just initialise the shape parameters with some sane defaults. */
    *a = 1.0;
    *phi = 0.0;
    circumscribe_x_y(xyz, n, &xm, &ym, &r, estimcache);
    *size = r/3.0;

    return estimate_feature_height(xyz, n, z0, height, xc, yc, estimcache);
}

static gdouble
dotprod_with_correl(const gdouble *diff,
                    const gdouble *param_err,
                    const gdouble *correl,
                    guint n)
{
    guint i, j;
    gdouble s = 0.0;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            if (diff[j] != 0 && diff[j] != 0) {
                gdouble c_ij = (j <= i) ? SLi(correl, i, j) : SLi(correl, j, i);
                gdouble s_ij = c_ij*param_err[i]*param_err[j];
                s += s_ij*diff[i]*diff[j];
            }
        }
    }

    return sqrt(fmax(s, 0.0));
}

static gdouble
data_line_pearson_coeff(GwyDataLine *dline1, GwyDataLine *dline2)
{
    gdouble avg1 = gwy_data_line_get_avg(dline1);
    gdouble avg2 = gwy_data_line_get_avg(dline2);
    gdouble rms1 = gwy_data_line_get_rms(dline1);
    gdouble rms2 = gwy_data_line_get_rms(dline2);
    const gdouble *d1, *d2;
    gdouble c = 0.0;
    guint res, i;

    if (!rms1 || !rms2)
        return 0.0;

    res = gwy_data_line_get_res(dline1);
    g_return_val_if_fail(gwy_data_line_get_res(dline2) == res, 0.0);
    d1 = gwy_data_line_get_data_const(dline1);
    d2 = gwy_data_line_get_data_const(dline2);
    for (i = 0; i < res; i++)
        c += (d1[i] - avg1)*(d2[i] - avg2);

    c /= res*rms1*rms2;
    gwy_debug("%g", c);
    return c;
}

/**************************************************************************
 *
 * Sphere
 *
 **************************************************************************/

static gdouble
sphere_func(gdouble x, gdouble y, const gdouble *param)
{
    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble kappa = param[3];
    gdouble r2k, t, val;

    x -= xc;
    y -= yc;
    /* Rewrite R - sqrt(R² - r²) as κ*r²/(1 + sqrt(1 - κ²r²)) where
     * r² = x² + y² and κR = 1 to get nice behaviour in the close-to-denegerate
     * cases, including completely flat surface.  The expression 1.0/kappa
     * is safe because we cannot get to this branch for κ → 0 unless
     * simultaneously r → ∞. */
    r2k = kappa*(x*x + y*y);
    t = 1.0 - kappa*r2k;
    if (t > 0.0)
        val = z0 + r2k/(1.0 + sqrt(t));
    else
        val = z0 + 2.0/kappa;

    return val;
}

static gboolean
sphere_init(const GwyXYZ *xyz, guint n, gdouble *param,
            FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc, r, zmin, zmax, zmean;

    circumscribe_x_y(xyz, n, &xc, &yc, &r, estimcache);
    range_z(xyz, n, &zmin, &zmax, estimcache);
    stat_z(xyz, n, &zmean, NULL, NULL, estimcache);

    param[0] = xc;
    param[1] = yc;
    if (fabs(zmean - zmin) > fabs(zmean - zmax)) {
        param[2] = zmax;
        param[3] = 2.0*(zmin - zmax)/(r*r);
    }
    else {
        param[2] = zmin;
        param[3] = 2.0*(zmax - zmin)/(r*r);
    }

    return TRUE;
}

/* Fit the data with a rotationally symmetric parabola and use its parameters
 * for the spherical surface estimate. */
static gboolean
sphere_estimate(const GwyXYZ *xyz, guint n, gdouble *param,
                FitShapeEstimateCache *estimcache)
{
    gdouble xc, yc;
    /* Linear fit with functions 1, x, y and x²+y². */
    gdouble a[10], b[4];
    guint i;

    /* XXX: Handle the surrounding flat area, which can be a part of the
     * function, better? */

    /* Using centered coodinates improves the condition number. */
    mean_x_y(xyz, n, &xc, &yc, estimcache);
    gwy_clear(a, 10);
    gwy_clear(b, 4);
    for (i = 0; i < n; i++) {
        gdouble x = xyz[i].x - xc, y = xyz[i].y - yc;
        gdouble r2 = x*x + y*y;

        b[0] += xyz[i].z;
        b[1] += x*xyz[i].z;
        b[2] += y*xyz[i].z;
        b[3] += r2*xyz[i].z;

        a[2] += x*x;
        a[4] += x*y;
        a[5] += y*y;
        a[6] += r2;
        a[7] += x*r2;
        a[8] += y*r2;
        a[9] += r2*r2;
    }
    a[0] = n;
    a[1] = a[3] = 0.0;

    param[0] = xc;
    param[1] = yc;
    param[2] = b[0]/n;
    param[3] = 0.0;

    if (!gwy_math_choleski_decompose(4, a))
        return FALSE;

    gwy_math_choleski_solve(4, a, b);

    param[3] = 2.0*b[3];
    if (param[3]) {
        param[0] = xc - b[1]/param[3];
        param[1] = yc - b[2]/param[3];
        param[2] = b[0] - 0.5*(b[1]*b[1] + b[2]*b[2])/param[3];
    }

    return TRUE;
}

static gdouble
sphere_calc_R(const gdouble *param)
{
    return 1.0/param[3];
}

static gdouble
sphere_calc_err_R(const gdouble *param,
                  const gdouble *param_err,
                  G_GNUC_UNUSED const gdouble *correl)
{
    return param_err[3]/(param[3]*param[3]);
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
