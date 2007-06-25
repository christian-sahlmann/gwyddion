/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-process.h>
#include <libprocess/linestats.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/interpolation.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define DRIFT_RUN_MODES (GWY_RUN_INTERACTIVE | GWY_RUN_IMMEDIATE)

enum {
    PREVIEW_SIZE = 400
};

typedef enum {
    PREVIEW_CORRECTED = 0,
    PREVIEW_MASK      = 1,
    PREVIEW_LAST
} DriftPreviewType;

/* Data for this function. */
typedef struct {
    DriftPreviewType preview_type;
    gint range;
    gboolean do_graph;
    gboolean do_correct;
    gboolean exclude_linear;
    GwyInterpolationType interp;
} DriftArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *do_graph;
    GtkWidget *do_correct;
    GtkWidget *exclude_linear;
    GtkObject *range;
    GtkWidget *interp;
    GSList *preview_type;
    GtkWidget *color_button;
    GwyContainer *mydata;
    GwyDataField *result;
    GwyDataLine *drift;
    gboolean computed;
    DriftArgs *args;
} DriftControls;

static gboolean      module_register              (void);
static void          compensate_drift             (GwyContainer *data,
                                                   GwyRunType run);
static void          drift_dialog                 (DriftArgs *args,
                                                   GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   gint id);
static void          run_noninteractive           (DriftArgs *args,
                                                   GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   GwyDataField *result,
                                                   GwyDataLine *drift,
                                                   gint id);
static void          mask_color_change_cb         (GtkWidget *color_button,
                                                   DriftControls *controls);
static void          load_mask_color              (GtkWidget *color_button,
                                                   GwyContainer *data);
static GwyDataField* create_mask_field            (GwyDataField *dfield);
static void          drift_dialog_update_controls (DriftControls *controls,
                                                   DriftArgs *args);
static void          drift_dialog_update_values   (DriftControls *controls,
                                                   DriftArgs *args);
static void          drift_invalidate             (GObject *obj,
                                                   DriftControls *controls);
static void          preview_type_changed         (GtkWidget *button,
                                                   DriftControls *controls);
static void          preview                      (DriftControls *controls,
                                                   DriftArgs *args);
static void          mask_process                 (GwyDataField *maskfield,
                                                   GwyDataLine *drift);
static void          drift_load_args              (GwyContainer *container,
                                                   DriftArgs *args);
static void          drift_save_args              (GwyContainer *container,
                                                   DriftArgs *args);
static void          drift_sanitize_args          (DriftArgs *args);
static void          gwy_data_field_normalize_rows(GwyDataField *dfield);
static void          match_line                   (gint res,
                                                   const gdouble *ref,
                                                   const gdouble *cmp,
                                                   gint maxoff,
                                                   gdouble *offset,
                                                   gdouble *score);
static void          calculate_correlation_scores (GwyDataField *dfield,
                                                   gdouble range,
                                                   gdouble maxoffset,
                                                   gint supersample,
                                                   GwyInterpolationType interp,
                                                   GwyDataField *scores,
                                                   GwyDataField *offsets);
static void          calculate_drift_very_naive   (GwyDataField *offsets,
                                                   GwyDataField *scores,
                                                   GwyDataLine *drift);
static void          apply_drift                  (GwyDataField *dfield,
                                                   GwyDataLine *drift,
                                                   GwyInterpolationType interp);
static void          drift_do                     (DriftArgs *args,
                                                   GwyDataField *dfield,
                                                   GwyDataField *result,
                                                   GwyDataLine *drift);

static const DriftArgs drift_defaults = {
    PREVIEW_MASK,
    12,
    TRUE,
    TRUE,
    FALSE,
    GWY_INTERPOLATION_BSPLINE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Evaluates and/or correct thermal drift in fast scan axis."),
    "Petr Klapetek <petr@klapetek.cz>, Yeti <yeti@gwyddion.net>",
    "2.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("drift",
                              (GwyProcessFunc)&compensate_drift,
                              N_("/_Correct Data/Compensate _Drift..."),
                              GWY_STOCK_DRIFT,
                              DRIFT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Evaluate/correct thermal drift in fast "
                                 "scan axis"));

    return TRUE;
}

