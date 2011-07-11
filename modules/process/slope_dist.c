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
#include <libgwyddion/gwymath.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define SLOPE_DIST_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    MAX_OUT_SIZE = 1024
};

typedef enum {
    SLOPE_DIST_2D_DIST,
    SLOPE_DIST_GRAPH_PHI,
    SLOPE_DIST_GRAPH_THETA,
    SLOPE_DIST_LAST
} SlopeOutput;

typedef struct {
    SlopeOutput output_type;
    gint size;
    gboolean logscale;
    gboolean fit_plane;
    gint kernel_size;
} SlopeArgs;

typedef struct {
    GSList *output_type_group;
    GtkObject *size;
    GtkWidget *logscale;
    GtkWidget *fit_plane;
    GtkObject *kernel_size;
} SlopeControls;

static gboolean       module_register             (void);
static void           slope_dist                  (GwyContainer *data,
                                                   GwyRunType run);
static gboolean       slope_dialog                (SlopeArgs *args,
                                                   gboolean same_units);
static void           slope_dialog_update_controls(SlopeControls *controls,
                                                   SlopeArgs *args);
static void           slope_dialog_update_values  (SlopeControls *controls,
                                                   SlopeArgs *args);
static void           slope_fit_plane_cb          (GtkToggleButton *check,
                                                   SlopeControls *controls);
static void           slope_output_type_cb        (GtkToggleButton *radio,
                                                   SlopeControls *controls);
static GwyDataField*  slope_do_2d                 (GwyDataField *dfield,
                                                   SlopeArgs *args);
static GwyGraphModel* slope_do_graph_phi          (GwyDataField *dfield,
                                                   SlopeArgs *args);
static GwyGraphModel* slope_do_graph_theta        (GwyDataField *dfield,
                                                   SlopeArgs *args);
static gdouble        compute_slopes              (GwyDataField *dfield,
                                                   gint kernel_size,
                                                   GwyDataField *xder,
                                                   GwyDataField *yder);
static GwyDataField*  make_datafield              (GwyDataField *old,
                                                   gint res,
                                                   gulong *count,
                                                   gdouble real,
                                                   gboolean logscale);
static void           load_args                   (GwyContainer *container,
                                                   SlopeArgs *args);
static void           save_args                   (GwyContainer *container,
                                                   SlopeArgs *args);
static void           sanitize_args               (SlopeArgs *args);

static const SlopeArgs slope_defaults = {
    SLOPE_DIST_2D_DIST,
    200,
    FALSE,
    FALSE,
    5,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates one- or two-dimensional distribution of slopes "
       "or graph of their angular distribution."),
    "Yeti <yeti@gwyddion.net>",
    "1.12",
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
    GwyDataField *dfield;
    GwyGraphModel *gmodel;
    SlopeArgs args;
    gint oldid, newid;
    gboolean ok, same_units;

    g_return_if_fail(run & SLOPE_DIST_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);
    load_args(gwy_app_settings_get(), &args);
    same_units = gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                                   gwy_data_field_get_si_unit_z(dfield));
    if (run == GWY_RUN_INTERACTIVE) {
        if (!same_units && args.output_type == SLOPE_DIST_GRAPH_THETA)
            args.output_type = SLOPE_DIST_GRAPH_PHI;
        ok = slope_dialog(&args, same_units);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    switch (args.output_type) {
        case SLOPE_DIST_2D_DIST:
        dfield = slope_do_2d(dfield, &args);
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        g_object_unref(dfield);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                0);
        gwy_app_set_data_field_title(data, newid, _("Slope distribution"));
        break;

        case SLOPE_DIST_GRAPH_PHI:
        gmodel = slope_do_graph_phi(dfield, &args);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
        break;

        case SLOPE_DIST_GRAPH_THETA:
        gmodel = slope_do_graph_theta(dfield, &args);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
        break;

        default:
        g_return_if_reached();
        break;
    }
}

