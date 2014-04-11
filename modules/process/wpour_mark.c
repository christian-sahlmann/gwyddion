/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
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

#define WPOUR_RUN_MODES (GWY_RUN_IMMEDIATE | GWY_RUN_INTERACTIVE)

enum {
    PREVIEW_SIZE = 400
};

enum {
    RESPONSE_RESET   = 1,
    RESPONSE_PREVIEW = 2
};

typedef struct {
    gboolean inverted;
    gboolean update;

    /* interface only */
    gboolean computed;
} WPourArgs;

typedef struct {
    GtkWidget *dialog;
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *color_button;
    GtkWidget *update;
    GwyContainer *mydata;
    WPourArgs *args;
    gboolean in_init;
} WPourControls;

static gboolean      module_register             (void);
static void          wpour_mark                  (GwyContainer *data,
                                                  GwyRunType run);
static void          run_noninteractive          (WPourArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  GQuark mquark);
static void          wpour_dialog                (WPourArgs *args,
                                                  GwyContainer *data,
                                                  GwyDataField *dfield,
                                                  gint id,
                                                  GQuark mquark);
static void          mask_color_changed          (GtkWidget *color_button,
                                                  WPourControls *controls);
static void          load_mask_color             (GtkWidget *color_button,
                                                  GwyContainer *data);
static void          wpour_dialog_update_controls(WPourControls *controls,
                                                  WPourArgs *args);
static void          inverted_changed            (WPourControls *controls,
                                                  GtkToggleButton *toggle);
static void          update_changed              (WPourControls *controls,
                                                  GtkToggleButton *toggle);
static void          table_attach_threshold      (GtkWidget *table,
                                                  gint *row,
                                                  const gchar *name,
                                                  GtkObject **adj,
                                                  gdouble value,
                                                  GtkWidget **check,
                                                  gpointer data);
static void          wpour_invalidate            (WPourControls *controls);
static GwyDataField* create_mask_field           (GwyDataField *dfield);
static void          preview                     (WPourControls *controls,
                                                  WPourArgs *args);
static void          wpour_do                    (GwyDataField *dfield,
                                                  GwyDataField *maskfield,
                                                  WPourArgs *args);
static void          wpour_load_args             (GwyContainer *container,
                                                  WPourArgs *args);
static void          wpour_save_args             (GwyContainer *container,
                                                  WPourArgs *args);
static void          wpour_sanitize_args         (WPourArgs *args);

static const WPourArgs wpour_defaults = {
    FALSE,
    TRUE,
    /* interface only */
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Segmentates image using watershed with pre- and postprocessing."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_process_func_register("wpour_mark",
                              (GwyProcessFunc)&wpour_mark,
                              N_("/_Grains/_Mark by Segmentation..."),
                              GWY_STOCK_GRAINS_WATER,
                              WPOUR_RUN_MODES,
                              GWY_MENU_FLAG_DATA,
                              N_("Segmentate using watershed "));

    return TRUE;
}

static void
wpour_mark(GwyContainer *data, GwyRunType run)
{
    WPourArgs args;
    GwyDataField *dfield;
    GQuark mquark;
    gint id;

    g_return_if_fail(run & WPOUR_RUN_MODES);
    wpour_load_args(gwy_app_settings_get(), &args);
    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     GWY_APP_MASK_FIELD_KEY, &mquark,
                                     0);
    g_return_if_fail(dfield && mquark);

    if (run == GWY_RUN_IMMEDIATE) {
        run_noninteractive(&args, data, dfield, mquark);
        gwy_app_channel_log_add(data, id, id, "proc::wpour_mark", NULL);
    }
    else {
        wpour_dialog(&args, data, dfield, id, mquark);
    }
}

static void
run_noninteractive(WPourArgs *args,
                   GwyContainer *data,
                   GwyDataField *dfield,
                   GQuark mquark)
{
    GwyDataField *mfield;

    gwy_app_undo_qcheckpointv(data, 1, &mquark);
    mfield = create_mask_field(dfield);
    wpour_do(dfield, mfield, args);
    gwy_container_set_object(data, mquark, mfield);
    g_object_unref(mfield);
}

