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

#ifndef __GWY_GRAIN_VALUE_H__
#define __GWY_GRAIN_VALUE_H__

#include <libgwyddion/gwyresource.h>
#include <libprocess/grains.h>

#define GWY_TYPE_GRAIN_VALUE             (gwy_grain_value_get_type())
#define GWY_GRAIN_VALUE(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAIN_VALUE, GwyGrainValue))
#define GWY_GRAIN_VALUE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAIN_VALUE, GwyGrainValueClass))
#define GWY_IS_GRAIN_VALUE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAIN_VALUE))
#define GWY_IS_GRAIN_VALUE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAIN_VALUE))
#define GWY_GRAIN_VALUE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAIN_VALUE, GwyGrainValueClass))

typedef struct _GwyGrainValue      GwyGrainValue;
typedef struct _GwyGrainValueClass GwyGrainValueClass;

typedef enum {
    GWY_GRAIN_VALUE_GROUP_ID = 0,
    GWY_GRAIN_VALUE_GROUP_POSITION,
    GWY_GRAIN_VALUE_GROUP_VALUE,
    GWY_GRAIN_VALUE_GROUP_AREA,
    GWY_GRAIN_VALUE_GROUP_VOLUME,
    GWY_GRAIN_VALUE_GROUP_BOUNDARY,
    GWY_GRAIN_VALUE_GROUP_SLOPE,
    GWY_GRAIN_VALUE_GROUP_CURVATURE,
    GWY_GRAIN_VALUE_GROUP_USER = 30
} GwyGrainValueGroup;

typedef enum {
    GWY_GRAIN_VALUE_SAME_UNITS = 1 << 0,
    GWY_GRAIN_VALUE_IS_ANGLE   = 1 << 1
} GwyGrainValueFlags;

typedef struct {
    GwyGrainValueGroup group;
    gchar *symbol_markup;
    gchar *symbol;
    gchar *reserveds;
    gint power_xy;
    gint power_z;
    guint flags;
    gint reservedi;
} GwyGrainValueData;

struct _GwyGrainValue {
    GwyResource parent_instance;

    GwyGrainValueData data;
    GwyGrainQuantity builtin;
    gchar *expression;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyGrainValueClass {
    GwyResourceClass parent_class;

    void (*reserved1)(void);
    void (*reserved2)(void);
};

GType              gwy_grain_value_get_type         (void) G_GNUC_CONST;
GwyGrainValueGroup gwy_grain_value_get_group        (GwyGrainValue *gvalue);
const gchar*       gwy_grain_value_get_symbol       (GwyGrainValue *gvalue);
void               gwy_grain_value_set_symbol       (GwyGrainValue *gvalue,
                                                     const gchar *symbol);
const gchar*       gwy_grain_value_get_symbol_markup(GwyGrainValue *gvalue);
void               gwy_grain_value_set_symbol_markup(GwyGrainValue *gvalue,
                                                     const gchar *symbol);
gint               gwy_grain_value_get_power_xy     (GwyGrainValue *gvalue);
void               gwy_grain_value_set_power_xy     (GwyGrainValue *gvalue,
                                                     gint power_xy);
gint               gwy_grain_value_get_power_z      (GwyGrainValue *gvalue);
void               gwy_grain_value_set_power_z      (GwyGrainValue *gvalue,
                                                     gint power_z);
GwyGrainValueFlags gwy_grain_value_get_flags        (GwyGrainValue *gvalue);
void               gwy_grain_value_set_flags        (GwyGrainValue *gvalue,
                                                     GwyGrainValueFlags flags);
GwyGrainQuantity   gwy_grain_value_get_quantity     (GwyGrainValue *gvalue);
const gchar*       gwy_grain_value_get_expression   (GwyGrainValue *gvalue);
gboolean           gwy_grain_value_set_expression   (GwyGrainValue *gvalue,
                                                     const gchar *expression,
                                                     GError **error);
const gchar*       gwy_grain_value_group_name       (GwyGrainValueGroup group);

GwyInventory*  gwy_grain_values                          (void);
GwyGrainValue* gwy_grain_values_get_grain_value          (const gchar *name);
GwyGrainValue* gwy_grain_values_get_builtin_grain_value  (GwyGrainQuantity quantity);

void gwy_grain_values_calculate(gint nvalues,
                                GwyGrainValue **gvalues,
                                gdouble **results,
                                GwyDataField *data_field,
                                gint ngrains,
                                const gint *grains);

#endif /*__GWY_GRAIN_VALUE_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
