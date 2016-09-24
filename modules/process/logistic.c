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
    gboolean use_gaussians;
    gint ngaussians;
    gboolean use_sobel;
    gboolean use_laplasian;
    gboolean use_hessian;
    GwyDataLine *thetas;
} LogisticArgs;

typedef struct {
    LogisticArgs *args;
    GSList *mode;
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
static void      predict                (GwyBrick *brick,
                                         gdouble *thetas,
                                         GwyDataField *dfield);
static void      predict_mask           (GwyBrick *brick,
                                         gdouble *thetas,
                                         GwyDataField *mask);
static void      logistic_mode_changed  (GtkWidget *button,
                                         LogisticControls *controls);
static void      logistic_values_update (LogisticControls *controls,
                                         LogisticArgs *args);
static void      logistic_dialog_update (LogisticControls *controls,
                                         LogisticArgs *args);
static void      logistic_load_args     (GwyContainer *settings,
                                         LogisticArgs *args);
static void      logistic_save_args     (GwyContainer *settings,
                                         LogisticArgs *args);
static void      logistic_reset_args    (LogisticArgs *args);
static void      logistic_filter_dx2    (GwyDataField *dfield);
static void      logistic_filter_dy2    (GwyDataField *dfield);
static void      logistic_filter_dxdy   (GwyDataField *dfield);
static gint      logistic_nfeatures     (LogisticArgs *args);

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
    GwyDataField *dfield, *mfield, *preview;
    GwyBrick *features;
    gint id;
    // gint xres, yres, newid;
    LogisticArgs args;
    gdouble *thetas;
    GQuark quark;

    g_return_if_fail(run & LOGISTIC_RUN_MODES);
    logistic_load_args(gwy_app_settings_get(), &args);
    logistic_reset_args(&args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &quark,
                                     0);
    logistic_dialog(data, &args);
    features = create_feature_vector(dfield, &args);

    /*
    xres = gwy_brick_get_xres(features);
    yres = gwy_brick_get_tres(features);
    gwy_brick_extract_plane(features, preview,
                            0, 0, 0, xres, yres, -1, TRUE);
    newid = gwy_app_data_browser_add_brick(features, preview, data, TRUE);
    g_object_unref(preview);
    */

    thetas = gwy_data_line_get_data(args.thetas);
    if (args.mode == LOGISTIC_MODE_TRAIN) {
        if (mfield) {
            train_logistic(data, features, mfield, thetas, 1.0);
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
    }
    gwy_app_channel_log_add_proc(data, id, id);
    g_object_unref(features);
}

static void
logistic_dialog(GwyContainer *data, LogisticArgs *args)
{
    GtkWidget *dialog, *table, *button;
    gint response, row;
    LogisticControls controls;

    controls.args = args;

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

    table = gtk_table_new(2, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       TRUE, TRUE, 4);
    row = 0;

    controls.mode = gwy_radio_buttons_createl(G_CALLBACK(logistic_mode_changed),
                                              &controls, args->mode,
                                              _("_Train logistic regression"),
                                              LOGISTIC_MODE_TRAIN,
                                              _("_Use trained regression"),
                                              LOGISTIC_MODE_USE,
                                              NULL);
    button = gwy_radio_buttons_find(controls.mode, LOGISTIC_MODE_TRAIN);
    gtk_table_attach(GTK_TABLE(table), button, 0, 3, row, row+1,
                     GTK_FILL, 0, 0, 0);
    row++;

    button = gwy_radio_buttons_find(controls.mode, LOGISTIC_MODE_USE);
    gtk_table_attach(GTK_TABLE(table), button, 0, 3, row, row+1,
                     GTK_FILL, 0, 0, 0);
    row++;
    logistic_dialog_update(&controls, args);
    gtk_widget_show_all(dialog);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            logistic_values_update(&controls, args);
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
    logistic_save_args(gwy_app_settings_get(), args);
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
    gdouble epsilon, alpha, cost, sum;
    gint i, iter, maxiter, zres, id;
    gboolean converged = FALSE, cancelled = FALSE;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    zres = gwy_brick_get_zres(features);
    thetas = g_malloc(zres * sizeof(gdouble));
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
        fprintf(stderr, "iter=%d cost=%g grad[0] = %g grad[20] = %g alpha=%g\n",
                iter, cost, grad[0], grad[20], alpha);
        iter++;
    }

    for (i = 0; i < zres; i++) {
        fprintf(stderr,"thetas[%d] = %g\n", i, thetas[i]);
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
}

static void
logistic_values_update(LogisticControls *controls,
                       LogisticArgs *args)
{
    args = controls->args;
}

static void
logistic_dialog_update(LogisticControls *controls,
                       LogisticArgs *args)
{
    controls->args = args;
}

static const gchar thetas_key[]  = "/module/logistic/thetas";

static void
logistic_load_args(GwyContainer *settings,
                   LogisticArgs *args)
{
    gint nfeatures;

    /*
    if (!gwy_container_gis_object_by_name(settings,
                                          thetas_key, &args->thetas)) {
        args->thetas = gwy_data_line_new(NFEATURES, NFEATURES, TRUE);
        logistic_reset_args(args);
    }
    */
    args->use_gaussians = TRUE;
    args->ngaussians = 4;
    args->use_sobel = TRUE;
    args->use_laplasian = TRUE;
    args->use_hessian = TRUE;    
    nfeatures = logistic_nfeatures(args);
    args->thetas = gwy_data_line_new(nfeatures, nfeatures, TRUE);
}

static void
logistic_save_args(GwyContainer *settings,
                   LogisticArgs *args)
{
    /*
    gwy_container_set_object_by_name(settings,
                                     thetas_key, args->thetas);
                                     */
}

static void
logistic_reset_args(LogisticArgs *args)
{
    gdouble *thetas;
    gint i, nfeatures;

    args->use_gaussians = TRUE;
    args->ngaussians = 4;
    args->use_sobel = TRUE;
    args->use_laplasian = TRUE;
    args->use_hessian = TRUE;

    thetas = gwy_data_line_get_data(args->thetas);
    thetas[0] = 0.592032;
    thetas[1] = 1.21119;
    thetas[2] = 0.105035;
    thetas[3] = 0.0131375;
    thetas[4] = 0.435931;
    thetas[5] = 0.218747;
    thetas[6] = -0.0838838;
    thetas[7] = 1.2983;
    thetas[8] = 0.985186;
    thetas[9] = 0.669358;
    thetas[10] = -0.060548;
    thetas[11] = -0.166977;
    thetas[12] = 0.359395;
    thetas[13] = 0.341714;
    thetas[14] = 1.50746;
    thetas[15] = 1.10401;
    thetas[16] = 0.751877;
    thetas[17] = 0.0940333;
    thetas[18] = 1.22919;
    thetas[19] = 0.485005;
    thetas[20] = -0.0659881;
    thetas[21] = 1.21087;
    thetas[22] = -7.40608;
    thetas[23] = -1.2167;
    thetas[24] = 0.085099;
    thetas[25] = -5.60057;
    thetas[26] = -4.7028;
    thetas[27] = -0.848886;
    thetas[28] = -2.91391;
    thetas[29] = -20.2171;
    thetas[30] = -3.59727;
    thetas[31] = -0.49366;
    thetas[32] = -14.5555;
    thetas[33] = -14.0601;
    thetas[34] = 1.00873;

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
    memcpy(rm, rp, width*sizeof(gdouble));

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
