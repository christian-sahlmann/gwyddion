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

#define LINECORR_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    line_correct_modus         (GwyContainer *data,
                                               GwyRunType run);
static gboolean    line_correct_remove_scars          (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "line_correct",
    "Simple automatic line correction.",
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo line_correct_modus_func_info = {
        "line_correct_modus",
        "/_Correct Data/_Modus Line Correction (Exp!)",
        (GwyProcessFunc)&line_correct_modus,
        LINECORR_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo line_correct_remove_scars_func_info = {
        "line_correct_remove_scars",
        "/_Correct Data/Remove _Scars (Exp!)",
        (GwyProcessFunc)&line_correct_remove_scars,
        LINECORR_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &line_correct_modus_func_info);
    gwy_process_func_register(name, &line_correct_remove_scars_func_info);

    return TRUE;
}

static gboolean
line_correct_modus(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GwyDataLine *line, *modi;
    gint xres, yres, i;
    gdouble modus;

    g_return_val_if_fail(run & LINECORR_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    xres = gwy_data_field_get_xres(dfield);
    line = (GwyDataLine*)gwy_data_line_new(xres, 1.0, FALSE);
    yres = gwy_data_field_get_yres(dfield);
    modi = (GwyDataLine*)gwy_data_line_new(yres, 1.0, FALSE);

    for (i = 0; i < yres; i++) {
        gwy_data_field_get_row(dfield, line, i);
        modus = gwy_data_line_get_modus(line, 0);
        gwy_data_line_set_val(modi, i, modus);
    }
    modus = gwy_data_line_get_modus(modi, 0);

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    for (i = 0; i < yres; i++) {
        gwy_data_field_area_add(dfield, 0, i, xres, i+1,
                                modus - gwy_data_line_get_val(modi, i));
    }

    g_object_unref(modi);
    g_object_unref(line);

    return TRUE;
}

static gboolean
line_correct_remove_scars(GwyContainer *data, GwyRunType run)
{
    /* FIXME: should be parameters */
    static const gdouble threshold_high = 0.666;
    static const gdouble threshold_low = 0.25;
    static const gint min_len = 8;

    GwyDataField *dfield;
    GObject *mask;
    gint xres, yres, i, j, k;
    gdouble rms;
    gdouble *d, *m, *row;

    g_return_val_if_fail(run & LINECORR_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);

    rms = 0.0;
    for (i = 1; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble z = d[i*xres + j] - d[(i - 1)*xres + j];

            rms += z*z;
        }
    }
    rms = sqrt(rms/(xres*yres));
    if (rms == 0.0)
        return FALSE;

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    mask = gwy_data_field_new(xres, yres,
                              gwy_data_field_get_xreal(dfield),
                              gwy_data_field_get_yreal(dfield),
                              TRUE);
    m = gwy_data_field_get_data(GWY_DATA_FIELD(mask));

    /* preliminary mark */
    for (i = 1; i < yres-1; i++) {
        for (j = 0; j < xres; j++) {
            gdouble prev = d[(i - 1)*xres + j];
            gdouble next = d[(i + 1)*xres + j];
            gdouble z = d[i*xres + j];

            z -= MAX(prev, next);
            m[i*xres + j] = z/rms;
        }
    }
    /* expand high threshold to neighbouring low threshold */
    for (i = 1; i < yres-1; i++) {
        row = m + i*xres;
        for (j = 1; j < xres; j++) {
            if (row[j] >= threshold_low && row[j-1] >= threshold_high)
                row[j] = threshold_high;
        }
        for (j = xres-1; j > 0; j--) {
            if (row[j-1] >= threshold_low && row[j] >= threshold_high)
                row[j-1] = threshold_high;
        }
    }
    /* kill too short segments, clamping mask along the way */
    for (i = 1; i < yres-1; i++) {
        row = m + i*xres;
        k = 0;
        for (j = 0; j < xres; j++) {
            if (row[j] >= threshold_high) {
                row[j] = 1.0;
                k++;
            }
            else if (k && k < min_len) {
                while (k) {
                    row[j-k] = 0.0;
                    k--;
                }
                row[j] = 0.0;
            }
            else
                row[j] = 0.0;
        }
    }
    /* interpolate
     * FIXME: if it somehow happend two lines touch vertically, bad things
     * happen */
    for (i = 1; i < yres-1; i++) {
        for (j = 0; j < xres; j++) {
            if (m[i*xres + j] > 0.0)
                d[i*xres + j] = (d[(i - 1)*xres + j] + d[(i + 1)*xres + j])/2;
        }
    }

    g_object_unref(mask);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
