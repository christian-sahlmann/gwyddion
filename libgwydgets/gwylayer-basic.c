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
#include <libgwyddion/gwydebugobjects.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/stats.h>
#include <libdraw/gwypixfield.h>
#include "gwydgetenums.h"
#include "gwylayer-basic.h"

#define GWY_LAYER_BASIC_TYPE_NAME "GwyLayerBasic"

#define BITS_PER_SAMPLE 8

static void gwy_layer_basic_class_init           (GwyLayerBasicClass *klass);
static void gwy_layer_basic_init                 (GwyLayerBasic *layer);
static void gwy_layer_basic_destroy              (GtkObject *object);
static GdkPixbuf* gwy_layer_basic_paint          (GwyPixmapLayer *layer);
static void gwy_layer_basic_plugged              (GwyDataViewLayer *layer);
static void gwy_layer_basic_unplugged            (GwyDataViewLayer *layer);
static void gwy_layer_basic_gradient_connect     (GwyLayerBasic *layer);
static void gwy_layer_basic_gradient_disconnect  (GwyLayerBasic *layer);
static void gwy_layer_basic_set_key              (GwyLayerBasic *layer,
                                                  const gchar *key,
                                                  GQuark *quark,
                                                  gulong *id);
static void gwy_layer_basic_container_connect    (GwyLayerBasic *basic_layer,
                                                  const gchar *data_key_string,
                                                  gulong *id,
                                                  GCallback callback);
static void gwy_layer_basic_gradient_item_changed(GwyLayerBasic *basic_layer);
static void gwy_layer_basic_changed              (GwyPixmapLayer *pixmap_layer);

static GwyPixmapLayerClass *parent_class = NULL;

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
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyPixmapLayerClass *pixmap_class = GWY_PIXMAP_LAYER_CLASS(klass);

    parent_class = g_type_class_peek_parent(klass);

    object_class->destroy = gwy_layer_basic_destroy;

    layer_class->plugged = gwy_layer_basic_plugged;
    layer_class->unplugged = gwy_layer_basic_unplugged;

    pixmap_class->paint = gwy_layer_basic_paint;
}

static void
gwy_layer_basic_init(G_GNUC_UNUSED GwyLayerBasic *layer)
{
}

static void
gwy_layer_basic_destroy(GtkObject *object)
{
    GwyLayerBasic *layer;

    layer = GWY_LAYER_BASIC(object);
    gwy_object_unref(layer->gradient);

    GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

/**
 * gwy_layer_basic_new:
 *
 * Creates a new basic data displaying layer.
 *
 * Returns: The newly created layer.
 **/
GwyPixmapLayer*
gwy_layer_basic_new(void)
{
    GwyLayerBasic *layer;

    layer = g_object_new(GWY_TYPE_LAYER_BASIC, NULL);

    return (GwyPixmapLayer*)layer;
}

static GdkPixbuf*
gwy_layer_basic_paint(GwyPixmapLayer *layer)
{
    GwyLayerBasic *basic_layer;
    GwyDataField *data_field;
    GwyLayerBasicRangeType range_type;
    GwyContainer *data;
    gdouble min, max;

    basic_layer = GWY_LAYER_BASIC(layer);
    data = GWY_DATA_VIEW_LAYER(layer)->data;

    /* TODO: We were special-casing "/0/show" here to ignore fixed range.
     * Move the logic where it belongs... */
    data_field = GWY_DATA_FIELD(layer->data_field);
    g_return_val_if_fail(data_field, NULL);

    range_type = GWY_LAYER_BASIC_RANGE_FULL;
    if (basic_layer->range_type_key)
        gwy_container_gis_enum(data, basic_layer->range_type_key, &range_type);

    /* Special-case full range, as gwy_pixbuf_draw_data_field() is simplier,
     * it doesn't have to deal with outliers */
    if (range_type == GWY_LAYER_BASIC_RANGE_FULL)
        gwy_pixbuf_draw_data_field(layer->pixbuf, data_field,
                                   basic_layer->gradient);
    else {
        gwy_layer_basic_get_range(basic_layer, &min, &max);
        gwy_pixbuf_draw_data_field_with_range(layer->pixbuf, data_field,
                                              basic_layer->gradient,
                                              min, max);
    }

    return layer->pixbuf;
}

static void
gwy_layer_basic_gradient_connect(GwyLayerBasic *basic_layer)
{
    GwyDataViewLayer *layer;
    GwyGradient *gradient;
    const guchar *s;

    g_return_if_fail(!basic_layer->gradient);
    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    /* FIXME: original implementation set /0/base/palette to default value
     * if unset. */
    if (basic_layer->gradient_key
        && gwy_container_gis_string(layer->data, basic_layer->gradient_key, &s)
        && (gradient = gwy_gradients_get_gradient(s))) {
        basic_layer->gradient = gradient;
    }
    else
        basic_layer->gradient
            = gwy_gradients_get_gradient(GWY_GRADIENT_DEFAULT);

    g_object_ref(basic_layer->gradient);
    basic_layer->gradient_id
        = g_signal_connect_swapped(basic_layer->gradient, "data_changed",
                                   G_CALLBACK(gwy_layer_basic_changed), layer);
}

static void
gwy_layer_basic_gradient_disconnect(GwyLayerBasic *layer)
{
    if (!layer->gradient)
        return;

    g_signal_handler_disconnect(layer->gradient, layer->gradient_id);
    layer->gradient_id = 0;
    gwy_object_unref(layer->gradient);
}

static void
gwy_layer_basic_plugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerBasic *basic_layer;
    GwyDataField *data_field = NULL;
    gint width, height;

    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    basic_layer = GWY_LAYER_BASIC(layer);

    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);

    data_field = GWY_DATA_FIELD(pixmap_layer->data_field);
    g_return_if_fail(data_field);

    gwy_layer_basic_container_connect
                            (basic_layer,
                             g_quark_to_string(basic_layer->gradient_key),
                             &basic_layer->gradient_item_id,
                             G_CALLBACK(gwy_layer_basic_gradient_item_changed));
    gwy_layer_basic_gradient_connect(basic_layer);

    width = gwy_data_field_get_xres(data_field);
    height = gwy_data_field_get_yres(data_field);
    pixmap_layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                          BITS_PER_SAMPLE, width, height);
    gwy_debug_objects_creation(G_OBJECT(pixmap_layer->pixbuf));
}