static gboolean
slope_dialog(SlopeArgs *args, gboolean same_units)
{
    static const GwyEnum output_types[] = {
        { N_("_Two-dimensional distribution"), SLOPE_DIST_2D_DIST,     },
        { N_("Directional (φ) _graph"),        SLOPE_DIST_GRAPH_PHI,   },
        { N_("_Inclination (θ) graph"),        SLOPE_DIST_GRAPH_THETA, },
    };
    GtkWidget *dialog, *table, *label;
    SlopeControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Slope Distribution"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(8, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.output_type_group
        = gwy_radio_buttons_create(output_types, G_N_ELEMENTS(output_types),
                                   G_CALLBACK(slope_output_type_cb),
                                   &controls,
                                   args->output_type);
    row = gwy_radio_buttons_attach_to_table(controls.output_type_group,
                                            GTK_TABLE(table), 4, row);
    if (!same_units) {
        GtkWidget *radio;

        radio = gwy_radio_buttons_find(controls.output_type_group,
                                       SLOPE_DIST_GRAPH_THETA);
        gtk_widget_set_sensitive(radio, FALSE);
    }

    controls.size = gtk_adjustment_new(args->size, 10, MAX_OUT_SIZE, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("Output _size:"), "px",
                            controls.size, 0);
    row++;

    controls.logscale
        = gtk_check_button_new_with_mnemonic(_("_Logarithmic value scale"));
    gtk_table_attach(GTK_TABLE(table), controls.logscale,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.fit_plane
        = gtk_check_button_new_with_mnemonic(_("Use local plane _fitting"));
    gtk_table_attach(GTK_TABLE(table), controls.fit_plane,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.fit_plane, "toggled",
                     G_CALLBACK(slope_fit_plane_cb), &controls);
    row++;

    controls.kernel_size = gtk_adjustment_new(args->kernel_size,
                                              2, 16, 1, 4, 0);
    gwy_table_attach_hscale(table, row, _("_Plane size:"), "px",
                            controls.kernel_size, 0);
    row++;

    slope_dialog_update_controls(&controls, args);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            slope_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = slope_defaults;
            slope_dialog_update_controls(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    slope_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
slope_dialog_update_controls(SlopeControls *controls,
                             SlopeArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size),
                             args->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->kernel_size),
                             args->kernel_size);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->logscale),
                                 args->logscale);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->fit_plane),
                                 args->fit_plane);
    gwy_table_hscale_set_sensitive(controls->kernel_size, args->fit_plane);
    gtk_widget_set_sensitive(controls->logscale,
                             args->output_type == SLOPE_DIST_2D_DIST);
    gwy_radio_buttons_set_current(controls->output_type_group,
                                  args->output_type);
}

static void
slope_dialog_update_values(SlopeControls *controls,
                           SlopeArgs *args)
{
    args->size = gwy_adjustment_get_int(controls->size);
    args->kernel_size = gwy_adjustment_get_int(controls->kernel_size);
    args->logscale =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->logscale));
    args->fit_plane =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->fit_plane));
    args->output_type =
        gwy_radio_buttons_get_current(controls->output_type_group);
}

static void
slope_fit_plane_cb(GtkToggleButton *check,
                   SlopeControls *controls)
{
    gwy_table_hscale_set_sensitive(controls->kernel_size,
                                   gtk_toggle_button_get_active(check));
}

static void
slope_output_type_cb(GtkToggleButton *button,
                     SlopeControls *controls)
{
    SlopeOutput otype;

    if (!gtk_toggle_button_get_active(button))
        return;

    otype = gwy_radio_buttons_get_current(controls->output_type_group);
    gtk_widget_set_sensitive(controls->logscale, otype == SLOPE_DIST_2D_DIST);
}

static GwyDataField*
slope_do_2d(GwyDataField *dfield,
            SlopeArgs *args)
{
    GwyDataField *xder, *yder;
    const gdouble *xd, *yd;
    gdouble max;
    gint xres, yres, n;
    gint xider, yider, i;
    gulong *count;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = gwy_data_field_new_alike(dfield, FALSE);
    yder = gwy_data_field_new_alike(dfield, FALSE);
    max = compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0,
                         xder, yder);
    count = g_new0(gulong, args->size*args->size);
    xd = gwy_data_field_get_data_const(xder);
    yd = gwy_data_field_get_data_const(yder);
    for (i = 0; i < n; i++) {
        xider = args->size*(xd[i]/(2.0*max) + 0.5);
        xider = CLAMP(xider, 0, args->size-1);
        yider = args->size*(yd[i]/(2.0*max) + 0.5);
        yider = CLAMP(yider, 0, args->size-1);

        count[yider*args->size + xider]++;
    }
    g_object_unref(yder);
    g_object_unref(xder);

    return make_datafield(dfield, args->size, count, 2.0*max, args->logscale);
}

