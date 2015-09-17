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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define LINEMATCH_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

/* Lower symmetric part indexing */
/* i MUST be greater or equal than j */
#define SLi(a, i, j) a[(i)*((i) + 1)/2 + (j)]

enum {
    PREVIEW_SIZE = 320,
    MAX_DEGREE = 5,
};

typedef enum {
    LINE_LEVEL_POLY = 0,
    LINE_LEVEL_MEDIAN = 1,
    LINE_LEVEL_MEDIAN_DIFF = 2,
    LINE_LEVEL_MODUS = 3,
    LINE_LEVEL_MATCH = 4,
    LINE_LEVEL_NMETHODS
} LineMatchMethod;

typedef struct {
    LineMatchMethod method;
    gint max_degree;
    gboolean do_extract;
    gboolean do_plot;
    GwyMaskingType masking;
    GwyOrientation direction;
    /* Runtime state */
    GwyDataField *result;
    GwyDataField *bg;
    GwyDataLine *shifts;
} LineMatchArgs;

typedef struct {
    LineMatchArgs *args;
    GtkWidget *dialog;
    GtkObject *max_degree;
    GSList *masking_group;
    GSList *method_group;
    GtkWidget *do_extract;
    GtkWidget *do_plot;
    GtkWidget *dataview;
    GtkWidget *direction;
    GwyContainer *data;
    GwyDataField *dfield;
    gboolean in_update;
} LineMatchControls;

static gboolean module_register         (void);
static void     linematch               (GwyContainer *data,
                                         GwyRunType run);
static void     linematch_do            (GwyDataField *mask,
                                         LineMatchArgs *args);
static void     linematch_do_poly       (GwyDataField *mask,
                                         const LineMatchArgs *args);
static void     linematch_do_median     (GwyDataField *mask,
                                         const LineMatchArgs *args);
static void     linematch_do_median_diff(GwyDataField *mask,
                                         const LineMatchArgs *args);
static void     linematch_do_modus      (GwyDataField *mask,
                                         const LineMatchArgs *args);
static void     linematch_do_match      (GwyDataField *mask,
                                         const LineMatchArgs *args);
static void     apply_row_shifts        (GwyDataField *dfield,
                                         GwyDataField *bg,
                                         GwyDataLine *shifts);
static void     flip_xy                 (GwyDataField *source,
                                         GwyDataField *dest,
                                         gboolean minor);
static gboolean linematch_dialog        (LineMatchArgs *args,
                                         GwyContainer *data,
                                         GwyDataField *dfield,
                                         GwyDataField *mfield,
                                         gint id);
static void     linematch_dialog_update (LineMatchControls *controls,
                                         LineMatchArgs *args);
static void     degree_changed          (LineMatchControls *controls,
                                         GtkObject *adj);
static void     do_extract_changed      (LineMatchControls *controls,
                                         GtkToggleButton *check);
static void     do_plot_changed         (LineMatchControls *controls,
                                         GtkToggleButton *check);
static void     masking_changed         (GtkToggleButton *button,
                                         LineMatchControls *controls);
static void     method_changed          (GtkToggleButton *button,
                                         LineMatchControls *controls);
static void     direction_changed       (GtkWidget *combo,
                                         LineMatchControls *controls);
static void     update_preview          (LineMatchControls *controls,
                                         LineMatchArgs *args);
static void     load_args               (GwyContainer *container,
                                         LineMatchArgs *args);
static void     save_args               (GwyContainer *container,
                                         LineMatchArgs *args);
static void     sanitize_args           (LineMatchArgs *args);

static const LineMatchArgs linematch_defaults = {
    LINE_LEVEL_MEDIAN,
    1,
    FALSE, FALSE,
    GWY_MASK_IGNORE, GWY_ORIENTATION_HORIZONTAL,
    /* Runtime state */
    NULL, NULL, NULL,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Align rows by various methods."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("align_rows",
                              (GwyProcessFunc)&linematch,
                              N_("/_Correct Data/_Align rows..."),
                              GWY_STOCK_LINE_LEVEL,
                              LINEMATCH_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Align rows using various methods"));

    return TRUE;
}

