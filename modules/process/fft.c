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
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define FFT_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

typedef enum {
  GWY_FFT_OUTPUT_REAL_IMG   = 0,
  GWY_FFT_OUTPUT_MOD_PHASE  = 1,
  GWY_FFT_OUTPUT_REAL       = 2,
  GWY_FFT_OUTPUT_IMG        = 3,
  GWY_FFT_OUTPUT_MOD        = 4,
  GWY_FFT_OUTPUT_PHASE      = 5
} GwyFFTOutputType;

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gboolean preserve;
    GwyInterpolationType interp;
    GwyWindowingType window;
    GwyFFTOutputType out;
} FFTArgs;

typedef struct {
    GtkWidget *preserve;
    GtkWidget *interp;
    GtkWidget *window;
    GtkWidget *out;
} FFTControls;

static gboolean    module_register            (const gchar *name);
static gboolean    fft                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    fft_dialog                 (FFTArgs *args);
static void        interp_changed_cb          (GObject *item,
                                               FFTArgs *args);
static void        window_changed_cb          (GObject *item,
                                               FFTArgs *args);
static void        out_changed_cb             (GObject *item,
                                               FFTArgs *args);
static void        preserve_changed_cb        (GtkToggleButton *button,
                                               FFTArgs *args);
static void        fft_dialog_update          (FFTControls *controls,
                                               FFTArgs *args);
static GtkWidget*  gwy_option_menu_fft_output (GCallback callback,
                                               gpointer cbdata,
                                               GwyFFTOutputType current);
static void        set_dfield_module          (GwyDataField *re,
                                               GwyDataField *im,
                                               GwyDataField *target);
static void        set_dfield_phase           (GwyDataField *re,
                                               GwyDataField *im,
                                               GwyDataField *target);
static void        set_dfield_real            (GwyDataField *re,
                                               GwyDataField *im,
                                               GwyDataField *target);
static void        set_dfield_imaginary       (GwyDataField *re,
                                               GwyDataField *im,
                                               GwyDataField *target);
static void        fft_load_args              (GwyContainer *container,
                                               FFTArgs *args);
static void        fft_save_args              (GwyContainer *container,
                                               FFTArgs *args);
static void        fft_sanitize_args          (FFTArgs *args);



FFTArgs fft_defaults = {
    0,
    GWY_INTERPOLATION_BILINEAR,
    GWY_WINDOWING_HANN,
    GWY_FFT_OUTPUT_MOD,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "fft",
    "2D Fast Fourier Transform module",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo fft_func_info = {
        "fft",
        "/_Integral Transforms/_2D FFT...",
        (GwyProcessFunc)&fft,
        FFT_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &fft_func_info);

    return TRUE;
}

