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
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov & Evgeniy Ryabov",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_volume_func_register("volume_kmeans",
                              (GwyVolumeFunc)&volume_kmeans_do,
                              N_("/_K-means clustering"),
                              NULL,
                              VOLUME_KMEANS_RUN_MODES,
                              GWY_MENU_FLAG_VOLUME,
                              N_("Calculate K-means clustering on volume data"));

    return TRUE;
}

static void
volume_kmeans_do(GwyContainer *container, GwyRunType run)
{
    GwyBrick *brick = NULL;
    GwyDataField *dfield = NULL;
    gint id;
    GRand *rand;
    const gdouble *data;
    gdouble *centers, *oldcenters, *sum, *data1;
    gdouble min, dist;
    gdouble epsilon = 1e-12;
    gint xres, yres, zres;
    gint *npix;
    gint k = 10;
    gint i, j, l, c;
    gint iterations = 0;
    gboolean converged = FALSE;
    gchar key[50];

    g_return_if_fail(run & VOLUME_KMEANS_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));

    g_snprintf(key, sizeof(key), "/brick/%d/preview", id);
    dfield = (GwyDataField *)gwy_container_get_object(container, g_quark_from_string(key));

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    data = gwy_brick_get_data_const(brick);

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
            *(centers + c * zres + l) = *(data + l * xres * yres + j * xres + i);
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
                        dist += (*(data + l * xres * yres + j * xres + i)
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

    gwy_data_field_data_changed (dfield);
    g_free(npix);
    g_free(sum);
    g_free(oldcenters);
    g_free(centers);

    gwy_app_volume_log_add_volume(container, id, id);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