static void
linematch(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    GQuark quark;
    LineMatchArgs args;
    gboolean ok;
    gint id, newid;

    g_return_if_fail(run & LINEMATCH_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && quark);

    load_args(gwy_app_settings_get(), &args);
    args.bg = gwy_data_field_new_alike(dfield, FALSE);
    args.shifts = gwy_data_line_new(dfield->yres, dfield->yreal, FALSE);
    gwy_data_field_copy_units_to_data_line(dfield, args.shifts);

    if (run == GWY_RUN_INTERACTIVE) {
        ok = linematch_dialog(&args, data, dfield, mfield, id);
        save_args(gwy_app_settings_get(), &args);
        if (!ok)
            goto end;
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        gwy_data_field_copy(args.result, dfield, FALSE);
    }
    else {
        gwy_app_undo_qcheckpointv(data, 1, &quark);
        args.result = g_object_ref(dfield);
        linematch_do(mfield, &args);
    }

    gwy_data_field_data_changed(dfield);
    gwy_app_channel_log_add_proc(data, id, id);

    if (args.do_extract) {
        newid = gwy_app_data_browser_add_data_field(args.bg, data, TRUE);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT,
                                0);
        gwy_app_set_data_field_title(data, newid, _("Row background"));
        gwy_app_channel_log_add(data, id, newid, NULL, NULL);
    }

    if (args.do_plot) {
        GwyGraphModel *gmodel = gwy_graph_model_new();
        GwyGraphCurveModel *gcmodel = gwy_graph_curve_model_new();

        gwy_graph_curve_model_set_data_from_dataline(gcmodel, args.shifts,
                                                     0, 0);
        g_object_set(gcmodel,
                     "description", _("Row background"),
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(0),
                     NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);

        g_object_set(gmodel,
                     "title", _("Row background"),
                     "axis-label-bottom", _("Vertical position"),
                     "axis-label-left", _("Corrected offset"),
                     NULL);
        gwy_graph_model_set_units_from_data_line(gmodel, args.shifts);
        gwy_app_data_browser_add_graph_model(gmodel, data, TRUE);
        g_object_unref(gmodel);
    }

end:
    gwy_object_unref(args.result);
    gwy_object_unref(args.shifts);
    gwy_object_unref(args.bg);
}

