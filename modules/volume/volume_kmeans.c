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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libprocess/datafield.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>

#define VOLUME_KMEANS_RUN_MODES (GWY_RUN_IMMEDIATE)

static gboolean module_register                    (void);
static void     volume_kmeans_do                   (GwyContainer *data,
                                                    GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates K-means clustering on volume data"),
    "Daniil Bratashov <dn2010@gmail.com> & Evgeniy Ryabov",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov & Evgeniy Ryabov",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_volume_func_register("kmeans",
                              (GwyVolumeFunc)&volume_kmeans_do,
                              N_("/_K-means clustering"),
                              NULL,
                              VOLUME_KMEANS_RUN_MODES,
                              GWY_MENU_FLAG_VOLUME,
                              N_("Calculate K-means clustering on volume data"));

    return TRUE;
}

GwyBrick *
normalize_brick(GwyBrick *brick)
{
    GwyBrick *result;
    gdouble wmin, dataval, integral;
    gint i, j, l, xres, yres, zres;
    const gdouble *olddata;
    gdouble *newdata;

    result = gwy_brick_new_alike(brick, TRUE);
    wmin = gwy_brick_get_min(brick);
    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    olddata = gwy_brick_get_data_const(brick);
    newdata = gwy_brick_get_data(result);

    for (i = 0; i < xres; i++)
        for (j = 0; j < yres; j++) {
            integral = 0;
            for (l = 0; l < zres; l++) {
                dataval = *(olddata + l * xres * yres + j * xres + i);
                integral += (dataval - wmin);
            }
            for (l = 0; l < zres; l++) {
                dataval = *(olddata + l * xres * yres + j * xres + i);
                if (integral != 0.0) {
                    *(newdata + l * xres * yres + j * xres + i)
                                   = (dataval - wmin) * zres / integral;
                }
            }
        }

    return result;
}

