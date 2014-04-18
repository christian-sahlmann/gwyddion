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
#include <libprocess/arithmetic.h>
#include <libprocess/filters.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <libgwydgets/gwycombobox.h>
#include <libgwydgets/gwyradiobuttons.h>
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

typedef enum {
    PREVIEW_ORIGINAL,
    PREVIEW_PREPROC,
    PREVIEW_NTYPES,
} PreviewType;

typedef struct {
    guint size;
    guint len;
    gint *data;
} IntList;

typedef struct {
    gboolean inverted;
    gboolean update;
    PreviewType preview_type;
    gdouble blur_fwhm;
    gdouble prefill_level;
    gdouble prefill_height;
    gdouble gradient_contrib;
    gdouble curvature_contrib;

    /* interface only */
    gboolean computed;
} WPourArgs;

typedef struct {
    WPourArgs *args;
    GtkWidget *dialog;
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *color_button;
    GtkWidget *update;
    GwyPixmapLayer *player;
    GSList *preview_type;
    GtkAdjustment *blur_fwhm;
    GtkAdjustment *prefill_level;
    GtkAdjustment *prefill_height;
    GtkAdjustment *gradient_contrib;
    GtkAdjustment *curvature_contrib;
    GwyContainer *mydata;
    gboolean in_init;
} WPourControls;

static gboolean       module_register             (void);
static void           wpour_mark                  (GwyContainer *data,
                                                   GwyRunType run);
static void           run_noninteractive          (WPourArgs *args,
                                                   GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   GQuark mquark);
static void           wpour_dialog                (WPourArgs *args,
                                                   GwyContainer *data,
                                                   GwyDataField *dfield,
                                                   gint id,
                                                   GQuark mquark);
static void           mask_color_changed          (GtkWidget *color_button,
                                                   WPourControls *controls);
static void           load_mask_color             (GtkWidget *color_button,
                                                   GwyContainer *data);
static void           wpour_dialog_update_controls(WPourControls *controls,
                                                   WPourArgs *args);
static void           inverted_changed            (WPourControls *controls,
                                                   GtkToggleButton *toggle);
static void           update_changed              (WPourControls *controls,
                                                   GtkToggleButton *toggle);
static void           preview_type_changed        (GtkToggleButton *toggle,
                                                   WPourControls *controls);
static GtkAdjustment* table_attach_threshold      (GtkWidget *table,
                                                   gint *row,
                                                   const gchar *name,
                                                   gdouble *value,
                                                   WPourControls *controls);
static void           wpour_update_double         (WPourControls *controls,
                                                   GtkAdjustment *adj);
static void           wpour_invalidate            (WPourControls *controls);
static GwyDataField*  create_mask_field           (GwyDataField *dfield);
static void           preview                     (WPourControls *controls,
                                                   WPourArgs *args);
static void           wpour_do                    (GwyDataField *dfield,
                                                   GwyDataField *maskfield,
                                                   GwyDataField *preprocessed,
                                                   WPourArgs *args);
static void           add_slope_contribs          (GwyDataField *workspace,
                                                   GwyDataField *dfield,
                                                   gdouble gradient_contrib,
                                                   gdouble curvature_contrib);
static void           normal_vector_difference    (GwyDataField *result,
                                                   GwyDataField *xder,
                                                   GwyDataField *yder);
static void           prefill_minima              (GwyDataField *dfield,
                                                   GwyDataField *workspace,
                                                   IntList *inqueue,
                                                   IntList *outqueue,
                                                   gdouble depth,
                                                   gdouble height);
static void           wpour_load_args             (GwyContainer *container,
                                                   WPourArgs *args);
static void           wpour_save_args             (GwyContainer *container,
                                                   WPourArgs *args);
static void           wpour_sanitize_args         (WPourArgs *args);

