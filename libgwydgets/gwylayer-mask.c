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

#include <string.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libdraw/gwypixfield.h>
#include "gwylayer-mask.h"

#define GWY_LAYER_MASK_TYPE_NAME "GwyLayerMask"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_mask_class_init        (GwyLayerMaskClass *klass);
static void       gwy_layer_mask_init              (GwyLayerMask *layer);
static void       gwy_layer_mask_finalize          (GObject *object);
static GdkPixbuf* gwy_layer_mask_paint             (GwyDataViewLayer *layer);
static gboolean   gwy_layer_mask_wants_repaint     (GwyDataViewLayer *layer);
static void       gwy_layer_mask_plugged           (GwyDataViewLayer *layer);
static void       gwy_layer_mask_unplugged         (GwyDataViewLayer *layer);
static void       gwy_layer_mask_update            (GwyDataViewLayer *layer);
static void       gwy_layer_mask_save              (GwyDataViewLayer *layer);
static void       gwy_layer_mask_restore           (GwyDataViewLayer *layer);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_mask_get_type(void)
{
    static GType gwy_layer_mask_type = 0;

    if (!gwy_layer_mask_type) {
        static const GTypeInfo gwy_layer_mask_info = {
            sizeof(GwyLayerMaskClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_mask_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerMask),
            0,
            (GInstanceInitFunc)gwy_layer_mask_init,
            NULL,
        };
        gwy_debug("%s", __FUNCTION__);
        gwy_layer_mask_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_MASK_TYPE_NAME,
                                     &gwy_layer_mask_info,
                                     0);
    }

    return gwy_layer_mask_type;
}

static void
gwy_layer_mask_class_init(GwyLayerMaskClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_mask_finalize;

    layer_class->paint = gwy_layer_mask_paint;
    layer_class->wants_repaint = gwy_layer_mask_wants_repaint;
    layer_class->plugged = gwy_layer_mask_plugged;
    layer_class->unplugged = gwy_layer_mask_unplugged;
}

static void
gwy_layer_mask_init(GwyLayerMask *layer)
{
    gwy_debug("%s", __FUNCTION__);

    layer->changed = TRUE;
    layer->color.r = layer->color.g = layer->color.b = layer->color.a = 0.0;

}

static void
gwy_layer_mask_finalize(GObject *object)
{
    GwyDataViewLayer *layer;
    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(GWY_IS_LAYER_MASK(object));
    layer = GWY_DATA_VIEW_LAYER(object);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_mask_new:
 *
 * Creates a new mask layer.
 *
 * By default, is uses a transparent color (thus not displaying anything),
 * or the color stored with the data as "/0/mask/red", "/0/mask/green",
 * "/0/mask/blue", and "/0/mask/alpha".  It gets the mask data from
 * "/0/mask".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_mask_new(void)
{
    GtkObject *object;
    GwyDataViewLayer *layer;

    gwy_debug("%s", __FUNCTION__);

    object = g_object_new(GWY_TYPE_LAYER_MASK, NULL);
    layer = (GwyDataViewLayer*)object;

    return object;
}

static GdkPixbuf*
gwy_layer_mask_paint(GwyDataViewLayer *layer)
{
    GwyDataField *data_field;
    GwyLayerMask *mask_layer;

    g_return_val_if_fail(GWY_IS_LAYER_MASK(layer), NULL);
    mask_layer = (GwyLayerMask*)layer;

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/mask"));
    g_return_val_if_fail(data_field, layer->pixbuf);
    gwy_layer_mask_restore(layer);
    /*if (GWY_LAYER_MASK(layer)->changed)*/ {
        gwy_pixfield_do_mask(layer->pixbuf, data_field, &mask_layer->color);
        GWY_LAYER_MASK(layer)->changed = FALSE;
    }

    return layer->pixbuf;
}

static gboolean
gwy_layer_mask_wants_repaint(GwyDataViewLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_MASK(layer), FALSE);

    return GWY_LAYER_MASK(layer)->changed;
}