static void
compensate_drift(GwyContainer *data, GwyRunType run)
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

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data, dfield, NULL, NULL, id);
    else {
        drift_dialog(&args, data, dfield, id);
        drift_save_args(gwy_app_settings_get(), &args);
    }
}

static void
drift_dialog(DriftArgs *args,
             GwyContainer *data,
             GwyDataField *dfield,
             gint id)
{
    enum {
        RESPONSE_RESET   = 1,
        RESPONSE_PREVIEW = 2
    };

    GtkWidget *dialog, *table, *hbox, *spin, *label;
    DriftControls controls;
    gint response;
    GwyPixmapLayer *layer;
    gint row;

    memset(&controls, 0, sizeof(DriftControls));
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Compensate Drift"), NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
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
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    g_object_unref(controls.mydata);

    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    layer = gwy_layer_mask_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/mask");
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view), layer);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.range = gtk_adjustment_new(args->range, 1.0, 50.0, 1.0, 5.0, 0);
    spin = gwy_table_attach_hscale(table, row, "_Search range:", "rows",
                                   controls.range, GWY_HSCALE_DEFAULT);
    g_signal_connect(controls.range, "value-changed",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interp, args->interp, TRUE);
    gwy_table_attach_hscale(table, row, _("_Interpolation type:"), NULL,
                            GTK_OBJECT(controls.interp),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    controls.do_graph
        = gtk_check_button_new_with_mnemonic(_("Plot drift _graph"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_graph),
                                 args->do_graph);
    gtk_table_attach(GTK_TABLE(table), controls.do_graph,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.do_graph, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    controls.do_correct
        = gtk_check_button_new_with_mnemonic(_("Correct _data"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_correct),
                                 args->do_correct);
    gtk_table_attach(GTK_TABLE(table), controls.do_correct,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.do_correct, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    row++;

    controls.exclude_linear
        = gtk_check_button_new_with_mnemonic(_("_Exclude linear skew"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.exclude_linear),
                                 args->exclude_linear);
    gtk_table_attach(GTK_TABLE(table), controls.exclude_linear,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.exclude_linear, "toggled",
                     G_CALLBACK(drift_invalidate), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Preview type"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.preview_type
        = gwy_radio_buttons_createl(G_CALLBACK(preview_type_changed), &controls,
                                    args->preview_type,
                                    _("Correc_ted data"), PREVIEW_CORRECTED,
                                    _("Drift _lines"), PREVIEW_MASK,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.preview_type,
                                            GTK_TABLE(table), 3, row);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

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

    controls.computed = FALSE;
    /* Set up initial layer keys properly */
    preview_type_changed(NULL, &controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            drift_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            gwy_object_unref(controls.result);
            gwy_object_unref(controls.drift);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = drift_defaults;
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

    if (controls.computed)
        run_noninteractive(args, data, dfield,
                           controls.result, controls.drift,
                           id);
    else {
        gwy_object_unref(controls.result);
        gwy_object_unref(controls.drift);
        run_noninteractive(args, data, dfield, NULL, NULL, id);
    }
}

/* XXX: Eats result and drift */
static void
run_noninteractive(DriftArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GwyDataField *result,
                   GwyDataLine *drift,
                   gint id)
{
    if (!args->do_correct && !args->do_graph) {
        gwy_object_unref(result);
        gwy_object_unref(drift);
        return;
    }

    if (!drift) {
        g_assert(!result);
        result = gwy_data_field_duplicate(dfield);
        drift = gwy_data_line_new(1, 1.0, FALSE);
        drift_do(args, dfield, result, drift);
    }

    if (args->do_correct) {
        gint newid;

        newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
        gwy_app_set_data_field_title(data, newid, _("Drift-corrected"));
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                GWY_DATA_ITEM_RANGE,
                                0);
    }
    g_object_unref(result);

    if (args->do_graph) {
        GwyGraphModel *gmodel;
        GwyGraphCurveModel *gcmodel;

        gmodel = gwy_graph_model_new();
        gwy_graph_model_set_units_from_data_line(gmodel, drift);
        g_object_set(gmodel,
                     "title", _("Drift"),
                     "axis-label-left", _("drift"),
                     "axis-label-bottom", "y",
                     NULL);

        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, drift, -1, -1);
        g_object_set(gcmodel, "description", _("x-axis drift"), NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        gwy_object_unref(gcmodel);

        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        gwy_object_unref(gmodel);
    }
    g_object_unref(drift);
}

static void
drift_dialog_update_controls(DriftControls *controls,
                            DriftArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->range), args->range);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_graph),
                                 args->do_graph);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_correct),
                                 args->do_correct);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->exclude_linear),
                                 args->exclude_linear);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
}