static void
gwy_layer_basic_unplugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerBasic *basic_layer;

    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    basic_layer = GWY_LAYER_BASIC(layer);

    if (basic_layer->gradient_item_id) {
        g_signal_handler_disconnect(layer->data, basic_layer->gradient_item_id);
        basic_layer->gradient_item_id = 0;
    }
    gwy_layer_basic_gradient_disconnect(basic_layer);
    gwy_object_unref(pixmap_layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

/**
 * gwy_layer_basic_set_gradient_key:
 * @basic_layer: A basic data view layer.
 * @key: Container string key identifying the color gradient to use.
 *
 * Sets the gradient to use to visualize data.
 **/
void
gwy_layer_basic_set_gradient_key(GwyLayerBasic *basic_layer,
                                 const gchar *key)
{
    GwyDataViewLayer *layer;
    GQuark quark;

    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));

    quark = key ? g_quark_from_string(key) : 0;
    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    if (!layer->data || basic_layer->gradient_key == quark) {
        basic_layer->gradient_key = quark;
        return;
    }

    if (basic_layer->gradient_item_id)
        g_signal_handler_disconnect(layer->data, basic_layer->gradient_item_id);
    basic_layer->gradient_item_id = 0;
    gwy_layer_basic_gradient_disconnect(basic_layer);
    basic_layer->gradient_key = quark;
    gwy_layer_basic_gradient_connect(basic_layer);
    gwy_layer_basic_container_connect
                            (basic_layer, key,
                             &basic_layer->gradient_item_id,
                             G_CALLBACK(gwy_layer_basic_gradient_item_changed));

    GWY_PIXMAP_LAYER(layer)->wants_repaint = TRUE;
    gwy_data_view_layer_updated(layer);
}

/**
 * gwy_layer_basic_get_gradient_key:
 * @basic_layer: A basic data view layer.
 *
 * Gets key identifying color gradient.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_layer_basic_get_gradient_key(GwyLayerBasic *basic_layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(basic_layer), NULL);
    return g_quark_to_string(basic_layer->gradient_key);
}

/**
 * gwy_layer_basic_set_range_type_key:
 * @basic_layer: A basic data view layer.
 * @key: Container string key identifying the range type to use.
 *
 * Sets the color range mapping type to use to visualize data.
 **/
void
gwy_layer_basic_set_range_type_key(GwyLayerBasic *basic_layer,
                                   const gchar *key)
{
    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));
    gwy_layer_basic_set_key(basic_layer, key,
                            &basic_layer->range_type_key,
                            &basic_layer->range_type_id);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(basic_layer));
}

/**
 * gwy_layer_basic_get_range_type_key:
 * @basic_layer: A basic data view layer.
 *
 * Gets key identifying color range mapping type.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_layer_basic_get_range_type_key(GwyLayerBasic *basic_layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(basic_layer), NULL);
    return g_quark_to_string(basic_layer->range_type_key);
}

/**
 * gwy_layer_basic_set_min_max_key:
 * @basic_layer: A basic data view layer.
 * @prefix: Prefix of keys identifying minimum and maximum values for fixed
 *          range, "/min" and "/max" is appended to it to get the individual
 *          minimum and maximum keys.
 *
 * Sets fixed range minimum and maximum.
 **/
