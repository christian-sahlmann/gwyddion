/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>

typedef struct {
    gboolean is_visible;  /* XXX: GTK_WIDGET_VISIBLE() returns BS? */
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *val;
    GtkWidget *windowname;
    GtkObject *radius;
    gdouble mag;
    gint precision;
    gchar *units;
} PointerControls;

static gboolean   module_register                (const gchar *name);
static void       pointer_use                    (GwyDataWindow *data_window,
                                                  GwyToolSwitchEvent reason);
static GtkWidget* pointer_dialog_create          (GwyDataWindow *data_window);
static void       pointer_selection_updated_cb   (void);
static void       pointer_data_updated_cb        (void);
static void       pointer_update_view            (void);
static void       pointer_dialog_response_cb     (gpointer unused,
                                                  gint response);
static gdouble    pointer_get_z_average          (GwyDataField *dfield,
                                                  gdouble xreal,
                                                  gdouble yreal,
                                                  gint radius);
static void       pointer_dialog_abandon         (void);
static void       pointer_dialog_set_visible     (gboolean visible);

static const gchar *radius_key = "/tool/pointer/radius";

static GtkWidget *pointer_dialog = NULL;
static PointerControls controls;
static gulong layer_updated_id = 0;
static gulong data_updated_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *pointer_layer = NULL;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "pointer",
    "Pointer tool.",
    "Yeti <yeti@physics.muni.cz>",
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
    static GwyToolFuncInfo pointer_func_info = {
        "pointer",
        "gwy_pointer_measure",
        "Read value under mouse cursor.",
        0,
        &pointer_use,
    };

    gwy_tool_func_register(name, &pointer_func_info);

    return TRUE;
}

static void
pointer_use(GwyDataWindow *data_window,
            GwyToolSwitchEvent reason)
{
    GwyDataViewLayer *layer;
    GwyDataView *data_view;

    gwy_debug("%p", data_window);

    if (!data_window) {
        pointer_dialog_abandon();
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && layer == pointer_layer)
        return;
    if (pointer_layer) {
        if (layer_updated_id)
        g_signal_handler_disconnect(pointer_layer, layer_updated_id);
        if (pointer_layer->parent && data_updated_id)
            g_signal_handler_disconnect(pointer_layer->parent, data_updated_id);
    }

    if (layer && GWY_IS_LAYER_POINTER(layer))
        pointer_layer = layer;
    else {
        pointer_layer = (GwyDataViewLayer*)gwy_layer_pointer_new();
        gwy_data_view_set_top_layer(data_view, pointer_layer);
    }
    if (!pointer_dialog)
        pointer_dialog = pointer_dialog_create(data_window);

    layer_updated_id = g_signal_connect(pointer_layer, "updated",
                                        G_CALLBACK(pointer_selection_updated_cb),
                                        NULL);
    data_updated_id = g_signal_connect(data_view, "updated",
                                       G_CALLBACK(pointer_data_updated_cb),
                                       NULL);
    if (reason == GWY_TOOL_SWITCH_TOOL)
        pointer_dialog_set_visible(TRUE);
    /* FIXME: window name can change also when saving under different name */
    if (reason == GWY_TOOL_SWITCH_WINDOW)
        gtk_label_set_text(GTK_LABEL(controls.windowname),
                           gwy_data_window_get_base_name(data_window));
    if (controls.is_visible)
        pointer_selection_updated_cb();
}

static gdouble
pointer_get_z_average(GwyDataField *dfield,
                      gdouble xreal,
                      gdouble yreal,
                      gint radius)
{
    gint x, y, xres, yres, uli, ulj, bri, brj;

    if (radius < 1)
        g_warning("Bad averaging radius %d, fixing to 1", radius);
    x = gwy_data_field_rtoj(dfield, xreal);
    y = gwy_data_field_rtoi(dfield, yreal);
    if (radius < 2)
        return gwy_data_field_get_val(dfield, x, y);
    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    ulj = CLAMP(x - radius, 0, xres - 1);
    uli = CLAMP(y - radius, 0, yres - 1);
    brj = CLAMP(x + radius, 0, xres - 1);
    bri = CLAMP(y + radius, 0, yres - 1);

    return gwy_data_field_get_area_avg(dfield, ulj, uli, brj, bri);
}

