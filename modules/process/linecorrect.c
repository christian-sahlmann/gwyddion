/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek, Luke Somers.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, lsomers@sas.upenn.edu.
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
#include <libprocess/filters.h>
#include <libprocess/linestats.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-process.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <app/gwyapp.h>

#define LINECORR_RUN_MODES GWY_RUN_IMMEDIATE

#define GOLDEN_RATIO .6180339887498948482

typedef struct {
    gdouble *a;
    gdouble *b;
    guint n;
} MedianLineData;

static gboolean module_register                    (void);
static void     line_correct_match                 (GwyContainer *data,
                                                    GwyRunType run);
static void     line_correct_step                  (GwyContainer *data,
                                                    GwyRunType run);
static void     gwy_data_field_absdiff_line_correct(GwyDataField *dfield);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Corrects line defects (mostly experimental algorithms)."),
    "Yeti <yeti@gwyddion.net>, Luke Somers <lsomers@sas.upenn.edu>",
    "1.10",
    "David Nečas (Yeti) & Petr Klapetek & Luke Somers",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("line_correct_match",
                              (GwyProcessFunc)&line_correct_match,
                              N_("/_Correct Data/Ma_tch Line Correction"),
                              GWY_STOCK_LINE_LEVEL,
                              LINECORR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Correct lines by matching flat segments"));
    gwy_process_func_register("line_correct_step",
                              (GwyProcessFunc)&line_correct_step,
                              N_("/_Correct Data/Ste_p Line Correction"),
                              GWY_STOCK_LINE_LEVEL,
                              LINECORR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Correct steps in lines"));

    return TRUE;
}

/* NB: This is in fact median correction. */
static void
gwy_data_field_absdiff_line_correct(GwyDataField *dfield)
{
    gdouble *buf;
    gint xres, yres, i;
    gdouble shift;
    gdouble *d;

    yres = gwy_data_field_get_yres(dfield);
    xres = gwy_data_field_get_xres(dfield);
    d = gwy_data_field_get_data(dfield);

    buf = g_new(gdouble, xres);
    for (i = 0; i < yres; i++) {
        memcpy(buf, d + i*xres, xres*sizeof(gdouble));
        shift = gwy_math_median(xres, buf);
        gwy_data_field_area_add(dfield, 0, i, xres, 1, -shift);
    }
}

static void
line_correct_match(GwyContainer *data,
                   GwyRunType run)
{
    GwyDataField *dfield;
    GwyDataLine *shifts;
    gint xres, yres, i, j;
    gdouble m, wsum, lambda, x;
    gdouble *d, *s, *w;
    const gdouble *a, *b;
    GQuark dquark;
    gint id;

    g_return_if_fail(run & LINECORR_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark);
    gwy_app_undo_qcheckpointv(data, 1, &dquark);

    yres = gwy_data_field_get_yres(dfield);
    xres = gwy_data_field_get_xres(dfield);
    d = gwy_data_field_get_data(dfield);

    shifts = gwy_data_line_new(yres, 1.0, TRUE);
    s = gwy_data_line_get_data(shifts);

    w = g_new(gdouble, xres-1);
    for (i = 1; i < yres; i++) {
        a = d + xres*(i - 1);
        b = d + xres*i;

        /* Diffnorm */
        wsum = 0.0;
        for (j = 0; j < xres-1; j++) {
            x = a[j+1] - a[j] - b[j+1] + b[j];
            wsum += fabs(x);
        }
        if (wsum == 0)
            continue;
        m = wsum/(xres-1);

        /* Weights */
        wsum = 0.0;
        for (j = 0; j < xres-1; j++) {
            x = a[j+1] - a[j] - b[j+1] + b[j];
            w[j] = exp(-(x*x/(2.0*m)));
            wsum += w[j];
        }

        /* Correction */
        lambda = (a[0] - b[0])*w[0];
        for (j = 1; j < xres-1; j++)
            lambda += (a[j] - b[j])*(w[j-1] + w[j]);
        lambda += (a[xres-1] - b[xres-1])*w[xres-2];
        lambda /= 2.0*wsum;

        gwy_debug("%g %g %g", m, wsum, lambda);

        s[i] = lambda;
    }
    gwy_data_line_cumulate(shifts);
    for (i = 1; i < yres; i++)
        gwy_data_field_area_add(dfield, 0, i, xres, 1, s[i]);
    gwy_data_field_add(dfield, -s[yres-1]/(xres*yres));

    g_object_unref(shifts);
    g_free(w);
    gwy_data_field_data_changed(dfield);
    gwy_app_channel_log_add_proc(data, id, id);
}

static void
calcualte_segment_correction(const gdouble *drow,
                             gdouble *mrow,
                             gint xres,
                             gint len)
{
    const gint min_len = 4;
    gdouble corr;
    gint j;

    if (len >= min_len) {
        corr = 0.0;
        for (j = 0; j < len; j++)
            corr += (drow[j] + drow[2*xres + j])/2.0 - drow[xres + j];
        corr /= len;
        for (j = 0; j < len; j++)
            mrow[j] = (3*corr
                       + (drow[j] + drow[2*xres + j])/2.0 - drow[xres + j])/4.0;
    }
    else {
        for (j = 0; j < len; j++)
            mrow[j] = 0.0;
    }
}

static void
line_correct_step_iter(GwyDataField *dfield,
                       GwyDataField *mask)
{
    const gdouble threshold = 3.0;
    gint xres, yres, i, j, len;
    gdouble u, v, w;
    const gdouble *d, *drow;
    gdouble *m, *mrow;

    yres = gwy_data_field_get_yres(dfield);
    xres = gwy_data_field_get_xres(dfield);
    d = gwy_data_field_get_data_const(dfield);
    m = gwy_data_field_get_data(mask);

    w = 0.0;
    for (i = 0; i < yres-1; i++) {
        drow = d + i*xres;
        for (j = 0; j < xres; j++) {
            v = drow[j + xres] - drow[j];
            w += v*v;
        }
    }
    w = w/(yres-1)/xres;

    for (i = 0; i < yres-2; i++) {
        drow = d + i*xres;
        mrow = m + (i + 1)*xres;

        for (j = 0; j < xres; j++) {
            u = drow[xres + j];
            v = (u - drow[j])*(u - drow[j + 2*xres]);
            if (G_UNLIKELY(v > threshold*w)) {
                if (2*u - drow[j] - drow[j + 2*xres] > 0)
                    mrow[j] = 1.0;
                else
                    mrow[j] = -1.0;
            }
        }

        len = 1;
        for (j = 1; j < xres; j++) {
            if (mrow[j] == mrow[j-1])
                len++;
            else {
                if (mrow[j-1])
                    calcualte_segment_correction(drow + j-len, mrow + j-len,
                                                 xres, len);
                len = 1;
            }
        }
        if (mrow[j-1]) {
            calcualte_segment_correction(drow + j-len, mrow + j-len,
                                         xres, len);
        }
    }

    gwy_data_field_sum_fields(dfield, dfield, mask);
}

static void
line_correct_step(GwyContainer *data,
                  GwyRunType run)
{
    GwyDataField *dfield, *mask;
    GQuark dquark;
    gint id;

    g_return_if_fail(run & LINECORR_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark);
    gwy_app_undo_qcheckpointv(data, 1, &dquark);

    gwy_data_field_absdiff_line_correct(dfield);

    mask = gwy_data_field_new_alike(dfield, TRUE);
    line_correct_step_iter(dfield, mask);
    gwy_data_field_clear(mask);
    line_correct_step_iter(dfield, mask);
    g_object_unref(mask);

    gwy_data_field_filter_conservative(dfield, 5);
    gwy_data_field_data_changed(dfield);
    gwy_app_channel_log_add_proc(data, id, id);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
