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
static GdkPixbuf* gwy_layer_basic_paint             (GwyPixmapLayer *layer);
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
        gwy_debug("");
        gwy_layer_basic_type
            = g_type_register_static(GWY_TYPE_PIXMAP_LAYER,
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
    GwyPixmapLayerClass *pixmap_class = GWY_PIXMAP_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_basic_finalize;

    layer_class->wants_repaint = gwy_layer_basic_wants_repaint;
    layer_class->plugged = gwy_layer_basic_plugged;
    layer_class->unplugged = gwy_layer_basic_unplugged;

    pixmap_class->paint = gwy_layer_basic_paint;
}

static void
gwy_layer_basic_init(GwyLayerBasic *layer)
{
    gwy_debug("");

    layer->changed = TRUE;
    layer->palette = NULL;
}

static void
gwy_layer_basic_finalize(GObject *object)
{
    GwyLayerBasic *layer;
    gwy_debug("");

    g_return_if_fail(GWY_IS_LAYER_BASIC(object));
    layer = GWY_LAYER_BASIC(object);

    gwy_object_unref(layer->palette);

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
    GwyLayerBasic *layer;

    gwy_debug("");

    object = g_object_new(GWY_TYPE_LAYER_BASIC, NULL);
    layer = (GwyLayerBasic*)object;

    layer->palette = (GwyPalette*)(gwy_palette_new(NULL));
    /*
    gwy_palette_set_by_name(layer->palette, GWY_PALETTE_GRAY);
    g_signal_connect_swapped(layer->palette, "value_changed",
                             G_CALLBACK(gwy_layer_basic_update), layer);
                             */

    return object;
}

static GdkPixbuf*
gwy_layer_basic_paint(GwyPixmapLayer *layer)
{
    GwyDataField *data_field;
    GwyLayerBasic *basic_layer;
    GwyContainer *data;
    gdouble min = 0.0, max = 0.0;
    gboolean fixedrange = FALSE;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);
    basic_layer = GWY_LAYER_BASIC(layer);
    data = GWY_DATA_VIEW_LAYER(layer)->data;

    /* TODO Container */
    if (gwy_container_contains_by_name(data, "/0/show"))
        data_field
            = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/show"));
    else
        data_field
            = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    g_return_val_if_fail(data_field, layer->pixbuf);
    if (gwy_container_contains_by_name(data, "/0/base/min")
        && gwy_container_contains_by_name(data, "/0/base/max")) {
        min = gwy_container_get_double_by_name(data, "/0/base/min");
        max = gwy_container_get_double_by_name(data, "/0/base/max");
        fixedrange = TRUE;
    }
    /* XXX */
    /*if (GWY_LAYER_BASIC(layer)->changed)*/ {
        if (fixedrange)
            gwy_pixfield_do_with_range(layer->pixbuf, data_field,
                                       basic_layer->palette, min, max);
        else
            gwy_pixfield_do(layer->pixbuf, data_field, basic_layer->palette);
        basic_layer->changed = FALSE;
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
gwy_layer_basic_set_palette(GwyLayerBasic *layer,
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
    palette_name
        = gwy_palette_def_get_name(gwy_palette_get_palette_def(palette));
    gwy_container_set_string_by_name(GWY_DATA_VIEW_LAYER(layer)->data,
                                     "/0/base/palette",
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
gwy_layer_basic_get_palette(GwyLayerBasic *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);

    return layer->palette;
}

static void
gwy_layer_basic_plugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerBasic *basic_layer;
    GwyDataField *data_field;
    gint width, height;
    const gchar *palette_name;

    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    basic_layer = GWY_LAYER_BASIC(layer);

    basic_layer->changed = TRUE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);

    /* TODO Container */
    /* XXX */
    if (gwy_container_contains_by_name(layer->data, "/0/show"))
        data_field = GWY_DATA_FIELD(
                         gwy_container_get_object_by_name(layer->data,
                                                          "/0/show"));
    else
        data_field = GWY_DATA_FIELD(
                         gwy_container_get_object_by_name(layer->data,
                                                          "/0/data"));
    g_return_if_fail(data_field);
    width = gwy_data_field_get_xres(data_field);
    height = gwy_data_field_get_yres(data_field);

    if (gwy_container_contains_by_name(layer->data, "/0/base/palette")) {
        palette_name = gwy_container_get_string_by_name(layer->data,
                                                        "/0/base/palette");
        gwy_palette_set_by_name(basic_layer->palette, palette_name);
    }
    else {
        palette_name = g_strdup(GWY_PALETTE_GRAY);
        gwy_palette_set_by_name(basic_layer->palette, palette_name);
        gwy_container_set_string_by_name(layer->data, "/0/base/palette",
                                         palette_name);
    }
    g_signal_connect_swapped(basic_layer->palette, "value_changed",
                             G_CALLBACK(gwy_layer_basic_update), layer);

    pixmap_layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                          BITS_PER_SAMPLE, width, height);
}

static void
gwy_layer_basic_unplugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerBasic *basic_layer;

    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    basic_layer = GWY_LAYER_BASIC(layer);

    g_signal_handlers_disconnect_matched(basic_layer->palette,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, layer);
    gwy_object_unref(pixmap_layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_basic_update(GwyDataViewLayer *layer)
{
    GwyLayerBasic *basic_layer;
    GwyPalette *palette;
    const gchar *pal;

    GWY_LAYER_BASIC(layer)->changed = TRUE;
    basic_layer = GWY_LAYER_BASIC(layer);
    palette = basic_layer->palette;
    pal = gwy_palette_def_get_name(gwy_palette_get_palette_def(palette));
    gwy_debug("storing palette %s", pal);
    gwy_container_set_string_by_name(layer->data, "/0/base/palette",
                                     g_strdup(pal));
    gwy_data_view_layer_updated(layer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
