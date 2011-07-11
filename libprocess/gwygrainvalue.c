/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyexpr.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/gwygrainvalue.h>
#include "gwyprocessinternal.h"

/* We know they are usable as bits */
enum {
    MAXBUILTINS = 64,
    GWY_GRAIN_QUANTITY_ID = MAXBUILTINS-1
};

static void           gwy_grain_value_finalize (GObject *object);
static void           gwy_grain_value_data_copy(const GwyGrainValueData *src,
                                                GwyGrainValueData *dest);
static gpointer       gwy_grain_value_copy     (gpointer item);
static GwyGrainValue* gwy_grain_value_new      (const gchar *name,
                                                const GwyGrainValueData *data,
                                                gboolean is_const);
static void           gwy_grain_value_dump     (GwyResource *resource,
                                                GString *str);
static GwyResource*   gwy_grain_value_parse    (const gchar *text,
                                                gboolean is_const);

/* This is zero-filled memory, albeit typecasted to misc types. */
static const GwyGrainValueData grainvaluedata_default = {
    GWY_GRAIN_VALUE_GROUP_USER, NULL, NULL, NULL, 0, 0, 0, 0,
};

static GwyExpr *expr = NULL;

static const struct {
    const gchar* name;
    GwyGrainQuantity quantity;
    GwyGrainValueData data;
}
grain_values[] = {
    {
        N_("Grain number"),
        GWY_GRAIN_QUANTITY_ID,  /* Does not correspond to any bit */
        {
            GWY_GRAIN_VALUE_GROUP_ID,
            "id", "", NULL,
            0, 0, 0, 0,
        }
    },
    {
        N_("Center x position"),
        GWY_GRAIN_VALUE_CENTER_X,
        {
            GWY_GRAIN_VALUE_GROUP_POSITION,
            "<i>x</i><sub>c</sub>", "x_c", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Center y position"),
        GWY_GRAIN_VALUE_CENTER_Y,
        {
            GWY_GRAIN_VALUE_GROUP_POSITION,
            "<i>y</i><sub>c</sub>", "y_c", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Minimum value"),
        GWY_GRAIN_VALUE_MINIMUM,
        {
            GWY_GRAIN_VALUE_GROUP_VALUE,
            "<i>z</i><sub>min</sub>", "z_min", NULL,
            0, 1, 0, 0,
        }
    },
    {
        N_("Maximum value"),
        GWY_GRAIN_VALUE_MAXIMUM,
        {
            GWY_GRAIN_VALUE_GROUP_VALUE,
            "<i>z</i><sub>max</sub>", "z_max", NULL,
            0, 1, 0, 0,
        }
    },
    {
        N_("Mean value"),
        GWY_GRAIN_VALUE_MEAN,
        {
            GWY_GRAIN_VALUE_GROUP_VALUE,
            "<i>z</i><sub>m</sub>", "z_m", NULL,
            0, 1, 0, 0,
        }
    },
    {
        N_("Median value"),
        GWY_GRAIN_VALUE_MEDIAN,
        {
            GWY_GRAIN_VALUE_GROUP_VALUE,
            "<i>z</i><sub>med</sub>", "z_med", NULL,
            0, 1, 0, 0,
        }
    },
    {
        N_("Minimum value on boundary"),
        GWY_GRAIN_VALUE_BOUNDARY_MINIMUM,
        {
            GWY_GRAIN_VALUE_GROUP_VALUE,
            "<i>b</i><sub>min</sub>", "b_min", NULL,
            0, 1, 0, 0,
        }
    },
    {
        N_("Maximum value on boundary"),
        GWY_GRAIN_VALUE_BOUNDARY_MAXIMUM,
        {
            GWY_GRAIN_VALUE_GROUP_VALUE,
            "<i>b</i><sub>max</sub>", "b_max", NULL,
            0, 1, 0, 0,
        }
    },
    {
        N_("Projected area"),
        GWY_GRAIN_VALUE_PROJECTED_AREA,
        {
            GWY_GRAIN_VALUE_GROUP_AREA,
            "<i>A</i><sub>0</sub>", "A_0", NULL,
            2, 0, 0, 0,
        }
    },
    {
        N_("Surface area"),
        GWY_GRAIN_VALUE_SURFACE_AREA,
        {
            GWY_GRAIN_VALUE_GROUP_AREA,
            "<i>A</i><sub>s</sub>", "A_s", NULL,
            2, 0, GWY_GRAIN_VALUE_SAME_UNITS, 0,
        }
    },
    {
        N_("Equivalent square side"),
        GWY_GRAIN_VALUE_EQUIV_SQUARE_SIDE,
        {
            GWY_GRAIN_VALUE_GROUP_AREA,
            "<i>a</i><sub>eq</sub>", "a_eq", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Equivalent disc radius"),
        GWY_GRAIN_VALUE_EQUIV_DISC_RADIUS,
        {
            GWY_GRAIN_VALUE_GROUP_AREA,
            "<i>r</i><sub>eq</sub>", "r_eq", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Area above half-height"),
        GWY_GRAIN_VALUE_HALF_HEIGHT_AREA,
        {
            GWY_GRAIN_VALUE_GROUP_AREA,
            "<i>A</i><sub>h</sub>", "A_h", NULL,
            2, 0, 0, 0,
        }
    },
    {
        /* TRANSLATORS: Grain volume calculated using basis (foot) set to
         * zero. */
        N_("Zero basis volume"),
        GWY_GRAIN_VALUE_VOLUME_0,
        {
            GWY_GRAIN_VALUE_GROUP_VOLUME,
            "<i>V</i><sub>0</sub>", "V_0", NULL,
            2, 1, 0, 0,
        }
    },
    {
        /* TRANSLATORS: Grain volume calculated using basis (foot) set to the
         * grain minimum. */
        N_("Grain minimum basis volume"),
        GWY_GRAIN_VALUE_VOLUME_MIN,
        {
            GWY_GRAIN_VALUE_GROUP_VOLUME,
            "<i>V</i><sub>min</sub>", "V_min", NULL,
            2, 1, 0, 0,
        }
    },
    {
        /* TRANSLATORS: Grain volume calculated using basis (foot) set to the
         * Laplacian background. */
        N_("Laplacian background basis volume"),
        GWY_GRAIN_VALUE_VOLUME_LAPLACE,
        {
            GWY_GRAIN_VALUE_GROUP_VOLUME,
            "<i>V</i><sub>L</sub>", "V_L", NULL,
            2, 1, 0, 0,
        }
    },
    {
        N_("Projected boundary length"),
        GWY_GRAIN_VALUE_FLAT_BOUNDARY_LENGTH,
        {
            GWY_GRAIN_VALUE_GROUP_BOUNDARY,
            "<i>L</i><sub>b0</sub>", "L_b0", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Minimum bounding size"),
        GWY_GRAIN_VALUE_MINIMUM_BOUND_SIZE,
        {
            GWY_GRAIN_VALUE_GROUP_BOUNDARY,
            "<i>D</i><sub>min</sub>", "D_min", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Minimum bounding direction"),
        GWY_GRAIN_VALUE_MINIMUM_BOUND_ANGLE,
        {
            GWY_GRAIN_VALUE_GROUP_BOUNDARY,
            "<i>φ</i><sub>min</sub>", "phi_min", NULL,
            0, 0, GWY_GRAIN_VALUE_IS_ANGLE, 0,
        }
    },
    {
        N_("Maximum bounding size"),
        GWY_GRAIN_VALUE_MAXIMUM_BOUND_SIZE,
        {
            GWY_GRAIN_VALUE_GROUP_BOUNDARY,
            "<i>D</i><sub>max</sub>", "D_max", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Maximum bounding direction"),
        GWY_GRAIN_VALUE_MAXIMUM_BOUND_ANGLE,
        {
            GWY_GRAIN_VALUE_GROUP_BOUNDARY,
            "<i>φ</i><sub>max</sub>", "phi_max", NULL,
            0, 0, GWY_GRAIN_VALUE_IS_ANGLE, 0,
        }
    },
    {
        N_("Inclination θ"),
        GWY_GRAIN_VALUE_SLOPE_THETA,
        {
            GWY_GRAIN_VALUE_GROUP_SLOPE,
            "<i>θ</i>", "theta", NULL,
            0, 0, GWY_GRAIN_VALUE_SAME_UNITS | GWY_GRAIN_VALUE_IS_ANGLE, 0,
        }
    },
    {
        N_("Inclination φ"),
        GWY_GRAIN_VALUE_SLOPE_PHI,
        {
            GWY_GRAIN_VALUE_GROUP_SLOPE,
            "<i>φ</i>", "phi", NULL,
            0, 0, GWY_GRAIN_VALUE_IS_ANGLE, 0,
        }
    },
    {
        N_("Curvature center x position"),
        GWY_GRAIN_VALUE_CURVATURE_CENTER_X,
        {
            GWY_GRAIN_VALUE_GROUP_CURVATURE,
            "<i>x</i><sub>0</sub>", "x_0", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Curvature center y position"),
        GWY_GRAIN_VALUE_CURVATURE_CENTER_Y,
        {
            GWY_GRAIN_VALUE_GROUP_CURVATURE,
            "<i>y</i><sub>0</sub>", "y_0", NULL,
            1, 0, 0, 0,
        }
    },
    {
        N_("Curvature center z value"),
        GWY_GRAIN_VALUE_CURVATURE_CENTER_Z,
        {
            GWY_GRAIN_VALUE_GROUP_CURVATURE,
            "<i>z</i><sub>0</sub>", "z_0", NULL,
            0, 1, 0, 0,
        }
    },
    {
        N_("Curvature 1"),
        GWY_GRAIN_VALUE_CURVATURE1,
        {
            GWY_GRAIN_VALUE_GROUP_CURVATURE,
            "<i>κ</i><sub>1</sub>", "kappa_1", NULL,
            -1, 0, GWY_GRAIN_VALUE_SAME_UNITS, 0,
        }
    },
    {
        N_("Curvature 2"),
        GWY_GRAIN_VALUE_CURVATURE2,
        {
            GWY_GRAIN_VALUE_GROUP_CURVATURE,
            "<i>κ</i><sub>2</sub>", "kappa_2", NULL,
            -1, 0, GWY_GRAIN_VALUE_SAME_UNITS, 0,
        }
    },
    {
        N_("Curvature angle 1"),
        GWY_GRAIN_VALUE_CURVATURE_ANGLE1,
        {
            GWY_GRAIN_VALUE_GROUP_CURVATURE,
            "<i>φ</i><sub>1</sub>", "phi_1", NULL,
            0, 0, GWY_GRAIN_VALUE_IS_ANGLE, 0,
        }
    },
    {
        N_("Curvature angle 2"),
        GWY_GRAIN_VALUE_CURVATURE_ANGLE2,
        {
            GWY_GRAIN_VALUE_GROUP_CURVATURE,
            "<i>φ</i><sub>2</sub>", "phi_2", NULL,
            0, 0, GWY_GRAIN_VALUE_IS_ANGLE, 0,
        }
    },
};

G_DEFINE_TYPE(GwyGrainValue, gwy_grain_value, GWY_TYPE_RESOURCE)

static void
gwy_grain_value_class_init(GwyGrainValueClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyResourceClass *parent_class, *res_class = GWY_RESOURCE_CLASS(klass);

    gobject_class->finalize = gwy_grain_value_finalize;

    parent_class = GWY_RESOURCE_CLASS(gwy_grain_value_parent_class);
    res_class->item_type = *gwy_resource_class_get_item_type(parent_class);

    res_class->item_type.type = G_TYPE_FROM_CLASS(klass);
    res_class->item_type.copy = gwy_grain_value_copy;
    res_class->item_type.compare = NULL;

    res_class->name = "grainvalues";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    res_class->dump = gwy_grain_value_dump;
    res_class->parse = gwy_grain_value_parse;
}

void
_gwy_grain_value_class_setup_presets(void)
{
    GwyResourceClass *klass;
    GwyGrainValue *gvalue;
    guint i;

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_GRAIN_VALUE);

    for (i = 0; i < G_N_ELEMENTS(grain_values); i++) {
        gvalue = gwy_grain_value_new(grain_values[i].name,
                                     &grain_values[i].data,
                                     TRUE);
        gvalue->builtin = grain_values[i].quantity;
        gwy_inventory_insert_item(klass->inventory, gvalue);
        g_object_unref(gvalue);
    }

    /* The gvalue added a reference so we can safely unref it again */
    g_type_class_unref(klass);
}

static void
gwy_grain_value_init(GwyGrainValue *gvalue)
{
    gwy_debug_objects_creation(G_OBJECT(gvalue));
    gwy_grain_value_data_copy(&grainvaluedata_default, &gvalue->data);
}

static void
gwy_grain_value_finalize(GObject *object)
{
    GwyGrainValue *gvalue;

    gvalue = GWY_GRAIN_VALUE(object);
    g_free(gvalue->data.symbol);
    g_free(gvalue->data.symbol_markup);

    G_OBJECT_CLASS(gwy_grain_value_parent_class)->finalize(object);
}

static void
gwy_grain_value_data_sanitize(GwyGrainValueData *data)
{
    /* TODO:
    if (!gwy_grain_value_check_size(data->size)) {
        gwy_grain_value_data_copy(&grainvaluedata_default,
                                                data);
        return;
    }

    if (!data->divisor)
        data->auto_divisor = TRUE;

    data->auto_divisor = !!data->auto_divisor;
    if (data->auto_divisor)
        gwy_grain_value_data_autodiv(data);
        */
}

static void
gwy_grain_value_data_copy(const GwyGrainValueData *src,
                          GwyGrainValueData *dest)
{
    g_free(dest->symbol);
    g_free(dest->symbol_markup);
    *dest = *src;
    dest->symbol = g_strdup(src->symbol ? src->symbol: "");
    dest->symbol_markup = g_strdup(src->symbol_markup ? src->symbol_markup: "");
}

gpointer
gwy_grain_value_copy(gpointer item)
{
    GwyGrainValue *gvalue, *copy;
    const gchar *name;

    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(item), NULL);

    gvalue = GWY_GRAIN_VALUE(item);
    name = gwy_resource_get_name(GWY_RESOURCE(item));
    copy = gwy_grain_value_new(name, &gvalue->data, FALSE);

    return copy;
}

static GwyGrainValue*
gwy_grain_value_new(const gchar *name,
                    const GwyGrainValueData *data,
                    gboolean is_const)
{
    GwyGrainValue *gvalue;

    gvalue = g_object_new(GWY_TYPE_GRAIN_VALUE,
                          "is-const", is_const,
                          NULL);
    gwy_grain_value_data_copy(data, &gvalue->data);
    g_string_assign(GWY_RESOURCE(gvalue)->name, name);
    /* New non-const resources start as modified */
    GWY_RESOURCE(gvalue)->is_modified = !is_const;

    return gvalue;
}

static void
gwy_grain_value_dump(GwyResource *resource,
                     GString *str)
{
    GwyGrainValue *gvalue;
    const GwyGrainValueData *data;

    g_return_if_fail(GWY_IS_GRAIN_VALUE(resource));
    gvalue = GWY_GRAIN_VALUE(resource);
    data = &gvalue->data;
    g_return_if_fail(data->group == GWY_GRAIN_VALUE_GROUP_USER);

    /* Information */
    g_string_append_printf(str,
                           "symbol %s\n"
                           "symbol_markup %s\n"
                           "power_xy %d\n"
                           "power_z %d\n"
                           "%s"
                           "%s"
                           "expression %s\n",
                           data->symbol,
                           data->symbol_markup,
                           data->power_xy,
                           data->power_z,
                           (data->flags & GWY_GRAIN_VALUE_SAME_UNITS)
                                ? "same_units 1\n" : "",
                           (data->flags & GWY_GRAIN_VALUE_IS_ANGLE)
                                ? "is_angle 1\n" : "",
                           gvalue->expression);
}

static gboolean
gwy_grain_value_resolve_expression(const gchar *expression,
                                   guint *vars,
                                   GError **error)
{
    static gchar **names = NULL;

    if (!expression)
        return FALSE;

    if (!expr)
        expr = gwy_expr_new();

    if (!gwy_expr_compile(expr, expression, error))
        return FALSE;

    if (!vars)
        vars = g_newa(guint, MAXBUILTINS);

    if (!names) {
        GwyGrainValue *gvalue;
        guint i;

        names = g_new(gchar*, MAXBUILTINS);
        for (i = 0; i < MAXBUILTINS; i++) {
            if ((gvalue = gwy_grain_values_get_builtin_grain_value(i)))
                names[i] = gvalue->data.symbol;
            else
                names[i] = "";  /* Impossible variable name */
        }
    }

    return !gwy_expr_resolve_variables(expr, MAXBUILTINS,
                                       (const gchar* const*)names, vars);
}

static GwyResource*
gwy_grain_value_parse(const gchar *text,
                      gboolean is_const)
{
    GwyGrainValueData data;
    GwyGrainValue *gvalue = NULL;
    GwyGrainValueClass *klass;
    gchar *str, *p, *line, *key, *value, *expression = NULL;

    g_return_val_if_fail(text, NULL);
    klass = g_type_class_peek(GWY_TYPE_GRAIN_VALUE);
    g_return_val_if_fail(klass, NULL);

    data = grainvaluedata_default;
    p = str = g_strdup(text);
    while ((line = gwy_str_next_line(&p))) {
        g_strstrip(line);
        if (!line[0] || line[0] == '#')
            continue;

        key = line;
        value = strchr(key, ' ');
        if (value) {
            *value = '\0';
            value++;
            g_strstrip(value);
        }
        if (!value || !*value) {
            g_warning("Missing value for `%s'.", key);
            continue;
        }

        if (gwy_strequal(key, "symbol")) {
            g_free(data.symbol);
            data.symbol = g_strdup(value);
        }
        else if (gwy_strequal(key, "symbol_markup")) {
            g_free(data.symbol_markup);
            data.symbol_markup = g_strdup(value);
        }
        else if (gwy_strequal(key, "power_xy"))
            data.power_xy = atoi(value);
        else if (gwy_strequal(key, "power_z"))
            data.power_z = atoi(value);
        else if (gwy_strequal(key, "same_units"))
            data.flags |= GWY_GRAIN_VALUE_SAME_UNITS*(!!atoi(value));
        else if (gwy_strequal(key, "is_angle"))
            data.flags |= GWY_GRAIN_VALUE_IS_ANGLE*(!!atoi(value));
        else if (gwy_strequal(key, "expression"))
            expression = value;
        else
            g_warning("Unknown field `%s'.", key);
    }

    if (data.symbol && gwy_grain_value_resolve_expression(expression,
                                                          NULL, NULL)) {
        gvalue = gwy_grain_value_new("", &data, is_const);
        gwy_grain_value_data_sanitize(&gvalue->data);
        gvalue->expression = g_strdup(expression);
    }

    g_free(data.symbol);
    g_free(data.symbol_markup);
    g_free(str);

    return (GwyResource*)gvalue;
}

/**
 * gwy_grain_value_get_group:
 * @gvalue: A grain value object.
 *
 * Gets the group of a grain value.
 *
 * All user-defined grain values belong to %GWY_GRAIN_VALUE_GROUP_USER group,
 * built-in grain values belong to other groups.
 *
 * Returns: The group of @gvalue.
 *
 * Since: 2.8
 **/
GwyGrainValueGroup
gwy_grain_value_get_group(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), 0);
    return gvalue->data.group;
}

/**
 * gwy_grain_value_get_symbol_markup:
 * @gvalue: A grain value object.
 *
 * Gets the rich text symbol representing a grain value.
 *
 * The returned value can contain Pango markup and is suitable for instance
 * for graph axis labels.
 *
 * Returns: Rich text symbol of @gvalue, owned by @gvalue.
 *
 * Since: 2.8
 **/
const gchar*
gwy_grain_value_get_symbol_markup(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), NULL);
    return gvalue->data.symbol_markup;
}

/**
 * gwy_grain_value_set_symbol_markup:
 * @gvalue: A grain value object.
 * @symbol: The new symbol.
 *
 * Sets the rich text symbol representing a grain value.
 *
 * See gwy_grain_value_get_symbol_markup() for details.
 *
 * Since: 2.8
 **/
void
gwy_grain_value_set_symbol_markup(GwyGrainValue *gvalue,
                                  const gchar *symbol)
{
    g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
    g_return_if_fail(gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER);
    g_return_if_fail(gwy_resource_get_is_modifiable(GWY_RESOURCE(gvalue)));

    if (gvalue->data.symbol_markup == symbol
        || (gvalue->data.symbol_markup
            && symbol
            && gwy_strequal(gvalue->data.symbol_markup, symbol)))
        return;

    g_free(gvalue->data.symbol_markup);
    gvalue->data.symbol_markup = g_strdup(symbol);
    gwy_resource_data_changed(GWY_RESOURCE(gvalue));
}

/**
 * gwy_grain_value_get_symbol:
 * @gvalue: A grain value object.
 *
 * Gets the plain symbol representing a grain value.
 *
 * The plain symbol is used in expressions.  It has to be a valid
 * identifier and it should be easy to type.  (Note currently it is not
 * possible to use user-defined grain quantities in expressions for other
 * user-defined grain quantities.)
 *
 * Returns: Plain symbol of @gvalue, owned by @gvalue.
 *
 * Since: 2.8
 **/
const gchar*
gwy_grain_value_get_symbol(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), NULL);
    return gvalue->data.symbol;
}

