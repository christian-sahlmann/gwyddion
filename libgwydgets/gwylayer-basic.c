/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwycontainer.h>
#include <libdraw/gwypixfield.h>
#include "gwylayer-basic.h"

#define GWY_LAYER_BASIC_TYPE_NAME "GwyLayerBasic"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_basic_class_init        (GwyLayerBasicClass *klass);
static void       gwy_layer_basic_init              (GwyLayerBasic *layer);
static void       gwy_layer_basic_finalize          (GObject *object);
static GdkPixbuf* gwy_layer_basic_paint             (GwyDataViewLayer *layer);
static gboolean   gwy_layer_basic_wants_repaint     (GwyDataViewLayer *layer);
static void       gwy_layer_basic_plugged           (GwyDataViewLayer *layer);
static void       gwy_layer_basic_unplugged         (GwyDataViewLayer *layer);
static void       gwy_layer_basic_update            (GwyDataViewLayer *layer);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_basic_get_type(void)
{
    static GType gwy_layer_basic_type = 0;

    if (!gwy_layer_basic_type) {
        static const GTypeInfo gwy_layer_basic_info = {
            sizeof(GwyLayerBasicClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_basic_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerBasic),
            0,
            (GInstanceInitFunc)gwy_layer_basic_init,
            NULL,
        };
        gwy_debug("%s", __FUNCTION__);
        gwy_layer_basic_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_BASIC_TYPE_NAME,
                                     &gwy_layer_basic_info,
                                     0);
    }

    return gwy_layer_basic_type;
}

static void
gwy_layer_basic_class_init(GwyLayerBasicClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_basic_finalize;

    layer_class->paint = gwy_layer_basic_paint;
    layer_class->wants_repaint = gwy_layer_basic_wants_repaint;
    layer_class->plugged = gwy_layer_basic_plugged;
    layer_class->unplugged = gwy_layer_basic_unplugged;
}

static void
gwy_layer_basic_init(GwyLayerBasic *layer)
{
    gwy_debug("%s", __FUNCTION__);

    layer->changed = TRUE;
}

static void
gwy_layer_basic_finalize(GObject *object)
{
    GwyDataViewLayer *layer;
    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(GWY_IS_LAYER_BASIC(object));
    layer = GWY_DATA_VIEW_LAYER(object);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_basic_new:
 *
 * Creates a new basic data displaying layer.
 *
 * By default, is uses a gray palette or the palette stored with the data
 * as "/0/base/palette".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_basic_new(void)
{
    GtkObject *object;
    GwyDataViewLayer *layer;

    gwy_debug("%s", __FUNCTION__);

    object = g_object_new(GWY_TYPE_LAYER_BASIC, NULL);
    layer = (GwyDataViewLayer*)object;

    layer->palette = (GwyPalette*)(gwy_palette_new(NULL));
    /*
    gwy_palette_set_by_name(layer->palette, GWY_PALETTE_GRAY);
    g_signal_connect_swapped(layer->palette, "value_changed",
                             G_CALLBACK(gwy_layer_basic_update), layer);
                             */

    return object;
}

static GdkPixbuf*
gwy_layer_basic_paint(GwyDataViewLayer *layer)
{
    GwyDataField *data_field;

    gwy_debug("%s", __FUNCTION__);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/data"));
    g_return_val_if_fail(data_field, layer->pixbuf);
    /* FIXME FIXME FIXME */
    /*if (GWY_LAYER_BASIC(layer)->changed)*/ {
        gwy_pixfield_do(layer->pixbuf, data_field, layer->palette);
        GWY_LAYER_BASIC(layer)->changed = FALSE;
    }

    return layer->pixbuf;
}

static gboolean
gwy_layer_basic_wants_repaint(GwyDataViewLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), FALSE);

    return GWY_LAYER_BASIC(layer)->changed;
}

/**
 * gwy_layer_basic_set_palette:
 * @layer: A #GwyLayerBasic.
 * @palette: The palette @layer should use.
 *
 * Sets the palette @layer should used.
 **/
void
gwy_layer_basic_set_palette(GwyDataViewLayer *layer,
                            GwyPalette *palette)
{
    GwyPalette *oldpalette;
    const gchar *palette_name;

    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    g_return_if_fail(GWY_IS_PALETTE(palette));

    oldpalette = layer->palette;
    g_signal_handlers_disconnect_matched(layer->palette, G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, layer);
    g_object_ref(palette);
    layer->palette = palette;
    g_signal_connect_swapped(layer->palette, "value_changed",
                             G_CALLBACK(gwy_layer_basic_update), layer);
    palette_name = gwy_palette_def_get_name(gwy_palette_get_palette_def(palette));
    gwy_container_set_string_by_name(layer->data, "/0/base/palette",
                                     g_strdup(palette_name));
    g_object_unref(oldpalette);
}

/**
 * gwy_layer_basic_get_palette:
 * @layer: A #GwyLayerBasic.
 *
 * Returns the palette used by @layer.
 *
 * Returns: The palette as #GwyPalette.
 **/
GwyPalette*
gwy_layer_basic_get_palette(GwyDataViewLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);

    return layer->palette;
}

static void
gwy_layer_basic_plugged(GwyDataViewLayer *layer)
{
    GwyDataField *data_field;
    gint width, height;
    const gchar *palette_name;

    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));

    GWY_LAYER_BASIC(layer)->changed = TRUE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/data"));
    g_return_if_fail(data_field);
    width = gwy_data_field_get_xres(data_field);
    height = gwy_data_field_get_yres(data_field);

    if (gwy_container_contains_by_name(layer->data, "/0/base/palette")) {
        palette_name = gwy_container_get_string_by_name(layer->data,
                                                        "/0/base/palette");
        gwy_palette_set_by_name(layer->palette, palette_name);
    }
    else {
        palette_name = g_strdup(GWY_PALETTE_GRAY);
        gwy_palette_set_by_name(layer->palette, palette_name);
        gwy_container_set_string_by_name(layer->data, "/0/base/palette",
                                         palette_name);
    }
    g_signal_connect_swapped(layer->palette, "value_changed",
                             G_CALLBACK(gwy_layer_basic_update), layer);

    layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                   BITS_PER_SAMPLE, width, height);
}

static void
gwy_layer_basic_unplugged(GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));

    g_signal_handlers_disconnect_matched(layer->palette, G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, layer);
    gwy_object_unref(layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_basic_update(GwyDataViewLayer *layer)
{
    const gchar *pal;

    GWY_LAYER_BASIC(layer)->changed = TRUE;
    pal = gwy_palette_def_get_name(gwy_palette_get_palette_def(layer->palette));
    gwy_debug("%s: storing palette %s", __FUNCTION__, pal);
    gwy_container_set_string_by_name(layer->data, "/0/base/palette", pal);
    gwy_data_view_layer_updated(layer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
