/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <app/gwyapp.h>

#define LEVEL_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    level                      (GwyContainer *data,
                                               GwyRunType run);
static gboolean    level_rotate               (GwyContainer *data,
                                               GwyRunType run);
static gboolean    fix_zero                   (GwyContainer *data,
                                               GwyRunType run);
static gboolean    zero_mean                  (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Levels data by simple plane subtraction or by rotation, "
       "and fixes minimal or mean value to zero."),
    "Yeti <yeti@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo level_func_info = {
        "level",
        N_("/_Level/_Level"),
        (GwyProcessFunc)&level,
        LEVEL_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo level_rotate_func_info = {
        "level_rotate",
        N_("/_Level/Level _Rotate"),
        (GwyProcessFunc)&level_rotate,
        LEVEL_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo fix_zero_func_info = {
        "fix_zero",
        N_("/_Level/Fix _Zero"),
        (GwyProcessFunc)&fix_zero,
        LEVEL_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo zero_mean_func_info = {
        "zero_mean",
        N_("/_Level/Zero _Mean Value"),
        (GwyProcessFunc)&zero_mean,
        LEVEL_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &level_func_info);
    gwy_process_func_register(name, &level_rotate_func_info);
    gwy_process_func_register(name, &fix_zero_func_info);
    gwy_process_func_register(name, &zero_mean_func_info);

    return TRUE;
}

static gboolean
level(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    gdouble c, bx, by;

    g_return_val_if_fail(run & LEVEL_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    gwy_data_field_plane_coeffs(dfield, &c, &bx, &by);
    c = -0.5*(bx*gwy_data_field_get_xreal(dfield)
              + by*gwy_data_field_get_yreal(dfield));
    gwy_data_field_plane_level(dfield, c, bx, by);

    return TRUE;
}

static gboolean
level_rotate(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    gdouble a, b, c;

    g_return_val_if_fail(run & LEVEL_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    gwy_data_field_plane_coeffs(dfield, &a, &b, &c);
    gwy_data_field_plane_rotate(dfield,
                                180/G_PI*atan2(b, 1),
                                180/G_PI*atan2(c, 1),
                                GWY_INTERPOLATION_BILINEAR);
    gwy_debug("b = %g, alpha = %g deg, c = %g, beta = %g deg",
              b, 180/G_PI*atan2(b, 1), c, 180/G_PI*atan2(c, 1));

    return TRUE;
}

static gboolean
fix_zero(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & LEVEL_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    gwy_data_field_add(dfield, -gwy_data_field_get_min(dfield));

    return TRUE;
}

static gboolean
zero_mean(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & LEVEL_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
