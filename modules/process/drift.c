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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <stdio.h>
#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-process.h>
#include <libprocess/linestats.h>
#include <libprocess/stats.h>
#include <libprocess/correct.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <app/gwyapp.h>

#define DRIFT_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    PREVIEW_SIZE = 320
};


typedef enum {
    GWY_DRIFT_CORRELATION = 0
  /*  GWY_DRIFT_ISOTROPY    = 1*/
} GwyDriftMethod;

/* Data for this function. */
typedef struct {
    GwyDriftMethod method;
    gdouble averaging;
    gdouble smoothing;
    gboolean is_graph;
    gboolean is_crop;
    gboolean is_correct;
    GwyInterpolationType interpolation;
} DriftArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *is_graph;
    GtkWidget *is_correct;
    GtkWidget *is_crop;
    GtkWidget *message;
    GtkObject *averaging;
    GtkObject *smoothing;
    GtkWidget *interpolation;
    GtkWidget *method;
    GtkWidget *color_button;
    GwyContainer *mydata;
    GwyDataLine *result;
    gboolean computed;
    DriftArgs *args;
} DriftControls;

static gboolean    module_register            (void);
static void        drift                      (GwyContainer *data,
                                               GwyRunType run);
static void        drift_dialog                (DriftArgs *args,
                                               GwyContainer *data,
					                           GwyDataField *dfield,
                                               gint id);
static void        mask_color_change_cb       (GtkWidget *color_button,
                                               DriftControls *controls);
static void        load_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static GwyDataField* create_mask_field        (GwyDataField *dfield);

static void        drift_dialog_update_controls(DriftControls *controls,
                                               DriftArgs *args);
static void        drift_dialog_update_values  (DriftControls *controls,
                                               DriftArgs *args);
static void        drift_invalidate            (GObject *obj,
                                               DriftControls *controls);
static void        preview                    (DriftControls *controls,
                                               DriftArgs *args);
static void        reset                    (DriftControls *controls,
                                               DriftArgs *args);
static void        drift_ok                    (DriftControls *controls,
                                               DriftArgs *args, 
                                                GwyContainer *data,
                                                GwyDataField *data_field,
                                                gint id);
static void        drift_load_args              (GwyContainer *container,
                                               DriftArgs *args);
static void        drift_save_args              (GwyContainer *container,
                                               DriftArgs *args);
static void        drift_sanitize_args         (DriftArgs *args);
static void         mask_process               (GwyDataField *dfield,
                                                GwyDataField *maskfield,
                                                DriftArgs *args,
                                                DriftControls *controls,
                                                GtkWindow *wait_window);

static GwyDataField* gwy_data_field_correct_drift(GwyDataField *data_field,
                                                GwyDataField *corrected_field,
                                                GwyDataLine *drift,
                                                gboolean crop);

static gint        gwy_data_field_get_drift_from_correlation(GwyDataField *data_field,
                                                             GwyDataLine *drift_result,
                                                             gint skip_tolerance,
                                                             gint polynom_degree,
                                                             gdouble threshold);

static gdouble     gwy_data_field_get_row_correlation_score(GwyDataField *data_field,
                                                            gint line1_index,
                                                            gint line1_start,
                                                            gint line2_index,
                                                            gint line2_start,
                                                            gint area_length);


static const DriftArgs drift_defaults = {
    GWY_DRIFT_CORRELATION,
    50,
    50,
    TRUE,
    TRUE,
    TRUE,
    GWY_INTERPOLATION_BILINEAR,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluate/correct thermal drift in fast scan axis."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("drift",
                              (GwyProcessFunc)&drift,
                              N_("/_Correct Data/_Compensate drift..."),
                              NULL,
                              DRIFT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Evaluate/correct thermal drift in fast scan axis."));


    return TRUE;
}

static void
drift(GwyContainer *data, GwyRunType run)
{
    DriftArgs args;
    GwyDataField *dfield;
    gint id;


    g_return_if_fail(run & DRIFT_RUN_MODES);
    drift_load_args(gwy_app_settings_get(), &args);    
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    g_return_if_fail(dfield);

    drift_dialog(&args, data, dfield, id);
    drift_save_args(gwy_app_settings_get(), &args);
}


