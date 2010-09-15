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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

#include "config.h"
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/filters.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define THRESHOLD_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

typedef struct {
    gdouble lower;
    gdouble upper;
    /* interface only */
    gdouble min, max;
} ThresholdArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GtkWidget *lower;
    GtkWidget *upper;
    GwyContainer *mydata;
    GwyDataField *dfield;
    ThresholdArgs *args;
    GwySIValueFormat *format;
    gdouble q;
    gdouble disp_min, disp_max;
    gboolean in_update;
} ThresholdControls;

static gboolean module_register                 (void);
static void     threshold                       (GwyContainer *data,
                                                 GwyRunType run);
static void     run_noninteractive              (ThresholdArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GQuark quark);
static void     threshold_dialog                (ThresholdArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id,
                                                 GQuark quark);
static void     threshold_set_to_full_range     (ThresholdControls *controls);
static void     threshold_set_to_display_range  (ThresholdControls *controls);
static void     threshold_lower_changed         (ThresholdControls *controls);
static void     threshold_upper_changed         (ThresholdControls *controls);
static void     preview                         (ThresholdControls *controls,
                                                 ThresholdArgs *args);
static void     threshold_load_args             (GwyContainer *settings,
                                                 ThresholdArgs *args);
static void     threshold_save_args             (GwyContainer *settings,
                                                 ThresholdArgs *args);
static void     threshold_sanitize_args         (ThresholdArgs *args);


static const ThresholdArgs threshold_defaults = {
    0.0, 0.0,
    0.0, 0.0,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Limit the data range using a lower/upper threshold."),
    "David Nečas <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("threshold",
                              (GwyProcessFunc)&threshold,
                              N_("/_Basic Operations/Threshol_d..."),
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
    GwyDataField *dfield;
    gdouble delta, range;
    GQuark quark;
    gint id;

    g_return_if_fail(run & THRESHOLD_RUN_MODES);
    threshold_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_DATA_FIELD_KEY, &quark,
                                     0);
    g_return_if_fail(dfield);

    gwy_data_field_get_min_max(dfield, &args.min, &args.max);
    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data, dfield, quark);
    else {
        /* Sanitize args */
        if (args.lower > args.upper) {
            GWY_SWAP(gdouble, args.lower, args.upper);
        }
        delta = args.upper - args.lower;
        range = args.max - args.min;
        if (args.lower > args.max || args.upper < args.min
            || 100*delta < range) {
            args.lower = args.min;
            args.upper = args.max;
        }
        else {
            if (args.upper > args.max + 10*delta)
                args.upper = args.max;
            if (args.lower < args.min - 10*delta)
                args.upper = args.min;
        }
        threshold_dialog(&args, data, dfield, id, quark);
        threshold_save_args(gwy_app_settings_get(), &args);
    }
}

static void
run_noninteractive(ThresholdArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GQuark quark)
{
    gwy_app_undo_qcheckpointv(data, 1, &quark);
    gwy_data_field_clamp(dfield,
                         MIN(args->lower, args->upper),
                         MAX(args->lower, args->upper));
    gwy_data_field_data_changed(dfield);
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
    gtk_table_attach(table, entry, 1, 2, row, row+1, GTK_FILL, 0, 0, 0);
    label = gtk_label_new(controls->format->units);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(table, label, 2, 3, row, row+1, GTK_FILL, 0, 0, 0);

    return entry;
}

static void
threshold_dialog(ThresholdArgs *args,
                 GwyContainer *data,
                 GwyDataField *dfield,
                 gint id,
                 GQuark quark)
{
    GtkWidget *dialog, *hbox, *button;
    GwyDataView *dataview;
    GtkTable *table;
    ThresholdControls controls;
    gint response;
    GwyPixmapLayer *layer;
    gint row;

    controls.args = args;
    controls.dfield = dfield;
    controls.in_update = FALSE;
    gwy_app_data_browser_get_current(GWY_APP_DATA_VIEW, &dataview,
                                     0);
    layer = gwy_data_view_get_base_layer(dataview);
    gwy_layer_basic_get_range(GWY_LAYER_BASIC(layer),
                              &controls.disp_min, &controls.disp_max);

    controls.format = gwy_data_field_get_value_format_z
                                   (dfield, GWY_SI_UNIT_FORMAT_VFMARKUP, NULL);

    dialog = gtk_dialog_new_with_buttons(_("Threshold"), NULL, 0,
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

    table = GTK_TABLE(gtk_table_new(5, 3, FALSE));
    gtk_table_set_row_spacings(table, 2);
    gtk_table_set_col_spacings(table, 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(table), TRUE, TRUE, 4);
    row = 0;

    gtk_table_attach(table, gwy_label_new_header(_("Thresholds")),
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
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
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    button = gtk_button_new_with_mnemonic(_("Set to Full Range"));
    gtk_table_attach(table, button, 0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(threshold_set_to_full_range),
                             &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    button = gtk_button_new_with_mnemonic(_("Set to Display Range"));
    gtk_table_attach(table, button, 0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(threshold_set_to_display_range),
                             &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    preview(&controls, args);

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
    run_noninteractive(args, data, controls.dfield, quark);
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
    preview(controls, controls->args);
}

static void
threshold_set_to_full_range(ThresholdControls *controls)
{
    threshold_set_to_range(controls, controls->args->min, controls->args->max);
}

static void
threshold_set_to_display_range(ThresholdControls *controls)
{
    threshold_set_to_range(controls, controls->disp_min, controls->disp_max);
}

static void
threshold_lower_changed(ThresholdControls *controls)
{
    const gchar *value = gtk_entry_get_text(GTK_ENTRY(controls->lower));
    controls->args->lower = g_strtod(value, NULL) * controls->format->magnitude;
    preview(controls, controls->args);
}

static void
threshold_upper_changed(ThresholdControls *controls)
{
    const gchar *value = gtk_entry_get_text(GTK_ENTRY(controls->upper));
    controls->args->upper = g_strtod(value, NULL) * controls->format->magnitude;
    preview(controls, controls->args);
}

static void
preview(ThresholdControls *controls,
        ThresholdArgs *args)
{
    GwyDataField *dfield;

    if (controls->in_update)
        return;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    gwy_data_field_copy(controls->dfield, dfield, FALSE);
    gwy_data_field_clamp(dfield, args->lower, args->upper);
    gwy_data_field_data_changed(dfield);
}

static const gchar lower_key[]  = "/module/threshold/lower";
static const gchar upper_key[]  = "/module/threshold/upper";

static void
threshold_sanitize_args(G_GNUC_UNUSED ThresholdArgs *args)
{
    /* lower and upper are sanitized when we see the data field */
}

static void
threshold_load_args(GwyContainer *settings,
                    ThresholdArgs *args)
{
    *args = threshold_defaults;

    gwy_container_gis_double_by_name(settings, lower_key, &args->lower);
    gwy_container_gis_double_by_name(settings, upper_key, &args->upper);
    threshold_sanitize_args(args);
}

static void
threshold_save_args(GwyContainer *settings,
                    ThresholdArgs *args)
{
    gwy_container_set_double_by_name(settings, lower_key, args->lower);
    gwy_container_set_double_by_name(settings, upper_key, args->upper);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
