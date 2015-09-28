/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek,
 *  Daniil Bratashov, Evgeniy Ryabov.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net,
 *  dn2010@gmail.com, k1u2r3ka@mail.ru.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libprocess/brick.h>
#include <libprocess/datafield.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule-volume.h>
#include <app/gwyapp.h>

#define KMEDIANS_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    RESPONSE_RESET = 1,
};

typedef struct {
    gint k;              /* number of clusters */
    gdouble epsilon;     /* convergence precision */
    gint max_iterations; /* maximum number of main cycle iterations */
    gboolean normalize;  /* normalize brick before K-medians run */
} KMediansArgs;

typedef struct {
    KMediansArgs *args;
    GwySIValueFormat *wvf;
    GtkObject *k;
    GtkObject *epsilon;
    GtkObject *max_iterations;
    GtkWidget *normalize;
} KMediansControls;

static gboolean  module_register       (void);
static void      volume_kmedians       (GwyContainer *data,
                                        GwyRunType run);
static void      kmedians_dialog       (GwyContainer *data,
                                        GwyBrick *brick,
                                        KMediansArgs *args);
static void      kmedians_dialog_update(KMediansControls *controls,
                                        KMediansArgs *args);
static void      kmedians_values_update(KMediansControls *controls,
                                        KMediansArgs *args);
static GwyBrick* normalize_brick       (GwyBrick *brick,
                                        GwyDataField *intfield);
static void      volume_kmedians_do    (GwyContainer *data,
                                        KMediansArgs *args);
static void      kmedians_load_args    (GwyContainer *container,
                                        KMediansArgs *args);
static void      kmedians_save_args    (GwyContainer *container,
                                        KMediansArgs *args);
static gint      compare_func          (gconstpointer a,
                                        gconstpointer b,
                                        gpointer user_data);

static const KMediansArgs kmedians_defaults = {
    10,
    1e-12,
    100,
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Calculates K-medians clustering on volume data."),
    "Daniil Bratashov <dn2010@gmail.com> & Evgeniy Ryabov <k1u2r3ka@mail.ru>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov & Evgeniy Ryabov",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_volume_func_register("kmedians",
                             (GwyVolumeFunc)&volume_kmedians,
                             N_("/_K-medians clustering..."),
                             NULL,
                             KMEDIANS_RUN_MODES,
                             GWY_MENU_FLAG_VOLUME,
                             N_("Calculate K-medians clustering on volume data"));

    return TRUE;
}

static void
volume_kmedians(GwyContainer *data, GwyRunType run)
{
    KMediansArgs args;
    GwyBrick *brick = NULL;
    gint id;

    g_return_if_fail(run & KMEDIANS_RUN_MODES);
    g_return_if_fail(data);

    kmedians_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));
    if (run == GWY_RUN_INTERACTIVE) {
        kmedians_dialog(data, brick, &args);
        kmedians_save_args(gwy_app_settings_get(), &args);
    }
    else if (run == GWY_RUN_IMMEDIATE) {
        volume_kmedians_do(data, &args);
    }
}

