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
#include <libprocess/arithmetic.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>

#define COERCE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    COERCE_DISTRIBUTION_DATA     = 0,
    COERCE_DISTRIBUTION_UNIFORM  = 1,
    COERCE_DISTRIBUTION_GAUSSIAN = 2,
    COERCE_NDISTRIBUTIONS
} CoerceDistributionType;

typedef enum {
    COERCE_PROCESSING_FIELD   = 0,
    COERCE_PROCESSING_ROWS    = 1,
    COERCE_NPROCESSING
} CoerceProcessingType;

typedef struct {
    CoerceDistributionType distribution;
    CoerceProcessingType processing;
    GwyAppDataId template;
} CoerceArgs;

typedef struct {
    CoerceArgs *args;
    GwyDataField *dfield;
    GtkWidget *dialogue;
    GSList *distribution;
    GSList *processing;
    GtkWidget *template;
} CoerceControls;

typedef struct {
    gdouble z;
    guint k;
} ValuePos;

static gboolean      module_register       (void);
static void          coerce                (GwyContainer *data,
                                            GwyRunType run);
static gboolean      coerce_dialogue       (CoerceArgs *args,
                                            GwyDataField *dfield);
static void          distribution_changed  (GtkToggleButton *toggle,
                                            CoerceControls *controls);
static void          processing_changed    (GtkToggleButton *toggle,
                                            CoerceControls *controls);
static void          template_changed      (GwyDataChooser *chooser,
                                            CoerceControls *controls);
static void          update_sensitivity    (CoerceControls *controls);
static gboolean      template_filter       (GwyContainer *data,
                                            gint id,
                                            gpointer user_data);
static GwyDataField* coerce_do             (GwyDataField *dfield,
                                            const CoerceArgs *args);
static void          coerce_do_field       (GwyDataField *dfield,
                                            GwyDataField *result,
                                            const CoerceArgs *args);
static void          coerce_do_rows        (GwyDataField *dfield,
                                            GwyDataField *result,
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
    COERCE_PROCESSING_FIELD,
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

    if (args.distribution == COERCE_DISTRIBUTION_DATA
        && !template_filter(gwy_app_data_browser_get(args.template.datano),
                            args.template.id, dfield))
        args.distribution = coerce_defaults.distribution;

    if (run == GWY_RUN_IMMEDIATE
        || (run == GWY_RUN_INTERACTIVE && coerce_dialogue(&args, dfield))) {
        result = coerce_do(dfield, &args);

        newid = gwy_app_data_browser_add_data_field(result, data, TRUE);
        g_object_unref(result);
        gwy_app_sync_data_items(data, data, id, newid, FALSE,
                                GWY_DATA_ITEM_PALETTE,
                                GWY_DATA_ITEM_RANGE_TYPE,
                                GWY_DATA_ITEM_REAL_SQUARE,
                                0);
        gwy_app_set_data_field_title(data, newid, _("Coerced"));
        gwy_app_channel_log_add_proc(data, id, newid);
    }

    save_args(settings, &args);
}

