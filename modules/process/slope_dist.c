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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define SLOPE_DIST_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400,
    MAX_OUT_SIZE = 4096,
    RESPONSE_PREVIEW = 2,
};

typedef enum {
    SLOPE_DIST_2D_DIST,
    SLOPE_DIST_GRAPH_PHI,
    SLOPE_DIST_GRAPH_THETA,
    SLOPE_DIST_GRAPH_GRADIENT,
    SLOPE_DIST_LAST
} SlopeOutput;

typedef struct {
    SlopeOutput output_type;
    gint size;
    gboolean logscale;
    gboolean fit_plane;
    gboolean update;
    gint kernel_size;
    GwyMaskingType masking;
} SlopeArgs;

typedef struct {
    SlopeArgs *args;
    GwyContainer *mydata;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *graph;
    GSList *output_type;
    GtkObject *size;
    GtkWidget *logscale;
    GtkWidget *fit_plane;
    GtkWidget *update;
    GtkObject *kernel_size;
    GSList *masking;

    GwyDataField *dfield;
    GwyDataField *mfield;
    gboolean in_init;
} SlopeControls;

static gboolean       module_register        (void);
static void           slope_dist             (GwyContainer *data,
                                              GwyRunType run);
static gboolean       slope_dialog           (SlopeArgs *args,
                                              gboolean same_units,
                                              GwyContainer *data,
                                              GwyDataField *dfield,
                                              GwyDataField *mfield,
                                              gint id);
static void           update_controls        (SlopeControls *controls,
                                              SlopeArgs *args);
static void           fit_plane_changed      (SlopeControls *controls,
                                              GtkToggleButton *check);
static void           logscale_changed       (SlopeControls *controls,
                                              GtkToggleButton *check);
static void           size_changed           (SlopeControls *controls,
                                              GtkAdjustment *adj);
static void           kernel_size_changed    (SlopeControls *controls,
                                              GtkAdjustment *adj);
static void           output_type_changed    (GtkToggleButton *radio,
                                              SlopeControls *controls);
static void           update_changed         (SlopeControls *controls,
                                              GtkToggleButton *check);
static void           masking_changed        (GtkToggleButton *button,
                                              SlopeControls *controls);
static void           slope_invalidate       (SlopeControls *controls);
static void           preview                (SlopeControls *controls);
static GwyDataField*  slope_do_2d            (GwyDataField *dfield,
                                              GwyDataField *mfield,
                                              SlopeArgs *args);
static GwyGraphModel* slope_do_graph_phi     (GwyDataField *dfield,
                                              GwyDataField *mfield,
                                              SlopeArgs *args);
static GwyGraphModel* slope_do_graph_theta   (GwyDataField *dfield,
                                              GwyDataField *mfield,
                                              SlopeArgs *args);
static GwyGraphModel* slope_do_graph_gradient(GwyDataField *dfield,
                                              GwyDataField *mfield,
                                              SlopeArgs *args);
static void           compute_slopes         (GwyDataField *dfield,
                                              gint kernel_size,
                                              GwyDataField *xder,
                                              GwyDataField *yder);
static GwyDataField*  make_datafield         (GwyDataField *old,
                                              gint res,
                                              gulong *count,
                                              gdouble real,
                                              gboolean logscale);
static void           load_args              (GwyContainer *container,
                                              SlopeArgs *args);
static void           save_args              (GwyContainer *container,
                                              SlopeArgs *args);
static void           sanitize_args          (SlopeArgs *args);

