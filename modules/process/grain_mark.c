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
#include <stdio.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define MARK_RUN_MODES \
    (GWY_RUN_MODAL)

#define MARK_HEIGHT 0
#define MARK_SLOPE 1
#define MARK_LAP 2

/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
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
} MarkControls;

static gboolean    module_register            (const gchar *name);
static gboolean    mark                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    mark_dialog                 (MarkArgs *args, GwyContainer *data);
/*
static void        inverted_changed_cb        (GtkToggleButton *button,
                                               MarkArgs *args);
*/
static void        isheight_changed_cb        (GtkToggleButton *button,
                                               MarkArgs *args);
static void        isslope_changed_cb          (GtkToggleButton *button,
                                               MarkArgs *args);
static void        islap_changed_cb            (GtkToggleButton *button,
                                               MarkArgs *args);
static void        merge_changed_cb            (GObject *item,
                                               MarkArgs *args);
static void        mask_color_changed_cb      (GtkWidget *color_button,
                                               MarkControls *controls);
static void        load_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        save_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        mark_dialog_update          (MarkControls *controls,
                                               MarkArgs *args);
static void        preview                     (MarkControls *controls,
                                               MarkArgs *args);
static void        ok                         (MarkControls *controls,
                                               MarkArgs *args,
                                               GwyContainer *data);
static void        mask_process               (GwyDataField *dfield,
                                               GwyDataField *maskfield,
                                               MarkArgs *args,
                                               MarkControls *controls);
static void        mark_load_args              (GwyContainer *container,
                                               MarkArgs *args);
static void        mark_save_args              (GwyContainer *container,
                                               MarkArgs *args);
static void        mark_sanitize_args         (MarkArgs *args);


MarkArgs mark_defaults = {
    MARK_HEIGHT,
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
    "mark_threshold",
    "Mark grains by thresholding",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.1",
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
        "/_Grains/_Mark by Threshold...",
        (GwyProcessFunc)&mark,
        MARK_RUN_MODES,
    };

    gwy_process_func_register(name, &mark_func_info);

    return TRUE;
}

static gboolean
mark(GwyContainer *data, GwyRunType run)
{
    MarkArgs args;
    gboolean ook;

    g_assert(run & MARK_RUN_MODES);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = mark_defaults;
    else
        mark_load_args(gwy_app_settings_get(), &args);
    ook = (run != GWY_RUN_MODAL) || mark_dialog(&args, data);
    if (ook) {

        if (run != GWY_RUN_WITH_DEFAULTS)
            mark_save_args(gwy_app_settings_get(), &args);
    }

    return ook;
}


static gboolean
mark_dialog(MarkArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *spin;
    MarkControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Mark grains by threshold"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Update preview"), RESPONSE_PREVIEW,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 2);

    table = gtk_table_new(10, 3, FALSE);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.mydata = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata,
                                                             "/0/data"));

    if (gwy_data_field_get_xres(dfield) >= gwy_data_field_get_yres(dfield))
        zoomval = 400.0/(gdouble)gwy_data_field_get_xres(dfield);
    else
        zoomval = 400.0/(gdouble)gwy_data_field_get_yres(dfield);

    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);


    controls.is_height
        = gtk_check_button_new_with_label(_("Threshold by height:"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height),
                                 args->is_height);

    g_signal_connect(controls.is_height, "toggled",
                     G_CALLBACK(isheight_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.is_height,
                     0, 1, 1, 2, GTK_FILL, 0, 2, 2);

    controls.threshold_height = gtk_adjustment_new(args->height,
                                                   0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 2, _("Height value [fractile]"), _(""),
                                controls.threshold_height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    controls.is_slope
        = gtk_check_button_new_with_label(_("Threshold by slope:"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_slope),
                                 args->is_slope);
    g_signal_connect(controls.is_slope, "toggled",
                     G_CALLBACK(isslope_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.is_slope,
                     0, 1, 3, 4, GTK_FILL, 0, 2, 2);

    controls.threshold_slope = gtk_adjustment_new(args->slope,
                                                  0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 4, _("Slope value [fractile]"), _(""),
                                controls.threshold_slope);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    
    controls.is_lap
        = gtk_check_button_new_with_label(_("Threshold by curvature:"));
    g_signal_connect(controls.is_lap, "toggled",
                     G_CALLBACK(islap_changed_cb), args);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_lap),
                                 args->is_lap);
    gtk_table_attach(GTK_TABLE(table), controls.is_lap,
                     0, 1, 5, 6, GTK_FILL, 0, 2, 2);

    controls.threshold_lap = gtk_adjustment_new(args->lap,
                                                0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 6, _("Curvature value [fractile]"), _(""),
                                controls.threshold_lap);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    label = gtk_label_new(_("Merge mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8, GTK_FILL, 0, 2, 2);

    controls.merge = gwy_option_menu_mergegrain(G_CALLBACK(merge_changed_cb),
                                            args, args->merge_type);
    gtk_table_attach(GTK_TABLE(table), controls.merge, 0, 1, 8, 9, GTK_FILL, 0, 2, 2);
    gtk_table_set_row_spacing(GTK_TABLE(table), 9, 8);

    label = gtk_label_new_with_mnemonic(_("Preview _mask color:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label,  0, 1, 9, 10, GTK_FILL, 0, 2, 2);
    controls.color_button = gwy_color_button_new();
    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(controls.color_button),
                                   TRUE);
    load_mask_color(controls.color_button,
                    gwy_data_view_get_data(GWY_DATA_VIEW(controls.view)));
    gtk_table_attach(GTK_TABLE(table), controls.color_button,
                     1, 2, 9, 10, GTK_FILL, 0, 2, 2);

    g_signal_connect(controls.color_button, "color_set",
                     G_CALLBACK(mask_color_changed_cb), &controls);

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
            save_mask_color(controls.color_button, data);
            ok(&controls, args, data);
            break;

            case RESPONSE_RESET:
            *args = mark_defaults;
            mark_dialog_update(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->height
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_height));
    args->slope
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_slope));
    args->lap
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_lap));
    gtk_widget_destroy(dialog);

    return TRUE;
}

