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

#include "config.h"
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define DIST_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


static gboolean    module_register            (const gchar *name);
static gboolean    dist                       (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluates distribution of grains (continuous parts of mask)."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo dist_func_info = {
        "grain_dist",
        N_("/_Grains/_Size Distribution"),
        (GwyProcessFunc)&dist,
        DIST_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };

    gwy_process_func_register(name, &dist_func_info);

    return TRUE;
}

static gboolean
dist(GwyContainer *data, GwyRunType run)
{
    GtkWidget *graph;
    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *dataline;
    GwyDataField *dfield;

    g_return_val_if_fail(run & DIST_RUN_MODES, FALSE);
    g_return_val_if_fail(gwy_container_contains_by_name(data, "/0/mask"),
                         FALSE);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    dataline = gwy_data_line_new(10, 10, TRUE);
    gwy_data_field_grains_get_distribution(dfield, dataline);

    gmodel = gwy_graph_model_new();
    gwy_graph_model_set_title(gmodel, _("Grain Size Histogram"));
    gwy_graph_model_set_x_siunit(gmodel, gwy_data_field_get_si_unit_xy(dfield));
    gwy_graph_model_set_y_siunit(gmodel, gwy_data_field_get_si_unit_z(dfield));

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_description(cmodel, "grain sizes");
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    gwy_graph_model_add_curve(gmodel, cmodel);

    graph = gwy_graph_new(gmodel);
    gwy_object_unref(cmodel);
    gwy_object_unref(gmodel);
    gwy_object_unref(dataline);
    gwy_app_graph_window_create(GWY_GRAPH(graph), data);

    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
