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
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>
#include <app/gwyapp.h>

#define OUTLIERS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


static gboolean    module_register            (const gchar *name);
static gboolean    outliers                   (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "outliers",
    "Create mask of outliers",
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
    static GwyProcessFuncInfo outliers_func_info = {
        "outliers",
        "/_Statistics/_Mask of Outliers",
        (GwyProcessFunc)&outliers,
        OUTLIERS_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &outliers_func_info);

    return TRUE;
}

static gboolean
outliers(GwyContainer *data, GwyRunType run)
{
    GObject *maskfield;
    GwyDataField *dfield;
    gdouble thresh;

    g_assert(run & OUTLIERS_RUN_MODES);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    if (!gwy_container_gis_object_by_name(data, "/0/mask", &maskfield)) {
        maskfield = gwy_data_field_new(gwy_data_field_get_xres(dfield),
                                       gwy_data_field_get_yres(dfield),
                                       gwy_data_field_get_xreal(dfield),
                                       gwy_data_field_get_yreal(dfield),
                                       TRUE);
        gwy_container_set_object_by_name(data, "/0/mask", maskfield);
        g_object_unref(maskfield);
    }

    thresh = 3.0;
    gwy_data_field_mask_outliers(dfield, GWY_DATA_FIELD(maskfield), thresh);

    return TRUE;
}




