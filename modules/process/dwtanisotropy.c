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
#include <app/undo.h>

#define DWT_ANISOTROPY_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    GwyInterpolationType interp;
    GwyDWTType wavelet;
    gdouble ratio;
    gint lowlimit;
} DWTAnisotropyArgs;

typedef struct {
    GtkWidget *wavelet;
    GtkWidget *interp;
    GtkObject *ratio;
    GtkObject *lowlimit;
} DWTAnisotropyControls;

static gboolean    module_register            (const gchar *name);
static gboolean    dwt_anisotropy                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    dwt_anisotropy_dialog                 (DWTAnisotropyArgs *args);
static void        interp_changed_cb          (GObject *item,
                                               DWTAnisotropyArgs *args);
static void        wavelet_changed_cb          (GObject *item,
                                               DWTAnisotropyArgs *args);
static void        ratio_changed_cb            (GtkAdjustment *adj,
                                               DWTAnisotropyArgs *args);
static void        lowlimit_changed_cb         (GtkAdjustment *adj,
                                               DWTAnisotropyArgs *args);
static void        dwt_anisotropy_dialog_update          (DWTAnisotropyControls *controls,
                                               DWTAnisotropyArgs *args);
static void        dwt_anisotropy_load_args              (GwyContainer *container,
                                               DWTAnisotropyArgs *args);
static void        dwt_anisotropy_save_args              (GwyContainer *container,
                                               DWTAnisotropyArgs *args);
static void        dwt_anisotropy_sanitize_args          (DWTAnisotropyArgs *args);


DWTAnisotropyArgs dwt_anisotropy_defaults = {
    GWY_INTERPOLATION_BILINEAR,
    GWY_DWT_DAUB12,
    0.2,
    4
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("2D DWT anisotropy detection based on X/Y components ratio."),
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
    static GwyProcessFuncInfo dwt_anisotropy_func_info = {
        "dwt_anisotropy",
        N_("/_Integral Transforms/DWT _Anisotropy..."),
        (GwyProcessFunc)&dwt_anisotropy,
        DWT_ANISOTROPY_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &dwt_anisotropy_func_info);

    return TRUE;
}

static gboolean
dwt_anisotropy(GwyContainer *data, GwyRunType run)
{
    GtkWidget *dialog;
    GwyDataField *dfield, *mask;
    GwyDataLine *wtcoefs;
    DWTAnisotropyArgs args;
    gboolean ok;
    gint xsize, ysize, newsize, limit;

    g_assert(run & DWT_ANISOTROPY_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_current()),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("%s: Data must be square."), _("DWT Anisotropy"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        return FALSE;
    }

    if (run == GWY_RUN_WITH_DEFAULTS)
        args = dwt_anisotropy_defaults;
    else
        dwt_anisotropy_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || dwt_anisotropy_dialog(&args);
    if (run == GWY_RUN_MODAL)
        dwt_anisotropy_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    mask = NULL;
    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    gwy_container_gis_object_by_name(data, "/0/mask", (GObject**)&mask);

    newsize = gwy_data_field_get_fft_res(xsize);
    dfield = gwy_data_field_duplicate(dfield);
    gwy_data_field_add(dfield, -gwy_data_field_get_avg(dfield));
    gwy_data_field_resample(dfield, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);
    if (mask)
        gwy_data_field_resample(mask, newsize, newsize,
                                GWY_INTERPOLATION_NONE);
    else {
        mask = GWY_DATA_FIELD(gwy_data_field_new_alike(dfield, TRUE));
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(mask));
        g_object_unref(mask);
    }

    wtcoefs = GWY_DATA_LINE(gwy_data_line_new(10, 10, TRUE));
    wtcoefs = gwy_dwt_set_coefficients(wtcoefs, args.wavelet);

    /*justo for sure clamp the lowlimit again*/
    limit = pow(2, CLAMP(args.lowlimit, 1, 20));
    mask = gwy_data_field_dwt_mark_anisotropy(dfield, mask, wtcoefs, args.ratio,
                                              limit);

    gwy_data_field_resample(mask, xsize, ysize,
                            GWY_INTERPOLATION_BILINEAR);
    g_object_unref(wtcoefs);
    g_object_unref(dfield);

    return TRUE;
}


static gboolean
dwt_anisotropy_dialog(DWTAnisotropyArgs *args)
{
    GtkWidget *dialog, *table, *spin;
    DWTAnisotropyControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D DWT Anisotropy"), NULL, 0,
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

    controls.ratio = gtk_adjustment_new(args->ratio,
                    0.0001, 10.0, 0.01, 0.1, 0);
    spin = gwy_table_attach_spinbutton(table, 3,
                       _("X/Y ratio threshold:"), NULL,
                       controls.ratio);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    g_signal_connect(controls.ratio, "value_changed",
             G_CALLBACK(ratio_changed_cb), args);

    controls.lowlimit = gtk_adjustment_new(args->lowlimit,
                    1, 20, 1, 1, 0);
    spin = gwy_table_attach_spinbutton(table, 4,
                       _("Low level exclude limit:"), NULL,
                       controls.lowlimit);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 0);
    g_signal_connect(controls.lowlimit, "value_changed",
             G_CALLBACK(lowlimit_changed_cb), args);



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
            *args = dwt_anisotropy_defaults;
            dwt_anisotropy_dialog_update(&controls, args);
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
                  DWTAnisotropyArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static void
wavelet_changed_cb(GObject *item,
                  DWTAnisotropyArgs *args)
{
    args->wavelet = GPOINTER_TO_INT(g_object_get_data(item, "dwt-wavelet-type"));
}

static void
ratio_changed_cb(GtkAdjustment *adj, DWTAnisotropyArgs *args)
{
    args->ratio = gtk_adjustment_get_value(adj);
}

static void
lowlimit_changed_cb(GtkAdjustment *adj, DWTAnisotropyArgs *args)
{
    args->lowlimit = gtk_adjustment_get_value(adj);
}
static void
dwt_anisotropy_dialog_update(DWTAnisotropyControls *controls,
                     DWTAnisotropyArgs *args)
{
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gwy_option_menu_set_history(controls->wavelet, "dwt-wavelet-type",
                                args->wavelet);
}


static const gchar *interp_key = "/module/dwt_anisotropy/interp";
static const gchar *wavelet_key = "/module/dwt_anisotropy/wavelet";
static const gchar *ratio_key = "/module/dwt_anisotropy/ratio";
static const gchar *lowlimit_key = "/module/dwt_anisotropy/lowlimit";

static void
dwt_anisotropy_sanitize_args(DWTAnisotropyArgs *args)
{
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->wavelet = CLAMP(args->wavelet, GWY_DWT_HAAR, GWY_DWT_DAUB20);
    args->lowlimit = CLAMP(args->lowlimit, 1, 20);
}

static void
dwt_anisotropy_load_args(GwyContainer *container,
              DWTAnisotropyArgs *args)
{
    *args = dwt_anisotropy_defaults;

    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, wavelet_key, &args->wavelet);
    gwy_container_gis_double_by_name(container, ratio_key, &args->ratio);
    gwy_container_gis_int32_by_name(container, lowlimit_key, &args->lowlimit);
    dwt_anisotropy_sanitize_args(args);
}

static void
dwt_anisotropy_save_args(GwyContainer *container,
              DWTAnisotropyArgs *args)
{
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, wavelet_key, args->wavelet);
    gwy_container_set_double_by_name(container, ratio_key, args->ratio);
    gwy_container_set_int32_by_name(container, lowlimit_key, args->lowlimit);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
