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

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/tip.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define TIP_BLIND_RUN_MODES GWY_RUN_INTERACTIVE

enum {
    MIN_RES = 3,
    MAX_RES = 128,
    MIN_STRIPES = 2,
    MAX_STRIPES = 64,
};

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    gdouble k1;
    gdouble k2;
    gdouble phi1;
    gdouble phi2;
    gdouble xc;
    gdouble yc;
    gdouble zc;
} GwyCurvatureParams;

typedef struct {
    gint xres;
    gint yres;
    gdouble thresh;
    gboolean use_boundaries;
    gboolean same_resolution;
    gboolean split_to_stripes;
    gboolean create_images;
    gboolean plot_size_graph;
    guint nstripes;
    GwyDataObjectId orig;  /* The original source, to filter out incompatible */
    GwyDataObjectId source;
    /* Stripe results */
    GwyDataField **stripetips;
    gboolean *goodtip;
} TipBlindArgs;

typedef struct {
    TipBlindArgs *args;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *graph;
    GtkWidget *data;
    GtkWidget *type;
    GtkObject *threshold;
    GtkWidget *threshold_spin;
    GtkWidget *threshold_unit;
    GtkWidget *boundaries;
    GwyDataField *tip;
    GwyContainer *vtip;
    GtkObject *xres;
    GtkObject *yres;
    gint vxres;
    gint vyres;
    gboolean tipdone;
    gboolean good_tip;
    GtkWidget *same_resolution;
    GtkObject *nstripes;
    GtkObject *stripeno;
    GtkWidget *split_to_stripes;
    GtkWidget *create_images;
    GtkWidget *plot_size_graph;
    gboolean in_update;
    gboolean oldnstripes;
} TipBlindControls;

static gboolean       module_register         (void);
static void           tip_blind               (GwyContainer *data,
                                               GwyRunType run);