static gboolean
fft(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window, *dialog;
    GwyDataField *dfield;
    GwyDataField *raout, *ipout, *imin;
    GwySIUnit *xyunit, *zunit;
    FFTArgs args;
    gboolean ok;
    gint xsize, ysize, newsize;
    gdouble newreals;

    g_assert(run & FFT_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = fft_defaults;
    else
        fft_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || fft_dialog(&args);
    if (run == GWY_RUN_MODAL)
        fft_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    gwy_app_clean_up_data(data);

    if (gwy_container_contains_by_name(data, "/0/show"))
        gwy_container_remove_by_name(data, "/0/show");
    if (gwy_container_contains_by_name(data, "/0/mask"))
        gwy_container_remove_by_name(data, "/0/mask");

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    xsize = gwy_data_field_get_xres(dfield);
    ysize = gwy_data_field_get_yres(dfield);
    if (xsize != ysize) {
        dialog = gtk_message_dialog_new
            (GTK_WINDOW(gwy_app_data_window_get_current()),
             GTK_DIALOG_DESTROY_WITH_PARENT,
             GTK_MESSAGE_ERROR,
             GTK_BUTTONS_OK,
             _("FFT: Data field must be rectangular."));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return ok;
    }

    newsize = gwy_data_field_get_fft_res(xsize);
    gwy_data_field_resample(dfield, newsize, newsize,
                            GWY_INTERPOLATION_BILINEAR);
    raout = GWY_DATA_FIELD(gwy_data_field_new(
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                gwy_data_field_get_xreal(dfield),
                                gwy_data_field_get_yreal(dfield),
                                TRUE));
    ipout = GWY_DATA_FIELD(gwy_data_field_new(
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                gwy_data_field_get_xreal(dfield),
                                gwy_data_field_get_yreal(dfield),
                                TRUE));

    imin = GWY_DATA_FIELD(gwy_data_field_new(
                                gwy_data_field_get_xres(dfield),
                                gwy_data_field_get_yres(dfield),
                                gwy_data_field_get_xreal(dfield),
                                gwy_data_field_get_yreal(dfield),
                                TRUE));

    gwy_data_field_multiply(dfield, 1e6);
    gwy_data_field_fill(raout,0);
    gwy_data_field_fill(ipout,0);
    gwy_data_field_fill(imin,0);

    gwy_data_field_2dfft(dfield, imin,
                         raout,
                         ipout,
                         gwy_data_line_fft_hum,
                         args.window,
                         1,
                         args.interp,
                         0,
                         0);
    gwy_data_field_2dffthumanize(raout);
    gwy_data_field_2dffthumanize(ipout);

    xyunit = (GwySIUnit*)gwy_si_unit_new("m<sup>-1</sup>");
    zunit = (GwySIUnit*)gwy_si_unit_new("m");
    newreals = ((gdouble)gwy_data_field_get_xres(dfield))
               /gwy_data_field_get_xreal(dfield);

    if (args.preserve)
    {
        gwy_data_field_resample(dfield, xsize, ysize, args.interp);
        gwy_data_field_resample(raout, xsize, ysize, args.interp);
        gwy_data_field_resample(ipout, xsize, ysize, args.interp);
    }

    if (args.out == GWY_FFT_OUTPUT_REAL_IMG || args.out == GWY_FFT_OUTPUT_REAL)
    {
        set_dfield_real(raout, ipout, dfield);
        gwy_data_field_set_si_unit_xy(dfield, xyunit);
        gwy_data_field_set_si_unit_z(dfield, zunit);
        gwy_data_field_set_xreal(dfield, newreals);
        gwy_data_field_set_yreal(dfield, newreals);


        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                            "FFT Real");
    }
    if (args.out == GWY_FFT_OUTPUT_REAL_IMG || args.out == GWY_FFT_OUTPUT_IMG)
    {
        if (args.out == GWY_FFT_OUTPUT_REAL_IMG)
        {
            data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
            dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                "/0/data"));
        }
        set_dfield_imaginary(raout, ipout, dfield);
        gwy_data_field_set_si_unit_xy(dfield, xyunit);
        gwy_data_field_set_si_unit_z(dfield, zunit);
        gwy_data_field_set_xreal(dfield, newreals);
        gwy_data_field_set_yreal(dfield, newreals);


        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                            "FFT Imag");
    }
    if (args.out == GWY_FFT_OUTPUT_MOD_PHASE || args.out == GWY_FFT_OUTPUT_MOD)
    {
        set_dfield_module(raout, ipout, dfield);
        gwy_data_field_set_si_unit_xy(dfield, xyunit);
        gwy_data_field_set_si_unit_z(dfield, zunit);
        gwy_data_field_set_xreal(dfield, newreals);
        gwy_data_field_set_yreal(dfield, newreals);


        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                            "FFT Modulus");
    }
    if (args.out == GWY_FFT_OUTPUT_MOD_PHASE
        || args.out == GWY_FFT_OUTPUT_PHASE)
    {
        if (args.out == GWY_FFT_OUTPUT_MOD_PHASE)
        {
            data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
            dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                "/0/data"));
        }
        set_dfield_phase(raout, ipout, dfield);
        gwy_data_field_set_si_unit_xy(dfield, xyunit);
        gwy_data_field_set_si_unit_xy(dfield, zunit);
        gwy_data_field_set_xreal(dfield, newreals);
        gwy_data_field_set_yreal(dfield, newreals);

        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window),
                                            "FFT Phase");
    }

    g_object_unref(raout);
    g_object_unref(ipout);
    g_object_unref(imin);

    return ok;
}

static void
set_dfield_module(GwyDataField *re, GwyDataField *im, GwyDataField *target)
{
    gint i, j;
    gdouble rval, ival;
    gint xres = gwy_data_field_get_xres(re);
    gint yres = gwy_data_field_get_xres(re);

    for (i=0; i<xres; i++)
    {
        for (j=0; j<yres; j++)
        {
            rval = gwy_data_field_get_val(re, i, j);
            ival = gwy_data_field_get_val(im, i, j);
            gwy_data_field_set_val(target, i, j, sqrt(rval*rval + ival*ival));
        }
    }
}

static void
set_dfield_phase(GwyDataField *re, GwyDataField *im,
                 GwyDataField *target)
{
    gint i;
    gint xres = gwy_data_field_get_xres(re);
    gint yres = gwy_data_field_get_xres(re);

    for (i = 0; i < (xres*yres); i++)
        target->data[i] = atan2(im->data[i], re->data[i]);
}

static void
set_dfield_real(GwyDataField *re, G_GNUC_UNUSED GwyDataField *im,
                GwyDataField *target)
{
    gwy_data_field_copy(re, target);
}

