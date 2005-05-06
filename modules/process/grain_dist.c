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
    "1.1",
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
    GString *lab;
    GtkWidget *graph;
    GwyDataWindow *data_window;
    GwyGraphAutoProperties prop;
    GwyDataLine *dataline;
    GwyDataField *dfield;
    GwySIValueFormat *units;

    g_return_val_if_fail(run & DIST_RUN_MODES, FALSE);
    g_return_val_if_fail(gwy_container_contains_by_name(data, "/0/mask"),
                         FALSE);

    graph = gwy_graph_new();
    gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    dataline = gwy_data_line_new(10, 10, TRUE);
    /*for (i=0; i<10; i++) dataline->data[i] = i*i;*/
    gwy_data_field_grains_get_distribution(dfield, dataline);

/*    lab = g_string_new(_("Grain size histogram"));
    units = gwy_si_unit_get_format(dfield->si_unit_xy, dataline->real, NULL);
    gwy_graph_add_dataline_with_units(GWY_GRAPH(graph), dataline, 0, lab, NULL,
                                        units->magnitude, 1, units->units, " ");

    data_window = gwy_app_data_window_get_for_data(data);
    gwy_app_graph_window_create_for_window(GWY_GRAPH(graph), data_window,
                                           _("Grain size distribution"));
*/
    g_string_free(lab, TRUE);
    g_object_unref(dataline);
    g_free(units);

    return FALSE;
}



/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
