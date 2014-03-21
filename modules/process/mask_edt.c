/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define MASKEDT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    MASKEDT_INTERIOR = 0,
    MASKEDT_EXTERIOR = 1,
    MASKEDT_SIGNED   = 2,
    MASKEDT_NTYPES
} MaskEdtType;

typedef struct {
    MaskEdtType type;
    gboolean from_border;
} MaskEdtArgs;

typedef struct {
    MaskEdtArgs *args;
    GtkWidget *dialog;
    GSList *type;
    GtkWidget *from_border;
} MaskEdtControls;

static gboolean      module_register              (void);
static void          mask_edt                     (GwyContainer *data,
                                                   GwyRunType run);
static gboolean      maskedt_dialog               (MaskEdtArgs *args);
static void          type_changed                 (GtkToggleButton *toggle,
                                                   MaskEdtControls *controls);
static void          from_border_changed          (GtkToggleButton *toggle,
                                                   MaskEdtControls *controls);
static GwyDataField* maskedt_do                   (GwyDataField *mfield,
                                                   GwyDataField *dfield,
                                                   MaskEdtArgs *args);
static void          borderless_distance_transform(GwyDataField *dfield);
static void          maskedt_sanitize_args        (MaskEdtArgs *args);
static void          maskedt_load_args            (GwyContainer *settings,
                                                   MaskEdtArgs *args);
static void          maskedt_save_args            (GwyContainer *settings,
                                                   MaskEdtArgs *args);

static const MaskEdtArgs maskedt_defaults = {
    MASKEDT_INTERIOR,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Performs Euclidean distance transform of masks."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("mask_edt",
                              (GwyProcessFunc)&mask_edt,
                              N_("/_Mask/_Euclidean Distance Transform..."),
                              NULL,
                              MASKEDT_RUN_MODES,
                              GWY_MENU_FLAG_DATA_MASK | GWY_MENU_FLAG_DATA,
                              N_("Euclidean distance transform of mask"));

    return TRUE;
}

static void
mask_edt(GwyContainer *data, GwyRunType run)
{
    GwyDataField *mfield, *dfield;
    MaskEdtArgs args;
    gint oldid, newid;

    g_return_if_fail(run & MASKEDT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(mfield && dfield);

    maskedt_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_IMMEDIATE || maskedt_dialog(&args)) {
        dfield = maskedt_do(mfield, dfield, &args);

        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        g_object_unref(dfield);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                0);
        gwy_app_set_data_field_title(data, newid, _("Distance Transform"));
        gwy_app_channel_log_add(data, oldid, newid, "proc::mask_edt", NULL);
    }
    maskedt_save_args(gwy_app_settings_get(), &args);
}

static gboolean
maskedt_dialog(MaskEdtArgs *args)
{
    static const GwyEnum types[] = {
        { N_("Interoir"),  MASKEDT_INTERIOR },
        { N_("Exteroir"),  MASKEDT_EXTERIOR },
        { N_("Two-sided"), MASKEDT_SIGNED   },
    };

    MaskEdtControls controls;
    GtkWidget *dialog;
    GtkWidget *table, *label;
    gint response, row = 0;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Distance Transform"),
                                         NULL, 0,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    table = gtk_table_new(5, 1, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.type = gwy_radio_buttons_create(types, G_N_ELEMENTS(types),
                                             G_CALLBACK(type_changed),
                                             &controls,
                                             args->type);
    row = gwy_radio_buttons_attach_to_table(controls.type, GTK_TABLE(table),
                                            1, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    controls.from_border
        = gtk_check_button_new_with_mnemonic(_("Shrink from _border"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.from_border),
                                 args->from_border);
    gtk_table_attach(GTK_TABLE(table), controls.from_border,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.from_border, "toggled",
                     G_CALLBACK(from_border_changed), &controls);
    row++;

    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
type_changed(G_GNUC_UNUSED GtkToggleButton *toggle,
             MaskEdtControls *controls)
{
    controls->args->type = gwy_radio_buttons_get_current(controls->type);
}

static void
from_border_changed(GtkToggleButton *toggle,
                    MaskEdtControls *controls)
{
    controls->args->from_border = gtk_toggle_button_get_active(toggle);
}

static GwyDataField*
maskedt_do(GwyDataField *mfield,
           GwyDataField *dfield,
           MaskEdtArgs *args)
{
    void (*edt_func)(GwyDataField *dfield);
    GwySIUnit *unitxy, *unitz;
    gdouble q;

    if (args->from_border)
        edt_func = gwy_data_field_grain_distance_transform;
    else
        edt_func = borderless_distance_transform;

    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_copy(mfield, dfield, FALSE);

    if (args->type == MASKEDT_INTERIOR) {
        edt_func(dfield);
    }
    else if (args->type == MASKEDT_EXTERIOR) {
        gwy_data_field_multiply(dfield, -1.0);
        gwy_data_field_add(dfield, 1.0);
        edt_func(dfield);
    }
    else if (args->type == MASKEDT_SIGNED) {
        GwyDataField *tmp = gwy_data_field_duplicate(dfield);

        edt_func(dfield);
        gwy_data_field_multiply(tmp, -1.0);
        gwy_data_field_add(tmp, 1.0);
        edt_func(tmp);
        gwy_data_field_subtract_fields(dfield, dfield, tmp);
        g_object_unref(tmp);
    }

    q = sqrt(gwy_data_field_get_xmeasure(dfield)
             * gwy_data_field_get_ymeasure(dfield));
    gwy_data_field_multiply(dfield, q);
    unitxy = gwy_data_field_get_si_unit_xy(dfield);
    unitz = gwy_data_field_get_si_unit_z(dfield);
    gwy_serializable_clone(G_OBJECT(unitxy), G_OBJECT(unitz));

    return dfield;
}

static void
borderless_distance_transform(GwyDataField *dfield)
{
    guint xres = dfield->xres, yres = dfield->yres;
    GwyDataField *extended;

    extended = gwy_data_field_extend(dfield,
                                     xres/2, xres/2, yres/2, yres/2,
                                     GWY_EXTERIOR_BORDER_EXTEND, 0.0, FALSE);
    gwy_data_field_grain_distance_transform(extended);
    gwy_data_field_area_copy(extended, dfield,
                             xres/2, yres/2, xres, yres, 0, 0);
    g_object_unref(extended);
}

static const gchar type_key[]        = "/module/mask_edt/type";
static const gchar from_border_key[] = "/module/mask_edt/from_border";

static void
maskedt_sanitize_args(MaskEdtArgs *args)
{
    args->type = MIN(args->type, MASKEDT_NTYPES-1);
    args->from_border = !!args->from_border;
}

static void
maskedt_load_args(GwyContainer *settings,
                  MaskEdtArgs *args)
{
    *args = maskedt_defaults;
    gwy_container_gis_enum_by_name(settings, type_key, &args->type);
    gwy_container_gis_boolean_by_name(settings, from_border_key,
                                      &args->from_border);
    maskedt_sanitize_args(args);
}

static void
maskedt_save_args(GwyContainer *settings,
                  MaskEdtArgs *args)
{
    gwy_container_set_enum_by_name(settings, type_key, args->type);
    gwy_container_set_boolean_by_name(settings, from_border_key,
                                      args->from_border);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