static gboolean
coerce_dialogue(CoerceArgs *args, GwyDataField *dfield)
{
    static const GwyEnum distributions[] = {
        { N_("distribution|Uniform"),  COERCE_DISTRIBUTION_UNIFORM,  },
        { N_("distribution|Gaussian"), COERCE_DISTRIBUTION_GAUSSIAN, },
        { N_("As another data"),       COERCE_DISTRIBUTION_DATA,     },
    };

    static const GwyEnum processings[] = {
        { N_("Entire image"),         COERCE_PROCESSING_FIELD, },
        { N_("By row (identically)"), COERCE_PROCESSING_ROWS,  },
    };

    CoerceControls controls;
    GtkWidget *dialogue, *table, *label;
    GwyDataChooser *chooser;
    gint row, response;

    controls.args = args;
    controls.dfield = dfield;

    dialogue = gtk_dialog_new_with_buttons(_("Coerce Statistics"),
                                           NULL, 0,
                                           GTK_STOCK_CANCEL,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_STOCK_OK,
                                           GTK_RESPONSE_OK,
                                           NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialogue), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    controls.dialogue = dialogue;

    table = gtk_table_new(COERCE_NDISTRIBUTIONS + COERCE_NPROCESSING + 4,
                          4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), table,
                       TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Coerce value distribution to:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.distribution
        = gwy_radio_buttons_create(distributions, G_N_ELEMENTS(distributions),
                                   G_CALLBACK(distribution_changed), &controls,
                                   args->distribution);
    row = gwy_radio_buttons_attach_to_table(controls.distribution,
                                            GTK_TABLE(table),
                                            3, row);

    controls.template = gwy_data_chooser_new_channels();
    chooser = GWY_DATA_CHOOSER(controls.template);
    gwy_data_chooser_set_active_id(chooser, &args->template);
    gwy_data_chooser_set_filter(chooser, &template_filter, dfield, NULL);
    gwy_data_chooser_set_active_id(chooser, &args->template);
    gwy_data_chooser_get_active_id(chooser, &args->template);
    gwy_table_attach_hscale(table, row, _("_Template:"), NULL,
                            GTK_OBJECT(controls.template), GWY_HSCALE_WIDGET);
    g_signal_connect(controls.template, "changed",
                     G_CALLBACK(template_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new(_("Data processing:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.processing
        = gwy_radio_buttons_create(processings, G_N_ELEMENTS(processings),
                                   G_CALLBACK(processing_changed), &controls,
                                   args->processing);
    row = gwy_radio_buttons_attach_to_table(controls.processing,
                                            GTK_TABLE(table),
                                            3, row);

    update_sensitivity(&controls);

    gtk_widget_show_all(dialogue);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialogue));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialogue);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialogue);

    return TRUE;
}

static void
distribution_changed(GtkToggleButton *toggle,
                     CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(toggle))
        return;

    args->distribution = gwy_radio_buttons_get_current(controls->distribution);
    update_sensitivity(controls);
}

static void
processing_changed(GtkToggleButton *toggle,
                   CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(toggle))
        return;

    args->processing = gwy_radio_buttons_get_current(controls->processing);
    update_sensitivity(controls);
}

static void
template_changed(GwyDataChooser *chooser,
                 CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    gwy_data_chooser_get_active_id(chooser, &args->template);
}

static void
update_sensitivity(CoerceControls *controls)
{
    CoerceArgs *args = controls->args;
    GtkWidget *widget;
    gboolean has_template, is_data;

    has_template
        = !!gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->template),
                                        NULL);
    widget = gwy_radio_buttons_find(controls->distribution,
                                    COERCE_DISTRIBUTION_DATA);
    gtk_widget_set_sensitive(widget, has_template);

    is_data = (args->distribution == COERCE_DISTRIBUTION_DATA);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->template), is_data);
}

static gboolean
template_filter(GwyContainer *data,
                gint id,
                gpointer user_data)
{
    GwyDataField *template, *dfield = (GwyDataField*)user_data;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    if (!gwy_container_gis_object(data, quark, &template))
        return FALSE;

    if (template == dfield)
        return FALSE;

    return !gwy_data_field_check_compatibility(dfield, template,
                                               GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE);
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

    if (args->processing == COERCE_PROCESSING_FIELD)
        coerce_do_field(dfield, result, args);
    else if (args->processing == COERCE_PROCESSING_ROWS)
        coerce_do_rows(dfield, result, args);
    else {
        g_assert_not_reached();
    }

    return result;
}

