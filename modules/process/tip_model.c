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
#include <app/gwyapp.h>

#define TIP_MODEL_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)

typedef enum {
    TIP_MODEL_CONTACT = 0,
    TIP_MODEL_NONCONTACT  = 1,
    TIP_MODEL_LAP    = 2
} TipType;

/* Data for this function. */
typedef struct {
    gdouble height;
    TipType type;
    GwyDataWindow *win;
} TipModelArgs;

typedef struct {
    GtkWidget *view;
    GtkWidget *data;
    GwyValUnit *real;
    GtkObject *res;
    GtkObject *type;
    GtkObject *radius;
    GtkObject *slope;
    GwyContainer *tip;
} TipModelControls;

static gboolean    module_register            (const gchar *name);
static gboolean    tip_model                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    tip_model_dialog                 (TipModelArgs *args, GwyContainer *data);
static void        tip_model_dialog_update_controls(TipModelControls *controls,
                                               TipModelArgs *args);
static void        tip_model_dialog_update_values  (TipModelControls *controls,
                                               TipModelArgs *args);
static void        preview                    (TipModelControls *controls,
                                               TipModelArgs *args);
static void        tip_model_do                    (TipModelArgs *args,
                                               GwyContainer *data);
static void        mask_process               (GwyDataField *dfield,
                                               GwyDataField *maskfield,
                                               TipModelArgs *args);
static void        tip_model_load_args              (GwyContainer *container,
                                               TipModelArgs *args);
static void        tip_model_save_args              (GwyContainer *container,
                                               TipModelArgs *args);
static void        tip_model_sanitize_args         (TipModelArgs *args);
static GtkWidget*  tip_model_data_option_menu      (GwyDataWindow **operand);
static void        tip_model_data_cb(GtkWidget *item);
    


TipModelArgs tip_model_defaults = {
    0,
    0,
    NULL,
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "tip_model",
    "Model SPM tip",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo tip_model_func_info = {
        "tip_model",
        "/_Tip operations/_Model Tip...",
        (GwyProcessFunc)&tip_model,
        TIP_MODEL_RUN_MODES,
        0,
    };

    gwy_process_func_register(name, &tip_model_func_info);

    return TRUE;
}

static gboolean
tip_model(GwyContainer *data, GwyRunType run)
{
    TipModelArgs args;
    gboolean ok = FALSE;

    g_assert(run & TIP_MODEL_RUN_MODES);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = tip_model_defaults;
    else
        tip_model_load_args(gwy_app_settings_get(), &args);

    ok = (run != GWY_RUN_MODAL) || tip_model_dialog(&args, data);
    if (run == GWY_RUN_MODAL)
        tip_model_save_args(gwy_app_settings_get(), &args);
    if (!ok)
        return FALSE;

    tip_model_do(&args, data);

    return ok;
}


