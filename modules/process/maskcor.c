/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/arithmetic.h>
#include <libprocess/correlation.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwyapp.h>

#define MASKCOR_RUN_MODES GWY_RUN_INTERACTIVE

typedef enum {
    GWY_MASKCOR_OBJECTS,
    GWY_MASKCOR_MAXIMA,
    GWY_MASKCOR_SCORE,
    GWY_MASKCOR_LAST
} MaskcorResult;

typedef struct {
    GwyContainer *data;
    gint id;
} GwyDataObjectId;

typedef struct {
    MaskcorResult result;
    gdouble threshold;
    GwyCorrelationType method;
    GwyDataObjectId data;
    GwyDataObjectId kernel;
} MaskcorArgs;

typedef struct {
    MaskcorArgs *args;
    GtkObject *threshold;
} MaskcorControls;

static gboolean module_register      (void);
static void     maskcor              (GwyContainer *data,
                                      GwyRunType run);
static gboolean maskcor_dialog       (MaskcorArgs *args);
static void     maskcor_operation_cb (GtkWidget *item,
                                      MaskcorControls *controls);
static void     maskcor_threshold_cb (GtkAdjustment *adj,
                                      gdouble *value);
static void     maskcor_kernel_cb    (GwyDataChooser *chooser,
                                      GwyDataObjectId *object);
static gboolean maskcor_kernel_filter(GwyContainer *data,
                                      gint id,
                                      gpointer user_data);
static void     maskcor_do           (MaskcorArgs *args);
static void     maskcor_load_args    (GwyContainer *settings,
                                      MaskcorArgs *args);
static void     maskcor_save_args    (GwyContainer *settings,
                                      MaskcorArgs *args);
static void     maskcor_sanitize_args(MaskcorArgs *args);

static const MaskcorArgs maskcor_defaults = {
    GWY_MASKCOR_OBJECTS, 0.95, GWY_CORRELATION_NORMAL,
    { NULL, -1 }, { NULL, -1 },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Creates mask by correlation with another data."),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("maskcor",
                              (GwyProcessFunc)&maskcor,
                              N_("/M_ultidata/_Mask by Correlation..."),
                              NULL,
                              MASKCOR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Create mask by correlation with another "
                                 "data"));

    return TRUE;
}

static void
maskcor(GwyContainer *data, GwyRunType run)
{
    MaskcorArgs args;
    GwyContainer *settings;

    g_return_if_fail(run & MASKCOR_RUN_MODES);
    settings = gwy_app_settings_get();
    maskcor_load_args(settings, &args);

    args.data.data = data;
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD_ID, &args.data.id, 0);
    args.kernel.data = NULL;

    if (maskcor_dialog(&args))
        maskcor_do(&args);

    maskcor_save_args(settings, &args);
}

static gboolean
maskcor_dialog(MaskcorArgs *args)
{
    static const GwyEnum results[] = {
        { N_("Objects marked"),     GWY_MASKCOR_OBJECTS },
        { N_("Correlation maxima"), GWY_MASKCOR_MAXIMA },
        { N_("Correlation score"),  GWY_MASKCOR_SCORE },
    };
    MaskcorControls controls;
    GtkWidget *dialog, *table, *chooser, *spin, *combo, *method;
    GtkObject *adj;
    gint row, response;
    gboolean ok;

    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Mask by Correlation"), NULL, 0,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    table = gtk_table_new(5, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 4);
    row = 0;

    /* Kernel */
    chooser = gwy_data_chooser_new_channels();
    g_object_set_data(G_OBJECT(chooser), "dialog", dialog);
    gwy_data_chooser_set_filter(GWY_DATA_CHOOSER(chooser),
                                maskcor_kernel_filter, &args->data, NULL);
    g_signal_connect(chooser, "changed",
                     G_CALLBACK(maskcor_kernel_cb), &args->kernel);
    maskcor_kernel_cb(GWY_DATA_CHOOSER(chooser), &args->kernel);
    gwy_table_attach_hscale(table, row, _("Correlation _kernel:"), NULL,
                            GTK_OBJECT(chooser), GWY_HSCALE_WIDGET);
    row++;

    /* Result */
    combo = gwy_enum_combo_box_new(results, G_N_ELEMENTS(results),
                                   G_CALLBACK(maskcor_operation_cb), &controls,
                                   args->result, TRUE);
    gwy_table_attach_row(table, row, _("Output _type:"), NULL, combo);
    row++;

    /* Parameters */
    method = gwy_enum_combo_box_new(gwy_correlation_type_get_enum(), -1,
                                    G_CALLBACK(gwy_enum_combo_box_update_int),
                                    &args->method, args->method, TRUE);
    gwy_table_attach_row(table, row, _("Correlation _method:"), NULL, method);
    row++;

    adj = gtk_adjustment_new(args->threshold, -1.0, 1.0, 0.01, 0.1, 0);
    controls.threshold = adj;
    spin = gwy_table_attach_hscale(table, row, _("T_hreshold:"), NULL, adj, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 3);
    gwy_table_hscale_set_sensitive(adj, args->result != GWY_MASKCOR_SCORE);
    g_signal_connect(adj, "value-changed",
                     G_CALLBACK(maskcor_threshold_cb), &args->threshold);
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
            gtk_widget_destroy(dialog);
            ok = TRUE;
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (!ok);

    return ok;
}

