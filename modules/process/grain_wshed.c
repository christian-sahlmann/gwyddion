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
#include <app/wait.h>

#define WSHED_RUN_MODES \
    (GWY_RUN_MODAL)


/* Data for this function.
 * (It looks a little bit silly with just one parameter.) */
typedef struct {
    gint locate_steps;
    gint locate_thresh;
    gint wshed_steps;
    gdouble locate_dropsize;
    gdouble wshed_dropsize;
} WshedArgs;

typedef struct {
    GtkWidget *view;
    GtkObject *entry_locate_steps;
    GtkObject *entry_locate_thresh;
    GtkObject *entry_wshed_steps;
    GtkObject *entry_locate_dropsize;
    GtkObject *entry_wshed_dropsize;
    GtkWidget *color_button;
    GwyContainer *mydata;
    gboolean computed;
} WshedControls;

static gboolean    module_register            (const gchar *name);
static gboolean    wshed                      (GwyContainer *data,
                                               GwyRunType run);
static gboolean    wshed_dialog               (WshedArgs *args,
                                               GwyContainer *data);
static void        mask_color_changed_cb      (GtkWidget *color_button,
                                               WshedControls *controls);
static void        load_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        save_mask_color            (GtkWidget *color_button,
                                               GwyContainer *data);
static void        wshed_dialog_update        (WshedControls *controls,
                                               WshedArgs *args);
static void        preview                    (WshedControls *controls,
                                               WshedArgs *args);
static void        ok                         (WshedControls *controls,
                                               WshedArgs *args,
                                               GwyContainer *data);
static void        mask_process               (GwyDataField *dfield,
                                               GwyDataField *maskfield,
                                               WshedArgs *args,
                                               WshedControls *controls);
static void        wshed_load_args            (GwyContainer *container,
                                               WshedArgs *args);
static void        wshed_save_args            (GwyContainer *container,
                                               WshedArgs *args);
static void        wshed_sanitize_args        (WshedArgs *args);


WshedArgs wshed_defaults = {
    10,
    3,
    10,
    10,
    1
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "wshed_threshold",
    "Mark grains by watershed algorithm",
    "Petr Klapetek <petr@klapetek.cz>",
    "1.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyProcessFuncInfo wshed_func_info = {
        "wshed_threshold",
        "/_Grains/Mark by _Watershed...",
        (GwyProcessFunc)&wshed,
        WSHED_RUN_MODES,
    };

    gwy_process_func_register(name, &wshed_func_info);

    return TRUE;
}

static gboolean
wshed(GwyContainer *data, GwyRunType run)
{
    WshedArgs args;
    gboolean ook;

    g_assert(run & WSHED_RUN_MODES);
    if (run == GWY_RUN_WITH_DEFAULTS)
        args = wshed_defaults;
    else
        wshed_load_args(gwy_app_settings_get(), &args);
    ook = (run != GWY_RUN_MODAL) || wshed_dialog(&args, data);
    if (ook) {

        if (run != GWY_RUN_WITH_DEFAULTS)
            wshed_save_args(gwy_app_settings_get(), &args);
    }

    return ook;
}


