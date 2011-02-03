/*
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
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/inttrans.h>
#include <libprocess/gwycaldata.h>
#include <libprocess/gwycalibration.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define CNEW_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)
typedef enum {
   DUPLICATE_NONE = 0,
   DUPLICATE_OVERWRITE = 1,
   DUPLICATE_APPEND = 2
} ResponseDuplicate;

typedef struct {
    gdouble xrange_from;
    gdouble xrange_to;
    gdouble yrange_from;
    gdouble yrange_to;
    gdouble zrange_from;
    gdouble zrange_to;
    gdouble xmult;
    gdouble ymult;
    gdouble zmult;
    gdouble xunc;
    gdouble yunc;
    gdouble zunc;
    gint xyexponent;
    gint zexponent;
    gint xyuexponent;
    gint zuexponent;
    gchar *name;
    ResponseDuplicate duplicate;
} CNewArgs;

typedef struct {
    GtkObject *xrange_from;
    GtkObject *xrange_to;
    GtkObject *yrange_from;
    GtkObject *yrange_to;
    GtkObject *zrange_from;
    GtkObject *zrange_to;
    GtkObject *xmult;
    GtkObject *ymult;
    GtkObject *zmult;
    GtkObject *xunc;
    GtkObject *yunc;
    GtkObject *zunc;
    GtkWidget *xyexponent;
    GtkWidget *xyuexponent;
    GtkWidget *zexponent;
    GtkWidget *zuexponent;
    GtkWidget *xyunits;
    GtkWidget *zunits;
    gboolean in_update;
    CNewArgs *args;
    GtkEntry *name;
} CNewControls;

static gboolean    module_register            (void);
static void        cnew                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    cnew_dialog                 (CNewArgs *args, 
                                                GwyDataField *dfield);
static void        cnew_load_args              (GwyContainer *container,
                                               CNewArgs *args);
static void        cnew_save_args              (GwyContainer *container,
                                               CNewArgs *args);
static void        cnew_dialog_update          (CNewControls *controls,
                                               CNewArgs *args);
static void        xyexponent_changed_cb       (GtkWidget *combo,
                                               CNewControls *controls);
static void        xyuexponent_changed_cb      (GtkWidget *combo,
                                               CNewControls *controls);
static void        zexponent_changed_cb       (GtkWidget *combo,
                                               CNewControls *controls);
static void        zuexponent_changed_cb       (GtkWidget *combo,
                                               CNewControls *controls);
static void        units_change_cb             (GtkWidget *button,
                                               CNewControls *controls);
static void        set_combo_from_unit       (GtkWidget *combo,
                                              const gchar *str,
                                              gint basepower);
static void        xfrom_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        xto_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        yfrom_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        yto_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        zfrom_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        zto_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        xunc_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        yunc_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        zunc_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);

static void        xmult_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        ymult_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);
static void        zmult_changed_cb           (GtkAdjustment *adj,
                                               CNewControls *controls);



static const CNewArgs cnew_defaults = {
    0.0,
    100e-6,
    0.0,
    100e-6,
    0.0,
    10e-6,
    1.0,
    1.0,
    1.0,
    1e-8,
    1e-8,
    1e-9,
    -6,
    -6,
    -6,
    -6,
    "new calibration",
    0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Create simple calibration data"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("cnew",
                              (GwyProcessFunc)&cnew,
                              N_("/Cali_bration/_Create..."),
                              GWY_STOCK_CWT,
                              CNEW_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Specify simple calibration data."));

    return TRUE;
}

static void
cnew(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    CNewArgs args;
    gboolean ok;
    gint oldid, i, j, k, n;
    GwyCalibration *calibration;
    GwyCalData *caldata = NULL, *old;
    gchar *filename;
    gchar *contents;
    gsize len;
    GError *err = NULL;
    gsize pos = 0;
    GString *str;
    gdouble *x, *y, *z, *xunc, *yunc, *zunc, *xerr, *yerr, *zerr;
    FILE *fh;


    g_return_if_fail(run & CNEW_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    cnew_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = cnew_dialog(&args, dfield);
        cnew_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    /*create the caldata*/
    caldata = gwy_caldata_new(8);
    x = gwy_caldata_get_x(caldata);
    y = gwy_caldata_get_y(caldata);
    z = gwy_caldata_get_z(caldata);
    xerr = gwy_caldata_get_xerr(caldata);
    yerr = gwy_caldata_get_yerr(caldata);
    zerr = gwy_caldata_get_zerr(caldata);
    xunc = gwy_caldata_get_xunc(caldata);
    yunc = gwy_caldata_get_yunc(caldata);
    zunc = gwy_caldata_get_zunc(caldata);

    n = 0;
    for (i=0; i<2; i++)
    {
        for (j=0; j<2; j++)
        {
            for (k=0; k<2; k++)
            {

                if (i) x[n] = args.xrange_from;
                else x[n] = args.xrange_to;
                if (j) y[n] = args.yrange_from;
                else y[n] = args.yrange_to;
                if (k) z[n] = args.zrange_from;
                else z[n] = args.zrange_to;

                if (i) xerr[n] = (args.xrange_to-args.xrange_from)*(args.xmult-1);
                else xerr[n] = 0;
                if (j) yerr[n] = (args.yrange_to-args.yrange_from)*(args.ymult-1);
                else yerr[n] = 0;
                if (k) zerr[n] = (args.zrange_to-args.zrange_from)*(args.zmult-1);
                else zerr[n] = 0;

                xunc[n] = args.xunc;
                yunc[n] = args.yunc;
                zunc[n] = args.zunc;
                n++;
            }
        }
    }
 
    if (args.duplicate == DUPLICATE_APPEND && (calibration = gwy_inventory_get_item(gwy_calibrations(), args.name)))
        {
        filename = g_build_filename(gwy_get_user_dir(), "caldata", calibration->filename, NULL);
        if (!g_file_get_contents(filename,
                                 &contents, &len, &err))
        {
             g_warning("Error loading file: %s\n", err->message);
             g_clear_error(&err);
             return;
        }
        else {
            if (len)
              old = GWY_CALDATA(gwy_serializable_deserialize(contents, len, &pos));
            g_free(contents);
        }
        
        gwy_caldata_append(old, caldata);
        g_object_unref(caldata);
        caldata = old;
        
    }

    gwy_caldata_set_range(caldata, args.xrange_from, args.xrange_to,
                          args.yrange_from, args.yrange_to,
                          args.zrange_from, args.zrange_to);


    /*now create and save the resource*/
    if ((calibration = GWY_CALIBRATION(gwy_inventory_get_item(gwy_calibrations(), args.name)))==NULL)
    {
        calibration = gwy_calibration_new(args.name, g_strconcat(args.name, ".dat", NULL));
        gwy_inventory_insert_item(gwy_calibrations(), calibration);
        g_object_unref(calibration);
    }

    filename = gwy_resource_build_filename(GWY_RESOURCE(calibration));
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_mkdir(g_build_filename(gwy_get_user_dir(), "calibrations", NULL), 0700);
    }
    fh = g_fopen(filename, "wb");
    if (!fh) {
        g_warning("Cannot save preset: %s", filename);
        g_free(filename);
        return;
    }
    g_free(filename);

    str = gwy_resource_dump(GWY_RESOURCE(calibration));
    fwrite(str->str, 1, str->len, fh);
    fclose(fh);
    g_string_free(str, TRUE);

    gwy_resource_data_saved(GWY_RESOURCE(calibration));

    /*now save the calibration data*/
    //gwy_caldata_debug(caldata, "Saving: ");

    gwy_caldata_save_data(caldata, calibration->filename);

}