static void           tip_blind_dialog        (TipBlindArgs *args);
static void           reset                   (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void           tip_blind_run           (TipBlindControls *controls,
                                               TipBlindArgs *args,
                                               gboolean full);
static void           tip_blind_do            (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void           tip_blind_do_single     (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void           tip_blind_do_images     (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void           tip_blind_do_size_plot  (TipBlindControls *controls,
                                               TipBlindArgs *args);
static void           tip_blind_load_args     (GwyContainer *container,
                                               TipBlindArgs *args);
static void           tip_blind_save_args     (GwyContainer *container,
                                               TipBlindArgs *args);
static void           tip_blind_sanitize_args (TipBlindArgs *args);
static void           width_changed           (GtkAdjustment *adj,
                                               TipBlindControls *controls);
static void           height_changed          (GtkAdjustment *adj,
                                               TipBlindControls *controls);
static void           thresh_changed          (gpointer object,
                                               TipBlindControls *controls);
static void           bound_changed           (GtkToggleButton *button,
                                               TipBlindArgs *args);
static void           same_resolution_changed (GtkToggleButton *button,
                                               TipBlindControls *controls);
static void           data_changed            (GwyDataChooser *chooser,
                                               GwyDataObjectId *object);
static void           split_to_stripes_changed(GtkToggleButton *toggle,
                                               TipBlindControls *controls);
static void           nstripes_changed        (GtkAdjustment *adj,
                                               TipBlindControls *controls);
static void           stripeno_changed        (GtkAdjustment *adj,
                                               TipBlindControls *controls);
static void           create_images_changed   (GtkToggleButton *toggle,
                                               TipBlindControls *controls);
static void           plot_size_graph_changed (GtkToggleButton *toggle,
                                               TipBlindControls *controls);
static gboolean       tip_blind_source_filter (GwyContainer *data,
                                               gint id,
                                               gpointer user_data);
static void           tip_update              (TipBlindControls *controls);
static void           tip_blind_dialog_abandon(TipBlindControls *controls);
static void           sci_entry_set_value     (GtkAdjustment *adj,
                                               GtkComboBox *metric,
                                               gdouble val);
static gboolean       prepare_fields          (GwyDataField *tipfield,
                                               GwyDataField *surface,
                                               gint xres,
                                               gint yres);
static void           prepare_stripe_fields   (TipBlindControls *controls,
                                               gboolean keep);
static void           free_stripe_results     (TipBlindArgs *args);
static void           tip_curvatures          (GwyDataField *tipfield,
                                               gdouble *pc1,
                                               gdouble *pc2);
static GwyGraphModel* size_plot               (TipBlindArgs *args);

static const TipBlindArgs tip_blind_defaults = {
    10, 10, 1e-10, FALSE, TRUE,
    FALSE, FALSE, TRUE, 16,
    { NULL, -1, }, { NULL, -1, },
    NULL, NULL,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Blind estimation of SPM tip using Villarubia's algorithm."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("tip_blind",
                              (GwyProcessFunc)&tip_blind,
                              N_("/_Tip/_Blind Estimation..."),
                              NULL,
                              TIP_BLIND_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Blind tip estimation"));

    return TRUE;
}

static void
tip_blind(GwyContainer *data, GwyRunType run)
{
    TipBlindArgs args;

    g_return_if_fail(run & TIP_BLIND_RUN_MODES);

    tip_blind_load_args(gwy_app_settings_get(), &args);
    args.orig.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.orig.id, 0);
    args.source = args.orig;
    tip_blind_dialog(&args);
    free_stripe_results(&args);
}

static void
tip_blind_dialog(TipBlindArgs *args)
{
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PARTIAL,
        RESPONSE_FULL
    };
    GtkWidget *dialog, *table, *hbox, *vbox, *label;
    GwyGraphModel *gmodel;
    GwyGraphArea *area;
    TipBlindControls controls;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    GQuark quark;
    GwySIUnit *unit;
    gint response, row;

    dialog = gtk_dialog_new_with_buttons(_("Blind Tip Estimation"), NULL, 0,
                                         _("Run _Partial"), RESPONSE_PARTIAL,
                                         _("Run _Full"), RESPONSE_FULL,
                                         _("_Reset Tip"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    controls.args = args;
    controls.in_update = TRUE;
    controls.good_tip = FALSE;
    controls.dialog = dialog;
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      controls.good_tip);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.vxres = 240;
    controls.vyres = 240;
    controls.oldnstripes = args->nstripes;

    /* set initial tip properties */
    quark = gwy_app_get_data_key_for_id(args->source.id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(args->source.data, quark));

    controls.tip = gwy_data_field_new_alike(dfield, TRUE);
    gwy_data_field_resample(controls.tip, args->xres, args->yres,
                            GWY_INTERPOLATION_NONE);
    gwy_data_field_clear(controls.tip);

    /* set up data of rescaled image of the tip */
    controls.vtip = gwy_container_new();
    gwy_app_sync_data_items(args->source.data, controls.vtip,
                            args->source.id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);

    dfield = gwy_data_field_new_alike(controls.tip, TRUE);
    gwy_data_field_resample(dfield, controls.vxres, controls.vyres,
                            GWY_INTERPOLATION_ROUND);
    gwy_container_set_object_by_name(controls.vtip, "/0/data", dfield);
    g_object_unref(dfield);

    /* set up rescaled image of the tip */
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    controls.view = gwy_data_view_new(controls.vtip);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), "/0/base/palette");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);

    /* set up tip estimation controls */
    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(gmodel);
    g_object_unref(gmodel);
    gwy_axis_set_visible(gwy_graph_get_axis(GWY_GRAPH(controls.graph),
                                            GTK_POS_LEFT),
                         FALSE);
    gwy_axis_set_visible(gwy_graph_get_axis(GWY_GRAPH(controls.graph),
                                            GTK_POS_BOTTOM),
                         FALSE);
    area = GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(controls.graph)));
    gtk_widget_set_no_show_all(gwy_graph_area_get_label(area), TRUE);
    g_signal_connect_after(gwy_graph_area_get_label(area), "map",
                           G_CALLBACK(gtk_widget_hide), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, TRUE, TRUE, 0);

    table = gtk_table_new(13, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);
    row = 0;

    controls.data = gwy_data_chooser_new_channels();
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.data),
                                tip_blind_source_filter, &args->orig, NULL);
    gwy_data_chooser_set_active(GWY_DATA_CHOOSER(controls.data),
                                args->source.data, args->source.id);
    g_signal_connect(controls.data, "changed",
                     G_CALLBACK(data_changed), &args->source);
    gwy_table_attach_hscale(table, row, _("Related _data:"), NULL,
                            GTK_OBJECT(controls.data), GWY_HSCALE_WIDGET);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(_("Estimated Tip Size"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.xres = gtk_adjustment_new(args->xres, MIN_RES, MAX_RES, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Width:"), "px", controls.xres, 0);
    g_object_set_data(G_OBJECT(controls.xres), "controls", &controls);
    g_signal_connect(controls.xres, "value-changed",
                     G_CALLBACK(width_changed), &controls);
    row++;

    controls.yres = gtk_adjustment_new(args->yres, MIN_RES, MAX_RES, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Height:"), "px", controls.yres, 0);
    g_object_set_data(G_OBJECT(controls.yres), "controls", &controls);
    g_signal_connect(controls.yres, "value-changed",
                     G_CALLBACK(height_changed), &controls);
    row++;

    controls.same_resolution
        = gtk_check_button_new_with_mnemonic(_("_Same resolution"));
    gtk_table_attach(GTK_TABLE(table), controls.same_resolution,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.same_resolution),
                                 args->same_resolution);
    g_signal_connect(controls.same_resolution, "toggled",
                     G_CALLBACK(same_resolution_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Options")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.threshold = gtk_adjustment_new(1.0, 0.01, 1000.0, 0.01, 1.0, 0.0);
    controls.threshold_spin
        = gtk_spin_button_new(GTK_ADJUSTMENT(controls.threshold), 0.1, 2);
    gtk_table_attach(GTK_TABLE(table), controls.threshold_spin,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new_with_mnemonic(_("Noise suppression t_hreshold:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), controls.threshold_spin);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    unit = gwy_data_field_get_si_unit_z(dfield);
    controls.threshold_unit
        = gwy_combo_box_metric_unit_new(G_CALLBACK(thresh_changed),
                                        &controls,
                                        -12, -3, unit, -9);
    gtk_table_attach(GTK_TABLE(table), controls.threshold_unit,
                     3, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.threshold, "value-changed",
                     G_CALLBACK(thresh_changed), &controls);
    sci_entry_set_value(GTK_ADJUSTMENT(controls.threshold),
                        GTK_COMBO_BOX(controls.threshold_unit),
                        args->thresh);
    row++;

    controls.boundaries
                    = gtk_check_button_new_with_mnemonic(_("Use _boundaries"));
    gtk_table_attach(GTK_TABLE(table), controls.boundaries,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.boundaries),
                                                 args->use_boundaries);
    g_signal_connect(controls.boundaries, "toggled",
                     G_CALLBACK(bound_changed), args);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Stripes")),
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.nstripes = gtk_adjustment_new(args->nstripes,
                                           MIN_STRIPES, MAX_STRIPES, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Split to stripes:"), NULL,
                            controls.nstripes, GWY_HSCALE_CHECK);
    controls.split_to_stripes = gwy_table_hscale_get_check(controls.nstripes);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.split_to_stripes),
                                 !args->split_to_stripes);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.split_to_stripes),
                                 args->split_to_stripes);
    g_signal_connect(controls.split_to_stripes, "toggled",
                     G_CALLBACK(split_to_stripes_changed), &controls);
    g_signal_connect(controls.nstripes, "value-changed",
                     G_CALLBACK(nstripes_changed), &controls);
    row++;

    controls.stripeno = gtk_adjustment_new(1, 1, args->nstripes, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("_Preview stripe:"), NULL,
                            controls.stripeno, GWY_HSCALE_DEFAULT);
    g_signal_connect(controls.stripeno, "value-changed",
                     G_CALLBACK(stripeno_changed), &controls);
    row++;

    controls.plot_size_graph
        = gtk_check_button_new_with_mnemonic(_("Plot size _graph"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.plot_size_graph),
                                 args->plot_size_graph);
    gtk_table_attach(GTK_TABLE(table), controls.plot_size_graph,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.plot_size_graph, "toggled",
                     G_CALLBACK(plot_size_graph_changed), &controls);
    row++;

    controls.create_images
        = gtk_check_button_new_with_mnemonic(_("Create tip i_mages"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.create_images),
                                 args->create_images);
    gtk_table_attach(GTK_TABLE(table), controls.create_images,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.create_images, "toggled",
                     G_CALLBACK(create_images_changed), &controls);
    row++;

    controls.tipdone = FALSE;
    controls.in_update = FALSE;
    split_to_stripes_changed(GTK_TOGGLE_BUTTON(controls.split_to_stripes),
                             &controls);
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            tip_blind_dialog_abandon(&controls);
            tip_blind_save_args(gwy_app_settings_get(), args);
            return;
            break;

            case GTK_RESPONSE_OK:
            tip_blind_save_args(gwy_app_settings_get(), args);
            tip_blind_do(&controls, args);
            break;

            case RESPONSE_RESET:
            reset(&controls, args);
            break;

            case RESPONSE_PARTIAL:
            tip_blind_run(&controls, args, FALSE);
            break;

            case RESPONSE_FULL:
            tip_blind_run(&controls, args, TRUE);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    tip_blind_dialog_abandon(&controls);

    return;
}

static void
tip_blind_dialog_abandon(TipBlindControls *controls)
{
    /*free data of the rescaled tip image*/
    g_object_unref(controls->vtip);

    /*if dialog was cancelled, free also tip data*/
    if (!controls->tipdone)
        g_object_unref(controls->tip);
}

static void
sci_entry_set_value(GtkAdjustment *adj,
                    GtkComboBox *metric,
                    gdouble val)
{
    gint mag;

    mag = 3*(gint)floor(log10(val)/3.0);
    mag = CLAMP(mag, -12, -3);
    g_signal_handlers_block_matched(metric, G_SIGNAL_MATCH_FUNC,
                                    0, 0, 0, thresh_changed, 0);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(metric), mag);
    g_signal_handlers_unblock_matched(metric, G_SIGNAL_MATCH_FUNC,
                                      0, 0, 0, thresh_changed, 0);
    gtk_adjustment_set_value(adj, val/pow10(mag));
}

static void
width_changed(GtkAdjustment *adj,
              TipBlindControls *controls)
{
    TipBlindArgs *args;
    gdouble v;

    args = controls->args;
    v = gtk_adjustment_get_value(adj);
    args->xres = GWY_ROUND(v);
    if (controls->in_update)
        return;

    if (args->same_resolution) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres), v);
        controls->in_update = FALSE;
    }

    tip_update(controls);
}

