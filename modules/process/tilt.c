/*
 *  @(#) $Id$
 *  Copyright (C) 2008 David Necas (Yeti).
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <gtk/gtk.h>
#include <libprocess/level.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define TILT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gdouble dx;
    gdouble dy;
} TiltArgs;

typedef struct {
    TiltArgs *args;
    GtkObject *dx;
    GtkObject *dy;
    GtkObject *theta;
    GtkObject *phi;
    gboolean in_update;
} TiltControls;

static gboolean module_register     (void);
static void     tilt                (GwyContainer *data,
                                     GwyRunType run);
static gboolean tilt_dialog         (TiltArgs *args,
                                     GwyDataField *dfield);
static void     dialog_reset        (TiltControls *controls);
static void     tilt_dx_changed     (GtkAdjustment *adj,
                                     TiltControls *controls);
static void     tilt_dy_changed     (GtkAdjustment *adj,
                                     TiltControls *controls);
static void     tilt_deriv_to_angles(TiltControls *controls);
static void     tilt_angle_changed  (TiltControls *controls);
static void     tilt_sanitize_args  (TiltArgs *args);
static void     tilt_load_args      (GwyContainer *container,
                                     TiltArgs *args);
static void     tilt_save_args      (GwyContainer *container,
                                     TiltArgs *args);

static const TiltArgs tilt_defaults = {
    0.0,
    0.0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Tilts image by specified amount."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("tilt",
                              (GwyProcessFunc)&tilt,
                              N_("/_Basic Operations/_Tilt..."),
                              NULL,
                              TILT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Tilt by specified amount"));

    return TRUE;
}

static void
tilt(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GQuark quark;
    TiltArgs args;
    gboolean ok;

    g_return_if_fail(run & TILT_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);
    g_return_if_fail(dfield);

    tilt_load_args(gwy_app_settings_get(), &args);

    if (run == GWY_RUN_INTERACTIVE) {
        ok = tilt_dialog(&args, dfield);
        tilt_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    gwy_app_undo_qcheckpointv(data, 1, &quark);

    {
        /* Use negative values since the module says `Tilt',
         * not `Remove tilt' */
        double bx = -args.dx*gwy_data_field_get_xmeasure(dfield);
        double by = -args.dy*gwy_data_field_get_ymeasure(dfield);
        double c = -0.5*(bx*gwy_data_field_get_xres(dfield)
                         + by*gwy_data_field_get_yres(dfield));
        gwy_data_field_plane_level(dfield, c, bx, by);
    }

    gwy_data_field_data_changed(dfield);
}

