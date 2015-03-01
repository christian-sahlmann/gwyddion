/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

static gboolean module_register (void);
static void     graph_align     (GwyGraph *graph);
static void     align_two_curves(GwyGraphCurveModel *base,
                                 GwyGraphCurveModel *cmodel);
static gdouble  find_best_offset(const gdouble *a,
                                 gint na,
                                 const gdouble *b,
                                 gint nb);
static gdouble* regularise      (const gdouble *xdata,
                                 const gdouble *ydata,
                                 gint ndata,
                                 gdouble dx,
                                 gint *pn);
static gdouble  difference_score(const gdouble *a,
                                 gint na,
                                 const gdouble *b,
                                 gint nb,
                                 gint boff);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Aligns graph curves."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("graph_align",
                            (GwyGraphFunc)&graph_align,
                            N_("/_Align"),
                            NULL,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Align curves"));

    return TRUE;
}

static void
graph_align(GwyGraph *graph)
{
    GwyContainer *data;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel, *basecmodel = NULL;
    gint i, ncurves, ndata, ndatamax = 0;
    const gdouble *xdata;
    gdouble len, maxlen = 0.0;
    GQuark quark;

    gmodel = gwy_graph_get_model(graph);
    ncurves = gwy_graph_model_get_n_curves(gmodel);
    if (ncurves < 2) {
        gwy_debug("too few curves");
        return;
    }

    for (i = 0; i < ncurves; i++) {
        cmodel = gwy_graph_model_get_curve(gmodel, i);
        ndata = gwy_graph_curve_model_get_ndata(cmodel);
        xdata = gwy_graph_curve_model_get_xdata(cmodel);
        len = xdata[ndata-1] - xdata[0];
        if (len > maxlen) {
            gwy_debug("curve %d selected as the base", i);
            basecmodel = cmodel;
            ndatamax = ndata;
            maxlen = len;
        }
    }
    g_assert(basecmodel);

    if (ndatamax < 6) {
        gwy_debug("base curve has only %d points", ndatamax);
        return;
    }

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data,
                                     GWY_APP_GRAPH_MODEL_KEY, &quark,
                                     0);
    gwy_app_undo_qcheckpointv(data, 1, &quark);

    for (i = 0; i < ncurves; i++) {
        cmodel = gwy_graph_model_get_curve(gmodel, i);
        if (cmodel == basecmodel)
            continue;

        gwy_debug("aligning curve %d to the base", i);
        align_two_curves(basecmodel, cmodel);
        g_signal_emit_by_name(cmodel, "data-changed");
    }
}

static void
align_two_curves(GwyGraphCurveModel *base,
                 GwyGraphCurveModel *cmodel)
{
    const gdouble *cxdata, *cydata, *bxdata, *bydata;
    gdouble *cline, *bline, *newcxdata, *newcydata;
    gint cndata, bndata, cn, bn, i;
    gdouble clen, dx, blen, off;

    bndata = gwy_graph_curve_model_get_ndata(base);
    bxdata = gwy_graph_curve_model_get_xdata(base);
    bydata = gwy_graph_curve_model_get_ydata(base);

    cndata = gwy_graph_curve_model_get_ndata(cmodel);
    cxdata = gwy_graph_curve_model_get_xdata(cmodel);
    cydata = gwy_graph_curve_model_get_ydata(cmodel);

    if (cndata < 6)
        return;

    blen = bxdata[bndata-1] - bxdata[0];
    clen = cxdata[cndata-1] - cxdata[0];
    /* Check if we are able to resample both curves to a common regular grid
     * without going insane. */
    dx = clen/120.0;
    if ((bxdata[bndata-1] - bxdata[0])/dx > 1e5) {
        dx = 1e5/blen;
        if (clen/dx < cndata)
            return;
    }

    bline = regularise(bxdata, bydata, bndata, dx, &bn);
    cline = regularise(cxdata, cydata, cndata, dx, &cn);
    off = find_best_offset(cline, cn, bline, bn);

    g_free(bline);
    g_free(cline);

    off = dx*off + (cxdata[0] - bxdata[0]);
    newcxdata = g_new(gdouble, cndata);
    newcydata = (gdouble*)g_memdup(cydata, cndata*sizeof(gdouble));
    for (i = 0; i < cndata; i++)
        newcxdata[i] = cxdata[i] - off;

    gwy_graph_curve_model_set_data(cmodel, newcxdata, newcydata, cndata);
    g_free(newcydata);
    g_free(newcxdata);
}