static const SlopeArgs slope_defaults = {
    SLOPE_DIST_2D_DIST,
    200,
    FALSE,
    FALSE,
    TRUE,
    5,
    GWY_MASK_IGNORE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates one- or two-dimensional distribution of slopes "
       "or graph of their angular distribution."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("slope_dist",
                              (GwyProcessFunc)&slope_dist,
                              N_("/_Statistics/_Slope Distribution..."),
                              NULL,
                              SLOPE_DIST_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Calculate angular slope distribution"));

    return TRUE;
}

static void
slope_dist(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    GwyGraphModel *gmodel;
    SlopeArgs args;
    gint oldid, newid;
    gboolean ok, same_units;

    g_return_if_fail(run & SLOPE_DIST_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield);
    load_args(gwy_app_settings_get(), &args);
    same_units = gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                                   gwy_data_field_get_si_unit_z(dfield));

    if (run == GWY_RUN_INTERACTIVE) {
        if (!same_units && (args.output_type == SLOPE_DIST_GRAPH_THETA))
            args.output_type = SLOPE_DIST_GRAPH_GRADIENT;
        ok = slope_dialog(&args, same_units, data, dfield, mfield, oldid);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    switch (args.output_type) {
        case SLOPE_DIST_2D_DIST:
        dfield = slope_do_2d(dfield, mfield, &args);
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        g_object_unref(dfield);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
        gwy_app_set_data_field_title(data, newid, _("Slope distribution"));
        gwy_app_channel_log_add(data, oldid, newid, "proc::slope_dist", NULL);
        break;

        case SLOPE_DIST_GRAPH_PHI:
        gmodel = slope_do_graph_phi(dfield, mfield, &args);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
        break;

        case SLOPE_DIST_GRAPH_THETA:
        gmodel = slope_do_graph_theta(dfield, mfield, &args);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
        break;

        case SLOPE_DIST_GRAPH_GRADIENT:
        gmodel = slope_do_graph_gradient(dfield, mfield, &args);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static gboolean
slope_dialog(SlopeArgs *args, gboolean same_units,
             GwyContainer *data,
             GwyDataField *dfield, GwyDataField *mfield, gint id)
{
    static const GwyEnum output_types[] = {
        { N_("_Two-dimensional distribution"), SLOPE_DIST_2D_DIST,        },
        { N_("Directional (φ) _graph"),        SLOPE_DIST_GRAPH_PHI,      },
        { N_("_Inclination (θ) graph"),        SLOPE_DIST_GRAPH_THETA,    },
        { N_("Inclination (gra_dient) graph"), SLOPE_DIST_GRAPH_GRADIENT, },
    };
    GtkWidget *dialog, *table, *label, *hbox, *vbox;
    GwyGraphModel *gmodel;
    GwyPixmapLayer *layer;
    SlopeControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;
    gint row;

    controls.args = args;
    controls.dfield = dfield;
    controls.mfield = mfield;
    controls.in_init = TRUE;

    dialog = gtk_dialog_new_with_buttons(_("Slope Distribution"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_new(PREVIEW_SIZE, PREVIEW_SIZE, 1.0, 1.0, TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    g_object_unref(dfield);
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

    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);
    if (args->output_type != SLOPE_DIST_2D_DIST)
        gtk_widget_set_no_show_all(controls.view, TRUE);

    gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(gmodel);
    gtk_widget_set_size_request(controls.graph, PREVIEW_SIZE, -1);
    g_object_unref(gmodel);
    gtk_box_pack_start(GTK_BOX(vbox), controls.graph, TRUE, TRUE, 0);
    if (args->output_type == SLOPE_DIST_2D_DIST)
        gtk_widget_set_no_show_all(controls.graph, TRUE);

    table = gtk_table_new(10 + (mfield ? 4 : 0), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.output_type
        = gwy_radio_buttons_create(output_types, G_N_ELEMENTS(output_types),
                                   G_CALLBACK(output_type_changed),
                                   &controls,
                                   args->output_type);
    row = gwy_radio_buttons_attach_to_table(controls.output_type,
                                            GTK_TABLE(table), 4, row);
    if (!same_units) {
        GtkWidget *radio;

        radio = gwy_radio_buttons_find(controls.output_type,
                                       SLOPE_DIST_GRAPH_THETA);
        gtk_widget_set_sensitive(radio, FALSE);
    }

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.size = gtk_adjustment_new(args->size, 10, MAX_OUT_SIZE, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("Output _size:"), "px",
                            controls.size, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.size, "value-changed",
                             G_CALLBACK(size_changed), &controls);
    row++;

    controls.logscale
        = gtk_check_button_new_with_mnemonic(_("_Logarithmic value scale"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.logscale),
                                 args->logscale);
    gtk_table_attach(GTK_TABLE(table), controls.logscale,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.logscale, "toggled",
                             G_CALLBACK(logscale_changed), &controls);
    row++;

    controls.fit_plane
        = gtk_check_button_new_with_mnemonic(_("Use local plane _fitting"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.fit_plane),
                                 args->fit_plane);
    gtk_table_attach(GTK_TABLE(table), controls.fit_plane,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.fit_plane, "toggled",
                             G_CALLBACK(fit_plane_changed), &controls);
    row++;

    controls.kernel_size = gtk_adjustment_new(args->kernel_size,
                                              2, 16, 1, 4, 0);
    gwy_table_attach_hscale(table, row, _("_Plane size:"), "px",
                            controls.kernel_size, 0);
    g_signal_connect_swapped(controls.kernel_size, "value-changed",
                             G_CALLBACK(kernel_size_changed), &controls);
    row++;

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_changed), &controls);
    row++;

    if (mfield) {
        gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
        label = gwy_label_new_header(_("Masking Mode"));
        gtk_table_attach(GTK_TABLE(table), label,
                        0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        controls.masking
            = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                       G_CALLBACK(masking_changed), &controls,
                                       args->masking);
        row = gwy_radio_buttons_attach_to_table(controls.masking,
                                                GTK_TABLE(table), 3, row);
    }
    else
        controls.masking = NULL;

    output_type_changed(GTK_TOGGLE_BUTTON(controls.output_type), &controls);
    fit_plane_changed(&controls, GTK_TOGGLE_BUTTON(controls.fit_plane));
    if (args->update) {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls.dialog),
                                          RESPONSE_PREVIEW, FALSE);
        preview(&controls);
    }
    controls.in_init = FALSE;

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
            controls.in_init = TRUE;
            *args = slope_defaults;
            update_controls(&controls, args);
            controls.in_init = FALSE;
            slope_invalidate(&controls);
            break;

            case RESPONSE_PREVIEW:
            preview(&controls);
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
update_controls(SlopeControls *controls, SlopeArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             args->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->kernel_size),
                             args->kernel_size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->logscale),
                                 args->logscale);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->fit_plane),
                                 args->fit_plane);
    gwy_radio_buttons_set_current(controls->output_type, args->output_type);
    if (controls->masking)
        gwy_radio_buttons_set_current(controls->masking, args->masking);
}

