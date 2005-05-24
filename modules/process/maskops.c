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

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#define MASKOPS_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

static gboolean    module_register            (const gchar *name);
static gboolean    mask_remove                (GwyContainer *data,
                                               GwyRunType run);
static gboolean    mask_invert                (GwyContainer *data,
                                               GwyRunType run);
static gboolean    mask_extract               (GwyContainer *data,
                                               GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Basic operations with mask: inversion, removal, extraction."),
    "Yeti <yeti@gwyddion.net>",
    "1.0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo mask_remove_func_info = {
        "mask_remove",
        N_("/_Mask/_Remove Mask"),
        (GwyProcessFunc)&mask_remove,
        MASKOPS_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };
    static GwyProcessFuncInfo mask_invert_func_info = {
        "mask_invert",
        N_("/_Mask/_Invert Mask"),
        (GwyProcessFunc)&mask_invert,
        MASKOPS_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };
    static GwyProcessFuncInfo mask_extract_func_info = {
        "mask_extract",
        N_("/_Mask/_Extract Mask"),
        (GwyProcessFunc)&mask_extract,
        MASKOPS_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };

    gwy_process_func_register(name, &mask_remove_func_info);
    gwy_process_func_register(name, &mask_invert_func_info);
    gwy_process_func_register(name, &mask_extract_func_info);

    return TRUE;
}

static gboolean
mask_invert(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & MASKOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    g_return_val_if_fail(dfield, FALSE);

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    gwy_data_field_multiply(dfield, -1.0);
    gwy_data_field_add(dfield, 1.0);
    gwy_data_field_data_changed(dfield);

    return TRUE;
}

static gboolean
mask_remove(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;

    g_return_val_if_fail(run & MASKOPS_RUN_MODES, FALSE);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/mask"));
    g_return_val_if_fail(dfield, FALSE);

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    gwy_container_remove_by_name(data, "/0/mask");

    return TRUE;
}

static gboolean
mask_extract(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    const gchar *pal;

    g_return_val_if_fail(run & MASKOPS_RUN_MODES, FALSE);
    dfield = gwy_container_get_object_by_name(data, "/0/mask");
    g_return_val_if_fail(dfield, FALSE);

    pal = gwy_container_get_string_by_name(data, "/0/base/palette");
    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_clamp(dfield, 0.0, 1.0);
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    data = gwy_container_new();
    gwy_container_set_object_by_name(data, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_string_by_name(data, "/0/base/palette", g_strdup(pal));

    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
