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
#include <libprocess/datafield.h>
#include <libprocess/dwt.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define DWT_CORRECTION_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    GwyInterpolationType interp;
    GwyDWTType wavelet;
} DWTCorrectionArgs;

typedef struct {
    GtkWidget *wavelet;
    GtkWidget *interp;
} DWTCorrectionControls;

static gboolean    module_register            (const gchar *name);
static gboolean    dwt_correction                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    dwt_correction_dialog                 (DWTCorrectionArgs *args);
static void        interp_changed_cb          (GObject *item,
                                               DWTCorrectionArgs *args);
static void        wavelet_changed_cb          (GObject *item,
                                               DWTCorrectionArgs *args);
static void        dwt_correction_dialog_update          (DWTCorrectionControls *controls,
                                               DWTCorrectionArgs *args);
static void        dwt_correction_load_args              (GwyContainer *container,
                                               DWTCorrectionArgs *args);
static void        dwt_correction_save_args              (GwyContainer *container,
                                               DWTCorrectionArgs *args);
static void        dwt_correction_sanitize_args          (DWTCorrectionArgs *args);


DWTCorrectionArgs dwt_correction_defaults = {
    GWY_INTERPOLATION_BILINEAR,
    GWY_DWT_DAUB12,
    4
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "dwt_correction",
    N_("2D Discrete Wavelet Transform module"),
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
    static GwyProcessFuncInfo dwt_correction_func_info = {
        "dwt_correction",
        N_("/_Integral Transforms/_DWT Correction..."),
        (GwyProcessFunc)&dwt_correction,
        DWT_CORRECTION_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &dwt_correction_func_info);

    return TRUE;
}

static gboolean
dwt_correction(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *mask, *maskfield;
    GwyDataLine *wtcoefs;
    DWTCorrectionArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;

    g_assert(run & DWT_CORRECTION_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = dwt_correction_defaults;
    else
        dwt_correction_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || dwt_correction_dialog(&args);
    if (run == GWY_RUN_MODAL)
        dwt_correction_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_current()),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Data must be square."), _("DWT Correction"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return ok;
    }

    if (!gwy_container_gis_object_by_name(data, "/0/mask", (GObject*)&mask)) {
        mask = GWY_DATA_FIELD(gwy_data_field_new_alike(dfield, TRUE));
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(mask));
        g_object_unref(mask);
    }

    newsize = gwy_data_field_get_fft_res(xsize);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    gwy_data_field_resample(dfield, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);
    gwy_data_field_resample(mask, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);


    wtcoefs = gwy_data_line_new(10, 10, TRUE);
    wtcoefs = gwy_dwt_set_coefficients(wtcoefs, args.wavelet);
    mask = gwy_data_field_dwt_correction(dfield, mask, wtcoefs);

    gwy_data_field_resample(mask, xsize, ysize,
                            GWY_INTERPOLATION_BILINEAR);

    gwy_container_remove_by_name(data, "/0/mask");
    g_object_unref(wtcoefs);
    return TRUE;
}


static gboolean
dwt_correction_dialog(DWTCorrectionArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    DWTCorrectionControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D DWT_CORRECTION"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(2, 5, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);


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
            *args = dwt_correction_defaults;
            dwt_correction_dialog_update(&controls, args);
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
interp_changed_cb(GObject *item,
                  DWTCorrectionArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static void
wavelet_changed_cb(GObject *item,
                  DWTCorrectionArgs *args)
{
    args->wavelet = GPOINTER_TO_INT(g_object_get_data(item, "dwt-wavelet-type"));
}


static void
dwt_correction_dialog_update(DWTCorrectionControls *controls,
                     DWTCorrectionArgs *args)
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


static const gchar *interp_key = "/module/dwt_correction/interp";
static const gchar *wavelet_key = "/module/dwt_correction/wavelet";

static void
dwt_correction_sanitize_args(DWTCorrectionArgs *args)
{
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = CLAMP(args->wavelet, GWY_DWT_HAAR, GWY_DWT_DAUB20);
}

static void
dwt_correction_load_args(GwyContainer *container,
              DWTCorrectionArgs *args)
{
    *args = dwt_correction_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    dwt_correction_sanitize_args(args);
}

static void
dwt_correction_save_args(GwyContainer *container,
              DWTCorrectionArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