static void
height_changed(GtkAdjustment *adj,
               TipBlindControls *controls)
{
    TipBlindArgs *args;
    gdouble v;

    args = controls->args;
    v = gtk_adjustment_get_value(adj);
    args->yres = GWY_ROUND(v);
    if (controls->in_update)
        return;

    if (args->same_resolution) {
        controls->in_update = TRUE;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xres), v);
        controls->in_update = FALSE;
    }

    tip_update(controls);
}

static void
thresh_changed(G_GNUC_UNUSED gpointer object,
               TipBlindControls *controls)
{
    gdouble val;
    gint p10;

    val = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold));
    p10 = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(controls->threshold_unit));
    controls->args->thresh = val * pow10(p10);
}

static void
bound_changed(GtkToggleButton *button,
              TipBlindArgs *args)
{
    args->use_boundaries
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
}

static void
same_resolution_changed(GtkToggleButton *button,
                        TipBlindControls *controls)
{
    TipBlindArgs *args;

    args = controls->args;
    args->same_resolution
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));

    if (!args->same_resolution)
        return;

    controls->in_update = TRUE;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yres), args->xres);
    controls->in_update = FALSE;

    tip_update(controls);
}

static void
data_changed(GwyDataChooser *chooser,
             GwyDataObjectId *object)
{
    object->data = gwy_data_chooser_get_active(chooser, &object->id);
}

