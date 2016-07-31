/*
 *  @(#) $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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

/* XXX: Write all estimation and fitting functions for point clouds.  This
 * means we can easily update this module to handle XYZ data later. */

#define DEBUG 1
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyrandgenset.h>
#include <libgwyddion/gwynlfit.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>
#include "preview.h"

#define FIT_SHAPE_RUN_MODES GWY_RUN_INTERACTIVE

/* Caching seems totally worth it.  It adds some small time every function call
 * and it does not improve anything when we care calculating derivatives by
 * the cached parameter, but for evaluation and derivatives by any other
 * paramter it speeds up the functions considerably. */
#define FIT_SHAPE_CACHE 1

enum { NREDLIM = 4096 };

typedef enum {
    FIT_SHAPE_DISPLAY_DATA   = 0,
    FIT_SHAPE_DISPLAY_RESULT = 1,
    FIT_SHAPE_DISPLAY_DIFF   = 2
} GwyFitShapeDisplayType;

typedef enum {
    FIT_SHAPE_OUTPUT_NONE = 0,
    FIT_SHAPE_OUTPUT_FIT  = 1,
    FIT_SHAPE_OUTPUT_DIFF = 2,
    FIT_SHAPE_OUTPUT_BOTH = 3,
} GwyFitShapeOutputType;

typedef gboolean (*FitShapeEstimate)(const GwyXY *xy,
                                     const gdouble *z,
                                     guint n,
                                     gdouble *param);

typedef struct {
    const char *name;
    gint power_x;
    gint power_y;
    gdouble min;
    gdouble max;
} FitShapeParam;

/* XXX: We may need two sets of parameters: nice to fit (e.g. curvature
 * for spheres) and nice for user (e.g. radius for sphere).  A one-to-one
 * transformation function between the two sets is then required. */
typedef struct {
    const gchar *name;
    gboolean needs_same_units;
    GwyNLFitFunc function;
    FitShapeEstimate estimate;
    guint nparams;
    const FitShapeParam *param;
} FitShapeFunc;

typedef struct {
    guint nparam;
    gboolean *param_fixed;

    guint n;
    gdouble *fake_x;
    GwyXY *xy;
    gdouble *z;
    gdouble *w;
} FitShapeContext;

typedef struct {
    gdouble *param;
    gdouble *param_err;
    gdouble chi_sq;
    gboolean success;
} FitShapeResult;

static gboolean     module_register          (void);
static void         fit_shape                (GwyContainer *data,
                                              GwyRunType run);
static void         fit_context_resize_params(FitShapeContext *ctx,
                                              guint n_param);
static void         fit_context_fill_data    (FitShapeContext *ctx,
                                              GwyDataField *dfield,
                                              GwyDataField *mask,
                                              GwyDataField *weight);
static void         fit_context_free         (FitShapeContext *ctx);
static GwyNLFitter* fit                      (const FitShapeFunc *func,
                                              const FitShapeContext *ctx,
                                              gdouble *param);
static GwyNLFitter* fit_reduced              (const FitShapeFunc *func,
                                              const FitShapeContext *ctx,
                                              gdouble *param);
static void         calculate_field          (const FitShapeFunc *func,
                                              const gdouble *param,
                                              GwyDataField *dfield);
static void         calculate_function       (const FitShapeFunc *func,
                                              const FitShapeContext *ctx,
                                              const gdouble *param,
                                              gdouble *z);
static gdouble      calculate_rss            (const FitShapeFunc *func,
                                              const FitShapeContext *ctx,
                                              const gdouble *param);
static void         reduce_data_size         (const GwyXY *xy,
                                              const gdouble *z,
                                              guint n,
                                              GwyXY *xyred,
                                              gdouble *zred,
                                              guint nred);

#define DECLARE_SHAPE_FUNC(name) \
    static gdouble name##_func(gdouble fake_x, \
                               gint n_param, \
                               const gdouble *param, \
                               gpointer user_data, \
                               gboolean *fres); \
    static gboolean name##_estimate(const GwyXY *xy, \
                                    const gdouble *z, \
                                    guint n, \
                                    gdouble *param);

DECLARE_SHAPE_FUNC(sphere);
DECLARE_SHAPE_FUNC(grating);

