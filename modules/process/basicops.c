/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

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
static void        flip_xy                    (GwyDataField *source,
                                               GwyDataField *dest,
                                               gboolean minor);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Basic operations like flipping, value inversion, and rotation "
       "by multiples of 90 degrees."),
    "Yeti <yeti@gwyddion.net>",
    "1.0.1",
    "David Nečas (Yeti) & Petr Klapetek",
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
        N_("/_Basic Operations/Flip _Horizontally"),
        (GwyProcessFunc)&flip_horizontally,
        BASICOPS_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo flip_vertically_func_info = {
        "flip_vertically",
        N_("/_Basic Operations/Flip _Vertically"),
        (GwyProcessFunc)&flip_vertically,
        BASICOPS_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo invert_value_func_info = {
        "invert_value",
        N_("/_Basic Operations/_Invert Value"),
        (GwyProcessFunc)&invert_value,
        BASICOPS_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo rotate_clockwise_90_func_info = {
        "rotate_clockwise_90",
        N_("/_Basic Operations/_Rotate Clockwise"),
        (GwyProcessFunc)&rotate_clockwise_90,
        BASICOPS_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo rotate_counterclockwise_90_func_info = {
        "rotate_counterclockwise_90",
        N_("/_Basic Operations/Rotate _Counterclockwise"),
        (GwyProcessFunc)&rotate_counterclockwise_90,
        BASICOPS_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo rotate_180_func_info = {
        "rotate_180",
        N_("/_Basic Operations/Flip _Both"),
        (GwyProcessFunc)&rotate_180,
        BASICOPS_RUN_MODES,
        0,
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
    const gchar *keys[3];
    gsize n;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    n = 0;
    keys[n++] = "/0/data";
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield))
        keys[n++] = "/0/mask";
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield))
        keys[n++] = "/0/show";
    gwy_app_undo_checkpointv(data, n, keys);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);
    gwy_data_field_data_changed(dfield);
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield)) {
        gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);
        gwy_data_field_data_changed(dfield);
    }
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield)) {
        gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);
        gwy_data_field_data_changed(dfield);
    }

    return TRUE;
}

static gboolean
flip_vertically(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    const gchar *keys[3];
    gsize n;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    n = 0;
    keys[n++] = "/0/data";
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield))
        keys[n++] = "/0/mask";
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield))
        keys[n++] = "/0/show";
    gwy_app_undo_checkpointv(data, n, keys);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
    gwy_data_field_data_changed(dfield);
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield)) {
        gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
        gwy_data_field_data_changed(dfield);
    }
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield)) {
        gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
        gwy_data_field_data_changed(dfield);
    }

    return TRUE;
}

static gboolean
invert_value(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    const gchar *keys[2];
    gsize n;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    n = 0;
    keys[n++] = "/0/data";
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield))
        keys[n++] = "/0/show";
    gwy_app_undo_checkpointv(data, n, keys);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);
    gwy_data_field_data_changed(dfield);
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield)) {
        gwy_data_field_invert(dfield, FALSE, FALSE, TRUE);
        gwy_data_field_data_changed(dfield);
    }

    return TRUE;
}

static gboolean
rotate_clockwise_90(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield, *old;
    GwyContainer *newdata;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    old = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    newdata = gwy_container_duplicate(data);
    gwy_app_clean_up_data(newdata);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(newdata,
                                                             "/0/data"));
    flip_xy(old, dfield, FALSE);
    if (gwy_container_gis_object_by_name(data, "/0/mask", &old)) {
        dfield = gwy_container_get_object_by_name(newdata, "/0/mask");
        flip_xy(old, dfield, FALSE);
    }
    if (gwy_container_gis_object_by_name(data, "/0/show", &old)) {
        dfield = gwy_container_get_object_by_name(newdata, "/0/show");
        flip_xy(old, dfield, FALSE);
    }
    data_window = gwy_app_data_window_create(newdata);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

static gboolean
rotate_counterclockwise_90(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield, *old;
    GwyContainer *newdata;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    old = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    newdata = gwy_container_duplicate(data);
    gwy_app_clean_up_data(newdata);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(newdata,
                                                             "/0/data"));
    flip_xy(old, dfield, TRUE);
    if (gwy_container_gis_object_by_name(data, "/0/mask", &old)) {
        dfield = gwy_container_get_object_by_name(newdata, "/0/mask");
        flip_xy(old, dfield, TRUE);
    }
    if (gwy_container_gis_object_by_name(data, "/0/show", &old)) {
        dfield = gwy_container_get_object_by_name(newdata, "/0/show");
        flip_xy(old, dfield, TRUE);
    }
    data_window = gwy_app_data_window_create(newdata);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

static void
flip_xy(GwyDataField *source, GwyDataField *dest, gboolean minor)
{
    gint xres, yres, i, j;
    gdouble *dd;
    const gdouble *sd;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    gwy_data_field_resample(dest, yres, xres, GWY_INTERPOLATION_NONE);
    sd = gwy_data_field_get_data_const(source);
    dd = gwy_data_field_get_data(dest);
    if (minor) {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + j] = sd[j*xres + (xres - 1 - i)];
            }
        }
    }
    else {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + (yres - 1 - j)] = sd[j*xres + i];
            }
        }
    }
    gwy_data_field_set_xreal(dest, gwy_data_field_get_yreal(source));
    gwy_data_field_set_yreal(dest, gwy_data_field_get_xreal(source));
}

static gboolean
rotate_180(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    const gchar *keys[3];
    gsize n;

    g_return_val_if_fail(run & BASICOPS_RUN_MODES, FALSE);
    n = 0;
    keys[n++] = "/0/data";
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield))
        keys[n++] = "/0/mask";
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield))
        keys[n++] = "/0/show";
    gwy_app_undo_checkpointv(data, n, keys);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_data_field_rotate(dfield, G_PI, GWY_INTERPOLATION_ROUND);
    gwy_data_field_data_changed(dfield);
    if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield)) {
        gwy_data_field_rotate(dfield, G_PI, GWY_INTERPOLATION_ROUND);
        gwy_data_field_data_changed(dfield);
    }
    if (gwy_container_gis_object_by_name(data, "/0/show", &dfield)) {
        gwy_data_field_rotate(dfield, G_PI, GWY_INTERPOLATION_ROUND);
        gwy_data_field_data_changed(dfield);
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
