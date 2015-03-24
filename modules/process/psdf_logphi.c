/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
#include <libprocess/inttrans.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define PSDFLP_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef struct {
    GwyWindowingType window;
    gdouble sigma;
} PSDFLPArgs;

typedef struct {
    PSDFLPArgs *args;
    GtkWidget *window;
    GtkObject *sigma;
} PSDFLPControls;

static gboolean module_register     (void);
static void     psdflp              (GwyContainer *data,
                                     GwyRunType run);
static void     psdflp_do           (const PSDFLPArgs *args,
                                     GwyDataField *dfield,
                                     GwyDataField *lpsdf);
static gboolean psdflp_dialog       (PSDFLPArgs *args);
static void     psdflp_dialog_update(PSDFLPControls *controls,
                                     const PSDFLPArgs *args);
static void     windowing_changed   (PSDFLPControls *controls,
                                     GtkComboBox *combo);
static void     sigma_changed       (PSDFLPControls *controls,
                                     GtkAdjustment *adj);
static void     psdflp_load_args    (GwyContainer *container,
                                     PSDFLPArgs *args);
static void     psdflp_save_args    (GwyContainer *container,
                                     PSDFLPArgs *args);
static void     psdflp_sanitize_args(PSDFLPArgs *args);

static const PSDFLPArgs psdflp_defaults = {
    GWY_WINDOWING_HANN,
    0.0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Two-dimensional FFT (Fast Fourier Transform) transformed to "
       "coordinates (log-frequency, angle)."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("psdf_logphi",
                              (GwyProcessFunc)&psdflp,
                              N_("/_Statistics/_Log-Phi PSDF..."),
                              GWY_STOCK_FFT,
                              PSDFLP_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Compute PSDF in Log-Phi coordinates"));

    return TRUE;
}

static void
psdflp(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *lpsdf;
    PSDFLPArgs args;
    gboolean ok;
    gint id, newid;

    g_return_if_fail(run & PSDFLP_RUN_MODES);

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    psdflp_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_INTERACTIVE) {
        ok = psdflp_dialog(&args);
        psdflp_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    lpsdf = gwy_data_field_new(1, 1, 1.0, 1.0, FALSE);
    psdflp_do(&args, dfield, lpsdf);

    newid = gwy_app_data_browser_add_data_field(lpsdf, data, TRUE);
    g_object_unref(lpsdf);
    gwy_app_set_data_field_title(data, newid, "Log-phi PSDF");
    gwy_app_channel_log_add_proc(data, id, newid);
}

static void
psdflp_do(const PSDFLPArgs *args, GwyDataField *dfield, GwyDataField *lpsdf)
{
    enum { N = 4 };

    GwyDataField *reout, *imout;
    gint pxres, pyres, fxres, fyres;
    gint i, j, fi, pi;
    gdouble *ldata, *redata, *imdata;
    gdouble *cosphi, *sinphi;
    gdouble xreal, yreal, f0, fmax, b, p;

    reout = gwy_data_field_new_alike(dfield, FALSE);
    imout = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_2dfft(dfield, NULL, reout, imout,
                         args->window, GWY_TRANSFORM_DIRECTION_FORWARD,
                         GWY_INTERPOLATION_ROUND, /* Ignored */
                         TRUE, 1);

    pxres = reout->xres;
    pyres = reout->yres;
    redata = gwy_data_field_get_data(reout);
    imdata = gwy_data_field_get_data(imout);
    for (i = 0; i < pxres*pyres; i++)
        redata[i] = redata[i]*redata[i] + imdata[i]*imdata[i];
    gwy_data_field_2dfft_humanize(reout);
    gwy_data_field_filter_gaussian(reout, args->sigma);
    for (i = 0; i < pxres*pyres; i++)
        redata[i] = sqrt(redata[i]);

    fxres = pxres/2;
    fyres = pyres/2;
    gwy_data_field_resample(lpsdf, fxres, fyres, GWY_INTERPOLATION_NONE);
    ldata = gwy_data_field_get_data(lpsdf);

    xreal = dfield->xreal;
    yreal = dfield->yreal;
    f0 = 2.0/MIN(xreal, yreal);
    fmax = 0.5*MIN(pxres/xreal, pyres/yreal);
    if (fmax <= f0) {
        g_warning("Minimum frequency is not smaller than maximum frequency.");
    }
    b = log(fmax/f0)/fyres;

    /* Incorporate some prefactors to sinphi[] and cosphi[], knowing that
     * cosine is only ever used for x and sine for y frequencies. */
    cosphi = g_new(gdouble, (N+1)*fxres);
    sinphi = g_new(gdouble, (N+1)*fxres);
    for (j = 0; j < fxres; j++) {
        gdouble phi_from = 2.0*G_PI*j/fxres;
        gdouble phi_to = 2.0*G_PI*(j + 1.0)/fxres;

        for (pi = 0; pi <= N; pi++) {
            gdouble phi = ((pi + 0.5)*phi_from + (N - 0.5 - pi)*phi_to)/N;
            cosphi[j*(N+1) + pi] = cos(phi)*xreal;
            sinphi[j*(N+1) + pi] = sin(phi)*yreal;
        }
    }

    for (i = 0; i < fyres; i++) {
        gdouble f_from = f0*exp(b*i);
        gdouble f_to = f0*exp(b*(i + 1.0));

        for (j = 0; j < fxres; j++) {
            const gdouble *cosphi_j = cosphi + j*(N+1);
            const gdouble *sinphi_j = sinphi + j*(N+1);
            guint n = 0;
            gdouble s = 0.0;

            for (fi = 0; fi <= N; fi++) {
                gdouble f = ((fi + 0.5)*f_from + (N - 0.5 - fi)*f_to)/N;
                for (pi = 0; pi <= N; pi++) {
                    gdouble x = f*cosphi_j[pi] + pxres/2.0,
                            y = f*sinphi_j[pi] + pyres/2.0;

                    if (G_UNLIKELY(x < 0.5
                                   || y < 0.5
                                   || x > pxres - 1.5
                                   || y > pyres - 1.5))
                        continue;

                    p = gwy_data_field_get_dval(reout, x, y,
                                                GWY_INTERPOLATION_SCHAUM);
                    s += p;
                    n++;
                }
            }

            if (!n)
                n = 1;

            ldata[i*fxres + j] = 2.0*G_PI/fxres * s/n*(f_to - f_from);
        }
    }

    g_object_unref(imout);
    g_object_unref(reout);

    gwy_data_field_set_xreal(lpsdf, 2.0*G_PI);
    gwy_data_field_set_xoffset(lpsdf, 0.0);
    gwy_data_field_set_yreal(lpsdf, log(fmax/f0));
    gwy_data_field_set_yoffset(lpsdf, log(f0));
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(lpsdf), "");
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(lpsdf), "");
    gwy_data_field_normalize(lpsdf);
}

