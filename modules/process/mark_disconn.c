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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/arithmetic.h>
#include <libprocess/elliptic.h>
#include <libprocess/filters.h>
#include <libprocess/grains.h>
#include <libprocess/stats.h>
#include <libprocess/linestats.h>
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>
#include "preview.h"

#define DISCONN_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    RESPONSE_PREVIEW = 2,
};

typedef enum {
    FEATURES_POSITIVE = 1 << 0,
    FEATURES_NEGATIVE = 1 << 2,
    FEATURES_BOTH     = (FEATURES_POSITIVE | FEATURES_NEGATIVE),
} GwyFeaturesType;

typedef struct {
    GwyFeaturesType type;
    gint size;
    gdouble threshold;
    gboolean combine;
    GwyMergeType combine_type;
} DisconnArgs;

typedef struct {
    DisconnArgs *args;
    GtkWidget *dialog;
    GSList *type;
    GtkObject *size;
    GtkObject *threshold;
    GwyContainer *mydata;
    GtkWidget *view;
    GtkWidget *color_button;
    GtkWidget *combine;
    GtkWidget *combine_type;
} DisconnControls;

static gboolean module_register      (void);
static void     mark_disconn         (GwyContainer *data,
                                      GwyRunType run);
static gboolean disconn_dialog       (DisconnArgs *args,
                                      GwyContainer *data,
                                      gint id);
static void     disconn_dialog_update(DisconnControls *controls,
                                      const DisconnArgs *args);
static void     preview              (DisconnControls *controls);
static gboolean disconn_do           (GwyDataField *dfield,
                                      GwyDataField *mask,
                                      const DisconnArgs *args);
static void     type_changed         (GtkToggleButton *toggle,
                                      DisconnControls *controls);
static void     size_changed         (GtkAdjustment *adj,
                                      DisconnControls *controls);
static void     threshold_changed    (GtkAdjustment *adj,
                                      DisconnControls *controls);
static void     combine_changed      (GtkToggleButton *toggle,
                                      DisconnControls *controls);
static void     combine_type_changed (GtkComboBox *combo,
                                      DisconnControls *controls);
static void     disconn_load_args    (GwyContainer *container,
                                      DisconnArgs *args);
static void     disconn_save_args    (GwyContainer *container,
                                      DisconnArgs *args);

static const DisconnArgs disconn_defaults = {
    FEATURES_BOTH, 5,
    0.1,
    FALSE, GWY_MERGE_UNION,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask of values disconnected to the rest."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("mark_disconn",
                              (GwyProcessFunc)&mark_disconn,
                              N_("/_Correct Data/Mask of _Disconnected..."),
                              NULL,
                              DISCONN_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark data disconnected from other values"));

    return TRUE;
}

static void
mark_disconn(GwyContainer *data,
             GwyRunType run)
{
    DisconnArgs args;
    GwyDataField *dfield, *maskfield = NULL, *existing_mask;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & DISCONN_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_MASK_FIELD, &existing_mask,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && mquark);

    disconn_load_args(gwy_app_settings_get(), &args);
    if (run == GWY_RUN_IMMEDIATE || disconn_dialog(&args, data, id)) {
        maskfield = create_mask_field(dfield);
        gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                           _("Initializing..."));
        if (!disconn_do(dfield, maskfield, &args))
            gwy_object_unref(maskfield);
        disconn_save_args(gwy_app_settings_get(), &args);
    }

    if (!maskfield)
        return;

    gwy_app_undo_qcheckpointv(data, 1, &mquark);

    if (!existing_mask && gwy_data_field_get_max(maskfield) <= 0.0) {
        g_object_unref(maskfield);
        return;
    }

    if (existing_mask && args.combine) {
        if (args.combine_type == GWY_MERGE_UNION)
            gwy_data_field_grains_add(existing_mask, maskfield);
        else if (args.combine_type == GWY_MERGE_INTERSECTION)
            gwy_data_field_grains_intersect(existing_mask, maskfield);
        gwy_data_field_data_changed(existing_mask);
    }
    else if (existing_mask) {
        gwy_data_field_copy(maskfield, existing_mask, FALSE);
        gwy_data_field_data_changed(existing_mask);
    }
    else {
        gwy_container_set_object(data, mquark, maskfield);
    }
    g_object_unref(maskfield);

    gwy_app_channel_log_add_proc(data, id, id);
}