static gboolean
cnew_dialog(CNewArgs *args,
            GwyDataField *dfield)
{
    GtkWidget *dialog, *dialog2, *table, *spin, *label;
    GwySIUnit *unit;
    gint row;
    CNewControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_DUPLICATE_OVERWRITE = 2,
           RESPONSE_DUPLICATE_APPEND = 3 };
    gint response;

    controls.args = args;
    controls.in_update = TRUE;

    dialog = gtk_dialog_new_with_buttons(_("Simple Calibration Data"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    /*x from*/
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_X from:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xrange_from = gtk_adjustment_new(args->xrange_from/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xrange_from), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_xy(dfield);
    controls.xyexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(xyexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->xyexponent);
    gtk_table_attach(GTK_TABLE(table), controls.xyexponent, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    controls.xyunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.xyunits), "id", (gpointer)"xy");
    gtk_table_attach(GTK_TABLE(table), controls.xyunits,
                     3, 4, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;
 
    label = gtk_label_new_with_mnemonic(_("_X to:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xrange_to = gtk_adjustment_new(args->xrange_to/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xrange_to), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y from:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.yrange_from = gtk_adjustment_new(args->yrange_from/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yrange_from), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Y to:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.yrange_to = gtk_adjustment_new(args->yrange_to/pow10(args->xyexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yrange_to), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;


    label = gtk_label_new_with_mnemonic(_("_Z from:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.zrange_from = gtk_adjustment_new(args->zrange_from/pow10(args->zexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zrange_from), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_z(dfield);
    controls.zexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(zexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->zexponent);
    gtk_table_attach(GTK_TABLE(table), controls.zexponent, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

    controls.zunits = gtk_button_new_with_label(_("Change"));
    g_object_set_data(G_OBJECT(controls.zunits), "id", (gpointer)"z");
    gtk_table_attach(GTK_TABLE(table), controls.zunits,
                     3, 4, row, row+2,
                     GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);
    row++;
 
    label = gtk_label_new_with_mnemonic(_("_Z to:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.zrange_to = gtk_adjustment_new(args->zrange_to/pow10(args->zexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zrange_to), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("_X uncertainty:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xunc = gtk_adjustment_new(args->xunc/pow10(args->xyuexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xunc), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_xy(dfield);
    controls.xyuexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(xyuexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->xyuexponent);
    gtk_table_attach(GTK_TABLE(table), controls.xyuexponent, 2, 3, row, row+2,
                     GTK_EXPAND | GTK_FILL , 0, 0, 0);

    row++;
    label = gtk_label_new_with_mnemonic(_("_Y uncertainty:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

   
    controls.yunc = gtk_adjustment_new(args->yunc/pow10(args->xyuexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yunc), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    row++;

    label = gtk_label_new_with_mnemonic(_("_Z uncertainty:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.zunc = gtk_adjustment_new(args->zunc/pow10(args->zuexponent),
                                        -10000, 10000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zunc), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    unit = gwy_data_field_get_si_unit_z(dfield);
    controls.zuexponent
        = gwy_combo_box_metric_unit_new(G_CALLBACK(zuexponent_changed_cb),
                                        &controls, -15, 6, unit,
                                        args->zuexponent);
    gtk_table_attach(GTK_TABLE(table), controls.zuexponent, 2, 3, row, row+1,
                     GTK_EXPAND | GTK_FILL , 0, 0, 0);

    row++;

    label = gtk_label_new_with_mnemonic(_("_X correction factor:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.xmult = gtk_adjustment_new(args->xmult,
                                        0, 1000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xmult), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;
    label = gtk_label_new_with_mnemonic(_("_Y correction factor:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.ymult = gtk_adjustment_new(args->ymult,
                                        0, 1000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.ymult), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;
    label = gtk_label_new_with_mnemonic(_("_Z correction factor:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.zmult = gtk_adjustment_new(args->zmult,
                                        0, 1000, 1, 10, 0);
    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zmult), 1, 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_table_attach(GTK_TABLE(table), spin,
                     1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new_with_mnemonic(_("Calibration name:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);

    controls.name = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_text(controls.name, args->name);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(controls.name),
                     1, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);





    g_signal_connect(controls.xrange_from, "value-changed",
                                              G_CALLBACK(xfrom_changed_cb), &controls);
    g_signal_connect(controls.xrange_to, "value-changed",
                                              G_CALLBACK(xto_changed_cb), &controls);
    g_signal_connect(controls.yrange_from, "value-changed",
                                              G_CALLBACK(yfrom_changed_cb), &controls);
    g_signal_connect(controls.yrange_to, "value-changed",
                                              G_CALLBACK(yto_changed_cb), &controls);
    g_signal_connect(controls.zrange_from, "value-changed",
                                              G_CALLBACK(zfrom_changed_cb), &controls);
    g_signal_connect(controls.zrange_to, "value-changed",
                                              G_CALLBACK(zto_changed_cb), &controls);
    g_signal_connect(controls.xunc, "value-changed",
                                              G_CALLBACK(xunc_changed_cb), &controls);
    g_signal_connect(controls.yunc, "value-changed",
                                              G_CALLBACK(yunc_changed_cb), &controls);
    g_signal_connect(controls.zunc, "value-changed",
                                              G_CALLBACK(zunc_changed_cb), &controls);
    g_signal_connect(controls.xmult, "value-changed",
                                              G_CALLBACK(xmult_changed_cb), &controls);
    g_signal_connect(controls.ymult, "value-changed",
                                              G_CALLBACK(ymult_changed_cb), &controls);
    g_signal_connect(controls.zmult, "value-changed",
                                              G_CALLBACK(zmult_changed_cb), &controls);


    g_signal_connect(controls.xyunits, "clicked", 
                     G_CALLBACK(units_change_cb), &controls);

    g_signal_connect(controls.zunits, "clicked", 
                     G_CALLBACK(units_change_cb), &controls);


    controls.in_update = FALSE;

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
            /*check whether this resource already exists*/
            args->name = g_strdup(gtk_entry_get_text(controls.name));
            if (gwy_inventory_get_item(gwy_calibrations(), args->name))
            {
                dialog2 = gtk_message_dialog_new (GTK_WINDOW(dialog),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                                 GTK_MESSAGE_WARNING,
                                                 GTK_BUTTONS_CANCEL,
                                                 "Calibration '%s' already exists",
                                                 args->name);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), "Overwrite", RESPONSE_DUPLICATE_OVERWRITE);
                gtk_dialog_add_button(GTK_DIALOG(dialog2), "Append", RESPONSE_DUPLICATE_APPEND);
                response = gtk_dialog_run(GTK_DIALOG(dialog2));
                if (response == RESPONSE_DUPLICATE_OVERWRITE) {
                    args->duplicate = DUPLICATE_OVERWRITE;
                    response = GTK_RESPONSE_OK;
                } else if (response == RESPONSE_DUPLICATE_APPEND) {
                    args->duplicate = DUPLICATE_APPEND;
                    response = GTK_RESPONSE_OK;
                }
                gtk_widget_destroy (dialog2);
            } else args->duplicate = DUPLICATE_NONE;
            break;

            case RESPONSE_RESET:
            *args = cnew_defaults;
            cnew_dialog_update(&controls, args);
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
xyexponent_changed_cb(GtkWidget *combo,
                      CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xyexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->xrange_from = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xrange_from))
                  * pow10(args->xyexponent);
    args->xrange_to = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xrange_to))
                  * pow10(args->xyexponent);
    args->yrange_from = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yrange_from))
                  * pow10(args->xyexponent);
    args->yrange_to = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yrange_to))
                  * pow10(args->xyexponent);

    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
xyuexponent_changed_cb(GtkWidget *combo,
                      CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xyuexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->xunc = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->xunc))
                  * pow10(args->xyuexponent);
    args->yunc = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->yunc))
                  * pow10(args->xyuexponent);

    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;
}


static void
zexponent_changed_cb(GtkWidget *combo,
                      CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->zrange_from = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zrange_from))
                  * pow10(args->zexponent);
    args->zrange_to = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zrange_to))
                  * pow10(args->zexponent);


    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;
}


static void
zuexponent_changed_cb(GtkWidget *combo,
                      CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zuexponent = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    args->zunc = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->zunc))
                  * pow10(args->zuexponent);

    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