static gboolean
psdflp_dialog(PSDFLPArgs *args)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table;
    PSDFLPControls controls;
    gint response, row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Log-Phi PSDF"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(2, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.window
        = gwy_enum_combo_box_new(gwy_windowing_type_get_enum(), -1,
                                 G_CALLBACK(gwy_enum_combo_box_update_int),
                                 &args->window, args->window, TRUE);
    gwy_table_attach_row(table, row, _("_Windowing type:"), NULL,
                         controls.window);
    g_signal_connect_swapped(controls.window, "changed",
                             G_CALLBACK(windowing_changed), &controls);
    row++;

    controls.sigma = gtk_adjustment_new(args->sigma, 0.0, 40.0, 0.01, 1.0, 0);
    gwy_table_attach_hscale(table, row, _("Gaussian _smoothing:"), "px",
                            controls.sigma, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.sigma, "value-changed",
                             G_CALLBACK(sigma_changed), &controls);
    row++;

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
            *args = psdflp_defaults;
            psdflp_dialog_update(&controls, args);
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
psdflp_dialog_update(PSDFLPControls *controls, const PSDFLPArgs *args)
{
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->window),
                                  args->window);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma), args->sigma);
}

static void
windowing_changed(PSDFLPControls *controls, GtkComboBox *combo)
{
    controls->args->window = gwy_enum_combo_box_get_active(combo);
}

static void
sigma_changed(PSDFLPControls *controls, GtkAdjustment *adj)
{
    controls->args->sigma = gtk_adjustment_get_value(adj);
}

static const gchar window_key[] = "/module/psdf_logphi/window";
static const gchar sigma_key[]  = "/module/psdf_logphi/sigma";

static void
psdflp_sanitize_args(PSDFLPArgs *args)
{
    args->window = gwy_enum_sanitize_value(args->window,
                                           GWY_TYPE_WINDOWING_TYPE);
    args->sigma = CLAMP(args->sigma, 0.0, 40.0);
}

static void
psdflp_load_args(GwyContainer *container,
                 PSDFLPArgs *args)
{
    *args = psdflp_defaults;

    gwy_container_gis_enum_by_name(container, window_key, &args->window);
    gwy_container_gis_double_by_name(container, sigma_key, &args->sigma);
    psdflp_sanitize_args(args);
}

static void
psdflp_save_args(GwyContainer *container,
                 PSDFLPArgs *args)
{
    gwy_container_set_enum_by_name(container, window_key, args->window);
    gwy_container_set_double_by_name(container, sigma_key, args->sigma);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
