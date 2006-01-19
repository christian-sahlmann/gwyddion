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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyenum.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libprocess/dwt.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define DWT_DENOISE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    gboolean preserve;
    GwyInterpolationType interp;
    GwyDWTType wavelet;
    GwyDWTDenoiseType method;
} DWTDenoiseArgs;

typedef struct {
    GtkWidget *preserve;
    GtkWidget *wavelet;
    GtkWidget *interp;
    GtkWidget *method;
} DWTDenoiseControls;

static gboolean    module_register            (const gchar *name);
static void        dwt_denoise                (GwyContainer *data,
                                               GwyRunType run);
static gboolean    dwt_denoise_dialog         (DWTDenoiseArgs *args);
static void        preserve_changed_cb        (GtkToggleButton *button,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_dialog_update  (DWTDenoiseControls *controls,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_load_args      (GwyContainer *container,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_save_args      (GwyContainer *container,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_sanitize_args  (DWTDenoiseArgs *args);

static const DWTDenoiseArgs dwt_denoise_defaults = {
    0,
    GWY_INTERPOLATION_BILINEAR,
    GWY_DWT_DAUB12,
    GWY_DWT_DENOISE_SCALE_ADAPTIVE,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D DWT denoising module"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo dwt_denoise_func_info = {
        "dwtdenoise",
        N_("/_Integral Transforms/DWT De_noise..."),
        (GwyProcessFunc)&dwt_denoise,
        DWT_DENOISE_RUN_MODES,
        GWY_MENU_FLAG_DATA,
    };

    gwy_process_func_register(name, &dwt_denoise_func_info);

    return TRUE;
}

static void
dwt_denoise(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window, *dialog;
    GwyDataField *dfield;
    GwyDataLine *wtcoefs;
    DWTDenoiseArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;

    g_return_if_fail(run & DWT_DENOISE_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_current()),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Data must be square."), _("DWT Denoise"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    dwt_denoise_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = dwt_denoise_dialog(&args);
        dwt_denoise_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/data",
                                             "/0/base/palette",
                                             NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    newsize = gwy_fft_find_nice_size(xsize);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    gwy_data_field_resample(dfield, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);

    wtcoefs = gwy_data_line_new(10, 10, TRUE);
    wtcoefs = gwy_dwt_set_coefficients(wtcoefs, args.wavelet);
    gwy_data_field_dwt_denoise(dfield, wtcoefs, TRUE, 20, args.method);

    if (args.preserve)
        gwy_data_field_resample(dfield, xsize, ysize, args.interp);

    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                     _("DWT Denoise"));
    g_object_unref(data);
    g_object_unref(wtcoefs);
}

static gboolean
dwt_denoise_dialog(DWTDenoiseArgs *args)
{
    GtkWidget *dialog, *table;
    DWTDenoiseControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D DWT Denoise"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 5, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

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
    gwy_table_attach_row(table, 1, _("_Interpolation type:"), "",
                         controls.interp);

    controls.wavelet
        = gwy_enum_combo_box_new(gwy_dwt_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->wavelet, args->wavelet, TRUE);
    gwy_table_attach_row(table, 2, _("_Wavelet type:"), "",
                         controls.wavelet);

    controls.method
        = gwy_enum_combo_box_new(gwy_dwt_denoise_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->method, args->method, TRUE);
    gwy_table_attach_row(table, 3, _("_Threshold:"), "",
             controls.method);

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
            *args = dwt_denoise_defaults;
            dwt_denoise_dialog_update(&controls, args);
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
preserve_changed_cb(GtkToggleButton *button, DWTDenoiseArgs *args)
{
    args->preserve = gtk_toggle_button_get_active(button);
}

static void
dwt_denoise_dialog_update(DWTDenoiseControls *controls,
                          DWTDenoiseArgs *args)
{
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->interp),
                                  args->interp);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->wavelet),
                                  args->wavelet);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->method),
                                  args->method);
}


static const gchar preserve_key[] = "/module/dwtdenoise/preserve";
static const gchar interp_key[]   = "/module/dwtdenoise/interp";
static const gchar wavelet_key[]  = "/module/dwtdenoise/wavelet";
static const gchar method_key[]   = "/module/dwtdenoise/method";

static void
dwt_denoise_sanitize_args(DWTDenoiseArgs *args)
{
    args->preserve = !!args->preserve;
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = gwy_enum_sanitize_value(args->wavelet, GWY_TYPE_DWT_TYPE);
    args->method = gwy_enum_sanitize_value(args->method,
                                           GWY_TYPE_DWT_DENOISE_TYPE);
}

static void
dwt_denoise_load_args(GwyContainer *container,
              DWTDenoiseArgs *args)
{
    *args = dwt_denoise_defaults;

    gwy_container_gis_boolean_by_name(container, preserve_key, &args->preserve);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    gwy_container_gis_enum_by_name(container, method_key, &args->method);
    dwt_denoise_sanitize_args(args);
}

static void
dwt_denoise_save_args(GwyContainer *container,
              DWTDenoiseArgs *args)
{
    gwy_container_set_boolean_by_name(container, preserve_key, args->preserve);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
    gwy_container_set_enum_by_name(container, method_key, args->method);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
