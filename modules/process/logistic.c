/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti), Petr Klapetek,
 *  Daniil Bratashov.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net,
 *  dn2010@gwyddion.net
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
#include <glib.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwydgets.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <string.h>

#define LOGISTIC_RUN_MODES GWY_RUN_INTERACTIVE

#define FWHM2SIGMA (1.0/(2.0*sqrt(2*G_LN2)))

enum {
    RESPONSE_RESET   = 1,
};

typedef enum {
    LOGISTIC_MODE_TRAIN,
    LOGISTIC_MODE_USE
} LogisticMode;

typedef struct {
    LogisticMode mode;
    gboolean cancelled;
    gboolean use_gaussians;
    gint ngaussians;
    gboolean use_sobel;
    gboolean use_laplasian;
    gboolean use_hessian;
    GwyDataLine *thetas;
    gdouble lambda;
} LogisticArgs;

typedef struct {
    LogisticArgs *args;
    GSList *mode;
    GtkWidget *use_gaussians;
    GtkObject *ngaussians;
    GtkWidget *use_sobel;
    GtkWidget *use_laplasian;
    GtkWidget *use_hessian;
    GtkObject *lambda;
    GtkWidget *dialog;
} LogisticControls;

static gboolean module_register         (void);
static void      logistic_run           (GwyContainer *data,
                                         GwyRunType run);
static void      logistic_dialog        (GwyContainer *data,
                                         LogisticArgs *args);
static GwyBrick* create_feature_vector  (GwyDataField *dfield,
                                         LogisticArgs *args);
static gdouble   cost_function          (GwyBrick *brick,
                                         GwyDataField *mask,
                                         gdouble *thetas,
                                         gdouble *grad,
                                         gdouble lambda);
static void      train_logistic         (GwyContainer *container,
                                         GwyBrick *features,
                                         GwyDataField *mfield,
                                         gdouble *thetas,
                                         gdouble lambda);
/*
static void      predict                (GwyBrick *brick,
                                         gdouble *thetas,
                                         GwyDataField *dfield);
*/
static void      predict_mask           (GwyBrick *brick,
                                         gdouble *thetas,
                                         GwyDataField *mask);
static void      logistic_mode_changed  (GtkWidget *button,
                                         LogisticControls *controls);
static void      logistic_values_update (LogisticControls *controls,
                                         LogisticArgs *args);
static void      logistic_dialog_update (LogisticControls *controls,
                                         LogisticArgs *args);
static void      logistic_invalidate    (LogisticControls *controls);
static void      logistic_filter_dx2    (GwyDataField *dfield);
static void      logistic_filter_dy2    (GwyDataField *dfield);
static void      logistic_filter_dxdy   (GwyDataField *dfield);
static gint      logistic_nfeatures     (LogisticArgs *args);
static void      logistic_load_args     (GwyContainer *settings,
                                         LogisticArgs *args);
static void      logistic_save_args     (GwyContainer *settings,
                                         LogisticArgs *args);
static void      logistic_reset_args    (LogisticArgs *args);
static void      logistic_sanitize_args (LogisticArgs *args);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Trains logistic regression to mark grains."),
    "Daniil Bratashov <dn2010@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("logistic_regression",
                              (GwyProcessFunc)&logistic_run,
                              N_("/_Grains/Logistic _Regression..."),
                              GWY_STOCK_GRAINS,
                              LOGISTIC_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark grains with logistic regression"));

    return TRUE;
}

