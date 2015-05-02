/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2015 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/arithmetic.h>
#include <libprocess/inttrans.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define FFT_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    GWY_FFT_OUTPUT_REAL_IMG   = 0,
    GWY_FFT_OUTPUT_MOD_PHASE  = 1,
    GWY_FFT_OUTPUT_REAL       = 2,
    GWY_FFT_OUTPUT_IMG        = 3,
    GWY_FFT_OUTPUT_MOD        = 4,
    GWY_FFT_OUTPUT_PHASE      = 5
} GwyFFTOutputType;

typedef struct {
    gboolean raw_transform;
    gboolean zeromean;
    gboolean preserverms;
    gboolean use_imgpart;
    gboolean inverse_transform;
    GwyWindowingType window;
    GwyFFTOutputType out;
    GwyAppDataId imgpart;
} FFTArgs;

typedef struct {
    FFTArgs *args;
    GtkWidget *raw_transform;
    GtkWidget *zeromean;
    GtkWidget *preserverms;
    GtkWidget *inverse_transform;
    GtkWidget *window;
    GtkWidget *out;
    GtkWidget *imgpart;
    GtkWidget *use_imgpart;
    GwyDataField *dfield;
} FFTControls;

static gboolean module_register          (void);
static void     fft                      (GwyContainer *data,
                                          GwyRunType run);
static void     fft_create_output        (GwyContainer *data,
                                          gint id,
                                          GwyDataField *dfield,
                                          const gchar *window_name,
                                          gboolean itransform);
static gboolean fft_dialog               (FFTArgs *args,
                                          GwyDataField *dfield);
static void     raw_transform_changed    (GtkToggleButton *button,
                                          FFTControls *controls);
static void     zeromean_changed         (GtkToggleButton *button,
                                          FFTArgs *args);
static void     preserverms_changed      (GtkToggleButton *button,
                                          FFTArgs *args);
static void     use_imgpart_changed      (GtkToggleButton *button,
                                          FFTArgs *args);
static void     inverse_transform_changed(GtkToggleButton *button,
                                          FFTArgs *args);
static void     imgpart_changed          (GwyDataChooser *chooser,
                                          FFTArgs *args);
static gboolean fft_imgpart_filter       (GwyContainer *data,
                                          gint id,
                                          gpointer user_data);
static void     update_sensitivity       (FFTControls *controls);
static void     fft_dialog_update        (FFTControls *controls,
                                          FFTArgs *args);
static void     set_dfield_modulus       (GwyDataField *re,
                                          GwyDataField *im,
                                          GwyDataField *target);
static void     set_dfield_phase         (GwyDataField *re,
                                          GwyDataField *im,
                                          GwyDataField *target);
static void     fft_load_args            (GwyContainer *container,
                                          FFTArgs *args);
static void     fft_save_args            (GwyContainer *container,
                                          FFTArgs *args);
static void     fft_sanitize_args        (FFTArgs *args);

static const FFTArgs fft_defaults = {
    FALSE, TRUE, FALSE, FALSE, FALSE,
    GWY_WINDOWING_HANN,
    GWY_FFT_OUTPUT_MOD,
    GWY_APP_DATA_ID_NONE,
};

static GwyAppDataId imgpart_id = GWY_APP_DATA_ID_NONE;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Two-dimensional FFT (Fast Fourier Transform)."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fft",
                              (GwyProcessFunc)&fft,
                              N_("/_Integral Transforms/2D _FFT..."),
                              GWY_STOCK_FFT,
                              FFT_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Compute Fast Fourier Transform"));

    return TRUE;
}