static void
fit_plane_changed(SlopeControls *controls, GtkToggleButton *check)
{
    controls->args->fit_plane = gtk_toggle_button_get_active(check);
    gwy_table_hscale_set_sensitive(controls->kernel_size,
                                   controls->args->fit_plane);
    slope_invalidate(controls);
}

static void
logscale_changed(SlopeControls *controls, GtkToggleButton *check)
{
    controls->args->logscale = gtk_toggle_button_get_active(check);
    slope_invalidate(controls);
}

static void
size_changed(SlopeControls *controls, GtkAdjustment *adj)
{
    controls->args->size = gwy_adjustment_get_int(adj);
    slope_invalidate(controls);
}

static void
kernel_size_changed(SlopeControls *controls, GtkAdjustment *adj)
{
    controls->args->kernel_size = gwy_adjustment_get_int(adj);
    slope_invalidate(controls);
}

static void
output_type_changed(GtkToggleButton *button, SlopeControls *controls)
{
    SlopeOutput otype;

    otype = gwy_radio_buttons_get_current(controls->output_type);
    controls->args->output_type = otype;
    gtk_widget_set_sensitive(controls->logscale, otype == SLOPE_DIST_2D_DIST);
    if (otype == SLOPE_DIST_2D_DIST) {
        gtk_widget_set_no_show_all(controls->graph, TRUE);
        gtk_widget_set_no_show_all(controls->view, FALSE);
        gtk_widget_hide(controls->graph);
        gtk_widget_show_all(controls->view);
    }
    else {
        gtk_widget_set_no_show_all(controls->view, TRUE);
        gtk_widget_set_no_show_all(controls->graph, FALSE);
        gtk_widget_hide(controls->view);
        gtk_widget_show_all(controls->graph);
    }

    if (!gtk_toggle_button_get_active(button))
        return;

    slope_invalidate(controls);
}

static void
update_changed(SlopeControls *controls, GtkToggleButton *check)
{
    controls->args->update = gtk_toggle_button_get_active(check);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);
    slope_invalidate(controls);
}

static void
masking_changed(GtkToggleButton *button, SlopeControls *controls)
{
    GwyMaskingType masking;

    masking = gwy_radio_buttons_get_current(controls->masking);
    controls->args->masking = masking;

    if (!gtk_toggle_button_get_active(button))
        return;

    slope_invalidate(controls);
}

static void
slope_invalidate(SlopeControls *controls)
{
    if (controls->in_init || !controls->args->update)
        return;

    preview(controls);
}

