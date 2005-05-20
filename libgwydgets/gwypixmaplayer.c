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

#include <string.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include "gwypixmaplayer.h"

#define GWY_PIXMAP_LAYER_TYPE_NAME "GwyPixmapLayer"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void     gwy_pixmap_layer_class_init   (GwyPixmapLayerClass *klass);
static void     gwy_pixmap_layer_init         (GwyPixmapLayer *layer);
static void     gwy_pixmap_layer_finalize     (GObject *object);
static void     gwy_pixmap_layer_destroy      (GtkObject *object);
static void     gwy_pixmap_layer_plugged      (GwyDataViewLayer *layer);
static void     gwy_pixmap_layer_unplugged    (GwyDataViewLayer *layer);
static void     gwy_pixmap_layer_item_changed (GwyPixmapLayer *pixmap_layer);
static void     gwy_pixmap_layer_data_changed (GwyPixmapLayer *pixmap_layer);

/* Local data */

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
        gwy_debug("");
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
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_pixmap_layer_finalize;

    object_class->destroy = gwy_pixmap_layer_destroy;

    layer_class->plugged = gwy_pixmap_layer_plugged;
    layer_class->unplugged = gwy_pixmap_layer_unplugged;

    klass->paint = NULL;
}

static void
gwy_pixmap_layer_init(G_GNUC_UNUSED GwyPixmapLayer *layer)
{
}

static void
gwy_pixmap_layer_finalize(GObject *object)
{
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_pixmap_layer_destroy(GtkObject *object)
{
    GwyPixmapLayer *layer;

    layer = GWY_PIXMAP_LAYER(object);
    gwy_object_unref(layer->data_field);
    gwy_object_unref(layer->pixbuf);
}

/**
 * gwy_pixmap_layer_paint:
 * @layer: A pixmap data view layer.
 *
 * Returns a pixbuf with painted pixmap layer @layer.
 *
 * Returns: The pixbuf.  It should not be modified or freed.  The layer must
 * be a pixmap layer.  Use gwy_pixmap_layer_draw() for vector layers.
 **/
GdkPixbuf*
gwy_pixmap_layer_paint(GwyPixmapLayer *pixmap_layer)
{
    GwyPixmapLayerClass *layer_class = GWY_PIXMAP_LAYER_GET_CLASS(pixmap_layer);

    g_return_val_if_fail(GWY_IS_PIXMAP_LAYER(pixmap_layer), NULL);
    g_return_val_if_fail(layer_class->paint, NULL);
    if (!pixmap_layer->data_field) {
        g_warning("Data field to paint is missing.  "
                  "That's probably because I didn't implement it yet.");
        /*return pixmap_layer->pixbuf;*/
    }
    return layer_class->paint(pixmap_layer);
}

static inline void
gwy_pixmap_layer_data_field_connect(GwyPixmapLayer *pixmap_layer)
{
    GwyDataViewLayer *layer;

    g_assert(!pixmap_layer->data_field);
    layer = GWY_DATA_VIEW_LAYER(pixmap_layer);
    if (!gwy_container_gis_object(layer->data, pixmap_layer->data_key,
                                  &pixmap_layer->data_field))
        return;

    g_object_ref(pixmap_layer->data_field);
    pixmap_layer->data_changed_id
        = g_signal_connect_swapped(pixmap_layer->data_field,
                                   "data_changed",
                                   G_CALLBACK(gwy_pixmap_layer_data_changed),
                                   layer);
}

static inline void
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
    const gchar *data_key_string;
    gchar *detailed_signal;

    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    if (!pixmap_layer->data_key)
        return;

    /* Watch for changes in container */
    data_key_string = g_quark_to_string(pixmap_layer->data_key);
    detailed_signal = g_newa(gchar, sizeof("item_changed::")
                                    + strlen(data_key_string));
    g_stpcpy(g_stpcpy(detailed_signal, "item_changed::"), data_key_string);

    pixmap_layer->item_changed_id
        = g_signal_connect_swapped(layer->data, detailed_signal,
                                   G_CALLBACK(gwy_pixmap_layer_item_changed),
                                   layer);

    /* Watch for changes in the data field itself */
    gwy_pixmap_layer_data_field_connect(pixmap_layer);
}

static void
gwy_pixmap_layer_unplugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;

    pixmap_layer = GWY_PIXMAP_LAYER(layer);

    gwy_pixmap_layer_data_field_disconnect(pixmap_layer);
    if (pixmap_layer->item_changed_id)
        g_signal_handler_disconnect(layer->data, pixmap_layer->item_changed_id);

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
    /* TODO: emit something */
}

static void
gwy_pixmap_layer_data_changed(GwyPixmapLayer *pixmap_layer)
{
    /* TODO: emit something */
}

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

    gwy_pixmap_layer_data_field_disconnect(pixmap_layer);
    pixmap_layer->data_key = quark;
    gwy_pixmap_layer_data_field_connect(pixmap_layer);
    /* TODO: emit something */
}

const gchar*
gwy_pixmap_layer_get_data_key(GwyPixmapLayer *pixmap_layer)
{
    g_return_val_if_fail(GWY_IS_PIXMAP_LAYER(pixmap_layer), NULL);
    return g_quark_to_string(pixmap_layer->data_key);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
