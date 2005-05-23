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
#include <libprocess/grains.h>
#include <libgwydgets/gwydgets.h>
#include <app/gwyapp.h>

#define MARK_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

enum {
    PREVIEW_SIZE = 320
};

enum {
    MARK_HEIGHT = 0,
    MARK_SLOPE  = 1,
    MARK_LAP    = 2
};

/* Data for this function. */
typedef struct {
    gboolean inverted;
    gdouble height;
    gdouble slope;
    gdouble lap;
    gboolean is_height;
    gboolean is_slope;
    gboolean is_lap;
    GwyMergeType merge_type;
} MarkArgs;

typedef struct {
    GtkWidget *inverted;
    GtkWidget *view;
    GtkWidget *is_height;
    GtkWidget *is_slope;
    GtkWidget *is_lap;
    GtkObject *threshold_height;
    GtkObject *threshold_slope;
    GtkObject *threshold_lap;
    GtkWidget *check_height;
    GtkWidget *check_slope;
    GtkWidget *check_lap;
    GtkWidget *merge;
    GtkWidget *color_button;
    GwyContainer *mydata;
    gboolean computed;
} MarkControls;

static gboolean    module_register            (const gchar *name);
static gboolean    mark                       (GwyContainer *data,
                                               GwyRunType run);
static gboolean    mark_dialog                (MarkArgs *args,
                                               GwyContainer *data);
static void        mask_color_change_cb       (GtkWidget *color_button,
                                               MarkControls *controls);
static void        load_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        save_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        mark_dialog_update_controls(MarkControls *controls,
                                               MarkArgs *args);
static void        mark_dialog_update_values  (MarkControls *controls,
                                               MarkArgs *args);
static void        mark_invalidate            (GObject *obj,
                                               MarkControls *controls);
static void        preview                    (MarkControls *controls,
                                               MarkArgs *args);
static void        mark_ok                    (MarkControls *controls,
                                               MarkArgs *args,
                                               GwyContainer *data);
static void        add_mask_layer             (GtkWidget *data_view);
static void        mask_process               (GwyDataField *dfield,
                                               GwyDataField *maskfield,
                                               MarkArgs *args);
static void        mark_load_args             (GwyContainer *container,
                                               MarkArgs *args);
static void        mark_save_args             (GwyContainer *container,
                                               MarkArgs *args);
static void        mark_sanitize_args         (MarkArgs *args);


MarkArgs mark_defaults = {
    FALSE,
    100,
    100,
    100,
    TRUE,
    FALSE,
    FALSE,
    GWY_MERGE_UNION,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Marks grains by thresholding (height, slope, curvature)."),
    "Petr Klapetek <petr@klapetek.cz>",
    "1.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo mark_func_info = {
        "mark_threshold",
        N_("/_Grains/_Mark by Threshold..."),
        (GwyProcessFunc)&mark,
        MARK_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &mark_func_info);

    return TRUE;
}

static gboolean
mark(GwyContainer *data, GwyRunType run)
{
    MarkArgs args;
    gboolean ok = TRUE;

    g_assert(run & MARK_RUN_MODES);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = mark_defaults;
    else
        mark_load_args(gwy_app_settings_get(), &args);

    if (run == GWY_RUN_MODAL) {
        ok = mark_dialog(&args, data);
        mark_save_args(gwy_app_settings_get(), &args);
    }
    else
        mark_ok(NULL, &args, data);

    return ok;
}

static void
table_attach_threshold(GtkWidget *table, gint *row, const gchar *name,
                       GtkObject **adj, gdouble value,
                       GtkWidget **check,
                       gpointer data)
{
    GtkWidget *spin;

    *adj = gtk_adjustment_new(value, 0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_hscale(table, *row, name, "%", *adj,
                                   GWY_HSCALE_CHECK);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 1);
    *check = g_object_get_data(G_OBJECT(*adj), "check");
    g_signal_connect(*adj, "value_changed", G_CALLBACK(mark_invalidate), data);
    (*row)++;
}

