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
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <string.h>

#define LOGISTIC_RUN_MODES GWY_RUN_INTERACTIVE

#define FWHM2SIGMA (1.0/(2.0*sqrt(2*G_LN2)))

static gboolean module_register              (void);
static void     logistic_run                 (GwyContainer *data,
                                              GwyRunType run);
static void     create_feature_vector        (GwyDataField *dfield,
                                              GwyBrick *features);
static gdouble  cost_function                (GwyBrick *brick,
                                              GwyDataField *mask,
                                              gdouble *thetas,
                                              gdouble *grad,
                                              gdouble lambda);
static void     train_logistic               (GwyBrick *features,
                                              GwyDataField *mfield,
                                              gdouble *thetas,
                                              gdouble lambda);
static void     predict                      (GwyBrick *brick,
                                              gdouble *thetas,
                                              GwyDataField *dfield);
static void     predict_mask                 (GwyBrick *brick,
                                              gdouble *thetas,
                                              GwyDataField *mask);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Mark grain by logistic regression."),
    "Daniil Bratashov <dn2010@gwyddion.net>",
    "0.1",
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
                              GWY_STOCK_VOLUMIZE_LAYERS,
                              FEATURE_VECTOR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark grains by logistic regression"));

    return TRUE;
}

static void
logistic_run(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    gint id;

    g_return_if_fail(run & LOGISTIC_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     0);

}

static void
create_feature_vector(GwyDataField *dfield, GwyBrick *features)
{
    GwyDataField *feature0 = NULL, *feature = NULL;
    gdouble max, min, avg, xreal, yreal, size;
    gdouble *fdata, *bdata;
    gint xres, yres, z, zres, i, ngauss;

    feature0 = gwy_data_field_duplicate(dfield);
    xres = gwy_data_field_get_xres(feature0);
    yres = gwy_data_field_get_yres(feature0);
    xreal = gwy_data_field_get_xreal(feature0);
    yreal = gwy_data_field_get_yreal(feature0);
    ngauss = 5;
    zres = 1 + 4 * ngauss;
    z = 0;
    max = gwy_data_field_get_max(feature0);
    min = gwy_data_field_get_min(feature0);
    g_return_if_fail(max - min > 0.0);
    gwy_data_field_multiply(feature0, 1.0/(max - min));
    avg = gwy_data_field_get_avg(feature0);
    gwy_data_field_add(feature0, -avg);

    features = gwy_brick_new(xres, yres, zres,
                             xreal, yreal, zres, TRUE);
    bdata = gwy_brick_get_data(features);
    fdata = gwy_data_field_get_data(feature0);
    memmove(bdata, fdata, xres * yres * sizeof(gdouble));
    z++;

    for (i = 0, size = 1.0; i < ngauss; i++, size *= 2.0) {
        feature = gwy_data_field_duplicate(feature0);
        gwy_data_field_filter_gaussian(feature, size * FWHM2SIGMA);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_if_fail(max - min > 0.0);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        fdata = gwy_data_field_get_data(feature);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        z++;
        gwy_data_field_filter_laplacian(feature);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_if_fail(max - min > 0.0);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, bdata + (z-1) * xres * yres,
                xres * yres * sizeof(gdouble));
        z++;
        gwy_data_field_filter_sobel(feature, GWY_ORIENTATION_HORIZONTAL);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_if_fail(max - min > 0.0);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, bdata + (z-2) * xres * yres,
                xres * yres * sizeof(gdouble));
        z++;
        gwy_data_field_filter_sobel(feature, GWY_ORIENTATION_VERTICAL);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_if_fail(max - min > 0.0);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        gwy_data_field_filter_sobel(feature, GWY_ORIENTATION_HORIZONTAL);
        max = gwy_data_field_get_max(feature);
        min = gwy_data_field_get_min(feature);
        g_return_if_fail(max - min > 0.0);
        gwy_data_field_multiply(feature, 1.0/(max - min));
        avg = gwy_data_field_get_avg(feature);
        gwy_data_field_add(feature, -avg);
        memmove(bdata + z * xres * yres, fdata,
                xres * yres * sizeof(gdouble));
        memmove(fdata, bdata + (z-3) * xres * yres,
                xres * yres * sizeof(gdouble));
        z++;
        g_object_unref(feature);
    }
    g_object_unref(feature0);
}

static inline gdouble
sigmoid(gdouble z)
{
    return 1.0/(1.0 + exp(-z));
}

static void
train_logistic(GwyBrick *features, GwyDataField *mfield,
gdouble *thetas, gdouble lambda)
{
    gdouble *grad;
    gdouble epsilon, alpha, cost;
    gint i, iter, maxiter, zres;
    gboolean converged = FALSE;

    zres = gwy_brick_get_zres(features);
    thetas = g_malloc(zres * sizeof(gdouble));
    grad = g_malloc(zres * sizeof(gdouble));
    for (i = 0; i < zres; i++) {
        thetas[i] = 0.0;
    }
    epsilon = 1E-12;
    alpha = 10.0;
    iter = 0;
    maxiter = 10000;
    while(!converged) {
        cost = cost_function(features, mfield, thetas, grad, lambda);

        converged = TRUE;
        for (i = 0;  i < zres; i++) {
            thetas[i] -= alpha * grad[i];
            if (grad[i] > epsilon)
                converged = FALSE;
        }
        if (iter >= maxiter)
            converged = TRUE;
        fprintf(stderr, "iter=%d cost=%g\n", iter, cost);
        iter++;
    }
    for (i = 0; i < zres; i++) {
        fprintf(stderr,"thetas[%d] = %g\n", i, thetas[i]);
    }

    g_free(grad);
}

static gdouble
cost_function(GwyBrick *brick, GwyDataField *mask,
              gdouble *thetas, gdouble *grad, gdouble lambda)
{
    gint i, j, k, m, xres, yres, zres;
    GwyDataField *hfield;
    gdouble sum, jsum, h, x, y, theta;
    gdouble *bp, *mp, *hp;

    xres = gwy_brick_get_xres(brick);
    yres = gwy_brick_get_yres(brick);
    zres = gwy_brick_get_zres(brick);
    bp = gwy_brick_get_data(brick);
    m = xres * yres;

    hfield = gwy_data_field_new_alike(mask, TRUE);
    hp = gwy_data_field_get_data(hfield);
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
            *(hp + i * xres + j) = h;
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

    g_object_unref(hfield);

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
        for (j = 0; j < yres; j++) {
            sum = 0;
            for (k = 0; k < zres; k++) {
                x = *(bp + k *(xres * yres) + i * xres + j);
                sum += x * thetas[k];
            }
            *(mp + i * xres + j) = (sigmoid(sum) > 0.5) ? 1.0 : 0.0;
        }
}