static void
wpour_dialog(WPourArgs *args,
            GwyContainer *data,
            GwyDataField *dfield,
            gint id,
            GQuark mquark)
{
    GtkWidget *dialog, *table, *hbox;
    WPourControls controls;
    gint response;
    GwyPixmapLayer *layer;
    GwyDataField *mfield;
    gint row;
    gboolean temp;

    controls.args = args;
    controls.in_init = TRUE;

    dialog = gtk_dialog_new_with_buttons(_("Segmentate by Watershed"), NULL, 0,
                                         NULL);
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog),
                                 gwy_stock_like_button_new(_("_Update"),
                                                           GTK_STOCK_EXECUTE),
                                 RESPONSE_PREVIEW);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog),
                                      RESPONSE_PREVIEW, !args->update);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Reset"), RESPONSE_RESET);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog),
                          GTK_STOCK_OK, GTK_RESPONSE_OK);
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

    table = gtk_table_new(10, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Preprocessing")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Postprocessing")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    gtk_table_attach(GTK_TABLE(table), gwy_label_new_header(_("Options")),
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    row++;

    controls.inverted = gtk_check_button_new_with_mnemonic(_("_Invert height"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inverted),
                                 args->inverted);
    gtk_table_attach(GTK_TABLE(table), controls.inverted,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.inverted, "toggled",
                             G_CALLBACK(inverted_changed), &controls);
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
                     G_CALLBACK(mask_color_changed), &controls);
    row++;

    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_changed), &controls);

    controls.in_init = FALSE;
    wpour_invalidate(&controls);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            g_object_unref(controls.mydata);
            wpour_save_args(gwy_app_settings_get(), args);
            return;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            temp = args->update;
            *args = wpour_defaults;
            args->update = temp;
            wpour_dialog_update_controls(&controls, args);
            controls.in_init = TRUE;
            preview(&controls, args);
            controls.in_init = FALSE;
            break;

            case RESPONSE_PREVIEW:
            preview(&controls, args);
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

    if (args->computed) {
        mfield = gwy_container_get_object_by_name(controls.mydata, "/0/mask");
        gwy_app_undo_qcheckpointv(data, 1, &mquark);
        gwy_container_set_object(data, mquark, mfield);
        g_object_unref(controls.mydata);
    }
    else {
        g_object_unref(controls.mydata);
        run_noninteractive(args, data, dfield, mquark);
    }

    wpour_save_args(gwy_app_settings_get(), args);
    gwy_app_channel_log_add(data, id, id, "proc::wpour_mark", NULL);
}

static void
wpour_dialog_update_controls(WPourControls *controls,
                             WPourArgs *args)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverted),
                                 args->inverted);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
}

static void
inverted_changed(WPourControls *controls,
                 GtkToggleButton *toggle)
{
    controls->args->inverted = gtk_toggle_button_get_active(toggle);
    wpour_invalidate(controls);
}

static void
update_changed(WPourControls *controls,
               GtkToggleButton *toggle)
{
    controls->args->update = gtk_toggle_button_get_active(toggle);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(controls->dialog),
                                      RESPONSE_PREVIEW,
                                      !controls->args->update);
    wpour_invalidate(controls);
}

static void
table_attach_threshold(GtkWidget *table, gint *row, const gchar *name,
                       GtkObject **adj, gdouble value,
                       GtkWidget **check,
                       gpointer data)
{
    *adj = gtk_adjustment_new(value, 0.0, 100.0, 0.1, 5, 0);
    gwy_table_attach_hscale(table, *row, name, "%", *adj, GWY_HSCALE_CHECK);
    *check = gwy_table_hscale_get_check(*adj);
    g_signal_connect_swapped(*adj, "value-changed",
                             G_CALLBACK(wpour_invalidate), data);
    g_signal_connect_swapped(*check, "toggled",
                             G_CALLBACK(wpour_invalidate), data);
    (*row)++;
}

static void
mask_color_changed(GtkWidget *color_button,
                   WPourControls *controls)
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

static GwyDataField*
create_mask_field(GwyDataField *dfield)
{
    GwyDataField *mfield;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(mfield), NULL);

    return mfield;
}

static void
wpour_invalidate(WPourControls *controls)
{
    controls->args->computed = FALSE;
    if (controls->args->update && !controls->in_init) {
        preview(controls, controls->args);
    }
}

static void
preview(WPourControls *controls,
        WPourArgs *args)
{
    GwyDataField *mask, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask)) {
        mask = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);

        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }
    wpour_do(dfield, mask, args);
    gwy_data_field_data_changed(mask);

    args->computed = TRUE;
}

static void
wpour_do(GwyDataField *dfield,
         GwyDataField *maskfield,
         WPourArgs *args)
{
    GwyDataField *workfield = gwy_data_field_duplicate(dfield);

    if (args->inverted)
        gwy_data_field_invert(workfield, FALSE, FALSE, TRUE);
    gwy_data_field_waterpour(workfield, maskfield, NULL);
    g_object_unref(workfield);
}

static const gchar inverted_key[]  = "/module/wpour_mark/inverted";
static const gchar update_key[]    = "/module/wpour_mark/update";

static void
wpour_sanitize_args(WPourArgs *args)
{
    args->inverted = !!args->inverted;
    args->update = !!args->update;
}

static void
wpour_load_args(GwyContainer *container,
                WPourArgs *args)
{
    *args = wpour_defaults;

    gwy_container_gis_boolean_by_name(container, inverted_key, &args->inverted);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    wpour_sanitize_args(args);
}

static void
wpour_save_args(GwyContainer *container,
                WPourArgs *args)
{
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