static void
linematch_do(GwyDataField *mask,
             LineMatchArgs *args)
{
    GwyMaskingType masking = args->masking;
    GwyDataField *mymask = mask, *tmpresult = NULL;

    if (args->masking == GWY_MASK_IGNORE)
        mask = NULL;
    if (!mask)
        args->masking = GWY_MASK_IGNORE;

    /* Transpose the fields if necessary. */
    if (args->direction == GWY_ORIENTATION_VERTICAL) {
        tmpresult = gwy_data_field_new_alike(args->result, FALSE);
        flip_xy(args->result, tmpresult, FALSE);
        GWY_SWAP(GwyDataField*, args->result, tmpresult);
        if (mask) {
            mymask = gwy_data_field_new_alike(mask, FALSE);
            flip_xy(mask, mymask, FALSE);
        }
    }

    gwy_data_field_resample(args->bg,
                            args->result->xres, args->result->yres,
                            GWY_INTERPOLATION_NONE);

    /* Perform the correction. */
    if (args->method == LINE_LEVEL_POLY)
        linematch_do_poly(mymask, args);
    else if (args->method == LINE_LEVEL_MEDIAN)
        linematch_do_median(mymask, args);
    else if (args->method == LINE_LEVEL_MEDIAN_DIFF)
        linematch_do_median_diff(mymask, args);
    else if (args->method == LINE_LEVEL_MODUS)
        linematch_do_modus(mymask, args);
    else if (args->method == LINE_LEVEL_MATCH)
        linematch_do_match(mymask, args);
    else {
        g_assert_not_reached();
    }

    /* Transpose back if necessary. */
    if (args->direction == GWY_ORIENTATION_VERTICAL) {
        gwy_object_unref(mymask);
        flip_xy(args->result, tmpresult, TRUE);
        GWY_SWAP(GwyDataField*, args->result, tmpresult);
        flip_xy(args->bg, tmpresult, TRUE);
        gwy_data_field_resample(args->bg,
                                args->result->xres, args->result->yres,
                                GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(tmpresult, args->bg, FALSE);
        g_object_unref(tmpresult);
    }

    args->masking = masking;
    gwy_data_field_invalidate(args->result);
    gwy_data_field_invalidate(args->bg);
}

static void
linematch_do_poly(GwyDataField *mask,
                  const LineMatchArgs *args)
{
    GwyDataField *dfield, *bg;
    GwyDataLine *means;
    GwyMaskingType masking;
    gdouble *xpowers, *zxpowers, *matrix;
    gint xres, yres, degree, i, j, k;
    gdouble xc;
    const gdouble *m;
    gdouble *d, *b;

    dfield = args->result;
    bg = args->bg;
    means = args->shifts;
    masking = args->masking;

    degree = args->max_degree;
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xc = 0.5*(xres - 1);
    d = gwy_data_field_get_data(dfield);

    m = mask ? gwy_data_field_get_data_const(mask) : NULL;
    b = gwy_data_field_get_data(bg);

    xpowers = g_new(gdouble, 2*degree+1);
    zxpowers = g_new(gdouble, degree+1);
    matrix = g_new(gdouble, (degree+1)*(degree+2)/2);
    for (i = 0; i < yres; i++) {
        gwy_clear(xpowers, 2*degree+1);
        gwy_clear(zxpowers, degree+1);

        for (j = 0; j < xres; j++) {
            gdouble p = 1.0, x = j - xc;

            if ((masking == GWY_MASK_INCLUDE && m[j] <= 0.0)
                || (masking == GWY_MASK_EXCLUDE && m[j] >= 1.0))
                continue;

            for (k = 0; k <= degree; k++) {
                xpowers[k] += p;
                zxpowers[k] += p*d[j];
                p *= x;
            }
            for (k = degree+1; k <= 2*degree; k++) {
                xpowers[k] += p;
                p *= x;
            }
        }

        /* Solve polynomial coefficients. */
        if (xpowers[0] > degree) {
            for (j = 0; j <= degree; j++) {
                for (k = 0; k <= j; k++)
                    SLi(matrix, j, k) = xpowers[j + k];
            }
            gwy_math_choleski_decompose(degree+1, matrix);
            gwy_math_choleski_solve(degree+1, matrix, zxpowers);
        }
        else
            gwy_clear(zxpowers, degree+1);

        /* Subtract. */
        gwy_data_line_set_val(means, i, zxpowers[0]);
        for (j = 0; j < xres; j++) {
            gdouble p = 1.0, x = j - xc, z = 0.0;

            for (k = 0; k <= degree; k++) {
                z += p*zxpowers[k];
                p *= x;
            }

            d[j] -= z;
            b[j] = z;
        }

        d += xres;
        b += xres;
        m = m ? m+xres : NULL;
    }

    g_free(matrix);
    g_free(zxpowers);
    g_free(xpowers);
}

static void
linematch_do_median(GwyDataField *mask,
                    const LineMatchArgs *args)
{
    GwyDataField *dfield, *bg;
    GwyDataLine *medians, *line;
    GwyMaskingType masking;
    gint xres, yres, i;
    const gdouble *d, *m;
    gdouble median, total_median;
    gdouble *buf;

    dfield = args->result;
    bg = args->bg;
    masking = args->masking;
    medians = args->shifts;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    total_median = gwy_data_field_area_get_median_mask(dfield, mask, masking,
                                                       0, 0, xres, yres);

    d = gwy_data_field_get_data(dfield);
    m = mask ? gwy_data_field_get_data_const(mask) : NULL;
    line = gwy_data_line_new(yres, 1.0, FALSE);
    buf = gwy_data_line_get_data(line);

    if (mask) {
        for (i = 0; i < yres; i++) {
            const gdouble *row = d + i*xres, *mrow = m + i*xres;
            gint count = 0, j;

            if (masking == GWY_MASK_INCLUDE) {
                for (j = 0; j < xres; j++) {
                    if (mrow[j] > 0.0)
                        buf[count++] = row[j];
                }
            }
            else {
                for (j = 0; j < xres; j++) {
                    if (mrow[j] < 1.0)
                        buf[count++] = row[j];
                }
            }

            median = count ? gwy_math_median(count, buf) : total_median;
            gwy_data_line_set_val(medians, i, median);
        }
    }
    else {
        for (i = 0; i < yres; i++) {
            gwy_data_field_get_row(dfield, line, i);
            median = gwy_math_median(xres, gwy_data_line_get_data(line));
            gwy_data_line_set_val(medians, i, median);
        }
    }

    apply_row_shifts(dfield, bg, medians);

    g_object_unref(line);
}

static void
linematch_do_median_diff(GwyDataField *mask,
                         const LineMatchArgs *args)
{
    GwyDataField *dfield, *bg;
    GwyDataLine *medians, *line;
    GwyMaskingType masking;
    gint xres, yres, i;
    const gdouble *d, *m;
    gdouble median;
    gdouble *buf;

    dfield = args->result;
    bg = args->bg;
    masking = args->masking;
    medians = args->shifts;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);
    m = mask ? gwy_data_field_get_data_const(mask) : NULL;
    line = gwy_data_line_new(yres, 1.0, FALSE);
    buf = gwy_data_line_get_data(line);

    gwy_data_line_set_val(medians, 0, 0.0);
    for (i = 0; i+1 < yres; i++) {
        const gdouble *row = d + i*xres, *mrow = m + i*xres;
        gint count = 0, j;

        for (j = 0; j < xres; j++) {
            if ((masking == GWY_MASK_INCLUDE && (mrow[j] <= 0.0
                                                 || mrow[xres + j] <= 0.0))
                || (masking == GWY_MASK_EXCLUDE && (mrow[j] >= 1.0
                                                    || mrow[xres + j] >= 1.0)))
                continue;

            buf[count++] = row[xres + j] - row[j];
        }
        median = count ? gwy_math_median(count, buf) : 0.0;
        gwy_data_line_set_val(medians, i+1,
                              median + gwy_data_line_get_val(medians, i));
    }

    apply_row_shifts(dfield, bg, medians);

    g_object_unref(line);
}