static gboolean
tilt_dialog(TiltArgs *args,
            GwyDataField *dfield)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *spin, *table, *label;
    GwySIUnit *unit;
    gchar *unitstr;
    TiltControls controls;
    gboolean units_equal;
    gint row, response;

    unit = gwy_si_unit_new(NULL);
    units_equal = gwy_si_unit_equal(gwy_data_field_get_si_unit_z(dfield),
                                    gwy_data_field_get_si_unit_xy(dfield));
    gwy_si_unit_divide(gwy_data_field_get_si_unit_z(dfield),
                       gwy_data_field_get_si_unit_xy(dfield),
                       unit);
    unitstr = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_VFMARKUP);
    g_object_unref(unit);

    dialog = gtk_dialog_new_with_buttons(_("Tilt"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    controls.args = args;
    controls.in_update = TRUE;

    table = gtk_table_new(5 + (units_equal ? 1 : 0), 3, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
    row = 0;

    /* Slopes */
    label = gwy_label_new_header(_("Slopes"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.dx = gtk_adjustment_new(args->dx, -100, 100, 1e-4, 1e-2, 0);
    spin = gwy_table_attach_hscale(table, row, _("_X:"), unitstr,
                                   controls.dx, GWY_HSCALE_NO_SCALE);
    gwy_widget_set_activate_on_unfocus(spin, TRUE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 6);
    g_signal_connect(controls.dx, "value-changed",
                     G_CALLBACK(tilt_dx_changed), &controls);
    row++;

    controls.dy = gtk_adjustment_new(args->dy, -100, 100, 1e-4, 1e-2, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Y:"), unitstr,
                                   controls.dy, GWY_HSCALE_NO_SCALE);
    gwy_widget_set_activate_on_unfocus(spin, TRUE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 6);
    g_signal_connect(controls.dy, "value-changed",
                     G_CALLBACK(tilt_dy_changed), &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    /* Angles (adjustment values will be calculated later) */
    label = gwy_label_new_header(_("Angles"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    if (units_equal) {
        controls.theta = gtk_adjustment_new(0, 0, 89.6, 1e-2, 1.0, 0);
        spin = gwy_table_attach_hscale(table, row, _("θ:"), _("deg"),
                                       controls.theta, GWY_HSCALE_NO_SCALE);
        gwy_widget_set_activate_on_unfocus(spin, TRUE);
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
        g_signal_connect_swapped(controls.theta, "value-changed",
                                 G_CALLBACK(tilt_angle_changed), &controls);
        row++;
    }
    else
        controls.theta = gtk_adjustment_new(0, 0, 90.0, 1e-2, 1.0, 0);

    controls.phi = gtk_adjustment_new(0, -180, 180, 1e-2, 1.0, 0);
    spin = gwy_table_attach_hscale(table, row, _("φ:"), _("deg"),
                                   controls.phi, GWY_HSCALE_NO_SCALE);
    gwy_widget_set_activate_on_unfocus(spin, TRUE);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    g_signal_connect_swapped(controls.phi, "value-changed",
                             G_CALLBACK(tilt_angle_changed), &controls);
    row++;

    controls.in_update = FALSE;

    tilt_deriv_to_angles(&controls);

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
            dialog_reset(&controls);
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
dialog_reset(TiltControls *controls)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->dx), 0.0);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->dy), 0.0);
}

static void
tilt_dx_changed(GtkAdjustment *adj,
                TiltControls *controls)
{
    TiltArgs *args = controls->args;

    if (controls->in_update)
        return;

    args->dx = gtk_adjustment_get_value(adj);
    tilt_deriv_to_angles(controls);
}

static void
tilt_dy_changed(GtkAdjustment *adj,
                TiltControls *controls)
{
    TiltArgs *args = controls->args;

    if (controls->in_update)
        return;

    args->dy = gtk_adjustment_get_value(adj);
    tilt_deriv_to_angles(controls);
}

static void
tilt_deriv_to_angles(TiltControls *controls)
{
    const TiltArgs *args = controls->args;

    controls->in_update = TRUE;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->theta),
                             180.0/G_PI*atan(hypot(args->dx, args->dy)));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->phi),
                             180.0/G_PI*atan2(args->dy, args->dx));
    controls->in_update = FALSE;
}

static void
tilt_angle_changed(TiltControls *controls)
{
    double theta, phi, dx, dy;

    if (controls->in_update)
        return;

    theta = G_PI/180.0*gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->theta));
    phi = G_PI/180.0*gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->phi));
    dx = tan(theta)*cos(phi);
    dy = tan(theta)*sin(phi);

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->dx), dx);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->dy), dy);
}

static const gchar dx_key[] = "/module/tilt/dx";
static const gchar dy_key[] = "/module/tilt/dy";

static void
tilt_sanitize_args(TiltArgs *args)
{
    args->dx = CLAMP(args->dx, -100, 100);
    args->dy = CLAMP(args->dy, -100, 100);
}

static void
tilt_load_args(GwyContainer *container,
               TiltArgs *args)
{
    *args = tilt_defaults;

    gwy_container_gis_double_by_name(container, dx_key, &args->dx);
    gwy_container_gis_double_by_name(container, dy_key, &args->dy);
    tilt_sanitize_args(args);
}

static void
tilt_save_args(GwyContainer *container,
               TiltArgs *args)
{
    gwy_container_set_double_by_name(container, dx_key, args->dx);
    gwy_container_set_double_by_name(container, dy_key, args->dy);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
