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
#include <app/gwyapp.h>

#define LATSIM_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gdouble mu;
    gdouble adhesion;
    gdouble load;
} LatSimArgs;

typedef struct {
    GtkObject *mu;
    GtkObject *adhesion;
    GtkObject *load;
} LatSimControls;

static gboolean    module_register            (void);
static void        latsim                     (GwyContainer *data,
                                               GwyRunType run);
static gboolean    latsim_dialog              (LatSimArgs *args);
static void        latsim_load_args           (GwyContainer *container,
                                               LatSimArgs *args);
static void        latsim_save_args           (GwyContainer *container,
                                               LatSimArgs *args);
static void        latsim_dialog_update       (LatSimControls *controls,
                                               LatSimArgs *args);
static gboolean    run_latsim                  (GwyDataField *dfield,
                                               GwyDataField *fw, 
                                               GwyDataField *rev,
                                               LatSimArgs *args);
static void        latsim_values_update       (LatSimControls *controls,
                                               LatSimArgs *args);


static const LatSimArgs latsim_defaults = {
    1,
    1,
    1
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Lateral force simulator"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("latsim",
                              (GwyProcessFunc)&latsim,
                              N_("/_Tip/_Lateral force..."),
                              NULL,
                              LATSIM_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Simulate topograpy artefacts in lateral force channels"));

    return TRUE;
}

static void
latsim(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *forward, *reverse;
    LatSimArgs args;
    gboolean ok;
    gint oldid, newid;

    g_return_if_fail(run & LATSIM_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &oldid,
                                     0);
    g_return_if_fail(dfield);

    latsim_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = latsim_dialog(&args);
        latsim_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    forward = gwy_data_field_duplicate(dfield);
    reverse = gwy_data_field_duplicate(dfield);
    gwy_data_field_set_si_unit_z(forward, gwy_si_unit_new("N"));
    gwy_data_field_set_si_unit_z(reverse, gwy_si_unit_new("N"));

    ok = run_latsim(dfield, forward, reverse,
            &args);

    if (ok) {
        newid = gwy_app_data_browser_add_data_field(forward, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                0);
        g_object_unref(forward);
        gwy_app_set_data_field_title(data, newid, _("Fw lateral force "));

        newid = gwy_app_data_browser_add_data_field(reverse, data, TRUE);
        gwy_app_sync_data_items(data, data, oldid, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                GWY_DATA_ITEM_MASK_COLOR,
                                0);
        g_object_unref(reverse);
        gwy_app_set_data_field_title(data, newid, _("Rev lateral force"));

    }

}

static gboolean
latsim_dialog(LatSimArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    gint row = 0;
    LatSimControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Lateral force simulation"), NULL, 0,
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


    controls.mu = gtk_adjustment_new(args->mu, 0.001, 100, 0.1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Friction coef.:"), "",
                                controls.mu);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    row++;

    controls.load = gtk_adjustment_new(args->load, 0.0, 1000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Normal force:"), "nN",
                                controls.load);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);

    row++;
    controls.adhesion = gtk_adjustment_new(args->adhesion, 0.0, 1000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, row, _("_Adhesion force:"), "nN",
                                controls.adhesion);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    row++;



    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            latsim_values_update(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = latsim_defaults;
            latsim_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    latsim_values_update(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void        
latsim_values_update(LatSimControls *controls,
                  LatSimArgs *args)
{
    args->mu
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->mu));
    args->adhesion
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->adhesion));
    args->load
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->load));

}

static gboolean
run_latsim(GwyDataField *dfield, GwyDataField *fw, GwyDataField *rev,
        LatSimArgs *args)
{
    gint xres, yres;
    gint col, row;
    gdouble slope, dx, theta, load, adhesion;
    gdouble va, vb, vc, vd;
    gdouble *dfw, *drev, *surface;
    
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    dx = 2.0*gwy_data_field_get_xreal(dfield)/(gdouble)xres;
    gwy_data_field_clear(fw);
    gwy_data_field_clear(rev);
    dfw = gwy_data_field_get_data(fw);
    drev = gwy_data_field_get_data(rev);
    surface = gwy_data_field_get_data(dfield);

    load = args->load*1e-9;            //TODO hardcoded now, as well as in GUI
    adhesion = args->adhesion*1e-9;    //TODO hardcoded now, as well as in GUI

    for (row=0; row<yres; row++) 
    {
        for (col=0; col<xres; col++)
        {

            if (col==0) slope = 2.0*(surface[row*xres + col + 1] - surface[row*xres + col])/dx;
            else if (col==(xres-1)) slope = 2.0*(surface[row*xres + col] - surface[row*xres + col - 1])/dx;
            else slope = (surface[row*xres + col + 1] - surface[row*xres + col - 1])/dx;

            theta = fabs(atan(slope));
            va = load*sin(theta); 
            vc = cos(theta);
            vb = args->mu*(load*vc + adhesion); 
            vd = args->mu*sin(theta);

            if (slope>=0) 
            {
                dfw[row*xres + col] = (va + vb)/(vc - vd);
                drev[row*xres + col] = -(va - vb)/(vc + vd);
            } 
            else
            {
                dfw[row*xres + col] = -(va - vb)/(vc + vd);
                drev[row*xres + col] = (va + vb)/(vc - vd);
            }
        }
    }
    return 1;
}



static void
latsim_dialog_update(LatSimControls *controls,
                  LatSimArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->mu),
                             args->mu);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->adhesion),
                             args->adhesion);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->load),
                             args->load);
}

static const gchar mu_key[]  = "/module/latsim/mu";
static const gchar adhesion_key[]  = "/module/latsim/adhesion";
static const gchar load_key[]  = "/module/latsim/load";


static void
latsim_sanitize_args(LatSimArgs *args)
{
    args->mu = CLAMP(args->mu, 0.001, 100.0);
    args->adhesion = CLAMP(args->adhesion, 0, 1000.0);
    args->load = CLAMP(args->load, 0, 1000.0);
}

static void
latsim_load_args(GwyContainer *container,
              LatSimArgs *args)
{
    *args = latsim_defaults;

    gwy_container_gis_double_by_name(container, mu_key, &args->mu);
    gwy_container_gis_double_by_name(container, adhesion_key, &args->adhesion);
    gwy_container_gis_double_by_name(container, load_key, &args->load);
    
    latsim_sanitize_args(args);
}

static void
latsim_save_args(GwyContainer *container,
              LatSimArgs *args)
{
    gwy_container_set_double_by_name(container, mu_key, args->mu);
    gwy_container_set_double_by_name(container, adhesion_key, args->adhesion);
    gwy_container_set_double_by_name(container, load_key, args->load);
 
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