static void
fft(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *imgpart, *tmp, *raout, *ipout;
    GwyContainer *idata;
    FFTArgs args;
    gboolean is_inv, ok;
    gint id, datano;

    g_return_if_fail(run & FFT_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_CONTAINER_ID, &datano,
                                     0);
    g_return_if_fail(dfield);

    fft_load_args(gwy_app_settings_get(), &args);
    idata = gwy_app_data_browser_get(args.imgpart.datano);
    if (!fft_imgpart_filter(idata, args.imgpart.id, dfield)) {
        args.imgpart.datano = datano;
        args.imgpart.id = id;
    }

    if (run == GWY_RUN_INTERACTIVE) {
        ok = fft_dialog(&args, dfield);
        fft_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    raout = gwy_data_field_new_alike(dfield, FALSE);
    ipout = gwy_data_field_new_alike(dfield, FALSE);

    is_inv = args.inverse_transform && args.raw_transform;
    imgpart = NULL;
    if (args.use_imgpart && args.imgpart.datano) {
        GQuark quark = gwy_app_get_data_key_for_id(args.imgpart.id);
        idata = gwy_app_data_browser_get(args.imgpart.datano);
        imgpart = GWY_DATA_FIELD(gwy_container_get_object(idata, quark));
        if (gwy_data_field_check_compatibility(imgpart, dfield,
                                               GWY_DATA_COMPATIBILITY_ALL))
            imgpart = NULL;
    }

    if (is_inv) {
        GwyDataField *rein = gwy_data_field_duplicate(dfield);
        GwyDataField *imin = imgpart ? gwy_data_field_duplicate(imgpart) : NULL;

        gwy_data_field_2dfft_dehumanize(rein);
        gwy_data_field_fft_postprocess(rein, FALSE);
        if (imin) {
            gwy_data_field_2dfft_dehumanize(imin);
            gwy_data_field_fft_postprocess(imin, FALSE);
        }

        gwy_data_field_2dfft_raw(rein, imin, raout, ipout,
                                 GWY_TRANSFORM_DIRECTION_BACKWARD);
        g_object_unref(rein);
        g_object_unref(imin);

        gwy_data_field_fft_postprocess(raout, FALSE);
        gwy_data_field_fft_postprocess(ipout, FALSE);
    }
    else if (args.raw_transform) {
        gwy_data_field_2dfft_raw(dfield, imgpart, raout, ipout,
                                 GWY_TRANSFORM_DIRECTION_FORWARD);
        gwy_data_field_fft_postprocess(raout, TRUE);
        gwy_data_field_fft_postprocess(ipout, TRUE);
    }
    else {
        gwy_data_field_2dfft(dfield, imgpart, raout, ipout,
                             args.window, GWY_TRANSFORM_DIRECTION_FORWARD,
                             GWY_INTERPOLATION_LINEAR,  /* ignored */
                             args.preserverms,
                             args.zeromean ? 1 : 0);
        gwy_data_field_fft_postprocess(raout, TRUE);
        gwy_data_field_fft_postprocess(ipout, TRUE);
    }

    if (args.out == GWY_FFT_OUTPUT_REAL_IMG
        || args.out == GWY_FFT_OUTPUT_REAL)
        fft_create_output(data, id, gwy_data_field_duplicate(raout),
                          _("FFT Real"), is_inv);
    if (args.out == GWY_FFT_OUTPUT_REAL_IMG
        || args.out == GWY_FFT_OUTPUT_IMG)
        fft_create_output(data, id, gwy_data_field_duplicate(ipout),
                          _("FFT Imag"), is_inv);
    if (args.out == GWY_FFT_OUTPUT_MOD_PHASE
        || args.out == GWY_FFT_OUTPUT_MOD) {
        tmp = gwy_data_field_new_alike(raout, FALSE);
        set_dfield_modulus(raout, ipout, tmp);
        fft_create_output(data, id, tmp, _("FFT Modulus"), is_inv);
    }
    if (args.out == GWY_FFT_OUTPUT_MOD_PHASE
        || args.out == GWY_FFT_OUTPUT_PHASE) {
        tmp = gwy_data_field_new_alike(raout, FALSE);
        set_dfield_phase(raout, ipout, tmp);
        fft_create_output(data, id, tmp, _("FFT Phase"), is_inv);
    }

    g_object_unref(raout);
    g_object_unref(ipout);
}

static void
fft_create_output(GwyContainer *data,
                  gint id,
                  GwyDataField *dfield,
                  const gchar *output_name,
                  gboolean itransform)
{
    gint newid;
    gchar *key;

    newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
    g_object_unref(dfield);
    gwy_app_set_data_field_title(data, newid, output_name);
    gwy_app_channel_log_add_proc(data, id, newid);

    if (itransform)
        return;

    /* make fft more visible by choosing a good gradient and using auto range */
    key = g_strdup_printf("/%i/base/palette", newid);
    gwy_container_set_string_by_name(data, key, g_strdup("DFit"));
    g_free(key);
    key = g_strdup_printf("/%i/base/range-type", newid);
    gwy_container_set_enum_by_name(data, key, GWY_LAYER_BASIC_RANGE_AUTO);
    g_free(key);
}

static void
set_dfield_modulus(GwyDataField *re, GwyDataField *im, GwyDataField *target)
{
    const gdouble *datare, *dataim;
    gdouble *data;
    gint xres, yres, i;

    xres = gwy_data_field_get_xres(re);
    yres = gwy_data_field_get_yres(re);
    datare = gwy_data_field_get_data_const(re);
    dataim = gwy_data_field_get_data_const(im);
    data = gwy_data_field_get_data(target);
    for (i = xres*yres; i; i--, datare++, dataim++, data++)
        *data = hypot(*datare, *dataim);
}

static void
set_dfield_phase(GwyDataField *re, GwyDataField *im,
                 GwyDataField *target)
{
    GwySIUnit *unit;
    const gdouble *datare, *dataim;
    gdouble *data;
    gint xres, yres, i;

    xres = gwy_data_field_get_xres(re);
    yres = gwy_data_field_get_yres(re);
    datare = gwy_data_field_get_data_const(re);
    dataim = gwy_data_field_get_data_const(im);
    data = gwy_data_field_get_data(target);
    for (i = xres*yres; i; i--, datare++, dataim++, data++)
        *data = atan2(*dataim, *datare);

    unit = gwy_data_field_get_si_unit_z(target);
    gwy_si_unit_set_from_string(unit, NULL);
}

static gboolean
fft_dialog(FFTArgs *args,
           GwyDataField *dfield)
{
    enum { RESPONSE_RESET = 1 };
    static const GwyEnum fft_outputs[] = {
        { N_("Real + Imaginary"),  GWY_FFT_OUTPUT_REAL_IMG,  },
        { N_("Modulus + Phase"),   GWY_FFT_OUTPUT_MOD_PHASE, },
        { N_("Real"),              GWY_FFT_OUTPUT_REAL,      },
        { N_("Imaginary"),         GWY_FFT_OUTPUT_IMG,       },
        { N_("Modulus"),           GWY_FFT_OUTPUT_MOD,       },
        { N_("Phase"),             GWY_FFT_OUTPUT_PHASE,     },
    };
    GtkWidget *dialog, *table;
    FFTControls controls;
    gint response, row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("2D FFT"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(7, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.raw_transform
        = gtk_check_button_new_with_mnemonic(_("Ra_w transform"));
    gtk_table_attach(GTK_TABLE(table), controls.raw_transform,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.raw_transform),
                                 args->raw_transform);
    g_signal_connect(controls.raw_transform, "toggled",
                     G_CALLBACK(raw_transform_changed), &controls);
    row++;

    controls.imgpart = gwy_data_chooser_new_channels();
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(controls.imgpart),
                                fft_imgpart_filter, dfield, NULL);
    gwy_data_chooser_set_active_id(GWY_DATA_CHOOSER(controls.imgpart),
                                   &args->imgpart);
    gwy_table_attach_hscale(table, row, _("I_maginary part:"), NULL,
                            GTK_OBJECT(controls.imgpart),
                            GWY_HSCALE_WIDGET | GWY_HSCALE_CHECK);
    controls.use_imgpart
        = gwy_table_hscale_get_check(GTK_OBJECT(controls.imgpart));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.use_imgpart),
                                 args->use_imgpart);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls.imgpart),
                                   args->use_imgpart);
    g_signal_connect(controls.imgpart, "changed",
                     G_CALLBACK(imgpart_changed), args);
    g_signal_connect(controls.use_imgpart, "toggled",
                     G_CALLBACK(use_imgpart_changed), args);
    row++;

    controls.inverse_transform
        = gtk_check_button_new_with_mnemonic(_("_Inverse transform"));
    gtk_table_attach(GTK_TABLE(table), controls.inverse_transform,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inverse_transform),
                                 args->inverse_transform);
    g_signal_connect(controls.inverse_transform, "toggled",
                     G_CALLBACK(inverse_transform_changed), args);
    row++;

    controls.out
        = gwy_enum_combo_box_new(fft_outputs, G_N_ELEMENTS(fft_outputs),
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->out, args->out, TRUE);
    gwy_table_attach_hscale(table, row, _("Output _type:"), NULL,
                            GTK_OBJECT(controls.out),
                            GWY_HSCALE_WIDGET);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.window
        = gwy_enum_combo_box_new(gwy_windowing_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->window, args->window, TRUE);
    gwy_table_attach_hscale(table, row, _("_Windowing type:"), NULL,
                            GTK_OBJECT(controls.window),
                            GWY_HSCALE_WIDGET);
    row++;

    controls.zeromean
        = gtk_check_button_new_with_mnemonic(_("Subtract mean _value "
                                               "beforehand"));
    gtk_table_attach(GTK_TABLE(table), controls.zeromean,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.zeromean),
                                 args->zeromean);
    g_signal_connect(controls.zeromean, "toggled",
                     G_CALLBACK(zeromean_changed), args);
    row++;

    controls.preserverms
        = gtk_check_button_new_with_mnemonic(_("_Preserve RMS"));
    gtk_table_attach(GTK_TABLE(table), controls.preserverms,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.preserverms),
                                 args->preserverms);
    g_signal_connect(controls.preserverms, "toggled",
                     G_CALLBACK(preserverms_changed), args);
    row++;

    update_sensitivity(&controls);

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
            {
                GwyAppDataId imgpart = args->imgpart;
                *args = fft_defaults;
                args->imgpart = imgpart;
                fft_dialog_update(&controls, args);
            }
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
raw_transform_changed(GtkToggleButton *button, FFTControls *controls)
{
    controls->args->raw_transform = gtk_toggle_button_get_active(button);
    update_sensitivity(controls);
}

