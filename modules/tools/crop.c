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
#include <app/app.h>

typedef struct {
    gboolean is_visible;  /* XXX: GTK_WIDGET_VISIBLE() returns BS? */
    GtkWidget *x;
    GtkWidget *y;
    GtkWidget *w;
    GtkWidget *h;
    GtkWidget *windowname;
    gdouble mag;
    gint precision;
    gchar *units;
} CropControls;

static gboolean   module_register               (const gchar *name);
static void       crop_use                      (GwyDataWindow *data_window,
                                                 GwyToolSwitchEvent reason);
static GtkWidget* crop_dialog_create            (GwyDataWindow *data_window);
static void       crop_do                       (void);
static void       crop_selection_updated_cb     (void);
static void       crop_data_updated_cb          (void);
static void       crop_update_view              (void);
static void       crop_dialog_response_cb       (gpointer unused,
                                                 gint response);
static void       crop_dialog_abandon           (void);
static void       crop_dialog_set_visible       (gboolean visible);

static GtkWidget *crop_dialog = NULL;
static CropControls controls;
static gulong layer_updated_id = 0;
static gulong data_updated_id = 0;
static gulong response_id = 0;
static GwyDataViewLayer *select_layer = NULL;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "crop",
    "Crop tool.",
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
    static GwyToolFuncInfo crop_func_info = {
        "crop",
        "gwy_crop",
        "Crop data",
        20,
        crop_use,
    };

    gwy_tool_func_register(name, &crop_func_info);

    return TRUE;
}

static void
crop_use(GwyDataWindow *data_window,
         GwyToolSwitchEvent reason)
{
    GwyVectorLayer *layer;
    GwyDataView *data_view;
    GtkWidget *parent;

    gwy_debug("%p", data_window);

    if (!data_window) {
        crop_dialog_abandon();
        return;
    }
    g_return_if_fail(GWY_IS_DATA_WINDOW(data_window));
    data_view = (GwyDataView*)gwy_data_window_get_data_view(data_window);
    layer = gwy_data_view_get_top_layer(data_view);
    if (layer && (GwyDataViewLayer*)layer == select_layer)
        return;
    if (select_layer) {
        if (layer_updated_id)
        g_signal_handler_disconnect(select_layer, layer_updated_id);
        parent = GWY_DATA_VIEW_LAYER(select_layer)->parent;
        if (parent && data_updated_id)
            g_signal_handler_disconnect(parent, data_updated_id);
    }

    if (layer && GWY_IS_LAYER_SELECT(layer))
        select_layer = GWY_DATA_VIEW_LAYER(layer);
    else {
        select_layer = (GwyDataViewLayer*)gwy_layer_select_new();
        gwy_data_view_set_top_layer(data_view, GWY_VECTOR_LAYER(select_layer));
    }
    gwy_layer_select_set_is_crop(GWY_LAYER_SELECT(select_layer), TRUE);
    if (!crop_dialog)
        crop_dialog = crop_dialog_create(data_window);

    layer_updated_id = g_signal_connect(select_layer, "updated",
                                        G_CALLBACK(crop_selection_updated_cb),
                                        NULL);
    data_updated_id = g_signal_connect(data_view, "updated",
                                       G_CALLBACK(crop_data_updated_cb),
                                       NULL);
    if (reason == GWY_TOOL_SWITCH_TOOL)
        crop_dialog_set_visible(TRUE);
    /* FIXME: window name can change also when saving under different name */
    if (reason == GWY_TOOL_SWITCH_WINDOW)
        gtk_label_set_text(GTK_LABEL(controls.windowname),
                           gwy_data_window_get_base_name(data_window));
    if (controls.is_visible)
        crop_selection_updated_cb();
}

static void
crop_do(void)
{
    GtkWidget *data_window;
    GwyDataView *data_view;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xmin, ymin, xmax, ymax;

    if (!gwy_layer_select_get_selection(GWY_LAYER_SELECT(select_layer),
                                        &xmin, &ymin, &xmax, &ymax))
        return;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(select_layer)->parent);
    data = gwy_data_view_get_data(data_view);
    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(data)));
    gwy_app_clean_up_data(data);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xmin = gwy_data_field_rtoj(dfield, xmin);
    ymin = gwy_data_field_rtoi(dfield, ymin);
    xmax = gwy_data_field_rtoj(dfield, xmax) + 1;
    ymax = gwy_data_field_rtoi(dfield, ymax) + 1;
    gwy_data_field_resize(dfield,
                          (gint)xmin, (gint)ymin, (gint)xmax, (gint)ymax);
    data_window = gwy_app_data_window_create(data);
    gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    gwy_vector_layer_unselect(GWY_VECTOR_LAYER(select_layer));
    gwy_data_view_update(data_view);
    gwy_debug("%d %d",
              gwy_data_field_get_xres(dfield), gwy_data_field_get_yres(dfield));
}