/*
static void
inverted_changed_cb(GtkToggleButton *button, MarkArgs *args)
{
    args->inverted = gtk_toggle_button_get_active(button);
}
*/

static void
isheight_changed_cb(GtkToggleButton *button, MarkArgs *args)
{
    args->is_height = gtk_toggle_button_get_active(button);
}

static void
isslope_changed_cb(GtkToggleButton *button, MarkArgs *args)
{
    args->is_slope = gtk_toggle_button_get_active(button);
}
static void
islap_changed_cb(GtkToggleButton *button, MarkArgs *args)
{
    args->is_lap = gtk_toggle_button_get_active(button);
}

static void
mask_color_changed_cb(GtkWidget *color_button,
                      MarkControls *controls)
{
    GwyContainer *data;
    GdkColor color;
    guint16 alpha;
    const gdouble f = 65535.0;

    gwy_color_button_get_color(GWY_COLOR_BUTTON(color_button), &color);
    alpha = gwy_color_button_get_alpha(GWY_COLOR_BUTTON(color_button));
    gwy_debug("(%u, %u, %u, %u)", color.red, color.green, color.blue, alpha);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(controls->view));
    gwy_container_set_double_by_name(data, "/0/mask/red", color.red/f);
    gwy_container_set_double_by_name(data, "/0/mask/green", color.green/f);
    gwy_container_set_double_by_name(data, "/0/mask/blue", color.blue/f);
    gwy_container_set_double_by_name(data, "/0/mask/alpha", alpha/f);
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
load_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GwyContainer *settings;
    GdkColor color;
    guint16 alpha;
    GwyRGBA rgba;
    const gdouble f = 65535.0;

    settings = gwy_app_settings_get();
    gwy_debug("has alpha in settings: %s",
              gwy_container_contains_by_name(settings, "/mask/alpha")
              ? "TRUE" : "FALSE");
    gwy_container_gis_double_by_name(settings, "/mask/red", &rgba.r);
    gwy_container_gis_double_by_name(settings, "/mask/green", &rgba.g);
    gwy_container_gis_double_by_name(settings, "/mask/blue", &rgba.b);
    gwy_container_gis_double_by_name(settings, "/mask/alpha", &rgba.a);

    gwy_debug("has alpha in data: %s",
              gwy_container_contains_by_name(data, "/0/mask/alpha")
              ? "TRUE" : "FALSE");
    gwy_container_gis_double_by_name(data, "/0/mask/red", &rgba.r);
    gwy_container_gis_double_by_name(data, "/0/mask/green", &rgba.g);
    gwy_container_gis_double_by_name(data, "/0/mask/blue", &rgba.b);
    gwy_container_gis_double_by_name(data, "/0/mask/alpha", &rgba.a);

    gwy_debug("(%g, %g, %g, %g)", rgba.r, rgba.g, rgba.b, rgba.a);
    color.red = f*rgba.r;
    color.green = f*rgba.g;
    color.blue = f*rgba.b;
    alpha = f*rgba.a;
    gwy_debug("(%u, %u, %u, %u)", color.red, color.green, color.blue, alpha);

    gwy_color_button_set_color(GWY_COLOR_BUTTON(color_button), &color);
    gwy_color_button_set_alpha(GWY_COLOR_BUTTON(color_button), alpha);
}

