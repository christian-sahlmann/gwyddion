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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

//TODO: use gwy_math_refine_maximum


#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocess.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define CROSSCOR_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_CROSSCOR_ALL,
    GWY_CROSSCOR_ABS,
    GWY_CROSSCOR_X,
    GWY_CROSSCOR_Y,
    GWY_CROSSCOR_DIR,
    GWY_CROSSCOR_SCORE,
    GWY_CROSSCOR_LAST
} CrosscorResult;

typedef struct {
    CrosscorResult result;
    gint search_x;
    gint search_y;
    gint search_xoffset;
    gint search_yoffset;
    gint window_x;
    gint window_y;
    gint order;
    gdouble rot_pos;
    gdouble rot_neg;
    gboolean add_ls_mask;
    gdouble threshold;
    gboolean multiple;
    gboolean extend;
    gboolean correct;
    GwyAppDataId op1;
    GwyAppDataId op2;
    GwyAppDataId op3;
    GwyAppDataId op4;

} CrosscorArgs;

typedef struct {
    CrosscorArgs *args;
    GtkWidget *result;
    GtkObject *search_area_x;
    GtkObject *search_area_y;
    GtkObject *search_area_xoffset;
    GtkObject *search_area_yoffset;
    GtkObject *window_area_x;
    GtkObject *window_area_y;
    GtkObject *order;
    GtkObject *rotation_neg;
    GtkObject *rotation_pos;
    GtkWidget *add_ls_mask;
    GtkObject *threshold;
    GtkWidget *multiple;
    GtkWidget *extend;
    GtkWidget *correct;
    GtkWidget *chooser_op2;
    GtkWidget *chooser_op3;
    GtkWidget *chooser_op4;
} CrosscorControls;

static gboolean module_register         (void);
static void     crosscor                (GwyContainer *data,
                                         GwyRunType run);
static gboolean crosscor_dialog         (CrosscorArgs *args);
static void     crosscor_operation_cb   (GtkWidget *combo,
                                         CrosscorArgs *args);
static void     crosscor_data_changed   (GwyDataChooser *chooser,
                                         GwyAppDataId *object);
static gboolean crosscor_data_filter    (GwyContainer *data,
                                         gint id,
                                         gpointer user_data);
static gboolean crosscor_weaker_filter  (GwyContainer *data,
                                         gint id,
                                         gpointer user_data);
static void     guess_offsets           (CrosscorControls *controls);
static void     crosscor_update_values  (CrosscorControls *controls,
                                         CrosscorArgs *args);
static gboolean crosscor_do             (CrosscorArgs *args);
static void     crosscor_load_args      (GwyContainer *settings,
                                         CrosscorArgs *args);
static void     crosscor_save_args      (GwyContainer *settings,
                                         CrosscorArgs *args);
static void     crosscor_sanitize_args  (CrosscorArgs *args);
static void     mask_changed_cb         (GtkToggleButton *button,
                                         CrosscorControls *controls);
static void     multiple_changed_cb     (GtkToggleButton *button,
                                         CrosscorControls *controls);
static void     extend_changed_cb       (GtkToggleButton *button,
                                         CrosscorControls *controls);
static void     correct_changed_cb      (GtkToggleButton *button,
                                         CrosscorControls *controls);
static void     crosscor_update_areas_cb(GtkObject *adj,
                                         CrosscorControls *controls);

static const CrosscorArgs crosscor_defaults = {
    GWY_CROSSCOR_ABS, 10, 10, 0, 0, 25, 25, 0, 0.0, 0.0, 1, 0.95, 0, TRUE, TRUE,
    GWY_APP_DATA_ID_NONE,
    GWY_APP_DATA_ID_NONE,
    GWY_APP_DATA_ID_NONE,
    GWY_APP_DATA_ID_NONE,
};

static GwyAppDataId op2_id = GWY_APP_DATA_ID_NONE;
static GwyAppDataId op3_id = GWY_APP_DATA_ID_NONE;
static GwyAppDataId op4_id = GWY_APP_DATA_ID_NONE;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates cross-correlation of two data fields."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.8",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("crosscor",
                              (GwyProcessFunc)&crosscor,
                              N_("/M_ultidata/_Cross-Correlation..."),
                              NULL,
                              CROSSCOR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Cross-correlate two data fields"));

    return TRUE;
}

