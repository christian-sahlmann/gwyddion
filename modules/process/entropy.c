/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/level.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define ENTROPY_ENT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

#define ENTROPY_NORMAL 1.41893853320467274178l
#define ENTROPY_NORMAL_2D 2.144729885849400174l

enum {
    RESPONSE_PREVIEW = 2,
};

typedef enum {
    ENTROPY_VALUES = 0,
    ENTROPY_SLOPES = 1,
    ENTROPY_ANGLES = 2,
    ENTROPY_NMODES
} EntropyMode;

typedef struct {
    EntropyMode mode;
    GwyMaskingType masking;
    gboolean zoom_in;
    gboolean fit_plane;
    gint kernel_size;
} EntropyArgs;

typedef struct {
    EntropyArgs *args;
    GwyContainer *mydata;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *graph;
    GSList *mode;
    GSList *masking;
    GtkWidget *fit_plane;
    GtkObject *kernel_size;
    GtkWidget *zoom_in;
    GtkWidget *entropy;
    GtkWidget *entropydef;

    GwyDataField *dfield;
    GwyDataField *mfield;
} EntropyControls;

static gboolean module_register    (void);
static void     entropy            (GwyContainer *data,
                                    GwyRunType run);
static gboolean entropy_dialog     (EntropyArgs *args,
                                    GwyDataField *dfield,
                                    GwyDataField *mfield);
static void     update_sensitivity (EntropyControls *controls);
static void     mode_changed       (GtkToggleButton *check,
                                    EntropyControls *controls);
static void     masking_changed    (GtkToggleButton *toggle,
                                    EntropyControls *controls);
static void     fit_plane_changed  (EntropyControls *controls,
                                    GtkToggleButton *check);
static void     zoom_in_changed    (EntropyControls *controls,
                                    GtkToggleButton *check);
static void     kernel_size_changed(EntropyControls *controls,
                                    GtkAdjustment *adj);
static void     preview            (EntropyControls *controls);
static void     compute_slopes     (GwyDataField *dfield,
                                    gint kernel_size,
                                    GwyDataField *xder,
                                    GwyDataField *yder);
static void     load_args          (GwyContainer *container,
                                    EntropyArgs *args);
static void     save_args          (GwyContainer *container,
                                    EntropyArgs *args);
static void     sanitize_args      (EntropyArgs *args);

static const EntropyArgs slope_defaults = {
    ENTROPY_VALUES, GWY_MASK_IGNORE, FALSE, TRUE, 3,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Visualises entropy calculation for value and slope distribution."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("entropy",
                              (GwyProcessFunc)&entropy,
                              N_("/_Statistics/_Entropy..."),
                              NULL,
                              ENTROPY_ENT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Calculate entropy of value and "
                                 "slope distributions"));

    return TRUE;
}

static void
entropy(G_GNUC_UNUSED GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    EntropyArgs args;
    gboolean ok, same_units;

    g_return_if_fail(run & ENTROPY_ENT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);
    g_return_if_fail(dfield);
    load_args(gwy_app_settings_get(), &args);
    same_units = gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                                   gwy_data_field_get_si_unit_z(dfield));

    if (run == GWY_RUN_INTERACTIVE) {
        if (!same_units && args.mode == ENTROPY_ANGLES)
           args.mode = ENTROPY_SLOPES;
        ok = entropy_dialog(&args, dfield, mfield);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
}