static void
split_to_stripes_changed(GtkToggleButton *toggle,
                         TipBlindControls *controls)
{
    gboolean sens = gtk_toggle_button_get_active(toggle);
    controls->args->split_to_stripes = sens;
    gwy_table_hscale_set_sensitive(controls->stripeno, sens);
    gtk_widget_set_sensitive(controls->create_images, sens);
    gtk_widget_set_sensitive(controls->plot_size_graph, sens);
    tip_update(controls);
}

static void
nstripes_changed(GtkAdjustment *adj,
                 TipBlindControls *controls)
{
    TipBlindArgs *args = controls->args;
    args->nstripes = gwy_adjustment_get_int(adj);
}

static void
stripeno_changed(G_GNUC_UNUSED GtkAdjustment *adj,
                 TipBlindControls *controls)
{
    tip_update(controls);
}

static void
create_images_changed(GtkToggleButton *toggle,
                      TipBlindControls *controls)
{
    gboolean value = gtk_toggle_button_get_active(toggle);
    controls->args->create_images = value;
}

static void
plot_size_graph_changed(GtkToggleButton *toggle,
                        TipBlindControls *controls)
{
    gboolean value = gtk_toggle_button_get_active(toggle);
    controls->args->plot_size_graph = value;
}

static gboolean
tip_blind_source_filter(GwyContainer *data,
                        gint id,
                        gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *source, *orig;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    source = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    orig = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    return !gwy_data_field_check_compatibility(source, orig,
                                               GWY_DATA_COMPATIBILITY_MEASURE
                                               | GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE);
}

static void
reset(TipBlindControls *controls, TipBlindArgs *args)
{
    GwyGraphModel *gmodel;

    gwy_data_field_clear(controls->tip);
    if (args->stripetips) {
        guint i;
        for (i = 0; i < controls->oldnstripes; i++)
            gwy_data_field_clear(args->stripetips[i]);
    }
    controls->good_tip = FALSE;
    gmodel = gwy_graph_model_new();
    gwy_graph_set_model(GWY_GRAPH(controls->graph), gmodel);
    g_object_unref(gmodel);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      GTK_RESPONSE_OK, controls->good_tip);
    tip_update(controls);
}

