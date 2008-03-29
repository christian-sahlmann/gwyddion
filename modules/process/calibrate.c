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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <gtk/gtk.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define CALIBRATE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gdouble xratio;
    gdouble yratio;
    gdouble zratio;
    gint xyexponent;
    gint zexponent;
    gboolean square;
    gdouble xreal;
    gdouble yreal;
    gdouble zreal;
    gdouble x0;
    gdouble y0;
    gdouble xorig;
    gdouble yorig;
    gdouble zorig;
    gdouble x0orig;
    gdouble y0orig;
    gint xyorigexp;
    gint zorigexp;
    gint xres;
    gint yres;
    gchar *xyunit;
    gchar *xyunitorig;
    gchar *zunit;
    gchar *zunitorig;
} CalibrateArgs;

typedef struct {
    CalibrateArgs *args;
    GtkObject *xratio;
    GtkObject *yratio;
    GtkObject *zratio;
    GtkWidget *xyexponent;
    GtkWidget *zexponent;
    GtkWidget *xpower10;
    GtkWidget *ypower10;
    GtkWidget *zpower10;
    GtkWidget *square;
    GtkObject *xreal;
    GtkObject *yreal;
    GtkObject *zreal;
    GtkObject *x0;
    GtkObject *y0;
    gboolean in_update;
    GtkWidget *xyunits;
    GtkWidget *zunits;
} CalibrateControls;

static gboolean    module_register           (void);
static void        calibrate                 (GwyContainer *data,
                                              GwyRunType run);
static gboolean    calibrate_dialog          (CalibrateArgs *args,
                                              GwyDataField *dfield);
static void        dialog_reset              (CalibrateControls *controls,
                                              CalibrateArgs *args);
