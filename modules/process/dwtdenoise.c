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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/stats.h>
#include <libprocess/inttrans.h>
#include <libprocess/dwt.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define DWT_DENOISE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
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
static gboolean    dwt_denoise                (GwyContainer *data,
                                               GwyRunType run);
static gboolean    dwt_denoise_dialog         (DWTDenoiseArgs *args);
static void        interp_changed_cb          (GObject *item,
                                               DWTDenoiseArgs *args);
static void        wavelet_changed_cb         (GObject *item,
                                               DWTDenoiseArgs *args);
static void        method_changed_cb          (GObject *item,
                                               DWTDenoiseArgs *args);
static void        preserve_changed_cb        (GtkToggleButton *button,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_dialog_update  (DWTDenoiseControls *controls,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_load_args      (GwyContainer *container,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_save_args      (GwyContainer *container,
                                               DWTDenoiseArgs *args);
static void        dwt_denoise_sanitize_args  (DWTDenoiseArgs *args);

static GtkWidget*  menu_method                (GCallback callback,
                                               gpointer cbdata,
                                               GwyDWTDenoiseType current);

DWTDenoiseArgs dwt_denoise_defaults = {
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
        "dwt_denoise",
        N_("/_Integral Transforms/DWT De_noise..."),
        (GwyProcessFunc)&dwt_denoise,
        DWT_DENOISE_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &dwt_denoise_func_info);

    return TRUE;
}

static gboolean
dwt_denoise(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window, *dialog;
    GwyDataField *dfield;
    GwyDataLine *wtcoefs;
    DWTDenoiseArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;

    g_assert(run & DWT_DENOISE_RUN_MODES);
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

        return FALSE;
    }

    if (run == GWY_RUN_WITH_DEFAULTS)
        args = dwt_denoise_defaults;
    else
        dwt_denoise_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || dwt_denoise_dialog(&args);
    if (run == GWY_RUN_MODAL)
        dwt_denoise_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    data = gwy_container_duplicate_by_prefix(data,
                                             "/0/data",
                                             "/0/base/palette",
                                             NULL);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    newsize = gwy_data_field_get_fft_res(xsize);
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

    g_object_unref(wtcoefs);
    return FALSE;
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
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        args, args->interp);
    gwy_table_attach_row(table, 1, _("_Interpolation type:"), "",
                         controls.interp);

    controls.wavelet
        = gwy_option_menu_dwt(G_CALLBACK(wavelet_changed_cb),
                                    args, args->wavelet);
    gwy_table_attach_row(table, 2, _("_Wavelet type:"), "",
                         controls.wavelet);

    controls.method
    = menu_method(G_CALLBACK(method_changed_cb),
              args, args->method);
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

static GtkWidget*
menu_method(GCallback callback,
        gpointer cbdata,
        GwyDWTDenoiseType current)
{
    static const GwyEnum entries[] = {
    { N_("Universal"),  GWY_DWT_DENOISE_UNIVERSAL,  },
    { N_("Scale adaptive"),  GWY_DWT_DENOISE_SCALE_ADAPTIVE,  },
    { N_("Scale and space adaptive"),  GWY_DWT_DENOISE_SPACE_ADAPTIVE,  },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                  "denoise-type", callback, cbdata,
                  current);
}

static void
interp_changed_cb(GObject *item,
                  DWTDenoiseArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static void
wavelet_changed_cb(GObject *item,
                  DWTDenoiseArgs *args)
{
    args->wavelet = GPOINTER_TO_INT(g_object_get_data(item, "dwt-wavelet-type"));
    printf("wavelet: %d\n", args->wavelet);
}

static void
method_changed_cb(GObject *item,
                  DWTDenoiseArgs *args)
{
    args->method = GPOINTER_TO_INT(g_object_get_data(item, "denoise-type"));
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
    /*
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle);
     */
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gwy_option_menu_set_history(controls->wavelet, "dwt-wavelet-type",
                                args->wavelet);
}


static const gchar *preserve_key = "/module/dwt_denoise/preserve";
static const gchar *interp_key = "/module/dwt_denoise/interp";
static const gchar *wavelet_key = "/module/dwt_denoise/wavelet";
static const gchar *method_key = "/module/dwt_denoise/method";

static void
dwt_denoise_sanitize_args(DWTDenoiseArgs *args)
{
    args->preserve = !!args->preserve;
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = CLAMP(args->wavelet, GWY_DWT_HAAR, GWY_DWT_DAUB20);
    args->method = CLAMP(args->wavelet,
                         GWY_DWT_DENOISE_UNIVERSAL,
                         GWY_DWT_DENOISE_SPACE_ADAPTIVE);
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
