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
#include <libgwydgets/gwyradiobuttons.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define DISCONN_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

typedef enum {
    FEATURES_POSITIVE = 1 << 0,
    FEATURES_NEGATIVE = 1 << 2,
    FEATURES_BOTH     = (FEATURES_POSITIVE | FEATURES_NEGATIVE),
} GwyFeaturesType;

typedef struct {
    GwyFeaturesType type;
    gint size;
} DisconnArgs;

typedef struct {
    DisconnArgs *args;
    GSList *type;
    GtkObject *size;
} DisconnControls;

static gboolean module_register      (void);
static void     mark_disconn         (GwyContainer *data,
                                      GwyRunType run);
static gboolean disconn_dialog       (DisconnArgs *args);
static void     disconn_dialog_update(DisconnControls *controls,
                                      DisconnArgs *args);
static gboolean disconn_do           (GwyDataField *dfield,
                                      GwyDataField *mask,
                                      const DisconnArgs *args,
                                      GwyContainer *data,
                                      gint id);
static void     type_changed         (GtkToggleButton *toggle,
                                      DisconnControls *controls);
static void     size_changed         (GtkAdjustment *adj,
                                      DisconnControls *controls);
static void     disconn_load_args    (GwyContainer *container,
                                      DisconnArgs *args);
static void     disconn_save_args    (GwyContainer *container,
                                      DisconnArgs *args);

static const DisconnArgs disconn_defaults = {
    FEATURES_BOTH,
    5,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask of values disconnected to the rest."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
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
    GwyDataField *dfield, *maskfield;
    GQuark dquark, mquark;
    gboolean has_mask;
    gint xres, yres, count, id;

    g_return_if_fail(run & DISCONN_RUN_MODES);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_KEY, &dquark,
                                     GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     GWY_APP_MASK_FIELD, &maskfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    g_return_if_fail(dfield && dquark);
    has_mask = !!maskfield;

    disconn_load_args(gwy_app_settings_get(), &args);
    maskfield = NULL;
    if (run == GWY_RUN_IMMEDIATE || disconn_dialog(&args)) {
        maskfield = gwy_data_field_new_alike(dfield, FALSE);
        if (!disconn_do(dfield, maskfield, &args, data, id))
            gwy_object_unref(maskfield);
    }

    disconn_save_args(gwy_app_settings_get(), &args);
    if (!maskfield)
        return;

    /* Do not create useless undo levels if there was no mask and the new
     * mask would be empty.  And do not create empty masks at all, just
     * remove the mask instead.  */
    xres = gwy_data_field_get_xres(maskfield);
    yres = gwy_data_field_get_yres(maskfield);
    gwy_data_field_area_count_in_range(maskfield, NULL, 0, 0, xres, yres,
                                       0.0, 0.0, &count, NULL);
    count = xres*yres - count;

    if (count || has_mask)
        gwy_app_undo_qcheckpointv(data, 1, &mquark);

    if (count)
        gwy_container_set_object(data, mquark, maskfield);
    else if (has_mask)
        gwy_container_remove(data, mquark);

    gwy_app_channel_log_add_proc(data, id, id);

    g_object_unref(maskfield);
}

static gboolean
disconn_dialog(DisconnArgs *args)
{
    enum { RESPONSE_RESET = 1 };

    static const GwyEnum types[] = {
        { N_("Positive"), FEATURES_POSITIVE, },
        { N_("Negative"), FEATURES_NEGATIVE, },
        { N_("Both"),     FEATURES_BOTH,     },
    };

    GtkWidget *dialog, *table, *label;
    DisconnControls controls;
    GSList *group;
    gint response, row;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Mark Disconnected"), NULL, 0,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gwy_help_add_to_proc_dialog(GTK_DIALOG(dialog), GWY_HELP_DEFAULT);

    table = gtk_table_new(5, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table,
                       FALSE, FALSE, 4);
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
    gwy_table_attach_hscale(table, row, _("_Maximum outlier radius:"), "px",
                            controls.size, GWY_HSCALE_SQRT);
    g_signal_connect(controls.size, "value-changed",
                     G_CALLBACK(size_changed), &controls);
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

            case RESPONSE_RESET:
            *args = disconn_defaults;
            disconn_dialog_update(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
disconn_dialog_update(DisconnControls *controls,
                      DisconnArgs *args)
{
    gwy_radio_buttons_set_current(controls->type, args->type);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->size), args->size);
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

/* Remove from mask pixels with values that do not belong to the largest
 * contiguous block of values in the height distribution. */
static guint
unmark_disconnected_values(GwyDataField *dfield, GwyDataField *inclmask,
                           guint n)
{
    guint xres = gwy_data_field_get_xres(dfield);
    guint yres = gwy_data_field_get_yres(dfield);
    guint lineres = (guint)floor(cbrt(xres*yres - n) + 0.5);
    GwyDataLine *dline = gwy_data_line_new(lineres, lineres, FALSE);
    const gdouble *d;
    gdouble *m;
    guint nn, i, blockstart = 0, bestblockstart = 0, bestblocklen = 0;
    gdouble blocksum = 0.0, bestblocksum = 0.0;
    gdouble real, off, min, max;

    gwy_data_field_area_dh(dfield, inclmask, dline, 0, 0, xres, yres, lineres);
    d = gwy_data_line_get_data_const(dline);
    lineres = gwy_data_line_get_res(dline);

    for (i = 0; i <= lineres; i++) {
        if (i == lineres || d[i] == 0.0) {
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
           const DisconnArgs *args,
           GwyContainer *data,
           gint id)
{
    GwyDataField *difffield = NULL;
    gint xres = gwy_data_field_get_xres(dfield);
    gint yres = gwy_data_field_get_yres(dfield);
    guint n, nn;
    gboolean ok = FALSE;

    gwy_app_wait_start(gwy_app_find_window_for_channel(data, id),
                       _("Initializing..."));

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
    while ((nn = unmark_disconnected_values(difffield, mask, n)))
        n += nn;

    gwy_data_field_grains_invert(mask);
    ok = TRUE;

finish:
    gwy_app_wait_finish();
    gwy_object_unref(difffield);
    return ok;
}

static const gchar radius_key[] = "/module/median-bg/radius";
static const gchar type_key[]   = "/module/median-bg/type";

static void
disconn_sanitize_args(DisconnArgs *args)
{
    if (args->type != FEATURES_POSITIVE
        && args->type != FEATURES_NEGATIVE)
        args->type = FEATURES_BOTH;
    args->size = CLAMP(args->size, 1, 256);
}

static void
disconn_load_args(GwyContainer *container,
                  DisconnArgs *args)
{
    *args = disconn_defaults;

    gwy_container_gis_enum_by_name(container, type_key, &args->type);
    gwy_container_gis_int32_by_name(container, radius_key, &args->size);
    disconn_sanitize_args(args);
}

static void
disconn_save_args(GwyContainer *container,
                  DisconnArgs *args)
{
    gwy_container_set_int32_by_name(container, type_key, args->type);
    gwy_container_set_int32_by_name(container, radius_key, args->size);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