/**
 * gwy_grain_value_set_symbol:
 * @gvalue: A grain value object.
 * @symbol: The new symbol.
 *
 * Sets the plain symbol representing a grain value.
 *
 * See gwy_grain_value_get_symbol() for details.
 *
 * Since: 2.8
 **/
void
gwy_grain_value_set_symbol(GwyGrainValue *gvalue,
                           const gchar *symbol)
{
    g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
    g_return_if_fail(gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER);
    g_return_if_fail(gwy_resource_get_is_modifiable(GWY_RESOURCE(gvalue)));
    g_return_if_fail(!symbol || !*symbol || gwy_strisident(symbol, "_", NULL));

    if (gvalue->data.symbol == symbol
        || (gvalue->data.symbol
            && symbol
            && gwy_strequal(gvalue->data.symbol, symbol)))
        return;

    g_free(gvalue->data.symbol);
    gvalue->data.symbol = g_strdup(symbol);
    gwy_resource_data_changed(GWY_RESOURCE(gvalue));
}

/**
 * gwy_grain_value_get_power_xy:
 * @gvalue: A grain value object.
 *
 * Gets the power of lateral dimensions in a grain value.
 *
 * The units of a grain value are determined as the product of lateral units
 * and value units, raised to certain powers.  For instance lengths in the
 * horizontal plane have xy power of 1 and areas have 2, whereas volumes have
 * xy power of 2 and value power of 1.
 *
 * Returns: The power of lateral dimensions.
 *
 * Since: 2.8
 **/
