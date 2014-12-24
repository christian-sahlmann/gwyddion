/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libprocess/arithmetic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define SEMSIM_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    MAX_SIZE = 200,
    ERF_TABLE_SIZE = 16384
};

typedef enum {
    SEMSIM_METHOD_MONTECARLO = 0,
    SEMSIM_METHOD_INTEGRATION = 1,
} SEMsimMethod;

typedef struct {
    SEMsimMethod method;
    gdouble sigma;
    gdouble quality;
} SEMsimArgs;

typedef struct {
    SEMsimArgs *args;
    GtkObject *sigma;
    GtkWidget *value_sigma;
    GwySIValueFormat *format_sigma;
    GSList *method;
    GtkObject *quality;
} SEMsimControls;

typedef struct {
    gdouble w;
    gint k;
} WeightItem;

typedef struct {
    gdouble dx;
    gdouble dy;
    gdouble dz;
    gdouble *erftable;
    gint extv;
    gint exth;
    gint extxres;
    gint extyres;
    GwyDataField *extfield;
} SEMsimCommon;

static gboolean module_register      (void);
static void     semsim               (GwyContainer *data,
                                      GwyRunType run);
static gboolean semsim_dialog        (SEMsimArgs *args,
                                      GwyDataField *dfield);
static void     sigma_changed        (SEMsimControls *controls,
                                      GtkAdjustment *adj);
static void     method_changed       (GtkToggleButton *toggle,
                                      SEMsimControls *controls);
static void     quality_changed      (SEMsimControls *controls,
                                      GtkAdjustment *adj);
static void     semsim_dialog_reset  (SEMsimControls *controls);
static gboolean semsim_do            (GwyDataField *dfield,
                                      GwyDataField *show,
                                      SEMsimArgs *args);
static gboolean semsim_do_integration(SEMsimCommon *common,
                                      GwyDataField *show,
                                      SEMsimArgs *args);
static gboolean semsim_do_montecarlo (SEMsimCommon *common,
                                      GwyDataField *show,
                                      SEMsimArgs *args);
static gdouble* create_erf_table     (GwyDataField *dfield,
                                      gdouble sigma,
                                      gdouble *zstep);
static void     semsim_load_args     (GwyContainer *settings,
                                      SEMsimArgs *args);
static void     semsim_save_args     (GwyContainer *settings,
                                      SEMsimArgs *args);

static const SEMsimArgs semsim_defaults = {
    SEMSIM_METHOD_MONTECARLO,
    10.0,
    3.0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Simple SEM image simulation from topography."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("semsim",
                              (GwyProcessFunc)&semsim,
                              N_("/_Presentation/_SEM Image..."),
                              NULL,
                              SEMSIM_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Simple SEM simulation from topography"));

    return TRUE;
}

static void
semsim(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *showfield;
    SEMsimArgs args;
    GQuark squark;
    gint id;

    g_return_if_fail(run & SEMSIM_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_SHOW_FIELD_KEY, &squark,
                                     0);
    g_return_if_fail(dfield && squark);

    if (!gwy_si_unit_equal(gwy_data_field_get_si_unit_xy(dfield),
                           gwy_data_field_get_si_unit_z(dfield))) {
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new
                        (gwy_app_find_window_for_channel(data, id),
                         GTK_DIALOG_DESTROY_WITH_PARENT,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_OK,
                         _("%s: Lateral dimensions and value must "
                           "be the same physical quantity."),
                         _("SEM Image"));
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    semsim_load_args(gwy_app_settings_get(), &args);

    if (run == GWY_RUN_INTERACTIVE) {
        gboolean ok = semsim_dialog(&args, dfield);
        semsim_save_args(gwy_app_settings_get(), &args);
        if (!ok)
            return;
    }

    showfield = gwy_data_field_new_alike(dfield, FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(showfield), NULL);

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                       _("SEM image simulation..."));
    gwy_app_wait_set_fraction(0.0);

    if (semsim_do(dfield, showfield, &args)) {
        gwy_app_undo_qcheckpointv(data, 1, &squark);
        gwy_container_set_object(data, squark, showfield);
        gwy_app_channel_log_add_proc(data, id, id);
    }
    g_object_unref(showfield);

    gwy_app_wait_finish();
}