/**
 * gwy_layer_mask_set_color:
 * @layer: A #GwyLayerMask.
 * @color: The color @layer should use.
 *
 * Sets the color @layer should used.
 **/
void
gwy_layer_mask_set_color(GwyDataViewLayer *layer,
                         GwyRGBA *color)
{
    GwyLayerMask *mask_layer;

    g_return_if_fail(GWY_IS_LAYER_MASK(layer));
    g_return_if_fail(color);

    mask_layer = (GwyLayerMask*)layer;
    memcpy(&mask_layer->color, color, sizeof(GwyRGBA));
    gwy_layer_mask_save(layer);
    gwy_layer_mask_update(layer);
}

/**
 * gwy_layer_mask_get_color:
 * @layer: A #GwyLayerMask.
 *
 * Returns the color used by @layer.
 *
 * Returns: The color as #GwyPalette.
 **/
GwyRGBA
gwy_layer_mask_get_color(GwyDataViewLayer *layer)
{
    GwyRGBA none = { 0, 0, 0, 0 };

    g_return_val_if_fail(GWY_IS_LAYER_MASK(layer), none);

    return GWY_LAYER_MASK(layer)->color;
}

static void
gwy_layer_mask_plugged(GwyDataViewLayer *layer)
{
    GwyDataField *data_field;
    GwyLayerMask *mask_layer;
    gint width, height;

    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_MASK(layer));

    mask_layer = (GwyLayerMask*)layer;
    mask_layer->changed = TRUE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/mask"));
    g_return_if_fail(data_field);
    width = gwy_data_field_get_xres(data_field);
    height = gwy_data_field_get_yres(data_field);
    gwy_layer_mask_restore(layer);

    layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE,
                                   BITS_PER_SAMPLE, width, height);
}

static void
gwy_layer_mask_unplugged(GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_LAYER_MASK(layer));

    gwy_object_unref(layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_mask_update(GwyDataViewLayer *layer)
{
    GWY_LAYER_MASK(layer)->changed = TRUE;
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_mask_save(GwyDataViewLayer *layer)
{
    GwyLayerMask *mask_layer = GWY_LAYER_MASK(layer);
    GwyRGBA *c;

    c = &mask_layer->color;
    /* TODO Container */
    gwy_container_set_double_by_name(layer->data, "/0/mask/red", c->r);
    gwy_container_set_double_by_name(layer->data, "/0/mask/green", c->g);
    gwy_container_set_double_by_name(layer->data, "/0/mask/blue", c->b);
    gwy_container_set_double_by_name(layer->data, "/0/mask/alpha", c->a);
}

static void
gwy_layer_mask_restore(GwyDataViewLayer *layer)
{
    GwyLayerMask *mask_layer = GWY_LAYER_MASK(layer);
    GwyRGBA *c;
    static const GwyRGBA default_color = { 1.0, 0.0, 0.0, 0.5 };

    c = &mask_layer->color;
    *c = default_color;
    /* TODO Container */
    if (gwy_container_contains_by_name(layer->data, "/0/mask/red"))
        c->r = gwy_container_get_double_by_name(layer->data, "/0/mask/red");
    if (gwy_container_contains_by_name(layer->data, "/0/mask/green"))
        c->g = gwy_container_get_double_by_name(layer->data, "/0/mask/green");
    if (gwy_container_contains_by_name(layer->data, "/0/mask/blue"))
        c->b = gwy_container_get_double_by_name(layer->data, "/0/mask/blue");
    if (gwy_container_contains_by_name(layer->data, "/0/mask/alpha"))
        c->a = gwy_container_get_double_by_name(layer->data, "/0/mask/alpha");
    gwy_debug("%s: r = %f, g = %f, b = %f, a = %f", __FUNCTION__,
              c->r, c->g, c->b, c->a);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