static void
maskcor_operation_cb(GtkWidget *combo, MaskcorControls *controls)
{
    controls->args->result
        = gwy_enum_combo_box_get_active(GTK_COMBO_BOX(combo));
    gwy_table_hscale_set_sensitive(controls->threshold,
                                   controls->args->result != GWY_MASKCOR_SCORE);
}

static void
maskcor_threshold_cb(GtkAdjustment *adj, gdouble *value)
{
    *value = gtk_adjustment_get_value(adj);
}

static void
maskcor_kernel_cb(GwyDataChooser *chooser,
                  GwyDataObjectId *object)
{
    GtkWidget *dialog;

    object->data = gwy_data_chooser_get_active(chooser, &object->id);
    gwy_debug("kernel: %p %d", object->data, object->id);

    dialog = g_object_get_data(G_OBJECT(chooser), "dialog");
    g_assert(GTK_IS_DIALOG(dialog));
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK,
                                      object->data != NULL);
}

static gboolean
maskcor_kernel_filter(GwyContainer *data,
                      gint id,
                      gpointer user_data)
{
    GwyDataObjectId *object = (GwyDataObjectId*)user_data;
    GwyDataField *kernel, *dfield;
    GQuark quark;

    quark = gwy_app_get_data_key_for_id(id);
    kernel = GWY_DATA_FIELD(gwy_container_get_object(data, quark));

    quark = gwy_app_get_data_key_for_id(object->id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(object->data, quark));

    if (gwy_data_field_get_xreal(kernel) <= gwy_data_field_get_xreal(dfield)/4
        && gwy_data_field_get_yreal(kernel) <= gwy_data_field_get_yreal(dfield)/4
        && !gwy_data_field_check_compatibility(kernel, dfield,
                                               GWY_DATA_COMPATIBILITY_LATERAL
                                               | GWY_DATA_COMPATIBILITY_VALUE))
        return TRUE;

    return FALSE;
}

static void
plot_correlated(GwyDataField *retfield, gint xsize, gint ysize,
                gdouble threshold)
{
    GwyDataField *tmp;
    gint xres, yres, i, j, col, row, w, h;
    const gdouble *data;

    tmp = gwy_data_field_duplicate(retfield);
    gwy_data_field_clear(retfield);

    xres = gwy_data_field_get_xres(retfield);
    yres = gwy_data_field_get_yres(retfield);
    data = gwy_data_field_get_data_const(tmp);

    /* FIXME: this is very inefficient */
    for (i = 0; i < yres; i++) {
        row = MAX(i - ysize/2, 0);
        h = MIN(i + ysize - ysize/2, yres) - row;
        for (j = 0; j < xres; j++) {
            if (data[i*xres + j] > threshold) {
                col = MAX(j - xsize/2, 0);
                w = MIN(j + xsize - xsize/2, xres) - col;
                gwy_data_field_area_fill(retfield, col, row, w, h, 1.0);
            }
        }
    }

    g_object_unref(tmp);
}