static void
kmedians_dialog(GwyContainer *data, GwyBrick *brick, KMediansArgs *args)
{
    GtkWidget *dialog, *table;
    gint response;
    KMediansControls controls;
    gint row = 0;

    controls.args = args;
    controls.wvf = gwy_brick_get_value_format_w(brick,
                                                GWY_SI_UNIT_FORMAT_VFMARKUP,
                                                NULL);

    dialog = gtk_dialog_new_with_buttons(_("K-Medians"), NULL, 0, NULL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_volume_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(4, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       TRUE, TRUE, 4);

    controls.k = gtk_adjustment_new(args->k, 2, 100, 1, 10, 0);
    gwy_table_attach_hscale(table, row,
                            _("_Number of clusters:"), NULL,
                            controls.k, GWY_HSCALE_SQRT);
    row++;

    controls.epsilon = gtk_adjustment_new(-log10(args->epsilon),
                                          1.0, 20.0, 0.01, 1.0, 0);
    gwy_table_attach_hscale(table, row,
                            _("Convergence _precision digits:"), NULL,
                            controls.epsilon, GWY_HSCALE_DEFAULT);
    row++;

    controls.max_iterations = gtk_adjustment_new(args->max_iterations,
                                                 1, 10000, 1, 1, 0);
    gwy_table_attach_hscale(table, row,
                            _("_Max. iterations:"), NULL,
                            controls.max_iterations, GWY_HSCALE_LOG);
    row++;

    controls.normalize
        = gtk_check_button_new_with_mnemonic(_("_Normalize"));
    gtk_table_attach_defaults(GTK_TABLE(table), controls.normalize,
                              0, 3, row, row+1);

    kmedians_dialog_update(&controls, args);
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            kmedians_values_update(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            gwy_si_unit_value_format_free(controls.wvf);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = kmedians_defaults;
            kmedians_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    kmedians_values_update(&controls, args);
    gtk_widget_destroy(dialog);
    gwy_si_unit_value_format_free(controls.wvf);
    volume_kmedians_do(data, args);
}

/* XXX: Duplicate with volume_kmeans.c */
static GwyBrick*
normalize_brick(GwyBrick *brick, GwyDataField *intfield)
{
    GwyBrick *result;
    gdouble wmin, dataval, dataval2, integral;
    gint i, j, l, k, xres, yres, zres;
    gint len = 25;
    const gdouble *olddata;
    gdouble *newdata, *intdata;

    result = gwy_brick_new_alike(brick, TRUE);
    wmin = gwy_brick_get_min(brick);
    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    olddata = gwy_brick_get_data_const(brick);
    newdata = gwy_brick_get_data(result);
    intdata = gwy_data_field_get_data(intfield);

    for (i = 0; i < xres; i++) {
        for (j = 0; j < yres; j++) {
            integral = 0;
            for (l = 0; l < zres; l++) {
                dataval = *(olddata + l * xres * yres + j * xres + i);
                wmin = dataval;
                for (k = -len; k < len; k++) {
                    if (l + k < 0) {
                        k = -l;
                        continue;
                    }
                    if (l + k >= zres)
                        break;
                    dataval2 = *(olddata + (l + k) * xres * yres
                                                        + j * xres + i);
                    if (dataval2 < wmin)
                        wmin = dataval2;
                }
                integral += (dataval - wmin);
            }
            for (l = 0; l < zres; l++) {
                dataval = *(olddata + l * xres * yres + j * xres + i);
                wmin = dataval;
                for (k = -len; k < len; k++) {
                    if (l + k < 0) {
                        k = -l;
                        continue;
                    }
                    if (l + k >= zres)
                        break;
                    dataval2 = *(olddata + (l + k)* xres * yres + j * xres + i);
                    if (dataval2 < wmin)
                        wmin = dataval2;
                }
                if (integral != 0.0) {
                    *(newdata + l * xres * yres + j * xres + i)
                                   = (dataval - wmin) * zres / integral;
                }
            }
            *(intdata + j * xres + i) = integral / zres;
        }
    }

    return result;
}

static void
kmedians_values_update(KMediansControls *controls,
                     KMediansArgs *args)
{
    args->k = gwy_adjustment_get_int(GTK_ADJUSTMENT(controls->k));
    args->epsilon = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->epsilon));
    args->epsilon = pow(0.1, args->epsilon);
    args->max_iterations
        = gwy_adjustment_get_int(GTK_ADJUSTMENT(controls->max_iterations));
    args->normalize
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->normalize));
}

static gint
compare_func (gconstpointer a, gconstpointer b,
              G_GNUC_UNUSED gpointer user_data)
{
    const gdouble *aa = a;
    const gdouble *bb = b;

    return *aa - *bb;
}

static void
kmedians_dialog_update(KMediansControls *controls,
                       KMediansArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->k), args->k);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->epsilon),
                             -log10(args->epsilon));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->max_iterations),
                             args->max_iterations);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->normalize),
                                 args->normalize);
}