static void
drift_dialog(DriftArgs *args, 
             GwyContainer *data, 
             GwyDataField *dfield,
             gint id)
{
    GtkWidget *dialog, *table, *label, *hbox, *spin;
    DriftControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    gint yres, row;

    static const GwyEnum methods[] = {
        { N_("Line correlation"),  GWY_DRIFT_CORRELATION,  },
      /*  { N_("Local isotropy"),    GWY_DRIFT_ISOTROPY, },*/
    };

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Correct drift"), NULL, 0,
                                         _("_Update Result"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    zoomval = PREVIEW_SIZE/(gdouble)MAX(gwy_data_field_get_xres(dfield),
                                        gwy_data_field_get_yres(dfield));
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);


    yres = gwy_data_field_get_yres(dfield);
    controls.result
        = gwy_data_line_new(yres, gwy_data_field_get_yreal(dfield), TRUE);

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new_with_mnemonic(_("_Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                                GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls.method = gwy_enum_combo_box_new(methods, G_N_ELEMENTS(methods),
                                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                                 &args->method, args->method, TRUE);


    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.method);
    gtk_table_attach(GTK_TABLE(table), controls.method, 1, 2, row, row+1,
                                 GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;

    label = gtk_label_new_with_mnemonic(_("_Interpolation:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                                GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls.interpolation =
          gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interpolation, args->interpolation, TRUE);


    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.interpolation);
    gtk_table_attach(GTK_TABLE(table), controls.interpolation, 1, 2, row, row+1,
                                 GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;


    controls.averaging = gtk_adjustment_new(args->averaging, 1.0, 100.0, 1, 5, 0);
    spin = gwy_table_attach_hscale(table, row, "averaging number:", "rows", controls.averaging,
                                               GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.averaging, "value_changed",
                     G_CALLBACK(drift_invalidate), &controls);

    row++;

    controls.smoothing = gtk_adjustment_new(args->smoothing, 1.0, 10.0, 1, 2, 0);
    spin = gwy_table_attach_hscale(table, row, "smoothing degree:", "", controls.smoothing,
                                               GWY_HSCALE_DEFAULT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.smoothing, "value_changed",
                     G_CALLBACK(drift_invalidate), &controls);

    row++;

    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                                    TRUE);
    load_mask_color(controls.color_button,
                     gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    gwy_table_attach_hscale(table, row++, _("_Mask color:"), NULL,
                                    GTK_OBJECT(controls.color_button),
                                    GWY_HSCALE_WIDGET_NO_EXPAND);
    g_signal_connect(controls.color_button, "clicked",
                           G_CALLBACK(mask_color_change_cb), &controls);

    row++;

    controls.is_graph = gtk_check_button_new_with_mnemonic(_("_Extract drift graph"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_graph),
                                 args->is_graph);
    gtk_table_attach(GTK_TABLE(table), controls.is_graph,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.is_graph, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    controls.is_correct = gtk_check_button_new_with_mnemonic(_("_Correct data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_correct),
                                 args->is_correct);
    gtk_table_attach(GTK_TABLE(table), controls.is_correct,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.is_correct, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    controls.is_crop = gtk_check_button_new_with_mnemonic(_("_Crop out unknown data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_crop),
                                 args->is_crop);
    gtk_table_attach(GTK_TABLE(table), controls.is_crop,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.is_crop, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    label = gtk_label_new_with_mnemonic(_("Result:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
                                GTK_EXPAND | GTK_FILL, 0, 2, 2);

    row++;

    controls.message = gtk_label_new("Nothing computed");
    gtk_misc_set_alignment(GTK_MISC(controls.message), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.message,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.computed = FALSE;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_graph),
                                 args->is_graph);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_crop),
                                 args->is_crop);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_correct),
                                 args->is_correct);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            drift_dialog_update_values(&controls, args);
            gwy_object_unref(controls.mydata);
            gwy_object_unref(controls.result);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = drift_defaults;
            reset(&controls, args);
            drift_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            drift_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    drift_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);
    drift_ok(&controls, args, data, dfield, id);

    gwy_object_unref(controls.mydata);
    gwy_object_unref(controls.result);
}

static void
drift_dialog_update_controls(DriftControls *controls,
                            DriftArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->smoothing),
                             args->smoothing);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->averaging),
                             args->averaging);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_graph),
                                 args->is_graph);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_correct),
                                 args->is_correct);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_crop),
                                 args->is_crop);
}