static gboolean
semsim_dialog(SEMsimArgs *args, GwyDataField *dfield)
{
    enum { RESPONSE_RESET = 1 };
    GtkWidget *dialog, *table, *label;
    SEMsimControls controls;
    gint response;
    gint row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("SEM Image"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(6, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
    row = 0;

    controls.sigma = gtk_adjustment_new(args->sigma, 0.5, MAX_SIZE, 0.1, 10, 0);
    gwy_table_attach_hscale(table, row++, _("_Integration radius:"), "px",
                            controls.sigma, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.sigma, "value-changed",
                             G_CALLBACK(sigma_changed), &controls);
    row++;

    controls.value_sigma = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.value_sigma), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.value_sigma,
                     2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    controls.format_sigma
        = gwy_data_field_get_value_format_xy(dfield,
                                             GWY_SI_UNIT_FORMAT_VFMARKUP, NULL);
    controls.format_sigma->magnitude /= gwy_data_field_get_xmeasure(dfield);
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), controls.format_sigma->units);
    gtk_table_attach(GTK_TABLE(table), label,
                     3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    label = gtk_label_new(_("Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.method
        = gwy_radio_buttons_createl(G_CALLBACK(method_changed), &controls,
                                    args->method,
                                    _("Integration"),
                                    SEMSIM_METHOD_INTEGRATION,
                                    _("Monte Carlo"),
                                    SEMSIM_METHOD_MONTECARLO,
                                    NULL);
    row = gwy_radio_buttons_attach_to_table(controls.method, GTK_TABLE(table),
                                            3, row);

    controls.quality = gtk_adjustment_new(args->quality, 1.0, 7.0, 0.1, 1.0, 0);
    gwy_table_attach_hscale(table, row++, _("_Quality:"), NULL,
                            controls.quality, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.quality, "value-changed",
                             G_CALLBACK(quality_changed), &controls);
    row++;

    method_changed(NULL, &controls);
    sigma_changed(&controls, GTK_ADJUSTMENT(controls.sigma));

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            gwy_si_unit_value_format_free(controls.format_sigma);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            semsim_dialog_reset(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    gwy_si_unit_value_format_free(controls.format_sigma);

    return TRUE;
}

static void
semsim_dialog_reset(SEMsimControls *controls)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->sigma),
                             semsim_defaults.sigma);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->quality),
                             semsim_defaults.quality);
    gwy_radio_buttons_set_current(controls->method, semsim_defaults.method);
}

static void
sigma_changed(SEMsimControls *controls, GtkAdjustment *adj)
{
    gdouble sigma;
    gchar buf[24];

    sigma = controls->args->sigma = gtk_adjustment_get_value(adj);
    g_snprintf(buf, sizeof(buf), "%.*f",
               controls->format_sigma->precision,
               sigma/controls->format_sigma->magnitude);
    gtk_label_set_text(GTK_LABEL(controls->value_sigma), buf);
}

static void
method_changed(G_GNUC_UNUSED GtkToggleButton *toggle,
               SEMsimControls *controls)
{
    controls->args->method = gwy_radio_buttons_get_current(controls->method);
    if (controls->args->method == SEMSIM_METHOD_MONTECARLO) {
        gwy_table_hscale_set_sensitive(controls->quality, TRUE);
    }
    else {
        gwy_table_hscale_set_sensitive(controls->quality, FALSE);
    }
}

static void
quality_changed(SEMsimControls *controls,
                GtkAdjustment *adj)
{
    controls->args->quality = gtk_adjustment_get_value(adj);
}

static gboolean
semsim_do(GwyDataField *dfield,
          GwyDataField *show,
          SEMsimArgs *args)
{
    SEMsimCommon common;
    gint xres = dfield->xres, yres = dfield->yres;
    gdouble sigma;
    gboolean ok;

    common.dx = gwy_data_field_get_xmeasure(dfield);
    common.dy = gwy_data_field_get_ymeasure(dfield);
    sigma = args->sigma * common.dx;
    common.exth = (gint)ceil(5.5*sigma/common.dx);
    common.extv = (gint)ceil(5.5*sigma/common.dy);
    common.extxres = xres + 2*common.exth;
    common.extyres = yres + 2*common.extv;
    common.extfield = gwy_data_field_extend(dfield,
                                            common.exth, common.exth,
                                            common.extv, common.extv,
                                            GWY_EXTERIOR_BORDER_EXTEND,
                                            0.0, FALSE);
    common.erftable = create_erf_table(dfield, sigma, &common.dz);

    if (args->method == SEMSIM_METHOD_INTEGRATION)
        ok = semsim_do_integration(&common, show, args);
    else
        ok = semsim_do_montecarlo(&common, show, args);

    if (ok)
        gwy_data_field_normalize(show);

    g_free(common.erftable);
    g_object_unref(common.extfield);

    return ok;
}

static gboolean
semsim_do_integration(SEMsimCommon *common,
                      GwyDataField *show,
                      SEMsimArgs *args)
{
    gint xres = show->xres, yres = show->yres, i, j, k;
    gdouble dx = common->dx, dy = common->dy, dz = common->dz;
    gdouble sigma_r2 = G_SQRT2*args->sigma*dx;
    const gdouble *d = gwy_data_field_get_data_const(common->extfield);
    gdouble *s = gwy_data_field_get_data(show);
    gdouble *erftable = common->erftable;
    WeightItem *weight_table;
    gint exth = common->exth, extv = common->extv, extxres = common->extxres;
    gboolean ok = TRUE;
    gint nw;

    weight_table = g_new(WeightItem, (2*extv + 1)*(2*exth + 1));
    nw = 0;
    for (i = -extv; i <= extv; i++) {
        gdouble x = i*dy/sigma_r2;
        for (j = -exth; j <= exth; j++) {
            gdouble y = j*dx/sigma_r2;
            gdouble w = exp(-(x*x + y*y));
            if (w >= 1e-6) {
                weight_table[nw].w = w;
                weight_table[nw].k = (i + extv)*extxres + (j + exth);
                nw++;
            }
        }
    }

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble sum = 0.0, z0 = d[(i + extv)*extxres + (j + exth)];
            for (k = 0; k < nw; k++) {
                gdouble z = d[i*extxres + j + weight_table[k].k];
                gdouble w = weight_table[k].w;
                if (z >= z0)
                    sum -= w*erftable[GWY_ROUND((z - z0)/dz)];
                else
                    sum += w*erftable[GWY_ROUND((z0 - z)/dz)];
            }
            s[i*xres + j] = sum;
        }
        if (!gwy_app_wait_set_fraction((i + 1.0)/yres)) {
            ok = FALSE;
            break;
        }
    }

    g_free(weight_table);
    return ok;
}

