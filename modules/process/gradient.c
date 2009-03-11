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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define GRADIENT_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(void);
static void     gradient       (GwyContainer *data,
                                GwyRunType run,
                                const gchar *name);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates presentations with various gradients "
       "(Sobel, Prewitt)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("sobel_horizontal",
                              (GwyProcessFunc)&gradient,
                              N_("/_Presentation/_Gradient/_Sobel (horizontal)"),
                              NULL,
                              GRADIENT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Horizontal Sobel gradient presentation"));
    gwy_process_func_register("sobel_vertical",
                              (GwyProcessFunc)&gradient,
                              N_("/_Presentation/_Gradient/_Sobel (vertical)"),
                              NULL,
                              GRADIENT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Vertical Sobel gradient presentation"));
    gwy_process_func_register("prewitt_horizontal",
                              (GwyProcessFunc)&gradient,
                              N_("/_Presentation/_Gradient/_Prewitt (horizontal)"),
                              NULL,
                              GRADIENT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Horizontal Prewitt gradient presentation"));
    gwy_process_func_register("prewitt_vertical",
                              (GwyProcessFunc)&gradient,
                              N_("/_Presentation/_Gradient/_Prewitt (vertical)"),
                              NULL,
                              GRADIENT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Vertical Prewitt gradient presentation"));
    gwy_process_func_register("azimuth",
                              (GwyProcessFunc)&gradient,
                              N_("/_Presentation/_Gradient/_Azimuth"),
                              NULL,
                              GRADIENT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Local slope azimuth presentation"));

    return TRUE;
}

static void
filter_azimuth(GwyDataField *dfield, GwyDataField *target)
{
    const gdouble *sdata;
    gdouble *data;
    gint xres, yres, i, j;

    sdata = gwy_data_field_get_data_const(dfield);
    data = gwy_data_field_get_data(target);
    xres = gwy_data_field_get_xres(target);
    yres = gwy_data_field_get_yres(target);

    for (j = 0; j < xres; j++)
        data[j] = 0.0;

    for (i = 1; i < yres-1; i++) {
        data[i*xres] = 0.0;
        for (j = 1; j < xres-1; j++) {
            gdouble xd = sdata[i*xres + j+1] - sdata[i*xres + j-1];
            gdouble yd = sdata[(i+1)*xres + j] - sdata[(i-1)*xres + j];
            data[i*xres + j] = atan2(yd, xd);
        }
        data[i*xres + xres-1] = 0.0;
    }

    for (j = 0; j < xres; j++)
        data[(yres - 1)*xres + j] = 0.0;
}

static void
gradient(GwyContainer *data,
         GwyRunType run,
         const gchar *name)
{
    GwyDataField *dfield, *showfield;
    GQuark dquark, squark;
    GwySIUnit *siunit;
    gint id;

    g_return_if_fail(run & GRADIENT_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     GWY_APP_SHOW_FIELD, &showfield,
                                     0);
    g_return_if_fail(dfield && dquark && squark);

    gwy_app_undo_qcheckpointv(data, 1, &squark);
    if (!showfield) {
        showfield = gwy_data_field_new_alike(dfield, FALSE);
        siunit = gwy_si_unit_new("");
        gwy_data_field_set_si_unit_z(showfield, siunit);
        g_object_unref(siunit);
        gwy_container_set_object(data, squark, showfield);
        g_object_unref(showfield);
    }
    gwy_data_field_copy(dfield, showfield, FALSE);

    if (gwy_strequal(name, "sobel_horizontal"))
        gwy_data_field_filter_sobel(showfield, GWY_ORIENTATION_HORIZONTAL);
    else if (gwy_strequal(name, "sobel_vertical"))
        gwy_data_field_filter_sobel(showfield, GWY_ORIENTATION_VERTICAL);
    else if (gwy_strequal(name, "prewitt_horizontal"))
        gwy_data_field_filter_prewitt(showfield, GWY_ORIENTATION_HORIZONTAL);
    else if (gwy_strequal(name, "prewitt_vertical"))
        gwy_data_field_filter_prewitt(showfield, GWY_ORIENTATION_VERTICAL);
    else if (gwy_strequal(name, "azimuth"))
        filter_azimuth(dfield, showfield);
    else {
        g_warning("gradient does not provide function `%s'", name);
    }

    gwy_data_field_normalize(showfield);
    gwy_data_field_data_changed(showfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
