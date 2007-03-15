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
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define BASICOPS_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register           (void);
static void     flip_horizontally         (GwyContainer *data,
                                           GwyRunType run);
static void     flip_vertically           (GwyContainer *data,
                                           GwyRunType run);
static void     invert_value              (GwyContainer *data,
                                           GwyRunType run);
static void     rotate_clockwise_90       (GwyContainer *data,
                                           GwyRunType run);
static void     rotate_counterclockwise_90(GwyContainer *data,
                                           GwyRunType run);
static void     rotate_180                (GwyContainer *data,
                                           GwyRunType run);
static void     square_samples            (GwyContainer *data,
                                           GwyRunType run);
static void     flip_xy                   (GwyDataField *source,
                                           GwyDataField *dest,
                                           gboolean minor);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Basic operations like flipping, value inversion, and rotation "
       "by multiples of 90 degrees."),
    "Yeti <yeti@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("invert_value",
                              (GwyProcessFunc)&invert_value,
                              N_("/_Basic Operations/_Invert Value"),
                              GWY_STOCK_VALUE_INVERT,
                              BASICOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Invert values about mean"));
    gwy_process_func_register("flip_horizontally",
                              (GwyProcessFunc)&flip_horizontally,
                              N_("/_Basic Operations/Flip _Horizontally"),
                              GWY_STOCK_FLIP_HORIZONTALLY,
                              BASICOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Flip data horizontally"));
    gwy_process_func_register("flip_vertically",
                              (GwyProcessFunc)&flip_vertically,
                              N_("/_Basic Operations/Flip _Vertically"),
                              GWY_STOCK_FLIP_VERTICALLY,
                              BASICOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Flip data vertically"));
    gwy_process_func_register("rotate_180",
                              (GwyProcessFunc)&rotate_180,
                              N_("/_Basic Operations/Flip _Both"),
                              GWY_STOCK_ROTATE_180,
                              BASICOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Flip data both horizontally and vertically"));
    gwy_process_func_register("rotate_90_cw",
                              (GwyProcessFunc)&rotate_clockwise_90,
                              N_("/_Basic Operations/Rotate C_lockwise"),
                              GWY_STOCK_ROTATE_90_CW,
                              BASICOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Rotate data 90 degrees clockwise"));
    gwy_process_func_register("rotate_90_ccw",
                              (GwyProcessFunc)&rotate_counterclockwise_90,
                              N_("/_Basic Operations/Rotate _Counterclockwise"),
                              GWY_STOCK_ROTATE_90_CCW,
                              BASICOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Rotate data 90 degrees counterclockwise"));
    gwy_process_func_register("square_samples",
                              (GwyProcessFunc)&square_samples,
                              N_("/_Basic Operations/S_quare Samples"),
                              NULL,
                              BASICOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Resample data with non-1:1 aspect ratio to "
                                 "square samples"));

    return TRUE;
}

static inline void
clean_quarks(guint n,
             GQuark *quarks,
             GwyDataField **dfields)
{
    guint i;

    for (i = 0; i < n; i++) {
        if (!dfields[i])
            quarks[i] = 0;
    }
}

static void
flip_horizontally(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3];
    GQuark quarks[3];
    gint i;

    g_return_if_fail(run & BASICOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_KEY, quarks + 0,
                                     GWY_APP_MASK_FIELD_KEY, quarks + 1,
                                     GWY_APP_SHOW_FIELD_KEY, quarks + 2,
                                     0);
    clean_quarks(G_N_ELEMENTS(quarks), quarks, dfields);
    gwy_app_undo_qcheckpointv(data, G_N_ELEMENTS(quarks), quarks);
    for (i = 0; i < G_N_ELEMENTS(dfields); i++) {
        if (dfields[i]) {
            gwy_data_field_invert(dfields[i], FALSE, TRUE, FALSE);
            gwy_data_field_data_changed(dfields[i]);
        }
    }
}

static void
flip_vertically(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3];
    GQuark quarks[3];
    gint i;

    g_return_if_fail(run & BASICOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_KEY, quarks + 0,
                                     GWY_APP_MASK_FIELD_KEY, quarks + 1,
                                     GWY_APP_SHOW_FIELD_KEY, quarks + 2,
                                     0);
    clean_quarks(G_N_ELEMENTS(quarks), quarks, dfields);
    gwy_app_undo_qcheckpointv(data, G_N_ELEMENTS(quarks), quarks);
    for (i = 0; i < G_N_ELEMENTS(dfields); i++) {
        if (dfields[i]) {
            gwy_data_field_invert(dfields[i], TRUE, FALSE, FALSE);
            gwy_data_field_data_changed(dfields[i]);
        }
    }
}

static void
invert_value(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[2];
    GQuark quarks[2];
    guint i;

    g_return_if_fail(run & BASICOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_SHOW_FIELD, dfields + 1,
                                     GWY_APP_DATA_FIELD_KEY, quarks + 0,
                                     GWY_APP_SHOW_FIELD_KEY, quarks + 1,
                                     0);
    clean_quarks(G_N_ELEMENTS(quarks), quarks, dfields);
    gwy_app_undo_qcheckpointv(data, G_N_ELEMENTS(quarks), quarks);
    for (i = 0; i < G_N_ELEMENTS(dfields); i++) {
        if (dfields[i]) {
            gwy_data_field_invert(dfields[i], FALSE, FALSE, TRUE);
            gwy_data_field_data_changed(dfields[i]);
        }
    }
}

static void
rotate_clockwise_90(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3], *newfield;
    GQuark quarks[3];
    gint i, id;

    g_return_if_fail(run & BASICOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_KEY, quarks + 0,
                                     GWY_APP_MASK_FIELD_KEY, quarks + 1,
                                     GWY_APP_SHOW_FIELD_KEY, quarks + 2,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    clean_quarks(G_N_ELEMENTS(quarks), quarks, dfields);
    gwy_app_undo_qcheckpointv(data, G_N_ELEMENTS(quarks), quarks);
    for (i = 0; i < G_N_ELEMENTS(dfields); i++) {
        if (dfields[i]) {
            newfield = gwy_data_field_new_alike(dfields[i], FALSE);
            flip_xy(dfields[i], newfield, FALSE);
            gwy_container_set_object(data, quarks[i], newfield);
            g_object_unref(newfield);
        }
    }
    gwy_app_data_clear_selections(data, id);
}

static void
rotate_counterclockwise_90(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3], *newfield;
    GQuark quarks[3];
    gint i, id;

    g_return_if_fail(run & BASICOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_KEY, quarks + 0,
                                     GWY_APP_MASK_FIELD_KEY, quarks + 1,
                                     GWY_APP_SHOW_FIELD_KEY, quarks + 2,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    clean_quarks(G_N_ELEMENTS(quarks), quarks, dfields);
    gwy_app_undo_qcheckpointv(data, G_N_ELEMENTS(quarks), quarks);
    for (i = 0; i < G_N_ELEMENTS(dfields); i++) {
        if (dfields[i]) {
            newfield = gwy_data_field_new_alike(dfields[i], FALSE);
            flip_xy(dfields[i], newfield, TRUE);
            gwy_container_set_object(data, quarks[i], newfield);
            g_object_unref(newfield);
        }
    }
    gwy_app_data_clear_selections(data, id);
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

static void
rotate_180(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3];
    GQuark quarks[3];
    gint i;

    g_return_if_fail(run & BASICOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_KEY, quarks + 0,
                                     GWY_APP_MASK_FIELD_KEY, quarks + 1,
                                     GWY_APP_SHOW_FIELD_KEY, quarks + 2,
                                     0);
    clean_quarks(G_N_ELEMENTS(quarks), quarks, dfields);
    gwy_app_undo_qcheckpointv(data, G_N_ELEMENTS(quarks), quarks);
    for (i = 0; i < G_N_ELEMENTS(dfields); i++) {
        if (dfields[i]) {
            gwy_data_field_invert(dfields[i], TRUE, TRUE, FALSE);
            gwy_data_field_data_changed(dfields[i]);
        }
    }
}

static void
square_samples(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *dfields[3];
    gdouble xreal, yreal, qx, qy;
    gint oldid, newid, xres, yres;
    GQuark quark;

    g_return_if_fail(run & BASICOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    dfield = dfields[0];
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    qx = xres/xreal;
    qy = yres/yreal;
    if (fabs(log(qx/qy)) > 1.0/hypot(xres, yres)) {
        /* Resample */
        if (qx < qy)
            xres = MAX(GWY_ROUND(xreal*qy), 1);
        else
            yres = MAX(GWY_ROUND(yreal*qx), 1);

        dfields[0] = gwy_data_field_new_resampled(dfields[0], xres, yres,
                                                  GWY_INTERPOLATION_BSPLINE);
        if (dfields[1]) {
            dfields[1]
                = gwy_data_field_new_resampled(dfields[1], xres, yres,
                                               GWY_INTERPOLATION_BSPLINE);
        }
        if (dfields[2]) {
            dfields[2]
                = gwy_data_field_new_resampled(dfields[2], xres, yres,
                                               GWY_INTERPOLATION_BSPLINE);
        }
    }
    else {
        /* Ratios are equal, just duplicate */
        dfields[0] = gwy_data_field_duplicate(dfields[0]);
        if (dfields[1])
            dfields[1] = gwy_data_field_duplicate(dfields[1]);
        if (dfields[2])
            dfields[2] = gwy_data_field_duplicate(dfields[2]);
    }


    newid = gwy_app_data_browser_add_data_field(dfields[0], data, TRUE);
    g_object_unref(dfields[0]);
    gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);

    if (dfields[1]) {
        quark = gwy_app_get_mask_key_for_id(newid);
        gwy_container_set_object(data, quark, dfields[1]);
        g_object_unref(dfields[1]);
    }
    if (dfields[2]) {
        quark = gwy_app_get_show_key_for_id(newid);
        gwy_container_set_object(data, quark, dfields[2]);
        g_object_unref(dfields[2]);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
