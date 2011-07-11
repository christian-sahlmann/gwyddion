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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/grains.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define WSHED_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

typedef struct {
    gboolean inverted;
    gint locate_steps;
    gint locate_thresh;
    gint wshed_steps;
    gdouble locate_dropsize;
    gdouble wshed_dropsize;
} WshedArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *inverted;
    GtkWidget *view;
    GtkObject *locate_steps;
    GtkObject *locate_thresh;
    GtkObject *wshed_steps;
    GtkObject *locate_dropsize;
    GtkObject *wshed_dropsize;
    GtkWidget *color_button;
    GwyContainer *mydata;
    gboolean computed;
} WshedControls;

static gboolean    module_register              (void);
static void        grain_wshed                  (GwyContainer *data,
                                                 GwyRunType run);
static void        run_noninteractive           (WshedArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GQuark mquark,
                                                 gint id);
static void        wshed_dialog                 (WshedArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id,
                                                 GQuark mquark);
static void        mask_color_change_cb         (GtkWidget *color_button,
                                                 WshedControls *controls);
static void        load_mask_color              (GtkWidget *color_button,
                                                 GwyContainer *data);
static void        wshed_dialog_update_controls (WshedControls *controls,
                                                 WshedArgs *args);
static void        wshed_dialog_update_values   (WshedControls *controls,
                                                 WshedArgs *args);
static void        wshed_invalidate             (WshedControls *controls);
static void        preview                      (WshedControls *controls,
                                                 WshedArgs *args);
static gboolean    mask_process                 (GwyDataField *dfield,
                                                 GwyDataField *maskfield,
                                                 WshedArgs *args,
                                                 GtkWindow *wait_window);
static void        wshed_load_args              (GwyContainer *container,
                                                 WshedArgs *args);
static void        wshed_save_args              (GwyContainer *container,
                                                 WshedArgs *args);
static void        wshed_sanitize_args          (WshedArgs *args);

static const WshedArgs wshed_defaults = {
    FALSE,
    10,
    3,
    10,
    10,
    1
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Marks grains by watershed algorithm."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.16",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_wshed",
                              (GwyProcessFunc)&grain_wshed,
                              N_("/_Grains/Mark by _Watershed..."),
                              GWY_STOCK_GRAINS_WATER,
                              WSHED_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark grains by watershed"));

    return TRUE;
}

static void
grain_wshed(GwyContainer *data, GwyRunType run)
{
    WshedArgs args;
    GwyDataField *dfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & WSHED_RUN_MODES);
    wshed_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield && mquark);

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data, dfield, mquark, id);
    else {
        wshed_dialog(&args, data, dfield, id, mquark);
        wshed_save_args(gwy_app_settings_get(), &args);
    }
}

