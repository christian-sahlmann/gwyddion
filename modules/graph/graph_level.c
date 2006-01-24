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

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

static gboolean module_register(const gchar *name);
static void     level          (GwyGraph *graph);
static void     level_do       (const gdouble *x,
                                gdouble *y,
                                gdouble n);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Level graph by line."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_graph_func_register("graph_level",
                            (GwyGraphFunc)&level,
                            N_("/_Level"),
                            NULL,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Level graph curves"));

    return TRUE;
}

static void
level(GwyGraph *graph)
{
    GwyGraphCurveModel *cmodel;
    const gdouble *xdata, *ydata;
    GArray *newydata;
    gint i, ncurves, ndata;

    ncurves = gwy_graph_model_get_n_curves(gwy_graph_get_model(graph));
    newydata = g_array_new(FALSE, FALSE, sizeof(gdouble));
    for (i = 0; i < ncurves; i++) {
        cmodel = gwy_graph_model_get_curve_by_index(gwy_graph_get_model(graph),
                                                    i);
        xdata = gwy_graph_curve_model_get_xdata(cmodel);
        ydata = gwy_graph_curve_model_get_ydata(cmodel);
        ndata = gwy_graph_curve_model_get_ndata(cmodel);
        g_array_set_size(newydata, 0);
        g_array_append_vals(newydata, ydata, ndata);
        level_do(xdata, (gdouble*)newydata->data, ndata);
        gwy_graph_curve_model_set_data(cmodel, xdata, (gdouble*)newydata->data,
                                       ndata);
    }
    g_array_free(newydata, TRUE);
}

static void
level_do(const gdouble *x, gdouble *y, gdouble n)
{
    gdouble result[2];
    gint i;

    gwy_math_fit_polynom(n, x, y, 1, result);

    for (i = 0; i < n; i++)
        y[i] -= result[0] + result[1]*x[i];
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