gint
gwy_grain_value_get_power_xy(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), 0);
    return gvalue->data.power_xy;
}

/**
 * gwy_grain_value_set_power_xy:
 * @gvalue: A grain value object.
 * @power_xy: The new lateral dimensions power.
 *
 * Sets the power of lateral dimensions in a grain value.
 *
 * Since: 2.8
 **/
void
gwy_grain_value_set_power_xy(GwyGrainValue *gvalue,
                             gint power_xy)
{
    g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
    g_return_if_fail(gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER);
    g_return_if_fail(gwy_resource_get_is_modifiable(GWY_RESOURCE(gvalue)));

    if (gvalue->data.power_xy == power_xy)
        return;

    gvalue->data.power_xy = power_xy;
    gwy_resource_data_changed(GWY_RESOURCE(gvalue));
}

/**
 * gwy_grain_value_get_power_z:
 * @gvalue: A grain value object.
 *
 * Gets the power of value (height) in a grain value.
 *
 * See gwy_grain_value_get_power_xy() for details.
 *
 * Returns: The power of value (height).
 *
 * Since: 2.8
 **/
gint
gwy_grain_value_get_power_z(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), 0);
    return gvalue->data.power_z;
}

/**
 * gwy_grain_value_set_power_z:
 * @gvalue: A grain value object.
 * @power_z: The new value (height) power.
 *
 * Sets the power of value (height) in a grain value.
 *
 * Since: 2.8
 **/
