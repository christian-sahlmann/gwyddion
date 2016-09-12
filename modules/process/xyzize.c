/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/surface.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define XYZIZE_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(void);
static void     xyzize         (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Converts data fields to XYZ data."),
    "Yeti <yeti@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("xyzize",
                              (GwyProcessFunc)&xyzize,
                              N_("/_Basic Operations/_XYZize..."),
                              NULL,
                              XYZIZE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Convert to XYZ data"));

    return TRUE;
}

static void
xyzize(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield = NULL;
    GwySurface *surface;
    gint newid;

    g_return_if_fail(run & XYZIZE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield, 0);
    surface = gwy_surface_new();
    gwy_surface_set_from_data_field(surface, dfield);
    newid = gwy_app_data_browser_add_surface(surface, data, TRUE);
    g_object_unref(surface);
    gwy_app_xyz_log_add(data, -1, newid, "proc::xyzize", NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
