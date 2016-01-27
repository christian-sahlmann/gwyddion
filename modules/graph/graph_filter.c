/*
 *  @(#) $Id$
 *  Copyright (C) 2012 David Necas (Yeti), Petr Klapetek,
 *  Daniil Bratashov.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, dn2010@gmail.com.
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
#include <libgwydgets/gwygraphmodel.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwyapp.h>

static gboolean module_register(void);
static void     filter         (GwyGraph *graph);
static void     filter_do      (const gdouble *yold,
                                gdouble *y,
                                gdouble n);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Remove graph noise by filtering."),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.3",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov (dn2010)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_filter",
                            (GwyGraphFunc)&filter,
                            N_("/_Filter"),
                            NULL,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Remove noise from graph curves"));

    return TRUE;
}

static void
filter(GwyGraph *graph)
{
    GwyContainer *data;
    GwyGraphCurveModel *cmodel, *cmodelnew;
    GwyGraphModel *model;
    const gdouble *xdata, *ydata;
    gdouble *newydata;
    gint i, ncurves, ndata;
    GQuark quark;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data,
                                     GWY_APP_GRAPH_MODEL_KEY, &quark,
                                     0);
    gwy_app_undo_qcheckpointv(data, 1, &quark);

    model = gwy_graph_get_model(graph);
    ncurves = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < ncurves; i++) {
        cmodel = gwy_graph_model_get_curve(model, i);
        cmodelnew = gwy_graph_curve_model_new_alike(cmodel);
        xdata = gwy_graph_curve_model_get_xdata(cmodel);
        ydata = gwy_graph_curve_model_get_ydata(cmodel);
        ndata = gwy_graph_curve_model_get_ndata(cmodel);
        newydata = g_new(gdouble, ndata);
        filter_do(ydata, newydata, ndata);
        gwy_graph_curve_model_set_data(cmodelnew, xdata,
                                       newydata, ndata);
        g_free(newydata);
        gwy_graph_model_remove_curve(gwy_graph_get_model(graph), i);
        gwy_graph_model_add_curve(model, cmodelnew);
        g_object_unref(cmodelnew);
    }
}

static void
filter_do(const gdouble *yold, gdouble *y, gdouble n)
{
    gint i, j, min, max, nelem;
    gint num = 5;

    for (i = 0; i < n; i++) {
        nelem = 0;
        y[i] = 0;
        min = i - num < 0 ? 0 : i - num;
        max = i + num + 1 > n ? n : i + num + 1;
        for (j = min; j < max; j++) {
            nelem++;
            y[i] += yold[j];
        }
        y[i] /= nelem;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