static gboolean
prepare_fields(GwyDataField *tipfield,
               GwyDataField *surface,
               gint xres, gint yres)
{
    gint xoldres, yoldres;

    /*set real sizes corresponding to actual data*/
    gwy_data_field_set_xreal(tipfield,
                             gwy_data_field_get_xmeasure(surface)
                             *gwy_data_field_get_xres(tipfield));
    gwy_data_field_set_yreal(tipfield,
                             gwy_data_field_get_ymeasure(surface)
                             *gwy_data_field_get_yres(tipfield));

    /*if user has changed tip size, change it*/
    if ((xres != gwy_data_field_get_xres(tipfield))
        || (yres != gwy_data_field_get_yres(tipfield))) {
        xoldres = gwy_data_field_get_xres(tipfield);
        yoldres = gwy_data_field_get_yres(tipfield);
        gwy_data_field_resample(tipfield, xres, yres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_set_xreal(tipfield,
                                 gwy_data_field_get_xreal(tipfield)
                                 /xoldres*xres);
        gwy_data_field_set_yreal(tipfield,
                                 gwy_data_field_get_yreal(tipfield)
                                 /yoldres*yres);
        gwy_data_field_clear(tipfield);
        return FALSE;
    }

    return TRUE;
}

static void
prepare_stripe_fields(TipBlindControls *controls,
                      gboolean keep)
{
    TipBlindArgs *args = controls->args;
    guint i;

    if (!args->split_to_stripes) {
        free_stripe_results(args);
        return;
    }

    if (!keep || args->nstripes != controls->oldnstripes)
        free_stripe_results(args);

    if (!args->stripetips) {
        args->stripetips = g_new(GwyDataField*, args->nstripes);
        args->goodtip = g_new0(gboolean, args->nstripes);
        /* This can potentially initialise all the stripe tips from the global
         * one which is probably what we want. */
        for (i = 0; i < args->nstripes; i++)
            args->stripetips[i] = gwy_data_field_duplicate(controls->tip);
    }

    for (i = 0; i < args->nstripes; i++) {
        gwy_data_field_set_xreal(args->stripetips[i], controls->tip->xreal);
        gwy_data_field_set_yreal(args->stripetips[i], controls->tip->yreal);
    }
}

static void
tip_blind_run(TipBlindControls *controls,
              TipBlindArgs *args,
              gboolean full)
{
    typedef GwyDataField* (*TipFunc)(GwyDataField *tip,
                                     GwyDataField *surface,
                                     gdouble threshold,
                                     gboolean use_edges,
                                     gint *count,
                                     GwySetFractionFunc set_fraction,
                                     GwySetMessageFunc set_message);
    GwyDataField *surface;
    GwyGraphModel *gmodel;
    TipFunc tipfunc;
    GQuark quark;
    gint count;
    gboolean keep;

    quark = gwy_app_get_data_key_for_id(args->source.id);
    surface = GWY_DATA_FIELD(gwy_container_get_object(args->source.data,
                                                      quark));
    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Initializing"));

    /* control tip resolution and real/res ratio*/
    keep = prepare_fields(controls->tip, surface, args->xres, args->yres);
    prepare_stripe_fields(controls, keep);
    if (args->split_to_stripes) {
        GtkAdjustment *stripenoadj = GTK_ADJUSTMENT(controls->stripeno);
        gint stripeno;

        gtk_adjustment_set_upper(stripenoadj, args->nstripes);
        stripeno = gwy_adjustment_get_int(stripenoadj);
        if (stripeno > args->nstripes)
            gtk_adjustment_set_value(stripenoadj, stripeno);
    }
    controls->oldnstripes = args->nstripes;

    tipfunc = full ? gwy_tip_estimate_full : gwy_tip_estimate_partial;
    if (args->split_to_stripes) {
        guint ns = args->nstripes;
        guint xres = surface->xres, yres = surface->yres;
        guint anygood = 0, i;
        for (i = 0; i < ns; i++) {
            guint row = i*(yres - args->yres)/ns,
                  height = (i + 1)*(yres - args->yres)/ns + args->yres - row;
            gboolean ok;
            GwyDataField *stripe;
            gchar *prefix;

            /* TRANSLATORS: Prefix for the progressbar message. */
            prefix = g_strdup_printf(_("Stripe %u: "), i+1);
            gwy_app_wait_set_message_prefix(prefix);
            g_free(prefix);

            /* Do not crash in the silly case. */
            if (height < args->yres)
                continue;

            gwy_debug("[%u] (%u, %u) of %u", i, row, height, yres);
            count = -1;
            stripe = gwy_data_field_area_extract(surface, 0, row, xres, height);
            ok = !!tipfunc(args->stripetips[i], stripe,
                           args->thresh, args->use_boundaries,
                           &count,
                           gwy_app_wait_set_fraction,
                           gwy_app_wait_set_message);
            args->goodtip[i] = ok && count > 0;
            g_object_unref(stripe);
            gwy_debug("[%u] count = %d", i, count);
            anygood |= args->goodtip[i];
            /* Cancelled by user */
            if (!ok) {
                anygood = FALSE;
                gwy_clear(args->goodtip, ns);
                break;
            }
        }
        gmodel = size_plot(args);
        gwy_graph_set_model(GWY_GRAPH(controls->graph), gmodel);
        g_object_unref(gmodel);
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, anygood);
    }
    else {
        count = -1;
        controls->good_tip = (tipfunc(controls->tip, surface,
                                      args->thresh, args->use_boundaries,
                                      &count,
                                      gwy_app_wait_set_fraction,
                                      gwy_app_wait_set_message)
                              && count > 0);
        gwy_debug("count = %d", count);
        gmodel = gwy_graph_model_new();
        gwy_graph_set_model(GWY_GRAPH(controls->graph), gmodel);
        g_object_unref(gmodel);
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                          GTK_RESPONSE_OK, controls->good_tip);
    }

    gwy_app_wait_finish();
    tip_update(controls);
}

static void
tip_update(TipBlindControls *controls)
{
    GwyDataField *vtipfield, *tipfield, *buffer;
    TipBlindArgs *args = controls->args;
    GtkAdjustment *adj = GTK_ADJUSTMENT(controls->stripeno);
    guint stripeno;

    tipfield = controls->tip;
    stripeno = gwy_adjustment_get_int(adj);
    if (args->split_to_stripes && args->stripetips
        && stripeno > 0 && stripeno <= controls->oldnstripes
        && args->stripetips[stripeno]) {
        tipfield = args->stripetips[stripeno-1];
    }

    buffer = gwy_data_field_duplicate(tipfield);
    gwy_data_field_resample(buffer, controls->vxres, controls->vyres,
                            GWY_INTERPOLATION_ROUND);

    vtipfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->vtip,
                                                                "/0/data"));

    gwy_data_field_copy(buffer, vtipfield, FALSE);
    g_object_unref(buffer);
    gwy_data_field_data_changed(vtipfield);
}