static const FitShapeParam sphere_params[] = {
   { "x<sub>0</sub>", 1, 0, -G_MAXDOUBLE, G_MAXDOUBLE, },
   { "y<sub>0</sub>", 1, 0, -G_MAXDOUBLE, G_MAXDOUBLE, },
   { "z<sub>0</sub>", 0, 1, -G_MAXDOUBLE, G_MAXDOUBLE, },
   { "C",             0, 1, -G_MAXDOUBLE, G_MAXDOUBLE, },
};

static const FitShapeParam grating_params[] = {
   { "w",             1, 0, 0.0,          G_MAXDOUBLE, },
   { "p",             0, 0, 0.0,          1.0,         },
   { "h",             0, 1, -G_MAXDOUBLE, G_MAXDOUBLE, },
   { "z<sub>0</sub>", 0, 1, -G_MAXDOUBLE, G_MAXDOUBLE, },
   { "x<sub>0</sub>", 1, 0, -G_MAXDOUBLE, G_MAXDOUBLE, },
   { "α",             0, 0, 0.0,          2.0*G_PI,    },
   { "c",             0, 0, 0.0,          G_MAXDOUBLE, },
};

static const FitShapeFunc shapes[] = {
    {
        N_("Sphere"), TRUE,
        &sphere_func, &sphere_estimate,
        G_N_ELEMENTS(sphere_params), sphere_params,
    },
    {
        N_("Grating"), FALSE,
        &grating_func, &grating_estimate,
        G_N_ELEMENTS(grating_params), grating_params,
    },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Fits predefined geometrical shapes to data."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("fit_shape",
                              (GwyProcessFunc)&fit_shape,
                              N_("/_Level/_Fit Shape..."),
                              NULL,
                              FIT_SHAPE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Fit geometrical shapes"));

    return TRUE;
}

static void
fit_shape(GwyContainer *data, GwyRunType run)
{
    //FitShapeArgs args;
    GwyDataField *dfield;
    //GwySIUnit *siunitxy, *siunitz;
    gint id, newid;

    g_return_if_fail(run & FIT_SHAPE_RUN_MODES);

    //fit_shape_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield);

    //siunitxy = gwy_data_field_get_si_unit_xy(dfield);
    //siunitz = gwy_data_field_get_si_unit_z(dfield);

    //fit_shape_save_args(gwy_app_settings_get(), &args);

    {
        const FitShapeFunc *func = shapes + 1;
        FitShapeContext ctx;
        GwyNLFitter *fitter;
        gdouble *param;
        guint i;

        gwy_clear(&ctx, 1);
        fit_context_resize_params(&ctx, func->nparams);
        fit_context_fill_data(&ctx, dfield, NULL, NULL);

        param = g_new(gdouble, func->nparams);
        func->estimate(ctx.xy, ctx.z, ctx.n, param);
        gwy_debug("RSS after estimate: %g", calculate_rss(func, &ctx, param));

        dfield = gwy_data_field_new_alike(dfield, FALSE);
        calculate_field(func, param, dfield);
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        g_object_unref(dfield);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                0);
        gwy_container_set_const_string(data,
                                       gwy_app_get_data_title_key_for_id(newid),
                                       "Estimate");

        for (i = 0; i < func->nparams; i++)
            gwy_debug("param[%u] %g", i, param[i]);

        fitter = fit_reduced(func, &ctx, param);
        if (fitter) {
            gwy_debug("RSS after reduced fit: %g",
                      calculate_rss(func, &ctx, param));
            gwy_math_nlfit_free(fitter);
        }

        for (i = 0; i < func->nparams; i++)
            gwy_debug("param[%u] %g", i, param[i]);

        fitter = fit(func, &ctx, param);
        gwy_debug("RSS after fit: %g", calculate_rss(func, &ctx, param));
        gwy_math_nlfit_free(fitter);

        for (i = 0; i < func->nparams; i++)
            gwy_debug("param[%u] %g", i, param[i]);

        dfield = gwy_data_field_new_alike(dfield, FALSE);
        calculate_field(func, param, dfield);
        newid = gwy_app_data_browser_add_data_field(dfield, data, TRUE);
        g_object_unref(dfield);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                GWY_DATA_ITEM_MASK_COLOR,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                0);
        gwy_container_set_const_string(data,
                                       gwy_app_get_data_title_key_for_id(newid),
                                       "Fit");

        g_free(param);
        fit_context_free(&ctx);
    }
}