static void
linematch_do_modus(GwyDataField *mask,
                   const LineMatchArgs *args)
{
    GwyDataField *dfield, *bg;
    GwyDataLine *modi, *line;
    GwyMaskingType masking;
    gint xres, yres, i;
    const gdouble *d, *m;
    gdouble modus, total_median;
    gdouble *buf;

    dfield = args->result;
    bg = args->bg;
    masking = args->masking;
    modi = args->shifts;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    total_median = gwy_data_field_area_get_median_mask(dfield, mask, masking,
                                                       0, 0, xres, yres);

    d = gwy_data_field_get_data(dfield);
    m = mask ? gwy_data_field_get_data_const(mask) : NULL;
    line = gwy_data_line_new(yres, 1.0, FALSE);
    buf = gwy_data_line_get_data(line);

    for (i = 0; i < yres; i++) {
        const gdouble *row = d + i*xres, *mrow = m + i*xres;
        gint count = 0, j;

        for (j = 0; j < xres; j++) {
            if ((masking == GWY_MASK_INCLUDE && mrow[j] <= 0.0)
                || (masking == GWY_MASK_EXCLUDE && mrow[j] >= 1.0))
                continue;

            buf[count++] = row[j];
        }

        if (!count)
            modus = total_median;
        else if (count < 9)
            modus = gwy_math_median(count, buf);
        else {
            gint seglen = GWY_ROUND(sqrt(count)), bestj = 0;
            gdouble diff, bestdiff = G_MAXDOUBLE;

            gwy_math_sort(count, buf);
            for (j = 0; j + seglen-1 < count; j++) {
                diff = buf[j + seglen-1] - buf[j];
                if (diff < bestdiff) {
                    bestdiff = diff;
                    bestj = j;
                }
            }
            modus = 0.0;
            count = 0;
            for (j = seglen/3; j < seglen - seglen/3; j++, count++)
                modus += buf[bestj + j];
            modus /= count;
        }

        gwy_data_line_set_val(modi, i, modus);
    }

    apply_row_shifts(dfield, bg, modi);

    g_object_unref(line);
}

