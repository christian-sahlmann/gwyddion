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
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils.h>
#include "preview.h"

#define COERCE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    COERCE_DISTRIBUTION_DATA     = 0,
    COERCE_DISTRIBUTION_UNIFORM  = 1,
    COERCE_DISTRIBUTION_GAUSSIAN = 2,
    COERCE_DISTRIBUTION_LEVELS   = 3,
    COERCE_NDISTRIBUTIONS
} CoerceDistributionType;

typedef enum {
    COERCE_PROCESSING_FIELD   = 0,
    COERCE_PROCESSING_ROWS    = 1,
    COERCE_NPROCESSING
} CoerceProcessingType;

typedef enum {
    COERCE_LEVELS_UNIFORM  = 0,
    COERCE_LEVELS_EQUIAREA = 1,
    COERCE_NLEVELTYPES
} CoerceLevelsType;

typedef struct {
    CoerceDistributionType distribution;
    CoerceLevelsType level_type;
    gint nlevels;
    CoerceProcessingType processing;
    gboolean update;
    GwyAppDataId template;
} CoerceArgs;

typedef struct {
    CoerceArgs *args;
    GwyContainer *mydata;
    GwyDataField *dfield;
    GtkWidget *dialogue;
    GtkWidget *view;
    GSList *distribution;
    GtkWidget *level_type;
    GtkObject *nlevels;
    GSList *processing;
    GtkWidget *template;
    GtkWidget *update;
} CoerceControls;

typedef struct {
    gdouble z;
    guint k;
} ValuePos;

static gboolean      module_register       (void);
static void          coerce                (GwyContainer *data,
                                            GwyRunType run);
static gboolean      coerce_dialogue       (CoerceArgs *args,
                                            GwyContainer *data,
                                            gint id);
static void          distribution_changed  (GtkToggleButton *toggle,
                                            CoerceControls *controls);
static void          level_type_changed    (GtkComboBox *combo,
                                            CoerceControls *controls);
static void          nlevels_changed       (GtkAdjustment *adj,
                                            CoerceControls *controls);
static void          processing_changed    (GtkToggleButton *toggle,
                                            CoerceControls *controls);
static void          template_changed      (GwyDataChooser *chooser,
                                            CoerceControls *controls);
static void          update_changed        (GtkToggleButton *toggle,
                                            CoerceControls *controls);
static void          update_sensitivity    (CoerceControls *controls);
static gboolean      template_filter       (GwyContainer *data,
                                            gint id,
                                            gpointer user_data);
static void          preview               (CoerceControls *controls);
static GwyDataField* coerce_do             (GwyDataField *dfield,
                                            const CoerceArgs *args);
static void          coerce_do_field       (GwyDataField *dfield,
                                            GwyDataField *result,
                                            const CoerceArgs *args);
static void          coerce_do_field_levels(GwyDataField *dfield,
                                            GwyDataField *result,
                                            const CoerceArgs *args);
static void          coerce_do_rows        (GwyDataField *dfield,
                                            GwyDataField *result,
                                            const CoerceArgs *args);