static void
fit_context_resize_params(FitShapeContext *ctx,
                          guint n_param)
{
    guint i;

    ctx->nparam = n_param;
    ctx->param_fixed = g_renew(gboolean, ctx->param_fixed, n_param);
    for (i = 0; i < n_param; i++) {
        ctx->param_fixed[i] = FALSE;
    }
}

/* Construct separate xy[], z[] and w[] arrays from data field pixels under
 * the mask. */
static void
fit_context_fill_data(FitShapeContext *ctx,
                      GwyDataField *dfield,
                      GwyDataField *mask,
                      GwyDataField *weight)
{
    guint n, k, i, j, nn, xres, yres;
    const gdouble *d, *m, *w;
    gdouble dx, dy, xoff, yoff;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    dx = gwy_data_field_get_xmeasure(dfield);
    dy = gwy_data_field_get_ymeasure(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);

    nn = xres*yres;
    if (mask) {
        m = gwy_data_field_get_data_const(mask);
        n = 0;
        for (k = 0; k < nn; k++) {
            if (m[k] > 0.0)
                n++;
        }
    }
    else {
        m = NULL;
        n = nn;
    }

    ctx->n = n;
    ctx->fake_x = g_renew(gdouble, ctx->fake_x, n);
    ctx->xy = g_renew(GwyXY, ctx->xy, n);
    ctx->z = g_renew(gdouble, ctx->z, n);
    ctx->w = g_renew(gdouble, ctx->w, n);
    d = gwy_data_field_get_data_const(dfield);
    w = weight ? gwy_data_field_get_data_const(weight) : NULL;

    n = 0;
    for (i = 0; i < yres; i++) {
        gdouble y = (i + 0.5)*dy + yoff;

        for (j = 0; j < xres; j++) {
            if (!m || m[k] > 0.0) {
                gdouble x = (j + 0.5)*dx + xoff;

                k = i*xres + j;
                ctx->fake_x[n] = n;
                ctx->xy[n].x = x;
                ctx->xy[n].y = y;
                ctx->z[n] = d[k];
                ctx->w[n] = w ? w[k] : 1.0;
                n++;
            }
        }
    }
}

static void
fit_context_free(FitShapeContext *ctx)
{
    g_free(ctx->param_fixed);
    g_free(ctx->fake_x);
    g_free(ctx->xy);
    g_free(ctx->z);
    g_free(ctx->w);
    gwy_clear(ctx, 1);
}

static GwyNLFitter*
fit(const FitShapeFunc *func, const FitShapeContext *ctx, gdouble *param)
{
    GwyNLFitter *fitter;
    gdouble rss;

    fitter = gwy_math_nlfit_new(func->function, gwy_math_nlfit_derive);

    rss = gwy_math_nlfit_fit_full(fitter,
                                  ctx->n, ctx->fake_x, ctx->z, ctx->w,
                                  func->nparams, param, ctx->param_fixed, NULL,
                                  (gpointer)ctx);
    if (rss < 0.0)
        g_warning("Fit failed.");

    return fitter;
}

static GwyNLFitter*
fit_reduced(const FitShapeFunc *func, const FitShapeContext *ctx,
            gdouble *param)
{
    GwyNLFitter *fitter;
    FitShapeContext ctxred;
    guint nred;

    if (ctx->n <= NREDLIM)
        return NULL;

    ctxred = *ctx;
    nred = ctxred.n = sqrt(ctx->n*(gdouble)NREDLIM);
    ctxred.xy = g_new(GwyXY, nred);
    ctxred.z = g_new(gdouble, nred);
    /* TODO TODO TODO we must also reduce weights. */
    reduce_data_size(ctx->xy, ctx->z, ctx->n, ctxred.xy, ctxred.z, nred);
    fitter = fit(func, &ctxred, param);
    g_free(ctxred.xy);
    g_free(ctxred.z);

    return fitter;
}

static void
calculate_field(const FitShapeFunc *func,
                const gdouble *param,
                GwyDataField *dfield)
{
    FitShapeContext ctx;

    gwy_clear(&ctx, 1);
    fit_context_resize_params(&ctx, func->nparams);
    fit_context_fill_data(&ctx, dfield, NULL, NULL);
    calculate_function(func, &ctx, param, gwy_data_field_get_data(dfield));
    fit_context_free(&ctx);
}