static void
linematch_do_match(GwyDataField *mask,
                   const LineMatchArgs *args)
{
    GwyDataField *dfield, *bg;
    GwyDataLine *shifts;
    GwyMaskingType masking;
    gint xres, yres, i, j;
    gdouble q, wsum, lambda, x;
    const gdouble *d, *m, *a, *b, *ma, *mb;
    gdouble *s, *w;

    shifts = args->shifts;
    dfield = args->result;
    bg = args->bg;
    masking = args->masking;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);
    m = mask ? gwy_data_field_get_data_const(mask) : NULL;
    s = gwy_data_line_get_data(shifts);

    w = g_new(gdouble, xres-1);
    for (i = 1; i < yres; i++) {
        a = d + xres*(i - 1);
        b = d + xres*i;
        ma = m + xres*(i - 1);
        mb = m + xres*i;

        /* Diffnorm */
        wsum = 0.0;
        for (j = 0; j < xres-1; j++) {
            if ((masking == GWY_MASK_INCLUDE && (ma[j] <= 0.0
                                                 || mb[j] <= 0.0))
                || (masking == GWY_MASK_EXCLUDE && (ma[j] >= 1.0
                                                    || mb[j] >= 1.0)))
                continue;

            x = a[j+1] - a[j] - b[j+1] + b[j];
            wsum += fabs(x);
        }
        if (wsum == 0) {
            s[i] = s[i-1];
            continue;
        }
        q = wsum/(xres-1);

        /* Weights */
        wsum = 0.0;
        for (j = 0; j < xres-1; j++) {
            if ((masking == GWY_MASK_INCLUDE && (ma[j] <= 0.0
                                                 || mb[j] <= 0.0))
                || (masking == GWY_MASK_EXCLUDE && (ma[j] >= 1.0
                                                    || mb[j] >= 1.0)))
                continue;

            x = a[j+1] - a[j] - b[j+1] + b[j];
            w[j] = exp(-(x*x/(2.0*q)));
            wsum += w[j];
        }

        /* Correction */
        lambda = (a[0] - b[0])*w[0];
        for (j = 1; j < xres-1; j++) {
            if ((masking == GWY_MASK_INCLUDE && (ma[j] <= 0.0
                                                 || mb[j] <= 0.0))
                || (masking == GWY_MASK_EXCLUDE && (ma[j] >= 1.0
                                                    || mb[j] >= 1.0)))
                continue;

            lambda += (a[j] - b[j])*(w[j-1] + w[j]);
        }
        lambda += (a[xres-1] - b[xres-1])*w[xres-2];
        lambda /= 2.0*wsum;

        gwy_debug("%g %g %g", q, wsum, lambda);

        s[i] = -lambda + s[i-1];
    }
    apply_row_shifts(dfield, bg, shifts);

    g_free(w);
}

static void
apply_row_shifts(GwyDataField *dfield, GwyDataField *bg,
                 GwyDataLine *shifts)
{
    gint xres, yres, i, j;
    const gdouble *s;
    gdouble *d, *b;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    d = gwy_data_field_get_data(dfield);
    b = gwy_data_field_get_data(bg);
    s = gwy_data_line_get_data(shifts);
    for (i = 0; i < yres; i++) {
        gdouble z = s[i];
        for (j = 0; j < xres; j++) {
            d[j] -= z;
            b[j] = z;
        }
        d += xres;
        b += xres;
    }
}

static void
flip_xy(GwyDataField *source, GwyDataField *dest, gboolean minor)
{
    gint xres, yres, i, j;
    gdouble *dd;
    const gdouble *sd;

    xres = gwy_data_field_get_xres(source);
    yres = gwy_data_field_get_yres(source);
    gwy_data_field_resample(dest, yres, xres, GWY_INTERPOLATION_NONE);
    sd = gwy_data_field_get_data_const(source);
    dd = gwy_data_field_get_data(dest);
    if (minor) {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + j] = sd[j*xres + (xres - 1 - i)];
            }
        }
    }
    else {
        for (i = 0; i < xres; i++) {
            for (j = 0; j < yres; j++) {
                dd[i*yres + (yres - 1 - j)] = sd[j*xres + i];
            }
        }
    }
    gwy_data_field_set_xreal(dest, gwy_data_field_get_yreal(source));
    gwy_data_field_set_yreal(dest, gwy_data_field_get_xreal(source));
}

