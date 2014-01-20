/*
 *  Copyright (C) 2013 Brazilian Nanotechnology National Laboratory
 *  E-mail: Vinicius Barboza <vinicius.barboza@lnnano.cnpem.br>
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

#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libprocess/filters.h>
#include <libgwymodule/gwymodule.h>
#include <app/gwyapp.h>

/* Running mode */
#define OTSU_RUN_MODES GWY_RUN_IMMEDIATE

/* Constants */
#define FOREGROUND_FLAG 1
#define BACKGROUND_FLAG 0

/* Function prototypes */
static gboolean module_register(void);
static void     otsu_threshold (GwyContainer *data,
                                GwyRunType run);
static gdouble  class_weight   (GwyDataLine *hist,
                                gint t,
                                gint flag);
static gdouble  class_mean     (GwyDataField *dfield,
                                GwyDataLine *hist,
                                gint t,
                                gint flag);

/* Module info */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Automated threshold using Otsu's method on heights."),
    "Vinicius Barboza <vinicius.barboza@lnnano.cnpem.br>",
    "1.1",
    "Brazilian Nanotechnology National Laboratory",
    "2013",
};

/* Module query */
GWY_MODULE_QUERY(module_info)

/* Module register */
static gboolean
module_register(void)
{
    gwy_process_func_register("otsu-threshold",
                              (GwyProcessFunc)&otsu_threshold,
                              N_("/_Grains/_Mark by Otsu's..."),
                              NULL,
                              OTSU_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Automated threshold using Otsu's method "
                                 "on heights.")
                             );
    return TRUE;
}

/**
 * otsu_threshold:
 * @data: #GwyContainer
 * @run: #GwyRunType
 *
 * Does an automated thresholding of a data field using Otsu's method.
 *
 * Since: 2.26
 **/
static void
otsu_threshold(GwyContainer *data,
               GwyRunType run)
{
    GwyDataField *dfield, *mfield;
    GwyDataLine *hist;
    GwySIUnit *siunit;
    GQuark dquark, mquark;

    /* Background statistics*/
    gdouble weight_0, mean_0;
    /* Foreground statistics*/
    gdouble  weight_1, mean_1;
    /* Data field and histogram statistics*/
    gdouble min, max, max_var, var, thresh, bin;
    gint len, t, id;

    /* Running smoothly */
    g_return_if_fail(run & OTSU_RUN_MODES);

    /* Getting fields and quarks */
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_MASK_FIELD, &mfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    /* Setting checkopint */
    gwy_app_undo_qcheckpointv(data, 1, &mquark);

    /* Checking for mask and creating a new one */
    if (!mfield) {
        mfield = gwy_data_field_new_alike(dfield, TRUE);
        siunit = gwy_si_unit_new(NULL);
        gwy_data_field_set_si_unit_z(mfield, siunit);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(siunit);
        g_object_unref(mfield);
    }

    /* Copying the information to the mask field */
    gwy_data_field_copy(dfield, mfield, FALSE);

    /* Getting histogram */
    hist = gwy_data_line_new(1, 1, FALSE);
    gwy_data_field_dh(dfield, hist, -1);

    /* Getting histogram length and max/min values for the data field */
    len = gwy_data_line_get_res(hist);
    max = gwy_data_field_get_max(dfield);
    min = gwy_data_field_get_min(dfield);

    /* Calculating the threshold */
    thresh = max_var = 0;
    for (t = 0; t < len; t++) {
        /* Getting histogram statistics for fg and bg classes */
        weight_0 = class_weight(hist, t, BACKGROUND_FLAG);
        weight_1 = class_weight(hist, t, FOREGROUND_FLAG);
        mean_0 = class_mean(dfield, hist, t, BACKGROUND_FLAG);
        mean_1 = class_mean(dfield, hist, t, FOREGROUND_FLAG);

        /* Interclass variance */
        var = weight_0 * weight_1 * (mean_0 - mean_1) * (mean_0 - mean_1);
        bin = min + ( (t + 0.5) * (max-min) / len );

        /* Check for greater interclass variance */
        if (var > max_var) {
            max_var = var;
            thresh = bin;
        }

    }

    /* Apply threshold to mask field */
    gwy_data_field_threshold(mfield, thresh, 0.0, 1.0);
    gwy_data_field_data_changed(mfield);
    gwy_app_channel_log_add(data, id, id, "proc::otsu-threshold", NULL);

    g_object_unref(hist);

    return;
}

/**
 * class_weight:
 * @hist: A #GwyDataLine histogram from where to calculate the class
 *        probability for @t.
 * @t: A threshold value.
 * @flag: Alternates between %BACKGROUND or %FOREGROUND class probabilities.
 *
 * Returns: The probability for a class given a threshold value.
 **/
static gdouble
class_weight(GwyDataLine *hist,
             gint t,
             gint flag)
{
    gint i;
    gint len;
    gdouble roi, total, weight;
    gdouble *data;

    len = gwy_data_line_get_res(hist);

    roi = 0;
    total = 0;
    data = gwy_data_line_get_data(hist);

    for (i = 0; i < len; i++) {
        total += data[i];
    }

    for (i = (flag)*t; i < (1-flag)*t + (flag)*len; i++) {
        roi += data[i];
    }

    weight = roi/total;

    return weight;
}

/**
 * class_mean:
 * @hist: A #GwyDataLine hiustogram from where to calculate the class mean for
 *        @t.
 * @t: A threshold value.
 * @flag: Alternates between %BACKGROUND or %FOREGROUND class mean.
 *
 * Returns: The mean value for a class given a threshold value.
 **/
static gdouble
class_mean(GwyDataField *dfield,
           GwyDataLine *hist,
           gint t,
           gint flag)
{
    gint i;
    gint len;
    gdouble max, min, bin, val;
    gdouble roi, total, mean;
    gdouble *data;

    len = gwy_data_line_get_res(hist);
    max = gwy_data_field_get_max(dfield);
    min = gwy_data_field_get_min(dfield);

    roi = 0;
    total = 0;
    data = gwy_data_line_get_data(hist);

    if (t == 0 && flag == 0) {
        return 0.0;
    }

    for (i = (flag)*t; i < (1-flag)*t + (flag)*len; i++) {
        val = data[i];
        bin = min + ( (i + 0.5) * (max-min) / len );
        roi += bin * val;
        total += val;
    }

    mean = roi/total;

    return mean;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