static void
calculate_function(const FitShapeFunc *func,
                   const FitShapeContext *ctx,
                   const gdouble *param,
                   gdouble *z)
{
    guint k, n = ctx->n, nparams = func->nparams;

    for (k = 0; k < n; k++) {
        gboolean fres;

        z[k] = func->function((gdouble)k, nparams, param, (gpointer)ctx, &fres);
        if (!fres) {
            g_warning("Cannot evaluate function for pixel.");
            z[k] = 0.0;
        }
    }
}

G_GNUC_UNUSED
static gdouble
calculate_rss(const FitShapeFunc *func,
              const FitShapeContext *ctx,
              const gdouble *param)
{
    guint k, n = ctx->n, nparams = func->nparams;
    gdouble rss = 0.0;

    for (k = 0; k < n; k++) {
        gboolean fres;
        gdouble z;

        z = func->function((gdouble)k, nparams, param, (gpointer)ctx, &fres);
        if (!fres) {
            g_warning("Cannot evaluate function for pixel.");
        }
        else {
            z -= ctx->z[k];
            rss += z*z;
        }
    }

    return rss;
}

static gdouble
sphere_func(gdouble fake_x,
            G_GNUC_UNUSED gint n_param,
            const gdouble *param,
            gpointer user_data,
            gboolean *fres)
{
    const FitShapeContext *ctx = (const FitShapeContext*)user_data;
    gdouble xc = param[0];
    gdouble yc = param[1];
    gdouble z0 = param[2];
    gdouble kappa = param[3];
    gdouble x, y, r2k, t, val;
    guint i;

    g_assert(n_param == 4);

    i = (guint)fake_x;
    x = ctx->xy[i].x - xc;
    y = ctx->xy[i].y - yc;
    /* Rewrite R - sqrt(R² - r²) as κ*r²/(1 + sqrt(1 - κ²r²)) where
     * r² = x² + y² and κR = 1 to get nice behaviour in the close-to-denegerate
     * cases, including completely flat surface.  The expression 1.0/kappa
     * is safe because we cannot get to this branch for κ → 0 unless
     * simultaneously r → ∞. */
    r2k = kappa*(x*x + y*y);
    t = 1.0 - kappa*r2k;
    if (t > 0.0)
        val = z0 - r2k/(1.0 + sqrt(t));
    else
        val = z0 - 1.0/kappa;

    *fres = TRUE;
    return val;
}

static void
mean_x_y(const GwyXY *xy, guint n,
         gdouble *pxc, gdouble *pyc)
{
    gdouble xc = 0.0, yc = 0.0;
    guint i;

    if (!n) {
        *pxc = *pyc = 0.0;
        return;
    }

    for (i = 0; i < n; i++) {
        xc += xy[i].x;
        yc += xy[i].y;
    }

    *pxc = xc/n;
    *pyc = yc/n;
}

static void
range_z(const gdouble *z, guint n,
        gdouble *pmin, gdouble *pmax)
{
    gdouble min = G_MAXDOUBLE, max = -G_MAXDOUBLE;
    guint i;

    if (!n) {
        *pmin = *pmax = 0.0;
        return;
    }

    for (i = 0; i < n; i++) {
        if (z[i] < min)
            min = z[i];
        if (z[i] > max)
            max = z[i];
    }

    *pmin = min;
    *pmax = max;
}

static void
reduce_data_size(const GwyXY *xy, const gdouble *z, guint n,
                 GwyXY *xyred, gdouble *zred, guint nred)
{
    GRand *rng = g_rand_new();
    guint *redindex = g_new(guint, n);
    guint i, j;

    for (i = 0; i < n; i++)
        redindex[i] = i;

    for (i = 0; i < nred; i++) {
        j = g_rand_int_range(rng, 0, n-nred);
        xyred[i] = xy[redindex[j]];
        zred[i] = z[redindex[j]];
        redindex[j] = redindex[n-1-nred];
    }

    g_rand_free(rng);
}

/* Approximately cicrumscribe a set of points by finding a containing
 * octagon. */