static gboolean
linematch_dialog(LineMatchArgs *args,
                 GwyContainer *data,
                 GwyDataField *dfield,
                 GwyDataField *mfield,
                 gint id)
{
    enum { RESPONSE_RESET = 1 };

    static const GwyEnum methods[] = {
        { "Median", LINE_LEVEL_MEDIAN },
        { "Median difference", LINE_LEVEL_MEDIAN_DIFF },
        { "Modus", LINE_LEVEL_MODUS },
        { "Matching", LINE_LEVEL_MATCH },
        /* Put polynomial last so that is it followed visally by the degree
         * controls. */
        { "Polynomial", LINE_LEVEL_POLY },
    };

    GtkWidget *dialog, *table, *label, *hbox, *alignment;
    GwyPixmapLayer *layer;
    LineMatchControls controls;
    gint response;
    gint row;

    controls.args = args;
    controls.in_update = TRUE;
    controls.dfield = dfield;
    controls.data = gwy_container_new();

    args->result = gwy_data_field_duplicate(dfield);

    gwy_container_set_object_by_name(controls.data, "/0/data", args->result);
    gwy_app_sync_data_items(data, controls.data, id, 0, FALSE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            GWY_DATA_ITEM_GRADIENT,
                            GWY_DATA_ITEM_RANGE_TYPE,
                            0);
    if (mfield)
        gwy_container_set_object_by_name(controls.data, "/mask", mfield);

    dialog = gtk_dialog_new_with_buttons(_("Align Rows"),
                                         NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 0);

    controls.dataview = gwy_data_view_new(controls.data);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.dataview), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.dataview), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.dataview), PREVIEW_SIZE);

    alignment = GTK_WIDGET(gtk_alignment_new(0.5, 0, 0, 0));
    gtk_container_add(GTK_CONTAINER(alignment), controls.dataview);
    gtk_box_pack_start(GTK_BOX(hbox), alignment, FALSE, FALSE, 4);

    table = gtk_table_new(6 + LINE_LEVEL_NMETHODS + (mfield ? 4 : 0), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Method:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.method_group
        = gwy_radio_buttons_create(methods, G_N_ELEMENTS(methods),
                                   G_CALLBACK(method_changed),
                                   &controls, args->method);
    row = gwy_radio_buttons_attach_to_table(controls.method_group,
                                            GTK_TABLE(table), 3, row);

    controls.max_degree = gtk_adjustment_new(args->max_degree,
                                             0, MAX_DEGREE, 1, 1, 0);
    gwy_table_attach_hscale(table, row++,
                            _("_Polynomial degree:"), NULL,
                            controls.max_degree, 0);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls.max_degree),
                                   args->method == LINE_LEVEL_POLY);
    g_signal_connect_swapped(controls.max_degree, "value-changed",
                             G_CALLBACK(degree_changed), &controls);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.direction
        = gwy_enum_combo_box_new(gwy_orientation_get_enum(), -1,
                                 G_CALLBACK(direction_changed), &controls,
                                 args->direction, TRUE);
    gwy_table_attach_row(table, row, _("_Direction:"), NULL,
                         controls.direction);
    row++;

    controls.do_extract
        = gtk_check_button_new_with_mnemonic(_("E_xtract background"));
    gtk_table_attach(GTK_TABLE(table), controls.do_extract,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_extract),
                                 args->do_extract);
    g_signal_connect_swapped(controls.do_extract, "toggled",
                             G_CALLBACK(do_extract_changed), &controls);
    row++;

    controls.do_plot
        = gtk_check_button_new_with_mnemonic(_("Plot background _graph"));
    gtk_table_attach(GTK_TABLE(table), controls.do_plot,
                     0, 4, row, row+1, GTK_FILL, 0, 0, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.do_plot),
                                 args->do_plot);
    g_signal_connect_swapped(controls.do_plot, "toggled",
                             G_CALLBACK(do_plot_changed), &controls);
    row++;

    if (mfield) {
        gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
        label = gwy_label_new_header(_("Masking Mode"));
        gtk_table_attach(GTK_TABLE(table), label,
                        0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        controls.masking_group
            = gwy_radio_buttons_create(gwy_masking_type_get_enum(), -1,
                                       G_CALLBACK(masking_changed),
                                       &controls, args->masking);
        row = gwy_radio_buttons_attach_to_table(controls.masking_group,
                                                GTK_TABLE(table), 3, row);
    }
    else
        controls.masking_group = NULL;

    controls.in_update = FALSE;
    update_preview(&controls, args);

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
                GwyDataField *result = args->result, *bg = args->bg;
                GwyDataLine *shifts = args->shifts;
                *args = linematch_defaults;
                args->result = result;
                args->bg = bg;
                args->shifts = shifts;
                linematch_dialog_update(&controls, args);
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
linematch_dialog_update(LineMatchControls *controls,
                       LineMatchArgs *args)
{
    controls->in_update = TRUE;
    gwy_radio_buttons_set_current(controls->method_group, args->method);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->max_degree),
                             args->max_degree);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_extract),
                                 args->do_extract);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->do_plot),
                                 args->do_plot);
    gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->direction),
                                  args->direction);
    if (controls->masking_group)
        gwy_radio_buttons_set_current(controls->masking_group, args->masking);
    controls->in_update = FALSE;
    update_preview(controls, args);
}