static void
preview(SlopeControls *controls)
{
    SlopeArgs *args = controls->args;
    GwyGraphModel *gmodel;

    if (args->output_type == SLOPE_DIST_2D_DIST) {
        GwyDataField *dfield = slope_do_2d(controls->dfield, controls->mfield,
                                           args);

        gwy_container_set_object_by_name(controls->mydata, "/0/data", dfield);
        g_object_unref(dfield);
        gwy_set_data_preview_size(GWY_DATA_VIEW(controls->view), PREVIEW_SIZE);
        return;
    }

    if (args->output_type == SLOPE_DIST_GRAPH_PHI)
        gmodel = slope_do_graph_phi(controls->dfield, controls->mfield, args);
    else if (args->output_type == SLOPE_DIST_GRAPH_THETA)
        gmodel = slope_do_graph_theta(controls->dfield, controls->mfield, args);
    else if (args->output_type == SLOPE_DIST_GRAPH_GRADIENT)
        gmodel = slope_do_graph_gradient(controls->dfield, controls->mfield,
                                         args);
    else {
        g_return_if_reached();
    }

    gwy_graph_set_model(GWY_GRAPH(controls->graph), gmodel);
    g_object_unref(gmodel);
}

static inline gboolean
is_counted(GwyDataField *mfield, guint k, GwyMaskingType masking)
{
    if (!mfield || masking == GWY_MASK_IGNORE)
        return TRUE;

    if (masking == GWY_MASK_INCLUDE)
        return mfield->data[k] > 0.0;
    else
        return mfield->data[k] <= 0.0;
}

static GwyDataField*
slope_do_2d(GwyDataField *dfield,
            GwyDataField *mfield,
            SlopeArgs *args)
{
    GwyDataField *xder, *yder;
    const gdouble *xd, *yd;
    gdouble minxd, maxxd, minyd, maxyd, max;
    gint xres, yres, n;
    gint xider, yider, i;
    gulong *count;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = gwy_data_field_new_alike(dfield, FALSE);
    yder = gwy_data_field_new_alike(dfield, FALSE);
    compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0, xder, yder);

    gwy_data_field_get_min_max(xder, &minxd, &maxxd);
    maxxd = MAX(fabs(minxd), fabs(maxxd));
    gwy_data_field_get_min_max(yder, &minyd, &maxyd);
    maxyd = MAX(fabs(minyd), fabs(maxyd));
    max = MAX(maxxd, maxyd);
    if (!max) {
        max = 1.0;
    }

    count = g_new0(gulong, args->size*args->size);
    xd = gwy_data_field_get_data_const(xder);
    yd = gwy_data_field_get_data_const(yder);
    for (i = 0; i < n; i++) {
        if (is_counted(mfield, i, args->masking)) {
            xider = args->size*(xd[i]/(2.0*max) + 0.5);
            xider = CLAMP(xider, 0, args->size-1);
            yider = args->size*(yd[i]/(2.0*max) + 0.5);
            yider = CLAMP(yider, 0, args->size-1);

            count[yider*args->size + xider]++;
        }
    }
    g_object_unref(yder);
    g_object_unref(xder);

    return make_datafield(dfield, args->size, count, 2.0*max, args->logscale);
}

static GwyGraphModel*
slope_do_graph_phi(GwyDataField *dfield,
                   GwyDataField *mfield,
                   SlopeArgs *args)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel;
    GwyDataLine *dataline;
    GwySIUnit *siunitx, *siunity;
    GwyDataField *xder, *yder;
    const gdouble *xd, *yd;
    gdouble *data;
    gint xres, yres, n, i;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = gwy_data_field_new_alike(dfield, FALSE);
    yder = gwy_data_field_new_alike(dfield, FALSE);
    compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0, xder, yder);

    dataline = gwy_data_line_new(args->size, 360, TRUE);
    data = gwy_data_line_get_data(dataline);
    xd = gwy_data_field_get_data_const(xder);
    yd = gwy_data_field_get_data_const(yder);
    for (i = 0; i < n; i++) {
        if (is_counted(mfield, i, args->masking)) {
            gdouble phi = fmod(atan2(yd[i], -xd[i]) + 2*G_PI, 2*G_PI);
            gdouble d = (xd[i]*xd[i] + yd[i]*yd[i]);
            gint iphi = floor(args->size*phi/(2.0*G_PI));

            iphi = CLAMP(iphi, 0, args->size-1);
            data[iphi] += d;
        }
    }
    g_object_unref(yder);
    g_object_unref(xder);

    gmodel = gwy_graph_model_new();
    siunitx = gwy_si_unit_new("deg");
    siunity = gwy_si_unit_divide(gwy_data_field_get_si_unit_z(dfield),
                                 gwy_data_field_get_si_unit_xy(dfield), NULL);
    gwy_si_unit_power(siunity, 2, siunity);
    g_object_set(gmodel,
                 "title", _("Angular Slope Distribution"),
                 "si-unit-x", siunitx,
                 "si-unit-y", siunity,
                 "axis-label-bottom", "φ",
                 "axis-label-left", "w",
                 NULL);
    g_object_unref(siunity);
    g_object_unref(siunitx);

    cmodel = gwy_graph_curve_model_new();
    g_object_set(cmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", _("Slopes"),
                 NULL);
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    g_object_unref(dataline);
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    return gmodel;
}