static const WPourArgs wpour_defaults = {
    FALSE,
    TRUE,
    PREVIEW_ORIGINAL,
    0.0,
    0.0, 0.0,
    0.0, 0.0,
    /* interface only */
    FALSE,
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Segments image using watershed with pre- and postprocessing."),
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
                              N_("Segment using watershed "));

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
    wpour_do(dfield, mfield, NULL, args);
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
    GtkWidget *dialog, *table, *hbox, *label;
    GSList *group;
    GtkObject *gtkobj;
    WPourControls controls;
    gint response;
    GwyPixmapLayer *layer;
    GwyDataField *mfield;
    gint row;
    gboolean temp;

    controls.args = args;
    controls.in_init = TRUE;

    dialog = gtk_dialog_new_with_buttons(_("Segment by Watershed"), NULL, 0,
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
    controls.player = layer;
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

    gtkobj = gtk_adjustment_new(args->blur_fwhm, 0.0, 20.0, 0.01, 0.1, 0);
    controls.blur_fwhm = GTK_ADJUSTMENT(gtkobj);
    g_object_set_data(G_OBJECT(gtkobj), "target", &args->blur_fwhm);
    gwy_table_attach_hscale(table, row,
                            _("Gaussian _smoothening:"), "px", gtkobj,
                            GWY_HSCALE_SQRT);
    g_signal_connect_swapped(gtkobj, "value-changed",
                             G_CALLBACK(wpour_update_double), &controls);
    row++;

    controls.gradient_contrib
        = table_attach_threshold(table, &row, _("Add _gradient:"),
                                 &args->gradient_contrib, &controls);

    controls.curvature_contrib
        = table_attach_threshold(table, &row, _("Add _curvature:"),
                                 &args->curvature_contrib, &controls);

    controls.prefill_level
        = table_attach_threshold(table, &row, _("Prefill _level:"),
                                 &args->prefill_level, &controls);

    controls.prefill_height
        = table_attach_threshold(table, &row, _("Pre_fill from minima:"),
                                 &args->prefill_height, &controls);

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

    label = gtk_label_new(_("Preview:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 2, row, row+1, GTK_FILL, 0, 0, 0);
    row++;

    group = gwy_radio_buttons_createl(G_CALLBACK(preview_type_changed),
                                      &controls,
                                      args->preview_type,
                                      _("Original _image"), PREVIEW_ORIGINAL,
                                      _("Pr_eprocessed image"), PREVIEW_PREPROC,
                                      NULL);
    controls.preview_type = group;
    row = gwy_radio_buttons_attach_to_table(group, GTK_TABLE(table), 3, row);

    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);
    controls.update = gtk_check_button_new_with_mnemonic(_("I_nstant updates"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.update),
                                 args->update);
    gtk_table_attach(GTK_TABLE(table), controls.update,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
    g_signal_connect_swapped(controls.update, "toggled",
                             G_CALLBACK(update_changed), &controls);
    row++;

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
    controls->in_init = TRUE;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverted),
                                 args->inverted);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->update),
                                 args->update);
    gwy_radio_buttons_set_current(controls->preview_type, args->preview_type);
    gtk_adjustment_set_value(controls->blur_fwhm, args->blur_fwhm);
    gtk_adjustment_set_value(controls->prefill_level, args->prefill_level);
    gtk_adjustment_set_value(controls->prefill_height, args->prefill_height);
    gtk_adjustment_set_value(controls->gradient_contrib,
                             args->gradient_contrib);
    gtk_adjustment_set_value(controls->curvature_contrib,
                             args->curvature_contrib);
    controls->in_init = FALSE;
    wpour_invalidate(controls);
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
preview_type_changed(G_GNUC_UNUSED GtkToggleButton *toggle,
                     WPourControls *controls)
{
    WPourArgs *args = controls->args;
    args->preview_type = gwy_radio_buttons_get_current(controls->preview_type);
    wpour_invalidate(controls);
}

static GtkAdjustment*
table_attach_threshold(GtkWidget *table, gint *row, const gchar *name,
                       gdouble *value, WPourControls *controls)
{
    GtkObject *adj;

    adj = gtk_adjustment_new(*value, 0.0, 100.0, 0.01, 1.0, 0);
    g_object_set_data(G_OBJECT(adj), "target", value);
    gwy_table_attach_hscale(table, *row, name, "%", adj, GWY_HSCALE_DEFAULT);
    g_signal_connect_swapped(adj, "value-changed",
                             G_CALLBACK(wpour_update_double), controls);
    (*row)++;

    return GTK_ADJUSTMENT(adj);
}