static void
degree_changed(LineMatchControls *controls,
               GtkObject *adj)
{
    LineMatchArgs *args = controls->args;

    args->max_degree = gwy_adjustment_get_int(GTK_ADJUSTMENT(adj));
    if (controls->in_update)
        return;

    update_preview(controls, controls->args);
}

static void
do_extract_changed(LineMatchControls *controls,
                   GtkToggleButton *check)
{
    controls->args->do_extract = gtk_toggle_button_get_active(check);
}

static void
do_plot_changed(LineMatchControls *controls,
                         GtkToggleButton *check)
{
    controls->args->do_plot = gtk_toggle_button_get_active(check);
}

static void
masking_changed(GtkToggleButton *button,
                LineMatchControls *controls)
{
    LineMatchArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args->masking = gwy_radio_buttons_get_current(controls->masking_group);
    if (controls->in_update)
        return;

    update_preview(controls, args);
}

static void
method_changed(GtkToggleButton *button,
               LineMatchControls *controls)
{
    LineMatchArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(button))
        return;

    args->method = gwy_radio_buttons_get_current(controls->method_group);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->max_degree),
                                   args->method == LINE_LEVEL_POLY);
    if (controls->in_update)
        return;

    update_preview(controls, args);
}

static void
direction_changed(GtkWidget *combo,
                  LineMatchControls *controls)
{
    LineMatchArgs *args = controls->args;

    args->direction = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (controls->in_update)
        return;

    update_preview(controls, args);
}

static void
update_preview(LineMatchControls *controls, LineMatchArgs *args)
{
    GwyDataField *source, *mask = NULL;

    source = controls->dfield;
    gwy_container_gis_object_by_name(controls->data, "/mask", &mask);
    gwy_data_field_copy(source, args->result, FALSE);
    linematch_do(mask, args);
    gwy_data_field_data_changed(args->result);
}

static const gchar direction_key[]  = "/module/linematch/direction";
static const gchar do_extract_key[] = "/module/linematch/do_extract";
static const gchar do_plot_key[]    = "/module/linematch/do_plot";
static const gchar masking_key[]    = "/module/linematch/masking";
static const gchar method_key[]     = "/module/linematch/method";
static const gchar max_degree_key[] = "/module/linematch/max_degree";

static void
sanitize_args(LineMatchArgs *args)
{
    args->max_degree = CLAMP(args->max_degree, 0, MAX_DEGREE);
    args->masking = gwy_enum_sanitize_value(args->masking,
                                            GWY_TYPE_MASKING_TYPE);
    args->method = MIN(args->method, LINE_LEVEL_NMETHODS-1);
    args->direction = gwy_enum_sanitize_value(args->direction,
                                              GWY_TYPE_ORIENTATION);
    args->do_extract = !!args->do_extract;
    args->do_plot = !!args->do_plot;
}

static void
load_args(GwyContainer *container,
          LineMatchArgs *args)
{
    *args = linematch_defaults;

    gwy_container_gis_int32_by_name(container, max_degree_key,
                                    &args->max_degree);
    gwy_container_gis_enum_by_name(container, masking_key, &args->masking);
    gwy_container_gis_enum_by_name(container, method_key, &args->method);
    gwy_container_gis_enum_by_name(container, direction_key, &args->direction);
    gwy_container_gis_boolean_by_name(container, do_extract_key,
                                      &args->do_extract);
    gwy_container_gis_boolean_by_name(container, do_plot_key,
                                      &args->do_plot);
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          LineMatchArgs *args)
{
    gwy_container_set_int32_by_name(container, max_degree_key,
                                    args->max_degree);
    gwy_container_set_enum_by_name(container, masking_key, args->masking);
    gwy_container_set_enum_by_name(container, method_key, args->method);
    gwy_container_set_enum_by_name(container, direction_key, args->direction);
    gwy_container_set_boolean_by_name(container, do_extract_key,
                                      args->do_extract);
    gwy_container_set_boolean_by_name(container, do_plot_key, args->do_plot);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