static void
save_mask_color(GtkWidget *color_button,
                GwyContainer *data)
{
    GdkColor color;
    guint16 alpha;
    const gdouble f = 65535.0;

    gwy_color_button_get_color(GWY_COLOR_BUTTON(color_button), &color);
    alpha = gwy_color_button_get_alpha(GWY_COLOR_BUTTON(color_button));
    gwy_debug("(%u, %u, %u, %u)", color.red, color.green, color.blue, alpha);

    gwy_container_set_double_by_name(data, "/0/mask/red", color.red/f);
    gwy_container_set_double_by_name(data, "/0/mask/green", color.green/f);
    gwy_container_set_double_by_name(data, "/0/mask/blue", color.blue/f);
    gwy_container_set_double_by_name(data, "/0/mask/alpha", alpha/f);
}

static void
mark_dialog_update(MarkControls *controls,
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
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_slope),
                                 args->is_slope);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_lap),
                                 args->is_lap);

}

static void
preview(MarkControls *controls,
        MarkArgs *args)
{
    GwyDataField *maskfield, *dfield;
    GwyPixmapLayer *layer;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

    /*set up the mask*/
    if (gwy_container_contains_by_name(controls->mydata, "/0/mask")) {
        maskfield
            = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                              "/0/mask"));
        gwy_data_field_resample(maskfield,
                               gwy_data_field_get_xres(dfield),
                               gwy_data_field_get_yres(dfield),
                               GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(dfield, maskfield);
        if (!gwy_data_view_get_alpha_layer(GWY_DATA_VIEW(controls->view))) {
            layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
            gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                          GWY_PIXMAP_LAYER(layer));
        }
    }
    else {
        maskfield = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(controls->mydata, "/0/mask",
                                         G_OBJECT(maskfield));
        layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                 GWY_PIXMAP_LAYER(layer));

    }

    mask_process(dfield, maskfield, args, controls);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

}

static void
ok(MarkControls *controls,
   MarkArgs *args,
   GwyContainer *data)
{

    GwyDataField *dfield, *maskfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    if (gwy_container_contains_by_name(data, "/0/mask")) {
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                  "/0/mask"));
        gwy_data_field_resample(maskfield,
                               gwy_data_field_get_xres(dfield),
                               gwy_data_field_get_yres(dfield),
                               GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(dfield, maskfield);
    }
    else {
        maskfield = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(maskfield));

    }

    mask_process(dfield, maskfield, args, controls);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
merge_changed_cb(GObject *item, MarkArgs *args)
{
    args->merge_type = GPOINTER_TO_INT(g_object_get_data(item,
                                                        "mergegrain-type"));

}


static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             MarkArgs *args,
             MarkControls *controls)
{
    GwyDataField *output_field;
    gboolean is_field;

    is_field = FALSE;
    output_field = GWY_DATA_FIELD(gwy_data_field_new
                                      (gwy_data_field_get_xres(dfield),
                                       gwy_data_field_get_yres(dfield),
                                       gwy_data_field_get_xreal(dfield),
                                       gwy_data_field_get_yreal(dfield),
                                       FALSE));

    args->height
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_height));
    args->slope
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_slope));
    args->lap
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_lap));

    args->inverted = 0;
    if (args->is_height) {
        gwy_data_field_grains_mark_height(dfield, maskfield,
                                          args->height, args->inverted);
        is_field = TRUE;
    }
    if (args->is_slope) {
        gwy_data_field_grains_mark_slope(dfield, output_field,
                                         args->slope, args->inverted);
        if (is_field) {
            if (args->merge_type == GWY_MERGE_UNION)
                gwy_data_field_grains_add(maskfield, output_field);
            else if (args->merge_type == GWY_MERGE_INTERSECTION)
                gwy_data_field_grains_intersect(maskfield, output_field);
        }
        else gwy_data_field_copy(output_field, maskfield);
        is_field = TRUE;
    }
    if (args->is_lap) {
        gwy_data_field_grains_mark_curvature(dfield, output_field,
                                             args->lap, args->inverted);
        if (is_field) {
            if (args->merge_type == GWY_MERGE_UNION)
                gwy_data_field_grains_add(maskfield, output_field);
            else if (args->merge_type == GWY_MERGE_INTERSECTION)
                gwy_data_field_grains_intersect(maskfield, output_field);
        }
        else gwy_data_field_copy(output_field, maskfield);
     }

    g_object_unref(output_field);
}

static const gchar *isheight_key = "/module/mark_height/isheight";
static const gchar *isslope_key = "/module/mark_height/isslope";
static const gchar *islap_key = "/module/mark_height/islap";
static const gchar *inverted_key = "/module/mark_height/inverted";
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
