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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <cairo.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define VOLUMIZE_RUN_MODES (GWY_RUN_IMMEDIATE)
#define MAXPIX 600

typedef struct {
    gboolean update;
} VolumizeArgs;


static gboolean  module_register            (void);
static void      volumize                   (GwyContainer *data,
                                             GwyRunType run);
static GwyBrick* create_brick_from_datafield(GwyDataField *dfield);

static const VolumizeArgs volumize_defaults = {
    0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Converts datafield to 3D volume data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("volumize",
                              (GwyProcessFunc)&volumize,
                              N_("/_Basic Operations/Volumize..."),
                              NULL,
                              VOLUMIZE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Convert datafield to 3D data"));

    return TRUE;
}

static void
volumize(GwyContainer *data, GwyRunType run)
{
    VolumizeArgs args;
    GwyDataField *dfield = NULL;
    GwyBrick *brick;
    gint id;

    g_return_if_fail(run & VOLUMIZE_RUN_MODES);

    //volumize_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    brick = create_brick_from_datafield(dfield);
    /* Use showit=TRUE once we thin it's safe */
    gwy_app_data_browser_add_brick(brick, data, FALSE);
    g_object_unref(brick);
    //volumize_save_args(gwy_app_settings_get(), &args);
}


static GwyBrick*
create_brick_from_datafield(GwyDataField *dfield)
{
    gint xres, yres, zres;
    gint col, row, lev;
    gdouble ratio, *bdata, *ddata;
    gdouble zreal, offset;
    gboolean freeme = FALSE;
    GwyDataField *lowres;
    GwyBrick *brick;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    zres = MAX(xres, yres);

    if ((xres*yres)>(MAXPIX*MAXPIX))
    {
        ratio = (MAXPIX*MAXPIX)/(gdouble)(xres*yres);
        lowres = gwy_data_field_new_alike(dfield, TRUE);
        gwy_data_field_copy(dfield, lowres, TRUE);
        xres *= ratio;
        yres *= ratio;
        freeme = TRUE;
        gwy_data_field_resample(lowres, xres, yres, GWY_INTERPOLATION_BILINEAR);
    }
    else lowres = dfield;

    zres = MAX(xres, yres);

    offset = gwy_data_field_get_min(lowres);
    zreal = gwy_data_field_get_max(lowres) - offset;

    brick = gwy_brick_new(xres, yres, zres, xres, yres, zres, TRUE);

    ddata = gwy_data_field_get_data(lowres);
    bdata = gwy_brick_get_data(brick);

    for (col=0; col<xres; col++)
    {
        for (row=0; row<yres; row++)
        {
            for (lev=0; lev<zres; lev++) {
                if (ddata[col + xres*row]<(lev*zreal/zres + offset)) bdata[col + xres*row + xres*yres*lev] = 1;

            }
        }
    }
    if (freeme) gwy_object_unref(lowres);

    return brick;

}

/*static const gchar xpos_key[]       = "/module/volumize/xpos";

static void
volumize_sanitize_args(VolumizeArgs *args)
{
    args->xpos = CLAMP(args->xpos, 0, 100);
}

static void
volumize_load_args(GwyContainer *container,
                     VolumizeArgs *args)
{
    *args = volumize_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    volumize_sanitize_args(args);
}

static void
volumize_save_args(GwyContainer *container,
                     VolumizeArgs *args)
{
    gwy_container_set_enum_by_name(container, type_key, args->type);
}
*/
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