static void
circumscribe_x_y(const GwyXY *xy, guint n,
                 gdouble *pxc, gdouble *pyc, gdouble *pr)
{
    gdouble min[4], max[4], r[4];
    guint i, j;

    if (!n) {
        *pxc = *pyc = 0.0;
        *pr = 1.0;
        return;
    }

    for (j = 0; j < 4; j++) {
        min[j] = G_MAXDOUBLE;
        max[j] = -G_MAXDOUBLE;
    }

    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x, y = xy[i].y;
        gdouble t[4] = { x, x+y, y, y-x };

        for (j = 0; j < 4; j++) {
            if (t[j] < min[j])
                min[j] = t[j];
            if (t[j] > max[j])
                max[j] = t[j];
        }
    }

    for (j = 0; j < 4; j++) {
        r[j] = sqrt(10.0)/3.0*(max[j] - min[j]);
        if (j % 2)
            r[j] /= G_SQRT2;
    }

    i = 0;
    for (j = 1; j < 4; j++) {
        if (r[j] > r[i])
            i = j;
    }

    *pr = 0.5*r[i];
    if (i % 2) {
        *pxc = (min[1] - min[3] + max[1] - max[3])/4.0;
        *pyc = (min[1] + min[3] + max[1] + max[3])/4.0;
    }
    else {
        *pxc = (min[0] + max[0])/2.0;
        *pyc = (min[2] + max[2])/2.0;
    }
}

/* Fit the data with a rotationally symmetric parabola and use its parameters
 * for the spherical surface estimate. */
static gboolean
sphere_estimate(const GwyXY *xy,
                const gdouble *z,
                guint n,
                gdouble *param)
{
    gdouble xc, yc;
    /* Linear fit with functions 1, x, y and x²+y². */
    gdouble a[10], b[4];
    guint i;

    /* XXX: Handle the surrounding flat area, which can be a part of the
     * function, better? */

    /* Using centered coodinates improves the condition number. */
    mean_x_y(xy, n, &xc, &yc);
    gwy_clear(a, 10);
    gwy_clear(b, 4);
    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x - xc, y = xy[i].y - yc;
        gdouble r2 = x*x + y*y;

        b[0] += z[i];
        b[1] += x*z[i];
        b[2] += y*z[i];
        b[3] += r2*z[i];

        a[2] += x*x;
        a[4] += x*y;
        a[5] += y*y;
        a[6] += r2;
        a[7] += x*r2;
        a[8] += y*r2;
        a[9] += r2*r2;
    }
    a[0] = n;
    a[1] = a[3] = 0.0;

    param[0] = xc;
    param[1] = yc;
    param[2] = b[0]/n;
    param[3] = 0.0;

    if (!gwy_math_choleski_decompose(4, a))
        return FALSE;

    gwy_math_choleski_solve(4, a, b);

    param[3] = 2.0*b[3];
    if (param[3]) {
        param[0] = -b[1]/param[3];
        param[1] = -b[2]/param[3];
        param[2] = b[0] - 0.5*(b[1]*b[1] + b[2]*b[2])/param[3];
    }

    return TRUE;
}

static gdouble
grating_func(gdouble fake_x,
             G_GNUC_UNUSED gint n_param,
             const gdouble *param,
             gpointer user_data,
             gboolean *fres)
{
#ifdef FIT_SHAPE_CACHE
    static gdouble c_last = 0.0, cosh_c_last = 1.0;
    static gdouble alpha_last = 0.0, ca_last = 1.0, sa_last = 0.0;
#endif

    const FitShapeContext *ctx = (const FitShapeContext*)user_data;
    gdouble w = param[0];
    gdouble p = param[1];
    gdouble h = param[2];
    gdouble z0 = param[3];
    gdouble x0 = param[4];
    gdouble alpha = param[5];
    gdouble c = param[6];
    gdouble x, y, t, wp2, val, cosh_c, ca, sa;
    guint i;

    g_assert(n_param == 7);

    i = (guint)fake_x;
    x = ctx->xy[i].x;
    y = ctx->xy[i].y;

    /* FIXME: This is pretty unsafe.  Must handle w→0, c→0, ... */
    wp2 = 0.5*w*p;
#ifdef FIT_SHAPE_CACHE
    if (alpha == alpha_last) {
        ca = ca_last;
        sa = sa_last;
    }
    else {
        ca = ca_last = cos(alpha);
        sa = sa_last = sin(alpha);
        alpha_last = alpha;
    }
#else
    ca = cos(alpha);
    sa = sin(alpha);
#endif
    t = x*ca - y*sa - x0 + wp2;
    t = (t - w*floor(t/w))/wp2 - 1.0;
    if (fabs(t) < 1.0) {
#ifdef FIT_SHAPE_CACHE
        if (c == c_last)
            cosh_c = cosh_c_last;
        else {
            cosh_c = cosh_c_last = cosh(c);
            c_last = c;
        }
#else
        cosh_c = cosh(c);
#endif

        val = z0 + h*(1.0 - (cosh(c*t) - 1.0)/(cosh_c - 1.0));
    }
    else
        val = z0;

    *fres = TRUE;
    return val;
}