static void
coerce_do_field(GwyDataField *dfield, GwyDataField *result,
                const CoerceArgs *args)
{
    guint n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    ValuePos *vpos = g_new(ValuePos, n);
    const gdouble *d = gwy_data_field_get_data_const(dfield);
    gdouble *z = g_new(gdouble, n), *dr;
    guint k;

    if (args->distribution == COERCE_DISTRIBUTION_DATA) {
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
        g_return_if_reached();
    }

    dr = gwy_data_field_get_data(result);
    for (k = 0; k < n; k++) {
        vpos[k].z = d[k];
        vpos[k].k = k;
    }
    qsort(vpos, n, sizeof(ValuePos), compare_double);
    for (k = 0; k < n; k++)
        dr[vpos[k].k] = z[k];

    g_free(z);
    g_free(vpos);
}

static void
coerce_do_rows(GwyDataField *dfield, GwyDataField *result,
               const CoerceArgs *args)
{
    guint xres = gwy_data_field_get_xres(dfield),
          yres = gwy_data_field_get_yres(dfield);
    ValuePos *vpos = g_new(ValuePos, xres);
    const gdouble *d = gwy_data_field_get_data_const(dfield);
    gdouble *z = g_new(gdouble, xres), *dr;
    guint i, j;

    if (args->distribution == COERCE_DISTRIBUTION_DATA) {
        GQuark quark = gwy_app_get_data_key_for_id(args->template.id);
        GwyContainer *data = gwy_app_data_browser_get(args->template.datano);
        GwyDataField *src = gwy_container_get_object(data, quark);
        guint nsrc = gwy_data_field_get_xres(src)*gwy_data_field_get_yres(src);
        build_values_from_data(z, xres,
                               gwy_data_field_get_data_const(src), nsrc);
    }
    else if (args->distribution == COERCE_DISTRIBUTION_UNIFORM) {
        gdouble min, max;
        gwy_data_field_get_min_max(dfield, &min, &max);
        build_values_uniform(z, xres, min, max);
    }
    else if (args->distribution == COERCE_DISTRIBUTION_GAUSSIAN) {
        gdouble avg, rms;
        avg = gwy_data_field_get_avg(dfield);
        rms = gwy_data_field_get_rms(dfield);
        build_values_gaussian(z, xres, avg, rms);
    }
    else {
        g_return_if_reached();
    }

    dr = gwy_data_field_get_data(result);
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            vpos[j].z = d[i*xres + j];
            vpos[j].k = j;
        }
        qsort(vpos, xres, sizeof(ValuePos), compare_double);
        for (j = 0; j < xres; j++)
            dr[i*xres + vpos[j].k] = z[j];
    }

    g_free(z);
    g_free(vpos);
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
            z[0] = sorted[ndata/2];
        else if (n == 2) {
            z[0] = sorted[0];
            z[1] = sorted[ndata-1];
        }
        g_free(sorted);
        return;
    }

    for (i = 0; i < n; i++) {
        gdouble x = (ndata - 1.0)*i/(n - 1.0);
        gint j = (gint)floor(x);

        if (G_UNLIKELY(j >= ndata-1)) {
            j = ndata-2;
            x = 1.0;
        }
        else
            x -= j;

        z[i] = sorted[j]*(1.0 - x) + sorted[j+1]*x;
    }

    g_free(sorted);
}

static const gchar distribution_key[] = "/module/coerce/distribution";
static const gchar processing_key[]   = "/module/coerce/processing";

static void
sanitize_args(CoerceArgs *args)
{
    args->distribution = MIN(args->distribution, COERCE_NDISTRIBUTIONS-1);
    args->processing = MIN(args->processing, COERCE_NPROCESSING-1);
    if (!gwy_app_data_id_verify_channel(&args->template))
        args->distribution = coerce_defaults.distribution;
}

static void
load_args(GwyContainer *container,
          CoerceArgs *args)
{
    *args = coerce_defaults;

    gwy_container_gis_enum_by_name(container, distribution_key,
                                   &args->distribution);
    gwy_container_gis_enum_by_name(container, processing_key,
                                   &args->processing);
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
    gwy_container_set_enum_by_name(container, processing_key,
                                   args->processing);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
