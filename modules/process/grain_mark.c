/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)
    
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
} MarkControls;

static gboolean    module_register            (const gchar *name);
static gboolean    mark                        (GwyContainer *data,
                                               GwyRunType run);
static gboolean    mark_dialog                 (MarkArgs *args, GwyContainer *data);
static void        height_changed_cb          (GObject *item,
                                               MarkArgs *args);
static void        inverted_changed_cb        (GtkToggleButton *button,
                                               MarkArgs *args);
static void        isheight_changed_cb        (GtkToggleButton *button,
                                               MarkArgs *args);
static void        isslope_changed_cb          (GtkToggleButton *button,
                                               MarkArgs *args);
static void        islap_changed_cb            (GtkToggleButton *button,
                                               MarkArgs *args);
static void        merge_changed_cb            (GObject *item,
                                               MarkControls *controls);
static void        mark_load_args              (GwyContainer *container,
                                               MarkArgs *args);
static void        mark_save_args              (GwyContainer *container,
                                               MarkArgs *args);
static void        mark_dialog_update          (MarkControls *controls,
                                               MarkArgs *args);
static void        preview                     (MarkControls *controls,
                                               MarkArgs *args);


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
    "1.0",
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
        "/_Grains/_Mark by threshold...",
        (GwyProcessFunc)&mark,
        MARK_RUN_MODES,
    };

    gwy_process_func_register(name, &mark_func_info);

    return TRUE;
}

static gboolean
mark(GwyContainer *data, GwyRunType run)
{
    GtkWidget *data_window;
    GwyDataField *dfield;
    MarkArgs args;
    gboolean ok;
    gint i;
    gint xsize, ysize;
    gint newsize;

    g_assert(run & MARK_RUN_MODES);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = mark_defaults;
    else
        mark_load_args(gwy_app_settings_get(), &args);
    ok = (run != GWY_RUN_MODAL) || mark_dialog(&args, data);
    if (ok) {
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
        g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
        gwy_app_clean_up_data(data);
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/data"));

        xsize = gwy_data_field_get_xres(dfield);
        ysize = gwy_data_field_get_yres(dfield);

        if (run != GWY_RUN_WITH_DEFAULTS)
            mark_save_args(gwy_app_settings_get(), &args);
    }

    return ok;
}


static gboolean
mark_dialog(MarkArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table;
    MarkControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GtkHBox *hbox;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Mark grains by threshold"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         _("Update preview"), RESPONSE_PREVIEW,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 2); 
    
    table = gtk_table_new(3, 9, FALSE);
    
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox,
                       FALSE, FALSE, 4);    

    controls.view = gwy_data_view_new(data);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    zoomval = 400.0/(gdouble)gwy_data_field_get_xres(dfield);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);
    
    gtk_box_pack_start(GTK_BOX(hbox), controls.view,
                       FALSE, FALSE, 4);    

    gtk_box_pack_start(GTK_BOX(hbox), table,
                       FALSE, FALSE, 4);


    controls.is_height = gtk_check_button_new_with_label("Threshold by height:");
    if (args->height) gtk_toggle_button_set_active(controls.is_height, TRUE);
    else gtk_toggle_button_set_active(controls.is_height, FALSE);
    g_signal_connect(controls.is_height, "toggled", G_CALLBACK(isheight_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.is_height, 0, 1, 1, 2, GTK_FILL, 0, 2, 2);
                
    controls.threshold_height = gtk_adjustment_new(args->height, 0.0, 1000.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, 2, _("Height value"), _(""),
                                controls.threshold_height);

    controls.is_slope = gtk_check_button_new_with_label("Threshold by slope:");
    if (args->slope) gtk_toggle_button_set_active(controls.is_slope, TRUE);
    else gtk_toggle_button_set_active(controls.is_slope, FALSE);
    g_signal_connect(controls.is_slope, "toggled", G_CALLBACK(isslope_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.is_slope, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);
                
    controls.threshold_slope = gtk_adjustment_new(args->slope, 0.0, 1000.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, 4, _("Slope value"), _(""),
                                controls.threshold_slope);

    controls.is_lap = gtk_check_button_new_with_label("Threshold by curvature:");
    if (args->lap) gtk_toggle_button_set_active(controls.is_lap, TRUE);
    else gtk_toggle_button_set_active(controls.is_lap, FALSE);
    g_signal_connect(controls.is_lap, "toggled", G_CALLBACK(islap_changed_cb), &controls);
    gtk_table_attach(GTK_TABLE(table), controls.is_lap, 0, 1, 5, 6, GTK_FILL, 0, 2, 2);
                
    controls.threshold_lap = gtk_adjustment_new(args->lap, 0.0, 1000.0, 1, 10, 0);
    gwy_table_attach_spinbutton(table, 6, _("Curvature value"), _(""),
                                controls.threshold_lap);

   
    label = gtk_label_new(_("Merge mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8, GTK_FILL, 0, 2, 2); 
    
    controls.merge = gwy_option_menu_mergegrain(G_CALLBACK(merge_changed_cb),
                                            args, args->merge_type);
    gtk_table_attach(GTK_TABLE(table), controls.merge, 0, 1, 8, 9, GTK_FILL, 0, 2, 2);
    

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

    args->height = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_height));
    args->slope = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_slope));
    args->lap = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_lap));
    gtk_widget_destroy(dialog);

    return TRUE;
}


static void
inverted_changed_cb(GtkToggleButton *button, MarkArgs *args)
{
    args->inverted = gtk_toggle_button_get_active(button);
}

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

static const gchar *isheight_key = "/module/mark_height/isheight";
static const gchar *isslope_key = "/module/mark_height/isslope";
static const gchar *islap_key = "/module/mark_height/islap";
static const gchar *inverted_key = "/module/mark_height/inverted";
static const gchar *height_key = "/module/mark_height/height";
static const gchar *slope_key = "/module/mark_height/slope";
static const gchar *lap_key = "/module/mark_height/lap";


static void
mark_load_args(GwyContainer *container,
                 MarkArgs *args)
{
    *args = mark_defaults;

    if (gwy_container_contains_by_name(container, inverted_key))
        args->inverted = gwy_container_get_boolean_by_name(container, inverted_key);
    if (gwy_container_contains_by_name(container, isheight_key))
        args->is_height = gwy_container_get_boolean_by_name(container, isheight_key);    
    if (gwy_container_contains_by_name(container, isslope_key))
        args->is_slope = gwy_container_get_boolean_by_name(container, isslope_key);    
    if (gwy_container_contains_by_name(container, islap_key))
        args->is_lap = gwy_container_get_boolean_by_name(container, islap_key);    
    if (gwy_container_contains_by_name(container, height_key))
        args->height = gwy_container_get_double_by_name(container, height_key);
    if (gwy_container_contains_by_name(container, slope_key))
        args->slope = gwy_container_get_double_by_name(container, slope_key);
    if (gwy_container_contains_by_name(container, lap_key))
        args->lap = gwy_container_get_double_by_name(container, lap_key);
    
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
}

static void
preview(MarkControls *controls,
        MarkArgs *args)
{
    printf("preview!\n");
}

static void        
merge_changed_cb(GObject *item, MarkControls *controls)
{
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
