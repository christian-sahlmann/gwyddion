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

#include <libgwyddion/gwymacros.h>
#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/gwyapp.h>

#define GRADIENT_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register              (const gchar *name);
static gboolean    gradient_filter              (GwyContainer *data,
                                                 GwyRunType run,
                                                 const gchar *name);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates presentations with various gradients "
       "(Sobel, Prewitt)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo sobel_horizontal_func_info = {
        "sobel_horizontal",
        N_("/_Display/_Gradient/_Sobel (horizontal)"),
        (GwyProcessFunc)&gradient_filter,
        GRADIENT_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo sobel_vertical_func_info = {
        "sobel_vertical",
        N_("/_Display/_Gradient/_Sobel (vertical)"),
        (GwyProcessFunc)&gradient_filter,
        GRADIENT_RUN_MODES,
        0,
    };

    static GwyProcessFuncInfo prewitt_horizontal_func_info = {
        "prewitt_horizontal",
        N_("/_Display/_Gradient/_Prewitt (horizontal)"),
        (GwyProcessFunc)&gradient_filter,
        GRADIENT_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo prewitt_vertical_func_info = {
        "prewitt_vertical",
        N_("/_Display/_Gradient/_Prewitt (vertical)"),
        (GwyProcessFunc)&gradient_filter,
        GRADIENT_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &sobel_horizontal_func_info);
    gwy_process_func_register(name, &sobel_vertical_func_info);
    gwy_process_func_register(name, &prewitt_horizontal_func_info);
    gwy_process_func_register(name, &prewitt_vertical_func_info);

    return TRUE;
}

static gboolean
gradient_filter(GwyContainer *data,
                GwyRunType run,
                const gchar *name)
{
    GwyDataField *dfield, *gradfield;

    g_assert(run & GRADIENT_RUN_MODES);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/show", NULL);
    if (gwy_container_gis_object_by_name(data, "/0/show", &gradfield)) {
        gwy_data_field_resample(gradfield,
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                GWY_INTERPOLATION_NONE);
    }
    else {
        gradfield = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(data, "/0/show", gradfield);
        g_object_unref(gradfield);
    }

    gwy_data_field_copy(dfield, gradfield, FALSE);

    if (!strcmp(name, "sobel_horizontal"))
        gwy_data_field_filter_sobel(gradfield, GWY_ORIENTATION_HORIZONTAL);
    else if (!strcmp(name, "sobel_vertical"))
        gwy_data_field_filter_sobel(gradfield, GWY_ORIENTATION_VERTICAL);
    else if (!strcmp(name, "prewitt_horizontal"))
        gwy_data_field_filter_prewitt(gradfield, GWY_ORIENTATION_HORIZONTAL);
    else if (!strcmp(name, "prewitt_vertical"))
        gwy_data_field_filter_prewitt(gradfield, GWY_ORIENTATION_VERTICAL);
    else {
        g_critical("Function called under unregistered name `%s'", name);
    }
    gwy_data_field_data_changed(gradfield);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
