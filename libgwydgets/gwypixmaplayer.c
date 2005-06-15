/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwydebugobjects.h>

#include <string.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>
#include <libprocess/datafield.h>

#include "gwypixmaplayer.h"

#define GWY_PIXMAP_LAYER_TYPE_NAME "GwyPixmapLayer"

#define BITS_PER_SAMPLE 8

enum {
    PROP_0,
    PROP_DATA_KEY
};

static void gwy_pixmap_layer_class_init         (GwyPixmapLayerClass *klass);
static void gwy_pixmap_layer_init               (GwyPixmapLayer *layer);
static void gwy_pixmap_layer_set_property       (GObject *object,
                                                 guint prop_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec);
static void gwy_pixmap_layer_get_property       (GObject *object,
                                                 guint prop_id,
                                                 GValue *value,
                                                 GParamSpec *pspec);
static void gwy_pixmap_layer_destroy            (GtkObject *object);
static void gwy_pixmap_layer_plugged            (GwyDataViewLayer *layer);
static void gwy_pixmap_layer_unplugged          (GwyDataViewLayer *layer);
static void gwy_pixmap_layer_item_changed       (GwyPixmapLayer *pixmap_layer);
static void gwy_pixmap_layer_data_changed       (GwyPixmapLayer *pixmap_layer);
static void gwy_pixmap_layer_container_connect  (GwyPixmapLayer *pixmap_layer,
                                                 const gchar *data_key_string);
static void gwy_pixmap_layer_data_field_connect (GwyPixmapLayer *pixmap_layer);
static void gwy_pixmap_layer_data_field_disconnect(GwyPixmapLayer *pixmap_layer);

static GwyDataViewLayerClass *parent_class = NULL;

GType
gwy_pixmap_layer_get_type(void)
{
    static GType gwy_pixmap_layer_type = 0;

    if (!gwy_pixmap_layer_type) {
        static const GTypeInfo gwy_pixmap_layer_info = {
            sizeof(GwyPixmapLayerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_pixmap_layer_class_init,
            NULL,
            NULL,
            sizeof(GwyPixmapLayer),
            0,
            (GInstanceInitFunc)gwy_pixmap_layer_init,
            NULL,
        };
        gwy_pixmap_layer_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_PIXMAP_LAYER_TYPE_NAME,
                                     &gwy_pixmap_layer_info,
                                     0);
    }

    return gwy_pixmap_layer_type;
}

static void
gwy_pixmap_layer_class_init(GwyPixmapLayerClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->set_property = gwy_pixmap_layer_set_property;
    gobject_class->get_property = gwy_pixmap_layer_get_property;

    object_class->destroy = gwy_pixmap_layer_destroy;

    layer_class->plugged = gwy_pixmap_layer_plugged;
    layer_class->unplugged = gwy_pixmap_layer_unplugged;

    /**
     * GwyPixmapLayer:data-key:
     *
     * The :data_key property is the container key used to identify
     * displayed #GwyDataField in container.
     */
    g_object_class_install_property
        (gobject_class,
         PROP_DATA_KEY,
         g_param_spec_string("data_key",
                             "Data key",
                             "Key identifying data field in container",
                             NULL, G_PARAM_READWRITE));
}

static void
gwy_pixmap_layer_init(G_GNUC_UNUSED GwyPixmapLayer *layer)
{
}