static void
zeromean_changed(GtkToggleButton *button, FFTArgs *args)
{
    args->zeromean = gtk_toggle_button_get_active(button);
}

static void
preserverms_changed(GtkToggleButton *button, FFTArgs *args)
{
    args->preserverms = gtk_toggle_button_get_active(button);
}

static void
use_imgpart_changed(GtkToggleButton *button, FFTArgs *args)
{
    args->use_imgpart = gtk_toggle_button_get_active(button);
}

static void
inverse_transform_changed(GtkToggleButton *button, FFTArgs *args)
{
    args->inverse_transform = gtk_toggle_button_get_active(button);
}

static void
imgpart_changed(GwyDataChooser *chooser,
                FFTArgs *args)
{
    gwy_data_chooser_get_active_id(chooser, &args->imgpart);
}

static gboolean
fft_imgpart_filter(GwyContainer *data,
                   gint id,
                   gpointer user_data)
{
    GwyDataField *dfield = (GwyDataField*)user_data;
    GwyDataField *imgpart;
    GQuark quark;

    if (!data || id < 0)
        return FALSE;

    quark = gwy_app_get_data_key_for_id(id);
    if (!gwy_container_gis_object(data, quark, (GObject**)&imgpart))
        return FALSE;

    return !gwy_data_field_check_compatibility(imgpart, dfield,
                                               GWY_DATA_COMPATIBILITY_ALL);
}