cnew_dialog_update(CNewControls *controls,
                        CNewArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xrange_from),
                             args->xrange_from/pow10(args->xyexponent));

}

static void
units_change_cb(GtkWidget *button,
                CNewControls *controls)
{
    GtkWidget *dialog, *hbox, *label, *entry;
    const gchar *id, *unit;
    gint response;
    CNewArgs *args = controls->args;

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
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return;
    }

    unit = gtk_entry_get_text(GTK_ENTRY(entry));

    if (gwy_strequal(id, "xy")) {
        set_combo_from_unit(controls->xyexponent, unit, 0);
        set_combo_from_unit(controls->xyuexponent, unit, 0);
     }
    else if (gwy_strequal(id, "z")) {
        set_combo_from_unit(controls->zexponent, unit, 0);
    }

    gtk_widget_destroy(dialog);

    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;
}


static void
set_combo_from_unit(GtkWidget *combo,
                    const gchar *str,
                    gint basepower)
{
    GwySIUnit *unit;
    gint power10;

    unit = gwy_si_unit_new_parse(str, &power10);
    power10 += basepower;
    gwy_combo_box_metric_unit_set_unit(GTK_COMBO_BOX(combo),
                                       power10 - 6, power10 + 6, unit);
    g_object_unref(unit);
}

