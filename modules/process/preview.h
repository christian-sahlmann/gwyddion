/*
 *  @(#) $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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

#ifndef __GWY_PROCESS_PREVIEW_H__
#define __GWY_PROCESS_PREVIEW_H__

#include <libgwydgets/gwycolorbutton.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwylayer-basic.h>
#include <libgwydgets/gwylayer-mask.h>
#include <app/data-browser.h>

G_GNUC_UNUSED
static GwyPixmapLayer*
create_basic_layer(GwyDataView *dataview, gint id)
{
    GwyPixmapLayer *layer = gwy_layer_basic_new();
    GQuark quark = gwy_app_get_data_key_for_id(id);
    const gchar *key = g_quark_to_string(quark);
    gchar buf[24];

    gwy_pixmap_layer_set_data_key(layer, key);
    key = g_quark_to_string(gwy_app_get_data_palette_key_for_id(id));
    gwy_layer_basic_set_gradient_key(GWY_LAYER_BASIC(layer), key);
    key = g_quark_to_string(gwy_app_get_data_range_type_key_for_id(id));
    gwy_layer_basic_set_range_type_key(GWY_LAYER_BASIC(layer), key);
    g_snprintf(buf, sizeof(buf), "/%d/base", id);
    gwy_layer_basic_set_min_max_key(GWY_LAYER_BASIC(layer), buf);
    gwy_data_view_set_base_layer(dataview, layer);

    return layer;
}

G_GNUC_UNUSED
static GwyPixmapLayer*
create_mask_layer(GwyDataView *dataview, gint id)
{
    GwyPixmapLayer *layer = gwy_layer_mask_new();
    GQuark quark = gwy_app_get_mask_key_for_id(id);
    const gchar *key = g_quark_to_string(quark);

    gwy_pixmap_layer_set_data_key(layer, key);
    gwy_layer_mask_set_color_key(GWY_LAYER_MASK(layer), key);
    gwy_data_view_set_alpha_layer(dataview, layer);

    return layer;
}

G_GNUC_UNUSED
static GtkWidget*
create_preview(GwyContainer *data,
               gint id, gint preview_size, gboolean have_mask)
{
    GQuark quark = gwy_app_get_mask_key_for_id(id);
    const gchar *key = g_quark_to_string(quark);
    GwyDataView *dataview;
    GtkWidget *widget;

    widget = gwy_data_view_new(data);
    dataview = GWY_DATA_VIEW(widget);
    gwy_data_view_set_data_prefix(dataview, key);
    create_basic_layer(dataview, id);
    if (have_mask)
        create_mask_layer(dataview, id);
    gwy_set_data_preview_size(dataview, preview_size);

    return widget;
}

G_GNUC_UNUSED
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

G_GNUC_UNUSED
static void
mask_color_changed(GtkWidget *color_button)
{
    GObject *object = G_OBJECT(color_button);
    GtkWindow *dialog;
    GwyContainer *data;
    GQuark quark;
    gint id;

    data = GWY_CONTAINER(g_object_get_data(object, "data"));
    dialog = GTK_WINDOW(g_object_get_data(object, "dialog"));
    id = GPOINTER_TO_INT(g_object_get_data(object, "id"));
    quark = gwy_app_get_mask_key_for_id(id);
    gwy_mask_color_selector_run(NULL, dialog,
                                GWY_COLOR_BUTTON(color_button), data,
                                g_quark_to_string(quark));
    load_mask_color(color_button, data);
}

G_GNUC_UNUSED
static GtkWidget*
create_mask_color_button(GwyContainer *data, GtkWidget *dialog, gint id)
{
    GtkWidget *color_button;
    GObject *object;

    color_button = gwy_color_button_new();
    object = G_OBJECT(color_button);
    g_object_set_data(object, "data", data);
    g_object_set_data(object, "dialog", dialog);
    g_object_set_data(object, "id", GINT_TO_POINTER(id));

    gwy_color_button_set_use_alpha(GWY_COLOR_BUTTON(color_button), TRUE);
    load_mask_color(color_button, data);
    g_signal_connect(color_button, "clicked",
                     G_CALLBACK(mask_color_changed), NULL);

    return color_button;
}

#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
