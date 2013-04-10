/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/brick.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>

#define EXTRACT_RUN_MODES GWY_RUN_INTERACTIVE

static gboolean module_register(void);
static void     extract        (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Extracts one-dimensional and two-dimensional sections of volume data."),
    "Yeti <yeti@gwyddion.net>, Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_volume_func_register("volume_extract",
                             (GwyVolumeFunc)&extract,
                             N_("/_Extract Section"),
                             NULL,
                             EXTRACT_RUN_MODES,
                             GWY_MENU_FLAG_VOLUME,
                             N_("Extract one-dimensional and two-dimensional "
                                "sections of volume data."));

    return TRUE;
}

static void
extract(GwyContainer *data, GwyRunType run)
{
    GQuark quark;
    GwyBrick *brick;

    g_return_if_fail(run & EXTRACT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_BRICK_KEY, &quark,
                                     GWY_APP_BRICK, &brick,
                                     0);
    g_return_if_fail(brick && quark);

    /* Do something here. */
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