static gboolean
mark_dialog(MarkArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *label, *hbox;
    MarkControls controls;
    enum {
        RESPONSE_RESET = 1,
        RESPONSE_PREVIEW = 2
    };
    gint response;
    gdouble zoomval;
    GwyPixmapLayer *layer;
    GwyDataField *dfield;
    gint row;

    dialog = gtk_dialog_new_with_buttons(_("Mark Grains by Threshold"), NULL, 0,
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

    controls.mydata = gwy_container_duplicate(data);
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_pixmap_layer_set_data_key(layer, "/0/data");
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view), layer);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                             "/0/data"));

    if (gwy_data_field_get_xres(dfield) >= gwy_data_field_get_yres(dfield))
        zoomval = PREVIEW_SIZE/(gdouble)gwy_data_field_get_xres(dfield);
    else
        zoomval = PREVIEW_SIZE/(gdouble)gwy_data_field_get_yres(dfield);

    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);

    table = gtk_table_new(10, 4, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 4);
    row = 0;

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Threshold By</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    table_attach_threshold(table, &row, _("_Height:"),
                           &controls.threshold_height, args->height,
                           &controls.is_height, &controls);

    table_attach_threshold(table, &row, _("_Slope:"),
                           &controls.threshold_slope, args->slope,
                           &controls.is_slope, &controls);

    table_attach_threshold(table, &row, _("_Curvature:"),
                           &controls.threshold_lap, args->lap,
                           &controls.is_lap, &controls);
    gtk_table_set_row_spacing(GTK_TABLE(table), row-1, 8);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Options</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 4, row, row+1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);
    row++;

    controls.inverted = gtk_check_button_new_with_mnemonic(_("_Invert height"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.inverted),
                                 args->inverted);
    gtk_table_attach(GTK_TABLE(table), controls.inverted,
                     0, 3, row, row+1, GTK_EXPAND | GTK_FILL, 0, 2, 2);
    g_signal_connect(controls.inverted, "toggled",
                     G_CALLBACK(mark_invalidate), &controls);
    row++;

    controls.merge = gwy_option_menu_merge_type(G_CALLBACK(mark_invalidate),
                                                &controls, args->merge_type);
    gwy_table_attach_hscale(table, row, _("Mer_ge mode:"), NULL,
                            GTK_OBJECT(controls.merge), GWY_HSCALE_WIDGET);
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

    controls.computed = FALSE;

    /* cheap sync */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height),
                                 !args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height),
                                 args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_slope),
                                 !args->is_slope);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_slope),
                                 args->is_slope);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_lap),
                                 !args->is_lap);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_lap),
                                 args->is_lap);

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            mark_dialog_update_values(&controls, args);
            g_object_unref(controls.mydata);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = mark_defaults;
            mark_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            mark_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    save_mask_color(controls.color_button, data);
    mark_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);
    mark_ok(&controls, args, data);
    g_object_unref(controls.mydata);

    return TRUE;
}

static void
mark_dialog_update_controls(MarkControls *controls,
                            MarkArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_height),
                             args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_slope),
                             args->slope);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_lap),
                             args->lap);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_height),
                                 args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->inverted),
                                 args->inverted);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_slope),
                                 args->is_slope);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_lap),
                                 args->is_lap);
    gwy_option_menu_set_history(controls->merge, "merge-type",
                                args->merge_type);
}

static void
mark_dialog_update_values(MarkControls *controls,
                          MarkArgs *args)
{
    args->height
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_height));
    args->slope
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_slope));
    args->lap
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_lap));
    args->is_height
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_height));
    args->inverted
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->inverted));
    args->is_slope
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_slope));
    args->is_lap
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_lap));
    args->merge_type
        = gwy_option_menu_get_history(controls->merge, "merge-type");
}

static void
mark_invalidate(G_GNUC_UNUSED GObject *obj,
                MarkControls *controls)
{
    controls->computed = FALSE;
}

static void
mask_color_change_cb(GtkWidget *color_button,
                     MarkControls *controls)
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
preview(MarkControls *controls,
        MarkArgs *args)
{
    GwyDataField *dfield, *mask;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /*set up the mask*/
    if (gwy_container_gis_object_by_name(controls->mydata, "/0/mask", &mask))
        gwy_data_field_copy(dfield, mask, FALSE);
    else {
        mask = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(controls->mydata, "/0/mask", mask);
        g_object_unref(mask);
    }

    mask_process(dfield, mask, args);
    controls->computed = TRUE;
    add_mask_layer(controls->view);
    gwy_data_field_data_changed(mask);
}

static void
add_mask_layer(GtkWidget *data_view)
{
    GwyPixmapLayer *layer;

    if (!gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(data_view))) {
        layer = gwy_layer_mask_new();
        gwy_pixmap_layer_set_data_key(layer, "/0/mask");
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(data_view), layer);
    }
}

