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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define SLOPE_DIST_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    MAX_OUT_SIZE = 1024
};

typedef enum {
    SLOPE_DIST_2D_DIST,
    SLOPE_DIST_GRAPH,
    SLOPE_DIST_LAST
} SlopeOutput;

/* Data for this function. */
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

static gboolean      module_register              (const gchar *name);
static gboolean      slope_dist                   (GwyContainer *data,
                                                   GwyRunType run);
static gboolean      slope_dialog                 (SlopeArgs *args);
static void          slope_dialog_update_controls (SlopeControls *controls,
                                                   SlopeArgs *args);
static void          slope_dialog_update_values   (SlopeControls *controls,
                                                   SlopeArgs *args);
static void          slope_fit_plane_cb           (GtkToggleButton *check,
                                                   SlopeControls *controls);
static void          slope_output_type_cb         (GObject *radio,
                                                   SlopeControls *controls);
static GwyDataField* slope_do                     (GwyDataField *dfield,
                                                   SlopeArgs *args);
static GtkWidget*    slope_do_graph               (GwyContainer *data,
                                                   SlopeArgs *args);
static gdouble       compute_slopes               (GwyDataField *dfield,
                                                   gint kernel_size,
                                                   gdouble *xder,
                                                   gdouble *yder);
static GwyDataField* make_datafield               (GwyDataField *old,
                                                   gint res,
                                                   gulong *count,
                                                   gdouble real,
                                                   gboolean logscale);
static void          load_args                    (GwyContainer *container,
                                                   SlopeArgs *args);
static void          save_args                    (GwyContainer *container,
                                                   SlopeArgs *args);
static void          sanitize_args                (SlopeArgs *args);

SlopeArgs slope_defaults = {
    SLOPE_DIST_2D_DIST,
    200,
    FALSE,
    FALSE,
    5,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates two-dimensional distribution of slopes "
       "or graph of their angular distribution."),
    "Yeti <yeti@gwyddion.net>",
    "1.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo slope_dist_func_info = {
        "slope_dist",
        N_("/_Statistics/_Slope Distribution..."),
        (GwyProcessFunc)&slope_dist,
        SLOPE_DIST_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &slope_dist_func_info);

    return TRUE;
}

static gboolean
slope_dist(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    SlopeArgs args;
    const gchar *pal;
    gboolean ok;

    g_return_val_if_fail(run & SLOPE_DIST_RUN_MODES, FALSE);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = slope_defaults;
    else
        load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || slope_dialog(&args);
    if (run == GWY_RUN_MODAL)
        save_args(gwy_app_settings_get(), &args);
    if (ok) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        switch (args.output_type) {
            case SLOPE_DIST_2D_DIST:
            pal = gwy_container_get_string_by_name(data, "/0/base/palette");
            dfield = slope_do(dfield, &args);
            data = gwy_container_new();
            gwy_container_set_object_by_name(data, "/0/data", dfield);
            g_object_unref(dfield);
            if (pal)
                gwy_container_set_string_by_name(data, "/0/base/palette",
                                                 g_strdup(pal));

            data_window = gwy_app_data_window_create(data);
            gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                             _("Slope"));
            break;

            case SLOPE_DIST_GRAPH:
            slope_do_graph(data, &args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    }

    return FALSE;
}