static void
wshed_dialog(WshedArgs *args,
             GwyContainer *data,
             GwyDataField *dfield,
             gint id,
             GQuark mquark)
{
    GtkWidget *dialog, *table, *label, *spin, *hbox;
    WshedControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    GwyPixmapLayer *layer;
    GwyDataField *mfield;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Mark Grains by Watershed"),
                                         NULL, 0, NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(hbox),
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

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gwy_label_new_header(_("Grain Location"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.locate_steps = gtk_adjustment_new(args->locate_steps,
                                               1.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("_Number of steps:"), "",
                            controls.locate_steps, 0);
    g_signal_connect_swapped(controls.locate_steps, "value-changed",
                             G_CALLBACK(wshed_invalidate), &controls);
    row++;

    controls.locate_dropsize = gtk_adjustment_new(args->locate_dropsize,
                                                  0.01, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Drop size:"), "%",
                                   controls.locate_dropsize, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    g_signal_connect_swapped(controls.locate_dropsize, "value-changed",
                             G_CALLBACK(wshed_invalidate), &controls);

    row++;
    controls.locate_thresh = gtk_adjustment_new(args->locate_thresh,
                                                0.0, 100.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("T_hreshold:"), "px<sup>2</sup>",
                            controls.locate_thresh, 0);
    g_signal_connect_swapped(controls.locate_thresh, "value-changed",
                             G_CALLBACK(wshed_invalidate), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Segmentation")),
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.wshed_steps = gtk_adjustment_new(args->wshed_steps,
                                              1.0, 1000.0, 1, 5, 0);
    gwy_table_attach_hscale(table, row, _("Num_ber of steps:"), "",
                            controls.wshed_steps, 0);
    g_signal_connect_swapped(controls.wshed_steps, "value-changed",
                             G_CALLBACK(wshed_invalidate), &controls);
    row++;

    controls.wshed_dropsize = gtk_adjustment_new(args->wshed_dropsize,
                                                 0.01, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, row, _("Dr_op size:"), "%",
                                   controls.wshed_dropsize, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    g_signal_connect_swapped(controls.wshed_dropsize, "value-changed",
                             G_CALLBACK(wshed_invalidate), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Options")),
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.inverted = gtk_check_button_new_with_mnemonic(_("_Invert height"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inverted),
                                 args->inverted);
    gtk_table_attach(GTK_TABLE(table), controls.inverted,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.inverted, "toggled",
                             G_CALLBACK(wshed_invalidate), &controls);
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

    wshed_invalidate(&controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            wshed_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = wshed_defaults;
            wshed_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            wshed_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    wshed_dialog_update_values(&controls, args);
    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

    if (controls.computed) {
        mfield = gwy_container_get_object_by_name(controls.mydata, "/0/mask");
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data, dfield, mquark, id);
    }
}

static void
wshed_dialog_update_controls(WshedControls *controls,
                             WshedArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->wshed_dropsize),
                             args->wshed_dropsize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->locate_dropsize),
                             args->locate_dropsize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->locate_steps),
                             args->locate_steps);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->wshed_steps),
                             args->wshed_steps);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->locate_thresh),
                             args->locate_thresh);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverted),
                                 args->inverted);
}

static void
wshed_dialog_update_values(WshedControls *controls,
                           WshedArgs *args)
{
    args->locate_steps = gwy_adjustment_get_int(controls->locate_steps);
    args->locate_thresh = gwy_adjustment_get_int(controls->locate_thresh);
    args->locate_dropsize
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->locate_dropsize));
    args->wshed_steps = gwy_adjustment_get_int(controls->wshed_steps);
    args->wshed_dropsize
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->wshed_dropsize));
    args->inverted
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->inverted));
}

static void
wshed_invalidate(WshedControls *controls)
{
    controls->computed = FALSE;
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     WshedControls *controls)
{
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    gwy_mask_color_selector_run(NULL, GTK_WINDOW(controls->dialog),
                                GWY_COLOR_BUTTON(color_button), data,
                                "/0/mask");
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
    siunit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static void
preview(WshedControls *controls,
        WshedArgs *args)
{
    GwyDataField *mfield, *dfield;
    GwyPixmapLayer *layer;

    wshed_dialog_update_values(controls, args);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /* Set up the mask */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask",
                                          &mfield)) {
        mfield = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mfield);
        g_object_unref(mfield);

        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }

    controls->computed = mask_process(dfield, mfield, args,
                                      GTK_WINDOW(controls->dialog));
    if (controls->computed)
        gwy_data_field_data_changed(mfield);
}

static void
run_noninteractive(WshedArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GQuark mquark,
                   gint id)
{
    GwyDataField *mfield;

    mfield = create_mask_field(dfield);
    if (mask_process(dfield, mfield, args,
                     gwy_app_find_window_for_channel(data, id))) {
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
    }
    g_object_unref(mfield);
}

