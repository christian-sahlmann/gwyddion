/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwynlfitpreset.h>
#include <libprocess/level.h>
#include <libprocess/stats.h>
#include <libprocess/grains.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define FLATTEN_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

static gboolean  module_register     (void);
static void      flatten_base        (GwyContainer *data,
                                      GwyRunType run);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Levels the flat base of a surface with positive features."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("flatten_base",
                              (GwyProcessFunc)&flatten_base,
                              N_("/_Level/Flatten _Base"),
                              NULL,
                              FLATTEN_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Flatten base of surface with positive features"));

    return TRUE;
}

static gboolean
find_base_peak(GwyDataField *dfield, gdouble *mean, gdouble *rms)
{
    GwyNLFitPreset *gaussian;
    GwyNLFitter *fitter;
    GwyDataLine *dh = gwy_data_line_new(1, 1.0, FALSE);
    gdouble *d, *xdata, *ydata;
    gdouble real, off, dhmax = -G_MAXDOUBLE;
    gdouble param[4];
    gint i, res, from, to, ndata, m = 0;
    gboolean retval;

    gwy_data_field_dh(dfield, dh, 0);
    d = gwy_data_line_get_data(dh);
    res = dh->res;
    real = dh->real;
    off = dh->off;
    for (i = 0; i < res; i++) {
        if (d[i] > dhmax) {
            dhmax = d[i];
            m = i;
        }
    }

    for (from = m; from > 0; from--) {
        if (d[from] < 0.3*dhmax)
            break;
    }
    for (to = m; to < res-1; to++) {
        if (d[to] < 0.3*dhmax)
            break;
    }

    ndata = to+1 - from;
    while (ndata < 7) {
        if (from)
            from--;
        if (to < res-1)
            to++;
        ndata = to+1 - from;
    }

    xdata = g_new(gdouble, ndata);
    ydata = g_new(gdouble, ndata);
    for (i = 0; i < ndata; i++) {
        xdata[i] = (i + from + 0.5)*real/res + off;
        ydata[i] = d[i + from];
    }
    g_object_unref(dh);

    /* x0, y0, a, b */
    param[0] = (m + 0.5)*real/res + off;
    param[1] = 0.0;
    param[2] = dhmax;
    param[3] = 0.3*ndata * real/res;

    gaussian = gwy_inventory_get_item(gwy_nlfit_presets(), "Gaussian");
    fitter = gwy_nlfit_preset_fit(gaussian, NULL,
                                  ndata, xdata, ydata, param, NULL, NULL);
    retval = !!fitter->covar;
    *mean = param[0];
    *rms = param[3]/G_SQRT2;

    g_free(xdata);
    g_free(ydata);
    gwy_math_nlfit_free(fitter);

    return retval;
}

static void
grow_mask(GwyDataField *mask, gint amount)
{
    gint xres = mask->xres, yres = mask->yres;
    gdouble *buffer = g_new(gdouble, xres);
    gdouble *prow = g_new(gdouble, xres);
    gdouble *data = gwy_data_field_get_data(mask);
    gint iter, rowstride, i, j;
    gdouble min, max, q1, q2;

    gwy_debug("grow amount %d", amount);

    if (amount > 1)
        max = gwy_data_field_get_max(mask);
    else
        max = 1.0;

    for (iter = 0; iter < amount; iter++) {
        rowstride = xres;
        min = G_MAXDOUBLE;
        for (j = 0; j < xres; j++)
            prow[j] = -G_MAXDOUBLE;
        memcpy(buffer, data, xres*sizeof(gdouble));
        for (i = 0; i < yres; i++) {
            gdouble *row = data + i*xres;

            if (i == yres-1)
                rowstride = 0;

            j = 0;
            q2 = MAX(buffer[j], buffer[j+1]);
            q1 = MAX(prow[j], row[j+rowstride]);
            row[j] = MAX(q1, q2);
            min = MIN(min, row[j]);
            for (j = 1; j < xres-1; j++) {
                q1 = MAX(prow[j], buffer[j-1]);
                q2 = MAX(buffer[j], buffer[j+1]);
                q2 = MAX(q2, row[j+rowstride]);
                row[j] = MAX(q1, q2);
                min = MIN(min, row[j]);
            }
            j = xres-1;
            q2 = MAX(buffer[j-1], buffer[j]);
            q1 = MAX(prow[j], row[j+rowstride]);
            row[j] = MAX(q1, q2);
            min = MIN(min, row[j]);

            GWY_SWAP(gdouble*, prow, buffer);
            if (i < yres-1)
                memcpy(buffer, data + (i+1)*xres, xres*sizeof(gdouble));
        }
        if (min == max)
            break;
    }

    g_free(buffer);
    g_free(prow);
}