static void
update_sensitivity(FFTControls *controls)
{
    gboolean is_raw = controls->args->raw_transform;

    gtk_widget_set_sensitive(controls->preserverms, !is_raw);
    gtk_widget_set_sensitive(controls->zeromean, !is_raw);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->window), !is_raw);
    gtk_widget_set_sensitive(controls->inverse_transform, is_raw);
}

static void
fft_dialog_update(FFTControls *controls,
                  FFTArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->raw_transform),
                                 args->raw_transform);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverse_transform),
                                 args->inverse_transform);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->preserverms),
                                 args->preserverms);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->zeromean),
                                 args->zeromean);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->use_imgpart),
                                 args->use_imgpart);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->out),
                                  args->out);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->window),
                                  args->window);
}

static const gchar inverse_transform_key[] = "/module/fft/inverse_transform";
static const gchar out_key[]               = "/module/fft/out";
static const gchar preserverms_key[]       = "/module/fft/preserverms";
static const gchar raw_transform_key[]     = "/module/fft/raw_transform";
static const gchar use_imgpart_key[]       = "/module/fft/use_imgpart";
static const gchar window_key[]            = "/module/fft/window";
static const gchar zeromean_key[]          = "/module/fft/zeromean";

static void
fft_sanitize_args(FFTArgs *args)
{
    args->raw_transform = !!args->raw_transform;
    args->zeromean = !!args->zeromean;
    args->preserverms = !!args->preserverms;
    args->use_imgpart = !!args->use_imgpart;
    args->inverse_transform = !!args->inverse_transform;
    args->window = gwy_enum_sanitize_value(args->window,
                                           GWY_TYPE_WINDOWING_TYPE);
    args->out = MIN(args->out, GWY_FFT_OUTPUT_PHASE);
    gwy_app_data_id_verify_channel(&args->imgpart);
}

static void
fft_load_args(GwyContainer *container,
              FFTArgs *args)
{
    *args = fft_defaults;

    gwy_container_gis_boolean_by_name(container, raw_transform_key,
                                      &args->raw_transform);
    gwy_container_gis_boolean_by_name(container, zeromean_key, &args->zeromean);
    gwy_container_gis_boolean_by_name(container, preserverms_key,
                                      &args->preserverms);
    gwy_container_gis_boolean_by_name(container, use_imgpart_key,
                                      &args->use_imgpart);
    gwy_container_gis_boolean_by_name(container, inverse_transform_key,
                                      &args->inverse_transform);
    gwy_container_gis_enum_by_name(container, window_key, &args->window);
    gwy_container_gis_enum_by_name(container, out_key, &args->out);
    args->imgpart = imgpart_id;
    fft_sanitize_args(args);
}

static void
fft_save_args(GwyContainer *container,
              FFTArgs *args)
{
    imgpart_id = args->imgpart;
    gwy_container_set_boolean_by_name(container, raw_transform_key,
                                      args->raw_transform);
    gwy_container_set_boolean_by_name(container, zeromean_key, args->zeromean);
    gwy_container_set_boolean_by_name(container, preserverms_key,
                                      args->preserverms);
    gwy_container_set_boolean_by_name(container, use_imgpart_key,
                                      args->use_imgpart);
    gwy_container_set_boolean_by_name(container, inverse_transform_key,
                                      args->inverse_transform);
    gwy_container_set_enum_by_name(container, window_key, args->window);
    gwy_container_set_enum_by_name(container, out_key, args->out);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
