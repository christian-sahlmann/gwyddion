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


#define REMOVE_RUN_MODES \
    (GWY_RUN_MODAL | GWY_RUN_NONINTERACTIVE | GWY_RUN_WITH_DEFAULTS)


/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gdouble area;
    gdouble height;
    gboolean is_height;
    gboolean is_area;
    GwyMergeType merge_type;
} RemoveArgs;

typedef struct {
    GtkWidget *view;
    GtkWidget *is_height;
    GtkWidget *is_area;
    GtkObject *threshold_height;
    GtkObject *threshold_area;
    GtkWidget *merge;
    GwyContainer *mydata;
} RemoveControls;

static gboolean    module_register            (const gchar *name);
static gboolean    remove_th                  (GwyContainer *data,
                                               GwyRunType run);
static gboolean    remove_dialog              (RemoveArgs *args, GwyContainer *data);
static void        isheight_changed_cb        (GtkToggleButton *button,
                                               RemoveArgs *args);
static void        isarea_changed_cb          (GtkToggleButton *button,
                                               RemoveArgs *args);
static void        merge_changed_cb            (GObject *item,
                                               RemoveArgs *args);
static void        remove_load_args              (GwyContainer *container,
                                               RemoveArgs *args);
static void        remove_save_args              (GwyContainer *container,
                                               RemoveArgs *args);
static void        remove_dialog_update          (RemoveControls *controls,
                                               RemoveArgs *args);
static void        preview                     (RemoveControls *controls,
                                               RemoveArgs *args,
                                               GwyContainer *data);
static void        ok                         (RemoveControls *controls,
                                               RemoveArgs *args,
                                               GwyContainer *data);
static void        mask_process               (GwyDataField *dfield,
                                               GwyDataField *maskfield,
                                               RemoveArgs *args,
                                               RemoveControls *controls);
static void        intersect_removes          (GwyDataField *mask_a,
                                               GwyDataField *mask_b,
                                               GwyDataField *mask);

RemoveArgs remove_defaults = {
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
    "Remove grains by thresholding",
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
    static GwyProcessFuncInfo remove_func_info = {
        "remove_threshold",
        "/_Grains/_Remove by threshold...",
        (GwyProcessFunc)&remove_th,
        REMOVE_RUN_MODES,
    };

    gwy_process_func_register(name, &remove_func_info);
    gwy_process_func_set_sensitivity_flags(remove_func_info.name,
                                           GWY_MENU_FLAG_DATA_MASK);

    return TRUE;
}

static gboolean
remove_th(GwyContainer *data, GwyRunType run)
{
    RemoveArgs args;
    gboolean ook = FALSE;

    g_assert(run & REMOVE_RUN_MODES);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = remove_defaults;
    else
        remove_load_args(gwy_app_settings_get(), &args);

    if (gwy_container_contains_by_name(data, "/0/mask")) {
        ook = (run != GWY_RUN_MODAL) || remove_dialog(&args, data);
        if (ook) {

            if (run != GWY_RUN_WITH_DEFAULTS)
                remove_save_args(gwy_app_settings_get(), &args);
        }
    }

    return ook;
}


static gboolean
remove_dialog(RemoveArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table;
    RemoveControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;
    GtkWidget *label;

    dialog = gtk_dialog_new_with_buttons(_("Remove grains by threshold"),
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

    controls.mydata = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata, "/0/data"));
    zoomval = 400.0/(gdouble)gwy_data_field_get_xres(dfield);
    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    layer = gwy_layer_mask_new();
    gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));

    gtk_box_pack_start(GTK_BOX(hbox), controls.view,
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(hbox), table,
                       FALSE, FALSE, 4);


    controls.is_height = gtk_check_button_new_with_label("Threshold by maximum:");
    if (args->height) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height), TRUE);
    else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height), FALSE);
    g_signal_connect(controls.is_height, "toggled", G_CALLBACK(isheight_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.is_height, 0, 1, 1, 2, GTK_FILL, 0, 2, 2);

    controls.threshold_height = gtk_adjustment_new(args->height, 0.0, 100.0, 0.1, 5, 0);
    gwy_table_attach_spinbutton(table, 2, _("Height value [fractile]"), _(""),
                                controls.threshold_height);

    controls.is_area = gtk_check_button_new_with_label("Threshold by area:");
    if (args->area) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_area), TRUE);
    else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_area), FALSE);
    g_signal_connect(controls.is_area, "toggled", G_CALLBACK(isarea_changed_cb), args);
    gtk_table_attach(GTK_TABLE(table), controls.is_area, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);

    controls.threshold_area = gtk_adjustment_new(args->area, 0.0, 100.0, 0.1, 5, 0);
    gwy_table_attach_spinbutton(table, 4, _("Area [pixels]"), _(""),
                                controls.threshold_area);


    label = gtk_label_new(_("Selection mode:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8, GTK_FILL, 0, 2, 2);

    controls.merge = gwy_option_menu_mergegrain(G_CALLBACK(merge_changed_cb),
                                            args, args->merge_type);
    gtk_table_attach(GTK_TABLE(table), controls.merge, 0, 1, 8, 9, GTK_FILL, 0, 2, 2);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_height), args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls.is_area), args->is_area);

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
            ok(&controls, args, data);
            break;

            case RESPONSE_RESET:
            *args = remove_defaults;
            remove_dialog_update(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            preview(&controls, args, data);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->height = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_height));
    args->area = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.threshold_area));
    gtk_widget_destroy(dialog);

    return TRUE;
}