static void
volume_kmedians_do(GwyContainer *container, KMediansArgs *args)
{
    GwyBrick *brick = NULL, *normalized = NULL;
    GwyDataField *dfield = NULL, *errormap = NULL, *intmap = NULL;
    GwyGraphCurveModel *gcmodel;
    GwyGraphModel *gmodel;
    GwyDataLine *calibration = NULL;
    GwySIUnit *siunit;
    const GwyRGBA *rgba;
    gint id;
    gchar *description;
    GRand *rand;
    const gdouble *data;
    gdouble *centers, *oldcenters, *plane, *data1, *xdata, *ydata;
    gdouble *errordata;
    gdouble min, dist, xreal, yreal, zreal, xoffset, yoffset, zoffset;
    gdouble epsilon = args->epsilon;
    gint xres, yres, zres, i, j, l, c, newid;
    gint *npix;
    gint k = args->k;
    gint iterations = 0;
    gint max_iterations = args->max_iterations;
    gboolean converged = FALSE, cancelled = FALSE;
    gboolean normalize = args->normalize;

    gwy_app_data_browser_get_current(GWY_APP_BRICK, &brick,
                                     GWY_APP_BRICK_ID, &id,
                                     0);
    g_return_if_fail(GWY_IS_BRICK(brick));

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    xreal = gwy_brick_get_xreal(brick);
    yreal = gwy_brick_get_yreal(brick);
    zreal = gwy_brick_get_zreal(brick);
    xoffset = gwy_brick_get_xoffset(brick);
    yoffset = gwy_brick_get_yoffset(brick);
    zoffset = gwy_brick_get_zoffset(brick);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, TRUE);
    gwy_data_field_set_xoffset(dfield, xoffset);
    gwy_data_field_set_yoffset(dfield, yoffset);

    siunit = gwy_brick_get_si_unit_x(brick);
    gwy_data_field_set_si_unit_xy(dfield, siunit);

    intmap = gwy_data_field_new_alike(dfield, TRUE);
    siunit = gwy_brick_get_si_unit_w(brick);
    gwy_data_field_set_si_unit_z(intmap, siunit);

    gwy_app_wait_start(gwy_app_find_window_for_volume(container, id),
                       _("Initializing..."));

    if (normalize) {
        normalized = normalize_brick(brick, intmap);
        data = gwy_brick_get_data_const(normalized);
    }
    else {
        data = gwy_brick_get_data_const(brick);
    }

    centers = g_new(gdouble, zres * k);
    oldcenters = g_new(gdouble, zres * k);
    plane = g_new(gdouble, xres * yres * k);
    npix = g_new(gint, k);
    data1 = gwy_data_field_get_data(dfield);

    rand = g_rand_new();
    for (c = 0; c < k; c++) {
        i = g_rand_int_range(rand, 0, xres);
        j = g_rand_int_range(rand, 0, yres);
        for (l = 0; l < zres; l++) {
            *(centers + c * zres + l)
                             = *(data + l * xres * yres + j * xres + i);
        };
    };
    g_rand_free(rand);

    if (!gwy_app_wait_set_message(_("K-medians iteration...")))
        cancelled = TRUE;

    while (!converged && !cancelled) {
        if (!gwy_app_wait_set_fraction((gdouble)iterations/max_iterations)) {
            cancelled = TRUE;
            break;
        }

        /* pixels belong to cluster with min distance */
        for (j = 0; j < yres; j++)
            for (i = 0; i < xres; i++) {
                *(data1 + j * xres + i) = 0;
                min = G_MAXDOUBLE;
                for (c = 0; c < k; c++ ) {
                    dist = 0;
                    for (l = 0; l < zres; l++) {
                        *(oldcenters + c * zres + l)
                                            = *(centers + c * zres + l);
                        dist
                            += (*(data + l * xres * yres + j * xres + i)
                              - *(centers + c * zres + l))
                             * (*(data + l * xres * yres + j * xres + i)
                              - *(centers + c * zres + l));
                    }
                    if (dist < min) {
                        min = dist;
                        *(data1 + j * xres + i) = c;
                    }
                }
            }

        /* We're calculating median per one coordinate of all pixels
         * that belongs to same cluster and use it as this coordinate
         * position for cluster center */
        for (l = 0; l < zres; l++) {
            for (c = 0; c < k; c++) {
                *(npix + c) = 0;
            }

            for (j = 0; j < yres; j++)
                for (i = 0; i < xres; i++) {
                    c = (gint)(*(data1 + j * xres + i));
                    (*(npix + c))++;
                    *(plane + c * xres * yres + *(npix + c) - 1)
                             = *(data + l * xres * yres + j * xres + i);
                }
            for (c = 0; c < k; c++) {
                g_qsort_with_data (plane + c * xres * yres,
                                   *(npix + c),
                                   sizeof(gdouble),
                                   compare_func,
                                   NULL);
                *(centers + c * zres + l)
                         = *(plane + c * xres * yres + *(npix + c) / 2);
            }
        }

        converged = TRUE;
        for (c = 0; c < k; c++) {
            for (l = 0; l < zres; l++)
                if (fabs(*(oldcenters + c * zres + l)
                               - *(centers + c * zres + l)) > epsilon) {
                    converged = FALSE;
                    break;
                }
        }
        if (iterations == max_iterations) {
            converged = TRUE;
        }
        iterations++;
    }

    gwy_app_wait_finish();
    if (cancelled)
        goto fail;

    errormap = gwy_data_field_new_alike(dfield, TRUE);
    if (!normalize) {
        siunit = gwy_brick_get_si_unit_w(brick);
        siunit = gwy_si_unit_duplicate(siunit);
        gwy_data_field_set_si_unit_z(errormap, siunit);
        g_object_unref(siunit);
    }
    errordata = gwy_data_field_get_data(errormap);

    for (i = 0; i < xres; i++) {
        for (j = 0; j < yres; j++) {
            dist = 0.0;
            c = (gint)(*(data1 + j * xres + i));
            for (l = 0; l < zres; l++) {
                dist += (*(data + l * xres * yres + j * xres + i)
                         - *(centers + c * zres + l))
                    * (*(data + l * xres * yres + j * xres + i)
                       - *(centers + c * zres + l));
            }
            *(errordata + j * xres + i) = sqrt(dist);
        }
    }

    gwy_data_field_add(dfield, 1.0);
    newid = gwy_app_data_browser_add_data_field(dfield,
                                                container, TRUE);
    gwy_object_unref(dfield);
    description = gwy_app_get_brick_title(container, id);
    gwy_app_set_data_field_title(container, newid,
                                 g_strdup_printf(_("K-medians cluster of %s"),
                                                 description));
    gwy_app_channel_log_add(container, -1, newid,
                            "volume::kmedians",
                            NULL);

    newid = gwy_app_data_browser_add_data_field(errormap,
                                                container, TRUE);
    gwy_object_unref(errormap);
    gwy_app_set_data_field_title(container, newid,
                                 g_strdup_printf(_("K-medians error of %s"),
                                                 description));

    gwy_app_channel_log_add(container, -1, newid,
                            "volume::kmedians",
                            NULL);

    if (normalize) {
        newid = gwy_app_data_browser_add_data_field(intmap,
                                                    container, TRUE);
        gwy_object_unref(intmap);
        gwy_app_set_data_field_title(container, newid,
                                     g_strdup_printf(_("Pre-normalized "
                                                       "intensity of %s"),
                                                     description));

        gwy_app_channel_log_add(container, -1, newid,
                                "volume::kmeans", NULL);
    }

    g_free(description);

    gmodel = gwy_graph_model_new();
    calibration = gwy_brick_get_zcalibration(brick);
    ydata = g_new(gdouble, zres);
    xdata = g_new(gdouble, zres);
    if (calibration) {
        memcpy(xdata, gwy_data_line_get_data(calibration),
               zres*sizeof(gdouble));
        siunit = gwy_data_line_get_si_unit_y(calibration);
    }
    else {
        for (i = 0; i < zres; i++)
            *(xdata + i) = zreal * i / zres + zoffset;
        siunit = gwy_brick_get_si_unit_z(brick);
    }
    for (c = 0; c < k; c++) {
        memcpy(ydata, centers + c * zres, zres * sizeof(gdouble));
        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, zres);
        rgba = gwy_graph_get_preset_color(c);
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "description",
                     g_strdup_printf(_("K-medians center %d"), c + 1),
                     "color", rgba,
                     NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }
    g_free(xdata);
    g_object_set(gmodel,
                 "si-unit-x", siunit,
                 "si-unit-y", gwy_brick_get_si_unit_w(brick),
                 "axis-label-bottom", "x",
                 "axis-label-left", "y",
                 NULL);
    gwy_app_data_browser_add_graph_model(gmodel, container, TRUE);
    g_object_unref(gmodel);

    gwy_app_volume_log_add_volume(container, id, id);