static gboolean
tip_model_dialog(TipModelArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *spin, *omenu;
    TipModelControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response, col, row;
    gdouble zoomval;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Model tip"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Update preview"), RESPONSE_PREVIEW,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 3);

    table = gtk_table_new(10, 3, FALSE);
    col = 0; 
    row = 0;
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);

    controls.tip = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    controls.view = gwy_data_view_new(controls.tip);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));

    gtk_box_pack_start(GTK_BOX(hbox), controls.view, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), table, FALSE, FALSE, 4);

    label = gtk_label_new(_("Related data:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1, GTK_FILL, 0, 2, 2);

 
    omenu = tip_model_data_option_menu(&args->win);
    gtk_table_attach_defaults(GTK_TABLE(table), omenu, 1, 2, row, row+1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), omenu);
    gtk_table_set_row_spacing(GTK_TABLE(table), row, 4);
    row++;
    
/*
    controls.real = gtk_adjustment_new(args->real,
                                                   0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 2, _("Height value"), "%",
                                       controls.threshold_height);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    controls.res = gtk_adjustment_new(args->slope,
                                                  0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 4, _("Slope value"), "%",
                                       controls.threshold_slope);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    controls.is_lap
        = gtk_check_button_new_with_label(_("Threshold by curvature:"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_lap),
                                 args->is_lap);
    gtk_table_attach(GTK_TABLE(table), controls.is_lap,
                     0, 1, 5, 6, GTK_FILL, 0, 2, 2);

    controls.threshold_lap = gtk_adjustment_new(args->lap,
                                                0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 6, _("Curvature value"), "%",
                                       controls.threshold_lap);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);

    label = gtk_label_new(_("Merge mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8, GTK_FILL, 0, 2, 2);

    controls.merge = gwy_option_menu_mergegrain(NULL, NULL, args->merge_type);
    gtk_table_attach(GTK_TABLE(table), controls.merge,
                     0, 1, 8, 9, GTK_FILL, 0, 2, 2);
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
*/

    gtk_widget_show_all(dialog);
    do {
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        switch (response) {
            case GTK_RESPONSE_CANCEL:
            case GTK_RESPONSE_DELETE_EVENT:
            tip_model_dialog_update_values(&controls, args);
            gtk_widget_destroy(dialog);
            case GTK_RESPONSE_NONE:
            return FALSE;
            break;

            case GTK_RESPONSE_OK:
            break;

            case RESPONSE_RESET:
            *args = tip_model_defaults;
            tip_model_dialog_update_controls(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            tip_model_dialog_update_values(&controls, args);
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    tip_model_dialog_update_values(&controls, args);
    gtk_widget_destroy(dialog);

    return TRUE;
}


static GtkWidget*
tip_model_data_option_menu(GwyDataWindow **operand)
{
    GtkWidget *omenu, *menu;

    omenu = gwy_option_menu_data_window(G_CALLBACK(tip_model_data_cb),
                                       NULL, NULL, GTK_WIDGET(*operand));
    menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(omenu));
    g_object_set_data(G_OBJECT(menu), "operand", operand);

    return omenu;
}

static void
tip_model_data_cb(GtkWidget *item)
{
    GtkWidget *menu;
    gpointer p, *pp;

    menu = gtk_widget_get_parent(item);

    p = g_object_get_data(G_OBJECT(item), "data-window");
    pp = (gpointer*)g_object_get_data(G_OBJECT(menu), "operand");
    g_return_if_fail(pp);
    *pp = p;
}



static void
tip_model_dialog_update_controls(TipModelControls *controls,
                            TipModelArgs *args)
{
/*    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_height),
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
    gwy_option_menu_set_history(controls->merge, "mergegrain-type",
                                args->merge_type);
                                */
}

static void
tip_model_dialog_update_values(TipModelControls *controls,
                          TipModelArgs *args)
{
    /*
    args->height
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_height));
    args->slope
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_slope));
    args->lap
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_lap));
    args->is_height
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_height));
    args->is_slope
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_slope));
    args->is_lap
        = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(controls->is_lap));
    args->merge_type
        = gwy_option_menu_get_history(controls->merge, "mergegrain-type");
        */
}


static void
preview(TipModelControls *controls,
        TipModelArgs *args)
{
    GwyDataField *maskfield, *dfield;
    GwyPixmapLayer *layer;
/*
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata,
                                                             "/0/data"));

  */  /*set up the mask*/
/*    if (gwy_container_contains_by_name(controls->mydata, "/0/mask")) {
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
        g_object_unref(maskfield);
        layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                 GWY_PIXMAP_LAYER(layer));

    }

    mask_process(dfield, maskfield, args);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
*/
}

static void
tip_model_do(TipModelArgs *args,
        GwyContainer *data)
{

    GwyDataField *dfield, *maskfield;
/*
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));

    gwy_app_undo_checkpoint(data, "/0/mask", NULL);
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
        maskfield
            = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(maskfield));
        g_object_unref(maskfield);
    }

    mask_process(dfield, maskfield, args);
    */
}

static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             TipModelArgs *args)
{
    GwyDataField *output_field;
    gboolean is_field;

    /*
    is_field = FALSE;
    output_field = GWY_DATA_FIELD(gwy_data_field_new
                                      (gwy_data_field_get_xres(dfield),
                                       gwy_data_field_get_yres(dfield),
                                       gwy_data_field_get_xreal(dfield),
                                       gwy_data_field_get_yreal(dfield),
                                       FALSE));

    args->inverted = 0;
    if (args->is_height) {
        gwy_data_field_grains_tip_model_height(dfield, maskfield,
                                          args->height, args->inverted);
        is_field = TRUE;
    }
    if (args->is_slope) {
        gwy_data_field_grains_tip_model_slope(dfield, output_field,
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
        gwy_data_field_grains_tip_model_curvature(dfield, output_field,
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
    */
}

static const gchar *mergetype_key = "/module/tip_model_height/merge_type";

static void
tip_model_sanitize_args(TipModelArgs *args)
{
    /*
    args->merge_type = MIN(args->merge_type, GWY_MERGE_INTERSECTION);
    */
}

static void
tip_model_load_args(GwyContainer *container,
               TipModelArgs *args)
{
    *args = tip_model_defaults;

    /*
    gwy_container_gis_boolean_by_name(container, islap_key, &args->is_lap);
    gwy_container_gis_double_by_name(container, height_key, &args->height);
    gwy_container_gis_double_by_name(container, slope_key, &args->slope);
    gwy_container_gis_double_by_name(container, lap_key, &args->lap);
    gwy_container_gis_enum_by_name(container, mergetype_key,
                                   &args->merge_type);
                                   
    tip_model_sanitize_args(args);
    */
}

static void
tip_model_save_args(GwyContainer *container,
               TipModelArgs *args)
{
    /*
    gwy_container_set_boolean_by_name(container, inverted_key, args->inverted);
    gwy_container_set_boolean_by_name(container, isheight_key, args->is_height);
    gwy_container_set_boolean_by_name(container, isslope_key, args->is_slope);
    gwy_container_set_boolean_by_name(container, islap_key, args->is_lap);
    gwy_container_set_double_by_name(container, height_key, args->height);
    gwy_container_set_double_by_name(container, slope_key, args->slope);
    gwy_container_set_double_by_name(container, lap_key, args->lap);
    gwy_container_set_enum_by_name(container, mergetype_key, args->merge_type);
    */
}
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