static void
isheight_changed_cb(GtkToggleButton *button, RemoveArgs *args)
{
    args->is_height = gtk_toggle_button_get_active(button);
}

static void
isarea_changed_cb(GtkToggleButton *button, RemoveArgs *args)
{
    args->is_area = gtk_toggle_button_get_active(button);
}

static const gchar *isheight_key = "/module/remove_threshold/isheight";
static const gchar *isarea_key = "/module/remove_threshold/isarea";
static const gchar *height_key = "/module/remove_threshold/height";
static const gchar *area_key = "/module/remove_threshold/area";
static const gchar *mergetype_key = "/module/remove_threshold/mergetype";


static void
remove_load_args(GwyContainer *container,
                 RemoveArgs *args)
{
    *args = remove_defaults;

    if (gwy_container_contains_by_name(container, isheight_key))
        args->is_height = gwy_container_get_boolean_by_name(container, isheight_key);
    if (gwy_container_contains_by_name(container, isarea_key))
        args->is_area = gwy_container_get_boolean_by_name(container, isarea_key);
    if (gwy_container_contains_by_name(container, height_key))
        args->height = gwy_container_get_double_by_name(container, height_key);
    if (gwy_container_contains_by_name(container, area_key))
        args->area = gwy_container_get_double_by_name(container, area_key);
    if (gwy_container_contains_by_name(container, mergetype_key))
        args->merge_type = gwy_container_get_int32_by_name(container, mergetype_key);

}

static void
remove_save_args(GwyContainer *container,
                 RemoveArgs *args)
{
    gwy_container_set_boolean_by_name(container, isheight_key, args->is_height);
    gwy_container_set_boolean_by_name(container, isarea_key, args->is_area);
    gwy_container_set_double_by_name(container, height_key, args->height);
    gwy_container_set_double_by_name(container, area_key, args->area);
    gwy_container_set_int32_by_name(container, mergetype_key, args->merge_type);
}

static void
remove_dialog_update(RemoveControls *controls,
                     RemoveArgs *args)
{
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_height),
                                args->height);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->threshold_area),
                                args->area);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_height), args->is_height);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(controls->is_area), args->is_area);

}

static void
preview(RemoveControls *controls,
        RemoveArgs *args,
        GwyContainer *data)
{
    GwyDataField *maskfield, *dfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata, "/0/data"));

    /*set up the mask*/
    if (gwy_container_contains_by_name(controls->mydata, "/0/mask"))
    {
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata, "/0/mask"));

        gwy_data_field_copy(GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                  "/0/mask")), maskfield);
        mask_process(dfield, maskfield, args, controls);

        gwy_container_set_object_by_name(controls->mydata, "/0/mask", G_OBJECT(maskfield));

    }
    else
    {

    }
    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

}

static void
ok(RemoveControls *controls,
        RemoveArgs *args,
        GwyContainer *data)
{

    GwyDataField *dfield, *maskfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));


    if (gwy_container_contains_by_name(data, "/0/mask"))
    {
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                  "/0/mask"));
        mask_process(dfield, maskfield, args, controls);
    }
    else
    {
    }

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}

static void
merge_changed_cb(GObject *item, RemoveArgs *args)
{
    args->merge_type = GPOINTER_TO_INT(g_object_get_data(item,
                                                        "mergegrain-type"));

}


static void
mask_process(GwyDataField *dfield,
             GwyDataField *maskfield,
             RemoveArgs *args,
             RemoveControls *controls)
{
    GwyDataField *output_field_a, *output_field_b;

    args->height
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_height));
    args->area
        = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->threshold_area));

    if (args->merge_type == GWY_MERGE_UNION
        || (args->is_height*args->is_area) == 0) {
        if (args->is_height)
            gwy_data_field_grains_remove_by_height(dfield, maskfield,
                                                   args->height, 0);
        if (args->is_area)
            gwy_data_field_grains_remove_by_size(maskfield, args->area);
    }
    else {
        output_field_a
            = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(maskfield)));
        output_field_b
            = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(maskfield)));

         gwy_data_field_grains_remove_by_height(dfield, output_field_a,
                                                args->height, 0);
         gwy_data_field_grains_remove_by_size(output_field_b, args->area);

         intersect_removes(output_field_a, output_field_b, maskfield);

         g_object_unref(output_field_a);
         g_object_unref(output_field_b);
    }

}

static void
intersect_removes(GwyDataField *mask_a, GwyDataField *mask_b, GwyDataField *mask)
{
    gint i, xres, yres;
    xres = mask->xres;
    yres = mask->yres;

    for (i=0; i<(xres*yres); i++)
    {
        if (mask->data[i]>0 && mask_a->data[i]==0 && mask_b->data[i]==0)
            gwy_data_field_grains_remove_manually(mask, i);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
