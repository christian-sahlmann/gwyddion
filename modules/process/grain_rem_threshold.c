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

#include <math.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define REMOVE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    PREVIEW_SIZE = 320
};

/* Data for this function. */
typedef struct {
    gboolean inverted;
    gdouble area;
    gdouble height;
    gboolean is_height;
    gboolean is_area;
    GwyMergeType merge_type;
} RemoveArgs;

typedef struct {
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *is_height;
    GtkWidget *is_area;
    GtkObject *threshold_height;
    GtkObject *threshold_area;
    GtkWidget *merge;
    GtkWidget *color_button;
    GwyContainer *mydata;
} RemoveControls;

static gboolean    module_register               (const gchar *name);
static gboolean    remove_th                     (GwyContainer *data,
                                                  GwyRunType run);
static gboolean    remove_dialog                 (RemoveArgs *args,
                                                  GwyContainer *data);
static void        mask_color_change_cb          (GtkWidget *color_button,
                                                  RemoveControls *controls);
static void        load_mask_color               (GtkWidget *color_button,
                                                  GwyContainer *data);
static void        save_mask_color               (GtkWidget *color_button,
                                                  GwyContainer *data);
static void        remove_dialog_update_controls (RemoveControls *controls,
                                                  RemoveArgs *args);
static void        remove_dialog_update_args     (RemoveControls *controls,
                                                  RemoveArgs *args);
static void        preview                       (RemoveControls *controls,
                                                  RemoveArgs *args,
                                                  GwyContainer *data);
static void        remove_th_do                  (RemoveArgs *args,
                                                  GwyContainer *data);
static void        mask_process                  (GwyDataField *dfield,
                                                  GwyDataField *maskfield,
                                                  RemoveArgs *args);
static void        intersect_removes             (GwyDataField *mask_a,
                                                  GwyDataField *mask_b,
                                                  GwyDataField *mask);
static void        remove_load_args              (GwyContainer *container,
                                                  RemoveArgs *args);
static void        remove_save_args              (GwyContainer *container,
                                                  RemoveArgs *args);
static void        remove_sanitize_args          (RemoveArgs *args);

RemoveArgs remove_defaults = {
    FALSE,
    50,
    50,
    TRUE,
    FALSE,
    GWY_MERGE_UNION,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "remove_threshold",
    N_("Remove grains by thresholding"),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo remove_func_info = {
        "remove_threshold",
        N_("/_Grains/_Remove by Threshold..."),
        (GwyProcessFunc)&remove_th,
        REMOVE_RUN_MODES,
        GWY_MENU_FLAG_DATA_MASK,
    };

    gwy_process_func_register(name, &remove_func_info);

    return TRUE;
}

static gboolean
remove_th(GwyContainer *data, GwyRunType run)
{
    RemoveArgs args;
    gboolean ok = FALSE;

    g_assert(run & REMOVE_RUN_MODES);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = remove_defaults;
    else
        remove_load_args(gwy_app_settings_get(), &args);

    if (!gwy_container_contains_by_name(data, "/0/mask"))
        return FALSE;

    ok = (run != GWY_RUN_MODAL) || remove_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        remove_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    remove_th_do(&args, data);

    return ok;
}

static gboolean
remove_dialog(RemoveArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *spin, *hbox, *align, *label;
    RemoveControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GwyDataField *dfield;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Remove Grains by Threshold"),
                                         NULL, 0,
                                         _("_Update Preview"), RESPONSE_PREVIEW,
                                         _("_Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

    hbox = gtk_hbox_new(FALSE, 2);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    controls.view = gwy_data_view_new(controls.mydata);
    g_object_unref(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                             "/0/data"));
    if (gwy_data_field_get_xres(dfield) >= gwy_data_field_get_yres(dfield))
        zoomval = PREVIEW_SIZE/(gdouble)gwy_data_field_get_xres(dfield);
    else
        zoomval = PREVIEW_SIZE/(gdouble)gwy_data_field_get_yres(dfield);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    layer = gwy_layer_mask_new();
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(3, 9, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Threshold By</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.threshold_height = gtk_adjustment_new(args->height,
                                                   0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Height:"), "%",
                                   controls.threshold_height, GWY_HSCALE_CHECK);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    controls.is_height = g_object_get_data(G_OBJECT(controls.threshold_height),
                                           "check");
    row++;

    controls.threshold_area = gtk_adjustment_new(args->area,
                                                 0.0, 16384.0, 1, 10, 0);
    spin = gwy_table_attach_hscale(table, row, _("_Area:"), "px<sup>2</sup>",
                                   controls.threshold_area, GWY_HSCALE_CHECK);
    controls.is_area = g_object_get_data(G_OBJECT(controls.threshold_area),
                                         "check");
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 8);
    row++;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.inverted = gtk_check_button_new_with_mnemonic(_("_Invert height"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inverted),
                                 args->inverted);
    gtk_table_attach(GTK_TABLE(table), controls.inverted,
                     0, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.merge = gwy_option_menu_mergegrain(NULL, NULL, args->merge_type);
    gwy_table_attach_hscale(table, row, _("_Selection mode:"), NULL,
                            controls.merge, GWY_HSCALE_WIDGET);
    row++;

    label = gtk_label_new_with_mnemonic(_("_Mask color:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,
                     0, 1, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                   TRUE);
    load_mask_color(controls.color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(align), controls.color_button);
    gtk_table_attach(GTK_TABLE(table), align, 1, 2, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.color_button, "clicked",
                     G_CALLBACK(mask_color_change_cb), &controls);
    row++;

    /* cheap sync */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height),
                                 !args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height),
                                 args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_area),
                                 !args->is_area);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_area),
                                 args->is_area);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            remove_dialog_update_args(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = remove_defaults;
            remove_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            remove_dialog_update_args(&controls, args);
            preview(&controls, args, data);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    remove_dialog_update_args(&controls, args);
    save_mask_color(controls.color_button, data);
    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
remove_dialog_update_controls(RemoveControls *controls,
                              RemoveArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_height),
                                args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_area),
                                args->area);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverted),
                                 args->inverted);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_height),
                                 args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_area),
                                 args->is_area);
    gwy_option_menu_set_history(controls->merge, "mergegrain-type",
                                args->merge_type);
}