static void        xratio_changed_cb         (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        yratio_changed_cb         (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        zratio_changed_cb         (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        xreal_changed_cb          (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        yreal_changed_cb          (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        x0_changed_cb             (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        y0_changed_cb             (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        zreal_changed_cb          (GtkAdjustment *adj,
                                              CalibrateControls *controls);
static void        square_changed_cb         (GtkWidget *check,
                                              CalibrateControls *controls);
static void        xyexponent_changed_cb     (GtkWidget *combo,
                                              CalibrateControls *controls);
static void        zexponent_changed_cb      (GtkWidget *combo,
                                              CalibrateControls *controls);
static void        calibrate_dialog_update   (CalibrateControls *controls,
                                              CalibrateArgs *args);
static void        calibrate_sanitize_args   (CalibrateArgs *args);
static void        calibrate_load_args       (GwyContainer *container,
                                              CalibrateArgs *args);
static void        calibrate_save_args       (GwyContainer *container,
                                              CalibrateArgs *args);
static void        units_change_cb           (GtkWidget *button,
                                              CalibrateControls *controls);
static void        set_combo_from_unit       (GtkWidget *combo,
                                              const gchar *str);

static const CalibrateArgs calibrate_defaults = {
    1.0,
    1.0,
    1.0,
    -6,
    -6,
    TRUE,
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
    "m", "m", "m", "m"
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Recalibrates scan lateral dimensions or value range."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("calibrate",
                              (GwyProcessFunc)&calibrate,
                              N_("/_Basic Operations/_Recalibrate..."),
                              GWY_STOCK_DATA_MEASURE,
                              CALIBRATE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Recalibrate data"));

    return TRUE;
}

static void
calibrate(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfields[3];
    GQuark quark;
    gint oldid, newid;
    CalibrateArgs args;
    gboolean ok;
    GwySIUnit *siunitxy, *siunitz;

    g_return_if_fail(run & CALIBRATE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, dfields + 0,
                                     GWY_APP_MASK_FIELD, dfields + 1,
                                     GWY_APP_SHOW_FIELD, dfields + 2,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfields[0]);

    calibrate_load_args(gwy_app_settings_get(), &args);
    args.xorig = gwy_data_field_get_xreal(dfields[0]);
    args.yorig = gwy_data_field_get_yreal(dfields[0]);
    args.zorig = gwy_data_field_get_max(dfields[0])
                 - gwy_data_field_get_min(dfields[0]);
    args.xres = gwy_data_field_get_xres(dfields[0]);
    args.yres = gwy_data_field_get_yres(dfields[0]);
    args.x0orig = gwy_data_field_get_xoffset(dfields[0]);
    args.y0orig = gwy_data_field_get_yoffset(dfields[0]);
    args.xyorigexp = 3*floor(log10(args.xorig*args.yorig)/6);
    args.zorigexp = 3*floor(log10(args.zorig)/3);
    args.xreal = args.xratio * args.xorig;
    args.yreal = args.yratio * args.yorig;
    args.zreal = args.zratio * args.zorig;
    args.xyexponent = 3*floor(log10(args.xreal*args.yreal)/6);
    args.zexponent = 3*floor(log10(args.zreal)/3);
    args.x0 = args.x0orig;
    args.y0 = args.y0orig;
    siunitxy = gwy_data_field_get_si_unit_xy(dfields[0]);
    args.xyunitorig = gwy_si_unit_get_string(siunitxy, GWY_SI_UNIT_FORMAT_VFMARKUP);
    args.xyunit = args.xyunitorig;
    siunitz = gwy_data_field_get_si_unit_z(dfields[0]);
    args.zunitorig = gwy_si_unit_get_string(siunitz, GWY_SI_UNIT_FORMAT_VFMARKUP);
    args.zunit = args.zunitorig;

    if (run == GWY_RUN_INTERACTIVE) {
        ok = calibrate_dialog(&args, dfields[0]);
        calibrate_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    dfields[0] = gwy_data_field_duplicate(dfields[0]);

    if (args.xreal != args.xorig)
        gwy_data_field_set_xreal(dfields[0], args.xreal);
    if (args.yreal != args.yorig)
        gwy_data_field_set_yreal(dfields[0], args.yreal);
    if (args.zratio != 1.0)
        gwy_data_field_multiply(dfields[0], args.zratio);
    if (args.x0 != args.x0orig)
        gwy_data_field_set_xoffset(dfields[0], args.x0);
    if (args.y0 != args.y0orig)
        gwy_data_field_set_yoffset(dfields[0], args.y0);
    if (args.xyunit != args.xyunitorig) {
        gwy_si_unit_set_from_string(siunitxy, args.xyunit);
        gwy_data_field_set_si_unit_xy(dfields[0], siunitxy);
    }
    if (args.zunit != args.zunitorig) {
      gwy_si_unit_set_from_string(siunitz, args.zunit);
      gwy_data_field_set_si_unit_z(dfields[0], siunitz);
    }

    if (dfields[1]) {
        dfields[1] = gwy_data_field_duplicate(dfields[1]);
        gwy_data_field_set_xreal(dfields[1], args.xreal);
        gwy_data_field_set_yreal(dfields[1], args.yreal);
        gwy_data_field_set_xoffset(dfields[1], args.x0);
        gwy_data_field_set_xoffset(dfields[1], args.y0);
    }

    if (dfields[2]) {
        dfields[2] = gwy_data_field_duplicate(dfields[2]);
        gwy_data_field_set_xreal(dfields[2], args.xreal);
        gwy_data_field_set_yreal(dfields[2], args.yreal);
        gwy_data_field_set_xoffset(dfields[2], args.x0);
        gwy_data_field_set_xoffset(dfields[2], args.y0);
    }

    newid = gwy_app_data_browser_add_data_field(dfields[0], data, TRUE);
    g_object_unref(dfields[0]);
    gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    if (dfields[1]) {
        quark = gwy_app_get_mask_key_for_id(newid);
        gwy_container_set_object(data, quark, dfields[1]);
        g_object_unref(dfields[1]);
    }
    if (dfields[2]) {
        quark = gwy_app_get_show_key_for_id(newid);
        gwy_container_set_object(data, quark, dfields[2]);
        g_object_unref(dfields[2]);
    }

    gwy_app_set_data_field_title(data, newid, _("Recalibrated Data"));
}

static gboolean
calibrate_dialog(CalibrateArgs *args,
                 GwyDataField *dfield)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *spin, *table, *label;
    GwySIUnit *unit;
    CalibrateControls controls;
    gint row, response;

    dialog = gtk_dialog_new_with_buttons(_("Calibrate"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    controls.args = args;
    controls.in_update = TRUE;

    table = gtk_table_new(13, 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    /***** New Real Dimensions *****/
    label = gwy_label_new_header(_("New Real Dimensions"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_X range:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xreal = gtk_adjustment_new(args->xreal/pow10(args->xyexponent),
                                        0.01, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xreal), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_xy(dfield);
    controls.xyexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(xyexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->xyexponent);
    gtk_table_attach(GTK_TABLE(table), controls.xyexponent, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y range:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.yreal = gtk_adjustment_new(args->yreal/pow10(args->xyexponent),
                                        0.01, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yreal), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.xyunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.xyunits), "id", (gpointer)"xy");
    gtk_table_attach(GTK_TABLE(table), controls.xyunits,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;

    controls.square
        = gtk_check_button_new_with_mnemonic(_("_Square samples"));
    gtk_table_attach(GTK_TABLE(table), controls.square,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_X offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.x0 = gtk_adjustment_new(args->x0/pow10(args->xyexponent),
                                     -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.x0), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y offset:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.y0 = gtk_adjustment_new(args->y0/pow10(args->xyexponent),
                                     -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.y0), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /***** Value Range *****/
    label = gwy_label_new_header(_("Value Range"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Z range:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.zreal = gtk_adjustment_new(args->zreal/pow10(args->zexponent),
                                        0.01, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zreal), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_z(dfield);
    controls.zexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(zexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->zexponent);
    gtk_table_attach(GTK_TABLE(table), controls.zexponent, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;

    controls.zunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.zunits), "id", (gpointer)"z");
    gtk_table_attach(GTK_TABLE(table), controls.zunits,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    row++;

    /***** Calibration Coefficients *****/
    label = gwy_label_new_header(_("Calibration Coefficients"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.xratio = gtk_adjustment_new(args->xratio, 0.001, 1000, 0.1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("_X calibration factor:"), "",
                                       controls.xratio);
    controls.xpower10 = gwy_table_get_child_widget(table, row, 2);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    row++;

    controls.yratio = gtk_adjustment_new(args->yratio, 0.001, 1000, 0.1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("_Y calibration factor:"), "",
                                       controls.yratio);
    controls.ypower10 = gwy_table_get_child_widget(table, row, 2);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    row++;

    controls.zratio = gtk_adjustment_new(args->zratio, 0.001, 1000, 0.1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row,
                                       _("_Z calibration factor:"), "",
                                       controls.zratio);
    controls.zpower10 = gwy_table_get_child_widget(table, row, 2);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    row++;

    g_signal_connect(controls.xreal, "value-changed",
                     G_CALLBACK(xreal_changed_cb), &controls);
    g_signal_connect(controls.yreal, "value-changed",
                     G_CALLBACK(yreal_changed_cb), &controls);
    g_signal_connect(controls.xyunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    g_signal_connect(controls.zunits, "clicked",
                     G_CALLBACK(units_change_cb), &controls);
    g_signal_connect(controls.x0, "value-changed",
                     G_CALLBACK(x0_changed_cb), &controls);
    g_signal_connect(controls.y0, "value-changed",
                     G_CALLBACK(y0_changed_cb), &controls);
    g_signal_connect(controls.zreal, "value-changed",
                     G_CALLBACK(zreal_changed_cb), &controls);
    g_signal_connect(controls.xratio, "value-changed",
                     G_CALLBACK(xratio_changed_cb), &controls);
    g_signal_connect(controls.yratio, "value-changed",
                     G_CALLBACK(yratio_changed_cb), &controls);
    g_signal_connect(controls.zratio, "value-changed",
                     G_CALLBACK(zratio_changed_cb), &controls);
    g_signal_connect(controls.square, "toggled",
                     G_CALLBACK(square_changed_cb), &controls);

    controls.in_update = FALSE;
    /* sync all fields */
    calibrate_dialog_update(&controls, args);

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

            case RESPONSE_RESET:
            dialog_reset(&controls, args);
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
dialog_reset(CalibrateControls *controls,
             CalibrateArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->square),
                                 calibrate_defaults.square);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->xyexponent),
                                  args->xyorigexp);
    args->xyexponent = args->xyorigexp;
    set_combo_from_unit(controls->xyexponent, args->xyunitorig);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yratio),
                             calibrate_defaults.yratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xratio),
                             calibrate_defaults.xratio);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->zexponent),
                                  args->zorigexp);
    args->zexponent = args->zorigexp;
    set_combo_from_unit(controls->zexponent, args->zunitorig);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zratio),
                             calibrate_defaults.zratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->x0),
                             args->x0orig/pow10(args->xyexponent));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->y0),
                             args->y0orig/pow10(args->xyexponent));
    calibrate_dialog_update(controls, args);
}

static void
xratio_changed_cb(GtkAdjustment *adj,
                  CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xratio = gtk_adjustment_get_value(adj)
                   * pow10(args->xyexponent - args->xyorigexp);
    args->xreal = args->xratio * args->xorig;
    if (args->square) {
        args->yreal = args->xreal/args->xres * args->yres;
        args->yratio = args->yreal/args->yorig;
    }
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
yratio_changed_cb(GtkAdjustment *adj,
                  CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yratio = gtk_adjustment_get_value(adj)
                   * pow10(args->xyexponent - args->xyorigexp);
    args->yreal = args->yratio * args->yorig;
    if (args->square) {
        args->xreal = args->yreal/args->yres * args->xres;
        args->xratio = args->xreal/args->xorig;
    }
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
zratio_changed_cb(GtkAdjustment *adj,
                  CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zratio = gtk_adjustment_get_value(adj)
                   * pow10(args->zexponent - args->zorigexp);
    args->zreal = args->zratio * args->zorig;
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
xreal_changed_cb(GtkAdjustment *adj,
                 CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xreal = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    args->xratio = args->xreal/args->xorig;
    if (args->square) {
        args->yreal = args->xreal/args->xres * args->yres;
        args->yratio = args->yreal/args->yorig;
    }
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
yreal_changed_cb(GtkAdjustment *adj,
                 CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    args->yreal = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    args->yratio = args->yreal/args->yorig;
    if (args->square) {
        args->xreal = args->yreal/args->yres * args->xres;
        args->xratio = args->xreal/args->xorig;
    }
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
x0_changed_cb(GtkAdjustment *adj,
              CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->x0 = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
y0_changed_cb(GtkAdjustment *adj,
              CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    args->y0 = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
zreal_changed_cb(GtkAdjustment *adj,
                 CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    args->zreal = gtk_adjustment_get_value(adj) * pow10(args->zexponent);
    args->zratio = args->zreal/args->zorig;
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
square_changed_cb(GtkWidget *check,
                  CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    args->square
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check));
    if (args->square) {
        args->yreal = args->xreal/args->xres * args->yres;
        args->yratio = args->yreal/args->yorig;
    }
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
xyexponent_changed_cb(GtkWidget *combo,
                      CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xyexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->xreal = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xreal))
                  * pow10(args->xyexponent);
    args->x0 = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->x0))
               * pow10(args->xyexponent);
    args->xratio = args->xreal/args->xorig;
    args->yreal = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yreal))
                  * pow10(args->xyexponent);
    args->y0 = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->y0))
               * pow10(args->xyexponent);
    args->yratio = args->yreal/args->yorig;
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
zexponent_changed_cb(GtkWidget *combo,
                     CalibrateControls *controls)
{
    CalibrateArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;

    args->zexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->zreal = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zreal))
                  * pow10(args->zexponent);
    args->zratio = args->zreal/args->zorig;
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
units_change_cb(GtkWidget *button,
                CalibrateControls *controls)
{
  GtkWidget *dialog, *hbox, *label, *entry;
  const gchar *id, *unit;
  gint response;
  CalibrateArgs *args = controls->args;

  if (controls->in_update)
    return;

  controls->in_update = TRUE;

  id = g_object_get_data(G_OBJECT(button), "id");
  dialog = gtk_dialog_new_with_buttons(_("Change Units"),
                                       NULL,
                                           GTK_DIALOG_MODAL
                                               | GTK_DIALOG_NO_SEPARATOR,
                                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                               GTK_STOCK_OK, GTK_RESPONSE_OK,
                                               NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

  hbox = gtk_hbox_new(FALSE, 6);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                     FALSE, FALSE, 0);

  label = gtk_label_new_with_mnemonic(_("New _units:"));
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

  entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

  gtk_widget_show_all(dialog);
  response = gtk_dialog_run(GTK_DIALOG(dialog));
  if (response != GTK_RESPONSE_OK) {
    gtk_widget_destroy(dialog);
    return;
  }

  unit = gtk_entry_get_text(GTK_ENTRY(entry));
  if (gwy_strequal(id, "xy")) {
    set_combo_from_unit(controls->xyexponent, unit);
    controls->args->xyunit = g_strdup(unit);
  }
  else if (gwy_strequal(id, "z")) {
    set_combo_from_unit(controls->zexponent, unit);
    controls->args->zunit = g_strdup(unit);
  }

  gtk_widget_destroy(dialog);

  calibrate_dialog_update(controls, args);
  controls->in_update = FALSE;
}

static void
set_combo_from_unit(GtkWidget *combo,
                    const gchar *str)
{
  GwySIUnit *unit;
  gint power10;

  unit = gwy_si_unit_new_parse(str, &power10);
  gwy_combo_box_metric_unit_set_unit(GTK_COMBO_BOX(combo),
                                     power10 - 6, power10 + 6, unit);
  g_object_unref(unit);
}

static void
calibrate_dialog_update(CalibrateControls *controls,
                        CalibrateArgs *args)
{
    gchar buffer[32];
    gint e;

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xreal),
                             args->xreal/pow10(args->xyexponent));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yreal),
                             args->yreal/pow10(args->xyexponent));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->x0),
                             args->x0/pow10(args->xyexponent));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->y0),
                             args->y0/pow10(args->xyexponent));

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zreal),
                             args->zreal/pow10(args->zexponent));

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xratio),
                             args->xratio/pow10(args->xyexponent
                                                - args->xyorigexp));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yratio),
                             args->yratio/pow10(args->xyexponent
                                                - args->xyorigexp));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zratio),
                             args->zratio/pow10(args->zexponent
                                                - args->zorigexp));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->square),
                                 args->square);

    e = args->xyexponent - args->xyorigexp;
    if (!e)
        buffer[0] = '\0';
    else
        g_snprintf(buffer, sizeof(buffer), "× 10<sup>%d</sup>", e);
    gtk_label_set_markup(GTK_LABEL(controls->xpower10), buffer);
    gtk_label_set_markup(GTK_LABEL(controls->ypower10), buffer);

    e = args->zexponent - args->zorigexp;
    if (!e)
        buffer[0] = '\0';
    else
        g_snprintf(buffer, sizeof(buffer), "× 10<sup>%d</sup>", e);
    gtk_label_set_markup(GTK_LABEL(controls->zpower10), buffer);
}