static void
xfrom_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xrange_from = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}
static void
xto_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xrange_to = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
yfrom_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yrange_from = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}
static void
yto_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yrange_to = gtk_adjustment_get_value(adj) * pow10(args->xyexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
zfrom_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zrange_from = gtk_adjustment_get_value(adj) * pow10(args->zexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}
static void
zto_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zrange_to = gtk_adjustment_get_value(adj) * pow10(args->zexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
xunc_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xunc = gtk_adjustment_get_value(adj) * pow10(args->xyuexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
yunc_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yunc = gtk_adjustment_get_value(adj) * pow10(args->xyuexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
zunc_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;

    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zunc = gtk_adjustment_get_value(adj) * pow10(args->zuexponent);
    cnew_dialog_update(controls, args);
    controls->in_update = FALSE;

}

static void
xmult_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;
    args->xmult = gtk_adjustment_get_value(adj);
}
static void
ymult_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;
    args->ymult = gtk_adjustment_get_value(adj);
}
static void
zmult_changed_cb(GtkAdjustment *adj,
                 CNewControls *controls)
{
    CNewArgs *args = controls->args;
    args->zmult = gtk_adjustment_get_value(adj);
}


static const gchar xrange_from_key[]    = "/module/cnew/xrange_from";
static const gchar xrange_to_key[]    = "/module/cnew/xrange_to";
static const gchar yrange_from_key[]    = "/module/cnew/yrange_from";
static const gchar yrange_to_key[]    = "/module/cnew/yrange_to";
static const gchar zrange_from_key[]    = "/module/cnew/zrange_from";
static const gchar zrange_to_key[]    = "/module/cnew/zrange_to";

static void
cnew_sanitize_args(CNewArgs *args)
{
    args->xrange_from = CLAMP(args->xrange_from, -1e7, 1e7);
    args->xrange_to = CLAMP(args->xrange_to, -1e7, 1e7);
    args->yrange_from = CLAMP(args->yrange_from, -1e7, 1e7);
    args->yrange_to = CLAMP(args->yrange_to, -1e7, 1e7);
    args->zrange_from = CLAMP(args->zrange_from, -1e7, 1e7);
    args->zrange_to = CLAMP(args->zrange_to, -1e7, 1e7);
}

static void
cnew_load_args(GwyContainer *container,
              CNewArgs *args)
{
    *args = cnew_defaults;

    gwy_container_gis_double_by_name(container, xrange_from_key, &args->xrange_from);
    gwy_container_gis_double_by_name(container, xrange_to_key, &args->xrange_to);
    gwy_container_gis_double_by_name(container, yrange_from_key, &args->yrange_from);
    gwy_container_gis_double_by_name(container, yrange_to_key, &args->yrange_to);
    gwy_container_gis_double_by_name(container, zrange_from_key, &args->zrange_from);
    gwy_container_gis_double_by_name(container, zrange_to_key, &args->zrange_to);

    cnew_sanitize_args(args);
}

static void
cnew_save_args(GwyContainer *container,
              CNewArgs *args)
{
    gwy_container_set_double_by_name(container, xrange_from_key, args->xrange_from);
    gwy_container_set_double_by_name(container, xrange_to_key, args->xrange_to);
    gwy_container_set_double_by_name(container, yrange_from_key, args->yrange_from);
    gwy_container_set_double_by_name(container, yrange_to_key, args->yrange_to);
    gwy_container_set_double_by_name(container, zrange_from_key, args->zrange_from);
    gwy_container_set_double_by_name(container, zrange_to_key, args->zrange_to);

}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