static gboolean
wshed_dialog(WshedArgs *args, GwyContainer *data)
{
    GtkWidget *dialog, *table, *label, *spin;
    WshedControls controls;
    enum { RESPONSE_RESET = 1,
           RESPONSE_PREVIEW = 2 };
    gint response;
    gdouble zoomval;
    GtkObject *layer;
    GtkWidget *hbox;
    GwyDataField *dfield;

    dialog = gtk_dialog_new_with_buttons(_("Mark grains by watershed algorithm"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Update preview"), RESPONSE_PREVIEW,
                                         _("Reset"), RESPONSE_RESET,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                                         NULL);

    hbox = gtk_hbox_new(FALSE, 2);

    table = gtk_table_new(3, 9, FALSE);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(hbox),
                       FALSE, FALSE, 4);

    controls.mydata = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    controls.view = gwy_data_view_new(controls.mydata);
    layer = gwy_layer_basic_new();
    gwy_data_view_set_base_layer(GWY_DATA_VIEW(controls.view),
                                 GWY_PIXMAP_LAYER(layer));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls.mydata, "/0/data"));

    if (gwy_data_field_get_xres(dfield) >= gwy_data_field_get_yres(dfield))
                zoomval = 400.0/(gdouble)gwy_data_field_get_xres(dfield);
    else zoomval = 400.0/(gdouble)gwy_data_field_get_yres(dfield);

    gwy_data_view_set_zoom(GWY_DATA_VIEW(controls.view), zoomval);

    gtk_box_pack_start(GTK_BOX(hbox), controls.view,
                       FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(hbox), table,
                       FALSE, FALSE, 4);


    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Grain location:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, 0, 2, 2);


    controls.entry_locate_steps = gtk_adjustment_new(args->locate_steps,
                                                     0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 2, _("Number of steps"), _(""),
                                controls.entry_locate_steps);
    controls.entry_locate_dropsize = gtk_adjustment_new(args->locate_dropsize,
                                                        0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 3, _("Drop size [fractile]"), _(""),
                                controls.entry_locate_dropsize);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    controls.entry_locate_thresh = gtk_adjustment_new(args->locate_thresh,
                                                      0.0, 100.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 4, _("Threshold [size in pixels]"), _(""),
                                controls.entry_locate_thresh);
    controls.entry_wshed_steps = gtk_adjustment_new(args->wshed_steps,
                                                    0.0, 1000.0, 1, 5, 0);
    gwy_table_attach_spinbutton(table, 6, _("Number of steps"), _(""),
                                controls.entry_wshed_steps);
    
    controls.entry_wshed_dropsize = gtk_adjustment_new(args->wshed_dropsize,
                                                       0.0, 100.0, 0.1, 5, 0);
    spin = gwy_table_attach_spinbutton(table, 7, _("Drop size [fractile]"), _(""),
                                controls.entry_wshed_dropsize);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), 2);
    
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Segmentation:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6, GTK_FILL, 0, 2, 2);

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

    controls.computed = FALSE;


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
            *args = wshed_defaults;
            wshed_dialog_update(&controls, args);
            break;

            case RESPONSE_PREVIEW:
            preview(&controls, args);
            break;

            default:
            g_assert_not_reached();
            break;
        }
    } while (response != GTK_RESPONSE_OK);

    args->locate_steps = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.entry_locate_steps));
    args->locate_thresh = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.entry_locate_thresh));
    args->locate_dropsize = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.entry_locate_dropsize));
    args->wshed_steps = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.entry_wshed_steps));
    args->wshed_dropsize = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.entry_wshed_dropsize));

    gtk_widget_destroy(dialog);

    return TRUE;
}

static void
mask_color_changed_cb(GtkWidget *color_button,
                      WshedControls *controls)
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
wshed_dialog_update(WshedControls *controls,
                     WshedArgs *args)
{

    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->entry_wshed_dropsize),
                             args->wshed_dropsize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->entry_locate_dropsize),
                             args->locate_dropsize);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->entry_locate_steps),
                             args->locate_steps);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->entry_wshed_steps),
                             args->wshed_steps);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(controls->entry_locate_thresh),
                             args->locate_thresh);
}

static void
preview(WshedControls *controls,
        WshedArgs *args)
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
                                          layer);
        }
    }
    else {
        maskfield
            = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(controls->mydata, "/0/mask",
                                         G_OBJECT(maskfield));
        layer = GWY_PIXMAP_LAYER(gwy_layer_mask_new());
        gwy_data_view_set_alpha_layer(GWY_DATA_VIEW(controls->view),
                                 layer);

    }

    mask_process(dfield, maskfield, args, controls);

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));

}

static void
ok(WshedControls *controls,
        WshedArgs *args,
        GwyContainer *data)
{

    GwyDataField *dfield, *maskfield;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));


    if (gwy_container_contains_by_name(data, "/0/mask"))
    {
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                  "/0/mask"));
        gwy_data_field_resample(maskfield,
                               gwy_data_field_get_xres(dfield),
                               gwy_data_field_get_yres(dfield),
                               GWY_INTERPOLATION_NONE);
        gwy_data_field_copy(dfield, maskfield);
    }
    else
    {
        maskfield = GWY_DATA_FIELD(gwy_serializable_duplicate(G_OBJECT(dfield)));
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(maskfield));

    }

    if (controls->computed == FALSE)
    {
        mask_process(dfield, maskfield, args, controls);
    }
    else
    {
        maskfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(controls->mydata, "/0/mask"));
        gwy_container_set_object_by_name(data, "/0/mask", G_OBJECT(maskfield));
    }

    gwy_data_view_update(GWY_DATA_VIEW(controls->view));
}