static void
gwy_pixmap_layer_destroy(GtkObject *object)
{
    GwyPixmapLayer *layer;

    layer = GWY_PIXMAP_LAYER(object);
    gwy_object_unref(layer->data_field);
    gwy_object_unref(layer->pixbuf);

    GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

static void
gwy_pixmap_layer_set_property(GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    GwyPixmapLayer *pixmap_layer = GWY_PIXMAP_LAYER(object);

    switch (prop_id) {
        case PROP_DATA_KEY:
        gwy_pixmap_layer_set_data_key(pixmap_layer, g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_pixmap_layer_get_property(GObject*object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    GwyPixmapLayer *pixmap_layer = GWY_PIXMAP_LAYER(object);

    switch (prop_id) {
        case PROP_DATA_KEY:
        g_value_set_static_string(value,
                                  g_quark_to_string(pixmap_layer->data_key));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_pixmap_layer_paint:
 * @pixmap_layer: A pixmap data view layer.
 *
 * Returns a pixbuf with painted pixmap layer.
 *
 * This method does not enforce repaint.  If the layer doesn't think it needs
 * to repaint the pixbuf, it simply returns the current one.  To enforce
 * update, emit "data_changed" signal on corresponding data field.
 *
 * Returns: The pixbuf.  It should not be modified or freed.
 **/
GdkPixbuf*
gwy_pixmap_layer_paint(GwyPixmapLayer *pixmap_layer)
{
    GwyPixmapLayerClass *layer_class = GWY_PIXMAP_LAYER_GET_CLASS(pixmap_layer);

    g_return_val_if_fail(GWY_IS_PIXMAP_LAYER(pixmap_layer), NULL);
    g_return_val_if_fail(layer_class->paint, NULL);

    if (!pixmap_layer->data_field
        || !GWY_IS_DATA_FIELD(pixmap_layer->data_field)) {
        g_warning("Data field to paint is missing.  "
                  "That's probably because I didn't implement it yet.");
        pixmap_layer->wants_repaint = FALSE;
        return pixmap_layer->pixbuf;
    }
    if (pixmap_layer->wants_repaint)
        layer_class->paint(pixmap_layer);
    pixmap_layer->wants_repaint = FALSE;

    return pixmap_layer->pixbuf;
}

/**
 * gwy_data_view_layer_wants_repaint:
 * @pixmap_layer: A pixmap data view layer.
 *
 * Checks whether a pixmap layer wants repaint.
 *
 * Returns: %TRUE if the the layer wants repaint itself, %FALSE otherwise.
 **/
gboolean
gwy_pixmap_layer_wants_repaint(GwyPixmapLayer *pixmap_layer)
{
    g_return_val_if_fail(GWY_IS_PIXMAP_LAYER(pixmap_layer), FALSE);

    return pixmap_layer->wants_repaint;
}

/**
 * gwy_pixmap_layer_data_field_connect:
 * @pixmap_layer: A pixmap layer.
 *
 * Eventually connects to new data field's "data_changed" signal.
 **/
static void
gwy_pixmap_layer_data_field_connect(GwyPixmapLayer *pixmap_layer)
{
    GwyDataViewLayer *layer;

    g_return_if_fail(!pixmap_layer->data_field);
    if (!pixmap_layer->data_key)
        return;

    layer = GWY_DATA_VIEW_LAYER(pixmap_layer);
    if (!gwy_container_gis_object(layer->data, pixmap_layer->data_key,
                                  &pixmap_layer->data_field))
        return;

    /*g_return_if_fail(GWY_IS_DATA_FIELD(pixmap_layer->data_field));*/
    g_object_ref(pixmap_layer->data_field);
    pixmap_layer->data_changed_id
        = g_signal_connect_swapped(pixmap_layer->data_field,
                                   "data_changed",
                                   G_CALLBACK(gwy_pixmap_layer_data_changed),
                                   layer);
}

/**
 * gwy_pixmap_layer_data_field_disconnect:
 * @pixmap_layer: A pixmap layer.
 *
 * Disconnects from all data field's signals and drops reference to it.
 **/
static void
gwy_pixmap_layer_data_field_disconnect(GwyPixmapLayer *pixmap_layer)
{
    if (!pixmap_layer->data_field)
        return;

    g_signal_handler_disconnect(pixmap_layer->data_field,
                                pixmap_layer->data_changed_id);
    pixmap_layer->data_changed_id = 0;
    gwy_object_unref(pixmap_layer->data_field);
}

static void
gwy_pixmap_layer_plugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;

    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    if (!pixmap_layer->data_key)
        return;

    gwy_pixmap_layer_container_connect(pixmap_layer,
                                       g_quark_to_string
                                                     (pixmap_layer->data_key));
    gwy_pixmap_layer_data_field_connect(pixmap_layer);
    pixmap_layer->wants_repaint = TRUE;
}

static void
gwy_pixmap_layer_unplugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;

    pixmap_layer = GWY_PIXMAP_LAYER(layer);

    pixmap_layer->wants_repaint = FALSE;
    gwy_pixmap_layer_data_field_disconnect(pixmap_layer);
    if (pixmap_layer->item_changed_id) {
        g_signal_handler_disconnect(layer->data, pixmap_layer->item_changed_id);
        pixmap_layer->item_changed_id = 0;
    }

    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

/**
 * gwy_pixmap_layer_item_changed:
 * @layer: A pixmap data view layer.
 * @data: Container with the data field this pixmap layer display.
 *
 * Reconnects signals to a new data field when it was replaced in the
 * container.
 **/
static void
gwy_pixmap_layer_item_changed(GwyPixmapLayer *pixmap_layer)
{
    gwy_pixmap_layer_data_field_disconnect(pixmap_layer);
    gwy_pixmap_layer_data_field_connect(pixmap_layer);
    pixmap_layer->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(pixmap_layer));
}

static void
gwy_pixmap_layer_data_changed(GwyPixmapLayer *pixmap_layer)
{
    pixmap_layer->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(pixmap_layer));
}

/**
 * gwy_pixmap_layer_set_data_key:
 * @pixmap_layer: A pixmap layer.
 * @key: Container string key identifying the data field to display.
 *
 * Sets the data field to display by a pixmap layer.
 **/
void
gwy_pixmap_layer_set_data_key(GwyPixmapLayer *pixmap_layer,
                              const gchar *key)
{
    GwyDataViewLayer *layer;
    GQuark quark;

    g_return_if_fail(GWY_IS_PIXMAP_LAYER(pixmap_layer));

    quark = key ? g_quark_from_string(key) : 0;
    if (pixmap_layer->data_key == quark)
        return;

    layer = GWY_DATA_VIEW_LAYER(pixmap_layer);
    if (!layer->data) {
        pixmap_layer->data_key = quark;
        return;
    }

    g_signal_handler_disconnect(layer->data, pixmap_layer->item_changed_id);
    pixmap_layer->item_changed_id = 0;
    gwy_pixmap_layer_data_field_disconnect(pixmap_layer);
    pixmap_layer->data_key = quark;
    gwy_pixmap_layer_data_field_connect(pixmap_layer);
    gwy_pixmap_layer_container_connect(pixmap_layer, key);

    pixmap_layer->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(pixmap_layer));
}

static void
gwy_pixmap_layer_container_connect(GwyPixmapLayer *pixmap_layer,
                                   const gchar *data_key_string)
{
    GwyDataViewLayer *layer;
    gchar *detailed_signal;

    g_return_if_fail(data_key_string);
    layer = GWY_DATA_VIEW_LAYER(pixmap_layer);
    detailed_signal = g_newa(gchar, sizeof("item_changed::")
                                    + strlen(data_key_string));
    g_stpcpy(g_stpcpy(detailed_signal, "item_changed::"), data_key_string);

    pixmap_layer->item_changed_id
        = g_signal_connect_swapped(layer->data, detailed_signal,
                                   G_CALLBACK(gwy_pixmap_layer_item_changed),
                                   layer);
}

/**
 * gwy_pixmap_layer_get_data_key:
 * @pixmap_layer: A pixmap layer.
 *
 * Gets the key identifying data field this pixmap layer displays.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_pixmap_layer_get_data_key(GwyPixmapLayer *pixmap_layer)
{
    g_return_val_if_fail(GWY_IS_PIXMAP_LAYER(pixmap_layer), NULL);
    return g_quark_to_string(pixmap_layer->data_key);
}

/**
 * gwy_pixmap_layer_make_pixbuf:
 * @pixmap_layer: A pixmap layer.
 * @has_alpha: Whether pixbuf should have alpha channel.
 *
 * Creates or resizes pixmap layer #GdkPixbuf to match its data field.
 *
 * This method is intended for pixmap layer implementation.
 **/
void
gwy_pixmap_layer_make_pixbuf(GwyPixmapLayer *pixmap_layer,
                             gboolean has_alpha)
{
    GwyDataField *data_field;
    gint dwidth, dheight, pwidth, pheight;

    g_return_if_fail(GWY_IS_PIXMAP_LAYER(pixmap_layer));
    data_field = GWY_DATA_FIELD(pixmap_layer->data_field);
    g_return_if_fail(data_field);
    dwidth = gwy_data_field_get_xres(data_field);
    dheight = gwy_data_field_get_yres(data_field);
    if (pixmap_layer->pixbuf) {
        pwidth = gdk_pixbuf_get_width(pixmap_layer->pixbuf);
        pheight = gdk_pixbuf_get_height(pixmap_layer->pixbuf);
        if (pwidth == dwidth && pheight == dheight)
            return;

        gwy_object_unref(pixmap_layer->pixbuf);
    }

    pixmap_layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, has_alpha,
                                          BITS_PER_SAMPLE, dwidth, dheight);
    gwy_debug_objects_creation(G_OBJECT(pixmap_layer->pixbuf));
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