static void
logistic_run(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    // GwyDataField *preview, *predicted;
    GwyBrick *features;
    gint id;
    // gint xres, yres, newid;
    LogisticArgs args;
    gdouble *thetas;
    GQuark quark;

    g_return_if_fail(run & LOGISTIC_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &quark,
                                     0);
    logistic_load_args(gwy_app_settings_get(), &args);
    logistic_dialog(data, &args);

    if (!args.cancelled) {
        features = create_feature_vector(dfield, &args);

        /*
        xres = gwy_brick_get_xres(features);
        yres = gwy_brick_get_yres(features);
        preview = gwy_data_field_new_alike(dfield, FALSE);
        gwy_brick_extract_plane(features, preview,
                                0, 0, 0, xres, yres, -1, TRUE);
        newid = gwy_app_data_browser_add_brick(features,
                                               preview, data, TRUE);
        g_object_unref(preview);
        */

        thetas = gwy_data_line_get_data(args.thetas);

        if (args.mode == LOGISTIC_MODE_TRAIN) {
            if (mfield) {
                train_logistic(data,
                               features, mfield, thetas, args.lambda);
            }
        }
        else {
            gwy_app_undo_qcheckpointv(data, 1, &quark);

            if (!mfield) {
                mfield = gwy_data_field_new_alike(dfield, TRUE);
                predict_mask(features, thetas, mfield);
                gwy_container_set_object(data, quark, mfield);
                g_object_unref(mfield);
            }
            else {
                predict_mask(features, thetas, mfield);
                gwy_data_field_data_changed(mfield);
            }
            /*
            predicted = gwy_data_field_new_alike(dfield, FALSE);
            predict(features, thetas, predicted);
            newid = gwy_app_data_browser_add_data_field(predicted, data,
                                                        TRUE);
            g_object_unref(predicted);
            */
        }
        logistic_save_args(gwy_app_settings_get(), &args);
        gwy_app_channel_log_add_proc(data, id, id);

        g_object_unref(features);
        g_object_unref(args.thetas);
    }
}

