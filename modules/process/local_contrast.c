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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/gwyapp.h>

#define CONTRAST_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


static gboolean    module_register              (const gchar *name);
static gboolean    maximize_local_contrast      (GwyContainer *data,
                                                 GwyRunType run);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "local_contrast",
    N_("Local contrast maximization"),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo max_local_contrast_func_info = {
        "maximize_local_contrast",
        N_("/_Display/_Local Contrast"),
        (GwyProcessFunc)&maximize_local_contrast,
        CONTRAST_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &max_local_contrast_func_info);

    return TRUE;
}

static gboolean
maximize_local_contrast(GwyContainer *data, GwyRunType run)
{
    static const gdouble weight[] = {
        4, 3, 2, 1,
    };
    GwyDataField *dfield, *minfield, *maxfield, *showfield;
    const gdouble *dat, *min, *max;
    gdouble *show;
    gdouble mins, maxs, v, vc, minv, maxv;
    gdouble sum, gmin, gmax;
    gdouble amount = 0.7;
    gint xres, yres, i, j, k, l;
    gint size = 7;
    gint steps = G_N_ELEMENTS(weight);

    g_return_val_if_fail(run & CONTRAST_RUN_MODES, FALSE);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gmin = gwy_data_field_get_min(dfield);
    gmax = gwy_data_field_get_max(dfield);
    if (gmax == gmin)
        return FALSE;

    gwy_app_undo_checkpoint(data, "/0/show", NULL);
    if (gwy_container_gis_object_by_name(data, "/0/show",
                                         (GObject**)&showfield)) {
        gwy_data_field_resample(showfield, xres, yres, GWY_INTERPOLATION_NONE);
        gwy_data_field_area_copy(dfield, showfield, 0, 0, xres, yres, 0, 0);
    }
    else {
        showfield = GWY_DATA_FIELD(gwy_data_field_new_alike(dfield, FALSE));
        gwy_container_set_object_by_name(data, "/0/show", G_OBJECT(showfield));
        g_object_unref(showfield);
    }

    minfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_minimum(minfield, size);

    maxfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_filter_maximum(maxfield, size);

    dat = gwy_data_field_get_data_const(dfield);
    min = gwy_data_field_get_data_const(minfield);
    max = gwy_data_field_get_data_const(maxfield);
    show = gwy_data_field_get_data(showfield);

    sum = 0;
    for (k = 0; k < G_N_ELEMENTS(weight); k++)
        sum += weight[k];

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            minv = maxv = dat[i*xres + j];
            mins = minv*weight[0];
            maxs = maxv*weight[0];
            for (k = 1; k < steps; k++) {
                for (l = 0; l < 2*k+1; l++) {
                    /* top line */
                    v = max[MAX(0, i - k*size)*xres
                            + CLAMP(j + (l - k)*size, 0, xres-1)];
                    if (v > maxv)
                        maxv = v;

                    v = min[MAX(0, i - k*size)*xres
                            + CLAMP(j + (l - k)*size, 0, xres-1)];
                    if (v < minv)
                        minv = v;

                    /* bottom line */
                    v = max[MIN(0, i + k*size)*xres
                            + CLAMP(j + (l - k)*size, 0, xres-1)];
                    if (v > maxv)
                        maxv = v;

                    v = min[MIN(0, i + k*size)*xres
                            + CLAMP(j + (l - k)*size, 0, xres-1)];
                    if (v < minv)
                        minv = v;

                    /* left line */
                    v = max[CLAMP(i + (l - k)*size, 0, yres-1)*xres
                            + MAX(0, j - k*size)];
                    if (v > maxv)
                        maxv = v;

                    v = min[CLAMP(i + (l - k)*size, 0, yres-1)*xres
                            + MAX(0, j - k*size)];
                    if (v < minv)
                        minv = v;

                    /* right line */
                    v = max[CLAMP(i + (l - k)*size, 0, yres-1)*xres
                            + MIN(xres-1, j + k*size)];
                    if (v > maxv)
                        maxv = v;

                    v = min[CLAMP(i + (l - k)*size, 0, yres-1)*xres
                            + MIN(xres-1, j + k*size)];
                    if (v < minv)
                        minv = v;
                }
                mins += minv*weight[k];
                maxs += maxv*weight[k];
            }
            mins /= sum;
            maxs /= sum;
            v = dat[i*xres + j];
            if (G_LIKELY(mins < maxs)) {
                vc = (gmax - gmin)/(maxs - mins)*(v - mins) + gmin;
                v = amount*vc + (1.0 - amount)*v;
                v = CLAMP(v, gmin, gmax);
            }
            show[i*xres +j] = v;
        }
    }

    g_object_unref(minfield);
    g_object_unref(maxfield);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
