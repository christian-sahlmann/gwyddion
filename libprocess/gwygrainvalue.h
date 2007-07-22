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
    GWY_GRAIN_VALUE_GROUP_USER = 0,
    GWY_GRAIN_VALUE_GROUP_ID,
    GWY_GRAIN_VALUE_GROUP_POSITION,
    GWY_GRAIN_VALUE_GROUP_VALUE,
    GWY_GRAIN_VALUE_GROUP_AREA,
    GWY_GRAIN_VALUE_GROUP_VOLUME,
    GWY_GRAIN_VALUE_GROUP_BOUNDARY,
    GWY_GRAIN_VALUE_GROUP_SLOPE
} GwyGrainValueGroup;

typedef struct {
    GwyGrainValueGroup group;
    gchar *symbol;
    gchar *symbol_plain;
    gint power_xy;
    gint power_z;
    gboolean same_units;
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

GType              gwy_grain_value_get_type        (void) G_GNUC_CONST;
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
/* TODO: Evaluation methods */

GwyInventory*      gwy_grain_values                (void);
GwyGrainValue*     gwy_grain_values_get_grain_value(const gchar *name);

#endif /*__GWY_GRAIN_VALUE_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
