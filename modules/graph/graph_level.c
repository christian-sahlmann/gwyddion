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
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

/* Data for this function.*/

static gboolean    module_register           (const gchar *name);
static gboolean    level                     (GwyGraph *graph);
static void        level_do                  (gdouble *x, gdouble *y, gdouble n);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Level graph by line"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyGraphFuncInfo fit_func_info = {
        "graph_level",
        N_("/_Level"),
        (GwyGraphFunc)&level,
    };

    gwy_graph_func_register(name, &fit_func_info);

    return TRUE;
}

static gboolean
level(GwyGraph *graph)
{
    GwyGraphCurveModel *cmodel;
    gboolean ok;
    gdouble *xdata, *ydata;
    gint i, ndata;

    for (i=0; i<gwy_graph_model_get_n_curves(gwy_graph_get_model(graph)); i++)
    {
        cmodel = gwy_graph_model_get_curve_by_index(gwy_graph_get_model(graph), i);
        xdata = gwy_graph_curve_model_get_xdata(cmodel);
        ydata = gwy_graph_curve_model_get_ydata(cmodel);
        ndata = gwy_graph_curve_model_get_ndata(cmodel);

        level_do(xdata, ydata, ndata);
    }

    return ok;
}

static void        
level_do(gdouble *x, gdouble *y, gdouble n)
{

}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