static void
logistic_dialog(G_GNUC_UNUSED GwyContainer *data, LogisticArgs *args)
{
    GtkWidget *dialog, *table, *button;
    gint response, row;
    LogisticControls controls;

    controls.args = args;
    args->cancelled = FALSE;

    dialog = gtk_dialog_new_with_buttons(_("Logistic Regression"),
                                         NULL, 0, NULL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog),
                                    GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    table = gtk_table_new(9, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       TRUE, TRUE, 4);
    row = 0;

    controls.mode = gwy_radio_buttons_createl(G_CALLBACK(logistic_mode_changed),
                                              &controls, args->mode,
                                              _("_Use trained regression"),
                                              LOGISTIC_MODE_USE,
                                              _("_Train logistic regression"),
                                              LOGISTIC_MODE_TRAIN,
                                              NULL);
    button = gwy_radio_buttons_find(controls.mode, LOGISTIC_MODE_TRAIN);
    gtk_table_attach(GTK_TABLE(table), button, 0, 3, row, row+1,
                     GTK_FILL, 0, 0, 0);
    row++;

    button = gwy_radio_buttons_find(controls.mode, LOGISTIC_MODE_USE);
    gtk_table_attach(GTK_TABLE(table), button, 0, 3, row, row+1,
                     GTK_FILL, 0, 0, 0);
    row++;

    gtk_table_attach(GTK_TABLE(table),
                     gwy_label_new_header(_("Features")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.use_gaussians
              = gtk_check_button_new_with_mnemonic(_("_Gaussian blur"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.use_gaussians),
                                 args->use_gaussians);
    gtk_table_attach(GTK_TABLE(table), controls.use_gaussians,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.use_gaussians, "toggled",
                             G_CALLBACK(logistic_invalidate),
                             &controls);
    row++;

    controls.ngaussians
                 = gtk_adjustment_new(args->ngaussians, 1, 10, 1, 2, 0);
    gwy_table_attach_hscale(table, row,
                            _("_Number of Gaussians:"), NULL,
                            controls.ngaussians, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.ngaussians, "value-changed",
                             G_CALLBACK(logistic_invalidate),
                             &controls);
    row++;

    controls.use_sobel
          = gtk_check_button_new_with_mnemonic(_("_Sobel derivatives"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.use_sobel),
                                 args->use_sobel);
    gtk_table_attach(GTK_TABLE(table), controls.use_sobel,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.use_sobel, "toggled",
                             G_CALLBACK(logistic_invalidate),
                             &controls);
    row++;

    controls.use_laplasian
                  = gtk_check_button_new_with_mnemonic(_("_Laplacian"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.use_laplasian),
                                 args->use_laplasian);
    gtk_table_attach(GTK_TABLE(table), controls.use_laplasian,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.use_laplasian, "toggled",
                             G_CALLBACK(logistic_invalidate),
                             &controls);
    row++;

    controls.use_hessian
                    = gtk_check_button_new_with_mnemonic(_("_Hessian"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.use_hessian),
                                 args->use_hessian);
    gtk_table_attach(GTK_TABLE(table), controls.use_hessian,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.use_hessian, "toggled",
                             G_CALLBACK(logistic_invalidate),
                             &controls);
    row++;

    controls.lambda
             = gtk_adjustment_new(args->lambda, 0.0, 10.0, 0.1, 1.0, 0);
    gwy_table_attach_hscale(table, row,
                            _("_Regularization parameter:"), NULL,
                            controls.lambda, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(controls.lambda, "value-changed",
                             G_CALLBACK(logistic_invalidate),
                             &controls);
    row++;

    logistic_dialog_update(&controls, args);
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            logistic_values_update(&controls, args);
            args->cancelled = TRUE;
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            logistic_reset_args(args);
            logistic_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    logistic_values_update(&controls, args);
    gtk_widget_destroy(dialog);
}

static GwyBrick *
create_feature_vector(GwyDataField *dfield, LogisticArgs *args)
{
    GwyBrick *features = NULL;
    GwyDataField *feature0 = NULL,
                 *featureg = NULL,
                 *feature  = NULL;
    gdouble max, min, avg, xreal, yreal, size;
    gdouble *f0data, *gdata, *fdata, *bdata;
    gint xres, yres, z, zres, i, ngauss;

    feature0 = gwy_data_field_duplicate(dfield);
    xres = gwy_data_field_get_xres(feature0);
    yres = gwy_data_field_get_yres(feature0);
    xreal = gwy_data_field_get_xreal(feature0);
    yreal = gwy_data_field_get_yreal(feature0);
    ngauss = args->ngaussians;
    if (!args->use_gaussians) {
        ngauss = 0;
    }
    zres = logistic_nfeatures(args);
    z = 0;
    max = gwy_data_field_get_max(feature0);
    min = gwy_data_field_get_min(feature0);
    g_return_val_if_fail(max - min > 0.0, NULL);
    gwy_data_field_multiply(feature0, 1.0/(max - min));
    avg = gwy_data_field_get_avg(feature0);
    gwy_data_field_add(feature0, -avg);

    features = gwy_brick_new(xres, yres, zres,
                              xreal, yreal, zres, TRUE);
    bdata = gwy_brick_get_data(features);
    f0data = gwy_data_field_get_data(feature0);
    memmove(bdata, f0data, xres * yres * sizeof(gdouble));
    z++;

    feature = gwy_data_field_duplicate(feature0);
    fdata = gwy_data_field_get_data(feature);

    if (args->use_laplasian) {
        gwy_data_field_filter_laplacian(feature);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_val_if_fail(max - min > 0.0, NULL);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, f0data, xres * yres * sizeof(gdouble));
        z++;
    }

    if (args->use_sobel) {
        gwy_data_field_filter_sobel(feature, GWY_ORIENTATION_HORIZONTAL);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_val_if_fail(max - min > 0.0, NULL);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, f0data, xres * yres * sizeof(gdouble));
        z++;

        gwy_data_field_filter_sobel(feature, GWY_ORIENTATION_VERTICAL);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_val_if_fail(max - min > 0.0, NULL);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, f0data, xres * yres * sizeof(gdouble));
        z++;
    }

    if (args->use_hessian) {
        logistic_filter_dx2(feature);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_val_if_fail(max - min > 0.0, NULL);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, f0data, xres * yres * sizeof(gdouble));
        z++;

        logistic_filter_dy2(feature);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_val_if_fail(max - min > 0.0, NULL);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, f0data, xres * yres * sizeof(gdouble));
        z++;

        logistic_filter_dxdy(feature);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_val_if_fail(max - min > 0.0, NULL);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, f0data, xres * yres * sizeof(gdouble));
        z++;
    }

    g_object_unref(feature);

    for (i = 0, size = 2.0; i < ngauss; i++, size *= 2.0) {
        featureg = gwy_data_field_duplicate(feature0);
        gdata = gwy_data_field_get_data(featureg);
        gwy_data_field_filter_gaussian(featureg, size * FWHM2SIGMA);
        max = gwy_data_field_get_max(featureg);
        min = gwy_data_field_get_min(featureg);
        g_return_val_if_fail(max - min > 0.0, NULL);
        gwy_data_field_multiply(featureg, 1.0/(max - min));
        avg = gwy_data_field_get_avg(featureg);
        gwy_data_field_add(featureg, -avg);
        memmove(bdata + z * xres * yres, gdata,
                xres * yres * sizeof(gdouble));
        z++;

        feature = gwy_data_field_duplicate(featureg);
        fdata = gwy_data_field_get_data(feature);

        if (args->use_laplasian) {
            gwy_data_field_filter_laplacian(feature);
            max = gwy_data_field_get_max(feature);
            min = gwy_data_field_get_min(feature);
            g_return_val_if_fail(max - min > 0.0, NULL);
            gwy_data_field_multiply(feature, 1.0/(max - min));
            avg = gwy_data_field_get_avg(feature);
            gwy_data_field_add(feature, -avg);
            memmove(bdata + z * xres * yres, fdata,
                    xres * yres * sizeof(gdouble));
            memmove(fdata, gdata, xres * yres * sizeof(gdouble));
            z++;
        }

        if (args->use_sobel) {
            gwy_data_field_filter_sobel(feature, GWY_ORIENTATION_HORIZONTAL);
            max = gwy_data_field_get_max(feature);
            min = gwy_data_field_get_min(feature);
            g_return_val_if_fail(max - min > 0.0, NULL);
            gwy_data_field_multiply(feature, 1.0/(max - min));
            avg = gwy_data_field_get_avg(feature);
            gwy_data_field_add(feature, -avg);
            memmove(bdata + z * xres * yres, fdata,
                    xres * yres * sizeof(gdouble));
            memmove(fdata, gdata, xres * yres * sizeof(gdouble));
            z++;

            gwy_data_field_filter_sobel(feature, GWY_ORIENTATION_VERTICAL);
            max = gwy_data_field_get_max(feature);
            min = gwy_data_field_get_min(feature);
            g_return_val_if_fail(max - min > 0.0, NULL);
            gwy_data_field_multiply(feature, 1.0/(max - min));
            avg = gwy_data_field_get_avg(feature);
            gwy_data_field_add(feature, -avg);
            memmove(bdata + z * xres * yres, fdata,
                    xres * yres * sizeof(gdouble));
            memmove(fdata, gdata, xres * yres * sizeof(gdouble));
            z++;
        }

        if (args->use_hessian) {
            logistic_filter_dx2(feature);
            max = gwy_data_field_get_max(feature);
            min = gwy_data_field_get_min(feature);
            g_return_val_if_fail(max - min > 0.0, NULL);
            gwy_data_field_multiply(feature, 1.0/(max - min));
            avg = gwy_data_field_get_avg(feature);
            gwy_data_field_add(feature, -avg);
            memmove(bdata + z * xres * yres, fdata,
                    xres * yres * sizeof(gdouble));
            memmove(fdata, gdata, xres * yres * sizeof(gdouble));
            z++;

            logistic_filter_dy2(feature);
            max = gwy_data_field_get_max(feature);
            min = gwy_data_field_get_min(feature);
            g_return_val_if_fail(max - min > 0.0, NULL);
            gwy_data_field_multiply(feature, 1.0/(max - min));
            avg = gwy_data_field_get_avg(feature);
            gwy_data_field_add(feature, -avg);
            memmove(bdata + z * xres * yres, fdata,
                    xres * yres * sizeof(gdouble));
            memmove(fdata, gdata, xres * yres * sizeof(gdouble));
            z++;

            logistic_filter_dxdy(feature);
            max = gwy_data_field_get_max(feature);
            min = gwy_data_field_get_min(feature);
            g_return_val_if_fail(max - min > 0.0, NULL);
            gwy_data_field_multiply(feature, 1.0/(max - min));
            avg = gwy_data_field_get_avg(feature);
            gwy_data_field_add(feature, -avg);
            memmove(bdata + z * xres * yres, fdata,
                    xres * yres * sizeof(gdouble));
            memmove(fdata, gdata, xres * yres * sizeof(gdouble));
            z++;
        }

        g_object_unref(feature);
        g_object_unref(featureg);
    }
    g_object_unref(feature0);

    return features;
}

