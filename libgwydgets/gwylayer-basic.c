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
#include <libgwyddion/gwycontainer.h>
#include <libprocess/stats.h>
#include <libdraw/gwypixfield.h>
#include "gwylayer-basic.h"

#define GWY_LAYER_BASIC_TYPE_NAME "GwyLayerBasic"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_basic_class_init        (GwyLayerBasicClass *klass);
static void       gwy_layer_basic_init              (GwyLayerBasic *layer);
static void       gwy_layer_basic_destroy           (GtkObject *object);
static void       gwy_layer_basic_finalize          (GObject *object);
static GdkPixbuf* gwy_layer_basic_paint             (GwyPixmapLayer *layer);
static gboolean   gwy_layer_basic_wants_repaint     (GwyDataViewLayer *layer);
static void       gwy_layer_basic_plugged           (GwyDataViewLayer *layer);
static void       gwy_layer_basic_unplugged         (GwyDataViewLayer *layer);
static void       gwy_layer_basic_update            (GwyDataViewLayer *layer);

/* Local data */

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
        gwy_debug(" ");
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
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyPixmapLayerClass *pixmap_class = GWY_PIXMAP_LAYER_CLASS(klass);

    gwy_debug(" ");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_basic_finalize;

    object_class->destroy = gwy_layer_basic_destroy;

    layer_class->wants_repaint = gwy_layer_basic_wants_repaint;
    layer_class->plugged = gwy_layer_basic_plugged;
    layer_class->unplugged = gwy_layer_basic_unplugged;

    pixmap_class->paint = gwy_layer_basic_paint;
}

static void
gwy_layer_basic_init(GwyLayerBasic *layer)
{
    gwy_debug(" ");

    layer->changed = TRUE;
}

