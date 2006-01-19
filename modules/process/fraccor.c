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
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/fractals.h>
#include <app/gwyapp.h>

#define FRACCOR_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(const gchar *name);
static void     fraccor        (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Removes data under mask using fractal interpolation."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo fraccor_func_info = {
        "fraccor",
        N_("/_Correct Data/_Fractal correction"),
        (GwyProcessFunc)&fraccor,
        FRACCOR_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };

    gwy_process_func_register(name, &fraccor_func_info);

    return TRUE;
}

static void
fraccor(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *maskfield;

    g_return_if_fail(run & FRACCOR_RUN_MODES);

    if (gwy_container_gis_object_by_name(data, "/0/mask", &maskfield)) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        gwy_app_undo_checkpoint(data, "/0/data", "/0/mask", NULL);
        gwy_data_field_fractal_correction(dfield, maskfield,
                                          GWY_INTERPOLATION_BILINEAR);
        gwy_container_remove_by_name(data, "/0/mask");
        gwy_data_field_data_changed(dfield);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