static gdouble
projection_to_line(const GwyXY *xy,
                   const gdouble *z,
                   guint n,
                   gdouble alpha,
                   gdouble xc, gdouble yc,
                   GwyDataLine *mean_line,
                   GwyDataLine *rms_line,
                   guint *counts)
{
    guint res = gwy_data_line_get_res(mean_line);
    gdouble *mean = gwy_data_line_get_data(mean_line);
    gdouble *rms = rms_line ? gwy_data_line_get_data(rms_line) : NULL;
    gdouble dx = gwy_data_line_get_real(mean_line)/res;
    gdouble off = gwy_data_line_get_offset(mean_line);
    gdouble c = cos(alpha), s = sin(alpha), total_ms = 0.0;
    guint i, total_n = 0;
    gint j;

    gwy_data_line_clear(mean_line);
    gwy_clear(counts, res);

    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x - xc, y = xy[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res) {
            mean[j] += z[i];
            counts[j]++;
        }
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            mean[j] /= counts[j];
        }
    }

    if (!rms_line)
        return 0.0;

    gwy_data_line_clear(rms_line);

    for (i = 0; i < n; i++) {
        gdouble x = xy[i].x - xc, y = xy[i].y - yc;
        x = x*c - y*s;
        j = (gint)floor((x - off)/dx);
        if (j >= 0 && j < res)
            rms[j] += (z[i] - mean[j])*(z[i] - mean[j]);
    }

    for (j = 0; j < res; j++) {
        if (counts[j]) {
            total_ms += rms[j];
            rms[j] = sqrt(rms[j]/counts[j]);
            total_n += counts[j];
        }
    }

    return sqrt(total_ms/total_n);
}

/* Find direction along which projections capture best the shape, i.e. most
 * variance remains in the line-averaged data. */
static gdouble
estimate_projection_direction(const GwyXY *xy,
                              const gdouble *z,
                              guint n)
{
    enum { NROUGH = 48, NFINE = 8 };

    GwyDataLine *mean_line, *rms_line;
    guint *counts;
    gdouble xc, yc, r, alpha, alpha0, alpha_step, rms;
    gdouble best_rms = G_MAXDOUBLE, best_alpha = 0.0;
    guint iter, i, ni, res;

    circumscribe_x_y(xy, n, &xc, &yc, &r);
    res = (guint)floor(2.0*sqrt(n) + 1.0);

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    rms_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    for (iter = 0; iter < 6; iter++) {
        if (iter == 0) {
            ni = NROUGH;
            alpha_step = G_PI/ni;
            alpha0 = 0.5*alpha_step;
        }
        else {
            /* Choose the fine points so that we do not repeat calculation in
             * any of the rough points. */
            ni = NFINE;
            alpha0 = best_alpha - alpha_step*(NFINE - 1.0)/(NFINE + 1.0);
            alpha_step = 2.0*alpha_step/(NFINE + 1.0);
        }

        for (i = 0; i < ni; i++) {
            alpha = alpha0 + i*alpha_step;
            rms = projection_to_line(xy, z, n, alpha, xc, yc,
                                     mean_line, rms_line, counts);
            gwy_debug("[%u] %g %g", iter, alpha, rms);
            if (rms < best_rms) {
                best_rms = rms;
                best_alpha = alpha;
            }
        }
    }

    g_object_unref(mean_line);
    g_object_unref(rms_line);
    g_free(counts);

    return best_alpha;
}