static void
tip_blind_do(TipBlindControls *controls,
             TipBlindArgs *args)
{
    if (args->split_to_stripes && args->stripetips) {
        if (args->create_images)
            tip_blind_do_images(controls, args);
        if (args->plot_size_graph)
            tip_blind_do_size_plot(controls, args);
    }
    else
        tip_blind_do_single(controls, args);
}

static void
tip_blind_do_single(TipBlindControls *controls,
                    TipBlindArgs *args)
{
    gint newid;

    newid = gwy_app_data_browser_add_data_field(controls->tip,
                                                args->source.data, TRUE);
    g_object_unref(controls->tip);
    gwy_app_sync_data_items(args->source.data, args->source.data,
                            0, newid, FALSE,
                            GWY_DATA_ITEM_GRADIENT, 0);
    gwy_app_set_data_field_title(args->source.data, newid, _("Estimated tip"));
    gwy_app_channel_log_add_proc(args->source.data, -1, newid);
    controls->tipdone = TRUE;
}

static void
tip_blind_do_images(TipBlindControls *controls,
                    TipBlindArgs *args)
{
    gint newid, i;

    for (i = 0; i < args->nstripes; i++) {
        gchar *title;
        gchar key[24];

        if (!args->goodtip[i] || !args->stripetips[i])
            continue;

        newid = gwy_app_data_browser_add_data_field(args->stripetips[i],
                                                    args->source.data, TRUE);
        gwy_app_sync_data_items(args->source.data, args->source.data,
                                0, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        title = g_strdup_printf("%s %u/%u",
                                _("Estimated tip"), i+1, args->nstripes);
        g_snprintf(key, sizeof(key), "/%d/data/title", newid);
        gwy_container_set_string_by_name(args->source.data, key, title);
        gwy_app_channel_log_add_proc(args->source.data, -1, newid);
    }

    /* XXX: Have no idea what this means. */
    controls->tipdone = TRUE;
}

static void
tip_blind_do_size_plot(TipBlindControls *controls,
                       TipBlindArgs *args)
{
    GwyGraphModel *gmodel = size_plot(args);
    gwy_app_data_browser_add_graph_model(gmodel, args->source.data, TRUE);
    g_object_unref(gmodel);

    /* XXX: Have no idea what this means. */
    controls->tipdone = TRUE;
}

static void
free_stripe_results(TipBlindArgs *args)
{
    if (args->stripetips) {
        guint i;
        for (i = 0; i < args->nstripes; i++) {
            g_object_unref(args->stripetips);
        }
        g_free(args->stripetips);
        args->stripetips = NULL;
    }
    if (args->goodtip) {
        g_free(args->goodtip);
        args->goodtip = NULL;
    }
}

static gdouble
standardize_direction(gdouble phi)
{
    phi = fmod(phi, G_PI);
    if (phi <= -G_PI/2.0)
        phi += G_PI;
    if (phi > G_PI/2.0)
        phi -= G_PI;
    return phi;
}

static guint
calc_quadratic_curvatue(GwyCurvatureParams *curvature,
                        gdouble a, gdouble bx, gdouble by,
                        gdouble cxx, gdouble cxy, gdouble cyy)
{
    /* At least one quadratic term */
    gdouble cm = cxx - cyy;
    gdouble cp = cxx + cyy;
    gdouble phi = 0.5*atan2(cxy, cm);
    gdouble cx = cp + hypot(cm, cxy);
    gdouble cy = cp - hypot(cm, cxy);
    gdouble bx1 = bx*cos(phi) + by*sin(phi);
    gdouble by1 = -bx*sin(phi) + by*cos(phi);
    guint degree = 2;
    gdouble xc, yc;

    /* Eliminate linear terms */
    if (fabs(cx) < 1e-10*fabs(cy)) {
        /* Only y quadratic term */
        xc = 0.0;
        yc = -by1/cy;
        degree = 1;
    }
    else if (fabs(cy) < 1e-10*fabs(cx)) {
        /* Only x quadratic term */
        xc = -bx1/cx;
        yc = 0.0;
        degree = 1;
    }
    else {
        /* Two quadratic terms */
        xc = -bx1/cx;
        yc = -by1/cy;
    }

    curvature->xc = xc*cos(phi) - yc*sin(phi);
    curvature->yc = xc*sin(phi) + yc*cos(phi);
    curvature->zc = a + xc*bx1 + yc*by1 + 0.5*(xc*xc*cx + yc*yc*cy);

    if (cx > cy) {
        GWY_SWAP(gdouble, cx, cy);
        phi += G_PI/2.0;
    }

    curvature->k1 = cx;
    curvature->k2 = cy;
    curvature->phi1 = phi;
    curvature->phi2 = phi + G_PI/2.0;

    return degree;
}

static guint
math_curvature_at_origin(const gdouble *coeffs,
                         GwyCurvatureParams *curvature)
{
    gdouble a = coeffs[0], bx = coeffs[1], by = coeffs[2],
            cxx = coeffs[3], cxy = coeffs[4], cyy = coeffs[5];
    gdouble b, beta;
    guint degree;

    /* Eliminate the mixed term */
    if (fabs(cxx) + fabs(cxy) + fabs(cyy) <= 1e-10*(fabs(bx) + fabs(by))) {
        /* Linear gradient */
        gwy_clear(curvature, 1);
        curvature->phi2 = G_PI/2.0;
        curvature->zc = a;
        return 0;
    }

    b = hypot(bx, by);
    beta = atan2(by, bx);
    if (b > 1e-10) {
        gdouble cosbeta = bx/b,
                sinbeta = by/b,
                cbeta2 = cosbeta*cosbeta,
                sbeta2 = sinbeta*sinbeta,
                csbeta = cosbeta*sinbeta,
                qb = hypot(1.0, b);
        gdouble cxx1 = (cxx*cbeta2 + cxy*csbeta + cyy*sbeta2)/(qb*qb*qb),
                cxy1 = (2.0*(cyy - cxx)*csbeta + cxy*(cbeta2 - sbeta2))/(qb*qb),
                cyy1 = (cyy*cbeta2 - cxy*csbeta + cxx*sbeta2)/qb;
        cxx = cxx1;
        cxy = cxy1;
        cyy = cyy1;
    }
    else
        beta = 0.0;

    degree = calc_quadratic_curvatue(curvature, a, 0, 0, cxx, cxy, cyy);

    curvature->phi1 = standardize_direction(curvature->phi1 + beta);
    curvature->phi2 = standardize_direction(curvature->phi2 + beta);
    // This should already hold approximately.  Enforce it exactly.
    curvature->xc = curvature->yc = 0.0;
    curvature->zc = a;

    return degree;
}

static void
tip_curvatures(GwyDataField *tipfield,
               gdouble *pc1,
               gdouble *pc2)
{
    gint xres = tipfield->xres, yres = tipfield->yres;
    gdouble R = 2 + 0.25*log(xres*yres);
    gdouble sx2 = 0.0, sy2 = 0.0, sx4 = 0.0, sx2y2 = 0.0, sy4 = 0.0;
    gdouble sz = 0.0, szx = 0.0, szy = 0.0, szx2 = 0.0, szxy = 0.0, szy2 = 0.0;
    gdouble scale = sqrt(tipfield->xreal*tipfield->yreal/(xres*yres))*R;
    gdouble xc = 0.5*xres - 0.5, yc = 0.5*yres - 0.5;
    gdouble a[21], b[6];
    gint i, j, n = 0;
    GwyCurvatureParams params;

    for (i = 0; i < yres; i++) {
        gdouble y = (2.0*i + 1.0 - yres)/yres*tipfield->yreal/scale;
        for (j = 0; j < xres; j++) {
            gdouble x = (2.0*j + 1.0 - xres)/xres*tipfield->xreal/scale;
            gdouble z = tipfield->data[i*xres + j]/scale;
            gdouble rr = (i - yc)*(i - yc) + (j - xc)*(j - xc);
            gdouble xx = x*x, yy = y*y;

            /* Exclude also the central pixel – unreliable. */
            if (rr > R*R || rr < 1e-6)
                continue;

            sx2 += xx;
            sx2y2 += xx*yy;
            sy2 += yy;
            sx4 += xx*xx;
            sy4 += yy*yy;

            sz += z;
            szx += x*z;
            szy += y*z;
            szx2 += xx*z;
            szxy += x*y*z;
            szy2 += yy*z;
            n++;
        }
    }

    gwy_clear(a, 21);
    a[0] = n;
    a[2] = a[6] = sx2;
    a[5] = a[15] = sy2;
    a[18] = a[14] = sx2y2;
    a[9] = sx4;
    a[20] = sy4;
    if (gwy_math_choleski_decompose(6, a)) {
        b[0] = sz;
        b[1] = szx;
        b[2] = szy;
        b[3] = szx2;
        b[4] = szxy;
        b[5] = szy2;
        gwy_math_choleski_solve(6, a, b);
    }
    else {
        *pc1 = *pc2 = 0.0;
        return;
    }

    math_curvature_at_origin(b, &params);
    *pc1 = params.k1/scale;
    *pc2 = params.k2/scale;
}

static GwyGraphModel*
size_plot(TipBlindArgs *args)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *unit, *dunit;
    GwyDataField *surface;
    gdouble *xdata, *ydata;
    gint i, ngood = 0;
    guint ns = args->nstripes;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(args->source.id);
    surface = GWY_DATA_FIELD(gwy_container_get_object(args->source.data,
                                                      quark));

    xdata = g_new(gdouble, ns);
    ydata = g_new(gdouble, ns);

    for (i = 0; i < ns; i++) {
        guint row = i*(surface->yres - args->yres)/ns,
              height = ((i + 1)*(surface->yres - args->yres)/ns
                        + args->yres - row);
        gdouble y = (row + 0.5*height)*gwy_data_field_get_ymeasure(surface);
        gdouble k1, k2;

        if (!args->goodtip[i] || !args->stripetips[i])
            continue;

        tip_curvatures(args->stripetips[i], &k1, &k2);
        if (k1 == 0.0 || k2 == 0.0)
            continue;

        xdata[ngood] = y;
        /* The tip image is upside down, make curvatures positive. */
        ydata[ngood] = -2.0/(k1 + k2);
        ngood++;
    }

    gcmodel = gwy_graph_curve_model_new();
    g_object_set(gcmodel,
                 "description", _("Tip radius evolution"),
                 NULL);
    gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, ngood);

    g_free(xdata);
    g_free(ydata);

    gmodel = gwy_graph_model_new();
    g_object_set(gmodel,
                 "title", _("Tip radius evolution"),
                 NULL);
    unit = gwy_data_field_get_si_unit_xy(surface);

    dunit = gwy_si_unit_duplicate(unit);
    g_object_set(gmodel,
                 "si-unit-x", dunit,
                 "axis-label-bottom", "y",
                 NULL);
    g_object_unref(dunit);

    dunit = gwy_si_unit_duplicate(unit);
    g_object_set(gmodel,
                 "si-unit-y", dunit,
                 "axis-label-left", "r",
                 NULL);
    g_object_unref(dunit);

    gwy_graph_model_add_curve(gmodel, gcmodel);
    g_object_unref(gcmodel);

    return gmodel;
}

