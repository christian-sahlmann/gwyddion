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

#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#define SCARS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    scars_mark_scars           (GwyContainer *data,
                                               GwyRunType run);
static gboolean    scars_remove_scars         (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "scars",
    "Scar detection and removal.",
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo scars_mark_scars_func_info = {
        "scars_mark",
        "/_Correct Data/M_ark Scars",
        (GwyProcessFunc)&scars_mark_scars,
        SCARS_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo scars_remove_scars_func_info = {
        "scars_remove",
        "/_Correct Data/Remove _Scars",
        (GwyProcessFunc)&scars_remove_scars,
        SCARS_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &scars_mark_scars_func_info);
    gwy_process_func_register(name, &scars_remove_scars_func_info);

    return TRUE;
}

/* FIXME: should be parameters */
static const gdouble threshold_high = 0.666;
static const gdouble threshold_low = 0.25;
static const gint min_len = 12;
static const gint max_width = 4;

static gboolean
scars_mark_scars(GwyContainer *data, GwyRunType run)
{
    GObject *dfield, *mask;

    g_return_val_if_fail(run & SCARS_RUN_MODES, FALSE);
    dfield = gwy_container_get_object_by_name(data, "/0/data");

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    if (!gwy_container_gis_object_by_name(data, "/0/mask", &mask)) {
        mask = gwy_serializable_duplicate(dfield);
        gwy_container_set_object_by_name(data, "/0/mask", mask);
        g_object_unref(mask);
    }
    gwy_data_field_mark_scars(GWY_DATA_FIELD(dfield), GWY_DATA_FIELD(mask),
                              threshold_high, threshold_low,
                              min_len, max_width);

    return TRUE;
}

static gboolean
scars_remove_scars(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mask;
    gint xres, yres, i, j, k;
    gdouble *d, *m;

    g_return_val_if_fail(run & SCARS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    mask = GWY_DATA_FIELD(gwy_data_field_new(xres, yres,
                                             gwy_data_field_get_xreal(dfield),
                                             gwy_data_field_get_yreal(dfield),
                                             FALSE));
    gwy_data_field_mark_scars(dfield, mask,
                              threshold_high, threshold_low,
                              min_len, max_width);
    m = gwy_data_field_get_data(mask);

    /* interpolate */
    for (i = 1; i < yres-1; i++) {
        for (j = 0; j < xres; j++) {
            if (m[i*xres + j] > 0.0) {
                gdouble first, last;
                gint width;

                first = d[(i - 1)*xres + j];
                for (k = 1; m[(i + k)*xres + j] > 0.0; k++)
                    ;
                last = d[(i + k)*xres + j];
                width = k + 1;
                while (k) {
                    gdouble x = (gdouble)k/width;

                    d[(i + k - 1)*xres + j] = x*last + (1.0 - x)*first;
                    m[(i + k - 1)*xres + j] = 0.0;
                    k--;
                }
            }
        }
    }

    g_object_unref(mask);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
