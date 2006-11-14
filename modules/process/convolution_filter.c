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
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include "convolutionfilterpreset.h"

#define CONVOLUTION_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

static gboolean module_register   (void);
static void     convolution_filter(GwyContainer *data,
                                   GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Generic convolution filter with user-defined matrix."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("convolution_filter",
                              (GwyProcessFunc)&convolution_filter,
                              N_("/_Basic Operations/Convolution _Filter..."),
                              NULL,
                              CONVOLUTION_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Generic convolution filter"));

    return TRUE;
}

static void
convolution_filter(GwyContainer *data,
                   GwyRunType run)
{
    GwyDataField *dfield;
    GQuark dquark;
    gint id;

    g_return_if_fail(run & CONVOLUTION_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark);

    gwy_app_undo_qcheckpointv(data, 1, &dquark);

    gwy_data_field_data_changed(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
