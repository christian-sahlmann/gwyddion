/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
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
    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(GWY_IS_LAYER_BASIC(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_basic_new:
 *
 * Creates a new basic data displaying layer.
 *
 * By default, is uses a gray palette.
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
    gwy_palette_set_by_name(layer->palette, GWY_PALETTE_GRAY);

    return object;
}

static GdkPixbuf*
gwy_layer_basic_paint(GwyDataViewLayer *layer)
{
    GwyDataField *data_field;

    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/data"));
    g_return_val_if_fail(data_field, layer->pixbuf);
    if (GWY_LAYER_BASIC(layer)->changed) {
        GTimer *timer;

        timer = g_timer_new();
        gwy_pixfield_do(layer->pixbuf, data_field, layer->palette);
        gwy_debug("%s: %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
        g_timer_destroy(timer);
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

    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));
    g_return_if_fail(GWY_IS_PALETTE(palette));

    oldpalette = layer->palette;
    g_object_ref(palette);
    layer->palette = palette;
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

    layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                   BITS_PER_SAMPLE, width, height);
}

static void
gwy_layer_basic_unplugged(GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_LAYER_BASIC(layer));

    gwy_object_unref(layer->pixbuf);
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