static gboolean
slope_dialog(SlopeArgs *args)
{
    static const GwyEnum output_types[] = {
        { N_("_Two-dimensional distribution"), SLOPE_DIST_2D_DIST },
        { N_("Per-angle _graph"),              SLOPE_DIST_GRAPH },
    };
    GtkWidget *dialog, *table, *label;
    GSList *group;
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

    table = gtk_table_new(7, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    label = gtk_label_new(_("Output type"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    group = gwy_radio_buttons_create(output_types, G_N_ELEMENTS(output_types),
                                     "slope-output-type",
                                     G_CALLBACK(slope_output_type_cb),
                                     &controls,
                                     args->output_type);
    controls.output_type_group = group;
    while (group) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(group->data),
                         0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
        row++;
        group = g_slist_next(group);
    }

    controls.size = gtk_adjustment_new(args->size, 10, MAX_OUT_SIZE, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("Output _size:"), "px",
                            controls.size, 0);
    row++;

    controls.logscale
        = gtk_check_button_new_with_mnemonic(_("_Logarithmic value scale"));
    gtk_table_attach(GTK_TABLE(table), controls.logscale,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.fit_plane
        = gtk_check_button_new_with_mnemonic(_("Use local plane _fitting"));
    gtk_table_attach(GTK_TABLE(table), controls.fit_plane,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
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
                             args->output_type != SLOPE_DIST_GRAPH);
    gwy_radio_buttons_set_current(controls->output_type_group,
                                  "slope-output-type", args->output_type);
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
        gwy_radio_buttons_get_current(controls->output_type_group,
                                      "slope-output-type");
}

static void
slope_fit_plane_cb(GtkToggleButton *check,
                   SlopeControls *controls)
{
    gwy_table_hscale_set_sensitive(controls->kernel_size,
                                   gtk_toggle_button_get_active(check));
}

static void
slope_output_type_cb(GObject *radio,
                     SlopeControls *controls)
{
    SlopeOutput otype;

    otype = GPOINTER_TO_UINT(g_object_get_data(radio, "slope-output-type"));
    gtk_widget_set_sensitive(controls->logscale,
                             otype == SLOPE_DIST_2D_DIST);
}

static GwyDataField*
slope_do(GwyDataField *dfield,
         SlopeArgs *args)
{
    gdouble *xder, *yder;
    gdouble max;
    gint xres, yres, n;
    gint xider, yider, i;
    gulong *count;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = g_new(gdouble, n);
    yder = g_new(gdouble, n);
    max = compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0,
                         xder, yder);
    count = g_new0(gulong, args->size*args->size);
    for (i = 0; i < n; i++) {
        xider = args->size*(xder[i]/(2.0*max) + 0.5);
        xider = CLAMP(xider, 0, args->size-1);
        yider = args->size*(yder[i]/(2.0*max) + 0.5);
        yider = CLAMP(yider, 0, args->size-1);

        count[yider*args->size + xider]++;
    }
    g_free(yder);
    g_free(xder);

    return make_datafield(dfield, args->size, count, 2.0*max, args->logscale);
}

static GtkWidget*
slope_do_graph(GwyContainer *container,
               SlopeArgs *args)
{
    GtkWidget *graph;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel;
    GwyDataField *dfield;
    GwyDataLine *dataline;
    GwySIUnit *siunitx;
    gdouble *xder, *yder, *data;
    gint xres, yres, n, i, iphi;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             "/0/data"));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = g_new(gdouble, n);
    yder = g_new(gdouble, n);
    compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0, xder, yder);

    dataline = GWY_DATA_LINE(gwy_data_line_new(args->size, 360, TRUE));
    data = dataline->data;
    for (i = 0; i < n; i++) {
        gdouble phi = fmod(atan2(-yder[i], -xder[i]) + 2*G_PI, 2*G_PI);
        gdouble d = (xder[i]*xder[i] + yder[i]*yder[i]);

        iphi = floor(args->size*phi/2/G_PI);
        iphi = CLAMP(iphi, 0, args->size-1);
        data[iphi] += d;
    }
    g_free(yder);
    g_free(xder);

    gmodel = gwy_graph_model_new();
    gwy_graph_model_set_title(gmodel, _("Angular Slope Distribution"));
    siunitx = gwy_si_unit_divide(gwy_data_field_get_si_unit_z(dfield),
                                 gwy_data_field_get_si_unit_xy(dfield),
                                 NULL);
    gwy_graph_model_set_x_siunit(gmodel, siunitx);
    g_object_unref(siunitx);

    cmodel = gwy_graph_curve_model_new();
    gwy_graph_curve_model_set_description(cmodel, "slopes");
    gwy_graph_curve_model_set_data_from_dataline(cmodel, dataline, 0, 0);
    g_object_unref(dataline);
    gwy_graph_curve_model_set_curve_type(cmodel, GWY_GRAPH_CURVE_LINE);
    gwy_graph_model_add_curve(gmodel, cmodel);
    g_object_unref(cmodel);

    graph = gwy_graph_new(gmodel);
    g_object_unref(gmodel);
    gwy_app_graph_window_create(GWY_GRAPH(graph), container);

    return graph;
}

static gdouble
compute_slopes(GwyDataField *dfield,
               gint kernel_size,
               gdouble *xder,
               gdouble *yder)
{
    const gdouble *data;
    gdouble qx, qy;
    gdouble d, max;
    gint xres, yres;
    gint col, row;

    data = gwy_data_field_get_data_const(dfield);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    qx = xres/gwy_data_field_get_xreal(dfield);
    qy = yres/gwy_data_field_get_yreal(dfield);
    max = 0.0;
    if (kernel_size) {
        for (row = 0; row + kernel_size < yres; row++) {
            for (col = 0; col + kernel_size < xres; col++) {
                gdouble dx, dy;

                gwy_data_field_area_fit_plane(dfield, col, row,
                                              kernel_size, kernel_size,
                                              NULL, &dx, &dy);
                *(xder++) = dx;
                dx = fabs(dx);
                max = MAX(dx, max);

                *(yder++) = dy;
                dy = fabs(dy);
                max = MAX(dy, max);
            }
        }
    }
    else {
        for (row = 1; row + 1 < yres; row++) {
            for (col = 1; col + 1 < xres; col++) {
                d = data[row*xres + col + 1] - data[row*xres + col - 1];
                d *= qx;
                *(xder++) = d;
                d = fabs(d);
                max = MAX(d, max);

                d = data[row*xres + xres + col] - data[row*xres - xres + col];
                d *= qy;
                *(yder++) = d;
                d = fabs(d);
                max = MAX(d, max);
            }
        }
    }

    return max;
}

static GwyDataField*
make_datafield(GwyDataField *old,
               gint res, gulong *count,
               gdouble real, gboolean logscale)
{
    GwyDataField *dfield;
    GwySIUnit *unit, *uz, *uxy;
    gdouble *d;
    gint i;

    dfield = gwy_data_field_new(res, res, real, real, FALSE);

    unit = gwy_si_unit_new("");
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    uz = gwy_data_field_get_si_unit_z(old);
    uxy = gwy_data_field_get_si_unit_xy(old);
    unit = gwy_si_unit_new("");
    gwy_si_unit_divide(uz, uxy, unit);
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

static const gchar *output_type_key = "/module/slope_dist/output_type";
static const gchar *size_key = "/module/slope_dist/size";
static const gchar *logscale_key = "/module/slope_dist/logscale";
static const gchar *fit_plane_key = "/module/slope_dist/fit_plane";
static const gchar *kernel_size_key = "/module/slope_dist/kernel_size";

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