void
gwy_grain_value_set_power_z(GwyGrainValue *gvalue,
                            gint power_z)
{
    g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
    g_return_if_fail(gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER);
    g_return_if_fail(gwy_resource_get_is_modifiable(GWY_RESOURCE(gvalue)));

    if (gvalue->data.power_z == power_z)
        return;

    gvalue->data.power_z = power_z;
    gwy_resource_data_changed(GWY_RESOURCE(gvalue));
}

/**
 * gwy_grain_value_get_flags:
 * @gvalue: A grain value object.
 *
 * Obtains the special attributes of a grain quantity.
 *
 * Returns: The special attribute flags set on @gvalue.  See
 *          #GwyGrainValueFlags for their description.
 *
 * Since: 2.8
 **/
GwyGrainValueFlags
gwy_grain_value_get_flags(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), 0);
    return gvalue->data.flags;
}

/**
 * gwy_grain_value_set_flags:
 * @gvalue: A grain value object.
 * @flags: The special attributes to set on @gvalue.
 *
 * Sets the special attributes of a grain quantity.
 *
 * The value of @flags determines the complete new set of flags.  To set
 * individual flags, obtain the current set with gwy_grain_value_get_flags()
 * first and then set/unset individual flags.
 *
 * Since: 2.8
 **/