static gboolean
disconn_dialog(DisconnArgs *args, GwyContainer *data, gint id)
{
    enum { RESPONSE_RESET = 1 };

    static const GwyEnum types[] = {
        { N_("Positive"), FEATURES_POSITIVE, },
        { N_("Negative"), FEATURES_NEGATIVE, },
        { N_("Both"),     FEATURES_BOTH,     },
    };

    GtkWidget *dialog, *table, *label, *hbox, *spin;
    GwyDataField *dfield, *existing_mask = NULL;
    DisconnControls controls;
    GSList *group;
    gint response, row;

    gwy_clear(&controls, 1);
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Mark Disconnected"), NULL, 0,
                                         NULL);
    controls.dialog = dialog;
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    dfield = gwy_container_get_object(data, gwy_app_get_data_key_for_id(id));
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
    dfield = create_mask_field(dfield);
    gwy_container_set_object_by_name(controls.mydata, "/0/mask", dfield);
    g_object_unref(dfield);
    if (gwy_container_gis_object(data, gwy_app_get_mask_key_for_id(id),
                                 (GObject**)&existing_mask)) {
        gwy_container_set_object_by_name(controls.mydata, "/1/mask",
                                         existing_mask);
    }

    gwy_app_sync_data_items(data, controls.mydata, id, 0, FALSE,
                            GWY_DATA_ITEM_PALETTE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            GWY_DATA_ITEM_RANGE,
                            GWY_DATA_ITEM_REAL_SQUARE,
                            0);
    controls.view = create_preview(controls.mydata, 0, PREVIEW_SIZE, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(8 + 2*(!!existing_mask), 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(_("Defect type:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    group = gwy_radio_buttons_create(types, G_N_ELEMENTS(types),
                                     G_CALLBACK(type_changed), &controls,
                                     args->type);
    controls.type = group;
    row = gwy_radio_buttons_attach_to_table(group, GTK_TABLE(table), 3, row);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.size = gtk_adjustment_new(args->size, 1, 256, 1, 10, 0);
    gwy_table_attach_hscale(table, row, _("Defect _radius:"), "px",
                            controls.size, GWY_HSCALE_SQRT);
    g_signal_connect(controls.size, "value-changed",
                     G_CALLBACK(size_changed), &controls);
    row++;

    controls.threshold = gtk_adjustment_new(args->threshold,
                                            0.0, 1.0, 0.001, 0.1, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Threshold:"), NULL,
                                   controls.threshold, GWY_HSCALE_SQRT);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 4);
    g_signal_connect(controls.threshold, "value-changed",
                     G_CALLBACK(threshold_changed), &controls);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    label = gwy_label_new_header(_("Options"));
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    if (existing_mask) {
        controls.combine
            = gtk_check_button_new_with_mnemonic(_("Com_bine with "
                                                   "existing mask"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.combine),
                                     args->combine);
        gtk_table_attach(GTK_TABLE(table), controls.combine,
                         0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
        g_signal_connect(controls.combine, "toggled",
                         G_CALLBACK(combine_changed), &controls);
        row++;

        controls.combine_type
            = gwy_enum_combo_box_new(gwy_merge_type_get_enum(), -1,
                                     G_CALLBACK(combine_type_changed),
                                     &controls,
                                     args->combine_type, TRUE);
        gwy_table_attach_hscale(table, row, _("Operation:"), NULL,
                                GTK_OBJECT(controls.combine_type),
                                GWY_HSCALE_WIDGET);
        gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
        row++;
    }

    controls.color_button = create_mask_color_button(controls.mydata, dialog,
                                                     0);
    gwy_table_attach_hscale(table, row, _("_Mask color:"), NULL,
                            GTK_OBJECT(controls.color_button),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    row++;

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

            case RESPONSE_PREVIEW:
            preview(&controls);
            break;

            case RESPONSE_RESET:
            disconn_dialog_update(&controls, &disconn_defaults);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
disconn_dialog_update(DisconnControls *controls,
                      const DisconnArgs *args)
{
    gwy_radio_buttons_set_current(controls->type, args->type);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold),
                             args->threshold);
    if (controls->combine) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->combine),
                                     args->combine);
        gwy_enum_combo_box_set_active(GTK_COMBO_BOX(controls->combine_type),
                                      args->combine_type);
    }
}

static void
preview(DisconnControls *controls)
{
    DisconnArgs *args = controls->args;
    GwyDataField *dfield, *mask, *existing_mask = NULL;

    gwy_app_wait_start(GTK_WINDOW(controls->dialog), _("Initializing..."));
    dfield = gwy_container_get_object_by_name(controls->mydata, "/0/data");
    mask = gwy_container_get_object_by_name(controls->mydata, "/0/mask");
    if (!disconn_do(dfield, mask, controls->args))
        return;

    if (args->combine
        && gwy_container_gis_object_by_name(controls->mydata, "/1/mask",
                                            (GObject**)&existing_mask)) {
        if (args->combine_type == GWY_MERGE_UNION)
            gwy_data_field_grains_add(mask, existing_mask);
        else if (args->combine_type == GWY_MERGE_INTERSECTION)
            gwy_data_field_grains_intersect(mask, existing_mask);
    }
    gwy_data_field_data_changed(mask);
}

