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
    gdouble zreal;
    gdouble xreal;
    gdouble yreal;
} CalibrateArgs;

typedef struct {
    GtkObject *xratio;
    GtkObject *yratio;
    GtkObject *zratio;
    GtkWidget *xreal;
    GtkWidget *yreal;
    GtkWidget *zreal;
    gboolean in_update;
} CalibrateControls;

static gboolean    module_register           (const gchar *name);
static gboolean    calibrate                     (GwyContainer *data,
                                              GwyRunType run);
static gboolean    calibrate_dialog              (CalibrateArgs *args, GwyContainer *data);
static void        xcalibrate_changed_cb          (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        ycalibrate_changed_cb          (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        zcalibrate_changed_cb          (GtkAdjustment *adj,
                                              CalibrateArgs *args);
static void        width_changed_cb          (GwyValUnit *valunit,
                                              CalibrateArgs *args);
static void        height_changed_cb         (GwyValUnit *valunit,
                                              CalibrateArgs *args);
static void        z_changed_cb         (GwyValUnit *valunit,
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
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "calibrate",
    N_("Recalibrate scan axis"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* XXX: Evil static variable */
CalibrateControls *pcontrols;

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo calibrate_func_info = {
        "calibrate",
        N_("/_Basic Operations/Recalibrate..."),
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
    args.zreal = gwy_data_field_get_max(GWY_DATA_FIELD(dfield))
        - gwy_data_field_get_min(GWY_DATA_FIELD(dfield));
    ok = (run != GWY_RUN_MODAL) || calibrate_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        calibrate_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    gwy_app_clean_up_data(data);
    dfield = gwy_container_get_object_by_name(data, "/0/data");


    gwy_data_field_set_xreal(GWY_DATA_FIELD(dfield), args.xreal*args.xratio);
    gwy_data_field_set_yreal(GWY_DATA_FIELD(dfield), args.yreal*args.yratio);
    if (args.zratio != 1.0) gwy_data_field_multiply(GWY_DATA_FIELD(dfield), args.zratio);

    if (gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&dfield))
    {
        gwy_data_field_set_xreal(GWY_DATA_FIELD(dfield), args.xreal*args.xratio);
        gwy_data_field_set_yreal(GWY_DATA_FIELD(dfield), args.yreal*args.yratio);
        if (args.zratio != 1.0) gwy_data_field_multiply(GWY_DATA_FIELD(dfield), args.zratio);
    }
    if (gwy_container_gis_object_by_name(data, "/0/show", (GObject**)&dfield))
    {
        gwy_data_field_set_xreal(GWY_DATA_FIELD(dfield), args.xreal*args.xratio);
        gwy_data_field_set_yreal(GWY_DATA_FIELD(dfield), args.yreal*args.yratio);
        if (args.zratio != 1.0) gwy_data_field_multiply(GWY_DATA_FIELD(dfield), args.zratio);
    }



    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);

    return FALSE;
}

static gboolean
calibrate_dialog(CalibrateArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *spin, *hbox, *label;
    GwyDataField *dfield;
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

    pcontrols = &controls;
    controls.in_update = TRUE;
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>New Real Dimensions: </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label,
                       FALSE, FALSE, 4);
    controls.xreal = gwy_val_unit_new(_("_X Range: "),
                                       gwy_data_field_get_si_unit_xy(dfield));
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls.xreal), args->xreal);
    g_signal_connect(GWY_VAL_UNIT(controls.xreal), "value_changed",
                     G_CALLBACK(width_changed_cb), args);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.xreal,
                       FALSE, FALSE, 4);
    controls.yreal = gwy_val_unit_new(_("_Y Range: "),
                                       gwy_data_field_get_si_unit_xy(dfield));
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls.yreal), args->yreal);
    g_signal_connect(GWY_VAL_UNIT(controls.yreal), "value_changed",
                     G_CALLBACK(height_changed_cb), args);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.yreal,
                       FALSE, FALSE, 4);
    controls.zreal = gwy_val_unit_new(_("_Z Range: "),
                                       gwy_data_field_get_si_unit_z(dfield));
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls.zreal), args->zreal);
    g_signal_connect(GWY_VAL_UNIT(controls.zreal), "value_changed",
                     G_CALLBACK(z_changed_cb), args);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.zreal,
                       FALSE, FALSE, 4);



    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<b>Calibration Coefficients: </b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label,
                       FALSE, FALSE, 4);


    hbox = gtk_hbox_new(FALSE, 2);

    label = gtk_label_new_with_mnemonic(_("X C_alibration Factor: "));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label,
                       FALSE, FALSE, 4);
    controls.xratio = gtk_adjustment_new(args->xratio,
                                        0.01, 10000, 1, 10, 0);
    g_object_set_data(G_OBJECT(controls.xratio), "controls", &controls);
    g_signal_connect(controls.xratio, "value_changed",
                     G_CALLBACK(xcalibrate_changed_cb), args);

    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.xratio), 1, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), spin,
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    hbox = gtk_hbox_new(FALSE, 2);

    label = gtk_label_new_with_mnemonic(_("Y Cali_bration Factor: "));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label,
                       FALSE, FALSE, 4);
    controls.yratio = gtk_adjustment_new(args->yratio,
                                        0.01, 10000, 1, 10, 0);
    g_object_set_data(G_OBJECT(controls.yratio), "controls", &controls);
    g_signal_connect(controls.yratio, "value_changed",
                     G_CALLBACK(ycalibrate_changed_cb), args);

    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.yratio), 1, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), spin,
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    hbox = gtk_hbox_new(FALSE, 2);

    label = gtk_label_new_with_mnemonic(_("Z Calibration Fa_ctor: "));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label,
                       FALSE, FALSE, 4);
    controls.zratio = gtk_adjustment_new(args->zratio,
                                        0.01, 10000, 1, 10, 0);
    g_object_set_data(G_OBJECT(controls.zratio), "controls", &controls);
    g_signal_connect(controls.zratio, "value_changed",
                     G_CALLBACK(zcalibrate_changed_cb), args);

    spin = gtk_spin_button_new(GTK_ADJUSTMENT(controls.zratio), 1, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), spin,
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.in_update = FALSE;

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->xratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.xratio));
            args->yratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.yratio));
            args->zratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zratio));
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
    args->zratio = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.zratio));
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
width_changed_cb(GwyValUnit *valunit,
                 CalibrateArgs *args)
{
    if (pcontrols->in_update)
        return;

    pcontrols->in_update = TRUE;
    args->xratio = gwy_val_unit_get_value(valunit)/args->xreal;
    calibrate_dialog_update(pcontrols, args);
    pcontrols->in_update = FALSE;

}

static void
height_changed_cb(GwyValUnit *valunit,
                  CalibrateArgs *args)
{
    if (pcontrols->in_update)
        return;

    pcontrols->in_update = TRUE;

    args->yratio = gwy_val_unit_get_value(valunit)/args->yreal;
    calibrate_dialog_update(pcontrols, args);
    pcontrols->in_update = FALSE;
}

static void
z_changed_cb(GwyValUnit *valunit,
                  CalibrateArgs *args)
{
    if (pcontrols->in_update)
        return;

    pcontrols->in_update = TRUE;

    args->zratio = gwy_val_unit_get_value(valunit)/args->zreal;
    calibrate_dialog_update(pcontrols, args);
    pcontrols->in_update = FALSE;
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

    gwy_val_unit_set_value(GWY_VAL_UNIT(controls->xreal),
                           args->xratio*args->xreal);
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls->yreal),
                           args->yratio*args->yreal);
    gwy_val_unit_set_value(GWY_VAL_UNIT(controls->zreal),
                           args->zratio*args->zreal);


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