static void
drift_dialog_update_values(DriftControls *controls,
                          DriftArgs *args)
{
    args->smoothing
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->smoothing));
    args->averaging
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->averaging));
    args->is_graph
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_graph));
    args->is_crop
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_crop));
    args->is_correct
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_correct));
}

static void
drift_invalidate(G_GNUC_UNUSED GObject *obj,
                DriftControls *controls)
{
    controls->computed = FALSE;
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     DriftControls *controls)
{
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    gwy_mask_color_selector_run(NULL, GTK_WINDOW(controls->dialog),
                                GWY_COLOR_BUTTON(color_button),
                                data, "/0/mask");
    load_mask_color(color_button, data);
}

static void
load_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    if (!gwy_rgba_get_from_container(&rgba, data, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, gwy_app_settings_get(), "/mask");
        gwy_rgba_store_to_container(&rgba, data, "/0/mask");
    }
    gwy_color_button_set_color(GWY_COLOR_BUTTON(color_button), &rgba);
}


static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}


static void
preview(DriftControls *controls,
        DriftArgs *args)
{
    GwyDataField *mask, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    /* Set up the mask */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask)) {
        mask = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);

        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }
    gwy_data_field_copy(dfield, mask, FALSE);


    mask_process(dfield, mask, args, controls, GDK_WINDOW(controls->dialog->window));
    controls->computed = TRUE;
}

static void
reset(DriftControls *controls,
      G_GNUC_UNUSED DriftArgs *args)
{
    GwyDataField *maskfield;

    if (gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                         &maskfield)) {
        gwy_data_field_clear(maskfield);
        gwy_data_field_data_changed(maskfield);
    }
    gtk_label_set_text(GTK_LABEL(controls->message), "Nothing computed");
    controls->computed = FALSE;
}

static void
drift_ok(DriftControls *controls,
         DriftArgs *args,
         GwyContainer *data,
         GwyDataField *data_field,
         gint id)
{

    GwyGraphCurveModel *cmodel;
    GwyGraphModel *gmodel;
    GwyDataField *newdata_field;
    gint newid;


    if (!controls->computed) {
        mask_process(data_field, NULL, args, controls,
                     GDK_WINDOW(
                          GTK_WIDGET(gwy_app_find_window_for_channel(data, id))->window));
    }

    newdata_field = gwy_data_field_duplicate(data_field);

    newdata_field = gwy_data_field_correct_drift(data_field,
                                                 newdata_field,
                                                 controls->result,
                                                 args->is_crop);

    if (args->is_correct) {
        newid = gwy_app_data_browser_add_data_field(newdata_field, data, TRUE);

        gwy_app_set_data_field_title(data, newid, _("Drift corrected data"));

    }
    g_object_unref(newdata_field);

    if (args->is_graph) {
        gmodel = gwy_graph_model_new();
        cmodel = gwy_graph_curve_model_new();
        gwy_graph_model_add_curve(gmodel, cmodel);

        g_object_set(gmodel, "title", _("Drift graph"), 
                             "axis-label-left", _("drift"),
                             NULL);
        gwy_graph_model_set_units_from_data_line(gmodel, controls->result);
        g_object_set(cmodel, "description", _("x-axis drift"), NULL);
        gwy_graph_curve_model_set_data_from_dataline(cmodel, controls->result,
                                                     0, 0);

        newid = gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        gwy_object_unref(cmodel);
        gwy_object_unref(gmodel);
    }
}

