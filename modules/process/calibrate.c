/*
 *  @(#) $Id$
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define CALIBRATE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function. */
typedef struct {
    gdouble xratio;
    gdouble yratio;
    gdouble zratio;
    gdouble zmin;
    gdouble zmax;
    gint xreal;
    gint yreal;
} CalibrateArgs;

typedef struct {
    GtkWidget *xratio;
    GtkObject *yratio;
    GtkObject *zratio;
    GtkObject *xreal;
    GtkObject *yreal;
    GtkObject *zmin;
    GtkObject *zmax;
    gboolean in_update;
} CalibrateControls;

static gboolean    module_register           (const gchar *name);
static gboolean    calibrate                     (GwyContainer *data,
                                              GwyRunType run);
static gboolean    calibrate_dialog              (CalibrateArgs *args);
static void        xcalibrate_changed_cb          (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        ycalibrate_changed_cb          (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        zcalibrate_changed_cb          (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        width_changed_cb          (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        height_changed_cb         (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        zmin_changed_cb         (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        zmax_changed_cb         (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        calibrate_dialog_update       (CalibrateControls *controls,
                                              CalibrateArgs *args);
static void        calibrate_sanitize_args       (CalibrateArgs *args);
static void        calibrate_load_args           (GwyContainer *container,
                                              CalibrateArgs *args);
static void        calibrate_save_args           (GwyContainer *container,
                                              CalibrateArgs *args);

CalibrateArgs calibrate_defaults = {
    1.0,
    1.0,
    1.0,
    0,
    0,
    0,
    0,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "calibrate",
    "Recalibrate scan axis",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo calibrate_func_info = {
        "calibrate",
        "/_Basic Operations/Recalibrate...",
        (GwyProcessFunc)&calibrate,
        CALIBRATE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &calibrate_func_info);

    return TRUE;
}

static gboolean
calibrate(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GObject *dfield;
    CalibrateArgs args;
    gboolean ok;

    g_return_val_if_fail(run & CALIBRATE_RUN_MODES, FALSE);
    dfield = gwy_container_get_object_by_name(data, "/0/data");
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = calibrate_defaults;
    else
        calibrate_load_args(gwy_app_settings_get(), &args);
    args.xreal = gwy_data_field_get_xreal(GWY_DATA_FIELD(dfield));
    args.yreal = gwy_data_field_get_yreal(GWY_DATA_FIELD(dfield));
    args.zmin = gwy_data_field_get_min(GWY_DATA_FIELD(dfield));
    args.zmax = gwy_data_field_get_max(GWY_DATA_FIELD(dfield));
    ok = (run != GWY_RUN_MODAL) || calibrate_dialog(&args);
    if (run == GWY_RUN_MODAL)
        calibrate_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    gwy_app_clean_up_data(data);
    dfield = gwy_container_get_object_by_name(data, "/0/data");
 /*   
    gwy_data_field_set_xreal(GWY_DATA_FIELD(dfield), args.xreal);
    gwy_data_field_set_yreal(GWY_DATA_FIELD(dfield), args.yreal);
   
    
    if (gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&dfield))
    {
        gwy_data_field_set_xreal(GWY_DATA_FIELD(dfield), args.xreal);
        gwy_data_field_set_yreal(GWY_DATA_FIELD(dfield), args.yreal);
    }
    if (gwy_container_gis_object_by_name(data, "/0/show", (GObject**)&dfield))
    {
        gwy_data_field_set_xreal(GWY_DATA_FIELD(dfield), args.xreal);
        gwy_data_field_set_yreal(GWY_DATA_FIELD(dfield), args.yreal);
    }
 */
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

static gboolean
calibrate_dialog(CalibrateArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    CalibrateControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("Calibrate"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    controls.xratio = gwy_val_unit_new("X calibration factor: ");
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.xratio,
                       FALSE, FALSE, 4);
    
    table = gtk_table_new(8, 3, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

 
   
    
    /*
    controls.xratio = gtk_adjustment_new(args->xratio, 0.01, 100, 0.01, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, 0, _("X calibration factor:"), "",
                                       controls.xratio);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_object_set_data(G_OBJECT(controls.xratio), "controls", &controls);
    g_signal_connect(controls.xratio, "value_changed",
                     G_CALLBACK(xcalibrate_changed_cb), args);
*/
    controls.xreal = gtk_adjustment_new(args->xratio*args->xreal,
                                       0, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 1, _("New X range"), _("px"),
                                       controls.xreal);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.xreal), "controls", &controls);
    g_signal_connect(controls.xreal, "value_changed",
                     G_CALLBACK(width_changed_cb), args);

    
    controls.yratio = gtk_adjustment_new(args->yratio, 0.01, 100, 0.01, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, 2, _("X calibration factor:"), "",
                                       controls.yratio);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_object_set_data(G_OBJECT(controls.yratio), "controls", &controls);
    g_signal_connect(controls.yratio, "value_changed",
                     G_CALLBACK(ycalibrate_changed_cb), args);

    
    controls.yreal = gtk_adjustment_new(args->yratio*args->yreal,
                                       0, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 3, _("New Y range"), _("px"),
                                       controls.yreal);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.yreal), "controls", &controls);
    g_signal_connect(controls.yreal, "value_changed",
                     G_CALLBACK(height_changed_cb), args);

    controls.zratio = gtk_adjustment_new(args->zratio, 0.01, 100, 0.01, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, 4, _("Z calibration factor:"), "",
                                       controls.zratio);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    g_object_set_data(G_OBJECT(controls.zratio), "controls", &controls);
    g_signal_connect(controls.zratio, "value_changed",
                     G_CALLBACK(zcalibrate_changed_cb), args);

    controls.zmin = gtk_adjustment_new(args->zmin,
                                       0, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 5, _("Z minimum"), _("px"),
                                       controls.zmin);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.zmin), "controls", &controls);
    g_signal_connect(controls.zmin, "value_changed",
                     G_CALLBACK(zmin_changed_cb), args);

    controls.zmax = gtk_adjustment_new(args->zmax,
                                       0, 10000, 1, 10, 0);
    spin = gwy_table_attach_spinbutton(table, 6, _("Z maximum"), _("px"),
                                       controls.zmax);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(spin), TRUE);
    g_object_set_data(G_OBJECT(controls.zmax), "controls", &controls);
    g_signal_connect(controls.zmax, "value_changed",
                     G_CALLBACK(zmax_changed_cb), args);


    
    controls.in_update = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->xratio
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.xratio));
            args->yratio
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.yratio));
            args->zmin
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zmin));
            args->zmax
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zmax));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            args->xratio = calibrate_defaults.xratio;
            args->yratio = calibrate_defaults.yratio;
            args->zratio = calibrate_defaults.zratio;
            calibrate_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->xratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.xratio));
    args->yratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.yratio));
    args->zmin = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zmin));
    args->zmax = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zmax));
    gtk_widget_destroy(dialog);

    return TRUE;
}


