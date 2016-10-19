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
    /* Context for the opaque function during fitting. */
    const GwyXYZ *xyz;
};

typedef struct _GwyShapeFitPresetPrivate ShapeFitPreset;

static GwyShapeFitPreset*
gwy_shape_fit_preset_new_static(const FitShapeFunc *function);

G_DEFINE_TYPE(GwyShapeFitPreset, gwy_shape_fit_preset, GWY_TYPE_RESOURCE)

static const FitShapeFunc functions[] = {
#if 0
    { N_("Grating (simple)"),  FALSE, SHAPE_FUNC_ITEM(grating),    },
    { N_("Grating (3-level)"), FALSE, SHAPE_FUNC_ITEM(grating3),   },
    { N_("Holes"),             FALSE, SHAPE_FUNC_ITEM(holes),      },
    { N_("Ring"),              FALSE, SHAPE_FUNC_ITEM(pring),      },
    { N_("Sphere"),            TRUE,  SHAPE_FUNC_ITEM(sphere),     },
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

static GwyShapeFitPreset*
gwy_shape_fit_preset_new_static(const FitShapeFunc *builtin)
{
    GwyShapeFitPreset *preset;

    preset = g_object_new(GWY_TYPE_SHAPE_FIT_PRESET, "is-const", TRUE, NULL);
    preset->priv->builtin = builtin;
    g_string_assign(GWY_RESOURCE(preset)->name, builtin->name);

    return preset;
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
