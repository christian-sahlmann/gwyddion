/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#define DEBUG 1

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <app/app.h>
#include <app/undo.h>

#define ENTROPY_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    GRID_N = 8,
    GRID_M = 4
};

typedef struct {
    gint res;
    gboolean *known;
    gdouble *data;
    gdouble bx0;
    gdouble by0;
    gdouble bxrange;
    gdouble byrange;
} ScanGrid;

static gboolean      module_register              (const gchar *name);
static gboolean      entropy                      (GwyContainer *data,
                                                   GwyRunType run);
static void          entropy_do                   (GwyDataField *dfield);
static void          fill_scan_grid               (GwyDataField *dfield,
                                                   GwyDataField *tmp,
                                                   guint nreduced,
                                                   gdouble *buffer,
                                                   ScanGrid *grid);
static gdouble       compute_entropy              (GwyDataField *dfield,
                                                   gdouble epsilon,
                                                   guint n,
                                                   gdouble *buffer);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates two-dimensional distribution of entropys "
       "or graph of their angular distribution."),
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
    static GwyProcessFuncInfo entropy_func_info = {
        "entropy_level",
        N_("/_Level/_Entropy Level"),
        (GwyProcessFunc)&entropy,
        ENTROPY_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &entropy_func_info);

    return TRUE;
}

static gboolean
entropy(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & ENTROPY_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    entropy_do(dfield);
    gwy_data_field_data_changed(dfield);

    return TRUE;
}

static void
entropy_do(GwyDataField *dfield)
{
    GwyDataField *avg, *tmp;
    ScanGrid grid;
    gdouble *buffer, *adata;
    guint nreduced;
    gint xres, yres, i, j, mi, ifrom, ito, jfrom, jto;
    gdouble bx, by, real, theta, phi, xoff, yoff, m;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    nreduced = pow(xres*yres, 0.6182);

    gwy_data_field_get_inclination(dfield, &theta, &phi);
    real = tan(theta);
    xoff = real*cos(phi)/2.0;
    yoff = -real*sin(phi)/2.0;

    tmp = gwy_data_field_new_alike(dfield, FALSE);
    buffer = g_new(gdouble, nreduced);

    grid.bx0 = xoff - real;
    grid.bxrange = 6*real;
    grid.by0 = yoff - real;
    grid.byrange = 6*real;
    grid.res = GRID_N;
    grid.data = g_new(gdouble, grid.res*grid.res);
    grid.known = g_new0(gboolean, grid.res*grid.res);

    avg = gwy_data_field_new(grid.res, grid.res, 1, 1, FALSE);
    adata = gwy_data_field_get_data(avg);

    while (grid.bxrange + grid.byrange > 1e-5) {
        gwy_debug("Scan area: [%g,%g] x [%g,%g]",
                  grid.bx0, grid.bx0 + grid.bxrange,
                  grid.by0, grid.by0 + grid.byrange);
        /* Compute (missing) entropies */
        fill_scan_grid(dfield, tmp, nreduced, buffer, &grid);

        /* Average over squares */
        memcpy(adata, grid.data, grid.res*grid.res*sizeof(gdouble));
        /*gwy_data_field_filter_mean(avg, 3);*/

        /* Find maximum */
        m = adata[0];
        mi = 0;
        for (i = 1; i < grid.res*grid.res; i++) {
            if (adata[i] > m) {
                mi = i;
                m = adata[i];
            }
        }
        gwy_debug("Maximum found at (%d,%d) %g",
                  mi % grid.res, mi/grid.res, m);

        /* Compute suqare to zoom into.
         * Vertical: */
        ifrom = MAX(0, mi/grid.res - GRID_M/2);
        ito = MIN(grid.res - 1, mi/grid.res + GRID_M/2);
        if (ito - ifrom < GRID_M-1) {
            if (ifrom == 0)
                ito = GRID_M-1;
            else {
                g_assert(ito == grid.res - 1);
                ifrom = grid.res - GRID_M;
            }
        }
        grid.by0 += ifrom/(grid.res - 1.0)*grid.byrange;
        grid.byrange *= (GRID_M - 1.0)/(grid.res - 1.0);

        /* Horizontal: */
        jfrom = MAX(0, mi % grid.res - GRID_M/2);
        jto = MIN(grid.res - 1, mi % grid.res + GRID_M/2);
        if (jto - jfrom < GRID_M-1) {
            if (jfrom == 0)
                jto = GRID_M-1;
            else {
                g_assert(jto == grid.res - 1);
                jfrom = grid.res - GRID_M;
            }
        }
        grid.bx0 += jfrom/(grid.res - 1.0)*grid.bxrange;
        grid.bxrange *= (GRID_M - 1.0)/(grid.res - 1.0);

        gwy_debug("New squre (%d,%d) x (%d,%d)", jfrom, jto, ifrom, ito);

        /* Zoom actual data */
        memset(grid.known, 0, grid.res*grid.res*sizeof(gboolean));
        memcpy(grid.data, adata, grid.res*grid.res*sizeof(gdouble));
        for (i = 0; i < GRID_M; i++) {
            for (j = 0; j < GRID_M; j++) {
                grid.data[i*(grid.res - 1)/(GRID_M - 1)]
                    = adata[(i + ifrom)*grid.res + (j + jfrom)];
                grid.known[i*(grid.res - 1)/(GRID_M - 1)] = TRUE;
            }
        }
    }

    /* Final level */
    fill_scan_grid(dfield, tmp, nreduced, buffer, &grid);
    m = grid.data[0];
    mi = 0;
    for (i = 1; i < grid.res*grid.res; i++) {
        if (grid.data[i] > m) {
            mi = i;
            m = grid.data[i];
        }
    }
    bx = (i % grid.res)/(grid.res - 1.0)*grid.bxrange + grid.bx0;
    by = (i/grid.res)/(grid.res - 1.0)*grid.byrange + grid.by0;

    gwy_data_field_plane_level(dfield, 0, -bx, -by);

    g_object_unref(avg);
    g_free(grid.known);
    g_free(grid.data);
    g_free(buffer);
    g_object_unref(tmp);
}

