/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwystock.h>
#include <app/gwyapp.h>

#define MASKOPS_RUN_MODES GWY_RUN_IMMEDIATE

static gboolean module_register(void);
static void     mask_remove    (GwyContainer *data,
                                GwyRunType run);
static void     mask_invert    (GwyContainer *data,
                                GwyRunType run);
static void     mask_extract   (GwyContainer *data,
                                GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Basic operations with mask: inversion, removal, extraction."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("mask_remove",
                              (GwyProcessFunc)&mask_remove,
                              N_("/_Mask/_Remove Mask"),
                              GWY_STOCK_MASK_REMOVE,
                              MASKOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Remove mask from data"));
    gwy_process_func_register("mask_invert",
                              (GwyProcessFunc)&mask_invert,
                              N_("/_Mask/_Invert Mask"),
                              GWY_STOCK_MASK_INVERT,
                              MASKOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Invert mask"));
    gwy_process_func_register("mask_extract",
                              (GwyProcessFunc)&mask_extract,
                              N_("/_Mask/_Extract Mask"),
                              NULL,
                              MASKOPS_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Extract mask to a new channel"));

    return TRUE;
}

static void
mask_invert(GwyContainer *data, GwyRunType run)
{
    GwyDataField *mfield;
    GQuark mquark;

    g_return_if_fail(run & MASKOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(mfield && mquark);

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    gwy_data_field_multiply(mfield, -1.0);
    gwy_data_field_add(mfield, 1.0);
    gwy_data_field_data_changed(mfield);
}

static void
mask_remove(GwyContainer *data, GwyRunType run)
{
    GQuark mquark;

    g_return_if_fail(run & MASKOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_MASK_FIELD_KEY, &mquark, 0);
    g_return_if_fail(mquark);

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    gwy_container_remove(data, mquark);
}

static void
mask_extract(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gint oldid, newid;

    g_return_if_fail(run & MASKOPS_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_MASK_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_clamp(dfield, 0.0, 1.0);

    /* Other functions should set the units correctly, but do not trust them */
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);
    gwy_app_set_data_field_title(data, newid, _("Mask"));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
