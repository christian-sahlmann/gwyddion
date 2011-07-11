/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti), Petr Klapetek, Daniil Bratashov.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, dn2010@gmail.com.
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
#include <libprocess/grains.h>
#include <libprocess/stats.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwystock.h>
#include <libgwymodule/gwymodule-process.h>
#include <app/gwymoduleutils.h>
#include <app/gwyapp.h>

#define GEDGE_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

enum {
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2
};

typedef struct {
    gdouble threshold_laplasian;
    gboolean update;
} GEdgeArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *view;
    GwyContainer *mydata;
    GtkObject *threshold_laplasian;
    GtkWidget *color_button;
    GtkWidget *update;
    GEdgeArgs *args;
    gboolean in_init;
} GEdgeControls;

static gboolean    module_register              (void);
static void        grain_edge                   (GwyContainer *data,
                                                 GwyRunType run);
static void        run_noninteractive           (GEdgeArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 GQuark mquark);
static void        gedge_dialog                 (GEdgeArgs *args,
                                                 GwyContainer *data,
                                                 GwyDataField *dfield,
                                                 gint id,
                                                 GQuark mquark);
static void        mask_color_change_cb         (GtkWidget *color_button,
                                                 GEdgeControls *controls);
static void        load_mask_color              (GtkWidget *color_button,
                                                 GwyContainer *data);
static void        gedge_dialog_update_controls (GEdgeControls *controls,
                                                 GEdgeArgs *args);
static void        gedge_dialog_update_values   (GEdgeControls *controls,
                                                 GEdgeArgs *args);
static void        update_change_cb             (GEdgeControls *controls);
static void        gedge_invalidate             (GEdgeControls *controls);
static void        preview                      (GEdgeControls *controls,
                                                 GEdgeArgs *args);
static void        gedge_process                (GwyDataField *dfield,
                                                 GwyDataField *maskfield,
                                                 GEdgeArgs *args);
static void        gedge_load_args              (GwyContainer *container,
                                                 GEdgeArgs *args);
static void        gedge_save_args              (GwyContainer *container,
                                                 GEdgeArgs *args);
static void        gedge_sanitize_args          (GEdgeArgs *args);

static const GEdgeArgs gedge_defaults = {
    50.0,
    TRUE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Marks grains by edge detection method."),
    "Daniil Bratashov <dn2010@gmail.com>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek & Daniil Bratashov",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("grain_edge",
                              (GwyProcessFunc)&grain_edge,
                              N_("/_Grains/Mark by _Edge Detection..."),
                              GWY_STOCK_GRAINS,
                              GEDGE_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Mark grains with edge detection mechanism"));

    return TRUE;
}

static void
grain_edge(GwyContainer *data, GwyRunType run)
{
    GEdgeArgs args;
    GwyDataField *dfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & GEDGE_RUN_MODES);
    gedge_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield && mquark);

    if (run == GWY_RUN_IMMEDIATE)
        run_noninteractive(&args, data, dfield, mquark);
    else {
        gedge_dialog(&args, data, dfield, id, mquark);
        gedge_save_args(gwy_app_settings_get(), &args);
    }
}

static void
table_attach_threshold(GtkWidget *table, gint *row, const gchar *name,
                       GtkObject **adj, gdouble value,
                       gpointer data)
{
    *adj = gtk_adjustment_new(value, 0.0, 100.0, 0.1, 5, 0);
    gwy_table_attach_hscale(table, *row, name, "%", *adj, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(gedge_invalidate), data);
    (*row)++;
}

static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;
    GwySIUnit *siunit;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    siunit = gwy_si_unit_new(NULL);
    gwy_data_field_set_si_unit_z(mfield, siunit);
    g_object_unref(siunit);

    return mfield;
}

static void
run_noninteractive(GEdgeArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GQuark mquark)
{
    GwyDataField *mfield;

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    mfield = create_mask_field(dfield);
    gedge_process(dfield, mfield, args);
    gwy_container_set_object(data, mquark, mfield);
    g_object_unref(mfield);
}