void
gwy_grain_value_set_flags(GwyGrainValue *gvalue,
                          GwyGrainValueFlags flags)
{
    g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
    g_return_if_fail(gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER);
    g_return_if_fail(gwy_resource_get_is_modifiable(GWY_RESOURCE(gvalue)));

    if (gvalue->data.flags == flags)
        return;

    gvalue->data.flags = flags;
    gwy_resource_data_changed(GWY_RESOURCE(gvalue));
}

/**
 * gwy_grain_value_get_quantity:
 * @gvalue: A grain value object.
 *
 * Gets the built-in grain quantity corresponding to a grain value.
 *
 * Returns: The corresponding built-in #GwyGrainQuantity if @gvalue is a
 *          built-it grain value, -1 if @gvalue is an user-defined grain value.
 *
 * Since: 2.8
 **/
GwyGrainQuantity
gwy_grain_value_get_quantity(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), -1);

    if (gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER)
        return -1;

    if (gvalue->data.group == GWY_GRAIN_VALUE_GROUP_ID)
        return GWY_GRAIN_QUANTITY_ID;

    return gvalue->builtin;
}

/**
 * gwy_grain_value_get_expression:
 * @gvalue: A grain value object.
 *
 * Gets the expression of a user-defined grain value.
 *
 * Returns: The expression as a string owned by @gvalue, %NULL if @gvalue
 *          is a built-in grain value.
 *
 * Since: 2.8
 **/