static void
xcalibrate_changed_cb(GtkAdjustment *adj,
                 CalibrateArgs *args)
{
    CalibrateControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xratio = gtk_adjustment_get_value(adj);
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}
static void
ycalibrate_changed_cb(GtkAdjustment *adj,
                 CalibrateArgs *args)
{
    CalibrateControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yratio = gtk_adjustment_get_value(adj);
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}
static void
zcalibrate_changed_cb(GtkAdjustment *adj,
                 CalibrateArgs *args)
{
    CalibrateControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zratio = gtk_adjustment_get_value(adj);
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
width_changed_cb(GtkAdjustment *adj,
                 CalibrateArgs *args)
{
    CalibrateControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->xratio = gtk_adjustment_get_value(adj)/args->xreal;
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
height_changed_cb(GtkAdjustment *adj,
                  CalibrateArgs *args)
{
    CalibrateControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->yratio = gtk_adjustment_get_value(adj)/args->yreal;
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
zmin_changed_cb(GtkAdjustment *adj,
                  CalibrateArgs *args)
{
    CalibrateControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zratio = (args->zmax - gtk_adjustment_get_value(adj))/(args->zmax - args->zmin);
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}
static void
zmax_changed_cb(GtkAdjustment *adj,
                  CalibrateArgs *args)
{
    CalibrateControls *controls;

    controls = g_object_get_data(G_OBJECT(adj), "controls");
    if (controls->in_update)
        return;

    controls->in_update = TRUE;
    args->zratio = (gtk_adjustment_get_value(adj) - args->zmin)/(args->zmax - args->zmin);
    calibrate_dialog_update(controls, args);
    controls->in_update = FALSE;
}

static void
calibrate_dialog_update(CalibrateControls *controls,
                    CalibrateArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xratio),
                             args->xratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yratio),
                             args->yratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zratio),
                             args->zratio);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->xreal),
                             ROUND(args->xratio*args->xreal));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->yreal),
                             ROUND(args->yratio*args->yreal));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zmin),
                             ROUND(args->zmin));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->zmax),
                             ROUND(args->zmax));
}

static const gchar *xratio_key = "/module/calibrate/xratio"; 
static const gchar *yratio_key = "/module/calibrate/yratio";

static void
calibrate_sanitize_args(CalibrateArgs *args)
{
    args->xratio = CLAMP(args->xratio, 0.01, 100.0);
    args->yratio = CLAMP(args->yratio, 0.01, 100.0);
}

static void
calibrate_load_args(GwyContainer *container,
                CalibrateArgs *args)
{
    *args = calibrate_defaults;

    gwy_container_gis_double_by_name(container, xratio_key, &args->xratio);
    gwy_container_gis_double_by_name(container, yratio_key, &args->yratio);
    calibrate_sanitize_args(args);
}

static void
calibrate_save_args(GwyContainer *container,
                CalibrateArgs *args)
{
    gwy_container_set_double_by_name(container, xratio_key, args->xratio);
    gwy_container_set_double_by_name(container, yratio_key, args->yratio);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