static void
mark_ok(MarkControls *controls,
        MarkArgs *args,
        GwyContainer *data)
{
    GwyDataField *dfield, *maskfield;

    if (controls && controls->computed) {
        maskfield = gwy_container_get_object_by_name(controls->mydata,
                                                     "/0/mask");
        gwy_app_undo_checkpoint(data, "/0/mask", NULL);
        if (gwy_container_gis_object_by_name(data, "/0/mask", &dfield))
            gwy_data_field_copy(maskfield, dfield, FALSE);
        else
            gwy_container_set_object_by_name(data, "/0/mask", maskfield);
        return;
    }

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        maskfield = gwy_container_get_object_by_name(data, "/0/mask");
        gwy_data_field_copy(dfield, maskfield, FALSE);
    }
    else {
        maskfield = gwy_data_field_duplicate(dfield);
        gwy_container_set_object_by_name(data, "/0/mask", maskfield);
        g_object_unref(maskfield);
    }

    mask_process(dfield, maskfield, args);
}

static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             MarkArgs *args)
{
    GwyDataField *output_field;
    gboolean is_field;

    is_field = FALSE;
    output_field = gwy_data_field_new_alike(dfield, FALSE);

    if (args->is_height) {
        gwy_data_field_grains_mark_height(dfield, maskfield,
                                          args->inverted
                                          ? 100.0 - args->height : args->height,
                                          args->inverted);
        is_field = TRUE;
    }
    if (args->is_slope) {
        gwy_data_field_grains_mark_slope(dfield, output_field,
                                         args->slope, FALSE);
        if (is_field) {
            if (args->merge_type == GWY_MERGE_UNION)
                gwy_data_field_grains_add(maskfield, output_field);
            else if (args->merge_type == GWY_MERGE_INTERSECTION)
                gwy_data_field_grains_intersect(maskfield, output_field);
        }
        else
            gwy_data_field_copy(output_field, maskfield, FALSE);
        is_field = TRUE;
    }
    if (args->is_lap) {
        gwy_data_field_grains_mark_curvature(dfield, output_field,
                                             args->lap, FALSE);
        if (is_field) {
            if (args->merge_type == GWY_MERGE_UNION)
                gwy_data_field_grains_add(maskfield, output_field);
            else if (args->merge_type == GWY_MERGE_INTERSECTION)
                gwy_data_field_grains_intersect(maskfield, output_field);
        }
        else
            gwy_data_field_copy(output_field, maskfield, FALSE);
     }

    g_object_unref(output_field);
}

static const gchar *inverted_key = "/module/mark_height/inverted";
static const gchar *isheight_key = "/module/mark_height/isheight";
static const gchar *isslope_key = "/module/mark_height/isslope";
static const gchar *islap_key = "/module/mark_height/islap";
static const gchar *height_key = "/module/mark_height/height";
static const gchar *slope_key = "/module/mark_height/slope";
static const gchar *lap_key = "/module/mark_height/lap";
static const gchar *mergetype_key = "/module/mark_height/merge_type";

static void
mark_sanitize_args(MarkArgs *args)
{
    args->inverted = !!args->inverted;
    args->is_slope = !!args->is_slope;
    args->is_height = !!args->is_height;
    args->is_lap = !!args->is_lap;
    args->height = CLAMP(args->height, 0.0, 100.0);
    args->slope = CLAMP(args->slope, 0.0, 100.0);
    args->lap = CLAMP(args->lap, 0.0, 100.0);
    args->merge_type = MIN(args->merge_type, GWY_MERGE_INTERSECTION);
}

static void
mark_load_args(GwyContainer *container,
               MarkArgs *args)
{
    *args = mark_defaults;

    gwy_container_gis_boolean_by_name(container, inverted_key, &args->inverted);
    gwy_container_gis_boolean_by_name(container, isheight_key,
                                      &args->is_height);
    gwy_container_gis_boolean_by_name(container, isslope_key, &args->is_slope);
    gwy_container_gis_boolean_by_name(container, islap_key, &args->is_lap);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    gwy_container_gis_double_by_name(container, slope_key, &args->slope);
    gwy_container_gis_double_by_name(container, lap_key, &args->lap);
    gwy_container_gis_enum_by_name(container, mergetype_key,
                                   &args->merge_type);
    mark_sanitize_args(args);
}

static void
mark_save_args(GwyContainer *container,
               MarkArgs *args)
{
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    gwy_container_set_boolean_by_name(container, isheight_key, args->is_height);
    gwy_container_set_boolean_by_name(container, isslope_key, args->is_slope);
    gwy_container_set_boolean_by_name(container, islap_key, args->is_lap);
    gwy_container_set_double_by_name(container, height_key, args->height);
    gwy_container_set_double_by_name(container, slope_key, args->slope);
    gwy_container_set_double_by_name(container, lap_key, args->lap);
    gwy_container_set_enum_by_name(container, mergetype_key, args->merge_type);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
