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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/gwyapp.h>

#define EDGE_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


static gboolean    module_register              (const gchar *name);
static gboolean    laplacian                    (GwyContainer *data,
                                                 GwyRunType run);
static gboolean    canny                        (GwyContainer *data,
                                                 GwyRunType run);
static gboolean    rms                          (GwyContainer *data,
                                                 GwyRunType run);



/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "edge",
    N_("Edge detection presentations"),
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
    static GwyProcessFuncInfo laplacian_func_info = {
        "laplacian",
        N_("/_Display/_Edge detection/_Laplacian of Gaussian"),
        (GwyProcessFunc)&laplacian,
        EDGE_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo canny_func_info = {
        "canny",
        N_("/_Display/_Edge detection/_Canny"),
        (GwyProcessFunc)&canny,
        EDGE_RUN_MODES,
        0,
    };
    static GwyProcessFuncInfo rms_func_info = {
        "rms",
        N_("/_Display/_Edge detection/_RMS"),
        (GwyProcessFunc)&rms,
        EDGE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &laplacian_func_info);
    gwy_process_func_register(name, &canny_func_info);
    gwy_process_func_register(name, &rms_func_info);

    return TRUE;
}

static gboolean
laplacian(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *shadefield;
    gdouble avg;
    gint i, j;

    g_assert(run & EDGE_RUN_MODES);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/show", NULL);
    if (gwy_container_gis_object_by_name(data, "/0/show",
                                         (GObject**)&shadefield)) {
        gwy_data_field_resample(shadefield,
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                GWY_INTERPOLATION_NONE);
    }
    else {
        shadefield
            = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(data, "/0/show", G_OBJECT(shadefield));
        g_object_unref(shadefield);
    }

    gwy_data_field_area_copy(dfield, shadefield,
                             0, 0, gwy_data_field_get_xres(dfield),
                             gwy_data_field_get_yres(dfield), 0, 0);

    gwy_data_field_area_filter_laplacian(shadefield,
                                         0, 0,
                                         gwy_data_field_get_xres(dfield),
                                         gwy_data_field_get_yres(dfield));

    avg = gwy_data_field_get_area_avg(shadefield, 1, 1,
                                      gwy_data_field_get_xres(dfield)-1,
                                      gwy_data_field_get_yres(dfield)-1);

    for (i = 0; i < dfield->yres; i++) {
        shadefield->data[dfield->xres*i] = avg;
        shadefield->data[dfield->xres - 1 + dfield->xres*i] = avg;
    }
    for (j = 0; j < dfield->xres; j++) {
        shadefield->data[j] = avg;
        shadefield->data[j + dfield->xres*(dfield->yres-1)] = avg;
    }

    return TRUE;
}

static gboolean
canny(GwyContainer *data, GwyRunType run)
{
    GObject *shadefield;
    GwyDataField *dfield;

    g_assert(run & EDGE_RUN_MODES);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/show", NULL);
    if (gwy_container_gis_object_by_name(data, "/0/show", &shadefield)) {
        gwy_data_field_resample(GWY_DATA_FIELD(shadefield),
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                GWY_INTERPOLATION_NONE);
    }
    else {
        shadefield = gwy_serializable_duplicate(G_OBJECT(dfield));
        gwy_container_set_object_by_name(data, "/0/show", shadefield);
        g_object_unref(shadefield);
    }

    gwy_data_field_area_copy(dfield, GWY_DATA_FIELD(shadefield),
                             0, 0, gwy_data_field_get_xres(dfield),
                             gwy_data_field_get_yres(dfield), 0, 0);

    /*now we use fixed threshold, but in future, there could be API
     with some setting. We could also do smooting before apllying filter.*/
    gwy_data_field_area_filter_canny(GWY_DATA_FIELD(shadefield),
                                     0.1,
                                     0, 0,
                                     gwy_data_field_get_xres(dfield),
                                     gwy_data_field_get_yres(dfield));
    return TRUE;
}

static gboolean
rms(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *shadefield;

    g_assert(run & EDGE_RUN_MODES);

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/show", NULL);
    if (gwy_container_gis_object_by_name(data, "/0/show",
                                         (GObject**)&shadefield)) {
        gwy_data_field_resample(shadefield,
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_area_copy(dfield, shadefield,
                                 0, 0, gwy_data_field_get_xres(dfield),
                                 gwy_data_field_get_yres(dfield), 0, 0);
    }
    else {
        shadefield = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(data, "/0/show", G_OBJECT(shadefield));
        g_object_unref(shadefield);
    }

    gwy_data_field_filter_rms(shadefield, 5);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