static void
crosscor(G_GNUC_UNUSED GwyContainer *data, GwyRunType run)
{
    CrosscorArgs args;
    GwyContainer *settings;
    gboolean dorun;

    g_return_if_fail(run & CROSSCOR_RUN_MODES);

    settings = gwy_app_settings_get();
    crosscor_load_args(settings, &args);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.op1.id,
                                     GWY_APP_CONTAINER_ID, &args.op1.datano,
                                     0);

    dorun = crosscor_dialog(&args);
    crosscor_save_args(settings, &args);

    if (dorun)
        crosscor_do(&args);
}

static gboolean
crosscor_dialog(CrosscorArgs *args)
{
    CrosscorControls controls;
    GtkWidget *dialog, *table, *label, *combo, *button;
    GwyDataChooser *chooser;
    gint row, response;
    gboolean ok = FALSE;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Cross-Correlation"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /* Correlate with */
    controls.chooser_op2 = gwy_data_chooser_new_channels();
    chooser = GWY_DATA_CHOOSER(controls.chooser_op2);
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_active_id(chooser, &args->op2);
    gwy_data_chooser_set_filter(chooser,
                                crosscor_data_filter, &args->op1, NULL);
    g_signal_connect(chooser, "changed",
                     G_CALLBACK(crosscor_data_changed), &args->op2);
    crosscor_data_changed(chooser, &args->op2);
    gwy_table_attach_hscale(table, row, _("Co_rrelate with:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Search size */
    label = gtk_label_new(_("Search size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.search_area_x = gtk_adjustment_new(args->search_x,
                                                0.0, 200.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Width:"), "px",
                            controls.search_area_x, 0);

    g_signal_connect(controls.search_area_x, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;

    controls.search_area_y = gtk_adjustment_new(args->search_y,
                                                0.0, 200.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("H_eight:"), "px",
                            controls.search_area_y, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    g_signal_connect(controls.search_area_y, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;

    label = gtk_label_new(_("2nd channel global offset"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);

    button = gtk_button_new_with_mnemonic(_("_Guess"));
    gtk_table_attach(GTK_TABLE(table), button, 2, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                        G_CALLBACK(guess_offsets), &controls);


    row++;


    controls.search_area_xoffset = gtk_adjustment_new(args->search_xoffset,
                                                -200.0, 200.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_x offset:"), "px",
                            controls.search_area_xoffset, 0);

    g_signal_connect(controls.search_area_xoffset, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;

    controls.search_area_yoffset = gtk_adjustment_new(args->search_yoffset,
                                                -200.0, 200.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_y offset:"), "px",
                            controls.search_area_yoffset, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    g_signal_connect(controls.search_area_yoffset, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;


    /* Window size */
    label = gtk_label_new(_("Window size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.window_area_x = gtk_adjustment_new(args->window_x,
                                                0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("W_idth:"), "px",
                            controls.window_area_x, 0);
    g_signal_connect(controls.window_area_x, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);

    row++;

    controls.window_area_y = gtk_adjustment_new(args->window_y,
                                                0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("Hei_ght:"), "px",
                            controls.window_area_y, 0);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    g_signal_connect(controls.window_area_y, "value-changed",
                     G_CALLBACK(crosscor_update_areas_cb),
                     &controls);
    row++;

    /* Result */
    combo = gwy_enum_combo_box_newl(G_CALLBACK(crosscor_operation_cb), args,
                                    args->result,
                                    _("All"), GWY_CROSSCOR_ALL,
                                    _("Absolute"), GWY_CROSSCOR_ABS,
                                    _("X Distance"), GWY_CROSSCOR_X,
                                    _("Y Distance"), GWY_CROSSCOR_Y,
                                    _("Angle"), GWY_CROSSCOR_DIR,
                                    _("Score"), GWY_CROSSCOR_SCORE,
                                    NULL);
    gwy_table_attach_hscale(table, row, _("Output _type:"), NULL,
                            GTK_OBJECT(combo), GWY_HSCALE_WIDGET);
    row++;

    /* Do mask of thresholds */
    controls.add_ls_mask = gtk_check_button_new_with_mnemonic
                                           (_("Add _low score results mask"));
    gtk_table_attach(GTK_TABLE(table), controls.add_ls_mask, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.add_ls_mask),
                                 args->add_ls_mask);
    g_signal_connect(controls.add_ls_mask, "toggled",
                     G_CALLBACK(mask_changed_cb), &controls);
    row++;

    controls.threshold = gtk_adjustment_new(args->threshold,
                                            -1, 1, 0.005, 0.05, 0);
    gwy_table_attach_hscale(table, row, _("T_hreshold:"), NULL,
                            controls.threshold, 0);
    gwy_table_hscale_set_sensitive(controls.threshold, args->add_ls_mask);
    row++;

    /* Allow multiple channel cross-correlation */
    controls.multiple = gtk_check_button_new_with_mnemonic
                                           (_("Multichannel cross-corelation"));
    gtk_table_attach(GTK_TABLE(table), controls.multiple, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.multiple),
                                 args->multiple);
    g_signal_connect(controls.multiple, "toggled",
                     G_CALLBACK(multiple_changed_cb), &controls);
    row++;

    /* Second set to correlate with: source */
    controls.chooser_op3 = gwy_data_chooser_new_channels();
    chooser = GWY_DATA_CHOOSER(controls.chooser_op3);
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_active_id(chooser, &args->op3);
    gwy_data_chooser_set_filter(chooser,
                                crosscor_weaker_filter, &args->op1, NULL);
    g_signal_connect(controls.chooser_op3, "changed",
                     G_CALLBACK(crosscor_data_changed), &args->op3);
    crosscor_data_changed(chooser, &args->op3);
    gwy_table_attach_hscale(table, row, _("Second _source data:"), NULL,
                            GTK_OBJECT(controls.chooser_op3), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    gtk_widget_set_sensitive(controls.chooser_op3, args->multiple);

    row++;

    /* Second set to correlate with: second data */
    controls.chooser_op4 = gwy_data_chooser_new_channels();
    chooser = GWY_DATA_CHOOSER(controls.chooser_op4);
    g_object_set_data(G_OBJECT(controls.chooser_op4), "dialog", dialog);
    gwy_data_chooser_set_active_id(chooser, &args->op4);
    gwy_data_chooser_set_filter(chooser,
                                crosscor_weaker_filter, &args->op1, NULL);
    g_signal_connect(controls.chooser_op4, "changed",
                     G_CALLBACK(crosscor_data_changed), &args->op4);
    crosscor_data_changed(chooser, &args->op4);
    gwy_table_attach_hscale(table, row, _("Correlate with:"), NULL,
                            GTK_OBJECT(controls.chooser_op4), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    gtk_widget_set_sensitive(controls.chooser_op4, args->multiple);
    row++;

    /*postprocessing*/
    label = gtk_label_new(_("Postprocess:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.extend = gtk_check_button_new_with_mnemonic
                                           (_("Extend results to borders"));
    gtk_table_attach(GTK_TABLE(table), controls.extend, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.extend),
                                 args->extend);
    g_signal_connect(controls.extend, "toggled",
                     G_CALLBACK(extend_changed_cb), &controls);
    row++;

    controls.correct = gtk_check_button_new_with_mnemonic
                                           (_("Create corrected data from 2nd channel"));
    gtk_table_attach(GTK_TABLE(table), controls.correct, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.correct),
                                 args->correct);
    g_signal_connect(controls.correct, "toggled",
                     G_CALLBACK(correct_changed_cb), &controls);
    row++;


    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            crosscor_update_values(&controls, args);
            case GTK_RESPONSE_NONE:
            gtk_widget_destroy(dialog);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            crosscor_update_values(&controls, args);
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
crosscor_data_changed(GwyDataChooser *chooser,
                      GwyAppDataId *object)
{
    GtkWidget *dialog;

    gwy_data_chooser_get_active_id(chooser, object);
    gwy_debug("data: %d %d", object->datano, object->id);

    dialog = g_object_get_data(G_OBJECT(chooser), "dialog");
    g_assert(GTK_IS_DIALOG(dialog));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      object->datano);
}

static void
crosscor_operation_cb(GtkWidget *combo,
                      CrosscorArgs *args)
{
    args->result = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
}

static void
mask_changed_cb(GtkToggleButton *button, CrosscorControls *controls)
{
    controls->args->add_ls_mask = gtk_toggle_button_get_active(button);
    gwy_table_hscale_set_sensitive(controls->threshold,
                                   controls->args->add_ls_mask);
}
static void
extend_changed_cb(GtkToggleButton *button, CrosscorControls *controls)
{
    controls->args->extend = gtk_toggle_button_get_active(button);
}
static void
correct_changed_cb(GtkToggleButton *button, CrosscorControls *controls)
{
    controls->args->correct = gtk_toggle_button_get_active(button);
}
static void
multiple_changed_cb(GtkToggleButton *button, CrosscorControls *controls)
{
    controls->args->multiple = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(controls->chooser_op3,
                                   controls->args->multiple);
    gtk_widget_set_sensitive(controls->chooser_op4,
                                   controls->args->multiple);
}

static gboolean
crosscor_data_filter(GwyContainer *data,
                     gint id,
                     gpointer user_data)
{
    GwyAppDataId *object = (GwyAppDataId*)user_data;
    GwyDataField *op1, *op2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = gwy_app_data_browser_get(object->datano);
    quark = gwy_app_get_data_key_for_id(object->id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    /* It does not make sense to crosscorrelate with itself */
    if (op1 == op2)
        return FALSE;

    return !gwy_data_field_check_compatibility(op1, op2,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE);
}

static gboolean
crosscor_weaker_filter(GwyContainer *data,
                       gint id,
                       gpointer user_data)
{
    GwyAppDataId *object = (GwyAppDataId*)user_data;
    GwyDataField *op1, *op2;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    op1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = gwy_app_data_browser_get(object->datano);
    quark = gwy_app_get_data_key_for_id(object->id);
    op2 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    return !gwy_data_field_check_compatibility(op1, op2,
                                               GWY_DATA_COMPATIBILITY_RES
                                               | GWY_DATA_COMPATIBILITY_REAL
                                               | GWY_DATA_COMPATIBILITY_LATERAL);
}

static GwyDataField*
abs_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gdouble *data, *rdata;
    const gdouble *d2;
    gint i, n;
    GwyDataField *result = gwy_data_field_new_alike(dfieldx, TRUE);


    data = gwy_data_field_get_data(dfieldx);
    d2 = gwy_data_field_get_data_const(dfieldy);
    rdata = gwy_data_field_get_data(result);
    n = gwy_data_field_get_xres(dfieldx)*gwy_data_field_get_yres(dfieldx);

    for (i = 0; i < n; i++)
        rdata[i] = hypot(data[i], d2[i]);

    return result;
}

static GwyDataField*
dir_field(GwyDataField *dfieldx, GwyDataField *dfieldy)
{
    gdouble *data, *rdata;
    const gdouble *d2;
    gint i, n;
    GwyDataField *result = gwy_data_field_new_alike(dfieldx, TRUE);

    data = gwy_data_field_get_data(dfieldx);
    rdata = gwy_data_field_get_data(result);
    d2 = gwy_data_field_get_data_const(dfieldy);
    n = gwy_data_field_get_xres(dfieldx)*gwy_data_field_get_yres(dfieldx);

    for (i = 0; i < n; i++)
        rdata[i] = atan2(d2[i], data[i]);

    return result;
}

static void
crosscor_update_areas_cb(G_GNUC_UNUSED GtkObject *adj,
                         CrosscorControls *controls)
{
    static gboolean in_update = FALSE;
    if (in_update)  return;

    in_update = TRUE;

    controls->args->search_x = gwy_adjustment_get_int(controls->search_area_x);
    controls->args->search_y = gwy_adjustment_get_int(controls->search_area_y);

    controls->args->search_xoffset = gwy_adjustment_get_int(controls->search_area_xoffset);
    controls->args->search_yoffset = gwy_adjustment_get_int(controls->search_area_yoffset);


    controls->args->window_x = gwy_adjustment_get_int(controls->window_area_x);
    controls->args->window_y = gwy_adjustment_get_int(controls->window_area_y);

    if (controls->args->search_x<controls->args->window_x) {
        controls->args->search_x = controls->args->window_x;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->search_area_x),
                                 controls->args->search_x);
    }
    if (controls->args->search_y<controls->args->window_y) {
        controls->args->search_y = controls->args->window_y;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->search_area_y),
                                 controls->args->search_y);
    }

    in_update = FALSE;
}

static void
crosscor_update_values(CrosscorControls *controls,
                       CrosscorArgs *args)
{
    args->search_x = gwy_adjustment_get_int(controls->search_area_x);
    args->search_y = gwy_adjustment_get_int(controls->search_area_y);

    args->window_x = gwy_adjustment_get_int(controls->window_area_x);
    args->window_y = gwy_adjustment_get_int(controls->window_area_y);

    args->threshold =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold));
    args->add_ls_mask =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->add_ls_mask));
}

static void
add_mask(GwyDataField *score, GwyContainer *data, gdouble threshold, gint id)
{
    GQuark quark = gwy_app_get_mask_key_for_id(id);

    score = gwy_data_field_duplicate(score);
    gwy_data_field_threshold(score, threshold, 1.0, 0.0);
    gwy_container_set_object(data, quark, score);
    g_object_unref(score);
}

static void     
guess_offsets(CrosscorControls *controls)
{
    GwyContainer *data;
    CrosscorArgs *args = controls->args;
    GwyDataField *dfield1, *dfield2, *kernel, *score;
    GQuark quark;
    gint col, row, xres, yres, xoffset=0, yoffset=0;
    gint border;
    gdouble *scoredata, maxscore;
//    gint newid;


    data = gwy_app_data_browser_get(args->op1.datano);
    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = gwy_app_data_browser_get(args->op2.datano);
    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);

    border = CLAMP(xres/5, 10, 100); //should be more clever

    kernel = gwy_data_field_duplicate(dfield2);
    gwy_data_field_resize(kernel, border, border, 
             gwy_data_field_get_xres(dfield2)-border, gwy_data_field_get_yres(dfield2)-border);

    score = gwy_data_field_new_alike(dfield1, FALSE);

    gwy_data_field_correlate(dfield1, kernel,
                         score, GWY_CORRELATION_POC);
    scoredata = gwy_data_field_get_data(score);

    maxscore = G_MINDOUBLE;
    for (col=(2*border); col<(xres-2*border); col++) {
       for (row=(2*border); row<(yres-2*border); row++) {
           if (scoredata[col + xres*row]>maxscore) {
              xoffset = col-xres/2;
              yoffset = row-yres/2;
              maxscore = scoredata[col + xres*row];
           }
       }
    }
    //printf("result offset %d %d  border %d\n", xoffset, yoffset, border);

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->search_area_xoffset), xoffset);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->search_area_yoffset), yoffset);