static inline gdouble
sigmoid(gdouble z)
{
    return 1.0/(1.0 + exp(-z));
}

static void
train_logistic(GwyContainer *container, GwyBrick *features,
GwyDataField *mfield, gdouble *thetas, gdouble lambda)
{
    gdouble *grad, *oldgrad;
    gdouble epsilon, alpha, sum;
    G_GNUC_UNUSED gdouble cost;
    gint i, iter, maxiter, zres, id;
    gboolean converged = FALSE, cancelled = FALSE;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    zres = gwy_brick_get_zres(features);
    grad = g_malloc(zres * sizeof(gdouble));
    oldgrad = g_malloc(zres * sizeof(gdouble));
    for (i = 0; i < zres; i++) {
        thetas[i] = 0.0;
        oldgrad[i] = 0.0;
    }
    epsilon = 1E-5;
    alpha = 10.0;
    iter = 0;
    maxiter = 2000;
    gwy_app_wait_start(gwy_app_find_window_for_channel(container, id),
                       _("Training..."));
    while(!converged && !cancelled) {
        if (!gwy_app_wait_set_fraction((gdouble)iter/maxiter)) {
            cancelled = TRUE;
            break;
        }
        cost = cost_function(features, mfield, thetas, grad, lambda);

        sum = 0;
        for (i = 0; i < zres; i++) {
            sum += grad[i]*oldgrad[i];
        }

        if (sum > 0) {
            alpha *= 1.05;
        }
        else if (sum < 0) {
            for (i =0; i < zres; i++) {
                grad[i] += oldgrad[i];
            }
            alpha /= 2.0;
        }

        converged = TRUE;
        for (i = 0;  i < zres; i++) {
            thetas[i] -= alpha * grad[i];
            if (fabs(grad[i]) > epsilon) {
                converged = FALSE;
            }
            oldgrad[i] = grad[i];
        }

        if (iter >= maxiter) {
            converged = TRUE;
        }
        iter++;
    }
    gwy_app_wait_finish();
    g_free(grad);
    g_free(oldgrad);
}