static gboolean
mask_process(GwyDataField *dfield, GwyDataField *maskfield, WshedArgs *args,
             GtkWindow *wait_window)
{
    gdouble max, min, q;
    GwyComputationState *state;
    GwyWatershedStateType oldstate = -1;
    gboolean ok;

    max = gwy_data_field_get_max(dfield);
    min = gwy_data_field_get_min(dfield);
    q = (max - min)/5000.0;

    state = gwy_data_field_grains_watershed_init(dfield, maskfield,
                                                 args->locate_steps,
                                                 args->locate_thresh,
                                                 args->locate_dropsize*q,
                                                 args->wshed_steps,
                                                 args->wshed_dropsize*q,
                                                 FALSE, args->inverted);
    gwy_app_wait_start(wait_window, _("Initializing"));

    do {
        gwy_data_field_grains_watershed_iteration(state);
        if (oldstate != state->state) {
            if (state->state == GWY_WATERSHED_STATE_MIN)
                gwy_app_wait_set_message(_("Finding minima"));
            else if (state->state == GWY_WATERSHED_STATE_LOCATE)
                gwy_app_wait_set_message(_("Locating"));
            else if (state->state == GWY_WATERSHED_STATE_WATERSHED)
                gwy_app_wait_set_message(_("Watershed"));
            else if (state->state == GWY_WATERSHED_STATE_MARK)
                gwy_app_wait_set_message(_("Marking boundaries"));
            oldstate = state->state;
        }
        if (!gwy_app_wait_set_fraction(state->fraction))
            break;
    } while (state->state != GWY_WATERSHED_STATE_FINISHED);
    ok = (state->state == GWY_WATERSHED_STATE_FINISHED);

    gwy_app_wait_finish();
    gwy_data_field_grains_watershed_finalize(state);

    return ok;
}

static const gchar inverted_key[]        = "/module/grain_wshed/inverted";
static const gchar locate_steps_key[]    = "/module/grain_wshed/locate_steps";
static const gchar locate_thresh_key[]   = "/module/grain_wshed/locate_thresh";
static const gchar locate_dropsize_key[] = "/module/grain_wshed/locate_dropsize";
static const gchar wshed_steps_key[]     = "/module/grain_wshed/wshed_steps";
static const gchar wshed_dropsize_key[]  = "/module/grain_wshed/wshed_dropsize";

static void
wshed_sanitize_args(WshedArgs *args)
{
    args->inverted = !!args->inverted;
    args->locate_dropsize = CLAMP(args->locate_dropsize, 0.01, 100.0);
    args->wshed_dropsize = CLAMP(args->wshed_dropsize, 0.01, 100.0);
    args->locate_thresh = CLAMP(args->locate_thresh, 0, 100);
    args->locate_steps = CLAMP(args->locate_steps, 1, 100);
    args->wshed_steps = CLAMP(args->wshed_steps, 1, 1000);
}

static void
wshed_load_args(GwyContainer *container,
                WshedArgs *args)
{
    *args = wshed_defaults;

    gwy_container_gis_boolean_by_name(container, inverted_key, &args->inverted);
    gwy_container_gis_double_by_name(container, locate_dropsize_key,
                                     &args->locate_dropsize);
    gwy_container_gis_double_by_name(container, wshed_dropsize_key,
                                     &args->wshed_dropsize);
    gwy_container_gis_int32_by_name(container, locate_steps_key,
                                    &args->locate_steps);
    gwy_container_gis_int32_by_name(container, wshed_steps_key,
                                    &args->wshed_steps);
    gwy_container_gis_int32_by_name(container, locate_thresh_key,
                                    &args->locate_thresh);
    wshed_sanitize_args(args);
}

static void
wshed_save_args(GwyContainer *container,
                WshedArgs *args)
{
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    gwy_container_set_double_by_name(container, wshed_dropsize_key,
                                     args->wshed_dropsize);
    gwy_container_set_double_by_name(container, locate_dropsize_key,
                                     args->locate_dropsize);
    gwy_container_set_int32_by_name(container, locate_steps_key,
                                    args->locate_steps);
    gwy_container_set_int32_by_name(container, wshed_steps_key,
                                    args->wshed_steps);
    gwy_container_set_int32_by_name(container, locate_thresh_key,
                                    args->locate_thresh);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