//    newid = gwy_app_data_browser_add_data_field(score, data, TRUE);


    g_object_unref(kernel);
    g_object_unref(score);
}

static gboolean
crosscor_do(CrosscorArgs * args)
{
    GwyContainer *data;
    GwyDataField *dfieldx, *dfieldy, *dfield1, *dfield2, *dfield2b = NULL, *dfield4b = NULL,
                 *dfield3 = NULL, *dfield4 = NULL, *score, *buffer, *mask, *dir = NULL,
                 *abs = NULL, *corrected;
    GwyDataField *dfieldx2 = NULL, *dfieldy2 = NULL, *score2 = NULL;
    GwyXY *coords;

    gint newid, xres, yres, i, col, row;
    gdouble error, *xdata, *ydata;
    GwyComputationState *state;
    GwySIUnit *siunit;
    GQuark quark;

    data = gwy_app_data_browser_get(args->op1.datano);
    quark = gwy_app_get_data_key_for_id(args->op1.id);
    dfield1 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    data = gwy_app_data_browser_get(args->op2.datano);
    quark = gwy_app_get_data_key_for_id(args->op2.id);
    dfield2 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    /* result fields - after computation result should be at dfieldx */
    dfieldx = gwy_data_field_new_alike(dfield1, FALSE);
    dfieldy = gwy_data_field_new_alike(dfield1, FALSE);
    score = gwy_data_field_new_alike(dfield1, FALSE);

    xres = gwy_data_field_get_xres(dfield1);
    yres = gwy_data_field_get_yres(dfield1);


    if (args->multiple) {
        data = gwy_app_data_browser_get(args->op3.datano);
        quark = gwy_app_get_data_key_for_id(args->op3.id);
        dfield3 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

        data = gwy_app_data_browser_get(args->op4.datano);
        quark = gwy_app_get_data_key_for_id(args->op4.id);
        dfield4 = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

        dfieldx2 = gwy_data_field_new_alike(dfield1, FALSE);
        dfieldy2 = gwy_data_field_new_alike(dfield1, FALSE);
        score2 = gwy_data_field_new_alike(dfield1, FALSE);
    }

    /* FIXME */
    data = gwy_app_data_browser_get(args->op1.datano);
    gwy_app_wait_start(gwy_app_find_window_for_channel(data, args->op1.id),
                       _("Initializing..."));

    /*if a shift is requested, make a copy and shift it*/
    dfield2b = NULL;
    dfield4b = NULL;
    if (args->search_xoffset!=0 || args->search_yoffset!=0) {
        //printf("offset %d %d\n", args->search_xoffset, args->search_yoffset);

        dfield2b = gwy_data_field_new_alike(dfield1, FALSE);
        gwy_data_field_fill(dfield2b, gwy_data_field_get_avg(dfield2));

        gwy_data_field_area_copy(dfield2, dfield2b,
                          0, 0,
                          -1, -1,
                          args->search_xoffset, args->search_yoffset);

        dfield2 = dfield2b;

        if (args->multiple) {
            dfield4b = gwy_data_field_new_alike(dfield1, FALSE);
            gwy_data_field_fill(dfield4b, gwy_data_field_get_avg(dfield4));

            gwy_data_field_area_copy(dfield4, dfield4b,
                          0, 0,
                          -1, -1,
                          args->search_xoffset, args->search_yoffset);

            dfield4 = dfield4b;
        }

    }


    /* compute crosscorelation */
    state = gwy_data_field_crosscorrelate_init(dfield1, dfield2,
                                               dfieldx, dfieldy, score,
                                               args->search_x, args->search_y,
                                               args->window_x, args->window_y);
    gwy_app_wait_set_message(_("Correlating first set..."));
    do {
        gwy_data_field_crosscorrelate_iteration(state);
        if (!gwy_app_wait_set_fraction(state->fraction)) {
            gwy_data_field_crosscorrelate_finalize(state);
            gwy_app_wait_finish();
            g_object_unref(dfieldx);
            g_object_unref(dfieldy);
            g_object_unref(score);
            return FALSE;
        }
    } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
    gwy_data_field_crosscorrelate_finalize(state);
    gwy_app_wait_finish();

    /* compute crosscorelation of second set if it is there */
    if (args->multiple) {
        gwy_app_wait_start(gwy_app_find_window_for_channel(data, args->op1.id),
                           _("Initializing..."));

        state = gwy_data_field_crosscorrelate_init(dfield3, dfield4,
                                                   dfieldx2, dfieldy2, score2,
                                                   args->search_x,
                                                   args->search_y,
                                                   args->window_x,
                                                   args->window_y);
        gwy_app_wait_set_message(_("Correlating second set..."));
        do {
            gwy_data_field_crosscorrelate_iteration(state);
            if (!gwy_app_wait_set_fraction(state->fraction)) {
                gwy_data_field_crosscorrelate_finalize(state);
                gwy_app_wait_finish();
                g_object_unref(dfieldx2);
                g_object_unref(dfieldy2);
                g_object_unref(score2);
                return FALSE;
            }
        } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
        gwy_data_field_crosscorrelate_finalize(state);
        gwy_app_wait_finish();

        gwy_data_field_sum_fields(dfieldx, dfieldx, dfieldx2);
        gwy_data_field_sum_fields(dfieldy, dfieldy, dfieldy2);
        gwy_data_field_sum_fields(score, score, score2);

        gwy_data_field_multiply(dfieldx, 0.5);
        gwy_data_field_multiply(dfieldy, 0.5);
        gwy_data_field_multiply(score, 0.5);
    }

    if (args->search_xoffset!=0 || args->search_yoffset!=0) {
        gwy_data_field_add(dfieldx, gwy_data_field_itor(dfieldx, args->search_xoffset));
        gwy_data_field_add(dfieldy, gwy_data_field_jtor(dfieldy, args->search_yoffset));
    }

    if (args->extend) {
       
        buffer = gwy_data_field_new_alike(dfieldx, FALSE);
        mask = gwy_data_field_new_alike(dfieldx, TRUE);

        gwy_data_field_area_fill(mask, 0, 0, args->search_x/2 + 2, yres, 1);
        gwy_data_field_area_fill(mask, 0, 0, xres, args->search_y/2 + 2, 1);
        gwy_data_field_area_fill(mask, xres - args->search_x/2 - 2, 0, args->search_x/2 + 2, yres, 1);
        gwy_data_field_area_fill(mask, 0, yres - args->search_y/2 - 2, xres, args->search_y/2 + 2, 1);

        gwy_data_field_correct_average_unmasked(dfieldx, mask);
        gwy_data_field_correct_average_unmasked(dfieldy, mask);

        gwy_app_wait_start(gwy_app_find_window_for_channel(data, args->op1.id),
                           _("Borders extension..."));

        for (i=0; i<10000; i++) {
            gwy_data_field_correct_laplace_iteration(dfieldx,
                                mask,
                                buffer,
                                0.2,
                                &error);
            if ((i%1000)==0) if (!gwy_app_wait_set_fraction((gdouble)i/20000.0)) break;
           
        }
        for (i=0; i<10000; i++) {
            gwy_data_field_correct_laplace_iteration(dfieldy,
                                mask,
                                buffer,
                                0.2,
                                &error);
            if ((i%1000)==0) if (!gwy_app_wait_set_fraction((10000.0 + (gdouble)i)/20000.0)) break;
           
        }
         gwy_app_wait_finish();

        gwy_object_unref(buffer);
        gwy_object_unref(mask);
       
    }

    if (args->correct) {

       corrected = gwy_data_field_new_alike(dfield2, FALSE);
       coords = g_new(GwyXY, xres*yres);

       xdata = gwy_data_field_get_data(dfieldx);
       ydata = gwy_data_field_get_data(dfieldy);
       i = 0;
       for (row=0; row<yres; row++) 
       {
           for (col=0; col<xres; col++)
           {
               coords[i].x = col + gwy_data_field_rtoi(corrected, xdata[col + xres*row]);
               coords[i].y = row + gwy_data_field_rtoj(corrected, ydata[col + xres*row]);
               i++;
           }
       }

       gwy_data_field_sample_distorted (dfield2,
                                 corrected,
                                 coords,
                                 GWY_INTERPOLATION_BILINEAR,
                                 GWY_EXTERIOR_BORDER_EXTEND,
                                 0);
       
        newid = gwy_app_data_browser_add_data_field(corrected, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add_proc(data, args->op1.id, newid);

        gwy_app_set_data_field_title(data, newid, _("Corrected 2nd channel"));

        g_object_unref(corrected);
        g_free(coords); 
    }


    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_ABS)
        abs = abs_field(dfieldx, dfieldy);

    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_DIR)
        dir = dir_field(dfieldx, dfieldy);

    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_X) {
        siunit = gwy_data_field_get_si_unit_z(dfieldx);
        gwy_si_unit_set_from_string(siunit, NULL);

        newid = gwy_app_data_browser_add_data_field(dfieldx, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add_proc(data, args->op1.id, newid);
        if (args->add_ls_mask)
            add_mask(score, data, args->threshold, newid);

        gwy_app_set_data_field_title(data, newid, _("X difference"));
    }

    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_Y) {
        siunit = gwy_data_field_get_si_unit_z(dfieldy);
        gwy_si_unit_set_from_string(siunit, NULL);

        newid = gwy_app_data_browser_add_data_field(dfieldy, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add_proc(data, args->op1.id, newid);
        if (args->add_ls_mask)
            add_mask(score, data, args->threshold, newid);

        gwy_app_set_data_field_title(data, newid, _("Y difference"));
    }
    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_ABS) {
        siunit = gwy_data_field_get_si_unit_z(abs);
        gwy_si_unit_set_from_string(siunit, NULL);

        newid = gwy_app_data_browser_add_data_field(abs, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add_proc(data, args->op1.id, newid);
        if (args->add_ls_mask)
            add_mask(score, data, args->threshold, newid);

        gwy_app_set_data_field_title(data, newid, _("Absolute difference"));
    }
    if (args->result == GWY_CROSSCOR_ALL || args->result == GWY_CROSSCOR_DIR) {
        siunit = gwy_data_field_get_si_unit_z(dir);
        gwy_si_unit_set_from_string(siunit, NULL);

        newid = gwy_app_data_browser_add_data_field(dir, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add_proc(data, args->op1.id, newid);
        if (args->add_ls_mask)
            add_mask(score, data, args->threshold, newid);

        gwy_app_set_data_field_title(data, newid, _("Direction"));
    }
    if (args->result == GWY_CROSSCOR_ALL
        || args->result == GWY_CROSSCOR_SCORE) {
        siunit = gwy_data_field_get_si_unit_z(score);
        gwy_si_unit_set_from_string(siunit, NULL);

        newid = gwy_app_data_browser_add_data_field(score, data, TRUE);
        gwy_app_sync_data_items(data, data, args->op1.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_channel_log_add_proc(data, args->op1.id, newid);
        if (args->add_ls_mask)
            add_mask(score, data, args->threshold, newid);

        gwy_app_set_data_field_title(data, newid, _("Score"));
    }

    g_object_unref(score);
    g_object_unref(dfieldy);
    g_object_unref(dfieldx);
    if (dfield2b) g_object_unref(dfield2b);
    if (dfield4b) g_object_unref(dfield4b);
    gwy_object_unref(abs);
    gwy_object_unref(dir);

    return TRUE;
}

static const gchar add_ls_mask_key[]     = "/module/crosscor/add_ls_mask";
static const gchar multiple_key[]        = "/module/crosscor/multiple";
static const gchar result_key[]          = "/module/crosscor/result";
static const gchar rot_neg_key[]         = "/module/crosscor/rot_neg";
static const gchar rot_pos_key[]         = "/module/crosscor/rot_pos";
static const gchar search_x_key[]        = "/module/crosscor/search_x";
static const gchar search_y_key[]        = "/module/crosscor/search_y";
static const gchar search_xoffset_key[]  = "/module/crosscor/search_xoffset";
static const gchar search_yoffset_key[]  = "/module/crosscor/search_yoffset";
static const gchar threshold_key[]       = "/module/crosscor/threshold";
static const gchar window_x_key[]        = "/module/crosscor/window_x";
static const gchar window_y_key[]        = "/module/crosscor/window_y";
static const gchar extend_key[]          = "/module/crosscor/extend";
static const gchar correct_key[]         = "/module/crosscor/correct";

static void
crosscor_sanitize_args(CrosscorArgs *args)
{
    args->result = MIN(args->result, GWY_CROSSCOR_LAST-1);
    args->search_x = CLAMP(args->search_x, 0, 100);
    args->search_y = CLAMP(args->search_y, 0, 100);
    args->search_xoffset = CLAMP(args->search_xoffset, -200, 200);
    args->search_yoffset = CLAMP(args->search_yoffset, -200, 200);
    args->window_x = CLAMP(args->window_x, 0, 100);
    args->window_y = CLAMP(args->window_y, 0, 100);
    args->threshold = CLAMP(args->threshold, -1.0, 1.0);
    args->add_ls_mask = !!args->add_ls_mask;
    args->extend = !!args->extend;
    args->correct = !!args->correct;
    gwy_app_data_id_verify_channel(&args->op2);
    gwy_app_data_id_verify_channel(&args->op3);
    gwy_app_data_id_verify_channel(&args->op4);
}

static void
crosscor_load_args(GwyContainer *settings,
                   CrosscorArgs *args)
{
    *args = crosscor_defaults;
    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    gwy_container_gis_int32_by_name(settings, search_x_key, &args->search_x);
    gwy_container_gis_int32_by_name(settings, search_y_key, &args->search_y);
    gwy_container_gis_int32_by_name(settings, search_xoffset_key, &args->search_xoffset);
    gwy_container_gis_int32_by_name(settings, search_yoffset_key, &args->search_yoffset);
    gwy_container_gis_int32_by_name(settings, window_x_key, &args->window_x);
    gwy_container_gis_int32_by_name(settings, window_y_key, &args->window_y);
    gwy_container_gis_double_by_name(settings, threshold_key, &args->threshold);
    gwy_container_gis_boolean_by_name(settings, add_ls_mask_key,
                                      &args->add_ls_mask);
    gwy_container_gis_boolean_by_name(settings, extend_key, &args->extend);
    gwy_container_gis_boolean_by_name(settings, correct_key, &args->correct);
    gwy_container_gis_boolean_by_name(settings, multiple_key, &args->multiple);
    gwy_container_gis_double_by_name(settings, rot_pos_key, &args->rot_pos);
    gwy_container_gis_double_by_name(settings, rot_neg_key, &args->rot_neg);
    args->op2 = op2_id;
    args->op3 = op3_id;
    args->op4 = op4_id;
    crosscor_sanitize_args(args);
}

static void
crosscor_save_args(GwyContainer *settings,
                   CrosscorArgs *args)
{
    op2_id = args->op2;
    op3_id = args->op3;
    op4_id = args->op4;
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    gwy_container_set_int32_by_name(settings, search_x_key, args->search_x);
    gwy_container_set_int32_by_name(settings, search_y_key, args->search_y);
    gwy_container_set_int32_by_name(settings, search_xoffset_key, args->search_xoffset);
    gwy_container_set_int32_by_name(settings, search_yoffset_key, args->search_yoffset);
    gwy_container_set_int32_by_name(settings, window_x_key, args->window_x);
    gwy_container_set_int32_by_name(settings, window_y_key, args->window_y);
    gwy_container_set_double_by_name(settings, threshold_key, args->threshold);
    gwy_container_set_boolean_by_name(settings, add_ls_mask_key,
                                      args->add_ls_mask);
    gwy_container_set_boolean_by_name(settings, extend_key, args->extend);
    gwy_container_set_boolean_by_name(settings, correct_key, args->correct);
    gwy_container_set_boolean_by_name(settings, multiple_key, args->multiple);
    gwy_container_set_double_by_name(settings, rot_pos_key, args->rot_pos);
    gwy_container_set_double_by_name(settings, rot_neg_key, args->rot_neg);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