static gdouble
cost_function(GwyBrick *brick, GwyDataField *mask,
              gdouble *thetas, gdouble *grad, gdouble lambda)
{
    gint i, j, k, m, xres, yres, zres;
    gdouble sum, jsum, h, x, y, theta;
    gdouble *bp, *mp;

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    bp = gwy_brick_get_data(brick);
    m = xres * yres;

    mp = gwy_data_field_get_data(mask);

    jsum = 0;
    grad[0] = 0;
    for (k = 1; k < zres; k++) {
        grad[k] = thetas[k] * lambda / m;
    }

    for (i = 0; i < yres; i++)
        for (j = 0; j < xres; j++) {
            sum = 0;
            for (k = 0; k < zres; k++) {
                x = *(bp + k *(xres * yres) + i * xres + j);
                sum += x * thetas[k];
            }
            h = sigmoid(sum);
            y = *(mp + i * xres + j);
            jsum += -log(h)*y - log(1-h)*(1-y);
            for (k = 0; k < zres; k++) {
                x = *(bp + k *(xres * yres) + i * xres + j);
                grad[k] += 1.0 / m * x * (h - y);
            }
        }
    jsum /= m;

    sum = 0;
    for (k = 1; k < zres; k++) {
        theta = *(thetas + k);
        sum += theta * theta;
    }
    jsum += sum * lambda/2.0/m;

    return jsum;
}

/*
static void
predict(GwyBrick *brick, gdouble *thetas, GwyDataField *dfield)
{
    gint i, j, k, xres, yres, zres;
    gdouble sum, x;
    gdouble *bp, *dp;

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    bp = gwy_brick_get_data(brick);
    dp = gwy_data_field_get_data(dfield);

    for (i = 0; i < yres; i++)
        for (j = 0; j < yres; j++) {
            sum = 0;
            for (k = 0; k < zres; k++) {
                x = *(bp + k *(xres * yres) + i * xres + j);
                sum += x * thetas[k];
            }
            *(dp + i * xres + j) = sigmoid(sum);
        }
}
*/

