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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libprocess/stats.h>
#include <libdraw/gwypixfield.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwydgets/gwylayer-basic.h>

#define connect_swapped_after(obj, signal, cb, data) \
    g_signal_connect_object(obj, signal, G_CALLBACK(cb), data, \
                            G_CONNECT_SWAPPED | G_CONNECT_AFTER);

enum {
    PRESENTATION_SWITCHED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_GRADIENT_KEY,
    PROP_PRESENTATION_KEY,
    PROP_RANGE_TYPE_KEY,
    PROP_MIN_MAX_KEY
};

static void gwy_layer_basic_destroy              (GtkObject *object);
static void gwy_layer_basic_set_property         (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void gwy_layer_basic_get_property         (GObject *object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static GdkPixbuf* gwy_layer_basic_paint          (GwyPixmapLayer *layer);
static void gwy_layer_basic_plugged              (GwyDataViewLayer *layer);
static void gwy_layer_basic_unplugged            (GwyDataViewLayer *layer);
static void gwy_layer_basic_gradient_connect     (GwyLayerBasic *layer);
static void gwy_layer_basic_gradient_disconnect  (GwyLayerBasic *layer);
static void gwy_layer_basic_get_fixed_range      (GwyLayerBasic *basic_layer,
                                                  GwyContainer *container,
                                                  GwyDataField *data_field,
                                                  gdouble *rmin,
                                                  gdouble *rmax);
static void gwy_layer_basic_reconnect_fixed      (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_connect_fixed        (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_disconnect_fixed     (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_item_changed         (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_show_field_connect   (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_show_field_disconnect(GwyLayerBasic *basic_layer);
static void gwy_layer_basic_container_connect    (GwyLayerBasic *basic_layer,
                                                  const gchar *data_key_string,
                                                  gulong *id,
                                                  GCallback callback);
static void gwy_layer_basic_gradient_item_changed(GwyLayerBasic *basic_layer);
static void gwy_layer_basic_show_changed         (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_range_type_changed   (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_min_max_changed      (GwyLayerBasic *basic_layer);
static void gwy_layer_basic_changed              (GwyPixmapLayer *pixmap_layer);

static guint basic_layer_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyLayerBasic, gwy_layer_basic, GWY_TYPE_PIXMAP_LAYER)

static void
gwy_layer_basic_class_init(GwyLayerBasicClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyPixmapLayerClass *pixmap_class = GWY_PIXMAP_LAYER_CLASS(klass);

    gobject_class->set_property = gwy_layer_basic_set_property;
    gobject_class->get_property = gwy_layer_basic_get_property;

    object_class->destroy = gwy_layer_basic_destroy;

    layer_class->plugged = gwy_layer_basic_plugged;
    layer_class->unplugged = gwy_layer_basic_unplugged;

    pixmap_class->paint = gwy_layer_basic_paint;

    /**
     * GwyLayerBasic:gradient-key:
     *
     * The :gradient-key property is the container key used to identify
     * #GwyGradient data is colored with.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_GRADIENT_KEY,
         g_param_spec_string("gradient-key",
                             "Gradient key",
                             "Key identifying gradient in container",
                             NULL, G_PARAM_READWRITE));

    /**
     * GwyLayerBasic:presentation-key:
     *
     * The :presentation-key property is the container key used to identify
     * presentation data field .
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_PRESENTATION_KEY,
         g_param_spec_string("presentation-key",
                             "Presentation key",
                             "Key identifying presentation data in container",
                             NULL, G_PARAM_READWRITE));

    /**
     * GwyLayerBasic:range-type-key:
     *
     * The :range-type-key property is the container key used to identify
     * color range type.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_RANGE_TYPE_KEY,
         g_param_spec_string("range-type-key",
                             "Range type key",
                             "Key identifying color range type in container",
                             NULL, G_PARAM_READWRITE));

    /**
     * GwyLayerBasic:min-max-key:
     *
     * The :min-max-key property is the container key prefix used to identify
     * fixed range minimum and maximum.
     **/
    g_object_class_install_property
        (gobject_class,
         PROP_MIN_MAX_KEY,
         g_param_spec_string("min-max-key",
                             "Min, max key",
                             "Key prefix identifying fixed range minimum and "
                             "maximum in container",
                             NULL, G_PARAM_READWRITE));


    /**
     * GwyLayerBasic::presentation-switched:
     * @gwydataview: The #GwyLayerBasic which received the signal.
     *
     * The ::presentation-switched signal is emitted when the presentation
     * state changes, that is when a layer which displayed data directly
     * starts displaying a presentation, or conversely when it stops displaying
     * a presentation.
     **/
    basic_layer_signals[PRESENTATION_SWITCHED]
        = g_signal_new("presentation-switched",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyLayerBasicClass,
                                       presentation_switched),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
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
    if (layer->gradient) {
        gwy_resource_release(GWY_RESOURCE(layer->gradient));
        layer->gradient = NULL;
    }

    GTK_OBJECT_CLASS(gwy_layer_basic_parent_class)->destroy(object);
}

static void
gwy_layer_basic_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerBasic *basic_layer = GWY_LAYER_BASIC(object);

    switch (prop_id) {
        case PROP_GRADIENT_KEY:
        gwy_layer_basic_set_gradient_key(basic_layer,
                                         g_value_get_string(value));
        break;

        case PROP_PRESENTATION_KEY:
        gwy_layer_basic_set_presentation_key(basic_layer,
                                         g_value_get_string(value));
        break;

        case PROP_RANGE_TYPE_KEY:
        gwy_layer_basic_set_range_type_key(basic_layer,
                                           g_value_get_string(value));
        break;

        case PROP_MIN_MAX_KEY:
        gwy_layer_basic_set_min_max_key(basic_layer,
                                        g_value_get_string(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_basic_get_property(GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerBasic *basic_layer = GWY_LAYER_BASIC(object);

    switch (prop_id) {
        case PROP_GRADIENT_KEY:
        g_value_set_static_string(value,
                                  g_quark_to_string(basic_layer->gradient_key));
        break;

        case PROP_PRESENTATION_KEY:
        g_value_set_static_string(value,
                                  g_quark_to_string(basic_layer->show_key));
        break;

        case PROP_RANGE_TYPE_KEY:
        g_value_set_static_string(value,
                                  g_quark_to_string(basic_layer->range_type_key));
        break;

        case PROP_MIN_MAX_KEY:
        g_value_set_static_string(value,
                                  g_quark_to_string(basic_layer->fixed_key));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
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

    data_field = GWY_DATA_FIELD(layer->data_field);
    if (basic_layer->show_field)
        data_field = GWY_DATA_FIELD(basic_layer->show_field);
    g_return_val_if_fail(data && data_field, NULL);

    range_type = GWY_LAYER_BASIC_RANGE_FULL;
    if (!basic_layer->show_field && basic_layer->range_type_key)
        gwy_container_gis_enum(data, basic_layer->range_type_key, &range_type);

    /* Special-case full range, as gwy_pixbuf_draw_data_field() is simplier,
     * it doesn't have to deal with outliers */
    gwy_pixmap_layer_make_pixbuf(layer, FALSE);
    if (range_type == GWY_LAYER_BASIC_RANGE_FULL)
        gwy_pixbuf_draw_data_field(layer->pixbuf, data_field,
                                   basic_layer->gradient);
    else if (range_type == GWY_LAYER_BASIC_RANGE_ADAPT)
        gwy_pixbuf_draw_data_field_adaptive(layer->pixbuf, data_field,
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
    const guchar *s;

    g_return_if_fail(!basic_layer->gradient);
    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    /* FIXME: original implementation set /0/base/palette to default value
     * if unset. */
    s = NULL;
    if (basic_layer->gradient_key)
        gwy_container_gis_string(layer->data, basic_layer->gradient_key, &s);
    basic_layer->gradient = gwy_gradients_get_gradient(s);
    gwy_resource_use(GWY_RESOURCE(basic_layer->gradient));
    basic_layer->gradient_id
        = g_signal_connect_swapped(basic_layer->gradient, "data-changed",
                                   G_CALLBACK(gwy_layer_basic_changed), layer);
}

static void
gwy_layer_basic_gradient_disconnect(GwyLayerBasic *layer)
{
    if (!layer->gradient)
        return;

    g_signal_handler_disconnect(layer->gradient, layer->gradient_id);
    layer->gradient_id = 0;
    gwy_resource_release(GWY_RESOURCE(layer->gradient));
    layer->gradient = NULL;
}

static void
gwy_layer_basic_plugged(GwyDataViewLayer *layer)
{
    GwyLayerBasic *basic_layer;

    basic_layer = GWY_LAYER_BASIC(layer);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_basic_parent_class)->plugged(layer);

    gwy_layer_basic_container_connect
                            (basic_layer,
                             g_quark_to_string(basic_layer->show_key),
                             &basic_layer->show_item_id,
                             G_CALLBACK(gwy_layer_basic_item_changed));
    gwy_layer_basic_show_field_connect(basic_layer);
    gwy_layer_basic_container_connect
                            (basic_layer,
                             g_quark_to_string(basic_layer->gradient_key),
                             &basic_layer->gradient_item_id,
                             G_CALLBACK(gwy_layer_basic_gradient_item_changed));
    gwy_layer_basic_gradient_connect(basic_layer);
    gwy_layer_basic_container_connect
                              (basic_layer,
                               g_quark_to_string(basic_layer->range_type_key),
                               &basic_layer->range_type_id,
                               G_CALLBACK(gwy_layer_basic_range_type_changed));
    gwy_layer_basic_reconnect_fixed(basic_layer);
}

static void
gwy_layer_basic_unplugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerBasic *basic_layer;

    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    basic_layer = GWY_LAYER_BASIC(layer);

    gwy_debug("disconnecting all handlers");

    gwy_layer_basic_disconnect_fixed(basic_layer);
    if (basic_layer->range_type_id)
        g_signal_handler_disconnect(layer->data, basic_layer->range_type_id);
    if (basic_layer->gradient_item_id)
        g_signal_handler_disconnect(layer->data, basic_layer->gradient_item_id);
    gwy_layer_basic_gradient_disconnect(basic_layer);
    if (basic_layer->show_item_id)
        g_signal_handler_disconnect(layer->data, basic_layer->show_item_id);
    gwy_layer_basic_show_field_disconnect(basic_layer);

    basic_layer->range_type_id = 0;
    basic_layer->gradient_item_id = 0;
    basic_layer->show_item_id = 0;

    gwy_object_unref(pixmap_layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_basic_parent_class)->unplugged(layer);
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
    GwyDataViewLayer *layer;
    GQuark q;

    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));
    q = key ? g_quark_from_string(key) : 0;
    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    if (!layer->data || basic_layer->range_type_key == q) {
        basic_layer->range_type_key = q;
        return;
    }

    if (basic_layer->range_type_id)
        g_signal_handler_disconnect(layer->data, basic_layer->range_type_id);
    basic_layer->range_type_key = q;
    if (q)
        gwy_layer_basic_container_connect
                               (basic_layer, key, &basic_layer->range_type_id,
                                G_CALLBACK(gwy_layer_basic_range_type_changed));
    gwy_layer_basic_reconnect_fixed(basic_layer);

    GWY_PIXMAP_LAYER(layer)->wants_repaint = TRUE;
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
 * Sets basic layer fixed range minimum and maximum.
 **/
void
gwy_layer_basic_set_min_max_key(GwyLayerBasic *basic_layer,
                                const gchar *prefix)
{
    GwyDataViewLayer *layer;
    GQuark quark;

    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));

    quark = prefix ? g_quark_from_string(prefix) : 0;
    if (quark == basic_layer->fixed_key)
        return;

    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    basic_layer->fixed_key = quark;
    if (!quark)
        gwy_layer_basic_disconnect_fixed(basic_layer);
    else
        gwy_layer_basic_reconnect_fixed(basic_layer);

    GWY_PIXMAP_LAYER(layer)->wants_repaint = TRUE;
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
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(basic_layer), NULL);
    return g_quark_to_string(basic_layer->fixed_key);
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
    GwyPixmapLayer *pixmap_layer;
    GwyContainer *data;
    GwyDataField *data_field;
    GwyLayerBasicRangeType range_type;
    gdouble rmin, rmax;

    /* FIXME: Should we return something else for presentations? */
    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));
    data = GWY_DATA_VIEW_LAYER(basic_layer)->data;
    pixmap_layer = GWY_PIXMAP_LAYER(basic_layer);
    data_field = GWY_DATA_FIELD(pixmap_layer->data_field);
    g_return_if_fail(data && data_field);

    range_type = GWY_LAYER_BASIC_RANGE_FULL;
    if (basic_layer->range_type_key)
        gwy_container_gis_enum(data, basic_layer->range_type_key, &range_type);

    switch (range_type) {
        case GWY_LAYER_BASIC_RANGE_FULL:
        case GWY_LAYER_BASIC_RANGE_ADAPT:
        rmin = gwy_data_field_get_min(data_field);
        rmax = gwy_data_field_get_max(data_field);
        break;

        case GWY_LAYER_BASIC_RANGE_FIXED:
        gwy_layer_basic_get_fixed_range(basic_layer, data, data_field,
                                        &rmin, &rmax);
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
gwy_layer_basic_get_fixed_range(GwyLayerBasic *basic_layer,
                                GwyContainer *container,
                                GwyDataField *data_field,
                                gdouble *rmin,
                                gdouble *rmax)
{
    const gchar *prefix;
    gchar *key;
    guint len;

    if (!basic_layer->fixed_key) {
        *rmin = gwy_data_field_get_min(data_field);
        *rmax = gwy_data_field_get_max(data_field);
        return;
    }

    prefix = g_quark_to_string(basic_layer->fixed_key);
    len = strlen(prefix);
    key = g_newa(gchar, len + sizeof("/min"));

    g_stpcpy(g_stpcpy(key, prefix), "/min");
    if (!gwy_container_gis_double_by_name(container, key, rmin))
        *rmin = gwy_data_field_get_min(data_field);

    strcpy(key + len + 1, "max");
    if (!gwy_container_gis_double_by_name(container, key, rmax))
        *rmax = gwy_data_field_get_max(data_field);
}

/**
 * gwy_layer_basic_set_presentation_key:
 * @basic_layer: A basic data view layer.
 * @key: Container string key identifying the data field to actually display.
 *
 * Sets the data field to actually display by a basic layer.
 *
 * The data field set with gwy_pixmap_layer_get_data_key() is used to obtain
 * values, it determines physical dimensions, etc.  When a data field is set
 * with this method, it is displayed instead of the actual data.
 **/
void
gwy_layer_basic_set_presentation_key(GwyLayerBasic *basic_layer,
                                     const gchar *key)
{
    GwyDataViewLayer *layer;
    GQuark quark;
    gboolean presentation_switched;

    g_return_if_fail(GWY_IS_LAYER_BASIC(basic_layer));

    quark = key ? g_quark_from_string(key) : 0;
    if (basic_layer->show_key == quark)
        return;

    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    if (!layer->data) {
        basic_layer->show_key = quark;
        g_object_notify(G_OBJECT(basic_layer), "presentation-key");
        return;
    }

    presentation_switched = !!basic_layer->show_field;
    if (basic_layer->show_id)
        g_signal_handler_disconnect(layer->data, basic_layer->show_id);
    basic_layer->show_id = 0;
    gwy_layer_basic_show_field_disconnect(basic_layer);
    basic_layer->show_key = quark;
    gwy_layer_basic_show_field_connect(basic_layer);
    gwy_layer_basic_container_connect(basic_layer, key,
                                      &basic_layer->show_item_id,
                                      G_CALLBACK(gwy_layer_basic_item_changed));
    presentation_switched ^= !!basic_layer->show_field;

    GWY_PIXMAP_LAYER(basic_layer)->wants_repaint = TRUE;
    g_object_notify(G_OBJECT(basic_layer), "presentation-key");
    if (presentation_switched)
        g_signal_emit(basic_layer,
                      basic_layer_signals[PRESENTATION_SWITCHED], 0);
    gwy_data_view_layer_updated(layer);
}

/**
 * gwy_layer_basic_get_presentation_key:
 * @basic_layer: A basic data view layer.
 *
 * Gets the key identifying data field this pixmap layer actually displays.
 *
 * See gwy_layer_basic_set_presentation_key() for details.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_layer_basic_get_presentation_key(GwyLayerBasic *basic_layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(basic_layer), NULL);
    return g_quark_to_string(basic_layer->show_key);
}

gboolean
gwy_layer_basic_get_has_presentation(GwyLayerBasic *basic_layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(basic_layer), FALSE);
    return basic_layer->show_field != NULL;
}

/**
 * gwy_layer_basic_reconnect_fixed:
 * @basic_layer: A basic data view layer.
 *
 * Connect to min, max container keys, or disconnect, depending on range type.
 **/
static void
gwy_layer_basic_reconnect_fixed(GwyLayerBasic *basic_layer)
{
    GwyDataViewLayer *layer;
    GwyLayerBasicRangeType range_type;

    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    range_type = GWY_LAYER_BASIC_RANGE_FULL;
    if (layer->data && basic_layer->range_type_key)
        gwy_container_gis_enum(layer->data, basic_layer->range_type_key,
                               &range_type);

    gwy_layer_basic_disconnect_fixed(basic_layer);
    if (range_type == GWY_LAYER_BASIC_RANGE_FIXED)
        gwy_layer_basic_connect_fixed(basic_layer);
}

static void
gwy_layer_basic_connect_fixed(GwyLayerBasic *basic_layer)
{
    GwyDataViewLayer *layer;
    const gchar *prefix;
    gchar *detailed_signal;
    guint len;

    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    if (!layer->data || !basic_layer->fixed_key)
        return;

    prefix = g_quark_to_string(basic_layer->fixed_key);
    len = strlen(prefix);
    detailed_signal = g_newa(gchar, len + sizeof("item-changed::")
                                    + sizeof("/min"));
    len += sizeof("item-changed::");

    g_stpcpy(g_stpcpy(g_stpcpy(detailed_signal, "item-changed::"), prefix),
             "/min");
    basic_layer->min_id = connect_swapped_after(layer->data, detailed_signal,
                                                gwy_layer_basic_min_max_changed,
                                                layer);

    strcpy(detailed_signal + len, "max");
    basic_layer->max_id = connect_swapped_after(layer->data, detailed_signal,
                                                gwy_layer_basic_min_max_changed,
                                                layer);
}

static void
gwy_layer_basic_disconnect_fixed(GwyLayerBasic *basic_layer)
{
    GwyDataViewLayer *layer;

    layer = GWY_DATA_VIEW_LAYER(basic_layer);

    if (basic_layer->min_id)
        g_signal_handler_disconnect(layer->data, basic_layer->min_id);
    if (basic_layer->max_id)
        g_signal_handler_disconnect(layer->data, basic_layer->max_id);

    basic_layer->min_id = 0;
    basic_layer->max_id = 0;
}

/**
 * gwy_layer_basic_item_changed:
 * @layer: A basic data view layer.
 * @data: Container with the presentation data field this basic layer display.
 *
 * Reconnects signals to a new data field when it was replaced in the
 * container.
 **/
static void
gwy_layer_basic_item_changed(GwyLayerBasic *basic_layer)
{
    gboolean presentation_switched;

    presentation_switched = !!basic_layer->show_field;
    gwy_layer_basic_show_field_disconnect(basic_layer);
    gwy_layer_basic_show_field_connect(basic_layer);
    presentation_switched ^= !!basic_layer->show_field;
    GWY_PIXMAP_LAYER(basic_layer)->wants_repaint = TRUE;
    if (presentation_switched)
        g_signal_emit(basic_layer,
                      basic_layer_signals[PRESENTATION_SWITCHED], 0);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(basic_layer));
}

/**
 * gwy_layer_basic_show_field_connect:
 * @basic_layer: A basic layer.
 *
 * Eventually connects to new data field's "data-changed" signal.
 **/
static void
gwy_layer_basic_show_field_connect(GwyLayerBasic *basic_layer)
{
    GwyDataViewLayer *layer;

    g_return_if_fail(!basic_layer->show_field);
    if (!basic_layer->show_key)
        return;

    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    if (!gwy_container_gis_object(layer->data, basic_layer->show_key,
                                  &basic_layer->show_field))
        return;

    g_object_ref(basic_layer->show_field);
    basic_layer->show_id
        = g_signal_connect_swapped(basic_layer->show_field,
                                   "data-changed",
                                   G_CALLBACK(gwy_layer_basic_show_changed),
                                   layer);
}

/**
 * gwy_layer_basic_show_field_disconnect:
 * @basic_layer: A basic layer.
 *
 * Disconnects from all data field's signals and drops reference to it.
 **/
static void
gwy_layer_basic_show_field_disconnect(GwyLayerBasic *basic_layer)
{
    if (!basic_layer->show_field)
        return;

    if (basic_layer->show_id)
        g_signal_handler_disconnect(basic_layer->show_field,
                                    basic_layer->show_id);
    basic_layer->show_id = 0;
    gwy_object_unref(basic_layer->show_field);
}

static void
gwy_layer_basic_container_connect(GwyLayerBasic *basic_layer,
                                  const gchar *data_key_string,
                                  gulong *id,
                                  GCallback callback)
{
    GwyDataViewLayer *layer;
    gchar *detailed_signal;

    layer = GWY_DATA_VIEW_LAYER(basic_layer);
    if (!data_key_string || !layer->data) {
        *id = 0;
        return;
    }
    detailed_signal = g_newa(gchar, sizeof("item-changed::")
                                    + strlen(data_key_string));
    g_stpcpy(g_stpcpy(detailed_signal, "item-changed::"), data_key_string);
    *id = connect_swapped_after(layer->data, detailed_signal, callback, layer);
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
gwy_layer_basic_show_changed(GwyLayerBasic *basic_layer)
{
    GWY_PIXMAP_LAYER(basic_layer)->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(basic_layer));
}

static void
gwy_layer_basic_range_type_changed(GwyLayerBasic *basic_layer)
{
    gwy_layer_basic_reconnect_fixed(basic_layer);
    gwy_layer_basic_changed(GWY_PIXMAP_LAYER(basic_layer));
}

static void
gwy_layer_basic_min_max_changed(GwyLayerBasic *basic_layer)
{
    GwyContainer *data;
    GwyLayerBasicRangeType range_type;

    data = GWY_DATA_VIEW_LAYER(basic_layer)->data;
    range_type = GWY_LAYER_BASIC_RANGE_FULL;
    if (basic_layer->range_type_key)
        gwy_container_gis_enum(data, basic_layer->range_type_key, &range_type);

    if (range_type == GWY_LAYER_BASIC_RANGE_FIXED)
        gwy_layer_basic_changed(GWY_PIXMAP_LAYER(basic_layer));
}

static void
gwy_layer_basic_changed(GwyPixmapLayer *pixmap_layer)
{
    pixmap_layer->wants_repaint = TRUE;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(pixmap_layer));
}

/************************** Documentation ****************************/

/**
 * SECTION:gwylayer-basic
 * @title: GwyLayerBasic
 * @short_description: Display data colored with a gradient
 * @see_also: #GwyDataView -- data display widget,
 *            #GwyLayerMask -- display data as color mask,
 *            #GwyGradient -- color gradient representation
 *
 * #GwyLayerBasic is a data view layer that can paint data with a false color
 * gradient.  It is not normally usable outside a #GwyDataView.
 *
 * The layer takes all infromation from its parent #GwyDataView, therefore you
 * do not set its properties directly, but you change them in the corresponding
 * #GwyContainer and/or use methods like gwy_layer_basic_set_gradient_key() to
 * make the layer read a different value from the container.
 *
 * Several types of mapping of data to colors are available, see
 * #GwyLayerBasicRangeType for their description.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
