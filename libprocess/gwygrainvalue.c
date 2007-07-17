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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyexpr.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/gwygrainvalue.h>
#include "gwyprocessinternal.h"

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
    GWY_GRAIN_VALUE_GROUP_USER, NULL, NULL, 0, 0, FALSE,
};

static GwyExpr *expr = NULL;

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

    res_class->name = "grainvalue";
    res_class->inventory = gwy_inventory_new(&res_class->item_type);
    res_class->dump = gwy_grain_value_dump;
    res_class->parse = gwy_grain_value_parse;
}

void
_gwy_grain_value_class_setup_presets(void)
{
    GwyResourceClass *klass;
    /* GwyGrainValue *gvalue; */

    /* Force class instantiation, this function is called before it's first
     * referenced. */
    klass = g_type_class_ref(GWY_TYPE_GRAIN_VALUE);

    /* TODO: Insert builtin quantities */
    /*
    gvalue = gwy_grain_value_new(GWY_GRAIN_VALUE_DEFAULT,
                                 &grainvaluedata_default, TRUE);
    gwy_inventory_insert_item(klass->inventory, gvalue);
    g_object_unref(gvalue);
    */

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
    g_free(gvalue->data.symbol_plain);

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
    g_free(dest->symbol_plain);
    *dest = *src;
    dest->symbol = g_strdup(src->symbol ? src->symbol: "");
    dest->symbol_plain = g_strdup(src->symbol_plain ? src->symbol_plain: "");
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
                           "symbol_plain %s\n"
                           "power_xy %d\n"
                           "power_z %d\n"
                           "same_units %d\n"
                           "expression %s\n",
                           data->symbol,
                           data->symbol_plain,
                           data->power_xy,
                           data->power_z,
                           data->same_units,
                           gvalue->expression);
}

static gboolean
gwy_grain_value_check_expression(const gchar *expression)
{
    if (!expression)
        return FALSE;

    if (!expr)
        expr = gwy_expr_new();

    return gwy_expr_compile(expr, expression, NULL);
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
        else if (gwy_strequal(key, "symbol_plain")) {
            g_free(data.symbol);
            data.symbol = g_strdup(value);
        }
        else if (gwy_strequal(key, "power_xy"))
            data.power_xy = atoi(value);
        else if (gwy_strequal(key, "power_z"))
            data.power_z = atoi(value);
        else if (gwy_strequal(key, "same_units"))
            data.same_units = !!atoi(value);
        else if (gwy_strequal(key, "expression"))
            expression = value;
        else
            g_warning("Unknown field `%s'.", key);
    }


    if (data.symbol && !gwy_grain_value_check_expression(expression)) {
        gvalue = gwy_grain_value_new("", &data, is_const);
        gwy_grain_value_data_sanitize(&gvalue->data);
        gvalue->expression = g_strdup(expression);
    }

    g_free(data.symbol);
    g_free(data.symbol_plain);
    g_free(str);

    return (GwyResource*)gvalue;
}

/* TODO */
GwyGrainValueGroup gwy_grain_value_get_group       (GwyGrainValue *gvalue);
const gchar*       gwy_grain_value_get_symbol      (GwyGrainValue *gvalue);
const gchar*       gwy_grain_value_get_symbol_plain(GwyGrainValue *gvalue);
void               gwy_grain_value_set_symbol      (GwyGrainValue *gvalue,
                                                    const gchar *symbol);
void               gwy_grain_value_set_symbol_plain(GwyGrainValue *gvalue,
                                                    const gchar *symbol);
gint               gwy_grain_value_get_power_xy    (GwyGrainValue *gvalue);
void               gwy_grain_value_set_power_xy    (GwyGrainValue *gvalue,
                                                    gint power_xy);
gint               gwy_grain_value_get_power_z     (GwyGrainValue *gvalue);
void               gwy_grain_value_set_power_z     (GwyGrainValue *gvalue,
                                                    gint power_z);
gboolean           gwy_grain_value_get_same_units  (GwyGrainValue *gvalue);
void               gwy_grain_value_set_same_units  (GwyGrainValue *gvalue,
                                                    gboolean same_units);

GwyInventory*
gwy_grain_values(void)
{
    return GWY_RESOURCE_CLASS(g_type_class_peek
                              (GWY_TYPE_GRAIN_VALUE))->inventory;
}

GwyGrainValue*
gwy_grain_values_get_grain_value(const gchar *name)
{
    GwyInventory *i;

    i = GWY_RESOURCE_CLASS(g_type_class_peek(GWY_TYPE_GRAIN_VALUE))->inventory;
    return (GwyGrainValue*)gwy_inventory_get_item_or_default(i, name);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