const gchar*
gwy_grain_value_get_expression(GwyGrainValue *gvalue)
{
    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), NULL);

    if (gvalue->data.group != GWY_GRAIN_VALUE_GROUP_USER)
        return NULL;
    return gvalue->expression;
}

/**
 * gwy_grain_value_set_expression:
 * @gvalue: A grain value object.
 * @expression: New grain value expression.
 * @error: Return location for the error, or %NULL to ignore errors.
 *
 * Sets the expression of a user-defined grain value.
 *
 * It is an error to call this function on a built-in quantity.
 *
 * Returns: %TRUE if the expression is compilable and references only known
 *          grain quantities.  %FALSE is the expression is not calculable,
 *          in this case the @gvalue's expression is unchanged.
 *
 * Since: 2.8
 **/
gboolean
gwy_grain_value_set_expression(GwyGrainValue *gvalue,
                               const gchar *expression,
                               GError **error)
{
    GError *err = NULL;

    g_return_val_if_fail(GWY_IS_GRAIN_VALUE(gvalue), FALSE);
    g_return_val_if_fail(gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER,
                         FALSE);
    g_return_val_if_fail(gwy_resource_get_is_modifiable(GWY_RESOURCE(gvalue)),
                         FALSE);

    if (expression == gvalue->expression)
        return TRUE;

    if (!gwy_grain_value_resolve_expression(expression, NULL, &err)) {
        if (err)
            g_propagate_error(error, err);
        else
            g_set_error(error,
                        GWY_EXPR_ERROR, GWY_EXPR_ERROR_UNRESOLVED_IDENTIFIERS,
                        "Unresolved identifiers");
        return FALSE;
    }

    g_free(gvalue->expression);
    gvalue->expression = g_strdup(expression);
    gwy_resource_data_changed(GWY_RESOURCE(gvalue));

    return TRUE;
}