static void
drift_dialog_update_values(DriftControls *controls,
                          DriftArgs *args)
{
    args->range = gwy_adjustment_get_int(controls->range);
    args->do_graph
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->do_graph));
    args->do_correct
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->do_correct));
    args->exclude_linear
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->exclude_linear));
}

static void
drift_invalidate(G_GNUC_UNUSED GObject *obj,
                 DriftControls *controls)
{
    controls->computed = FALSE;
}

static void
preview_type_changed(G_GNUC_UNUSED GtkWidget *button,
                     DriftControls *controls)
{
    GwyPixmapLayer *blayer, *mlayer;

    controls->args->preview_type
        = gwy_radio_buttons_get_current(controls->preview_type);

    blayer = gwy_data_view_get_base_layer(GWY_DATA_VIEW(controls->view));
    mlayer = gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(controls->view));
    switch (controls->args->preview_type) {
        case PREVIEW_CORRECTED:
        gwy_layer_basic_set_presentation_key(GWY_LAYER_BASIC(blayer),
                                             "/1/data");
        gwy_pixmap_layer_set_data_key(mlayer, "");
        break;

        case PREVIEW_MASK:
        gwy_layer_basic_set_presentation_key(GWY_LAYER_BASIC(blayer), NULL);
        gwy_pixmap_layer_set_data_key(mlayer, "/0/mask");
        break;

        default:
        g_return_if_reached();
        break;
    }
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

    mfield = gwy_data_field_new_alike(dfield, TRUE);
    siunit = gwy_si_unit_new(NULL);
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static void
preview(DriftControls *controls,
        DriftArgs *args)
{
    GwyDataField *mask, *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    /* Set up the mask */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask)) {
        mask = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);
    }

    gwy_app_wait_cursor_start(GTK_WINDOW(controls->dialog));
    if (!controls->result) {
        controls->result = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/1/data",
                                         controls->result);
        controls->drift = gwy_data_line_new(1, 1.0, FALSE);
    }
    else
        gwy_data_field_copy(dfield, controls->result, FALSE);
    drift_do(args, dfield, controls->result, controls->drift);
    gwy_data_field_data_changed(controls->result);
    mask_process(mask, controls->drift);
    gwy_app_wait_cursor_finish(GTK_WINDOW(controls->dialog));

    controls->computed = TRUE;
}

static void
mask_process(GwyDataField *maskfield,
             GwyDataLine *drift)
{
    gint i, j, k, step, pos, xres, yres, w, from, to;
    gdouble *mdata, *rdata;

    gwy_data_field_clear(maskfield);
    xres = gwy_data_field_get_xres(maskfield);
    yres = gwy_data_field_get_yres(maskfield);

    step = xres/10;
    w = (xres + 3*PREVIEW_SIZE/4)/PREVIEW_SIZE;
    w = MAX(w, 1);
    rdata = gwy_data_line_get_data(drift);
    mdata = gwy_data_field_get_data(maskfield);
    for (i = 0; i < yres; i++) {
        for (j = -2*step - step/2; j <= xres + 2*step + step/2; j += step) {
            pos = j + GWY_ROUND(gwy_data_line_rtoi(drift, rdata[i]));
            from = MAX(0, pos - w/2);
            to = MIN(pos + (w - w/2) - 1, xres-1);
            for (k = from; k <= to; k++)
                mdata[i*xres + k] = 1.0;
        }
    }
    gwy_data_field_data_changed(maskfield);
}