static void
pointer_dialog_abandon(void)
{
    GwyContainer *settings;
    gint radius;

    if (pointer_layer) {
        if (layer_updated_id)
        g_signal_handler_disconnect(pointer_layer, layer_updated_id);
        if (pointer_layer->parent && data_updated_id)
            g_signal_handler_disconnect(pointer_layer->parent, data_updated_id);
    }
    layer_updated_id = 0;
    data_updated_id = 0;
    pointer_layer = NULL;
    if (pointer_dialog) {
        radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.radius));
        radius = CLAMP(radius, 1, 16);
        settings = gwy_app_settings_get();
        gwy_container_set_int32_by_name(settings, radius_key, radius);
        g_signal_handler_disconnect(pointer_dialog, response_id);
        gtk_widget_destroy(pointer_dialog);
        pointer_dialog = NULL;
        response_id = 0;
        g_free(controls.units);
        controls.is_visible = FALSE;
    }
}

static GtkWidget*
pointer_dialog_create(GwyDataWindow *data_window)
{
    GwyContainer *data, *settings;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label, *frame;
    gdouble xreal, yreal, max, unit;
    gint radius;

    gwy_debug("");
    data = gwy_data_window_get_data(data_window);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    max = MAX(xreal, yreal);
    unit = MIN(xreal/gwy_data_field_get_xres(dfield),
               yreal/gwy_data_field_get_yres(dfield));
    controls.mag = gwy_math_humanize_numbers(unit, max, &controls.precision);
    controls.units = g_strconcat(gwy_math_SI_prefix(controls.mag), "m", NULL);

    dialog = gtk_dialog_new_with_buttons(_("Show value"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(pointer_dialog_response_cb), NULL);
 
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);
    label = gtk_label_new(gwy_data_window_get_base_name(data_window));
    controls.windowname = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 4, 2);
    gtk_container_add(GTK_CONTAINER(frame), label);

    table = gtk_table_new(2, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_table_set_col_spacings(GTK_TABLE(table), 6);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>X</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Y</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Value</b>"));
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 0, 1, GTK_FILL, 0, 2, 2);

    label = controls.x = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, 0, 0, 2, 2);
    label = controls.y = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, 0, 0, 2, 2);
    label = controls.val = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2,
                        GTK_EXPAND | GTK_FILL, 0, 2, 2);


    table = gtk_table_new(1, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

    settings = gwy_app_settings_get();
    if (gwy_container_contains_by_name(settings, radius_key))
        radius = gwy_container_get_int32_by_name(settings, radius_key);
    else
        radius = 1;
    controls.radius = gtk_adjustment_new((gdouble)radius, 1, 16, 1, 5, 16);
    gwy_table_attach_spinbutton(table, 9, "Averaging radius", "px",
                                controls.radius);
    g_signal_connect(controls.radius, "value_changed",
                     G_CALLBACK(pointer_selection_updated_cb), NULL);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
    controls.is_visible = FALSE;

    return dialog;
}

static void
update_coord_label(GtkWidget *label, gdouble value)
{
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
               controls.precision, value/controls.mag, controls.units);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
update_value_label(GtkWidget *label, gdouble value)
{
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%g", value);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
pointer_selection_updated_cb(void)
{
    gboolean is_selected;

    gwy_debug("");
    is_selected = gwy_layer_pointer_get_point(pointer_layer, NULL, NULL);
    pointer_update_view();
    if (is_selected && !controls.is_visible)
        pointer_dialog_set_visible(TRUE);
}

static void
pointer_data_updated_cb(void)
{
    gwy_debug("");
    pointer_update_view();
}

static void
pointer_update_view(void)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble x, y, value;
    gboolean is_visible, is_selected;
    gint radius;

    gwy_debug("");

    data = gwy_data_view_get_data(GWY_DATA_VIEW(pointer_layer->parent));
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    radius = (gint)gtk_adjustment_get_value(GTK_ADJUSTMENT(controls.radius));

    is_visible = controls.is_visible;
    is_selected = gwy_layer_pointer_get_point(pointer_layer, &x, &y);
    if (!is_visible && !is_selected)
        return;
    if (is_selected) {
        update_coord_label(controls.x, x);
        update_coord_label(controls.y, y);
        value = pointer_get_z_average(dfield, x, y, radius);
        update_value_label(controls.val, value);
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls.x), "");
        gtk_label_set_text(GTK_LABEL(controls.y), "");
        gtk_label_set_text(GTK_LABEL(controls.val), "");
    }
}

static void
pointer_dialog_response_cb(G_GNUC_UNUSED gpointer unused, gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        pointer_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        pointer_use(NULL, 0);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
pointer_dialog_set_visible(gboolean visible)
{
    gwy_debug("now %d, setting to %d",
              controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(pointer_dialog));
    else
        gtk_widget_hide(pointer_dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