static GwyGraphModel*
slope_do_graph_phi(GwyDataField *dfield,
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
        gdouble phi = fmod(atan2(yd[i], -xd[i]) + 2*G_PI, 2*G_PI);
        gdouble d = (xd[i]*xd[i] + yd[i]*yd[i]);
        gint iphi;

        iphi = floor(args->size*phi/(2.0*G_PI));
        iphi = CLAMP(iphi, 0, args->size-1);
        data[iphi] += d;
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
                     SlopeArgs *args)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel;
    GwyDataLine *dataline;
    GwySIUnit *siunitx, *siunity;
    GwyDataField *xder, *yder;
    const gdouble *yd;
    gdouble *xd, *data;
    gint xres, yres, n, i;
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
    max = gwy_data_field_get_max(xder);
    gwy_data_line_set_real(dataline, max);
    for (i = 0; i < n; i++) {
        gint itheta;

        itheta = floor(args->size*xd[i]/max);
        itheta = CLAMP(itheta, 0, args->size-1);
        data[itheta] += args->size/(n*max);
    }
    g_object_unref(xder);

    gmodel = gwy_graph_model_new();
    siunitx = gwy_si_unit_new("deg");
    siunity = gwy_si_unit_new("deg^-1");
    g_object_set(gmodel,
                 "title", _("Inclination Distribution"),
                 "si-unit-x", siunitx,
                 "si-unit-y", siunity,
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

static gdouble
compute_slopes(GwyDataField *dfield,
               gint kernel_size,
               GwyDataField *xder,
               GwyDataField *yder)
{
    gint xres, yres;
    gdouble minxd, maxxd, minyd, maxyd;

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
    }
    else {
        gint col, row;
        gdouble *xd, *yd;
        const gdouble *data;
        gdouble d;

        data = gwy_data_field_get_data_const(dfield);
        xd = gwy_data_field_get_data(xder);
        yd = gwy_data_field_get_data(yder);
        for (row = 0; row < yres; row++) {
            for (col = 0; col < xres; col++) {
                if (!col)
                    d = data[row*xres + col + 1] - data[row*xres + col];
                else if (col == xres-1)
                    d = data[row*xres + col] - data[row*xres + col - 1];
                else
                    d = (data[row*xres + col + 1]
                         - data[row*xres + col - 1])/2;
                *(xd++) = d;

                if (!row)
                    d = data[row*xres + xres + col] - data[row*xres + col];
                else if (row == yres-1)
                    d = data[row*xres + col] - data[row*xres - xres + col];
                else
                    d = (data[row*xres + xres + col]
                         - data[row*xres - xres + col])/2;
                *(yd++) = d;
            }
        }
    }

    gwy_data_field_multiply(xder, xres/gwy_data_field_get_xreal(dfield));
    gwy_data_field_get_min_max(xder, &minxd, &maxxd);
    maxxd = MAX(fabs(minxd), fabs(maxxd));
    gwy_data_field_multiply(yder, yres/gwy_data_field_get_yreal(dfield));
    gwy_data_field_get_min_max(yder, &minyd, &maxyd);
    maxyd = MAX(fabs(minyd), fabs(maxyd));

    return MAX(maxxd, maxyd);
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
static const gchar kernel_size_key[] = "/module/slope_dist/kernel_size";

static void
sanitize_args(SlopeArgs *args)
{
    args->output_type = MIN(args->output_type, SLOPE_DIST_LAST-1);
    args->size = CLAMP(args->size, 1, MAX_OUT_SIZE);
    args->kernel_size = CLAMP(args->kernel_size, 2, 16);
    args->logscale = !!args->logscale;
    args->fit_plane = !!args->fit_plane;
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
    gwy_container_gis_int32_by_name(container, kernel_size_key,
                                    &args->kernel_size);
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
    gwy_container_set_int32_by_name(container, kernel_size_key,
                                    args->kernel_size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