static void
wpour_update_double(WPourControls *controls, GtkAdjustment *adj)
{
    gdouble *value = g_object_get_data(G_OBJECT(adj), "target");

    *value = gtk_adjustment_get_value(adj);
    wpour_invalidate(controls);
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
    GwyDataField *mask, *dfield, *preprocessed;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    if (!gwy_container_gis_object_by_name(controls->mydata, "/1/data",
                                          &preprocessed)) {
        preprocessed = gwy_data_field_new_alike(dfield, FALSE);
        gwy_container_set_object_by_name(controls->mydata, "/1/data",
                                         preprocessed);
        g_object_unref(preprocessed);
    }

    if (!gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask)) {
        mask = create_mask_field(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);

        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view), layer);
    }
    wpour_do(dfield, mask, preprocessed, args);
    gwy_data_field_data_changed(mask);
    gwy_data_field_data_changed(preprocessed);

    if (args->preview_type == PREVIEW_ORIGINAL)
        g_object_set(controls->player, "data-key", "/0/data", NULL);
    else
        g_object_set(controls->player, "data-key", "/1/data", NULL);

    args->computed = TRUE;
}

static inline IntList*
int_list_new(guint prealloc)
{
    IntList *list = g_slice_new0(IntList);
    prealloc = MAX(prealloc, 16);
    list->size = prealloc;
    list->data = g_new(gint, list->size);
    return list;
}

static inline void
int_list_add(IntList *list, gint i)
{
    if (G_UNLIKELY(list->len == list->size)) {
        list->size = MAX(2*list->size, 16);
        list->data = g_renew(gint, list->data, list->size);
    }

    list->data[list->len] = i;
    list->len++;
}

static inline void
int_list_add_unique(IntList **plist, gint i)
{
    IntList *list;
    guint j;

    if (!*plist)
        *plist = int_list_new(0);

    list = *plist;
    for (j = 0; j < list->len; j++) {
        if (list->data[j] == i)
            return;
    }
    int_list_add(list, i);
}

static void
int_list_free(IntList *list)
{
    g_free(list->data);
    g_slice_free(IntList, list);
}

static void
wpour_do(GwyDataField *dfield,
         GwyDataField *maskfield,
         GwyDataField *preprocessed,
         WPourArgs *args)
{
    guint xres = dfield->xres, yres = dfield->yres;
    IntList *inqueue = int_list_new(0);
    IntList *outqueue = int_list_new(0);

    if (preprocessed) {
        g_object_ref(preprocessed);
        gwy_data_field_copy(dfield, preprocessed, FALSE);
    }
    else {
        preprocessed = gwy_data_field_duplicate(dfield);
    }

    if (args->inverted)
        gwy_data_field_invert(preprocessed, FALSE, FALSE, TRUE);

    /* Use maskfield as a scratch buffer. */
    gwy_data_field_add(preprocessed, -gwy_data_field_get_max(preprocessed));
    if (args->blur_fwhm) {
        gdouble sigma = args->blur_fwhm/(2.0*sqrt(2*G_LN2));
        gwy_data_field_area_filter_gaussian(preprocessed, sigma,
                                            0, 0, xres, yres);
    }
    add_slope_contribs(maskfield, preprocessed,
                       args->gradient_contrib, args->curvature_contrib);
    prefill_minima(preprocessed, maskfield, inqueue, outqueue,
                   args->prefill_level, args->prefill_height);
    gwy_data_field_waterpour(preprocessed, maskfield, NULL);

    int_list_free(outqueue);
    int_list_free(inqueue);
    g_object_unref(preprocessed);
}