static void
predict_mask(GwyBrick *brick, gdouble *thetas, GwyDataField *mask)
{
    gint i, j, k, xres, yres, zres;
    gdouble sum, x;
    gdouble *bp, *mp;

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    bp = gwy_brick_get_data(brick);
    mp = gwy_data_field_get_data(mask);

    for (i = 0; i < yres; i++)
        for (j = 0; j < xres; j++) {
            sum = 0;
            for (k = 0; k < zres; k++) {
                x = *(bp + k *(xres * yres) + i * xres + j);
                sum += x * thetas[k];
            }
            *(mp + i * xres + j) = (sigmoid(sum) > 0.5) ? 1.0 : 0.0;
        }
}

static void
logistic_mode_changed(G_GNUC_UNUSED GtkWidget *button,
                      LogisticControls *controls)
{
    controls->args->mode = gwy_radio_buttons_get_current(controls->mode);
    if (controls->args->mode == LOGISTIC_MODE_USE) {
        gtk_widget_set_sensitive(controls->use_gaussians, FALSE);
        gwy_table_hscale_set_sensitive(controls->ngaussians, FALSE);
        gtk_widget_set_sensitive(controls->use_sobel, FALSE);
        gtk_widget_set_sensitive(controls->use_laplasian, FALSE);
        gtk_widget_set_sensitive(controls->use_hessian, FALSE);
        gwy_table_hscale_set_sensitive(controls->lambda, FALSE);
    }
    else {
        gtk_widget_set_sensitive(controls->use_gaussians, TRUE);
        gwy_table_hscale_set_sensitive(controls->ngaussians, TRUE);
        gtk_widget_set_sensitive(controls->use_sobel, TRUE);
        gtk_widget_set_sensitive(controls->use_laplasian, TRUE);
        gtk_widget_set_sensitive(controls->use_hessian, TRUE);
        gwy_table_hscale_set_sensitive(controls->lambda, FALSE);
    }
}

static void
logistic_values_update(LogisticControls *controls,
                       LogisticArgs *args)
{
    args->mode = gwy_radio_buttons_get_current(controls->mode);
    args->use_gaussians = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->use_gaussians));
    args->ngaussians
       = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->ngaussians));
    args->use_sobel = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->use_sobel));
    args->use_laplasian = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->use_laplasian));
    args->use_hessian = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->use_hessian));
    args->lambda
           = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->lambda));
}

static void
logistic_dialog_update(LogisticControls *controls,
                       LogisticArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->use_gaussians),
                                 args->use_gaussians);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->ngaussians),
                             args->ngaussians);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->use_sobel),
                                 args->use_sobel);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->use_laplasian),
                                 args->use_laplasian);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->use_hessian),
                                 args->use_hessian);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->lambda),
                             args->lambda);
}

static void
logistic_invalidate(LogisticControls *controls)
{
    logistic_values_update(controls, controls->args);
    gtk_widget_set_sensitive(gwy_radio_buttons_find(controls->mode,
                             LOGISTIC_MODE_USE), FALSE);
}

