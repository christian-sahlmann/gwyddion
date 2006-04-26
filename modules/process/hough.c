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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/hough.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define HOUGH_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(void);
static void     hough          (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask of hough."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("hough",
                              (GwyProcessFunc)&hough,
                              N_("/_Integral Transforms/_Hough"),
                              NULL,
                              HOUGH_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Detect lines by Hough transform"));


    return TRUE;
}

static void
hough(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *edgefield, *result, *f1, *f2;
    GQuark squark;
    GwySIUnit *siunit;

    g_return_if_fail(run & HOUGH_RUN_MODES);


    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &result,
                                     0);
    g_return_if_fail(dfield);


    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (!result){
        result = gwy_data_field_new_alike(dfield, FALSE);
        siunit = gwy_si_unit_new("");
        gwy_data_field_set_si_unit_z(result, siunit);
        g_object_unref(siunit);
        gwy_container_set_object(data, squark, result);
        g_object_unref(result);
    }

    edgefield = gwy_data_field_duplicate(dfield);
    f1 = gwy_data_field_duplicate(dfield);
    f2 = gwy_data_field_duplicate(dfield);


    gwy_data_field_filter_canny(edgefield, 0.1);
    gwy_data_field_filter_sobel(f1, GWY_ORIENTATION_HORIZONTAL);
    gwy_data_field_filter_sobel(f2, GWY_ORIENTATION_VERTICAL);
    gwy_data_field_hough_line(edgefield,
			      NULL,
			      NULL,
			      result,
			      1);

    /*gwy_data_field_hough_circle(edgefield,
                                f1,
                                f2,
                                result,
                                10);
    */
    g_object_unref(edgefield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