static void
maskcor_do(MaskcorArgs *args)
{
    enum { WORK_PER_UPDATE = 50000000 };
    GwyDataField *dfield, *kernel, *retfield, *score;
    GwyComputationState *state;
    GQuark quark;
    gint newid, work, wpi;

    quark = gwy_app_get_data_key_for_id(args->kernel.id);
    kernel = GWY_DATA_FIELD(gwy_container_get_object(args->kernel.data, quark));

    quark = gwy_app_get_data_key_for_id(args->data.id);
    dfield = GWY_DATA_FIELD(gwy_container_get_object(args->data.data, quark));

    retfield = gwy_data_field_new_alike(dfield, FALSE);

    /* FIXME */
    if (args->method == GWY_CORRELATION_NORMAL) {
        gwy_app_wait_start(gwy_app_find_window_for_channel(args->data.data,
                                                           args->data.id),
                           _("Initializing"));
        state = gwy_data_field_correlate_init(dfield, kernel, retfield);
        gwy_app_wait_set_message(_("Correlating"));
        work = 0;
        wpi = gwy_data_field_get_xres(kernel)*gwy_data_field_get_yres(kernel);
        wpi = MIN(wpi, WORK_PER_UPDATE);
        do {
            gwy_data_field_correlate_iteration(state);
            work += wpi;
            if (work > WORK_PER_UPDATE) {
                work -= WORK_PER_UPDATE;
                if (!gwy_app_wait_set_fraction(state->fraction)) {
                    gwy_data_field_correlate_finalize(state);
                    gwy_app_wait_finish();
                    g_object_unref(retfield);
                    return;
                }
            }
        } while (state->state != GWY_COMPUTATION_STATE_FINISHED);
        gwy_data_field_correlate_finalize(state);
        gwy_app_wait_finish();
    }
    else
        gwy_data_field_correlate(dfield, kernel, retfield, args->method);

    /* score - do new data with score */
    if (args->result == GWY_MASKCOR_SCORE) {
        score = gwy_data_field_duplicate(retfield);
        newid = gwy_app_data_browser_add_data_field(score, args->data.data,
                                                    TRUE);
        gwy_app_sync_data_items(args->data.data, args->data.data,
                                args->data.id, newid, FALSE,
                                GWY_DATA_ITEM_GRADIENT, 0);
        gwy_app_set_data_field_title(args->data.data, newid,
                                     _("Correlation score"));
        g_object_unref(score);
    }
    else {
        /* add mask */
        quark = gwy_app_get_mask_key_for_id(args->data.id);
        gwy_app_undo_qcheckpointv(args->data.data, 1, &quark);
        if (args->result == GWY_MASKCOR_OBJECTS)
            plot_correlated(retfield,
                            gwy_data_field_get_xres(kernel),
                            gwy_data_field_get_yres(kernel),
                            args->threshold);
        else if (args->result == GWY_MASKCOR_MAXIMA)
            gwy_data_field_threshold(retfield, args->threshold, 0.0, 1.0);

        gwy_container_set_object(args->data.data, quark, retfield);
    }
    g_object_unref(retfield);
}

static const gchar result_key[]    = "/module/maskcor/result";
static const gchar method_key[]    = "/module/maskcor/method";
static const gchar threshold_key[] = "/module/maskcor/threshold";

static void
maskcor_sanitize_args(MaskcorArgs *args)
{
    args->result = MIN(args->result, GWY_MASKCOR_LAST-1);
    args->method = MIN(args->method, GWY_CORRELATION_POC);
    args->threshold = CLAMP(args->threshold, -1.0, 1.0);
}

static void
maskcor_load_args(GwyContainer *settings,
                  MaskcorArgs *args)
{
    *args = maskcor_defaults;
    gwy_container_gis_enum_by_name(settings, result_key, &args->result);
    gwy_container_gis_enum_by_name(settings, method_key, &args->method);
    gwy_container_gis_double_by_name(settings, threshold_key, &args->threshold);
    maskcor_sanitize_args(args);
}

static void
maskcor_save_args(GwyContainer *settings,
                  MaskcorArgs *args)
{
    gwy_container_set_enum_by_name(settings, result_key, args->result);
    gwy_container_set_enum_by_name(settings, method_key, args->method);
    gwy_container_set_double_by_name(settings, threshold_key, args->threshold);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