static void
gwy_data_field_area_convolve_3x3(GwyDataField *data_field,
                                 const gdouble *kernel,
                                 gint col, gint row,
                                 gint width, gint height)
{
    gdouble *rm, *rc, *rp;
    gdouble t, v;
    gint xres, i, j;

    xres = data_field->xres;
    rp = data_field->data + row*xres + col;

    /* Special-case width == 1 to avoid complications below.  It's silly but
     * the API guarantees it. */
    if (width == 1) {
        t = rp[0];
        for (i = 0; i < height; i++) {
            rc = rp = data_field->data + (row + i)*xres + col;
            if (i < height-1)
                rp += xres;

            v = (kernel[0] + kernel[1] + kernel[2])*t
                + (kernel[3] + kernel[4] + kernel[5])*rc[0]
                + (kernel[6] + kernel[7] + kernel[8])*rp[0];
            t = rc[0];
            rc[0] = v;
        }
        gwy_data_field_invalidate(data_field);

        return;
    }

    rm = g_new(gdouble, width);
    gwy_assign(rm, rp, width);

    for (i = 0; i < height; i++) {
        rc = rp;
        if (i < height-1)
            rp += xres;
        v = (kernel[0] + kernel[1])*rm[0] + kernel[2]*rm[1]
            + (kernel[3] + kernel[4])*rc[0] + kernel[5]*rc[1]
            + (kernel[6] + kernel[7])*rp[0] + kernel[8]*rp[1];
        t = rc[0];
        rc[0] = v;
        if (i < height-1) {
            for (j = 1; j < width-1; j++) {
                v = kernel[0]*rm[j-1] + kernel[1]*rm[j] + kernel[2]*rm[j+1]
                    + kernel[3]*t + kernel[4]*rc[j] + kernel[5]*rc[j+1]
                    + kernel[6]*rp[j-1] + kernel[7]*rp[j] + kernel[8]*rp[j+1];
                rm[j-1] = t;
                t = rc[j];
                rc[j] = v;
            }
            v = kernel[0]*rm[j-1] + (kernel[1] + kernel[2])*rm[j]
                + kernel[3]*t + (kernel[4] + kernel[5])*rc[j]
                + kernel[6]*rp[j-1] + (kernel[7] + kernel[8])*rp[j];
        }
        else {
            for (j = 1; j < width-1; j++) {
                v = kernel[0]*rm[j-1] + kernel[1]*rm[j] + kernel[2]*rm[j+1]
                    + kernel[3]*t + kernel[4]*rc[j] + kernel[5]*rc[j+1]
                    + kernel[6]*t + kernel[7]*rc[j] + kernel[8]*rc[j+1];
                rm[j-1] = t;
                t = rc[j];
                rc[j] = v;
            }
            v = kernel[0]*rm[j-1] + (kernel[1] + kernel[2])*rm[j]
                + kernel[3]*t + (kernel[4] + kernel[5])*rc[j]
                + kernel[6]*t + (kernel[7] + kernel[8])*rc[j];
        }
        rm[j-1] = t;
        rm[j] = rc[j];
        rc[j] = v;
    }

    g_free(rm);
    gwy_data_field_invalidate(data_field);
}

static void
logistic_filter_dx2(GwyDataField *dfield)
{
    gint xres, yres;

    static const gdouble dx2_kernel[] = {
        0.125, -0.25, 0.125,
        0.25,  -0.5,  0.25,
        0.125, -0.25, 0.125,
    };

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_area_convolve_3x3(dfield, dx2_kernel,
                                     0, 0, xres, yres);
}

static void
logistic_filter_dy2(GwyDataField *dfield)
{
    gint xres, yres;
    static const gdouble dy2_kernel[] = {
        0.125,  0.25, 0.125,
        -0.25,  -0.5, -0.25,
        0.125,  0.25, 0.125,
    };

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_area_convolve_3x3(dfield, dy2_kernel,
                                     0, 0, xres, yres);
}

static void
logistic_filter_dxdy(GwyDataField *dfield)
{
    gint xres, yres;
    static const gdouble dxdy_kernel[] = {
        0.5,  0, -0.5,
        0,    0, 0,
        -0.5, 0, 0.5,
    };

    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    gwy_data_field_area_convolve_3x3(dfield, dxdy_kernel,
                                     0, 0, xres, yres);
}

static gint
logistic_nfeatures(LogisticArgs *args)
{
    gint ngauss, nfeatures;

    ngauss = args->ngaussians;
    nfeatures = 1;
    if (!args->use_gaussians) {
        ngauss = 0;
    }
    nfeatures += ngauss;
    if (args->use_laplasian) {
        nfeatures += ngauss + 1;
    }
    if (args->use_sobel) {
        nfeatures += 2 * (ngauss + 1);
    }
    if (args->use_hessian) {
        nfeatures += 3 * (ngauss + 1);
    }

    return nfeatures;
}

static const gchar gaussians_key[]  = "/module/logistic/usegaussians";
static const gchar ngaussians_key[] = "/module/logistic/numgaussians";
static const gchar sobel_key[]      = "/module/logistic/usesobel";
static const gchar laplasian_key[]  = "/module/logistic/uselaplasians";
static const gchar hessian_key[]    = "/module/logistic/usehessian";
static const gchar lambda_key[] = "/module/logistic/lambda";

