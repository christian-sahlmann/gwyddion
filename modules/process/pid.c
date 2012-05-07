/*
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwyddion/gwymathfallback.h>
#include <app/gwyapp.h>

#define PID_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gdouble proportional;
    gdouble integral;
    gdouble derivative;
    gint tau;
    gint ratio;
    gint fpower;
    gdouble fstrength;
    gdouble fsetpoint;
} PIDArgs;

typedef struct {
    GtkObject *proportional;
    GtkObject *integral;
    GtkObject *derivative;
    GtkObject *ratio;
    GtkObject *tau;
    GtkObject *fpower;
    GtkObject *fstrength;
    GtkObject *fsetpoint;
} PIDControls;

static gboolean    module_register            (void);
static void        pid                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    pid_dialog                 (PIDArgs *args);
static void        pid_load_args              (GwyContainer *container,
                                               PIDArgs *args);
static void        pid_save_args              (GwyContainer *container,
                                               PIDArgs *args);
static void        pid_dialog_update          (PIDControls *controls,
                                               PIDArgs *args);
static gboolean    run_pid                    (GwyDataField *dfield,
                                               GwyContainer *data,
                                               gint oldid, 
                                               GwyDataField *fw, 
                                               GwyDataField *ffw,
                                               GwyDataField *revdata,
                                               GwyDataField *frev,
                                               PIDArgs *args);
static void        pid_values_update          (PIDControls *controls,
                                               PIDArgs *args);


static const PIDArgs pid_defaults = {
    1, 
    1,
    0,
    100,
    100,
    1,
    1,
    10
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("A simple PID simulator"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("pid",
                              (GwyProcessFunc)&pid,
                              N_("/_Tip/_PID simulation..."),
                              NULL,
                              PID_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Simulate PID effects on measurement"));

    return TRUE;
}

static void
pid(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *presult, *fresult, *rpresult, *rfresult;
    PIDArgs args;
    gboolean ok;
    gint oldid, newid;

    g_return_if_fail(run & PID_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    pid_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = pid_dialog(&args);
        pid_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    presult = gwy_data_field_duplicate(dfield);
    fresult = gwy_data_field_duplicate(dfield);
    rpresult = gwy_data_field_duplicate(dfield);
    rfresult = gwy_data_field_duplicate(dfield);
    gwy_data_field_set_si_unit_z(fresult, gwy_si_unit_new(NULL));
    gwy_data_field_set_si_unit_z(rfresult, gwy_si_unit_new(NULL));

    ok = run_pid(dfield, data, oldid, presult, fresult, rpresult, rfresult,
            &args);


    if (ok) {
        newid = gwy_app_data_browser_add_data_field(presult, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                0);
        g_object_unref(presult);
        gwy_app_set_data_field_title(data, newid, _("PID FW result"));

        newid = gwy_app_data_browser_add_data_field(fresult, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                0);
        g_object_unref(fresult);
        gwy_app_set_data_field_title(data, newid, _("PID FW max. force"));

        newid = gwy_app_data_browser_add_data_field(rpresult, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                0);
        g_object_unref(rpresult);
        gwy_app_set_data_field_title(data, newid, _("PID REV result"));

        newid = gwy_app_data_browser_add_data_field(rfresult, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                0);
        g_object_unref(rfresult);
        gwy_app_set_data_field_title(data, newid, _("PID REV max. force"));
    }

}

static gboolean
pid_dialog(PIDArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    gint row = 0;
    PIDControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("PID simulation"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);


    controls.proportional = gtk_adjustment_new(args->proportional, 0.0, 100, 0.1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Proportional:"), "",
                                controls.proportional);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    row++;

    controls.integral = gtk_adjustment_new(args->integral, 0.0, 100, 0.1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Integral:"), "",
                                controls.integral);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    row++;

    controls.tau = gtk_adjustment_new(args->tau, 2, 100, 1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Integration steps:"), "",
                                controls.tau);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;


    controls.derivative = gtk_adjustment_new(args->derivative, 0.0, 100, 0.1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Derivative:"), "",
                                controls.derivative);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);

    row++;

    controls.ratio = gtk_adjustment_new(args->ratio, 1, 1000, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row, _("PID/scan speed _ratio:"), "",
                                controls.ratio);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;

/*    controls.fpower = gtk_adjustment_new(args->fpower, 1, 20, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row, _("Force power law:"), "",
                                controls.fpower);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    row++;*/

    controls.fstrength = gtk_adjustment_new(args->fstrength, 0, 1000, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row, _("Force strength:"), "",
                                controls.fstrength);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    row++;

    controls.fsetpoint = gtk_adjustment_new(args->fsetpoint, 0, 1000, 1, 10, 0);
    gwy_table_attach_spinbutton(table, row, _("Force setpoint:"), "",
                                controls.fsetpoint);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    row++;


    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            pid_values_update(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = pid_defaults;
            pid_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    pid_values_update(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void        
pid_values_update(PIDControls *controls,
                  PIDArgs *args)
{
    args->proportional
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->proportional));
    args->integral
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->integral));
    args->derivative
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->derivative));
    args->ratio
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->ratio));
    args->tau
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->tau));
    //args->fpower
    //    = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->fpower));
    args->fstrength
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->fstrength));
    args->fsetpoint
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->fsetpoint));

}

