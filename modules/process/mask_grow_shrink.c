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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#define MASK_GROW_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    mask_shrink                (GwyContainer *data,
                                               GwyRunType run);
static gboolean    mask_grow                  (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "mask_grow_shrink",
    "Grows or shrinks masks.",
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
    static GwyProcessFuncInfo mask_grow_func_info = {
        "mask_grow",
        "/_Mask Operations/_Grow mask...",
        (GwyProcessFunc)&mask_grow,
        MASK_GROW_RUN_MODES,
    };
    static GwyProcessFuncInfo mask_shrink_func_info = {
        "mask_shrink",
        "/_Mask Operations/_Shrink mask...",
        (GwyProcessFunc)&mask_shrink,
        MASK_GROW_RUN_MODES,
    };

    gwy_process_func_register(name, &mask_shrink_func_info);
    gwy_process_func_register(name, &mask_grow_func_info);

    gwy_process_func_set_sensitivity_flags(mask_shrink_func_info.name,
                                           GWY_MENU_FLAG_DATA_MASK);
    gwy_process_func_set_sensitivity_flags(mask_grow_func_info.name,
                                           GWY_MENU_FLAG_DATA_MASK);

    return TRUE;
}

static gboolean
mask_grow(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    gdouble *dfdata, *buffer, *prow;
    gdouble q1, q2;
    gint xres, yres, rowstride;
    gint i, j;

    g_return_val_if_fail(run & MASK_GROW_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    g_return_val_if_fail(dfield, FALSE);

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    dfdata = gwy_data_field_get_data(dfield);
    rowstride = xres;

    buffer = g_new(gdouble, xres);
    prow = g_new(gdouble, xres);
    for (j = 0; j < xres; j++)
        prow[j] = -G_MAXDOUBLE;
    memcpy(buffer, dfdata, xres*sizeof(gdouble));
    for (i = 0; i < yres; i++) {
        gdouble *row = dfdata + i*xres;

        if (i == yres-1)
            rowstride = 0;

        j = 0;
        q2 = MAX(buffer[j], buffer[j+1]);
        q1 = MAX(prow[j], row[j+rowstride]);
        row[j] = MAX(q1, q2);
        for (j = 1; j < xres-1; j++) {
            q1 = MAX(prow[j], buffer[j-1]);
            q2 = MAX(buffer[j], buffer[j+1]);
            q2 = MAX(q2, row[j+rowstride]);
            row[j] = MAX(q1, q2);
        }
        j = xres-1;
        q2 = MAX(buffer[j-1], buffer[j]);
        q1 = MAX(prow[j], row[j+rowstride]);
        row[j] = MAX(q1, q2);

        GWY_SWAP(gdouble*, prow, buffer);
        if (i < yres-1)
            memcpy(buffer, dfdata + (i+1)*xres, xres*sizeof(gdouble));
    }
    g_free(buffer);
    g_free(prow);

    return TRUE;
}

static gboolean
mask_shrink(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    gdouble *dfdata, *buffer, *prow;
    gdouble q1, q2;
    gint xres, yres, rowstride;
    gint i, j;

    g_return_val_if_fail(run & MASK_GROW_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    g_return_val_if_fail(dfield, FALSE);

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    dfdata = gwy_data_field_get_data(dfield);
    rowstride = xres;

    buffer = g_new(gdouble, xres);
    prow = g_new(gdouble, xres);
    for (j = 0; j < xres; j++)
        prow[j] = -G_MINDOUBLE;
    memcpy(buffer, dfdata, xres*sizeof(gdouble));
    for (i = 0; i < yres; i++) {
        gdouble *row = dfdata + i*xres;

        if (i == yres-1)
            rowstride = 0;

        j = 0;
        q2 = MIN(buffer[j], buffer[j+1]);
        q1 = MIN(prow[j], row[j+rowstride]);
        row[j] = MIN(q1, q2);
        for (j = 1; j < xres-1; j++) {
            q1 = MIN(prow[j], buffer[j-1]);
            q2 = MIN(buffer[j], buffer[j+1]);
            q2 = MIN(q2, row[j+rowstride]);
            row[j] = MIN(q1, q2);
        }
        j = xres-1;
        q2 = MIN(buffer[j-1], buffer[j]);
        q1 = MIN(prow[j], row[j+rowstride]);
        row[j] = MIN(q1, q2);

        GWY_SWAP(gdouble*, prow, buffer);
        if (i < yres-1)
            memcpy(buffer, dfdata + (i+1)*xres, xres*sizeof(gdouble));
    }
    g_free(buffer);
    g_free(prow);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