/**
 * gwy_grain_value_group_name:
 * @group: Grain value group.
 *
 * Obtains the name of a grain value group.
 *
 * Returns: The grain value group name as a constant untranslated string,
 *          owned by the library.
 *
 * Since: 2.8
 **/
const gchar*
gwy_grain_value_group_name(GwyGrainValueGroup group)
{
    static const gchar *group_names[] = {
        N_("Id"),
        N_("Position"),
        N_("Value"),
        N_("Area"),
        N_("Volume"),
        N_("Boundary"),
        N_("Slope"),
        N_("Curvature"),
    };

    if (group == GWY_GRAIN_VALUE_GROUP_USER)
        return N_("User");

    g_return_val_if_fail(group < G_N_ELEMENTS(group_names), NULL);
    return group_names[group];
}

/**
 * gwy_grain_values:
 *
 * Gets the inventory with all the grain values.
 *
 * Returns: Grain value inventory.
 *
 * Since: 2.8
 **/
GwyInventory*
gwy_grain_values(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek
                              (GWY_TYPE_GRAIN_VALUE))->inventory;
}

/**
 * gwy_grain_values_get_grain_value:
 * @name: Grain quantity name.
 *
 * Convenience function to get a grain quantity from gwy_grain_values() by
 * name.
 *
 * Returns: Grain quantity identified by @name or %NULL if there is no such
 *          grain quantity.
 *
 * Since: 2.8
 **/
GwyGrainValue*
gwy_grain_values_get_grain_value(const gchar *name)
{
    GwyInventory *i;

    i = GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_GRAIN_VALUE))->inventory;
    return (GwyGrainValue*)gwy_inventory_get_item(i, name);
}

static gboolean
find_grain_value_by_quantity(G_GNUC_UNUSED gpointer key,
                             gpointer value,
                             gpointer user_data)
{
    GwyGrainValue *gvalue = (GwyGrainValue*)value;

    return (gvalue->data.group != GWY_GRAIN_VALUE_GROUP_USER
            && gvalue->builtin == GPOINTER_TO_UINT(user_data));
}

/**
 * gwy_grain_values_get_builtin_grain_value:
 * @quantity: A #GwyGrainQuantity value.
 *
 * Obtains the built-in grain value corresponding to given enum value.
 *
 * Returns: The built-in grain value corresponding to @quantity, %NULL if there
 *          is no such grain value.
 *
 * Since: 2.8
 **/
GwyGrainValue*
gwy_grain_values_get_builtin_grain_value(GwyGrainQuantity quantity)
{
    return gwy_inventory_find(gwy_grain_values(),
                              find_grain_value_by_quantity,
                              GUINT_TO_POINTER(quantity));
}

/**
 * gwy_grain_values_calculate:
 * @nvalues: Number of items in @gvalues.
 * @gvalues: Array of grain value objects.
 * @results: Array of length @nvalues of arrays of length @ngrains+1 of doubles
 *           to put the calculated values to.
 * @data_field: Data field used for marking.  For some values its values
 *              are not used, but its dimensions determine the dimensions of
 *              @grains.
 * @grains: Grain numbers filled with gwy_data_field_number_grains().
 * @ngrains: The number of grains as returned by
 *           gwy_data_field_number_grains().
 *
 * Calculates a set of grain values.
 *
 * See also gwy_data_field_grains_get_quantities() for a simplier function
 * for built-in grain values.
 *
 * Since: 2.8
 **/