static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             DriftArgs *args,
             DriftControls *controls,
             GtkWindow *wait_window)
{
    gint i, j, step, pos, xres, yres;
    gint npoints = 0;
    gdouble *mdata, *rdata;
    gchar message[50];
    GdkCursor *wait_cursor;

    g_assert(GWY_IS_DATA_FIELD(dfield));

    if (maskfield)
        gwy_data_field_clear(maskfield);
    gwy_data_line_clear(controls->result);

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    if (args->method == GWY_DRIFT_CORRELATION) {
        wait_cursor = gdk_cursor_new(GDK_WATCH);
        gdk_window_set_cursor(wait_window, wait_cursor);
        while (gtk_events_pending())
            gtk_main_iteration_do(FALSE);

        npoints = gwy_data_field_get_drift_from_correlation
            (dfield, controls->result,
             MAX(1, (gint)(args->averaging)),
             args->smoothing,
             0.9);

        gdk_window_set_cursor(wait_window, NULL);
        gdk_cursor_unref(wait_cursor);
    }
    if (maskfield) {
        g_snprintf(message, sizeof(message), "%d points fitted", npoints);
        gtk_label_set_text(GTK_LABEL(controls->message), message);
    }

    if (npoints)
    {
        controls->computed = TRUE;
        step = yres/10;
        rdata = gwy_data_line_get_data(controls->result);
        if (maskfield) {
            mdata = gwy_data_field_get_data(maskfield);
            for (i = 0; i < yres; i++) {
                for (j = -step; j < xres + step; j += step) {
                    pos = j + gwy_data_field_rtoi(dfield, rdata[i]);
                    if (pos > 1 && pos < xres) {
                        mdata[(gint)(pos + i*xres)] = 1;
                    if (xres >= 300)
                        mdata[(gint)(pos - 1 + i*xres)] = 1;
                    }
                }
            }
        }
    }
    else
        controls->computed = FALSE;
    if (maskfield)  gwy_data_field_data_changed(maskfield);
}

static const gchar iscorrect_key[]     = "/module/drift/iscorrect";
static const gchar isgraph_key[]       = "/module/drift/isgraph";
static const gchar iscrop_key[]        = "/module/drift/iscrop";
static const gchar averaging_key[]   = "/module/drift/averaging";
static const gchar smoothing_key[]     = "/module/drift/smoothing";
static const gchar method_key[]        = "/module/drift/method";
static const gchar interpolation_key[] = "/module/drift/interpolation";

static void
drift_sanitize_args(DriftArgs *args)
{
    args->is_correct = !!args->is_correct;
    args->is_crop = !!args->is_crop;
    args->is_graph = !!args->is_graph;
    args->averaging = CLAMP(args->averaging, 1.0, 100.0);
    args->smoothing = CLAMP(args->smoothing, 1.0, 10.0);
    args->method = GWY_DRIFT_CORRELATION;
    /*args->method = MIN(args->method, GWY_DRIFT_CORRELATION);*/
    args->interpolation = gwy_enum_sanitize_value(args->interpolation,
                                                  GWY_TYPE_INTERPOLATION_TYPE);
}

static void
drift_load_args(GwyContainer *container,
               DriftArgs *args)
{
    *args = drift_defaults;

    gwy_container_gis_boolean_by_name(container, iscorrect_key,
                                      &args->is_correct);
    gwy_container_gis_boolean_by_name(container, iscrop_key,
                                      &args->is_crop);
    gwy_container_gis_boolean_by_name(container, isgraph_key, &args->is_graph);
    gwy_container_gis_double_by_name(container, averaging_key,
                                     &args->averaging);
    gwy_container_gis_double_by_name(container, smoothing_key,
                                     &args->smoothing);
    gwy_container_gis_enum_by_name(container, method_key,
                                   &args->method);
    gwy_container_gis_enum_by_name(container, interpolation_key,
                                   &args->interpolation);
    drift_sanitize_args(args);
}