static void
volume_kmeans_do(GwyContainer *container, GwyRunType run)
{
    GwyBrick *brick = NULL, *normalized = NULL;
    GwyDataField *dfield = NULL;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *calibration = NULL;
    // GwySIUnit *siunit;
    gint id;
    GRand *rand;
    const gdouble *data;
    gdouble *centers, *oldcenters, *sum, *data1, *xdata, *ydata;
    gdouble min, dist, xreal, yreal, zreal, xoffset, yoffset, zoffset;
    gdouble epsilon = 1e-12;
    gint xres, yres, zres, i, j, l, c, newid;
    gint *npix;
    gint k = 10;
    gint iterations = 0;
    gboolean converged = FALSE, normalize = TRUE;

    g_return_if_fail(run & VOLUME_KMEANS_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    xreal = gwy_brick_get_xreal(brick);
    yreal = gwy_brick_get_yreal(brick);
    zreal = gwy_brick_get_zreal(brick);
    xoffset = gwy_brick_get_xoffset(brick);
    yoffset = gwy_brick_get_yoffset(brick);
    zoffset = gwy_brick_get_zoffset(brick);

    if (normalize) {
        normalized = normalize_brick(brick);
        data = gwy_brick_get_data_const(normalized);
    }
    else {
        data = gwy_brick_get_data_const(brick);
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, TRUE);
    gwy_data_field_set_xoffset(dfield, xoffset);
    gwy_data_field_set_yoffset(dfield, yoffset);

    centers = g_malloc(zres*k*sizeof(gdouble));
    oldcenters = g_malloc (zres*k*sizeof(gdouble));
    sum = g_malloc(zres*k*sizeof (gdouble));
    npix = g_malloc(k*sizeof (gint));
    data1 = gwy_data_field_get_data(dfield);

    rand=g_rand_new();
    for (c = 0; c < k; c++) {
        i = g_rand_int_range(rand, 0, xres);
        j = g_rand_int_range(rand, 0, yres);
        for (l = 0; l < zres; l++) {
            *(centers + c * zres + l)
                             = *(data + l * xres * yres + j * xres + i);
        };
    };
    g_rand_free(rand);

    while (!converged) {
        /* pixels belong to cluster with min distance */
        for (i = 0; i < xres; i++)
            for (j = 0; j < yres; j++) {
                *(data1 + j * xres + i) = 0;
                min = G_MAXDOUBLE;
                for (c = 0; c < k; c++ ) {
                    dist = 0;
                    for (l = 0; l < zres; l++) {
                        *(oldcenters + c * zres + l)
                                            = *(centers + c * zres + l);
                        dist
                            += (*(data + l * xres * yres + j * xres + i)
                              - *(centers + c * zres + l))
                             * (*(data + l * xres * yres + j * xres + i)
                              - *(centers + c * zres + l));
                    }
                    if (dist < min) {
                        min = dist;
                        *(data1 + j * xres + i) = c;
                    }
                }
            }
        /* new center coordinates as average of pixels */

        for (c = 0; c < k; c++) {
            *(npix + c) = 0;
            for (l = 0; l < zres; l++) {
                *(sum + c * zres + l) = 0;
            }
        }
        for (i = 0; i < xres; i++)
            for (j = 0; j < yres; j++) {
                c = (gint)(*(data1 + j * xres + i));
                *(npix + c) += 1;
                for (l = 0; l < zres; l++) {
                    *(sum + c * zres + l)
                            += *(data + l * xres * yres + j * xres + i);
                }
            }

        for (c = 0; c < k; c++)
            for (l =0; l < zres; l++) {
                *(centers + c * zres + l) = (*(npix + c) > 0) ?
                     *(sum + c * zres + l) / (gdouble)(*(npix + c)) : 0;
        }

        converged = TRUE;
        for (c = 0; c < k; c++)
            for (l = 0; l < zres; l++)
                if (*(oldcenters + c * zres + l)
                                - *(centers + c * zres + l) > epsilon) {
                    converged = FALSE;
                    break;
                }
        if (iterations == 100)
            converged = TRUE;
        iterations++;
    }

    if (container) {
        newid = gwy_app_data_browser_add_data_field(dfield,
                                                    container, TRUE);
        g_object_unref(dfield);
        // gwy_app_set_data_field_title(data, newid, description);
        gwy_app_channel_log_add(container, -1, newid, "volume::kmeans",
                                NULL);

        gmodel = gwy_graph_model_new();
        calibration = gwy_brick_get_zcalibration(brick);
        if (calibration) {
            xdata = gwy_data_line_get_data(calibration);
        }
        else {
            xdata = g_malloc(zres * sizeof(gdouble));
            for (i = 0; i < zres; i++)
                *(xdata + i) = zreal * i / zres + zoffset;
        }
        for (c = 0; c < k; c++) {
            ydata = g_memdup(centers + c * zres,
                             zres * sizeof(gdouble));
            gcmodel = gwy_graph_curve_model_new();
            gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, zres);
            g_object_set(gcmodel,
                         "mode", GWY_GRAPH_CURVE_LINE,
                         "description",
                         g_strdup_printf(_("K-means center %d"), c),
                         NULL);
            gwy_graph_model_add_curve(gmodel, gcmodel);
            g_object_unref(gcmodel);
        }
        g_object_set(gmodel,
             //        "si-unit-x", gwy_data_line_get_si_unit_x(dline),
             //        "si-unit-y", gwy_data_line_get_si_unit_y(dline),
                     "axis-label-bottom", "x",
                     "axis-label-left", "y",
                     NULL);
        gwy_app_data_browser_add_graph_model(gmodel, container, TRUE);
        g_object_unref(gmodel);
    }

    if (normalized) {
        g_object_unref(normalized);
    }
    g_free(npix);
    g_free(sum);
    g_free(oldcenters);
    g_free(centers);

    gwy_app_volume_log_add_volume(container, id, id);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

