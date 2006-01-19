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

#include "config.h"
#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/inttrans.h>
#include <libprocess/cwt.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define CWT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

/* Data for this function. */
typedef struct {
    gboolean preserve;
    gdouble scale;
    GwyInterpolationType interp;
    Gwy2DCWTWaveletType wavelet;
} CWTArgs;

typedef struct {
    GtkWidget *preserve;
    GtkObject *scale;
    GtkWidget *interp;
    GtkWidget *wavelet;
} CWTControls;

static gboolean    module_register            (const gchar *name);
static void        cwt                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    cwt_dialog                 (CWTArgs *args);
static void        preserve_changed_cb        (GtkToggleButton *button,
                                               CWTArgs *args);
static void        cwt_load_args              (GwyContainer *container,
                                               CWTArgs *args);
static void        cwt_save_args              (GwyContainer *container,
                                               CWTArgs *args);
static void        cwt_dialog_update          (CWTControls *controls,
                                               CWTArgs *args);

static const CWTArgs cwt_defaults = {
    1,
    10,
    GWY_INTERPOLATION_BILINEAR,
    GWY_2DCWT_GAUSS,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Two-dimensional CWT (Continuous Wavelet Transform)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo cwt_func_info = {
        "cwt",
        N_("/_Integral Transforms/_2D CWT..."),
        (GwyProcessFunc)&cwt,
        CWT_RUN_MODES,
        GWY_MENU_FLAG_DATA,
    };

    gwy_process_func_register(name, &cwt_func_info);

    return TRUE;
}

static void
cwt(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window, *dialog;
    GwyDataField *dfield;
    CWTArgs args;
    gboolean ok;
    gint xsize, ysize;
    gint newsize;

    g_return_if_fail(run & CWT_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_for_data(data)),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Data must be square."), "CWT");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    cwt_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = cwt_dialog(&args);
        cwt_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/data",
                                             "/0/select",
                                             "/0/base/palette",
                                             NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    newsize = gwy_fft_find_nice_size(xsize);
    gwy_data_field_resample(dfield, newsize, newsize, args.interp);

    gwy_data_field_cwt(dfield,
                       args.interp,
                       args.scale,
                       args.wavelet);

    if (args.preserve)
        gwy_data_field_resample(dfield, xsize, ysize, args.interp);

    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    g_object_unref(data);
}


static gboolean
cwt_dialog(CWTArgs *args)
{
    GtkWidget *dialog, *table;
    CWTControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D CWT"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);


    controls.scale = gtk_adjustment_new(args->scale, 0.0, 1000.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, 1, _("_Scale:"), _("pixels"),
                                controls.scale);

    controls.preserve
        = gtk_check_button_new_with_mnemonic(_("_Preserve size (don't "
                                               "resize to power of 2)"));
    gtk_table_attach(GTK_TABLE(table), controls.preserve, 0, 3, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.preserve),
                                 args->preserve);
    g_signal_connect(controls.preserve, "toggled",
                     G_CALLBACK(preserve_changed_cb), args);

    controls.interp
        = gwy_enum_combo_box_new(gwy_interpolation_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->interp, args->interp, TRUE);
    gwy_table_attach_row(table, 2, _("_Interpolation type:"), "",
                         controls.interp);
    controls.wavelet
        = gwy_enum_combo_box_new(gwy_2d_cwt_wavelet_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->wavelet, args->wavelet, TRUE);
    gwy_table_attach_row(table, 3, _("_Wavelet type:"), "",
                         controls.wavelet);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            args->scale
                = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.scale));
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = cwt_defaults;
            cwt_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->scale = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.scale));
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
preserve_changed_cb(GtkToggleButton *button, CWTArgs *args)
{
    args->preserve = gtk_toggle_button_get_active(button);
}

static void
cwt_dialog_update(CWTControls *controls,
                  CWTArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->scale),
                             args->scale);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->wavelet),
                                  args->wavelet);
}

static const gchar preserve_key[] = "/module/cwt/preserve";
static const gchar interp_key[]   = "/module/cwt/interp";
static const gchar wavelet_key[]  = "/module/cwt/wavelet";
static const gchar scale_key[]    = "/module/cwt/scale";

static void
cwt_sanitize_args(CWTArgs *args)
{
    args->preserve = !!args->preserve;
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = MIN(args->wavelet, GWY_2DCWT_HAT);
    args->scale = CLAMP(args->scale, 0.0, 1000.0);
}

static void
cwt_load_args(GwyContainer *container,
              CWTArgs *args)
{
    *args = cwt_defaults;

    gwy_container_gis_boolean_by_name(container, preserve_key, &args->preserve);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    gwy_container_gis_double_by_name(container, scale_key, &args->scale);
    cwt_sanitize_args(args);
}

static void
cwt_save_args(GwyContainer *container,
              CWTArgs *args)
{
    gwy_container_set_boolean_by_name(container, preserve_key, args->preserve);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
    gwy_container_set_double_by_name(container, scale_key, args->scale);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
