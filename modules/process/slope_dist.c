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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define SLOPE_DIST_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

typedef enum {
    SLOPE_DIST_2D_DIST,
    SLOPE_DIST_GRAPH
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
    GtkWidget *kernel_size_spin;
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
static GtkWidget*    slope_do_graph               (GwyDataField *dfield,
                                                   SlopeArgs *args);
static void          load_args                    (GwyContainer *container,
                                                   SlopeArgs *args);
static void          save_args                    (GwyContainer *container,
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
    "slope_dist",
    "Slope distribution.",
    "Yeti <yeti@gwyddion.net>",
    "1.2",
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
        "/_Statistics/_Slope distribution...",
        (GwyProcessFunc)&slope_dist,
        SLOPE_DIST_RUN_MODES,
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
    if (ok) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));
        pal = gwy_container_get_string_by_name(data, "/0/base/palette");
        switch (args.output_type) {
            case SLOPE_DIST_2D_DIST:
            dfield = slope_do(dfield, &args);
            data = GWY_CONTAINER(gwy_container_new());
            gwy_container_set_object_by_name(data, "/0/data", G_OBJECT(dfield));
            g_object_unref(dfield);
            gwy_container_set_string_by_name(data, "/0/base/palette",
                                             g_strdup(pal));

            data_window = gwy_app_data_window_create(data);
            gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                             _("Slope"));
            break;

            case SLOPE_DIST_GRAPH:
            slope_do_graph(dfield, &args);
            break;

            default:
            g_assert_not_reached();
            break;
        }

        if (run != GWY_RUN_WITH_DEFAULTS)
            save_args(gwy_app_settings_get(), &args);
    }

    return FALSE;
}

static gboolean
slope_dialog(SlopeArgs *args)
{
    static const GwyEnum output_types[] = {
        { "_Two-dimensional distribution", SLOPE_DIST_2D_DIST },
        { "Per-angle _graph",              SLOPE_DIST_GRAPH },
    };
    GtkWidget *dialog, *table, *spin, *label;
    GSList *group;
    SlopeControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Slope Distribution"), NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(7, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    label = gtk_label_new(_("Output type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    group = gwy_radio_buttons_create(output_types, G_N_ELEMENTS(output_types),
                                     "slope-output-type",
                                     G_CALLBACK(slope_output_type_cb),
                                     &controls,
                                     args->output_type);
    controls.output_type_group = group;
    while (group) {
        gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(group->data),
                         0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
        row++;
        group = g_slist_next(group);
    }

    controls.size = gtk_adjustment_new(args->size, 10, 16384, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("Output size:"), "samples",
                                       controls.size);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

    controls.logscale
        = gtk_check_button_new_with_mnemonic(_("_Logarithmic value scale"));
    gtk_table_attach(GTK_TABLE(table), controls.logscale,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.fit_plane
        = gtk_check_button_new_with_mnemonic(_("Use local plane _fitting"));
    gtk_table_attach(GTK_TABLE(table), controls.fit_plane,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.fit_plane, "toggled",
                     G_CALLBACK(slope_fit_plane_cb), &controls);
    row++;

    controls.kernel_size = gtk_adjustment_new(args->kernel_size,
                                              2, 16, 1, 4, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Plane size:"), "samples",
                                       controls.kernel_size);
    controls.kernel_size_spin = spin;
    row++;

    slope_dialog_update_controls(&controls, args);

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
    gtk_widget_set_sensitive(controls->kernel_size_spin, args->fit_plane);
    gtk_widget_set_sensitive(controls->logscale,
                             args->output_type != SLOPE_DIST_GRAPH);
    gwy_radio_buttons_set_current(controls->output_type_group,
                                  "slope-output-type", args->output_type);
}

static void
slope_dialog_update_values(SlopeControls *controls,
                           SlopeArgs *args)
{
    args->size = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->size));
    args->kernel_size =
        gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->kernel_size));
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
    gtk_widget_set_sensitive(controls->kernel_size_spin,
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
slope_do_graph(GwyDataField *dfield,
               SlopeArgs *args)
{
    GtkWidget *graph;
    GwyGraphAutoProperties prop;
    GwyDataLine *dataline;
    GString *lab;
    gdouble *xder, *yder, *data;;
    gint xres, yres, n, i, iphi;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);

    n = args->fit_plane ? args->kernel_size : 2;
    n = (xres - n)*(yres - n);
    xder = g_new(gdouble, n);
    yder = g_new(gdouble, n);
    compute_slopes(dfield, args->fit_plane ? args->kernel_size : 0, xder, yder);

    graph = gwy_graph_new();
    gwy_graph_get_autoproperties(GWY_GRAPH(graph), &prop);
    prop.is_point = 0;
    prop.is_line = 1;
    gwy_graph_set_autoproperties(GWY_GRAPH(graph), &prop);

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

    lab = g_string_new("Angular slope distribution");
    gwy_graph_add_dataline_with_units(GWY_GRAPH(graph), dataline, 0, lab, NULL,
                                      1, 1, "deg", " ");

    gwy_app_graph_window_create(graph);

    g_string_free(lab, TRUE);
    g_object_unref(dataline);

    return graph;
}

static gdouble
compute_slopes(GwyDataField *dfield,
               gint kernel_size,
               gdouble *xder,
               gdouble *yder)
{
    gdouble *data;
    gdouble qx, qy;
    gdouble d, max;
    gint xres, yres;
    gint col, row;

    data = gwy_data_field_get_data(dfield);
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
    GwySIUnit *unit;
    gchar *xyu, *zu, *u;
    gdouble *d;
    gint i;

    dfield = GWY_DATA_FIELD(gwy_data_field_new(res, res, real, real, FALSE));

    unit = GWY_SI_UNIT(gwy_si_unit_new(""));
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    zu = gwy_si_unit_get_unit_string(gwy_data_field_get_si_unit_z(old));
    xyu = gwy_si_unit_get_unit_string(gwy_data_field_get_si_unit_xy(old));
    if (!strcmp(zu, xyu))
        unit = GWY_SI_UNIT(gwy_si_unit_new(""));
    else {
        u = g_strconcat(zu, "/", xyu, NULL);
        unit = GWY_SI_UNIT(gwy_si_unit_new(u));
        g_free(u);
    }

    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_free(xyu);
    g_free(zu);
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
load_args(GwyContainer *container,
          SlopeArgs *args)
{
    *args = slope_defaults;

    gwy_container_gis_enum_by_name(container, output_type_key,
                                   &args->output_type);
    gwy_container_gis_int32_by_name(container, size_key, &args->size);
    args->size = CLAMP(args->size, 1, 16384);
    gwy_container_gis_boolean_by_name(container, logscale_key, &args->logscale);
    gwy_container_gis_boolean_by_name(container, fit_plane_key,
                                      &args->fit_plane);
    gwy_container_gis_int32_by_name(container, kernel_size_key,
                                    &args->kernel_size);
    args->kernel_size = CLAMP(args->kernel_size, 2, 16);
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