static void
drift_save_args(GwyContainer *container,
               DriftArgs *args)
{
    gwy_container_set_boolean_by_name(container, isgraph_key, args->is_graph);
    gwy_container_set_boolean_by_name(container, iscorrect_key,
                                      args->is_correct);
    gwy_container_set_boolean_by_name(container, iscrop_key, args->is_crop);
    gwy_container_set_double_by_name(container, averaging_key,
                                     args->averaging);
    gwy_container_set_double_by_name(container, smoothing_key, args->smoothing);
    gwy_container_set_enum_by_name(container, interpolation_key,
                                   args->interpolation);
    gwy_container_set_enum_by_name(container, method_key, args->method);
}




static gdouble
gwy_data_field_get_row_correlation_score(GwyDataField *data_field,
                                            gint line1_index,
                                            gint line1_start,
                                            gint line2_index,
                                            gint line2_start,
                                            gint area_length)
{
    gint i;
    gdouble score = 0, avg1 = 0, avg2 = 0, rms1 = 0, rms2 = 0;

    g_return_val_if_fail(line1_index >= 0 && line1_index < data_field->yres, -1);
    g_return_val_if_fail(line2_index >= 0 && line2_index < data_field->yres, -1);

    for (i = 0; i < area_length; i++)
    {
        avg1 += data_field->data[i + line1_start + data_field->xres*line1_index];
        avg2 += data_field->data[i + line2_start + data_field->xres*line2_index];
    }
    avg1 /= area_length;
    avg2 /= area_length;

    for (i = 0; i < area_length; i++)
    {
        score += (data_field->data[i + line1_start + data_field->xres*line1_index]-avg1)
            *(data_field->data[i + line2_start + data_field->xres*line2_index]-avg2);

        rms1 += (data_field->data[i + line1_start + data_field->xres*line1_index]-avg1)
            *(data_field->data[i + line1_start + data_field->xres*line1_index]-avg1);
        rms2 += (data_field->data[i + line2_start + data_field->xres*line2_index]-avg2)
            *(data_field->data[i + line2_start + data_field->xres*line2_index]-avg2);
    }
    rms1 = sqrt(rms1/area_length);
    rms2 = sqrt(rms2/area_length);

    score /= rms1 * rms2 * area_length;

    return score;
}