static gboolean
run_pid(GwyDataField *dfield, GwyContainer *data, gint id, GwyDataField *fw, GwyDataField *ffw, GwyDataField *rev, GwyDataField *frev,
        PIDArgs *args)
{
    gdouble zpos;
    gint xres, yres, i, j;
    gint col, tcol, row, trow;
    gdouble *dfw, *dffw, *drev, *dfrev, *surface;
    gdouble force, setpoint;
    gdouble accumulator;
    gdouble previous[1000];
    gint nprev;
    gboolean revdir;
    gdouble strength;
    gdouble zrange;
    GtkWidget *dialog;
    
    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id), _("Starting..."));

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_clear(fw);
    gwy_data_field_clear(ffw);
    gwy_data_field_clear(rev);
    gwy_data_field_clear(frev);
    dfw = gwy_data_field_get_data(fw);
    dffw = gwy_data_field_get_data(ffw);
    drev = gwy_data_field_get_data(rev);
    dfrev = gwy_data_field_get_data(frev);
    surface = gwy_data_field_get_data(dfield);


    /*primitive normalisation*/
    zpos = surface[0];
    setpoint = args->fsetpoint;
    accumulator = 0;
    zrange = (gwy_data_field_get_max(dfield)-gwy_data_field_get_min(dfield));
    strength = args->fstrength/zrange;
    nprev = args->tau;

    /*scan, forming forward and backward data and error signals*/
    if (!gwy_app_wait_set_message(_("Scanning..."))) return 0;

    for (trow=-2; trow<(2*yres); trow++) //start with one complete scan line that is then throwen away
    {
        revdir = trow%2;
        row = MAX(0, trow/2);

        for (tcol=0; tcol<xres; tcol++)
        {
            if (revdir) col = xres-tcol-1;
            else col = tcol;

            for (i=0; i<args->ratio; i++) { //here comes the ratio between scanning and feedback bandwidth
                force = strength*(surface[row*xres + col]-zpos);
                //force = strength*pow((surface[row*xres + col]-zpos), args->fpower);

                for (j=nprev; j>0; j--) 
                    previous[j]=previous[j-1];
                previous[0] = (force-setpoint);

                accumulator = 0;
                for (j=0; j<nprev; j++) accumulator += previous[j]*(gdouble)(nprev-j)/(gdouble)nprev;
                accumulator/=nprev;

                zpos += (args->proportional*(force-setpoint) 
                        + args->integral*accumulator 
                        + args->derivative*(previous[0]-previous[1])/args->ratio)*zrange;
            }
            if (gwy_isinf(zpos) || gwy_isnan(zpos) || gwy_isinf(force) || gwy_isnan(force)) 
            {
                dialog = gtk_message_dialog_new(GTK_WINDOW(gwy_app_find_window_for_channel(data, id)),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR,
                                                GTK_BUTTONS_OK,
                                                _("Computation diverged, try to change parameters"));
                gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_destroy(dialog);

                gwy_app_wait_finish();
                return 0;
            }

            if (trow>=0) {
                if (!revdir) {
                    dfw[row*xres + col] = zpos;
                    dffw[row*xres + col] = force;
                }
                else {
                    drev[row*xres + col] = zpos;
                    dfrev[row*xres + col] = force;
                }
            }
 
        }
        if (!gwy_app_wait_set_fraction((gdouble)row/(gdouble)yres)) return 0;
    }
    gwy_app_wait_finish();


    return 1;
}