static GwyGraphModel*
slope_do_graph_theta(GwyDataField *dfield,
                     GwyDataField *mfield,
                     SlopeArgs *args)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel;
    GwyDataLine *dataline;
    GwySIUnit *siunitx, *siunity;
    GwyDataField *xder, *yder;
    const gdouble *yd;
    gdouble *xd, *data;
    gint xres, yres, n, i, nc;
    gdouble max;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = gwy_data_field_new_alike(dfield, FALSE);
    yder = gwy_data_field_new_alike(dfield, FALSE);
    compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0, xder, yder);

    dataline = gwy_data_line_new(args->size, 90, TRUE);
    data = gwy_data_line_get_data(dataline);
    xd = gwy_data_field_get_data(xder);
    yd = gwy_data_field_get_data_const(yder);
    for (i = 0; i < n; i++)
        xd[i] = 180.0/G_PI*atan(hypot(xd[i], yd[i]));
    g_object_unref(yder);
    gwy_data_field_area_get_min_max_mask(xder, mfield, args->masking,
                                         0, 0, xres, yres, NULL, &max);
    gwy_data_line_set_real(dataline, max);
    nc = 0;
    for (i = 0; i < n; i++) {
        if (is_counted(mfield, i, args->masking)) {
            gint itheta = floor(args->size*xd[i]/max);

            itheta = CLAMP(itheta, 0, args->size-1);
            data[itheta]++;
            nc++;
        }
    }
    g_object_unref(xder);

    if (nc && max)
        gwy_data_line_multiply(dataline, args->size/(nc*max));

    gmodel = gwy_graph_model_new();
    siunitx = gwy_si_unit_new("deg");
    siunity = gwy_si_unit_power(siunitx, -1, NULL);
    g_object_set(gmodel,
                 "title", _("Inclination Distribution"),
                 "si-unit-x", siunitx,
                 "si-unit-y", siunity,
                 "axis-label-bottom", "θ",
                 "axis-label-left", "ρ",
                 NULL);
    g_object_unref(siunity);
    g_object_unref(siunitx);

    cmodel = gwy_graph_curve_model_new();
    g_object_set(cmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", _("Inclinations"),
                 NULL);
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    g_object_unref(dataline);
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    return gmodel;
}

static GwyGraphModel*
slope_do_graph_gradient(GwyDataField *dfield,
                        GwyDataField *mfield,
                        SlopeArgs *args)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel;
    GwyDataLine *dataline;
    GwySIUnit *siunitx, *siunity;
    GwyDataField *xder, *yder;
    const gdouble *yd;
    gdouble *xd, *data;
    gint xres, yres, n, i, nc;
    gdouble max;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = gwy_data_field_new_alike(dfield, FALSE);
    yder = gwy_data_field_new_alike(dfield, FALSE);
    compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0, xder, yder);

    xd = gwy_data_field_get_data(xder);
    yd = gwy_data_field_get_data_const(yder);
    for (i = 0; i < n; i++)
        xd[i] = hypot(xd[i], yd[i]);
    g_object_unref(yder);
    gwy_data_field_area_get_min_max_mask(xder, mfield, args->masking,
                                         0, 0, xres, yres, NULL, &max);

    dataline = gwy_data_line_new(args->size, max, TRUE);
    data = gwy_data_line_get_data(dataline);
    nc = 0;
    for (i = 0; i < n; i++) {
        if (is_counted(mfield, i, args->masking)) {
            gint ider = floor(args->size*xd[i]/max);

            ider = CLAMP(ider, 0, args->size-1);
            data[ider]++;
            nc++;
        }
    }
    g_object_unref(xder);

    if (nc && max)
        gwy_data_line_multiply(dataline, args->size/(nc*max));

    gmodel = gwy_graph_model_new();
    siunitx = gwy_si_unit_divide(gwy_data_field_get_si_unit_z(dfield),
                                 gwy_data_field_get_si_unit_xy(dfield),
                                 NULL);
    siunity = gwy_si_unit_power(siunitx, -1, NULL);
    g_object_set(gmodel,
                 "title", _("Inclination Distribution"),
                 "si-unit-x", siunitx,
                 "si-unit-y", siunity,
                 "axis-label-bottom", "η",
                 "axis-label-left", "ρ",
                 NULL);
    g_object_unref(siunity);
    g_object_unref(siunitx);

    cmodel = gwy_graph_curve_model_new();
    g_object_set(cmodel,
                 "mode", GWY_GRAPH_CURVE_LINE,
                 "description", _("Inclinations"),
                 NULL);
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    g_object_unref(dataline);
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    return gmodel;
}