static gboolean
polylevel_with_mask(GwyDataField *dfield, GwyDataField *mask,
                    gint max_degree,
                    gdouble mean, gdouble rms)
{
    gint nterms = (max_degree + 1)*(max_degree + 2)/2;
    gint *term_powers = g_new(gint, 2*nterms);
    gint i, j, k;
    gdouble min, max, threshold, threshval;
    gdouble *coeffs;

    gwy_data_field_get_min_max(dfield, &min, &max);
    if (max <= min)
        return FALSE;

    threshold = mean + 3*rms;
    threshval = 100.0*(threshold - min)/(max - min);
    gwy_debug("min %g, max %g, threshold %g => threshval %g",
              min, max, threshold, threshval);
    gwy_data_field_grains_mark_height(dfield, mask, threshval, FALSE);
    grow_mask(mask, 1 + max_degree/2);

    k = 0;
    for (i = 0; i <= max_degree; i++) {
        for (j = 0; j <= max_degree - i; j++) {
            term_powers[k++] = i;
            term_powers[k++] = j;
        }
    }

    coeffs = gwy_data_field_fit_poly(dfield, mask, nterms, term_powers,
                                     TRUE, NULL);
    gwy_data_field_subtract_poly(dfield, nterms, term_powers, coeffs);
    g_free(coeffs);

    return TRUE;
}

static void
flatten_base(GwyContainer *data, GwyRunType run)
{
    GwyDataField *dfield, *origfield, *mfield;
    GQuark quark;
    gdouble mean, sigma, min, a, bx, by;
    gboolean found_peak;
    gint id, i;

    g_return_if_fail(run & FLATTEN_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &quark,
                                     GWY_APP_DATA_FIELD, &origfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(origfield && quark);

    dfield = gwy_data_field_duplicate(origfield);
    found_peak = find_base_peak(dfield, &mean, &sigma);
    gwy_debug("initial peak: %s (mean=%g, rms=%g)",
              found_peak ? "OK" : "NOT FOUND", mean, sigma);

    for (i = 0; i < 5; i++) {
        if (!gwy_data_field_fit_facet_plane(dfield, NULL, GWY_MASK_IGNORE,
                                            &a, &bx, &by))
            break;

        gwy_data_field_plane_level(dfield, a, bx, by);
        found_peak = find_base_peak(dfield, &mean, &sigma);
        gwy_debug("facet[%d] peak: %s (mean=%g, rms=%g)",
                  i, found_peak ? "OK" : "NOT FOUND", mean, sigma);
        if (!found_peak)
            break;
    }

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    for (i = 2; i <= 5; i++) {
        polylevel_with_mask(dfield, mfield, i, mean, sigma);
        found_peak = find_base_peak(dfield, &mean, &sigma);
        gwy_debug("poly[%d] peak: %s (mean=%g, rms=%g)",
                  i, found_peak ? "OK" : "NOT FOUND", mean, sigma);
        if (!found_peak)
            break;
    }
    g_object_unref(mfield);

    if (found_peak)
        gwy_data_field_add(dfield, -mean);

    if ((min = gwy_data_field_get_min(dfield)) > 0.0)
        gwy_data_field_add(dfield, -min);

    gwy_app_undo_qcheckpoint(data, quark, NULL);
    gwy_data_field_copy(dfield, origfield, FALSE);
    g_object_unref(dfield);
    gwy_app_channel_log_add_proc(data, id, id);
    gwy_data_field_data_changed(origfield);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