static void
pid_dialog_update(PIDControls *controls,
                  PIDArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->proportional),
                             args->proportional);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->integral),
                             args->integral);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->derivative),
                             args->derivative);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ratio),
                             args->ratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->tau),
                             args->tau);
    //gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->fpower),
    //                         args->fpower);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->fstrength),
                             args->fstrength);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->fsetpoint),
                             args->fsetpoint);
}

static const gchar proportional_key[]  = "/module/pid/proportional";
static const gchar integral_key[]  = "/module/pid/integral";
static const gchar derivative_key[]  = "/module/pid/derivative";
static const gchar ratio_key[]  = "/module/pid/ratio";
static const gchar tau_key[]    = "/module/pid/tau";
static const gchar fpower_key[]    = "/module/pid/fpower";
static const gchar fstrength_key[]    = "/module/pid/fstrength";
static const gchar fsetpoint_key[]    = "/module/pid/fsetpoint";


static void
pid_sanitize_args(PIDArgs *args)
{
    args->proportional = CLAMP(args->proportional, 0.0, 100.0);
    args->integral = CLAMP(args->integral, 0.0, 100.0);
    args->derivative = CLAMP(args->derivative, 0.0, 100.0);
    args->ratio = CLAMP(args->ratio, 1, 1000);
    args->tau = CLAMP(args->tau, 2, 1000);
    args->fpower = CLAMP(args->fpower, 1, 20);
    args->fstrength = CLAMP(args->fstrength, 0.0, 1000.0);
    args->fsetpoint = CLAMP(args->fsetpoint, 0.0, 1000.0);

}

static void
pid_load_args(GwyContainer *container,
              PIDArgs *args)
{
    *args = pid_defaults;

    gwy_container_gis_double_by_name(container, proportional_key, &args->proportional);
    gwy_container_gis_double_by_name(container, integral_key, &args->integral);
    gwy_container_gis_double_by_name(container, derivative_key, &args->derivative);
    gwy_container_gis_int32_by_name(container, ratio_key, &args->ratio);
    gwy_container_gis_int32_by_name(container, tau_key, &args->tau);
    gwy_container_gis_int32_by_name(container, fpower_key, &args->fpower);
    gwy_container_gis_double_by_name(container, fstrength_key, &args->fstrength);
    gwy_container_gis_double_by_name(container, fsetpoint_key, &args->fsetpoint);
    
    pid_sanitize_args(args);
}

static void
pid_save_args(GwyContainer *container,
              PIDArgs *args)
{
    gwy_container_set_double_by_name(container, proportional_key, args->proportional);
    gwy_container_set_double_by_name(container, integral_key, args->integral);
    gwy_container_set_double_by_name(container, derivative_key, args->derivative);
    gwy_container_set_int32_by_name(container, ratio_key, args->ratio);
    gwy_container_set_int32_by_name(container, tau_key, args->tau);
    gwy_container_set_int32_by_name(container, fpower_key, args->fpower);
    gwy_container_set_double_by_name(container, fstrength_key, args->fstrength);
    gwy_container_gis_double_by_name(container, fsetpoint_key, &args->fsetpoint);
 
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
