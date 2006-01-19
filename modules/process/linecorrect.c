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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/arithmetic.h>
#include <libprocess/filters.h>
#include <libprocess/linestats.h>
#include <app/gwyapp.h>

#define LINECORR_RUN_MODES GWY_RUN_IMMEDIATE

#define GOLDEN_RATIO .6180339887498948482

typedef struct {
    gdouble *a;
    gdouble *b;
    guint n;
} MedianLineData;

static gboolean module_register    (const gchar *name);
static void     line_correct_modus (GwyContainer *data,
                                    GwyRunType run);
static void     line_correct_median(GwyContainer *data,
                                    GwyRunType run);
static void     line_correct_match (GwyContainer *data,
                                    GwyRunType run);
static void     line_correct_step  (GwyContainer *data,
                                    GwyRunType run);
static gdouble  find_minima_golden (gdouble (*func)(gdouble x,
                                    gpointer data),
                                    gdouble from,
                                    gdouble to,
                                    gpointer data);
static gdouble  sum_of_abs_diff    (gdouble shift,
                                    gpointer data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Corrects line defects (mostly experimental algorithms)."),
    "Yeti <yeti@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo line_correct_modus_func_info = {
        "line_correct_modus",
        N_("/_Correct Data/M_odus Line Correction"),
        (GwyProcessFunc)&line_correct_modus,
        LINECORR_RUN_MODES,
        GWY_MENU_FLAG_DATA,
    };
    static GwyProcessFuncInfo line_correct_median_func_info = {
        "line_correct_median",
        N_("/_Correct Data/M_edian Line Correction"),
        (GwyProcessFunc)&line_correct_median,
        LINECORR_RUN_MODES,
        GWY_MENU_FLAG_DATA,
    };
    static GwyProcessFuncInfo line_correct_match_func_info = {
        "line_correct_match",
        N_("/_Correct Data/Ma_tch Line Correction"),
        (GwyProcessFunc)&line_correct_match,
        LINECORR_RUN_MODES,
        GWY_MENU_FLAG_DATA,
    };
    static GwyProcessFuncInfo line_correct_step_func_info = {
        "line_correct_step",
        N_("/_Correct Data/Ste_p Line Correction"),
        (GwyProcessFunc)&line_correct_step,
        LINECORR_RUN_MODES,
        GWY_MENU_FLAG_DATA,
    };

    gwy_process_func_register(name, &line_correct_modus_func_info);
    gwy_process_func_register(name, &line_correct_median_func_info);
    gwy_process_func_register(name, &line_correct_match_func_info);
    gwy_process_func_register(name, &line_correct_step_func_info);

    return TRUE;
}