/**
 * gwy_data_field_normalize_rows:
 * @dfield: A data field.
 *
 * Normalizes all rows to have mean value 0 and rms 1 (unless they consist of
 * all identical values, then they will have rms 0).
 **/
static void
gwy_data_field_normalize_rows(GwyDataField *dfield)
{
    gdouble *data, *row;
    gdouble avg, rms;
    gint xres, yres, i, j;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    for (i = 0; i < yres; i++) {
        row = data + i*xres;
        avg = 0.0;
        for (j = 0; j < xres; j++)
            avg += row[j];
        avg /= xres;
        rms = 0.0;
        for (j = 0; j < xres; j++) {
            row[j] -= avg;
            rms += row[j]*row[j];
        }
        if (rms > 0.0) {
            rms = sqrt(rms/xres);
            for (j = 0; j < xres; j++)
                row[j] /= rms;
        }
    }
}

/**
 * match_line:
 * @res: The length of @ref and @cmp.
 * @ref: Reference line, it must be normalized.
 * @cmp: Line to match to @ref, it must be normalized.
 * @maxoff: Maximum offset to try.
 * @offset: Location to store the offset.
 * @score: Location to store the score.
 *
 * Matches a line to a reference line using correlation.
 *
 * The offset if from @ref to @cmp.
 **/
static void
match_line(gint res,
           const gdouble *ref,
           const gdouble *cmp,
           gint maxoff,
           gdouble *offset,
           gdouble *score)
{
    gdouble *d;
    gdouble s, z0, zm, zp;
    gint i, j, from, to;

    d = g_newa(gdouble, 2*maxoff + 1);

    for (i = -maxoff; i <= maxoff; i++) {
        s = 0.0;
        from = MAX(0, -i);
        to = res-1 - MAX(0, i);
        for (j = from; j <= to; j++)
            s += ref[j]*cmp[j + i];
        d[i+maxoff] = s/(to - from + 1);
    }

    j = 0;
    for (i = -maxoff; i <= maxoff; i++) {
        if (d[i+maxoff] > d[j+maxoff])
            j = i;
    }

    *score = d[j+maxoff];
    if (ABS(j) == maxoff)
        *offset = j;
    else {
        /* Subpixel correction */
        z0 = d[j+maxoff];
        zm = d[j+maxoff - 1];
        zp = d[j+maxoff + 1];
        *offset = j + (zm - zp)/(zm + zp - 2.0*z0)/2.0;
    }
}