static void
type_changed(GtkToggleButton *toggle,
             DisconnControls *controls)
{
    if (!gtk_toggle_button_get_active(toggle))
        return;

    controls->args->type = gwy_radio_buttons_get_current(controls->type);
}

static void
size_changed(GtkAdjustment *adj, DisconnControls *controls)
{
    controls->args->size = gwy_adjustment_get_int(adj);
}

static void
threshold_changed(GtkAdjustment *adj, DisconnControls *controls)
{
    controls->args->threshold = gtk_adjustment_get_value(adj);
}

static void
combine_changed(GtkToggleButton *toggle, DisconnControls *controls)
{
    controls->args->combine = gtk_toggle_button_get_active(toggle);
}

static void
combine_type_changed(GtkComboBox *combo, DisconnControls *controls)
{
    controls->args->combine_type = gwy_enum_combo_box_get_active(combo);
}

/* Remove from mask pixels with values that do not belong to the largest
 * contiguous block of values in the height distribution. */
static guint
unmark_disconnected_values(GwyDataField *dfield, GwyDataField *inclmask,
                           guint n, gdouble threshold)
{
    guint xres = gwy_data_field_get_xres(dfield);
    guint yres = gwy_data_field_get_yres(dfield);
    guint lineres = (guint)floor(2.5*cbrt(xres*yres - n) + 0.5);
    GwyDataLine *dline = gwy_data_line_new(lineres, lineres, FALSE);
    const gdouble *d;
    gdouble *m;
    guint nn, i, blockstart = 0, bestblockstart = 0, bestblocklen = 0;
    gdouble blocksum = 0.0, bestblocksum = 0.0;
    gdouble real, off, min, max, rho_zero;

    gwy_data_field_area_dh(dfield, inclmask, dline, 0, 0, xres, yres, lineres);
    rho_zero = gwy_data_line_get_max(dline)/sqrt(xres*yres - n)*threshold;
    d = gwy_data_line_get_data_const(dline);
    lineres = gwy_data_line_get_res(dline);

    for (i = 0; i <= lineres; i++) {
        if (i == lineres || (i && d[i] + d[i-1] < rho_zero)) {
            if (blocksum > bestblocksum) {
                bestblocksum = blocksum;
                bestblockstart = blockstart;
                bestblocklen = i - blockstart;
            }
            blockstart = i+1;
            blocksum = 0.0;
        }
        else
            blocksum += d[i];
    }

    if (bestblocklen == lineres) {
        g_object_unref(dline);
        return 0;
    }

    real = gwy_data_line_get_real(dline);
    off = gwy_data_line_get_offset(dline);
    min = off + real/lineres*bestblockstart;
    max = off + real/lineres*(bestblockstart + bestblocklen + 1);
    m = gwy_data_field_get_data(inclmask);
    d = gwy_data_field_get_data_const(dfield);
    nn = 0;
    for (i = 0; i < xres*yres; i++) {
        if (m[i] > 0.0 && (d[i] < min || d[i] > max)) {
            m[i] = 0.0;
            nn++;
        }
    }

    g_object_unref(dline);
    return nn;
}

static gint*
median_make_circle(gint radius)
{
    gint *data;
    gint i;

    data = g_new(gint, 2*radius + 1);

    for (i = 0; i <= radius; i++) {
        gdouble u = (gdouble)i/radius;

        if (G_UNLIKELY(u > 1.0))
            data[radius+i] = data[radius-i] = 0;
        else
            data[radius+i] = data[radius-i] = GWY_ROUND(radius*sqrt(1.0 - u*u));

    }

    return data;
}