void
gwy_grain_values_calculate(gint nvalues,
                           GwyGrainValue **gvalues,
                           gdouble **results,
                           GwyDataField *data_field,
                           gint ngrains,
                           const gint *grains)
{
    GwyGrainValue *gvalue;
    guint vars[MAXBUILTINS];
    GwyGrainQuantity builtins[MAXBUILTINS];
    gdouble *quantities[MAXBUILTINS], *packed_quantities[MAXBUILTINS],
            *mapped[MAXBUILTINS+1];
    GList *l, *buffers = NULL;
    gboolean duplicities = FALSE;
    guint i, j, n, q;

    g_return_if_fail(GWY_IS_DATA_FIELD(data_field));
    if (!nvalues)
        return;

    /* Builtins */
    gwy_clear(quantities, MAXBUILTINS);
    gwy_clear(mapped, MAXBUILTINS+1);
    for (i = 0; i < nvalues; i++) {
        gvalue = gvalues[i];
        g_return_if_fail(GWY_IS_GRAIN_VALUE(gvalue));
        if (gvalue->data.group == GWY_GRAIN_VALUE_GROUP_USER)
            continue;

        q = gvalue->builtin;
        if (quantities[q])
            duplicities = TRUE;
        else
            quantities[q] = results[i];
    }

    /* Expressions */
    for (i = 0; i < nvalues; i++) {
        gboolean resolved;

        gvalue = gvalues[i];
        if (gvalue->data.group != GWY_GRAIN_VALUE_GROUP_USER)
            continue;
        resolved = gwy_grain_value_resolve_expression(gvalue->expression, vars,
                                                      NULL);
        g_return_if_fail(resolved);
        for (q = 0; q < MAXBUILTINS; q++) {
            if (!vars[q])
                continue;

            if (!quantities[q]) {
                quantities[q] = g_new(gdouble, ngrains + 1);
                buffers = g_list_prepend(buffers, quantities[q]);
            }
        }
    }

    /* Pack into a flat array */
    for (i = n = 0; i < MAXBUILTINS; i++) {
        if (quantities[i]) {
            if ((guint)i == GWY_GRAIN_QUANTITY_ID) {
                for (j = 0; j <= ngrains; j++)
                    quantities[i][j] = j;
            }
            else {
                builtins[n] = i;
                packed_quantities[n] = quantities[i];
                n++;
            }
        }
    }

    /* Calculate the built-in quantities */
    gwy_data_field_grains_get_quantities(data_field, packed_quantities,
                                         builtins, n, ngrains, grains);

    /* Calculate the user quantities */
    for (i = 0; i < nvalues; i++) {
        gvalue = gvalues[i];
        if (gvalue->data.group != GWY_GRAIN_VALUE_GROUP_USER)
            continue;
        gwy_grain_value_resolve_expression(gvalue->expression, vars, NULL);
        gwy_clear(mapped, MAXBUILTINS + 1);
        for (q = 0; q < MAXBUILTINS; q++) {
            if (vars[q]) {
                g_assert(quantities[q]);
                mapped[vars[q]] = quantities[q];
            }
        }
        gwy_expr_vector_execute(expr, ngrains+1, (const gdouble**)mapped,
                                results[i]);
    }

    /* Copy duplicities to all instances in @results. */
    if (duplicities) {
        for (i = 1; i < nvalues; i++) {
            gvalue = gvalues[i];
            for (j = i; j; j--) {
                if (gvalues[j-1] == gvalue)
                    break;
            }
            if (!j)
                continue;

            j--;
            memcpy(quantities[i], quantities[j], (ngrains + 1)*sizeof(gdouble));
        }
    }

    /* Free */
    for (l = buffers; l; l = g_list_next(l))
        g_free(l->data);
    g_list_free(buffers);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygrainvalue
 * @title: GwyGrainValue
 * @short_description: Grain value resource type
 **/

/**
 * GwyGrainValueFlags:
 * @GWY_GRAIN_VALUE_SAME_UNITS: Certain grain quantities, such as the surface
 *                              area or absolute inclination, are only
 *                              meaningful if value (height) is the same
 *                              physical quantity as lateral dimensions.  These
 *                              have this flag set to %TRUE.
 * @GWY_GRAIN_VALUE_IS_ANGLE: Angular quantities are internally always
 *                            calculated in radians, however they can be
 *                            presented in degrees in the user interface.
 *                            To enable such special handling, set this
 *                            flag to %TRUE.
 *
 * Special attributes of grain values.
 *
 * Since: 2.8
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
