/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/app.h>

#define BASICOPS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    flip_horizontally          (GwyContainer *data,
                                               GwyRunType run);
static gboolean    flip_vertically            (GwyContainer *data,
                                               GwyRunType run);
static gboolean    invert_value               (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_clockwise_90        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_counterclockwise_90 (GwyContainer *data,
                                               GwyRunType run);
static gboolean    rotate_180                 (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "basicops",
    "Basic operations like inversion or flipping.",
    "Yeti",
    "1.0",
    "Yeti & PK",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo flip_horizontally_func_info = {
        "flip_horizontally",
        "/_Basic Operations/Flip _Horizontally",
        (GwyProcessFunc)&flip_horizontally,
        BASICOPS_RUN_MODES,
    };
    static GwyProcessFuncInfo flip_vertically_func_info = {
        "flip_vertically",
        "/_Basic Operations/Flip _Vertically",
        (GwyProcessFunc)&flip_vertically,
        BASICOPS_RUN_MODES,
    };
    static GwyProcessFuncInfo invert_value_func_info = {
        "invert_value",
        "/_Basic Operations/_Invert Value",
        (GwyProcessFunc)&invert_value,
        BASICOPS_RUN_MODES,
    };
    static GwyProcessFuncInfo rotate_clockwise_90_func_info = {
        "rotate_clockwise_90",
        "/_Basic Operations/_Rotate Clockwise (BROKEN)",
        (GwyProcessFunc)&rotate_clockwise_90,
        BASICOPS_RUN_MODES,
    };
    static GwyProcessFuncInfo rotate_counterclockwise_90_func_info = {
        "rotate_counterclockwise_90",
        "/_Basic Operations/Rotate _Counterclockwise (BROKEN)",
        (GwyProcessFunc)&rotate_counterclockwise_90,
        BASICOPS_RUN_MODES,
    };
    static GwyProcessFuncInfo rotate_180_func_info = {
        "rotate_180",
        "/_Basic Operations/Rotate 1_80 degrees (BROKEN)",
        (GwyProcessFunc)&rotate_180,
        BASICOPS_RUN_MODES,
    };

    gwy_process_func_register(name, &flip_horizontally_func_info);
    gwy_process_func_register(name, &flip_vertically_func_info);
    gwy_process_func_register(name, &invert_value_func_info);
    gwy_process_func_register(name, &rotate_clockwise_90_func_info);
    gwy_process_func_register(name, &rotate_counterclockwise_90_func_info);
    gwy_process_func_register(name, &rotate_180_func_info);

    return TRUE;
}

static gboolean
flip_horizontally(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);

    return TRUE;
}

static gboolean
flip_vertically(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    return TRUE;
}

static gboolean
invert_value(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);

    return TRUE;
}

static gboolean
rotate_clockwise_90(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_rotate(dfield, 270, GWY_INTERPOLATION_ROUND);

    return TRUE;
}

static gboolean
rotate_counterclockwise_90(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_rotate(dfield, 90, GWY_INTERPOLATION_ROUND);

    return TRUE;
}

static gboolean
rotate_180(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data");
    gwy_data_field_rotate(dfield, 180, GWY_INTERPOLATION_ROUND);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