static void
logistic_load_args(GwyContainer *settings,
                   LogisticArgs *args)
{
    gchar *filename, *buffer, *line, *p;
    gsize size;
    gint i, nfeatures;
    gdouble *thetas;

    args->thetas = NULL;
    logistic_reset_args(args);
    gwy_container_gis_boolean_by_name(settings, gaussians_key,
                                      &args->use_gaussians);
    gwy_container_gis_int32_by_name(settings, ngaussians_key,
                                    &args->ngaussians);
    gwy_container_gis_boolean_by_name(settings, sobel_key,
                                      &args->use_sobel);
    gwy_container_gis_boolean_by_name(settings, laplasian_key,
                                      &args->use_laplasian);
    gwy_container_gis_boolean_by_name(settings, hessian_key,
                                      &args->use_hessian);
    gwy_container_gis_double_by_name(settings, lambda_key,
                                     &args->lambda);

    nfeatures = logistic_nfeatures(args);
    thetas = gwy_data_line_get_data(args->thetas);
    for (i = 0; i < nfeatures; i++) {
        *(thetas + i) = 0.0;
    }

    filename = g_build_filename(gwy_get_user_dir(), "logistic",
                                "thetas", NULL);

    if (g_file_get_contents(filename, &buffer, &size, NULL)) {
        p = buffer;
        i = 0;
        while ((line = gwy_str_next_line(&p)) && (i++ < nfeatures)) {
            g_strstrip(line);
            *(thetas++) = g_ascii_strtod(line, NULL);
        }
        g_free(buffer);
    }
    g_free(filename);

    logistic_sanitize_args(args);
}

static void
logistic_save_args(GwyContainer *settings,
                   LogisticArgs *args)
{
    gchar *filename, *s;
    gdouble *thetas;
    FILE *fh;
    gint i, nfeatures;

    gwy_container_set_boolean_by_name(settings, gaussians_key,
                                      args->use_gaussians);
    gwy_container_set_int32_by_name(settings, ngaussians_key,
                                    args->ngaussians);
    gwy_container_set_boolean_by_name(settings, sobel_key,
                                      args->use_sobel);
    gwy_container_set_boolean_by_name(settings, laplasian_key,
                                      args->use_laplasian);
    gwy_container_set_boolean_by_name(settings, hessian_key,
                                      args->use_hessian);
    gwy_container_set_double_by_name(settings, lambda_key,
                                     args->lambda);

    filename = g_build_filename(gwy_get_user_dir(), "logistic", NULL);
    if (!g_file_test(filename, G_FILE_TEST_IS_DIR))
        g_mkdir(filename, 0700);
    g_free(filename);

    filename = g_build_filename(gwy_get_user_dir(), "logistic",
                                "thetas", NULL);
    thetas = gwy_data_line_get_data(args->thetas);
    if ((fh = gwy_fopen(filename, "w"))) {
        nfeatures = logistic_nfeatures(args);
        for (i = 0; i < nfeatures; i++) {
            s = g_malloc(G_ASCII_DTOSTR_BUF_SIZE);
            g_ascii_dtostr(s, G_ASCII_DTOSTR_BUF_SIZE, *(thetas+i));
            fputs(s, fh);
            fputc('\n', fh);
            g_free(s);
        }
        fclose(fh);
    }
    g_free(filename);
}

static void
logistic_reset_args(LogisticArgs *args)
{
    gint nfeatures;

    args->mode = LOGISTIC_MODE_TRAIN;
    args->use_gaussians = TRUE;
    args->ngaussians = 4;
    args->use_sobel = TRUE;
    args->use_laplasian = TRUE;
    args->use_hessian = TRUE;
    args->lambda = 1.0;
    nfeatures = logistic_nfeatures(args);
    if (args->thetas) {
        g_object_unref(args->thetas);
    }
    args->thetas = gwy_data_line_new(nfeatures, nfeatures, TRUE);
}

static void
logistic_sanitize_args(LogisticArgs *args)
{
    args->mode = MIN(args->mode, LOGISTIC_MODE_USE);
    args->use_gaussians = !!args->use_gaussians;
    args->ngaussians = CLAMP(args->ngaussians, 1.0, 10.0);
    args->use_sobel = !!args->use_sobel;
    args->use_laplasian = !!args->use_laplasian;
    args->use_hessian = !!args->use_hessian;
    args->lambda = CLAMP(args->lambda, 0.0, 10.0);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