static void
compute_slopes(GwyDataField *dfield,
               gint kernel_size,
               GwyDataField *xder,
               GwyDataField *yder)
{
    gint xres, yres;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    if (kernel_size) {
        GwyPlaneFitQuantity quantites[] = {
            GWY_PLANE_FIT_BX, GWY_PLANE_FIT_BY
        };
        GwyDataField *fields[2];

        fields[0] = xder;
        fields[1] = yder;
        gwy_data_field_fit_local_planes(dfield, kernel_size,
                                        2, quantites, fields);
        gwy_data_field_multiply(xder, xres/gwy_data_field_get_xreal(dfield));
        gwy_data_field_multiply(yder, yres/gwy_data_field_get_yreal(dfield));
    }
    else
        gwy_data_field_filter_slope(dfield, xder, yder);
}

static GwyDataField*
make_datafield(GwyDataField *old,
               gint res, gulong *count,
               gdouble real, gboolean logscale)
{
    GwyDataField *dfield;
    GwySIUnit *unit;
    gdouble *d;
    gint i;

    dfield = gwy_data_field_new(res, res, real, real, FALSE);
    gwy_data_field_set_xoffset(dfield, -real/2);
    gwy_data_field_set_yoffset(dfield, -real/2);

    unit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    unit = gwy_si_unit_divide(gwy_data_field_get_si_unit_z(old),
                              gwy_data_field_get_si_unit_xy(old),
                              NULL);
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    d = gwy_data_field_get_data(dfield);
    if (logscale) {
        for (i = 0; i < res*res; i++)
            d[i] = count[i] ? log((gdouble)count[i]) + 1.0 : 0.0;
    }
    else {
        for (i = 0; i < res*res; i++)
            d[i] = count[i];
    }
    g_free(count);

    return dfield;
}

static const gchar output_type_key[] = "/module/slope_dist/output_type";
static const gchar size_key[]        = "/module/slope_dist/size";
static const gchar logscale_key[]    = "/module/slope_dist/logscale";
static const gchar fit_plane_key[]   = "/module/slope_dist/fit_plane";
static const gchar update_key[]      = "/module/slope_dist/update";
static const gchar kernel_size_key[] = "/module/slope_dist/kernel_size";
static const gchar masking_key[]     = "/module/slope_dist/masking";

static void
sanitize_args(SlopeArgs *args)
{
    args->output_type = MIN(args->output_type, SLOPE_DIST_LAST-1);
    args->size = CLAMP(args->size, 1, MAX_OUT_SIZE);
    args->kernel_size = CLAMP(args->kernel_size, 2, 16);
    args->logscale = !!args->logscale;
    args->fit_plane = !!args->fit_plane;
    args->update = !!args->update;
    args->masking = MIN(args->masking, GWY_MASK_IGNORE);
}

static void
load_args(GwyContainer *container,
          SlopeArgs *args)
{
    *args = slope_defaults;

    gwy_container_gis_enum_by_name(container, output_type_key,
                                   &args->output_type);
    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    gwy_container_gis_boolean_by_name(container, logscale_key, &args->logscale);
    gwy_container_gis_boolean_by_name(container, fit_plane_key,
                                      &args->fit_plane);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_int32_by_name(container, kernel_size_key,
                                    &args->kernel_size);
    gwy_container_gis_enum_by_name(container, masking_key, &args->masking);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          SlopeArgs *args)
{
    gwy_container_set_enum_by_name(container, output_type_key,
                                   args->output_type);
    gwy_container_set_int32_by_name(container, size_key, args->size);
    gwy_container_set_boolean_by_name(container, logscale_key, args->logscale);
    gwy_container_set_boolean_by_name(container, fit_plane_key,
                                      args->fit_plane);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_int32_by_name(container, kernel_size_key,
                                    args->kernel_size);
    gwy_container_set_enum_by_name(container, masking_key, args->masking);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