static void
add_slope_contribs(GwyDataField *workspace, GwyDataField *dfield,
                   gdouble gradient_contrib, gdouble curvature_contrib)
{
    GwyDataField *xder, *yder;
    gdouble r, rg, pg, pc;

    if (!gradient_contrib && !curvature_contrib)
        return;

    r = gwy_data_field_get_rms(dfield);
    if (!r)
        return;

    xder = gwy_data_field_new_alike(dfield, FALSE);
    yder = gwy_data_field_new_alike(dfield, FALSE);

    pg = gradient_contrib/100.0;
    pc = curvature_contrib/100.0;

    gwy_data_field_filter_slope(dfield, xder, yder);
    gwy_data_field_multiply(dfield, 1.0 - MAX(pg, pc));

    /* We need this for both operations. */
    gwy_data_field_hypot_of_fields(workspace, xder, yder);
    rg = gwy_data_field_get_rms(workspace);

    if (gradient_contrib) {
        gwy_data_field_multiply(workspace, pg * r/rg);
        gwy_data_field_sum_fields(dfield, dfield, workspace);
    }

    if (curvature_contrib) {
        gdouble rc;

        gwy_data_field_multiply(xder, 1.0/rg);
        gwy_data_field_multiply(yder, 1.0/rg);
        normal_vector_difference(workspace, xder, yder);
        rc = gwy_data_field_get_rms(workspace);
        if (rc) {
            gwy_data_field_multiply(workspace, pc * r/rc);
            gwy_data_field_sum_fields(dfield, dfield, workspace);
        }
    }

    g_object_unref(yder);
    g_object_unref(xder);

    gwy_data_field_invalidate(dfield);
    gwy_data_field_invalidate(workspace);
}

static void
normal_vector(gdouble bx, gdouble by,
              gdouble *nx, gdouble *ny, gdouble *nz)
{
    gdouble b = sqrt(1.0 + bx*bx + by*by);

    *nx = -bx/b;
    *ny = -by/b;
    *nz = 1.0/b;
}

static void
normal_vector_difference(GwyDataField *result,
                         GwyDataField *xder,
                         GwyDataField *yder)
{
    const gdouble *bx, *by;
    guint xres, yres, i, j;
    gdouble *d;

    gwy_data_field_clear(result);
    xres = result->xres;
    yres = result->yres;
    d = result->data;
    bx = xder->data;
    by = yder->data;

    for (i = 0; i < yres; i++) {
        gdouble *row = d + i*xres, *next = row + xres;
        const gdouble *bxrow = bx + i*xres, *nextbx = bxrow + xres;
        const gdouble *byrow = by + i*yres, *nextby = byrow + yres;

        for (j = 0; j < xres; j++) {
            gdouble nx, ny, nz;
            gdouble nxr, nyr, nzr;
            gdouble nxd, nyd, nzd;
            gdouble ch, cv;

            normal_vector(bxrow[j], byrow[j], &nx, &ny, &nz);
            if (j < xres-1) {
                normal_vector(bxrow[j+1], byrow[j+1], &nxr, &nyr, &nzr);
                nxr -= nx;
                nyr -= ny;
                nzr -= nz;
                ch = sqrt(nxr*nxr + nyr*nyr + nzr*nzr);
                row[j] += ch;
                row[j+1] += ch;
            }

            if (i < yres-1) {
                normal_vector(nextbx[j], nextby[j], &nxd, &nyd, &nzd);
                nxd -= nx;
                nyd -= ny;
                nzd -= nz;
                cv = sqrt(nxd*nxd + nyd*nyd + nzd*nzd);
                row[j] += cv;
                next[j] += cv;
            }
        }
    }

    gwy_data_field_invalidate(result);
}

static void
prefill_minima(GwyDataField *dfield, GwyDataField *workspace,
               IntList *inqueue, IntList *outqueue,
               gdouble depth, gdouble height)
{
    guint i, j, k, m, xres = dfield->xres, yres = dfield->yres;
    gdouble min, max;

    gwy_data_field_get_min_max(dfield, &min, &max);
    if (min == max)
        return;

    /* Simple absolute prefilling corresponding to plain mark-by-threshold. */
    if (depth > 0.0) {
        gdouble depththreshold = depth/100.0*(max - min) + min;
        gdouble *d = dfield->data;

        for (k = 0; k < xres*yres; k++) {
            if (d[k] < depththreshold)
                d[k] = depththreshold;
        }
        gwy_data_field_invalidate(dfield);
    }

    /* Simple height prefilling which floods all pixels with heights only
     * little above the minimum. */
    if (height > 0.0) {
        gdouble heightthreshold = height/100.0*(max - min);
        gdouble *d = dfield->data, *w = workspace->data;

        gwy_data_field_mark_extrema(dfield, workspace, FALSE);

        inqueue->len = 0;
        for (k = 0; k < xres*yres; k++) {
            if (w[k])
                int_list_add(inqueue, k);
        }

        while (inqueue->len) {
            outqueue->len = 0;
            for (m = 0; m < inqueue->len; m++) {
                gdouble z, zth;

                k = inqueue->data[m];
                i = k/xres;
                j = k % xres;
                z = d[k];
                zth = z + heightthreshold*fabs(z)/(max - min);

                if (i > 0 && d[k-xres] > z && d[k-xres] < zth) {
                    d[k-xres] = z;
                    int_list_add(outqueue, k-xres);
                }
                if (j > 0 && d[k-1] > z && d[k-1] < zth) {
                    d[k-1] = z;
                    int_list_add(outqueue, k-1);
                }
                if (j < xres-1 && d[k+1] > z && d[k+1] < zth) {
                    d[k+1] = z;
                    int_list_add(outqueue, k+1);
                }
                if (i < yres-1 && d[k+xres] > z && d[k+xres] < zth) {
                    d[k+xres] = z;
                    int_list_add(outqueue, k+xres);
                }
            }

            GWY_SWAP(IntList*, inqueue, outqueue);
        }

        gwy_data_field_invalidate(dfield);
    }
}