static void
line_correct_modus(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield;
    GwyDataLine *line, *modi;
    gint xres, yres, i;
    gdouble modus;

    g_return_if_fail(run & LINECORR_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    xres = gwy_data_field_get_xres(dfield);
    line = gwy_data_line_new(xres, 1.0, FALSE);
    yres = gwy_data_field_get_yres(dfield);
    modi = gwy_data_line_new(yres, 1.0, FALSE);

    for (i = 0; i < yres; i++) {
        gwy_data_field_get_row(dfield, line, i);
        modus = gwy_data_line_get_modus(line, 0);
        gwy_data_line_set_val(modi, i, modus);
    }
    modus = gwy_data_line_get_modus(modi, 0);

    gwy_app_undo_checkpoint(data, "/0/data", NULL);
    for (i = 0; i < yres; i++) {
        gwy_data_field_area_add(dfield, 0, i, xres, i+1,
                                modus - gwy_data_line_get_val(modi, i));
    }

    g_object_unref(modi);
    g_object_unref(line);
    gwy_data_field_data_changed(dfield);
}

static void
line_correct_median(GwyContainer *data, GwyRunType run)
{
    MedianLineData mldata;
    GwyDataField *dfield;
    gint xres, yres, i, j;
    gdouble shift, csum, mindiff, maxdiff, x;
    gdouble *d;

    g_return_if_fail(run & LINECORR_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);

    yres = gwy_data_field_get_yres(dfield);
    xres = gwy_data_field_get_xres(dfield);
    d = gwy_data_field_get_data(dfield);

    csum = 0.0;
    mldata.n = xres;
    for (i = 1; i < yres; i++) {
        mldata.a = d + xres*(i - 1);
        mldata.b = d + xres*i;
        mindiff = G_MAXDOUBLE;
        maxdiff = -G_MAXDOUBLE;
        for (j = 0; j < xres; j++) {
            x = mldata.b[j] - mldata.a[j];
            if (x < mindiff)
                mindiff = x;
            if (x > maxdiff)
                maxdiff = x;
        }
        shift = find_minima_golden(sum_of_abs_diff, mindiff, maxdiff, &mldata);
        gwy_data_field_area_add(dfield, 0, i, xres, i+1, -shift);
        csum -= shift;
    }
    gwy_data_field_add(dfield, -csum/(xres*yres));
    gwy_data_field_data_changed(dfield);
}

static gdouble
find_minima_golden(gdouble (*func)(gdouble x, gpointer data),
                   gdouble from,
                   gdouble to,
                   gpointer data)
{
    gdouble a, b, c, d;
    gdouble fa, fb, fc, fd;
    guint i;

    a = from;
    b = to;
    c = GOLDEN_RATIO*a + (1.0 - GOLDEN_RATIO)*b;
    d = (1.0 - GOLDEN_RATIO)*a + GOLDEN_RATIO*b;
    fa = func(a, data);
    fb = func(b, data);
    fc = func(c, data);
    fd = func(d, data);

    g_return_val_if_fail(MAX(fa, fb) >= MAX(fc, fd), 0.0);

    /* more than enough to converge on single precision */
    for (i = 0; i < 40; i++) {
        if (fc < fd) {
            b = d;
            fb = fd;
            d = c;
            fd = fc;
            c = GOLDEN_RATIO*a + (1.0 - GOLDEN_RATIO)*b;
            fc = func(c, data);
        }
        else if (fc > fd) {
            a = c;
            fa = fc;
            c = d;
            fc = fd;
            d = (1.0 - GOLDEN_RATIO)*a + GOLDEN_RATIO*b;
            fd = func(d, data);
        }
        else
            break;
    }

    return (c + d)/2.0;
}

static gdouble
sum_of_abs_diff(gdouble shift,
                gpointer data)
{
    gdouble *a, *b;
    gdouble sum = 0.0;
    guint i, n;

    n = ((MedianLineData*)data)->n;
    a = ((MedianLineData*)data)->a;
    b = ((MedianLineData*)data)->b;
    for (i = 0; i < n; i++)
        sum += fabs(b[i] - (a[i] + shift));

    return sum;
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

    g_return_if_fail(run & LINECORR_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);

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

        g_printerr("%g %g %g\n", m, wsum, lambda);

        s[i] = lambda;
    }
    gwy_data_line_cumulate(shifts);
    for (i = 1; i < yres; i++)
        gwy_data_field_area_add(dfield, 0, i, xres, i+1, s[i]);
    gwy_data_field_add(dfield, -s[yres-1]/(xres*yres));

    g_object_unref(shifts);
    g_free(w);
    gwy_data_field_data_changed(dfield);
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

    g_return_if_fail(run & LINECORR_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    gwy_app_undo_checkpoint(data, "/0/data", NULL);

    mask = gwy_data_field_new_alike(dfield, TRUE);
    line_correct_step_iter(dfield, mask);
    gwy_data_field_clear(mask);
    line_correct_step_iter(dfield, mask);
    g_object_unref(mask);

    gwy_data_field_filter_conservative(dfield, 5);
    gwy_data_field_data_changed(dfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