static void
set_dfield_imaginary(G_GNUC_UNUSED GwyDataField *re, GwyDataField *im,
                     GwyDataField *target)
{
    gwy_data_field_copy(im, target);
}





static gboolean
fft_dialog(FFTArgs *args)
{
    GtkWidget *dialog, *table;
    FFTControls controls;
    enum { RESPONSE_RESET = 1 };
    gint response;

    dialog = gtk_dialog_new_with_buttons(_("2D FFT"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    table = gtk_table_new(2, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);

    controls.preserve = gtk_check_button_new_with_label("preserve size");
    gwy_table_attach_row(table, 0, _("Data size treatment:"), "",
                         controls.preserve);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.preserve),
                                 args->preserve);

    g_signal_connect(controls.preserve, "toggled",
                     G_CALLBACK(preserve_changed_cb), args);

    controls.interp
        = gwy_option_menu_interpolation(G_CALLBACK(interp_changed_cb),
                                        args, args->interp);
    gwy_table_attach_row(table, 1, _("_Interpolation type:"), "",
                         controls.interp);
    controls.window
        = gwy_option_menu_windowing(G_CALLBACK(window_changed_cb),
                                    args, args->interp);
    gwy_table_attach_row(table, 2, _("_Windowing type:"), "",
                         controls.window);

    controls.out
        = gwy_option_menu_fft_output(G_CALLBACK(out_changed_cb),
                                     args, args->out);
    gwy_table_attach_row(table, 3, _("_Output type:"), "",
                         controls.out);

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
            *args = fft_defaults;
            fft_dialog_update(&controls, args);
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
                  FFTArgs *args)
{
    args->interp = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "interpolation-type"));
}

static void
out_changed_cb(GObject *item,
                  FFTArgs *args)
{
    args->out = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "fft-output-type"));
}

static void
window_changed_cb(GObject *item,
                  FFTArgs *args)
{
    args->window = GPOINTER_TO_INT(g_object_get_data(item,
                                                     "windowing-type"));
}

static void
preserve_changed_cb(GtkToggleButton *button, FFTArgs *args)
{
    args->preserve = gtk_toggle_button_get_active(button);
}

static void
fft_dialog_update(FFTControls *controls,
                     FFTArgs *args)
{
    /*
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->angle),
                             args->angle);
     */
    gwy_option_menu_set_history(controls->interp, "interpolation-type",
                                args->interp);
    gwy_option_menu_set_history(controls->window, "windowing-type",
                                args->window);
}

static GtkWidget*
gwy_option_menu_fft_output(GCallback callback,
                           gpointer cbdata,
                           GwyFFTOutputType current)
{
    static const GwyEnum entries[] = {
        { "Real + Imaginary",  GWY_FFT_OUTPUT_REAL_IMG,  },
        { "Module + Phase",    GWY_FFT_OUTPUT_MOD_PHASE, },
        { "Real",              GWY_FFT_OUTPUT_REAL,      },
        { "Imaginary",         GWY_FFT_OUTPUT_IMG,       },
        { "Module",            GWY_FFT_OUTPUT_MOD,       },
        { "Phase",             GWY_FFT_OUTPUT_PHASE,     },
    };

    return gwy_option_menu_create(entries, G_N_ELEMENTS(entries),
                                  "fft-output-type", callback, cbdata,
                                  current);
}

static const gchar *preserve_key = "/module/fft/preserve";
static const gchar *interp_key = "/module/fft/interp";
static const gchar *window_key = "/module/fft/window";
static const gchar *out_key = "/module/fft/out";

static void
fft_sanitize_args(FFTArgs *args)
{
    args->preserve = !!args->preserve;
    args->interp = CLAMP(args->interp,
                         GWY_INTERPOLATION_ROUND, GWY_INTERPOLATION_NNA);
    args->window = MIN(args->window, GWY_WINDOWING_RECT);
    args->out = MIN(args->out, GWY_FFT_OUTPUT_PHASE);
}

static void
fft_load_args(GwyContainer *container,
              FFTArgs *args)
{
    *args = fft_defaults;

    gwy_container_gis_boolean_by_name(container, preserve_key, &args->preserve);
    gwy_container_gis_enum_by_name(container, interp_key, &args->interp);
    gwy_container_gis_enum_by_name(container, window_key, &args->window);
    gwy_container_gis_enum_by_name(container, out_key, &args->out);
    fft_sanitize_args(args);
}

static void
fft_save_args(GwyContainer *container,
              FFTArgs *args)
{
    gwy_container_set_boolean_by_name(container, preserve_key, args->preserve);
    gwy_container_set_enum_by_name(container, interp_key, args->interp);
    gwy_container_set_enum_by_name(container, window_key, args->window);
    gwy_container_set_enum_by_name(container, out_key, args->out);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