static void
crop_dialog_abandon(void)
{
    GtkWidget *parent;

    gwy_debug("");
    if (select_layer) {
        if (layer_updated_id)
        g_signal_handler_disconnect(select_layer, layer_updated_id);
        parent = GWY_DATA_VIEW_LAYER(select_layer)->parent;
        if (parent && data_updated_id)
            g_signal_handler_disconnect(parent, data_updated_id);
    }
    layer_updated_id = 0;
    data_updated_id = 0;
    select_layer = NULL;
    if (crop_dialog) {
        g_signal_handler_disconnect(crop_dialog, response_id);
        gtk_widget_destroy(crop_dialog);
        crop_dialog = NULL;
        response_id = 0;
        g_free(controls.units);
        controls.is_visible = FALSE;
    }
}

static GtkWidget*
crop_dialog_create(GwyDataWindow *data_window)
{
    GwyContainer *data;
    GwyDataField *dfield;
    GtkWidget *dialog, *table, *label, *frame;
    gdouble xreal, yreal, max, unit;

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

    dialog = gtk_dialog_new_with_buttons(_("Crop"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(gwy_dialog_prevent_delete_cb), NULL);
    response_id = g_signal_connect(dialog, "response",
                                   G_CALLBACK(crop_dialog_response_cb), NULL);
 
    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame,
                       FALSE, FALSE, 0);
    label = gtk_label_new(gwy_data_window_get_base_name(data_window));
    controls.windowname = label;
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(label), 4, 2);
    gtk_container_add(GTK_CONTAINER(frame), label);

    table = gtk_table_new(6, 3, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Origin</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("X"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Y"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Size</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Width"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5, GTK_FILL, 0, 2, 2);
    label = gtk_label_new(_("Height"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6, GTK_FILL, 0, 2, 2);

    controls.x = gtk_label_new("");
    controls.y = gtk_label_new("");
    controls.w = gtk_label_new("");
    controls.h = gtk_label_new("");
    gtk_misc_set_alignment(GTK_MISC(controls.x), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.y), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.w), 1.0, 0.5);
    gtk_misc_set_alignment(GTK_MISC(controls.h), 1.0, 0.5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.x, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.y, 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.w, 2, 3, 4, 5);
    gtk_table_attach_defaults(GTK_TABLE(table), controls.h, 2, 3, 5, 6);
    gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
    controls.is_visible = FALSE;

    return dialog;
}

static void
update_label(GtkWidget *label, gdouble value)
{
    gchar buffer[16];

    g_snprintf(buffer, sizeof(buffer), "%.*f %s",
                controls.precision, value/controls.mag, controls.units);
    gtk_label_set_text(GTK_LABEL(label), buffer);
}

static void
crop_selection_updated_cb(void)
{
    gboolean is_selected;

    gwy_debug("");
    is_selected = gwy_layer_select_get_selection(GWY_LAYER_SELECT(select_layer),
                                                 NULL, NULL, NULL, NULL);
    crop_update_view();
    if (is_selected && !controls.is_visible)
        crop_dialog_set_visible(TRUE);
}

static void
crop_data_updated_cb(void)
{
    gwy_debug("");
    crop_update_view();
}

static void
crop_update_view(void)
{
    gdouble xmin, ymin, xmax, ymax;
    gboolean is_visible, is_selected;

    gwy_debug("");
    is_visible = controls.is_visible;
    is_selected = gwy_layer_select_get_selection(GWY_LAYER_SELECT(select_layer),
                                                 &xmin, &ymin, &xmax, &ymax);
    if (!is_visible && !is_selected)
        return;
    if (is_selected) {
        update_label(controls.x, MIN(xmin, xmax));
        update_label(controls.y, MIN(ymin, ymax));
        update_label(controls.w, fabs(xmax - xmin));
        update_label(controls.h, fabs(ymax - ymin));
    }
    else {
        gtk_label_set_text(GTK_LABEL(controls.x), "");
        gtk_label_set_text(GTK_LABEL(controls.y), "");
        gtk_label_set_text(GTK_LABEL(controls.w), "");
        gtk_label_set_text(GTK_LABEL(controls.h), "");
    }
}

static void
crop_dialog_response_cb(G_GNUC_UNUSED gpointer unused,
                        gint response)
{
    gwy_debug("response %d", response);
    switch (response) {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_DELETE_EVENT:
        crop_dialog_set_visible(FALSE);
        break;

        case GTK_RESPONSE_NONE:
        g_warning("Tool dialog destroyed.");
        crop_use(NULL, 0);
        break;

        case GTK_RESPONSE_APPLY:
        crop_do();
        crop_dialog_set_visible(FALSE);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static void
crop_dialog_set_visible(gboolean visible)
{
    gwy_debug("now %d, setting to %d",
              controls.is_visible, visible);
    if (controls.is_visible == visible)
        return;

    controls.is_visible = visible;
    if (visible)
        gtk_window_present(GTK_WINDOW(crop_dialog));
    else
        gtk_widget_hide(crop_dialog);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

