/*
 *  @(#) $Id$
 *  Copyright (C) 2010, David Necas (Yeti), Petr Klapetek, Daniil Bratashov
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, dn2010@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or CUTNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-graph.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define DELTA (1e-12)

static gboolean    module_register           (void);
static void        dos_spectrum              (GwyGraph *graph);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("DOS Spectrum"),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_graph_func_register("dos_spectrum",
                            (GwyGraphFunc)&dos_spectrum,
                            N_("/_DOS Spectrum"),
                            GWY_STOCK_GRAPH_FUNCTION,
                            GWY_MENU_FLAG_GRAPH,
                            N_("Calculate DOS spectrum from I-V tunneling spectroscopy"));

    return TRUE;
}

static void dos_spectrum(GwyGraph *graph)
{
    GtkWidget *dialog;
    GwyContainer *data;
    GwyGraphModel *gmodel, *ngmodel;
    GwyGraphCurveModel *gcmodel, *ngcmodel;
    const gdouble *xdata, *ydata;
    gdouble *nxdata, *nydata;
    gdouble ydatamin, xdatamin;
    guint i, j, k, ncurves, ndata, nndata;
    gchar *graphtitle, *newtitle;
    GwySIUnit *siunitx, *siunity, *testunitx, *testunity;

    gwy_app_data_browser_get_current(GWY_APP_CONTAINER, &data, NULL);
    gmodel = gwy_graph_get_model(GWY_GRAPH(graph));
    g_object_get(gmodel,
                 "title", &graphtitle,
                 "si-unit-x", &siunitx,
                 "si-unit-y", &siunity,
                 NULL);

    /* Checking axis units to be voltage-current spectroscopy */
    testunitx = gwy_si_unit_new("V");
    testunity = gwy_si_unit_new("A");
    if(!((gwy_si_unit_equal(siunitx, testunitx)) &&
        (gwy_si_unit_equal(siunity, testunity)))) {

        dialog = gtk_message_dialog_new
            (gwy_app_find_window_for_channel(data, -1),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Graph should be I-V spectroscopy."), "dos_spectrum");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        g_object_unref(testunitx);
        g_object_unref(testunity);
        g_free(graphtitle);
        g_object_unref(siunitx);
        g_object_unref(siunity);

        return;
    }
    g_object_unref(testunitx);
    g_object_unref(testunity);
    g_object_unref(siunity);

    ngmodel = gwy_graph_model_new_alike(gmodel);

    siunity = gwy_si_unit_new("a.u.");
    newtitle = g_strdup_printf(_("DOS Spectrum for \"%s\""), graphtitle);

    g_object_set(ngmodel,
                 "title", newtitle,
                 "si-unit-y", siunity,
                 NULL);

    g_free(graphtitle);
    g_free(newtitle);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    ncurves = gwy_graph_model_get_n_curves(gmodel);

    for(k = 0; k < ncurves; k++) {
        gcmodel = gwy_graph_model_get_curve(gmodel, k);

        xdata = gwy_graph_curve_model_get_xdata(gcmodel);
        ydata = gwy_graph_curve_model_get_ydata(gcmodel);
        ndata = gwy_graph_curve_model_get_ndata(gcmodel);
        nndata = ndata-1;
        ydatamin = fabs(ydata[0]);
        xdatamin = xdata[0];
        for(i = 1; i < ndata; i++) {
            if (fabs(xdata[i]) < DELTA)
                nndata--;
            if (fabs(xdata[i]-xdata[i-1]) < DELTA)
                nndata--;
            /* Zero U search */
            if (fabs(ydata[i]) < ydatamin) {
                ydatamin = fabs(ydata[i]);
                xdatamin = xdata[i];
            }
            /* End of Zero U search */
        }

        if (nndata == 0) continue;

        ngcmodel = gwy_graph_curve_model_duplicate(gcmodel);
        nxdata = g_new(gdouble, nndata);
        nydata = g_new(gdouble, nndata);

        j = 0;
        for(i = 1; i < ndata; i++) {
            if (fabs(xdata[i]) < DELTA)
                continue;
            if (fabs(xdata[i]-xdata[i-1]) < DELTA)
                continue;
            nxdata[j] = xdata[i] - xdatamin;
            nydata[j] = ((ydata[i]-ydata[i-1])/(xdata[i]-xdata[i-1]))/
                        (ydata[i]/nxdata[j]);
            j++;
        }


        gwy_graph_curve_model_set_data(ngcmodel, nxdata, nydata, nndata);

        gwy_graph_model_add_curve(ngmodel, ngcmodel);
        g_object_unref(ngcmodel);
    }

    gwy_app_data_browser_add_graph_model(ngmodel, data, TRUE);

    g_object_unref(ngmodel);
}
