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

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyrandgenset.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define COERCE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    COERCE_DISTRIBUTION_DATA     = 0,
    COERCE_DISTRIBUTION_GAUSSIAN = 1,
    COERCE_DISTRIBUTION_UNIFORM  = 2,
    COERCE_NDISTRIBUTIONS
} CoerceDistributionType;

typedef struct {
    CoerceDistributionType distribution;
    gboolean profiles;
    GwyAppDataId template;
} CoerceArgs;

typedef struct {
    gdouble z;
    guint k;
} ValuePos;

static gboolean      module_register       (void);
static void          coerce                (GwyContainer *data,
                                            GwyRunType run);
static GwyDataField* coerce_do             (GwyDataField *dfield,
                                            const CoerceArgs *args);
static void          build_values_uniform  (gdouble *z,
                                            guint n,
                                            gdouble min,
                                            gdouble max);
static void          build_values_gaussian (gdouble *z,
                                            guint n,
                                            gdouble mean,
                                            gdouble rms);
static void          build_values_from_data(gdouble *z,
                                            guint n,
                                            const gdouble *data,
                                            guint ndata);
static void          load_args             (GwyContainer *container,
                                            CoerceArgs *args);
static void          save_args             (GwyContainer *container,
                                            CoerceArgs *args);

static const CoerceArgs coerce_defaults = {
    COERCE_DISTRIBUTION_UNIFORM,
    FALSE,
    GWY_APP_DATA_ID_NONE,
};

static GwyAppDataId template_id = GWY_APP_DATA_ID_NONE;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Transforms surfaces to have prescribed statistical properties."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("coerce",
                              (GwyProcessFunc)&coerce,
                              N_("/S_ynthetic/Co_erce..."),
                              NULL,
                              COERCE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              _("Enforce prescribed statistical properties"));

    return TRUE;
}

static void
coerce(GwyContainer *data, GwyRunType run)
{
    CoerceArgs args;
    GwyContainer *settings;
    GwyDataField *dfield, *result;
    gint id, newid;

    g_return_if_fail(run & COERCE_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);

    settings = gwy_app_settings_get();
    load_args(settings, &args);
    result = coerce_do(dfield, &args);
    save_args(settings, &args);

    newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
    g_object_unref(result);
    gwy_app_sync_data_items(data, data, id, newid, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            0);
    gwy_app_set_data_field_title(data, newid, _("Coerced"));
    gwy_app_channel_log_add_proc(data, id, newid);
}

static int
compare_double(const void *a, const void *b)
{
    const gdouble *da = (const gdouble*)a;
    const gdouble *db = (const gdouble*)b;

    if (*da < *db)
        return -1;
    if (*da > *db)
        return 1;
    return 0;
}

static GwyDataField*
coerce_do(GwyDataField *dfield, const CoerceArgs *args)
{
    GwyDataField *result = gwy_data_field_new_alike(dfield, FALSE);
    guint n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    ValuePos *vpos = g_new(ValuePos, n);
    const gdouble *d = gwy_data_field_get_data_const(dfield);
    gdouble *z = g_new(gdouble, n), *dr;
    guint k;

    for (k = 0; k < n; k++) {
        vpos[k].z = d[k];
        vpos[k].k = k;
    }
    qsort(vpos, n, sizeof(ValuePos), compare_double);

    if (args->distribution == COERCE_DISTRIBUTION_DATA) {
        /* FIXME: Must check if the field exists! */
        GQuark quark = gwy_app_get_data_key_for_id(args->template.id);
        GwyContainer *data = gwy_app_data_browser_get(args->template.datano);
        GwyDataField *src = gwy_container_get_object(data, quark);
        guint nsrc = gwy_data_field_get_xres(src)*gwy_data_field_get_yres(src);
        build_values_from_data(z, n,
                               gwy_data_field_get_data_const(src), nsrc);
    }
    else if (args->distribution == COERCE_DISTRIBUTION_UNIFORM) {
        gdouble min, max;
        gwy_data_field_get_min_max(dfield, &min, &max);
        build_values_uniform(z, n, min, max);
    }
    else if (args->distribution == COERCE_DISTRIBUTION_GAUSSIAN) {
        gdouble avg, rms;
        avg = gwy_data_field_get_avg(dfield);
        rms = gwy_data_field_get_rms(dfield);
        build_values_gaussian(z, n, avg, rms);
    }
    else {
        g_return_val_if_reached(result);
    }

    dr = gwy_data_field_get_data(result);
    for (k = 0; k < n; k++)
        dr[vpos[k].k] = z[k];

    g_free(z);
    g_free(vpos);

    return result;
}

static void
build_values_uniform(gdouble *z, guint n, gdouble min, gdouble max)
{
    gdouble x;
    guint i;

    for (i = 0; i < n; i++) {
        x = i/(n - 1.0);
        z[i] = min + x*(max - min);
    }
}

/* FIXME: It would be nice to do this deterministically, but for that we need
 * to invert the error function – or there is a better way? */
static void
build_values_gaussian(gdouble *z, guint n, gdouble mean, gdouble rms)
{
    GwyRandGenSet *rngset = gwy_rand_gen_set_new(1);
    guint i;

    for (i = 0; i < n; i++)
        z[i] = gwy_rand_gen_set_gaussian(rngset, 0, rms);

    gwy_rand_gen_set_free(rngset);
    gwy_math_sort(n, z);

    for (i = 0; i < n; i++)
        z[i] += mean;
}

static void
build_values_from_data(gdouble *z, guint n, const gdouble *data, guint ndata)
{
    gdouble *sorted;
    guint i;

    if (n == ndata) {
        memcpy(z, data, n*sizeof(gdouble));
        gwy_math_sort(n, z);
        return;
    }

    if (ndata < 2) {
        for (i = 0; i < n; i++)
            z[i] = data[0];
        return;
    }

    sorted = g_memdup(data, ndata*sizeof(gdouble));
    gwy_math_sort(ndata, sorted);

    if (n < 3) {
        if (n == 1)
            z[0] = data[ndata/2];
        else if (n == 2) {
            z[0] = data[0];
            z[1] = data[ndata-1];
        }
        g_free(sorted);
        return;
    }

    for (i = 0; i < n; i++) {
        gdouble x = (ndata - 1.0)*i/(n - 1.0);
        gint j = (gint)floor(x);

        if (G_UNLIKELY(j >= n-1)) {
            j = n-2;
            x = 1.0;
        }

        z[i] = sorted[j]*(1.0 - x) + sorted[j+1]*x;
    }

    g_free(sorted);
}

static const gchar distribution_key[] = "/module/coerce/distribution";
static const gchar profiles_key[]     = "/module/coerce/profiles";

static void
sanitize_args(CoerceArgs *args)
{
    args->distribution = MIN(args->distribution, COERCE_NDISTRIBUTIONS-1);
    args->profiles = !!args->profiles;
    gwy_app_data_id_verify_channel(&args->template);
}

static void
load_args(GwyContainer *container,
          CoerceArgs *args)
{
    *args = coerce_defaults;

    gwy_container_gis_enum_by_name(container, distribution_key,
                                   &args->distribution);
    gwy_container_gis_boolean_by_name(container, profiles_key, &args->profiles);
    args->template = template_id;
    sanitize_args(args);
}

static void
save_args(GwyContainer *container,
          CoerceArgs *args)
{
    template_id = args->template;
    gwy_container_set_enum_by_name(container, distribution_key,
                                   args->distribution);
    gwy_container_set_boolean_by_name(container, profiles_key, args->profiles);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