static void
mask_process(GwyDataField *dfield, GwyDataField *maskfield, WshedArgs *args, WshedControls *controls)
{
    gdouble max, min;
    GwyWatershedStatus status;

    max = gwy_data_field_get_max(dfield);
    min = gwy_data_field_get_min(dfield);

    args->locate_steps = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->entry_locate_steps));
    args->locate_thresh = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->entry_locate_thresh));
    args->locate_dropsize = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->entry_locate_dropsize));
    args->wshed_steps = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->entry_wshed_steps));
    args->wshed_dropsize = gtk_adjustment_get_value(GTK_ADJUSTMENT(controls->entry_wshed_dropsize));

    /*
    gwy_data_field_grains_mark_watershed(dfield, maskfield,
                                         args->locate_steps,
                                         args->locate_thresh,
                                         args->locate_dropsize*(max-min)/5000.0,
                                         args->wshed_steps,
                                         args->wshed_dropsize*(max-min)/5000.0,
                                         FALSE, 0);
    */
    status.state = GWY_WSHED_INIT;
    gwy_app_wait_start(GTK_WIDGET(gwy_app_data_window_get_current()),"Initializing...");
    do
    {
        gwy_data_field_grains_watershed_iteration(dfield, maskfield,
                                         &status,
                                         args->locate_steps,
                                         args->locate_thresh,
                                         args->locate_dropsize*(max-min)/5000.0,
                                         args->wshed_steps,
                                         args->wshed_dropsize*(max-min)/5000.0,
                                         FALSE, 0);

        if (status.state == GWY_WSHED_MIN) {
            gwy_app_wait_set_message("Finding minima...");
            if (!gwy_app_wait_set_fraction(0.0)) break;
        }
        else if (status.state == GWY_WSHED_LOCATE) {
            gwy_app_wait_set_message("Location...");
            if (!gwy_app_wait_set_fraction((gdouble)status.internal_i/(gdouble)args->locate_steps)) break;
        }
        else if (status.state == GWY_WSHED_WSHED) {
            gwy_app_wait_set_message("Watershed...");
            if (!gwy_app_wait_set_fraction((gdouble)status.internal_i/(gdouble)args->wshed_steps)) break;
        }
        else if (status.state == GWY_WSHED_MARK) {
            gwy_app_wait_set_message("Marking boundaries...");
            if (!gwy_app_wait_set_fraction(0.0)) break;
        }
        else {
            gwy_app_wait_set_message("Finished.");
            if (!gwy_app_wait_set_fraction(0.0)) break;
        }


    } while (status.state != GWY_WSHED_FINISHED);
    gwy_app_wait_finish();

    controls->computed = TRUE;
}

static const gchar *locate_steps_key = "/module/mark_wshed/locate_steps";
static const gchar *locate_thresh_key = "/module/mark_wshed/locate_thresh";
static const gchar *locate_dropsize_key = "/module/mark_wshed/locate_dropsize";
static const gchar *wshed_steps_key = "/module/mark_wshed/wshed_steps";
static const gchar *wshed_dropsize_key = "/module/mark_wshed/wshed_dropsize";

static void
wshed_sanitize_args(WshedArgs *args)
{
    args->locate_dropsize = CLAMP(args->locate_dropsize, 0.0, 100.0);
    args->wshed_dropsize = CLAMP(args->wshed_dropsize, 0.0, 100.0);
    args->locate_thresh = CLAMP(args->locate_thresh, 0, 100);
    args->locate_steps = CLAMP(args->locate_steps, 0, 100);
    args->wshed_steps = CLAMP(args->wshed_steps, 0, 1000);
}

static void
wshed_load_args(GwyContainer *container,
                WshedArgs *args)
{
    *args = wshed_defaults;

    gwy_container_gis_double_by_name(container, locate_dropsize_key,
                                     &args->locate_dropsize);
    gwy_container_gis_double_by_name(container, wshed_dropsize_key,
                                     &args->wshed_dropsize);
    gwy_container_gis_int32_by_name(container, locate_steps_key,
                                    &args->locate_steps);
    gwy_container_gis_int32_by_name(container, wshed_steps_key,
                                    &args->wshed_steps);
    gwy_container_gis_int32_by_name(container, locate_thresh_key,
                                    &args->locate_thresh);
    wshed_sanitize_args(args);
}

static void
wshed_save_args(GwyContainer *container,
                WshedArgs *args)
{
    gwy_container_set_double_by_name(container, wshed_dropsize_key,
                                     args->wshed_dropsize);
    gwy_container_set_double_by_name(container, locate_dropsize_key,
                                     args->locate_dropsize);
    gwy_container_set_int32_by_name(container, locate_steps_key,
                                    args->locate_steps);
    gwy_container_set_int32_by_name(container, wshed_steps_key,
                                    args->wshed_steps);
    gwy_container_set_int32_by_name(container, locate_thresh_key,
                                    args->locate_thresh);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