static gboolean
entropy_dialog(EntropyArgs *args, GwyDataField *dfield, GwyDataField *mfield)
{
    GtkWidget *dialog, *table, *label, *hbox;
    GwyGraphModel *gmodel;
    EntropyControls controls;
    gint response;
    gint row;

    controls.args = args;
    controls.dfield = dfield;
    controls.mfield = mfield;

    dialog = gtk_dialog_new_with_buttons(_("Entropy"), NULL, 0,
                                         NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 12);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       TRUE, TRUE, 0);

    table = gtk_table_new(8 + 4*(!!mfield), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 0);
    row = 0;

    controls.mode
        = gwy_radio_buttons_createl(G_CALLBACK(mode_changed), &controls,
                                    args->mode,
                                    _("Value distribution"),
                                    ENTROPY_VALUES,
                                    _("Slope derivative distribution"),
                                    ENTROPY_SLOPES,
                                    _("Slope angle distribution"),
                                    ENTROPY_ANGLES,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.mode,
                                            GTK_TABLE(table), 3, row);
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

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
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

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.zoom_in
        = gtk_check_button_new_with_mnemonic(_("_Zoom graph around estimate"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.zoom_in),
                                 args->zoom_in);
    gtk_table_attach(GTK_TABLE(table), controls.zoom_in,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.zoom_in, "toggled",
                             G_CALLBACK(zoom_in_changed), &controls);
    row++;

    label = gtk_label_new(_("Entropy:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.entropy = label;
    row++;

    label = gtk_label_new(_("Entropy deficit:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.entropydef = label;
    row++;

    update_sensitivity(&controls);

    gmodel = gwy_graph_model_new();
    controls.graph = gwy_graph_new(gmodel);
    g_object_unref(gmodel);
    gtk_widget_set_size_request(controls.graph, 400, 320);
    gtk_box_pack_start(GTK_BOX(hbox), controls.graph, TRUE, TRUE, 0);

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
update_sensitivity(EntropyControls *controls)
{
    GwyDataField *dfield = controls->dfield;
    gboolean is_slope = (controls->args->mode != ENTROPY_VALUES);

    if (!gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                          gwy_data_field_get_si_unit_z(dfield))) {
        GtkWidget *button = gwy_radio_buttons_find(controls->mode,
                                                   ENTROPY_ANGLES);
        gtk_widget_set_sensitive(button, FALSE);
    }

    gtk_widget_set_sensitive(controls->fit_plane, is_slope);
    gwy_table_hscale_set_sensitive(controls->kernel_size,
                                   is_slope && controls->args->fit_plane);
}

static void
mode_changed(GtkToggleButton *toggle, EntropyControls *controls)
{
    if (!gtk_toggle_button_get_active(toggle))
        return;

    controls->args->mode = gwy_radio_buttons_get_current(controls->mode);
    update_sensitivity(controls);
}

static void
masking_changed(GtkToggleButton *toggle, EntropyControls *controls)
{
    if (!gtk_toggle_button_get_active(toggle))
        return;

    controls->args->masking = gwy_radio_buttons_get_current(controls->masking);
}

static void
fit_plane_changed(EntropyControls *controls, GtkToggleButton *check)
{
    controls->args->fit_plane = gtk_toggle_button_get_active(check);
    update_sensitivity(controls);
}

static void
zoom_in_changed(EntropyControls *controls, GtkToggleButton *check)
{
    GwyGraphModel *gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    GwyGraphCurveModel *gcmodel;
    const gdouble *xdata, *ydata;
    gdouble S;
    guint ndata, i;

    g_object_set(gmodel,
                 "x-min-set", FALSE,
                 "x-max-set", FALSE,
                 "y-min-set", FALSE,
                 "y-max-set", FALSE,
                 NULL);

    if (!(controls->args->zoom_in = gtk_toggle_button_get_active(check))
        || (gwy_graph_model_get_n_curves(gmodel) < 2)) {
        return;
    }

    gcmodel = gwy_graph_model_get_curve(gmodel, 1);
    ydata = gwy_graph_curve_model_get_ydata(gcmodel);
    S = ydata[0];

    gcmodel = gwy_graph_model_get_curve(gmodel, 0);
    ndata = gwy_graph_curve_model_get_ndata(gcmodel);
    if (ndata < 5)
        return;

    xdata = gwy_graph_curve_model_get_xdata(gcmodel);
    ydata = gwy_graph_curve_model_get_ydata(gcmodel);
    for (i = 1; i+1 < ndata; i++) {
        if (ydata[i] > S - G_LN2) {
            g_object_set(gmodel,
                         "x-min", xdata[i-1],
                         "x-min-set", TRUE,
                         "y-min", ydata[i-1],
                         "y-min-set", TRUE,
                         NULL);
            break;
        }
    }
    for (i = ndata-2; i; i--) {
        if (ydata[i] < S + G_LN2) {
            g_object_set(gmodel,
                         "x-max", xdata[i+1],
                         "x-max-set", TRUE,
                         "y-max", ydata[i+1],
                         "y-max-set", TRUE,
                         NULL);
            break;
        }
    }
}

static void
kernel_size_changed(EntropyControls *controls, GtkAdjustment *adj)
{
    controls->args->kernel_size = gwy_adjustment_get_int(adj);
}

/* This does not transform to spherical (theta,phi) but to a planar coordinate
 * system with unit |J| so the entropy should be preserved.  It is the same
 * transformation as in facet analysis. */
static void
transform_to_sphere(GwyDataField *xder, GwyDataField *yder)
{
    gdouble *xdata = gwy_data_field_get_data(xder);
    gdouble *ydata = gwy_data_field_get_data(yder);
    guint i, n = gwy_data_field_get_xres(xder)*gwy_data_field_get_yres(xder);

    for (i = 0; i < n; i++) {
        gdouble x = xdata[i], y = ydata[i];
        gdouble r2 = x*x + y*y;

        if (r2 > 0.0) {
            gdouble s_r = G_SQRT2*sqrt((1.0 - 1.0/sqrt(1.0 + r2))/r2);
            xdata[i] *= s_r;
            ydata[i] *= s_r;
        }
    }
}

static gdouble
calculate_sigma2_2d(GwyDataField *xfield, GwyDataField *yfield)
{
    gdouble xc = gwy_data_field_get_avg(xfield);
    gdouble yc = gwy_data_field_get_avg(yfield);
    const gdouble *xdata = gwy_data_field_get_data(xfield);
    const gdouble *ydata = gwy_data_field_get_data(yfield);
    gdouble s2 = 0.0;
    guint n, i;

    n = gwy_data_field_get_xres(xfield)*gwy_data_field_get_yres(xfield);
    for (i = 0; i < n; i++)
        s2 += (xdata[i] - xc)*(xdata[i] - xc) + (ydata[i] - yc)*(ydata[i] - yc);

    return s2/n;
}

static GwyDataField*
fake_mask(GwyDataField *dfield, GwyDataField *mask, GwyMaskingType masking)
{
    GwyDataField *masked;
    gint xres = gwy_data_field_get_xres(dfield);
    gint yres = gwy_data_field_get_yres(dfield);
    const gdouble *d, *m;
    gdouble *md;
    gint i, n;

    if (!mask || masking == GWY_MASK_IGNORE)
        return dfield;

    gwy_data_field_area_count_in_range(mask, NULL, 0, 0, xres, yres,
                                       G_MAXDOUBLE, 1.0, NULL, &n);
    if (masking == GWY_MASK_EXCLUDE)
        n = xres*yres - n;

    if (n == xres*yres)
        return dfield;

    masked = gwy_data_field_new(n, 1, n, 1.0, FALSE);
    md = gwy_data_field_get_data(masked);
    d = gwy_data_field_get_data_const(dfield);
    m = gwy_data_field_get_data_const(mask);
    n = 0;
    for (i = 0; i < xres*yres; i++) {
        gboolean mi = (m[i] >= 1.0);
        if ((mi && masking == GWY_MASK_INCLUDE)
            || (!mi && masking == GWY_MASK_EXCLUDE))
            md[n++] = d[i];
    }
    g_object_unref(dfield);

    return masked;
}

static void
preview(EntropyControls *controls)
{
    EntropyArgs *args = controls->args;
    GwyDataField *dfield = controls->dfield;
    GwyDataField *mfield = controls->mfield;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GwyDataLine *ecurve;
    gchar buf[24];
    gdouble S, s, Smax = 0.0;

    ecurve = gwy_data_line_new(1, 1.0, FALSE);
    if (args->mode == ENTROPY_VALUES) {
        S = gwy_data_field_area_get_entropy_at_scales(dfield, ecurve,
                                                      mfield, args->masking,
                                                      0, 0,
                                                      dfield->xres,
                                                      dfield->yres,
                                                      0);
        s = gwy_data_field_area_get_rms_mask(dfield,
                                             mfield, args->masking,
                                             0, 0, dfield->xres, dfield->yres);
        Smax = ENTROPY_NORMAL + log(s);
    }
    else {
        GwyDataField *xder = gwy_data_field_new_alike(dfield, FALSE);
        GwyDataField *yder = gwy_data_field_new_alike(dfield, FALSE);

        compute_slopes(controls->dfield,
                       args->fit_plane ? args->kernel_size : 0, xder, yder);
        xder = fake_mask(xder, mfield, args->masking);
        yder = fake_mask(yder, mfield, args->masking);
        if (args->mode == ENTROPY_ANGLES)
            transform_to_sphere(xder, yder);

        S = gwy_data_field_get_entropy_2d_at_scales(xder, yder, ecurve, 0);
        if (args->mode == ENTROPY_SLOPES) {
            s = calculate_sigma2_2d(xder, yder);
            Smax = ENTROPY_NORMAL_2D + log(s);
        }

        g_object_unref(xder);
        g_object_unref(yder);
    }

    g_snprintf(buf, sizeof(buf), "%g", S);
    gtk_label_set_text(GTK_LABEL(controls->entropy), buf);

    if (args->mode != ENTROPY_ANGLES) {
        g_snprintf(buf, sizeof(buf), "%g", Smax - S);
        gtk_label_set_text(GTK_LABEL(controls->entropydef), buf);
    }
    else
        gtk_label_set_text(GTK_LABEL(controls->entropydef), _("N.A."));

    gmodel = gwy_graph_get_model(GWY_GRAPH(controls->graph));
    gwy_graph_model_remove_all_curves(gmodel);
    g_object_set(gmodel,
                 "axis-label-bottom", "log h",
                 "axis-label-left", "S",
                 "label-position", GWY_GRAPH_LABEL_NORTHWEST,
                 NULL);

    if (gwy_data_line_get_min(ecurve) > -0.5*G_MAXDOUBLE) {
        gcmodel = gwy_graph_curve_model_new();
        g_object_set(gcmodel,
                     "description", _("Entropy at scales"),
                     "mode", GWY_GRAPH_CURVE_LINE_POINTS,
                     "color", gwy_graph_get_preset_color(0),
                     NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, ecurve, 0, 0);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }

    if (S > -0.5*G_MAXDOUBLE) {
        GwyDataLine *best = gwy_data_line_duplicate(ecurve);
        gdouble *ydata = gwy_data_line_get_data(best);
        guint i, res = gwy_data_line_get_res(best);

        for (i = 0; i < res; i++)
            ydata[i] = S;

        gcmodel = gwy_graph_curve_model_new();
        g_object_set(gcmodel,
                     "description", _("Best estimate"),
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(1),
                     NULL);
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, best, 0, 0);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        g_object_unref(best);
    }

    g_object_unref(ecurve);

    zoom_in_changed(controls, GTK_TOGGLE_BUTTON(controls->zoom_in));
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

static const gchar fit_plane_key[]   = "/module/entropy/fit_plane";
static const gchar kernel_size_key[] = "/module/entropy/kernel_size";
static const gchar masking_key[]     = "/module/entropy/masking";
static const gchar mode_key[]        = "/module/entropy/mode";
static const gchar zoom_in_key[]     = "/module/entropy/zoom_in";

static void
sanitize_args(EntropyArgs *args)
{
    args->mode = MIN(args->mode, ENTROPY_NMODES-1);
    args->zoom_in = !!args->zoom_in;
    args->fit_plane = !!args->fit_plane;
    args->kernel_size = CLAMP(args->kernel_size, 2, 16);
    args->masking = gwy_enum_sanitize_value(args->masking,
                                            GWY_TYPE_MASKING_TYPE);
}

static void
load_args(GwyContainer *container,
          EntropyArgs *args)
{
    *args = slope_defaults;

    gwy_container_gis_enum_by_name(container, mode_key, &args->mode);
    gwy_container_gis_enum_by_name(container, masking_key, &args->masking);
    gwy_container_gis_boolean_by_name(container, zoom_in_key, &args->zoom_in);
    gwy_container_gis_boolean_by_name(container, fit_plane_key,
                                      &args->fit_plane);
    gwy_container_gis_int32_by_name(container, kernel_size_key,
                                    &args->kernel_size);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          EntropyArgs *args)
{
    gwy_container_set_enum_by_name(container, mode_key, args->mode);
    gwy_container_set_enum_by_name(container, masking_key, args->masking);
    gwy_container_set_boolean_by_name(container, zoom_in_key, args->zoom_in);
    gwy_container_set_boolean_by_name(container, fit_plane_key,
                                      args->fit_plane);
    gwy_container_set_int32_by_name(container, kernel_size_key,
                                    args->kernel_size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