fail:
    gwy_object_unref(errormap);
    gwy_object_unref(intmap);
    gwy_object_unref(dfield);
    gwy_object_unref(normalized);
    g_free(npix);
    g_free(plane);
    g_free(oldcenters);
    g_free(centers);
}

static const gchar epsilon_key[]        = "/module/kmedians/epsilon";
static const gchar kmedians_k_key[]     = "/module/kmedians/k";
static const gchar max_iterations_key[] = "/module/kmedians/max_iterations";
static const gchar normalize_key[]      = "/module/kmedians/normalize";

static void
kmedians_sanitize_args(KMediansArgs *args)
{
    args->k = CLAMP(args->k, 2, 100);
    args->epsilon = CLAMP(args->epsilon, 1e-20, 0.1);
    args->max_iterations = CLAMP(args->max_iterations, 0, 10000);
    args->normalize = !!args->normalize;
}

static void
kmedians_load_args(GwyContainer *container,
                   KMediansArgs *args)
{
    *args = kmedians_defaults;

    gwy_container_gis_int32_by_name(container, kmedians_k_key, &args->k);
    gwy_container_gis_double_by_name(container, epsilon_key, &args->epsilon);
    gwy_container_gis_int32_by_name(container, max_iterations_key,
                                    &args->max_iterations);
    gwy_container_gis_boolean_by_name(container, normalize_key,
                                      &args->normalize);

    kmedians_sanitize_args(args);
}

static void
kmedians_save_args(GwyContainer *container,
                   KMediansArgs *args)
{
    gwy_container_set_int32_by_name(container, kmedians_k_key, args->k);
    gwy_container_set_double_by_name(container, epsilon_key, args->epsilon);
    gwy_container_set_int32_by_name(container, max_iterations_key,
                                    args->max_iterations);
    gwy_container_set_boolean_by_name(container, normalize_key,
                                      args->normalize);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