static gboolean
semsim_do_montecarlo(SEMsimCommon *common,
                     GwyDataField *show,
                     SEMsimArgs *args)
{
    gint xres = show->xres, yres = show->yres, i, j, k;
    gdouble dx = common->dx, dy = common->dy, dz = common->dz;
    gdouble sigma_r2 = G_SQRT2*args->sigma*dx;
    const gdouble *d = gwy_data_field_get_data_const(common->extfield);
    gdouble *s = gwy_data_field_get_data(show);
    gdouble *erftable = common->erftable;
    gint exth = common->exth, extv = common->extv;
    gint extxres = common->extxres, extyres = common->extyres;
    gdouble noise_limit = pow10(-args->quality);
    gint miniter = (gint)ceil(10*args->quality);
    GRand *rng = g_rand_new();
    gboolean ok = TRUE;

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gdouble sum = 0.0, sum2 = 0.0, ss;
            gdouble z0 = d[(i + extv)*extxres + (j + exth)];

            k = 1;
            while (TRUE) {
                gdouble r = sigma_r2*sqrt(-log(1.0 - g_rand_double(rng)));
                gdouble phi = 2.0*G_PI*g_rand_double(rng);
                gdouble x = r*cos(phi), y = r*sin(phi), z;
                gint ii, jj;

                jj = j + exth + GWY_ROUND(x/dx);
                if (jj < 0 || jj > extxres-1)
                    continue;

                ii = i + extv + GWY_ROUND(y/dy);
                if (ii < 0 || ii > extyres-1)
                    continue;

                z = d[ii*extxres + jj];
                if (z >= z0)
                    ss = -erftable[GWY_ROUND((z - z0)/dz)];
                else
                    ss = erftable[GWY_ROUND((z0 - z)/dz)];

                sum += ss;
                sum2 += ss*ss;
                if (k - miniter >= 0 && (k - miniter) % 5 == 0) {
                    gdouble mean = sum/k;
                    gdouble disp = sum2/k - mean*mean;

                    mean = 0.5*(1.0 + mean);
                    disp /= 2.0*k;
                    if (disp < noise_limit*mean*(1.0 - mean))
                        break;
                }
                k++;
            }

            s[i*xres + j] = sum/k;
        }
        if (!gwy_app_wait_set_fraction((i + 1.0)/yres)) {
            ok = FALSE;
            break;
        }
    }

    g_rand_free(rng);

    return ok;
}

static gdouble*
create_erf_table(GwyDataField *dfield, gdouble sigma, gdouble *zstep)
{
    gdouble min, max, dz;
    gdouble *table;
    gint i;

    gwy_data_field_get_min_max(dfield, &min, &max);
    dz = (max - min)/(ERF_TABLE_SIZE - 1);
    table = g_new(gdouble, ERF_TABLE_SIZE + 1);
    for (i = 0; i <= ERF_TABLE_SIZE; i++)
        table[i] = erf(i*dz/(G_SQRT2*sigma));

    *zstep = dz;
    return table;
}

static const gchar method_key[]  = "/module/semsim/method";
static const gchar quality_key[] = "/module/semsim/quality";
static const gchar sigma_key[]   = "/module/semsim/sigma";

static void
semsim_sanitize_args(SEMsimArgs *args)
{
    args->sigma = CLAMP(args->sigma, 0.5, MAX_SIZE);
    if (args->method != SEMSIM_METHOD_INTEGRATION)
        args->method = SEMSIM_METHOD_MONTECARLO;
    args->quality = CLAMP(args->quality, 1.0, 7.0);
}

static void
semsim_load_args(GwyContainer *settings,
                 SEMsimArgs *args)
{
    *args = semsim_defaults;
    gwy_container_gis_double_by_name(settings, sigma_key, &args->sigma);
    gwy_container_gis_double_by_name(settings, quality_key, &args->quality);
    gwy_container_gis_enum_by_name(settings, method_key, &args->method);
    semsim_sanitize_args(args);
}

static void
semsim_save_args(GwyContainer *settings,
                 SEMsimArgs *args)
{
    gwy_container_set_double_by_name(settings, sigma_key, args->sigma);
    gwy_container_set_double_by_name(settings, quality_key, args->quality);
    gwy_container_set_enum_by_name(settings, method_key, args->method);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
