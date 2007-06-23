/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define ACF2D_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(void);
static void     acf2d          (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates two-dimensional autocorrelation function."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("acf2d",
                              (GwyProcessFunc)&acf2d,
                              N_("/_Statistics/2D Auto_correlation"),
                              NULL,
                              ACF2D_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Calculate 2D autocorrelation function"));

    return TRUE;
}

static void
acf2d(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *result;
    gint xres, yres, id;

    g_return_if_fail(run & ACF2D_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield, 0);
    g_return_if_fail(dfield);

    result = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_2dacf(dfield, result);
    id = gwy_app_data_browser_add_data_field(result, data, TRUE);
    g_object_unref(result);
    gwy_app_set_data_field_title(data, id, _("2D ACF"));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