static void
remove_dialog_update_args(RemoveControls *controls,
                          RemoveArgs *args)
{
    args->is_height
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_height));
    args->is_area
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_area));
    args->inverted
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->inverted));
    args->height
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_height));
    args->area
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_area));
    args->merge_type = gwy_option_menu_get_history(controls->merge,
                                                   "mergegrain-type");
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     RemoveControls *controls)
{
    gwy_color_selector_for_mask(NULL,
                                GWY_DATA_VIEW(controls->view),
                                GWY_COLOR_BUTTON(color_button),
                                NULL, "/0/mask");
    load_mask_color(color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls->view)));
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
save_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyRGBA rgba;

    gwy_color_button_get_color(GWY_COLOR_BUTTON(color_button), &rgba);
    gwy_rgba_store_to_container(&rgba, data, "/0/mask");
}

static void
preview(RemoveControls *controls,
        RemoveArgs *args,
        GwyContainer *data)
{
    GwyDataField *maskfield, *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /*set up the mask*/
    if (gwy_container_contains_by_name(controls->mydata, "/0/mask")) {
        maskfield
            = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                              "/0/mask"));

        gwy_data_field_copy(GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                  "/0/mask")), maskfield);
        mask_process(dfield, maskfield, args);

        gwy_container_set_object_by_name(controls->mydata, "/0/mask",
                                         G_OBJECT(maskfield));

    }
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

}

static void
remove_th_do(RemoveArgs *args,
             GwyContainer *data)
{

    GwyDataField *dfield, *maskfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (gwy_container_contains_by_name(data, "/0/mask")) {
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                  "/0/mask"));
        mask_process(dfield, maskfield, args);
    }

}

static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             RemoveArgs *args)
{
    GwyDataField *output_field_a, *output_field_b;

    if (args->merge_type == GWY_MERGE_UNION
        || (args->is_height*args->is_area) == 0) {
        if (args->is_height)
            gwy_data_field_grains_remove_by_height(dfield, maskfield,
                                                   args->inverted ?
                                                   100.0 - args->height
                                                   : args->height,
                                                   args->inverted);
        if (args->is_area)
            gwy_data_field_grains_remove_by_size(maskfield, args->area);
    }
    else {
        output_field_a
            = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(maskfield)));
        output_field_b
            = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(maskfield)));

        gwy_data_field_grains_remove_by_height(dfield, output_field_a,
                                               args->inverted ?
                                               100.0 - args->height
                                               : args->height,
                                               args->inverted);
        gwy_data_field_grains_remove_by_size(output_field_b, args->area);

        intersect_removes(output_field_a, output_field_b, maskfield);

        g_object_unref(output_field_a);
        g_object_unref(output_field_b);
    }
}

/* FIXME: this is *very* inefficient, should streamlined like other grain
 * algorithms */
static void
intersect_removes(GwyDataField *mask_a, GwyDataField *mask_b, GwyDataField *mask)
{
    gint i, xres, yres;
    xres = mask->xres;
    yres = mask->yres;

    for (i = 0; i < xres*yres; i++) {
        if (mask->data[i] > 0 && !mask_a->data[i] && !mask_b->data[i])
            gwy_data_field_grains_remove_manually(mask, i);
    }
}

static const gchar *inverted_key = "/module/remove_threshold/inverted";
static const gchar *isheight_key = "/module/remove_threshold/isheight";
static const gchar *isarea_key = "/module/remove_threshold/isarea";
static const gchar *height_key = "/module/remove_threshold/height";
static const gchar *area_key = "/module/remove_threshold/area";
static const gchar *mergetype_key = "/module/remove_threshold/mergetype";

static void
remove_sanitize_args(RemoveArgs *args)
{
    args->inverted = !!args->inverted;
    args->is_height = !!args->is_height;
    args->is_area = !!args->is_area;
    args->height = CLAMP(args->height, 0.0, 100.0);
    args->area = CLAMP(args->area, 0.0, 100.0);
    args->merge_type = MIN(args->merge_type, GWY_MERGE_INTERSECTION);
}

static void
remove_load_args(GwyContainer *container,
                 RemoveArgs *args)
{
    *args = remove_defaults;

    gwy_container_gis_boolean_by_name(container, inverted_key, &args->inverted);
    gwy_container_gis_boolean_by_name(container, isheight_key,
                                      &args->is_height);
    gwy_container_gis_boolean_by_name(container, isarea_key, &args->is_area);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    gwy_container_gis_double_by_name(container, area_key, &args->area);
    gwy_container_gis_enum_by_name(container, mergetype_key,
                                   &args->merge_type);
    remove_sanitize_args(args);
}

static void
remove_save_args(GwyContainer *container,
                 RemoveArgs *args)
{
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    gwy_container_set_boolean_by_name(container, isheight_key, args->is_height);
    gwy_container_set_boolean_by_name(container, isarea_key, args->is_area);
    gwy_container_set_double_by_name(container, height_key, args->height);
    gwy_container_set_double_by_name(container, area_key, args->area);
    gwy_container_set_enum_by_name(container, mergetype_key, args->merge_type);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
