/*
 *  @(#) $Id$
 *  Copyright (C) 2010 David Necas (Yeti)
 *  E-mail: yeti@gwyddion.net
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define THRESHOLD_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

typedef enum {
    THRESHOLD_RANGE_USER,
    THRESHOLD_RANGE_DISPLAY,
    THRESHOLD_RANGE_OUTLIERS,
    THRESHOLD_RANGE_NMODES
} ThresholdRangeMode;

typedef struct {
    ThresholdRangeMode mode;
    gdouble lower;
    gdouble upper;
    gdouble sigma;
} ThresholdArgs;

typedef struct {
    gdouble min, max;
    gdouble disp_min, disp_max;
    gdouble avg, rms;
} ThresholdRanges;

typedef struct {
    ThresholdArgs *args;
    const ThresholdRanges *ranges;
    GtkWidget *dialog;
    GtkWidget *view;
    GSList *mode;
    GtkWidget *lower;
    GtkWidget *upper;
    GtkObject *sigma;
    GwyContainer *mydata;
    GwyDataField *dfield;
    GwySIValueFormat *format;
    gdouble q;
    gboolean in_update;
} ThresholdControls;

static gboolean module_register            (void);
static void     threshold                  (GwyContainer *data,
                                            GwyRunType run);
static void     run_noninteractive         (const ThresholdArgs *args,
                                            const ThresholdRanges *ranges,
                                            GwyContainer *data,
                                            GwyDataField *dfield,
                                            GQuark quark);
static void     threshold_dialog           (ThresholdArgs *args,
                                            const ThresholdRanges *ranges,
                                            GwyContainer *data,
                                            GwyDataField *dfield,
                                            gint id,
                                            GQuark quark);
static void     threshold_get_display_range(GwyContainer *container,
                                            gint id,
                                            GwyDataField *data_field,
                                            gdouble *disp_min,
                                            gdouble *disp_max);
static void     threshold_sanitize_min_max (ThresholdArgs *args,
                                            const ThresholdRanges *ranges);
static void     threshold_mode_changed     (GtkWidget *button,
                                            ThresholdControls *controls);
static void     threshold_set_to_full_range(ThresholdControls *controls);
static void     threshold_lower_changed    (ThresholdControls *controls);
static void     threshold_upper_changed    (ThresholdControls *controls);
static void     sigma_changed              (ThresholdControls *controls);
static void     preview                    (ThresholdControls *controls);
static void     threshold_do               (const ThresholdArgs *args,
                                            const ThresholdRanges *ranges,
                                            GwyDataField *dfield);
static void     threshold_load_args        (GwyContainer *settings,
                                            ThresholdArgs *args);
static void     threshold_save_args        (GwyContainer *settings,
                                            ThresholdArgs *args);
static void     threshold_sanitize_args    (ThresholdArgs *args);


static const ThresholdArgs threshold_defaults = {
    THRESHOLD_RANGE_USER,
    0.0, 0.0, 3.0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Limit the data range using a lower/upper threshold."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("threshold",
                              (GwyProcessFunc)&threshold,
                              N_("/_Basic Operations/Li_mit range..."),
                              NULL,
                              THRESHOLD_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Limit data range"));

    return TRUE;
}

static void
threshold(GwyContainer *data, GwyRunType run)
{
    ThresholdArgs args;
    ThresholdRanges ranges;
    GwyDataField *dfield;
    GQuark quark;
    gint id;

    g_return_if_fail(run & THRESHOLD_RUN_MODES);
    threshold_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);
    g_return_if_fail(dfield);

    gwy_data_field_get_min_max(dfield, &ranges.min, &ranges.max);
    ranges.avg = gwy_data_field_get_avg(dfield);
    ranges.rms = gwy_data_field_get_rms(dfield);
    threshold_get_display_range(data, id, dfield,
                                &ranges.disp_min, &ranges.disp_max);

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, &ranges, data, dfield, quark);
    else {
        threshold_sanitize_min_max(&args, &ranges);
        threshold_dialog(&args, &ranges, data, dfield, id, quark);
        threshold_save_args(gwy_app_settings_get(), &args);
    }
}

static void
run_noninteractive(const ThresholdArgs *args,
                   const ThresholdRanges *ranges,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GQuark quark)
{
    gwy_app_undo_qcheckpointv(data, 1, &quark);
    threshold_do(args, ranges, dfield);
}

static void
threshold_format_value(ThresholdControls *controls,
                       GtkEntry *entry,
                       gdouble value)
{
    gchar *s;

    s = g_strdup_printf("%.*f",
                        controls->format->precision+1,
                        value/controls->format->magnitude);
    gtk_entry_set_text(GTK_ENTRY(entry), s);
    g_free(s);
}

static GtkWidget*
threshold_entry_attach(ThresholdControls *controls,
                       GtkTable *table,
                       gint row,
                       gdouble value,
                       const gchar *name)
{
    GtkWidget *label, *entry;

    label = gtk_label_new_with_mnemonic(name);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, 0, 0, 0);
    entry = gtk_entry_new();
    gwy_widget_set_activate_on_unfocus(entry, TRUE);
    threshold_format_value(controls, GTK_ENTRY(entry), value);
    gtk_table_attach(table, entry, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);
    label = gtk_label_new(controls->format->units);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 3, 4, row, row+1, GTK_FILL, 0, 0, 0);

    return entry;
}

static void
threshold_dialog(ThresholdArgs *args,
                 const ThresholdRanges *ranges,
                 GwyContainer *data,
                 GwyDataField *dfield,
                 gint id,
                 GQuark quark)
{
    GtkWidget *dialog, *hbox, *button, *label;
    GtkTable *table;
    ThresholdControls controls;
    gint response;
    GwyPixmapLayer *layer;
    gchar *s;
    gint row;

    controls.args = args;
    controls.ranges = ranges;
    controls.dfield = dfield;
    controls.in_update = FALSE;

    controls.format = gwy_data_field_get_value_format_z
                                   (dfield, GWY_SI_UNIT_FORMAT_VFMARKUP, NULL);

    dialog = gtk_dialog_new_with_buttons(_("Limit Range"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    dfield = gwy_data_field_duplicate(controls.dfield);
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    g_object_set(layer,
                 "data-key", "/0/data",
                 "gradient-key", "/0/base/palette",
                 "range-type-key", "/0/base/range-type",
                 "min-max-key", "/0/base",
                 NULL);
    gwy_data_view_set_data_prefix(GWY_DATA_VIEW(controls.view), "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    gwy_set_data_preview_size(GWY_DATA_VIEW(controls.view), PREVIEW_SIZE);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = GTK_TABLE(gtk_table_new(5, 4, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), TRUE, TRUE, 4);
    row = 0;

    controls.mode = gwy_radio_buttons_createl(G_CALLBACK(threshold_mode_changed),
                                              &controls, args->mode,
                                              _("Specify _thresholds"),
                                              THRESHOLD_RANGE_USER,
                                              _("Use _display range"),
                                              THRESHOLD_RANGE_DISPLAY,
                                              _("Cut off outlier_s"),
                                              THRESHOLD_RANGE_OUTLIERS,
                                              NULL);

    button = gwy_radio_buttons_find(controls.mode, THRESHOLD_RANGE_USER);
    gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.lower = threshold_entry_attach(&controls, table, row, args->lower,
                                            _("_Lower:"));
    g_signal_connect_swapped(controls.lower, "activate",
                             G_CALLBACK(threshold_lower_changed), &controls);
    row++;

    controls.upper = threshold_entry_attach(&controls, table, row, args->upper,
                                            _("_Upper:"));
    g_signal_connect_swapped(controls.upper, "activate",
                             G_CALLBACK(threshold_upper_changed), &controls);
    row++;

    button = gtk_button_new_with_mnemonic(_("Set to _Full Range"));
    gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(threshold_set_to_full_range),
                             &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    button = gwy_radio_buttons_find(controls.mode, THRESHOLD_RANGE_DISPLAY);
    gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    s = g_strdup_printf("%.*f to %.*f",
                        controls.format->precision+1,
                        controls.ranges->disp_min/controls.format->magnitude,
                        controls.format->precision+1,
                        controls.ranges->disp_max/controls.format->magnitude);
    label = gtk_label_new(s);
    g_free(s);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(table, label, 1, 3, row, row+1, GTK_FILL, 0, 0, 0);

    label = gtk_label_new(controls.format->units);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 3, 4, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    button = gwy_radio_buttons_find(controls.mode, THRESHOLD_RANGE_OUTLIERS);
    gtk_table_attach(table, button, 0, 3, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    controls.sigma = gtk_adjustment_new(args->sigma, 1.0, 12.0, 0.01, 1.0, 0);
    gwy_table_attach_hscale(GTK_WIDGET(table), row,
                            _("F_arther than:"), _("RMS"),
                            controls.sigma, GWY_HSCALE_SQRT);
    g_signal_connect_swapped(controls.sigma, "value-changed",
                             G_CALLBACK(sigma_changed), &controls);

    preview(&controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            gwy_si_unit_value_format_free(controls.format);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);
    g_object_unref(controls.mydata);
    gwy_si_unit_value_format_free(controls.format);
    run_noninteractive(args, ranges, data, controls.dfield, quark);
}

static void
threshold_get_display_range(GwyContainer *container,
                            gint id,
                            GwyDataField *data_field,
                            gdouble *disp_min,
                            gdouble *disp_max)
{
    GwyLayerBasicRangeType range_type = GWY_LAYER_BASIC_RANGE_FULL;
    gchar key[64];

    g_snprintf(key, sizeof(key), "/%d/base/range-type", id);
    gwy_container_gis_enum_by_name(container, key, &range_type);

    switch (range_type) {
        case GWY_LAYER_BASIC_RANGE_FULL:
        case GWY_LAYER_BASIC_RANGE_ADAPT:
        gwy_data_field_get_min_max(data_field, disp_min, disp_max);
        break;

        case GWY_LAYER_BASIC_RANGE_FIXED:
        gwy_data_field_get_min_max(data_field, disp_min, disp_max);
        g_snprintf(key, sizeof(key), "/%d/base/min", id);
        gwy_container_gis_double_by_name(container, key, disp_min);
        g_snprintf(key, sizeof(key), "/%d/base/max", id);
        gwy_container_gis_double_by_name(container, key, disp_max);
        break;

        case GWY_LAYER_BASIC_RANGE_AUTO:
        gwy_data_field_get_autorange(data_field, disp_min, disp_max);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
threshold_sanitize_min_max(ThresholdArgs *args,
                           const ThresholdRanges *ranges)
{
    gdouble delta, range;

    if (args->lower > args->upper) {
        GWY_SWAP(gdouble, args->lower, args->upper);
    }
    delta = args->upper - args->lower;
    range = ranges->max - ranges->min;
    if (args->lower > ranges->max || args->upper < ranges->min
        || 100*delta < range) {
        args->lower = ranges->min;
        args->upper = ranges->max;
    }
    else {
        if (args->upper > ranges->max + 10*delta)
            args->upper = ranges->max;
        if (args->lower < ranges->min - 10*delta)
            args->upper = ranges->min;
    }
}

static void
threshold_mode_changed(G_GNUC_UNUSED GtkWidget *button,
                       ThresholdControls *controls)
{
    controls->args->mode = gwy_radio_buttons_get_current(controls->mode);
    preview(controls);
}

static void
threshold_set_to_range(ThresholdControls *controls,
                       gdouble lower, gdouble upper)
{
    controls->in_update = TRUE;
    threshold_format_value(controls, GTK_ENTRY(controls->lower), lower);
    gtk_widget_activate(controls->lower);
    threshold_format_value(controls, GTK_ENTRY(controls->upper), upper);
    gtk_widget_activate(controls->upper);
    controls->in_update = FALSE;
    preview(controls);
}

static void
threshold_set_to_full_range(ThresholdControls *controls)
{
    threshold_set_to_range(controls,
                           controls->ranges->min, controls->ranges->max);
}

static void
threshold_lower_changed(ThresholdControls *controls)
{
    const gchar *value = gtk_entry_get_text(GTK_ENTRY(controls->lower));
    controls->args->lower = g_strtod(value, NULL) * controls->format->magnitude;
    preview(controls);
}

static void
threshold_upper_changed(ThresholdControls *controls)
{
    const gchar *value = gtk_entry_get_text(GTK_ENTRY(controls->upper));
    controls->args->upper = g_strtod(value, NULL) * controls->format->magnitude;
    preview(controls);
}

static void
sigma_changed(ThresholdControls *controls)
{
    ThresholdArgs *args = controls->args;
    args->sigma = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->sigma));
    preview(controls);
}

static void
preview(ThresholdControls *controls)
{
    GwyDataField *dfield;

    if (controls->in_update)
        return;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));
    gwy_data_field_copy(controls->dfield, dfield, FALSE);
    threshold_do(controls->args, controls->ranges, dfield);
}

static void
threshold_do(const ThresholdArgs *args,
             const ThresholdRanges *ranges,
             GwyDataField *dfield)
{
    gdouble lower, upper;

    switch (args->mode) {
        case THRESHOLD_RANGE_USER:
        lower = MIN(args->lower, args->upper);
        upper = MAX(args->lower, args->upper);
        break;

        case THRESHOLD_RANGE_DISPLAY:
        lower = MIN(ranges->disp_min, ranges->disp_max);
        upper = MAX(ranges->disp_min, ranges->disp_max);
        break;

        case THRESHOLD_RANGE_OUTLIERS:
        lower = ranges->avg - args->sigma*ranges->rms;
        upper = ranges->avg + args->sigma*ranges->rms;
        break;

        default:
        g_return_if_reached();
        break;
    }
    gwy_data_field_clamp(dfield, lower, upper);
    gwy_data_field_data_changed(dfield);
}

static const gchar mode_key[]  = "/module/threshold/mode";
static const gchar lower_key[] = "/module/threshold/lower";
static const gchar upper_key[] = "/module/threshold/upper";
static const gchar sigma_key[] = "/module/threshold/sigma";

static void
threshold_sanitize_args(ThresholdArgs *args)
{
    args->mode = MIN(args->mode, THRESHOLD_RANGE_NMODES-1);
    /* lower and upper are sanitized when we see the data field */
    args->sigma = CLAMP(args->sigma, 1.0, 12.0);
}

static void
threshold_load_args(GwyContainer *settings,
                    ThresholdArgs *args)
{
    *args = threshold_defaults;

    gwy_container_gis_enum_by_name(settings, mode_key, &args->mode);
    gwy_container_gis_double_by_name(settings, lower_key, &args->lower);
    gwy_container_gis_double_by_name(settings, upper_key, &args->upper);
    gwy_container_gis_double_by_name(settings, sigma_key, &args->sigma);
    threshold_sanitize_args(args);
}

static void
threshold_save_args(GwyContainer *settings,
                    ThresholdArgs *args)
{
    gwy_container_set_enum_by_name(settings, mode_key, args->mode);
    gwy_container_set_double_by_name(settings, lower_key, args->lower);
    gwy_container_set_double_by_name(settings, upper_key, args->upper);
    gwy_container_set_double_by_name(settings, sigma_key, args->sigma);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