static const gchar xres_key[]             = "/module/tip_blind/xres";
static const gchar yres_key[]             = "/module/tip_blind/yres";
static const gchar thresh_key[]           = "/module/tip_blind/threshold";
static const gchar use_boundaries_key[]   = "/module/tip_blind/use_boundaries";
static const gchar same_resolution_key[]  = "/module/tip_blind/same_resolution";
static const gchar split_to_stripes_key[] = "/module/tip_blind/split_to_stripes";
static const gchar create_images_key[]    = "/module/tip_blind/create_images";
static const gchar plot_size_graph_key[]  = "/module/tip_blind/plot_size_graph";
static const gchar nstripes_key[]         = "/module/tip_blind/nstripes";

static void
tip_blind_sanitize_args(TipBlindArgs *args)
{
    args->xres = CLAMP(args->xres, MIN_RES, MAX_RES);
    args->yres = CLAMP(args->yres, MIN_RES, MAX_RES);
    args->nstripes = CLAMP(args->nstripes, MIN_STRIPES, MAX_STRIPES);
    args->use_boundaries = !!args->use_boundaries;
    args->same_resolution = !!args->same_resolution;
    args->split_to_stripes = !!args->split_to_stripes;
    args->create_images = !!args->create_images;
    args->plot_size_graph = !!args->plot_size_graph;
    if (args->same_resolution)
        args->yres = args->xres;
}

