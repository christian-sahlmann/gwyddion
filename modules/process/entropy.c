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
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define ENTROPY_ENT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    RESPONSE_PREVIEW = 2,
};

typedef struct {
    gboolean on_sphere;
    gboolean fit_plane;
    gint kernel_size;
} EntropyArgs;

typedef struct {
    EntropyArgs *args;
    GwyContainer *mydata;
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *graph;
    GtkWidget *on_sphere;
    GtkWidget *fit_plane;
    GtkObject *kernel_size;
    GtkWidget *entropy;

    GwyDataField *dfield;
} EntropyControls;

static gboolean module_register    (void);
static void     entropy            (GwyContainer *data,
                                    GwyRunType run);
static gboolean entropy_dialog     (EntropyArgs *args,
                                    gboolean same_units,
                                    GwyDataField *dfield);
static void     on_sphere_changed  (EntropyControls *controls,
                                    GtkToggleButton *check);
static void     fit_plane_changed  (EntropyControls *controls,
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
    TRUE, TRUE, 5,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates entropy of two-dimensional distribution of slopes."),
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
                              N_("Calculate entropy of slope distribution"));

    return TRUE;
}

static void
entropy(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    //GwyGraphModel *gmodel;
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
        if (!same_units)
           args.on_sphere = FALSE;
        ok = entropy_dialog(&args, same_units, dfield);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }
}

static gboolean
entropy_dialog(EntropyArgs *args, gboolean same_units, GwyDataField *dfield)
{
    GtkWidget *dialog, *table, *label, *hbox;
    EntropyControls controls;
    gint response;
    gint row;

    controls.args = args;
    controls.dfield = dfield;

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
                       FALSE, FALSE, 4);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    controls.on_sphere
        = gtk_check_button_new_with_mnemonic(_("Angular distribution on sphere"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.on_sphere),
                                 args->on_sphere);
    gtk_table_attach(GTK_TABLE(table), controls.on_sphere,
                     0, 4, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    if (same_units)
        g_signal_connect_swapped(controls.on_sphere, "toggled",
                                 G_CALLBACK(on_sphere_changed), &controls);
    else
        gtk_widget_set_sensitive(controls.on_sphere, FALSE);
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

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gtk_label_new(_("Entropy:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    label = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     2, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    controls.entropy = label;

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
fit_plane_changed(EntropyControls *controls, GtkToggleButton *check)
{
    controls->args->fit_plane = gtk_toggle_button_get_active(check);
    gwy_table_hscale_set_sensitive(controls->kernel_size,
                                   controls->args->fit_plane);
}

static void
on_sphere_changed(EntropyControls *controls, GtkToggleButton *check)
{
    controls->args->on_sphere = gtk_toggle_button_get_active(check);
    /* TODO: forget all already calculated values. */
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

static void
preview(EntropyControls *controls)
{
    EntropyArgs *args = controls->args;
    GwyDataField *dfield = controls->dfield;
    GwyDataField *xder = gwy_data_field_new_alike(dfield, FALSE);
    GwyDataField *yder = gwy_data_field_new_alike(dfield, FALSE);
    gdouble S;
    gchar buf[24];

    compute_slopes(controls->dfield,
                   args->fit_plane ? args->kernel_size : 0, xder, yder);
    if (args->on_sphere)
        transform_to_sphere(xder, yder);
    S = gwy_data_field_get_entropy_2d(xder, yder);

    g_snprintf(buf, sizeof(buf), "%g", S);
    gtk_label_set_text(GTK_LABEL(controls->entropy), buf);

    g_object_unref(xder);
    g_object_unref(yder);
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

static const gchar fit_plane_key[]   = "/module/slope_ent/fit_plane";
static const gchar kernel_size_key[] = "/module/slope_ent/kernel_size";
static const gchar on_sphere_key[]   = "/module/slope_ent/on_sphere";

static void
sanitize_args(EntropyArgs *args)
{
    args->on_sphere = !!args->on_sphere;
    args->fit_plane = !!args->fit_plane;
    args->kernel_size = CLAMP(args->kernel_size, 2, 16);
}

static void
load_args(GwyContainer *container,
          EntropyArgs *args)
{
    *args = slope_defaults;

    gwy_container_gis_boolean_by_name(container, on_sphere_key,
                                      &args->on_sphere);
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
    gwy_container_set_boolean_by_name(container, on_sphere_key,
                                      args->on_sphere);
    gwy_container_set_boolean_by_name(container, fit_plane_key,
                                      args->fit_plane);
    gwy_container_set_int32_by_name(container, kernel_size_key,
                                    args->kernel_size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