static void          build_values_levels   (const ValuePos *vpos,
                                            gdouble *z,
                                            guint n,
                                            guint nlevels);
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
    COERCE_LEVELS_EQUIAREA,
    4,
    COERCE_PROCESSING_FIELD,
    TRUE,
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
        || (run == GWY_RUN_INTERACTIVE && coerce_dialogue(&args, data, id))) {
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
coerce_dialogue(CoerceArgs *args, GwyContainer *data, gint id)
{
    static const GwyEnum distributions[] = {
        { N_("distribution|Uniform"),  COERCE_DISTRIBUTION_UNIFORM,  },
        { N_("distribution|Gaussian"), COERCE_DISTRIBUTION_GAUSSIAN, },
        { N_("As another data"),       COERCE_DISTRIBUTION_DATA,     },
        { N_("Discrete levels"),       COERCE_DISTRIBUTION_LEVELS,   },
    };

    static const GwyEnum processings[] = {
        { N_("Entire image"),         COERCE_PROCESSING_FIELD, },
        { N_("By row (identically)"), COERCE_PROCESSING_ROWS,  },
    };

    CoerceControls controls;
    GtkWidget *dialogue, *table, *label, *vbox, *hbox;
    GwyDataField *dfield;
    GwyDataChooser *chooser;
    gint row, response;
    GSList *l;

    dfield = gwy_container_get_object(data, gwy_app_get_data_key_for_id(id));
    controls.args = args;
    controls.dfield = dfield;

    dialogue = gtk_dialog_new_with_buttons(_("Coerce Statistics"), NULL, 0,
                                           NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialogue),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialogue),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialogue),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialogue), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialogue), GWY_HELP_DEFAULT);
    controls.dialogue = dialogue;

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialogue)->vbox), hbox,
                       FALSE, FALSE, 4);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_new_alike(dfield, TRUE);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = create_preview(controls.mydata, 0, PREVIEW_SIZE, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), controls.view, FALSE, FALSE, 0);

    table = gtk_table_new(COERCE_NDISTRIBUTIONS + COERCE_NPROCESSING + 6,
                          4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
    row = 0;

    label = gtk_label_new(_("Coerce value distribution to:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.distribution
        = gwy_radio_buttons_create(distributions, G_N_ELEMENTS(distributions),
                                   G_CALLBACK(distribution_changed), &controls,
                                   args->distribution);
    for (l = controls.distribution; l; l = g_slist_next(l)) {
        GtkWidget *widget = GTK_WIDGET(l->data);
        CoerceDistributionType dist = gwy_radio_button_get_value(widget);

        gtk_table_attach(GTK_TABLE(table), widget, 0, 3, row, row + 1,
                         GTK_EXPAND | GTK_FILL, 0, 0, 0);
        row++;

        if (dist == COERCE_DISTRIBUTION_DATA) {
            controls.template = gwy_data_chooser_new_channels();
            chooser = GWY_DATA_CHOOSER(controls.template);
            gwy_data_chooser_set_active_id(chooser, &args->template);
            gwy_data_chooser_set_filter(chooser,
                                        &template_filter, dfield, NULL);
            gwy_data_chooser_set_active_id(chooser, &args->template);
            gwy_data_chooser_get_active_id(chooser, &args->template);
            gwy_table_attach_hscale(table, row, _("_Template:"), NULL,
                                    GTK_OBJECT(controls.template),
                                    GWY_HSCALE_WIDGET);
            g_signal_connect(controls.template, "changed",
                             G_CALLBACK(template_changed), &controls);
            row++;
        }
        else if (dist == COERCE_DISTRIBUTION_LEVELS) {
            controls.level_type
                = gwy_enum_combo_box_newl(G_CALLBACK(level_type_changed),
                                          &controls, args->level_type,
                                          _("distribution|Uniform"),
                                          COERCE_LEVELS_UNIFORM,
                                          _("Same area"),
                                          COERCE_LEVELS_EQUIAREA,
                                          NULL);
            gwy_table_attach_hscale(table, row, _("_Type:"), NULL,
                                    GTK_OBJECT(controls.level_type),
                                    GWY_HSCALE_WIDGET);
            row++;

            controls.nlevels = gtk_adjustment_new(args->nlevels,
                                                  2.0, 16384.0,
                                                  1.0, 100.0, 0.0);
            gwy_table_attach_hscale(table, row, _("Number of _levels:"), NULL,
                                    controls.nlevels, GWY_HSCALE_LOG);
            g_signal_connect(controls.nlevels, "value-changed",
                             G_CALLBACK(nlevels_changed), &controls);
            row++;
        }
    }

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new(_("Data processing:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.processing
        = gwy_radio_buttons_create(processings, G_N_ELEMENTS(processings),
                                   G_CALLBACK(processing_changed), &controls,
                                   args->processing);
    row = gwy_radio_buttons_attach_to_table(controls.processing,
                                            GTK_TABLE(table),
                                            3, row);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect(controls.update, "toggled",
                     G_CALLBACK(update_changed), &controls);
    row++;

    update_sensitivity(&controls);
    gtk_widget_show_all(dialogue);
    if (args->update)
        preview(&controls);

    do {
        response = gtk_dialog_run(GTK_DIALOG(dialogue));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialogue);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_PREVIEW:
            preview(&controls);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialogue);
    g_object_unref(controls.mydata);

    return TRUE;
}

static void
distribution_changed(GtkToggleButton *toggle, CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(toggle))
        return;

    args->distribution = gwy_radio_buttons_get_current(controls->distribution);
    update_sensitivity(controls);
    if (args->update)
        preview(controls);
}

static void
level_type_changed(GtkComboBox *combo, CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    args->level_type = gwy_enum_combo_box_get_active(combo);
    if (args->distribution == COERCE_DISTRIBUTION_LEVELS && args->update)
        preview(controls);
}

static void
nlevels_changed(GtkAdjustment *adj, CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    args->nlevels = gwy_adjustment_get_int(adj);
    if (args->distribution == COERCE_DISTRIBUTION_LEVELS && args->update)
        preview(controls);
}

static void
processing_changed(GtkToggleButton *toggle, CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    if (!gtk_toggle_button_get_active(toggle))
        return;

    args->processing = gwy_radio_buttons_get_current(controls->processing);
    update_sensitivity(controls);
    if (args->update)
        preview(controls);
}

static void
template_changed(GwyDataChooser *chooser, CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    gwy_data_chooser_get_active_id(chooser, &args->template);
    if (args->update)
        preview(controls);
}

static void
update_changed(GtkToggleButton *toggle, CoerceControls *controls)
{
    CoerceArgs *args = controls->args;

    args->update = gtk_toggle_button_get_active(toggle);
    update_sensitivity(controls);
    if (args->update)
        preview(controls);
}

static void
update_sensitivity(CoerceControls *controls)
{
    CoerceArgs *args = controls->args;
    GtkWidget *widget;
    gboolean has_template, is_data, is_levels, do_update;

    has_template
        = !!gwy_data_chooser_get_active(GWY_DATA_CHOOSER(controls->template),
                                        NULL);
    widget = gwy_radio_buttons_find(controls->distribution,
                                    COERCE_DISTRIBUTION_DATA);
    gtk_widget_set_sensitive(widget, has_template);

    is_data = (args->distribution == COERCE_DISTRIBUTION_DATA);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->template), is_data);

    is_levels = (args->distribution == COERCE_DISTRIBUTION_LEVELS);
    gwy_table_hscale_set_sensitive(GTK_OBJECT(controls->level_type), is_levels);
    gwy_table_hscale_set_sensitive(controls->nlevels, is_levels);

    do_update = args->update;
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialogue),
                                      RESPONSE_PREVIEW, !do_update);
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