static void
gwy_layer_basic_finalize(GObject *object)
{
    gwy_debug(" ");

    gwy_object_unref(GWY_LAYER_BASIC(object)->gradient);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_layer_basic_destroy(GtkObject *object)
{
    GwyLayerBasic *layer;

    layer = (GwyLayerBasic*)object;
    if (layer->gradient_id)
        g_signal_handler_disconnect(layer->gradient, layer->gradient_id);

    GTK_OBJECT_CLASS(parent_class)->destroy(object);
}

/**
 * gwy_layer_basic_new:
 *
 * Creates a new basic data displaying layer.
 *
 * By default, is uses a gray gradient or gradient whose name is stored with
 * the data as "/0/base/palette".
 *
 * Other used container values: "/0/show" is shown instead of "/0/data" if
 * present.  If "/0/base/min" and "/0/base/max" is set, it is used as the
 * color scale range instead of fitting it to data value range.
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_basic_new(void)
{
    GtkObject *object;
    GwyLayerBasic *layer;

    gwy_debug(" ");

    object = g_object_new(GWY_TYPE_LAYER_BASIC, NULL);
    layer = (GwyLayerBasic*)object;

    layer->gradient = gwy_gradients_get_gradient(GWY_GRADIENT_DEFAULT);
    g_object_ref(layer->gradient);

    return object;
}

static GdkPixbuf*
gwy_layer_basic_paint(GwyPixmapLayer *layer)
{
    GwyDataField *data_field;
    GwyLayerBasic *basic_layer;
    GwyContainer *data;
    gdouble min = 0.0, max = 0.0;
    gboolean fixedmin, fixedmax;
    gboolean fixedrange = FALSE;

    gwy_debug(" ");
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);
    basic_layer = GWY_LAYER_BASIC(layer);
    data = GWY_DATA_VIEW_LAYER(layer)->data;

    /* TODO Container */
    if (!gwy_container_gis_object_by_name(data, "/0/show",
                                          (GObject**)&data_field)) {
        data_field
            = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
        fixedrange = TRUE;
    }
    g_return_val_if_fail(GWY_IS_DATA_FIELD(data_field), layer->pixbuf);
    if (fixedrange) {
        fixedmin = gwy_container_gis_double_by_name(data, "/0/base/min", &min);
        fixedmax = gwy_container_gis_double_by_name(data, "/0/base/max", &max);
        if (fixedmin || fixedmax) {
            if (!fixedmin)
                min = gwy_data_field_get_min(data_field);
            if (!fixedmax)
                max = gwy_data_field_get_max(data_field);
        }
        else
            fixedrange = FALSE;
    }
    /* XXX */
    /*if (GWY_LAYER_BASIC(layer)->changed)*/ {
        if (fixedrange)
            gwy_pixbuf_draw_data_field_with_range(layer->pixbuf, data_field,
                                                  basic_layer->gradient,
                                                  min, max);
        else
            gwy_pixbuf_draw_data_field(layer->pixbuf, data_field,
                                       basic_layer->gradient);
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
 * gwy_layer_basic_set_gradient:
 * @layer: A basic data view layer.
 * @gradient: Name of gradient @layer should use.  It should exist.
 *
 * Sets the color gradient a basic layer should use.
 **/
void
gwy_layer_basic_set_gradient(GwyLayerBasic *layer,
                             const gchar *gradient)
{
    GwyGradient *grad, *old;
    gchar *gradstr;

    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    gwy_debug("%s", gradient);

    grad = gwy_gradients_get_gradient(gradient);
    if (!grad || grad == layer->gradient)
        return;

    /* the string we've got as argument can be owned by somethin we are
     * going to destroy */
    gradstr = g_strdup(gradient);
    old = layer->gradient;
    if (layer->gradient_id)
        g_signal_handler_disconnect(layer->gradient, layer->gradient_id);
    g_object_ref(grad);
    layer->gradient = grad;
    layer->gradient_id
        = g_signal_connect_swapped(layer->gradient, "value_changed",
                                   G_CALLBACK(gwy_layer_basic_update), layer);
    gwy_container_set_string_by_name(GWY_DATA_VIEW_LAYER(layer)->data,
                                     "/0/base/palette", gradstr);
    g_object_unref(old);

    gwy_layer_basic_update(GWY_DATA_VIEW_LAYER(layer));
}

/**
 * gwy_layer_basic_get_gradient:
 * @layer: A basic data view layer.
 *
 * Returns the color gradient a basic layer uses.
 *
 * Returns: The gradient name.  It must not be modified or freed.  It may
 *          differ the name that was used on initialization or set with
 *          gwy_shader_set_gradient(), if the gradient didn't exist or
 *          was renamed meanwhile.
 **/
const gchar*
gwy_layer_basic_get_gradient(GwyLayerBasic *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);

    return gwy_gradient_get_name(layer->gradient);
}

static void
gwy_layer_basic_plugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerBasic *basic_layer;
    GwyDataField *data_field = NULL;
    gint width, height;
    const guchar *gradient_name;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    basic_layer = GWY_LAYER_BASIC(layer);

    basic_layer->changed = TRUE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);

    /* TODO Container */
    /* XXX */
    data_field = GWY_DATA_FIELD(gwy_container_get_object_by_name(layer->data,
                                                                 "/0/data"));
    gwy_container_gis_object_by_name(layer->data, "/0/show",
                                     (GObject**)&data_field);
    g_return_if_fail(data_field);
    width = gwy_data_field_get_xres(data_field);
    height = gwy_data_field_get_yres(data_field);

    if (gwy_container_gis_string_by_name(layer->data, "/0/base/palette",
                                         &gradient_name))
        gwy_layer_basic_set_gradient((GwyLayerBasic*)layer, gradient_name);
    else {
        /* FIXME: this is probably wrong, it should work with unset gradient
         * and use default, but things depend on the fact /0/base/palette
         * is set. */
        gwy_layer_basic_set_gradient((GwyLayerBasic*)layer,
                                     GWY_GRADIENT_DEFAULT);
        gwy_container_set_string_by_name(GWY_DATA_VIEW_LAYER(layer)->data,
                                         "/0/base/palette",
                                         g_strdup(GWY_GRADIENT_DEFAULT));
    }

    pixmap_layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                          BITS_PER_SAMPLE, width, height);
    gwy_debug_objects_creation(G_OBJECT(pixmap_layer->pixbuf));
}

static void
gwy_layer_basic_unplugged(GwyDataViewLayer *layer)
{
    GwyPixmapLayer *pixmap_layer;
    GwyLayerBasic *basic_layer;

    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    pixmap_layer = GWY_PIXMAP_LAYER(layer);
    basic_layer = GWY_LAYER_BASIC(layer);

    gwy_object_unref(pixmap_layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_basic_update(GwyDataViewLayer *layer)
{
    gwy_debug(" ");

    GWY_LAYER_BASIC(layer)->changed = TRUE;
    gwy_data_view_layer_updated(layer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