static const gchar xratio_key[] = "/module/calibrate/xratio";
static const gchar yratio_key[] = "/module/calibrate/yratio";
static const gchar zratio_key[] = "/module/calibrate/zratio";
static const gchar square_key[] = "/module/calibrate/square";

static void
calibrate_sanitize_args(CalibrateArgs *args)
{
    args->xratio = CLAMP(args->xratio, 1e-15, 1e15);
    args->yratio = CLAMP(args->yratio, 1e-15, 1e15);
    args->zratio = CLAMP(args->zratio, 1e-15, 1e15);
    args->square = !!args->square;
}

static void
calibrate_load_args(GwyContainer *container,
                    CalibrateArgs *args)
{
    *args = calibrate_defaults;

    gwy_container_gis_double_by_name(container, xratio_key, &args->xratio);
    gwy_container_gis_double_by_name(container, yratio_key, &args->yratio);
    gwy_container_gis_double_by_name(container, zratio_key, &args->zratio);
    gwy_container_gis_boolean_by_name(container, square_key, &args->square);
    calibrate_sanitize_args(args);
}

static void
calibrate_save_args(GwyContainer *container,
                    CalibrateArgs *args)
{
    gwy_container_set_double_by_name(container, xratio_key, args->xratio);
    gwy_container_set_double_by_name(container, yratio_key, args->yratio);
    gwy_container_set_double_by_name(container, zratio_key, args->zratio);
    gwy_container_set_boolean_by_name(container, square_key, args->square);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