void
gwy_layer_basic_set_min_max_key(GwyLayerBasic *basic_layer,
                                const gchar *prefix)
{
    gchar *key = NULL;
    guint len;

    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));
    if (prefix) {
        len = strlen(prefix);
        key = g_newa(gchar, len + sizeof("/min"));
        g_stpcpy(g_stpcpy(key, prefix), "/min");
        gwy_layer_basic_set_key(basic_layer, key,
                                &basic_layer->min_key, &basic_layer->min_id);
        key[len + 2] = 'a';
        key[len + 3] = 'x';
        gwy_layer_basic_set_key(basic_layer, key,
                                &basic_layer->max_key, &basic_layer->max_id);
    }
    else {
        gwy_layer_basic_set_key(basic_layer, NULL,
                                &basic_layer->min_key, &basic_layer->min_id);
        gwy_layer_basic_set_key(basic_layer, NULL,
                                &basic_layer->max_key, &basic_layer->max_id);
    }
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(basic_layer));
}

/**
 * gwy_layer_basic_get_min_max_key:
 * @basic_layer: A basic data view layer.
 *
 * Gets prefix identifying fixed range minimum and maximum.
 *
 * Returns: The prefix, or %NULL if it isn't set.
 **/
const gchar*
gwy_layer_basic_get_min_max_key(GwyLayerBasic *basic_layer)
{
    const gchar *prefix;
    guint len;
    gchar *s;

    g_return_val_if_fail(GWY_IS_LAYER_BASIC(basic_layer), NULL);
    prefix = g_quark_to_string(basic_layer->min_key);
    if (!prefix)
        return NULL;

    len = strlen(prefix);
    g_assert(len >= 4);
    s = g_newa(gchar, len-3);
    g_strlcpy(s, prefix, len-4);

    /* Eventually instantiate the quark string and return this one */
    return g_quark_to_string(g_quark_from_string(s));
}

/**
 * gwy_layer_basic_get_range:
 * @basic_layer: A basic data view layer.
 * @min: Location to store range minimum to.
 * @max: Location to store range maximum to.
 *
 * Gets the range colors are mapped from in current mode.
 **/
void
gwy_layer_basic_get_range(GwyLayerBasic *basic_layer,
                          gdouble *min,
                          gdouble *max)
{
    GwyContainer *data;
    GwyDataField *data_field;
    GwyLayerBasicRangeType range_type;
    gdouble rmin, rmax;

    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));
    data = GWY_DATA_VIEW_LAYER(basic_layer)->data;
    data_field = GWY_DATA_FIELD(GWY_PIXMAP_LAYER(basic_layer)->data_field);
    g_return_if_fail(data && data_field);

    range_type = GWY_LAYER_BASIC_RANGE_FULL;
    if (basic_layer->range_type_key)
        gwy_container_gis_enum(data, basic_layer->range_type_key, &range_type);

    switch (range_type) {
        case GWY_LAYER_BASIC_RANGE_FULL:
        rmin = gwy_data_field_get_min(data_field);
        rmax = gwy_data_field_get_max(data_field);
        break;

        case GWY_LAYER_BASIC_RANGE_FIXED:
        if (!basic_layer->min_key
            || !gwy_container_gis_double(data, basic_layer->min_key, &rmin))
            rmin = gwy_data_field_get_min(data_field);

        if (!basic_layer->max_key
            || !gwy_container_gis_double(data, basic_layer->max_key, &rmax))
            rmax = gwy_data_field_get_max(data_field);
        break;

        case GWY_LAYER_BASIC_RANGE_AUTO:
        gwy_data_field_get_autorange(data_field, &rmin, &rmax);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    if (min)
        *min = rmin;
    if (max)
        *max = rmax;
}

static void
gwy_layer_basic_set_key(GwyLayerBasic *basic_layer,
                        const gchar *key,
                        GQuark *quark,
                        gulong *id)
{
    GwyDataViewLayer *layer;
    GQuark q;

    q = key ? g_quark_from_string(key) : 0;
    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    if (!layer->data || *quark == q) {
        *quark = q;
        return;
    }

    if (*id) {
        g_signal_handler_disconnect(layer->data, *id);
        *id = 0;
    }
    if ((*quark = q))
        gwy_layer_basic_container_connect(basic_layer, key, id,
                                          G_CALLBACK(gwy_layer_basic_changed));

    GWY_PIXMAP_LAYER(layer)->wants_repaint = TRUE;
}

static void
gwy_layer_basic_container_connect(GwyLayerBasic *basic_layer,
                                  const gchar *data_key_string,
                                  gulong *id,
                                  GCallback callback)
{
    GwyDataViewLayer *layer;
    gchar *detailed_signal;

    if (!data_key_string)
        return;
    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    detailed_signal = g_newa(gchar, sizeof("item_changed::")
                                    + strlen(data_key_string));
    g_stpcpy(g_stpcpy(detailed_signal, "item_changed::"), data_key_string);

    *id = g_signal_connect_swapped(layer->data, detailed_signal,
                                   G_CALLBACK(callback), layer);
}

static void
gwy_layer_basic_gradient_item_changed(GwyLayerBasic *basic_layer)
{
    gwy_layer_basic_gradient_disconnect(basic_layer);
    gwy_layer_basic_gradient_connect(basic_layer);
    GWY_PIXMAP_LAYER(basic_layer)->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(basic_layer));
}

static void
gwy_layer_basic_changed(GwyPixmapLayer *pixmap_layer)
{
    pixmap_layer->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(pixmap_layer));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