static gint
gwy_data_field_get_drift_from_correlation(GwyDataField *data_field,
                                          GwyDataLine *drift_result,
                                          gint skip_tolerance,
                                          gint polynom_degree,
                                          gdouble threshold)
{
    gint col, row, nextrow, colmax, i;
    gint maxshift = 10;
    gdouble val;
    gdouble maxscore, coefs[3];
    GwyDataLine *score;
    gdouble *shift_rows, *shift_cols, shift_coefs[20];
    gint shift_ndata, neighbourhood_start, neighbourhood_end;
    gdouble glob_maxscore, glob_colmax, fit_score, fit_colmax, avg_score;
    
    fit_score = glob_colmax = colmax = 0;

    g_assert(GWY_IS_DATA_FIELD(data_field));
    if (gwy_data_line_get_res(drift_result) != (data_field->yres))
        gwy_data_line_resample(drift_result, data_field->yres, GWY_INTERPOLATION_NONE);

    gwy_data_field_copy_units_to_data_line(data_field, drift_result);

    score = (GwyDataLine *) gwy_data_line_new(2*maxshift, 2*maxshift, FALSE);
    shift_cols = (gdouble *)g_malloc((data_field->yres - 1)*sizeof(gdouble));
    shift_rows = (gdouble *)g_malloc((data_field->yres - 1)*sizeof(gdouble));
    shift_ndata= 0;

    for (row=0; row < (data_field->yres - 1); row++)
        shift_rows[row] = 0;

    for (row=0; row < (data_field->yres - 1); row++)
    {
        glob_maxscore = -G_MAXDOUBLE;

        neighbourhood_start = MAX(0, row -  skip_tolerance);
        neighbourhood_end = MIN(data_field->yres - 1, row + skip_tolerance);

        for (nextrow=neighbourhood_start; nextrow <= neighbourhood_end; nextrow++)
        {
            if (nextrow == row) continue;

            maxscore = -G_MAXDOUBLE;

            avg_score = 0;
            for (col = -maxshift; col < maxshift; col++)
            {
                score->data[col+maxshift] = gwy_data_field_get_row_correlation_score(data_field,
                                                            row,
                                                            maxshift,
                                                            nextrow,
                                                            maxshift + col,
                                                            data_field->xres - 2*maxshift);
                avg_score += score->data[col+maxshift]/2/maxshift;
                if (score->data[col+maxshift] > maxscore) {
                    maxscore = score->data[col+maxshift];
                    colmax = col;
                }
            }
            if (colmax <= (-maxshift + 2) || colmax >= (maxshift - 3) || (fabs(maxscore/avg_score)<1.01))
            {
                fit_score = fit_colmax = 0;
            }
            else
            {
                gwy_data_line_part_fit_polynom(score, 2, coefs, colmax + maxshift - 2, colmax + maxshift + 2);
                fit_colmax = -coefs[1]/2/coefs[2];
                fit_score = coefs[2]*fit_colmax*fit_colmax + coefs[1]*fit_colmax + coefs[0];
            }

            if (fit_score > glob_maxscore)
            {
                glob_maxscore = fit_score;
                glob_colmax = (fit_colmax - maxshift)/(nextrow - row);
            }

        }
        if (fit_score>threshold) {
            shift_rows[shift_ndata] = row;
            if (shift_ndata)
                shift_cols[shift_ndata] = shift_cols[shift_ndata - 1] + glob_colmax;
            else shift_cols[shift_ndata] = glob_colmax;
            shift_ndata++;
        }
    }
    gwy_math_fit_polynom(shift_ndata, shift_rows, shift_cols, polynom_degree, shift_coefs);

    for (row=0; row < (data_field->yres); row++)
    {
        val = 0;
        for (i = (polynom_degree); i; i--) {
            val += shift_coefs[i];
            val *= row;
        }
        val += shift_coefs[0];
        drift_result->data[row] = gwy_data_field_itor(data_field, val);
    }

    g_object_unref(score);
    g_free(shift_cols);
    g_free(shift_rows);

    return shift_ndata;
}

static GwyDataField*
gwy_data_field_correct_drift(GwyDataField *data_field,
                             GwyDataField *corrected_field,
                             GwyDataLine *measured_drift,
                             gboolean crop)
{
    gint min, max, newxres, col, row;
    gdouble *newdata, dx, dy;

    min = (gint)gwy_data_field_rtoi(data_field, gwy_data_line_get_min(measured_drift));
    max = (gint)gwy_data_field_rtoi(data_field, gwy_data_line_get_max(measured_drift));
    newxres = gwy_data_field_get_xres(data_field) + MIN(0, min) + MAX(0, max);

    gwy_data_field_resample(corrected_field, newxres, gwy_data_field_get_yres(data_field),
                            GWY_INTERPOLATION_NONE);
    gwy_data_field_fill(corrected_field, gwy_data_field_get_min(data_field));
    newdata = gwy_data_field_get_data(corrected_field);

    for (col = 0; col < newxres; col++)
        for (row = 0; row < gwy_data_field_get_yres(data_field); row++)
        {
            dy = row;
            dx = col + gwy_data_field_rtoi(data_field, measured_drift->data[row]);

            if (dx > 0 && dx <= (gwy_data_field_get_xres(data_field) - 1))
                newdata[col + row*newxres] =
                    gwy_data_field_get_dval(data_field, dx, dy, GWY_INTERPOLATION_BILINEAR);
        }
    if (crop)
        gwy_data_field_resize(corrected_field, 
                              MAX(0, -(min - 1)), 0,
                              MIN(newxres, 
                                  gwy_data_field_get_xres(data_field) - max), 
                                  gwy_data_field_get_yres(data_field));

    return corrected_field;
}




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
