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

#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwypixmaplayer.h"

#define GWY_PIXMAP_LAYER_TYPE_NAME "GwyPixmapLayer"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void     gwy_pixmap_layer_class_init   (GwyPixmapLayerClass *klass);
static void     gwy_pixmap_layer_init         (GwyPixmapLayer *layer);
static void     gwy_pixmap_layer_finalize     (GObject *object);
static void     gwy_pixmap_layer_plugged      (GwyDataViewLayer *layer);
static void     gwy_pixmap_layer_unplugged    (GwyDataViewLayer *layer);

/* Local data */

static GtkObjectClass *parent_class = NULL;

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
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_pixmap_layer_finalize;

    layer_class->plugged = gwy_pixmap_layer_plugged;
    layer_class->unplugged = gwy_pixmap_layer_unplugged;

    klass->paint = NULL;
}

static void
gwy_pixmap_layer_init(GwyPixmapLayer *layer)
{
    gwy_debug("");

    layer->pixbuf = NULL;
}

static void
gwy_pixmap_layer_finalize(GObject *object)
{
    GwyPixmapLayer *layer;

    gwy_debug("");

    g_return_if_fail(GWY_IS_PIXMAP_LAYER(object));

    layer = GWY_PIXMAP_LAYER(object);

    gwy_object_unref(layer->pixbuf);

    G_OBJECT_CLASS(parent_class)->finalize(object);
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
gwy_pixmap_layer_paint(GwyPixmapLayer *layer)
{
    GwyPixmapLayerClass *layer_class = GWY_PIXMAP_LAYER_GET_CLASS(layer);

    g_return_val_if_fail(layer_class->paint, NULL);
    return layer_class->paint(layer);
}

static void
gwy_pixmap_layer_plugged(GwyDataViewLayer *layer)
{
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
}

static void
gwy_pixmap_layer_unplugged(GwyDataViewLayer *layer)
{
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