static const gchar inverted_key[]          = "/module/wpour_mark/inverted";
static const gchar update_key[]            = "/module/wpour_mark/update";
static const gchar preview_type_key[]      = "/module/wpour_mark/preview_type";
static const gchar blur_fwhm_key[]         = "/module/wpour_mark/blur_fwhm";
static const gchar prefill_level_key[]     = "/module/wpour_mark/prefill_level";
static const gchar prefill_height_key[]    = "/module/wpour_mark/prefill_height";
static const gchar gradient_contrib_key[]  = "/module/wpour_mark/gradient_contrib";
static const gchar curvature_contrib_key[] = "/module/wpour_mark/curvature_contrib";

static void
wpour_sanitize_args(WPourArgs *args)
{
    args->inverted = !!args->inverted;
    args->update = !!args->update;
    args->preview_type = MIN(args->preview_type, PREVIEW_NTYPES-1);
    args->blur_fwhm = CLAMP(args->blur_fwhm, 0.0, 100.0);
    args->prefill_level = CLAMP(args->prefill_level, 0.0, 100.0);
    args->prefill_height = CLAMP(args->prefill_height, 0.0, 100.0);
    args->gradient_contrib = CLAMP(args->gradient_contrib, 0.0, 100.0);
    args->curvature_contrib = CLAMP(args->curvature_contrib, 0.0, 100.0);
}

static void
wpour_load_args(GwyContainer *container,
                WPourArgs *args)
{
    *args = wpour_defaults;

    gwy_container_gis_boolean_by_name(container, inverted_key, &args->inverted);
    gwy_container_gis_boolean_by_name(container, update_key, &args->update);
    gwy_container_gis_enum_by_name(container, preview_type_key,
                                   &args->preview_type);
    gwy_container_gis_double_by_name(container, blur_fwhm_key,
                                     &args->blur_fwhm);
    gwy_container_gis_double_by_name(container, prefill_level_key,
                                     &args->prefill_level);
    gwy_container_gis_double_by_name(container, prefill_height_key,
                                     &args->prefill_height);
    gwy_container_gis_double_by_name(container, gradient_contrib_key,
                                     &args->gradient_contrib);
    gwy_container_gis_double_by_name(container, curvature_contrib_key,
                                     &args->curvature_contrib);
    wpour_sanitize_args(args);
}

static void
wpour_save_args(GwyContainer *container,
                WPourArgs *args)
{
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    gwy_container_set_boolean_by_name(container, update_key, args->update);
    gwy_container_set_enum_by_name(container, preview_type_key,
                                   args->preview_type);
    gwy_container_set_double_by_name(container, blur_fwhm_key,
                                     args->blur_fwhm);
    gwy_container_set_double_by_name(container, prefill_level_key,
                                     args->prefill_level);
    gwy_container_set_double_by_name(container, prefill_height_key,
                                     args->prefill_height);
    gwy_container_set_double_by_name(container, gradient_contrib_key,
                                     args->gradient_contrib);
    gwy_container_set_double_by_name(container, curvature_contrib_key,
                                     args->curvature_contrib);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