static void
tip_blind_load_args(GwyContainer *container,
                    TipBlindArgs *args)
{
    *args = tip_blind_defaults;

    gwy_container_gis_int32_by_name(container, xres_key, &args->xres);
    gwy_container_gis_int32_by_name(container, yres_key, &args->yres);
    gwy_container_gis_int32_by_name(container, nstripes_key, &args->nstripes);
    gwy_container_gis_double_by_name(container, thresh_key, &args->thresh);
    gwy_container_gis_boolean_by_name(container, use_boundaries_key,
                                      &args->use_boundaries);
    gwy_container_gis_boolean_by_name(container, same_resolution_key,
                                      &args->same_resolution);
    gwy_container_gis_boolean_by_name(container, split_to_stripes_key,
                                      &args->split_to_stripes);
    gwy_container_gis_boolean_by_name(container, create_images_key,
                                      &args->create_images);
    gwy_container_gis_boolean_by_name(container, plot_size_graph_key,
                                      &args->plot_size_graph);
    tip_blind_sanitize_args(args);
}

static void
tip_blind_save_args(GwyContainer *container,
                    TipBlindArgs *args)
{
    gwy_container_set_int32_by_name(container, xres_key, args->xres);
    gwy_container_set_int32_by_name(container, yres_key, args->yres);
    gwy_container_set_int32_by_name(container, nstripes_key, args->nstripes);
    gwy_container_set_double_by_name(container, thresh_key, args->thresh);
    gwy_container_set_boolean_by_name(container, use_boundaries_key,
                                      args->use_boundaries);
    gwy_container_set_boolean_by_name(container, same_resolution_key,
                                      args->same_resolution);
    gwy_container_set_boolean_by_name(container, split_to_stripes_key,
                                      args->split_to_stripes);
    gwy_container_set_boolean_by_name(container, create_images_key,
                                      args->create_images);
    gwy_container_set_boolean_by_name(container, plot_size_graph_key,
                                      args->plot_size_graph);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