static void
preview(CoerceControls *controls)
{
    CoerceArgs *args = controls->args;
    GwyDataField *result;

    result = coerce_do(controls->dfield, args);
    gwy_container_set_object_by_name(controls->mydata, "/0/data", result);
    g_object_unref(result);
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
    const gdouble *d = gwy_data_field_get_data_const(dfield);
    ValuePos *vpos;
    gdouble *z, *dr;
    guint k;

    if (args->distribution == COERCE_DISTRIBUTION_LEVELS
        && args->level_type == COERCE_LEVELS_UNIFORM) {
        coerce_do_field_levels(dfield, result, args);
        return;
    }

    vpos = g_new(ValuePos, n);
    z = g_new(gdouble, n);
    for (k = 0; k < n; k++) {
        vpos[k].z = d[k];
        vpos[k].k = k;
    }
    qsort(vpos, n, sizeof(ValuePos), compare_double);

    if (args->distribution == COERCE_DISTRIBUTION_DATA) {
        GQuark quark = gwy_app_get_data_key_for_id(args->template.id);
        GwyContainer *data = gwy_app_data_browser_get(args->template.datano);
        GwyDataField *src = gwy_container_get_object(data, quark);
        guint nsrc = gwy_data_field_get_xres(src)*gwy_data_field_get_yres(src);
        build_values_from_data(z, n,
                               gwy_data_field_get_data_const(src), nsrc);
    }
    else if (args->distribution == COERCE_DISTRIBUTION_LEVELS) {
        build_values_levels(vpos, z, n, args->nlevels);
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
    for (k = 0; k < n; k++)
        dr[vpos[k].k] = z[k];

    g_free(z);
    g_free(vpos);
}

static void
coerce_do_field_levels(GwyDataField *dfield, GwyDataField *result,
                       const CoerceArgs *args)
{
    guint n = gwy_data_field_get_xres(dfield)*gwy_data_field_get_yres(dfield);
    const gdouble *d = gwy_data_field_get_data_const(dfield);
    gdouble *dr = gwy_data_field_get_data(result);
    gdouble min, max, q;
    guint k, v, nlevels = args->nlevels;

    gwy_data_field_get_min_max(dfield, &min, &max);
    if (max <= min) {
        gwy_data_field_fill(result, 0.5*(min + max));
        return;
    }

    q = (max - min)/nlevels;
    for (k = 0; k < n; k++) {
        v = (guint)ceil((d[k] - min)/q);
        v = MIN(v, nlevels-1);
        dr[k] = (v + 0.5)*q + min;
    }
}

static void
coerce_do_rows(GwyDataField *dfield, GwyDataField *result,
               const CoerceArgs *args)
{
    guint xres = gwy_data_field_get_xres(dfield),
          yres = gwy_data_field_get_yres(dfield);
    const gdouble *d = gwy_data_field_get_data_const(dfield);
    ValuePos *vpos;
    gdouble *z, *dr;
    guint i, j;

    /* It is not completely clear what we should do in the case of row-wise
     * processing but this at least ensures that the levels are the same in the
     * entire field but individual rows are transformed individualy. */
    if (args->distribution == COERCE_DISTRIBUTION_LEVELS) {
        GwyDataField *tmp = gwy_data_field_duplicate(dfield);
        gdouble min, max;
        gwy_data_field_get_min_max(dfield, &min, &max);
        for (i = 0; i < yres; i++)
            gwy_data_field_area_renormalize(tmp, 0, i, xres, 1, max-min, min);
        coerce_do_field(tmp, result, args);
        g_object_unref(tmp);
        return;
    }

    vpos = g_new(ValuePos, xres);
    z = g_new(gdouble, xres);

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
build_values_levels(const ValuePos *vpos, gdouble *z, guint n, guint nlevels)
{
    guint i, j, blockstart, counter;
    gdouble v;

    if (nlevels >= n) {
        for (i = 0; i < n; i++)
            z[i] = vpos[i].z;
        return;
    }

    blockstart = 0;
    counter = nlevels/2;
    for (i = 0; i < n; i++) {
        counter += nlevels;
        if (counter >= n) {
            v = 0.0;
            for (j = blockstart; j <= i; j++)
                v += vpos[j].z;

            v /= (i+1 - blockstart);
            for (j = blockstart; j <= i; j++)
                z[j] = v;

            counter -= n;
            blockstart = i+1;
        }
    }
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

static gdouble
gwy_inverf(gdouble y)
{
    /* Coefficients in rational approximations. */
    static const gdouble a[4] = {
        0.886226899, -1.645349621, 0.914624893, -0.140543331
    };
    static const gdouble b[4] = {
        -2.118377725, 1.442710462, -0.329097515, 0.012229801
    };
    static const gdouble c[4] = {
        -1.970840454, -1.624906493, 3.429567803, 1.641345311
    };
    static const gdouble d[2] = {
        3.543889200, 1.637067800
    };
    const gdouble y0 = 0.7;

    gdouble x, z;

    if (y <= -1.0)
       return -G_MAXDOUBLE;
    if (y >= 1.0)
       return G_MAXDOUBLE;

    if (y < -y0) {
        z = sqrt(-log(0.5*(1.0 + y)));
        x = -(((c[3]*z + c[2])*z + c[1])*z + c[0])/((d[1]*z + d[0])*z + 1.0);
    }
    else if (y > y0) {
        z = sqrt(-log(0.5*(1.0 - y)));
        x = (((c[3]*z + c[2])*z + c[1])*z + c[0])/((d[1]*z + d[0])*z + 1.0);
    }
    else {
        z = y*y;
        x = y*(((a[3]*z + a[2])*z + a[1])*z + a[0])
              /((((b[3]*z + b[3])*z + b[1])*z + b[0])*z + 1.0);
    }

    /* Three steps of Newton method correction to full accuracy. */
    x -= (erf(x) - y)/(2.0/GWY_SQRT_PI*exp(-x*x));
    x -= (erf(x) - y)/(2.0/GWY_SQRT_PI*exp(-x*x));
    x -= (erf(x) - y)/(2.0/GWY_SQRT_PI*exp(-x*x));

    return x;
}

static void
build_values_gaussian(gdouble *z, guint n, gdouble mean, gdouble rms)
{
    gdouble x;
    guint i;

    rms *= sqrt(2.0);
    for (i = 0; i < n; i++) {
        x = (2.0*i + 1.0)/n - 1.0;
        z[i] = mean + rms*gwy_inverf(x);
    }
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
static const gchar level_type_key[]   = "/module/coerce/level_type";
static const gchar nlevels_key[]      = "/module/coerce/nlevels";
static const gchar processing_key[]   = "/module/coerce/processing";

static void
sanitize_args(CoerceArgs *args)
{
    args->distribution = MIN(args->distribution, COERCE_NDISTRIBUTIONS-1);
    args->processing = MIN(args->processing, COERCE_NPROCESSING-1);
    args->level_type = MIN(args->level_type, COERCE_NLEVELTYPES-1);
    args->nlevels = CLAMP(args->nlevels, 2, 16384);
    if (args->distribution == COERCE_DISTRIBUTION_DATA
        && !gwy_app_data_id_verify_channel(&args->template))
        args->distribution = coerce_defaults.distribution;
}

static void
load_args(GwyContainer *container,
          CoerceArgs *args)
{
    *args = coerce_defaults;

    gwy_container_gis_enum_by_name(container, distribution_key,
                                   &args->distribution);
    gwy_container_gis_enum_by_name(container, level_type_key,
                                   &args->level_type);
    gwy_container_gis_int32_by_name(container, nlevels_key, &args->nlevels);
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
    gwy_container_set_enum_by_name(container, level_type_key,
                                   args->level_type);
    gwy_container_set_int32_by_name(container, nlevels_key, args->nlevels);
    gwy_container_set_enum_by_name(container, processing_key,
                                   args->processing);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