static gboolean
median_background(gint size,
                  GwyDataField *dfield,
                  GwyDataField *result)
{
    gint *circle;
    gdouble *data, *rdata, *buffer;
    gint i, j, xres, yres, buflen;

    data = gwy_data_field_get_data(dfield);
    xres = gwy_data_field_get_xres(result);
    yres = gwy_data_field_get_yres(result);
    rdata = gwy_data_field_get_data(result);

    buflen = 0;
    circle = median_make_circle(size);
    for (i = 0; i < 2*size + 1; i++)
        buflen += 2*circle[i] + 1;
    buffer = g_new(gdouble, buflen);

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            gint n, k, from, to;

            n = 0;
            for (k = MAX(0, i - size); k <= MIN(yres - 1, i + size); k++) {
                gdouble *row = data + k*xres;

                from = MAX(0, j - circle[k - i + size]);
                to = MIN(xres - 1, j + circle[k - i + size]);
                memcpy(buffer + n, row + from, (to - from + 1)*sizeof(gdouble));
                n += to - from + 1;
            }
            rdata[i*xres + j] = gwy_math_median(n, buffer);
        }
        if (i % 10 == 0 && !gwy_app_wait_set_fraction((gdouble)i/yres)) {
            g_free(circle);
            return FALSE;
        }
    }

    g_free(circle);

    return TRUE;
}

static gboolean
disconn_do(GwyDataField *dfield,
           GwyDataField *mask,
           const DisconnArgs *args)
{
    GwyDataField *difffield = NULL;
    gint xres = gwy_data_field_get_xres(dfield);
    gint yres = gwy_data_field_get_yres(dfield);
    guint n, nn;
    gboolean ok = FALSE;

    /* Remove the positive, negative (or both) defects using a filter.  This
     * produces a non-defect field. */
    gwy_data_field_copy(dfield, mask, FALSE);
    if (!gwy_app_wait_set_message(_("Filtering...")))
        goto finish;

    if (args->type == FEATURES_POSITIVE || args->type == FEATURES_NEGATIVE) {
        gint size = 2*args->size + 1;
        GwyMinMaxFilterType filtertpe;

        GwyDataField *kernel = gwy_data_field_new(size, size, size, size, TRUE);
        gwy_data_field_elliptic_area_fill(kernel, 0, 0, size, size, 1.0);
        if (args->type == FEATURES_POSITIVE)
            filtertpe = GWY_MIN_MAX_FILTER_OPENING;
        else
            filtertpe = GWY_MIN_MAX_FILTER_CLOSING;
        gwy_data_field_area_filter_min_max(mask, kernel, filtertpe,
                                           0, 0, xres, yres);
    }
    else {
        if (!median_background(args->size, dfield, mask))
            goto finish;
    }

    /* Then find look at the difference and mark any outliers in it because
     * these must be defects. */
    difffield = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_subtract_fields(difffield, dfield, mask);
    gwy_data_field_fill(mask, 1.0);

    if (!gwy_app_wait_set_message(_("Marking outliers...")))
        goto finish;

    n = 0;
    while ((nn = unmark_disconnected_values(difffield, mask,
                                            n, 4.0*args->threshold))) {
        n += nn;
    }

    gwy_data_field_grains_invert(mask);
    ok = TRUE;

finish:
    gwy_app_wait_finish();
    gwy_object_unref(difffield);
    return ok;
}

static const gchar combine_key[]      = "/module/mark_disconn/combine";
static const gchar combine_type_key[] = "/module/mark_disconn/combine_type";
static const gchar radius_key[]       = "/module/mark_disconn/radius";
static const gchar threshold_key[]    = "/module/mark_disconn/threshold";
static const gchar type_key[]         = "/module/mark_disconn/type";

static void
disconn_sanitize_args(DisconnArgs *args)
{
    if (args->type != FEATURES_POSITIVE
        && args->type != FEATURES_NEGATIVE)
        args->type = FEATURES_BOTH;
    args->size = CLAMP(args->size, 1, 256);
    args->threshold = CLAMP(args->threshold, 0.0, 1.0);
    args->combine = !!args->combine;
    args->combine_type = MIN(args->combine_type, GWY_MERGE_INTERSECTION);
}

static void
disconn_load_args(GwyContainer *container,
                  DisconnArgs *args)
{
    *args = disconn_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_int32_by_name(container, radius_key, &args->size);
    gwy_container_gis_double_by_name(container, threshold_key,
                                     &args->threshold);
    gwy_container_gis_boolean_by_name(container, combine_key, &args->combine);
    gwy_container_gis_enum_by_name(container, combine_type_key,
                                   &args->combine_type);
    disconn_sanitize_args(args);
}

static void
disconn_save_args(GwyContainer *container,
                  DisconnArgs *args)
{
    gwy_container_set_int32_by_name(container, type_key, args->type);
    gwy_container_set_int32_by_name(container, radius_key, args->size);
    gwy_container_set_double_by_name(container, threshold_key, args->threshold);
    gwy_container_set_boolean_by_name(container, combine_key, args->combine);
    gwy_container_set_enum_by_name(container, combine_type_key,
                                   args->combine_type);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