static void
fill_scan_grid(GwyDataField *dfield,
               GwyDataField *tmp,
               guint nreduced,
               gdouble *buffer,
               ScanGrid *grid)
{
    gint i, j;
    gdouble bx, by;

    for (i = 0; i < grid->res; i++) {
        by = i/(grid->res - 1.0)*grid->byrange + grid->by0;
        for (j = 0; j < grid->res; j++) {
            if (grid->known[i*grid->res + j])
                continue;

            bx = j/(grid->res - 1.0)*grid->bxrange + grid->bx0;
            gwy_data_field_copy(dfield, tmp, FALSE);
            gwy_data_field_plane_level(tmp, 0, -bx, -by);
            grid->data[i*grid->res + j]
                = compute_entropy(tmp, 0.2, nreduced, buffer);
            grid->known[i*grid->res + j] = TRUE;
        }
    }

    for (i = 0; i < grid->res; i++) {
        for (j = 0; j < grid->res; j++) {
            g_print("%.4f\t", grid->data[i*grid->res + j] - 28);
        }
        g_print("\n");
    }
}

static gdouble
compute_entropy(GwyDataField *dfield,
                gdouble epsilon,
                guint n,
                gdouble *buffer)
{
    const gdouble *data;
    gdouble x, ws, weps;
    gint xres, yres;
    guint i, m;

    data = gwy_data_field_get_data_const(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    m = xres*yres;
    g_return_val_if_fail(m >= n, 0.0);

    for (i = 0; i < n; i++)
        buffer[i] = data[i*(m-1)/(n-1)];

    gwy_math_sort(n, buffer);

    epsilon *= (buffer[n-1] - buffer[0])/n;
    if (!epsilon) {
        return HUGE_VAL;
    }

    weps = log(epsilon)/epsilon;
    ws = 0.0;
    for (i = 1; i < n; i++) {
        x = buffer[i] - buffer[i-1];
        ws += (x > epsilon) ? log(x)/x : weps;
    }

    return log(-ws);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