static void
gedge_dialog(GEdgeArgs *args,
             GwyContainer *data,
             GwyDataField *dfield,
             gint id,
             GQuark mquark)
{
    GtkWidget *dialog, *table, *hbox;
    GEdgeControls controls;
    GwyPixmapLayer *layer;
    gint row;
    gint response;
    gboolean temp;

    controls.in_init = TRUE;
    controls.args = args;

    dialog = gtk_dialog_new_with_buttons(_("Mark Grains by Edge Detection"), NULL, 0,
                                         NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    controls.dialog = dialog;

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = gwy_container_new();
    gwy_container_set_object_by_name(controls.mydata, "/0/data", dfield);
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

    table = gtk_table_new(5, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Threshold")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    table_attach_threshold(table, &row, _("_Laplacian:"),
                           &controls.threshold_laplasian,
                           args->threshold_laplasian,
                           &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Options")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                   TRUE);
    load_mask_color(controls.color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    gwy_table_attach_hscale(table, row++, _("_Mask color:"), NULL,
                            GTK_OBJECT(controls.color_button),
                            GWY_HSCALE_WIDGET_NO_EXPAND);
    g_signal_connect(controls.color_button, "clicked",
                     G_CALLBACK(mask_color_change_cb), &controls);
    row++;

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_change_cb), &controls);

    gedge_invalidate(&controls);

    /* finished initializing, allow instant updates */
    controls.in_init = FALSE;

    /* show initial preview if instant updates are on */
    if (args->update) {
        gtk_dialog_set_response_sensitive(GTK_DIALOG(controls.dialog),
                                          RESPONSE_PREVIEW, FALSE);
        preview(&controls, args);
    }

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gedge_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            temp = args->update;
            *args = gedge_defaults;
            args->update = temp;
            gedge_dialog_update_controls(&controls, args);
            controls.in_init = TRUE;
            preview(&controls, args);
            controls.in_init = FALSE;
            break;

            case RESPONSE_PREVIEW:
            gedge_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    gedge_dialog_update_values(&controls, args);
    gwy_app_sync_data_items(controls.mydata, data, 0, id, FALSE,
                            GWY_DATA_ITEM_MASK_COLOR,
                            0);
    gtk_widget_destroy(dialog);

    g_object_unref(controls.mydata);
    run_noninteractive(args, data, dfield, mquark);
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     GEdgeControls *controls)
{
    GwyContainer *data;

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    gwy_mask_color_selector_run(NULL, GTK_WINDOW(controls->dialog),
                                GWY_COLOR_BUTTON(color_button), data,
                                "/0/mask");
    load_mask_color(color_button, data);
}

static void
load_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    if (!gwy_rgba_get_from_container(&rgba, data, "/0/mask")) {
        gwy_rgba_get_from_container(&rgba, gwy_app_settings_get(), "/mask");
        gwy_rgba_store_to_container(&rgba, data, "/0/mask");
    }
    gwy_color_button_set_color(GWY_COLOR_BUTTON(color_button), &rgba);
}

static void
gedge_dialog_update_controls(GEdgeControls *controls,
                             GEdgeArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_laplasian),
                             args->threshold_laplasian);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
}

static void
gedge_dialog_update_values(GEdgeControls *controls,
                           GEdgeArgs *args)
{
    args->threshold_laplasian
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_laplasian));
    args->update
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));
}

static void
gedge_invalidate(GEdgeControls *controls)
{
    /* create preview if instant updates are on */
    if (controls->args->update && !controls->in_init) {
        gedge_dialog_update_values(controls, controls->args);
        preview(controls, controls->args);
    }
}

static void
preview(GEdgeControls *controls,
        GEdgeArgs *args)
{
    GwyDataField *mask, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /* Set up the mask */
    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask)) {
        mask = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);

        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }
    gwy_data_field_copy(dfield, mask, FALSE);
    gedge_process(dfield, mask, args);
    gwy_data_field_data_changed(mask);
}

static void
update_change_cb(GEdgeControls *controls)
{
    controls->args->update
            = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->update));

    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);

    if (controls->args->update)
        gedge_invalidate(controls);
}

static void
gedge_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             GEdgeArgs *args)
{
    GwyDataField *temp_field;

    temp_field = gwy_data_field_new_alike(dfield, FALSE);
    gwy_data_field_copy(dfield, temp_field, FALSE);
    gwy_data_field_filter_laplacian_of_gaussians(temp_field);
    gwy_data_field_grains_mark_height(temp_field, maskfield,
                                      args->threshold_laplasian, TRUE);

    g_object_unref(temp_field);
}

static const gchar threshold_laplasian_key[]
    = "/module/grain_edge/threshold_laplasian";
static const gchar update_key[] = "/module/grain_edge/update";

static void
gedge_sanitize_args(GEdgeArgs *args)
{
    args->threshold_laplasian
        = CLAMP(args->threshold_laplasian, 0.0, 100.0);
    args->update = !!args->update;
}

static void
gedge_load_args(GwyContainer *container,
               GEdgeArgs *args)
{
    *args = gedge_defaults;

    gwy_container_gis_double_by_name(container, threshold_laplasian_key,
                                     &args->threshold_laplasian);
    gwy_container_gis_boolean_by_name(container, update_key,
                                     &args->update);

    gedge_sanitize_args(args);
}

static void
gedge_save_args(GwyContainer *container,
               GEdgeArgs *args)
{
    gwy_container_set_double_by_name(container, threshold_laplasian_key,
                                     args->threshold_laplasian);
    gwy_container_set_boolean_by_name(container, update_key,
                                      args->update);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