static gboolean
estimate_period_and_phase(const GwyXY *xy, const gdouble *z, guint n,
                          gdouble alpha, gdouble *pT, gdouble *poff)
{
    GwyDataLine *mean_line, *tmp_line;
    gdouble xc, yc, r, T, t, real, off, a_s, a_c, phi0;
    const gdouble *mean, *tmp;
    guint *counts;
    guint res, i, ibest;
    gboolean found;

    circumscribe_x_y(xy, n, &xc, &yc, &r);
    res = (guint)floor(3.0*sqrt(n) + 1.0);

    *pT = r/4.0;
    *poff = 0.0;

    mean_line = gwy_data_line_new(res, 2.0*r, FALSE);
    gwy_data_line_set_offset(mean_line, -r);
    tmp_line = gwy_data_line_new_alike(mean_line, FALSE);
    counts = g_new(guint, res);

    projection_to_line(xy, z, n, alpha, xc, yc, mean_line, NULL, counts);
    gwy_data_line_add(mean_line, -gwy_data_line_get_avg(mean_line));
    gwy_data_line_psdf(mean_line, tmp_line,
                       GWY_WINDOWING_HANN, GWY_INTERPOLATION_LINEAR);
    tmp = gwy_data_line_get_data_const(tmp_line);

#if 0
    {
        FILE *fh = fopen("psdf.dat", "w");
        for (i = 0; i < tmp_line->res; i++)
            fprintf(fh, "%g %g\n",
                    gwy_data_line_itor(tmp_line, i+0.5), tmp[i]);
        fclose(fh);
    }
#endif

    found = FALSE;
    ibest = G_MAXUINT;
    for (i = 4; i < MIN(res/3, res-3); i++) {
        if (tmp[i] > tmp[i-2] && tmp[i] > tmp[i-1]
            && tmp[i] > tmp[i+1] && tmp[i] > tmp[i+2]) {
            if (ibest == G_MAXUINT || tmp[i] > tmp[ibest]) {
                found = TRUE;
                ibest = i;
            }
        }
    }
    if (!found)
        goto fail;

    T = *pT = 2.0*G_PI/gwy_data_line_itor(tmp_line, ibest);
    gwy_debug("found period %g", T);

    mean = gwy_data_line_get_data_const(mean_line);
    real = gwy_data_line_get_real(mean_line);
    off = gwy_data_line_get_offset(mean_line);
    a_s = a_c = 0.0;
    for (i = 0; i < res; i++) {
        t = off + real/res*(i + 0.5);
        a_s += sin(2*G_PI*t/T)*mean[i];
        a_c += cos(2*G_PI*t/T)*mean[i];
    }
    gwy_debug("a_s %g, a_c %g", a_s, a_c);

    phi0 = atan2(a_s, a_c);
    *poff = phi0*T/(2.0*G_PI) + xc*cos(alpha) - yc*sin(alpha);

fail:
    g_object_unref(mean_line);
    g_object_unref(tmp_line);
    g_free(counts);

    return found;
}

static gboolean
grating_estimate(const GwyXY *xy,
                 const gdouble *z,
                 guint n,
                 gdouble *param)
{
    GwyXY *xyred = NULL;
    gdouble *zred = NULL;
    guint nred = 0;
    gdouble t;

    /* Just initialise the percentage and shape with some sane defaults. */
    param[1] = 0.5;
    param[6] = 5.0;

    /* Simple height parameter estimate. */
    range_z(z, n, param+3, param+2);
    t = param[2] - param[3];
    param[2] = 0.9*t;
    param[3] += 0.05*t;

    /* First we estimate the orientation (alpha). */
    if (n > NREDLIM) {
        nred = sqrt(n*(gdouble)NREDLIM);
        xyred = g_new(GwyXY, nred);
        zred = g_new(gdouble, nred);
        reduce_data_size(xy, z, n, xyred, zred, nred);
    }

    if (nred)
        param[5] = estimate_projection_direction(xyred, zred, nred);
    else
        param[5] = estimate_projection_direction(xy, z, n);

    if (nred) {
        g_free(xyred);
        g_free(zred);
    }

    /* Then we extract a representative profile with this orientation. */
    return estimate_period_and_phase(xy, z, n, param[5], param + 0, param + 4);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