static gdouble
find_best_offset(const gdouble *a, gint na,
                 const gdouble *b, gint nb)
{
    gdouble scores[3] = { 0.0, 0.0, 0.0 };
    gdouble prev, score = G_MAXDOUBLE, bestscore = G_MAXDOUBLE;
    gint off, off_from, off_to, bestoff = 0;
    gdouble off0, subpixoff = 0.0;

    g_assert(nb > 4);

    off_from = -((2*na + 1)/5);
    off_to = na - (3*na + 1)/5;
    off0 = 0.5*(off_from + off_to);

    gwy_debug("off range [%d, %d]", off_from, off_to);
    for (off = off_from; off <= off_to; off++) {
        gdouble t = 4.0*(off - off0)/(off_to - off_from);
        prev = score;
        score = difference_score(a, na, b, nb, off);
        score *= 1.0 + t*t;
        if (score < bestscore) {
            scores[0] = prev;
            scores[1] = score;
            bestscore = score;
            bestoff = off;
        }
        if (off == bestoff+1) {
            scores[2] = score;
        }
        /*g_printerr("%d %g\n", off, score);*/
    }

    gwy_debug("best offset %d [%g %g %g]",
              bestoff,
              scores[0]/bestscore, scores[1]/bestscore, scores[2]/bestscore);

    if (scores[0] > scores[1] && scores[2] > scores[1]) {
        subpixoff = 0.5*(scores[0] - scores[2])/(scores[0] + scores[2]
                                                 - 2.0*scores[1]);
        gwy_debug("subpix %g", subpixoff);
    }

    return bestoff + subpixoff;
}

static gdouble
difference_score(const gdouble *a, gint na,
                 const gdouble *b, gint nb,
                 gint boff)
{
    gint afrom, bfrom, len, i;
    gdouble s = 0.0;

    if (boff <= 0) {
        afrom = 0;
        bfrom = -boff;
        len = MIN(na, nb + boff);
    }
    else {
        afrom = boff;
        bfrom = 0;
        len = MIN(nb, na - afrom);
    }

    g_assert(len > 0);

    a += afrom;
    b += bfrom;
    for (i = 0; i < len; i++) {
        gdouble d = a[i] - b[i];
        s += fabs(d);
    }

    return s/len;
}

static gdouble*
regularise(const gdouble *xdata, const gdouble *ydata, gint ndata,
           gdouble dx, gint *pn)
{
    gint n = floor((xdata[ndata-1] - xdata[0])/dx) + 1;
    gint i, j, k, ic;
    gdouble *data, *weight;

    data = g_new0(gdouble, n);
    weight = g_new0(gdouble, n);
    *pn = n;

    for (ic = 0; ic < ndata; ic++) {
        gdouble x = xdata[ic];
        i = (gint)floor((x - xdata[0])/dx);
        i = CLAMP(i, 0, n-1);
        data[i] += ydata[ic];
        weight[i] += 1.0;
    }

    for (i = 0; i < n; i++) {
        if (weight[i])
            data[i] /= weight[i];
    }
    if (!weight[0]) {
        data[0] = ydata[0];
        weight[0] = 1.0;
    }
    if (!weight[n-1]) {
        data[n-1] = ydata[ndata-1];
        weight[n-1] = 1.0;
    }

    i = 1;
    while (i < n-1) {
        gdouble yf, yt;

        if (weight[i]) {
            i++;
            continue;
        }

        for (j = i+1; !weight[j]; j++)
            ;

        i--;
        yf = ydata[i];
        yt = ydata[j];
        for (k = i; k < j; k++) {
            data[k] = (yf*(j - k) + yt*(k - i))/(j - i);
        }

        i = j+1;
    }

    g_free(weight);

    return data;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