static void
calculate_correlation_scores(GwyDataField *dfield,
                             gdouble range,
                             gdouble maxoffset,
                             gint supersample,
                             GwyInterpolationType interp,
                             GwyDataField *scores,
                             GwyDataField *offsets)
{
    GwyDataField *dsuper;
    GwySIUnit *siunit;
    gint xres, yres;
    gint maxoff, i, ii;
    gdouble offset, score;
    const gdouble *ds;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    maxoff = (gint)ceil(supersample*maxoffset);
    xres *= supersample;
    dsuper = gwy_data_field_new_resampled(dfield, xres, yres, interp);
    gwy_data_field_normalize_rows(dsuper);

    gwy_data_field_resample(scores, 2*range + 1, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_resample(offsets, 2*range + 1, yres, GWY_INTERPOLATION_NONE);

    ds = gwy_data_field_get_data_const(dsuper);
    for (i = 0; i < yres; i++) {
        gwy_data_field_set_val(offsets, range, i, 0.0);
        gwy_data_field_set_val(scores, range, i, 1.0);
        for (ii = i+1; ii <= i+range; ii++) {
            if (ii < yres) {
                match_line(xres, ds + i*xres, ds + ii*xres, maxoff,
                           &offset, &score);
            }
            else {
                offset = 0.0;
                score = -1.0;
            }
            offset = gwy_data_field_jtor(dsuper, offset);
            gwy_data_field_set_val(offsets, ii - (i - range), i, offset);
            gwy_data_field_set_val(scores, ii - (i - range), i, score);
        }
    }
    g_object_unref(dsuper);

    /* Fill symmetric correlation scores and offsets:
     * Delta_{i,j} = -Delta_{j,i}
     * w_{i,j} = w_{j,i}
     */
    for (i = 0; i < yres; i++) {
        for (ii = i-range; ii < i; ii++) {
            if (ii >= 0) {
                offset = gwy_data_field_get_val(offsets, i - (ii - range), ii);
                score = gwy_data_field_get_val(scores, i - (ii - range), ii);
            }
            else {
                offset = 0.0;
                score = -1.0;
            }
            gwy_data_field_set_val(offsets, ii - (i - range), i, -offset);
            gwy_data_field_set_val(scores, ii - (i - range), i, score);
        }
    }

    gwy_data_field_set_yreal(scores, gwy_data_field_get_yreal(dfield));
    gwy_data_field_set_xreal(scores, gwy_data_field_itor(dfield, 2*range + 1));
    gwy_data_field_set_xoffset(scores, gwy_data_field_itor(dfield,
                                                           -range - 0.5));
    gwy_data_field_set_yreal(offsets, gwy_data_field_get_yreal(dfield));
    gwy_data_field_set_xreal(offsets, gwy_data_field_itor(dfield, 2*range + 1));
    gwy_data_field_set_xoffset(offsets, gwy_data_field_itor(dfield,
                                                            -range - 0.5));

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    siunit = gwy_si_unit_duplicate(siunit);
    gwy_data_field_set_si_unit_xy(scores, siunit);
    g_object_unref(siunit);
    siunit = gwy_si_unit_duplicate(siunit);
    gwy_data_field_set_si_unit_xy(offsets, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_duplicate(siunit);
    gwy_data_field_set_si_unit_z(offsets, siunit);
    g_object_unref(siunit);
}

static void
calculate_drift_very_naive(GwyDataField *offsets,
                           GwyDataField *scores,
                           GwyDataLine *drift)
{
    const gdouble *doff, *dsco;
    gdouble *dd;
    gint range, xres, yres;
    gint i, j;
    gdouble dm;

    yres = gwy_data_field_get_yres(offsets);
    xres = gwy_data_field_get_xres(offsets);
    range = (xres - 1)/2;

    doff = gwy_data_field_get_data_const(offsets);
    dsco = gwy_data_field_get_data_const(scores);
    gwy_data_line_resample(drift, yres, GWY_INTERPOLATION_NONE);
    gwy_data_field_copy_units_to_data_line(offsets, drift);
    gwy_data_line_set_real(drift, gwy_data_field_get_yreal(offsets));
    dd = gwy_data_line_get_data(drift);

    for (i = 0; i < yres; i++) {
        gdouble w, sxx, sxz, q;

        sxx = sxz = w = 0.0;
        for (j = -range; j <= range; j++) {
            q = dsco[i*xres + j + range];
            q -= 0.6;
            q = MAX(q, 0.0);
            w += q;
            sxx += w*j*j;
            sxz += w*j*doff[i*xres + j + range];
        }
        if (!w) {
            g_warning("Cannot fit point %d", i);
            dd[i] = 0.0;
        }
        else
            dd[i] = sxz/sxx;
    }

    dm = dd[0];
    dd[0] = 0.0;
    for (i = 1; i < yres; i++) {
        gdouble d = dd[i];

        dd[i] = (dm + d)/2.0;
        dm = d;
    }

    gwy_data_line_cumulate(drift);
}

static void
apply_drift(GwyDataField *dfield,
            GwyDataLine *drift,
            GwyInterpolationType interp)
{
    gdouble *coeff, *data;
    gint xres, yres, i;
    gdouble corr;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);
    coeff = g_new(gdouble, xres);

    for (i = 0; i < yres; i++) {
        corr = gwy_data_field_rtoj(dfield, gwy_data_line_get_val(drift, i));
        memcpy(coeff, data + i*xres, xres*sizeof(gdouble));
        gwy_interpolation_shift_block_1d(xres, coeff, corr, data + i*xres,
                                         interp, GWY_EXTERIOR_BORDER_EXTEND,
                                         0.0, FALSE);
    }

    g_free(coeff);
}

static void
drift_do(DriftArgs *args,
         GwyDataField *dfield,
         GwyDataField *result,
         GwyDataLine *drift)
{
    GwyDataField *offsets, *scores;
    gint yres, maxoffset;

    yres = gwy_data_field_get_yres(dfield);

    offsets = gwy_data_field_new(2*args->range + 1, yres, 1.0, 1.0, FALSE);
    scores = gwy_data_field_new(2*args->range + 1, yres, 1.0, 1.0, FALSE);
    maxoffset = MAX(1, args->range/5);
    calculate_correlation_scores(dfield, args->range, maxoffset,
                                 4, args->interp,
                                 scores, offsets);

    calculate_drift_very_naive(offsets, scores, drift);
    g_object_unref(offsets);
    g_object_unref(scores);

    if (args->exclude_linear) {
        gdouble a, b;

        gwy_data_line_get_line_coeffs(drift, &a, &b);
        gwy_data_line_line_level(drift, a, b);
    }
    gwy_data_line_add(drift, -gwy_data_line_get_median(drift));

    apply_drift(result, drift, args->interp);
}

static const gchar do_correct_key[]     = "/module/drift/do-correct";
static const gchar do_graph_key[]       = "/module/drift/do-graph";
static const gchar exclude_linear_key[] = "/module/drift/exclude-linear";
static const gchar interp_key[]         = "/module/drift/interp";
static const gchar preview_type_key[]   = "/module/drift/preview-type";
static const gchar range_key[]          = "/module/drift/range";

static void
drift_sanitize_args(DriftArgs *args)
{
    args->do_correct = !!args->do_correct;
    args->do_graph = !!args->do_graph;
    args->exclude_linear = !!args->exclude_linear;
    args->range = CLAMP(args->range, 1, 50);
    args->interp = gwy_enum_sanitize_value(args->interp,
                                           GWY_TYPE_INTERPOLATION_TYPE);
    args->preview_type = MAX(args->preview_type, PREVIEW_LAST);
}

static void
drift_load_args(GwyContainer *container,
               DriftArgs *args)
{
    *args = drift_defaults;

    gwy_container_gis_boolean_by_name(container, do_correct_key,
                                      &args->do_correct);
    gwy_container_gis_boolean_by_name(container, do_graph_key, &args->do_graph);
    gwy_container_gis_boolean_by_name(container, exclude_linear_key,
                                      &args->exclude_linear);
    gwy_container_gis_int32_by_name(container, range_key, &args->range);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, preview_type_key,
                                   &args->preview_type);
    drift_sanitize_args(args);
}

static void
drift_save_args(GwyContainer *container,
               DriftArgs *args)
{
    gwy_container_set_boolean_by_name(container, do_graph_key, args->do_graph);
    gwy_container_set_boolean_by_name(container, do_correct_key,
                                      args->do_correct);
    gwy_container_set_boolean_by_name(container, exclude_linear_key,
                                      args->exclude_linear);
    gwy_container_set_int32_by_name(container, range_key, args->range);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, preview_type_key,
                                   args->preview_type);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
