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
#include <libprocess/gwyprocess.h>
#include <app/gwyapp.h>

static gboolean module_register(void);
static void     filter         (GwyGraph *graph);
static void     filter_do      (const gdouble *yold,
                                gdouble *y,
                                gint res);
static void     filter_fft_do  (const gdouble *yold,
                                gdouble *y,
                                gint res);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Remove graph noise by filtering."),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.1",
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
    GwyGraphCurveModel *cmodel;
    const gdouble *xdata, *ydata;
    GArray *newydata;
    gint i, ncurves, ndata;

    ncurves = gwy_graph_model_get_n_curves(gwy_graph_get_model(graph));
    newydata = g_array_new(FALSE, FALSE, sizeof(gdouble));
    for (i = 0; i < ncurves; i++) {
        cmodel = gwy_graph_model_get_curve(gwy_graph_get_model(graph),
                                           i);
        xdata = gwy_graph_curve_model_get_xdata(cmodel);
        ydata = gwy_graph_curve_model_get_ydata(cmodel);
        ndata = gwy_graph_curve_model_get_ndata(cmodel);
        g_array_set_size(newydata, 0);
        g_array_append_vals(newydata, ydata, ndata);
        filter_fft_do(ydata, (gdouble*)newydata->data, ndata);
        gwy_graph_curve_model_set_data(cmodel, xdata,
                                      (gdouble*)newydata->data,
                                       ndata);
    }
    for (i = 0; i < ncurves; i++) {
        cmodel = gwy_graph_model_get_curve(gwy_graph_get_model(graph),
                                           i);
        g_signal_emit_by_name(cmodel, "data-changed");
    }
    g_array_free(newydata, TRUE);
}

static void
filter_do(const gdouble *yold, gdouble *y, gint res)
{
    gint i, j;
    gint num = 5;

    for (i = num; i < res-num; i++) {
        for (j = 1; j < num; j++)
            y[i] += yold[i+j]+yold[i-j];
        y[i] /= 2 * num - 1;
    }
}

static void
filter_fft_do(const gdouble *yold, gdouble *y, gint res)
{
    GwyDataLine *rsource, *isource, *rdest, *idest;
    gdouble *data, *idata, re, im;
    gint i;
    gint freq = 20, freq2 = 200;

    rsource = gwy_data_line_new(res, res, FALSE);
    isource = gwy_data_line_new_alike(rsource, TRUE);
    rdest = gwy_data_line_new_alike(rsource, TRUE);
    idest = gwy_data_line_new_alike(rsource, TRUE);
    data = gwy_data_line_get_data(rsource);
    for (i = 0; i < res; i++)
        *(data++) = yold[i];
    gwy_data_line_fft(rsource, isource, rdest, idest,
                      GWY_WINDOWING_FLAT_TOP,
                      GWY_TRANSFORM_DIRECTION_FORWARD,
                      GWY_INTERPOLATION_ROUND,
                      FALSE, 2);

    gwy_data_line_part_clear(rdest, 0, freq);
    gwy_data_line_part_clear(idest, 0, freq);
    gwy_data_line_part_clear(rdest, freq2, res);
    gwy_data_line_part_clear(idest, freq2, res);
    /*
    gwy_data_line_part_clear(rdest, res/2-freq2, res/2+freq2);
    gwy_data_line_part_clear(idest, res/2-freq2, res/2+freq2);    
    */

    gwy_data_line_fft_raw(rdest, idest, rsource, isource,
                                      GWY_TRANSFORM_DIRECTION_BACKWARD);
    data = gwy_data_line_get_data(rsource);
    idata = gwy_data_line_get_data(isource);
    for (i = 0; i < res; i++) {
        re = *(data++);
        im = *(idata++);
        *(y++) = sqrt(re * re + im * im);
    }

    g_object_unref(rsource);
    g_object_unref(isource);
    g_object_unref(rdest);
    g_object_unref(idest);
}
