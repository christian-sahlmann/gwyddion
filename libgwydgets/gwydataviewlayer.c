#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include "gwydataview.h"
#include "gwydataviewlayer.h"

#define _(x) x

#define GWY_DATA_VIEW_LAYER_TYPE_NAME "GwyDataViewLayer"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void     gwy_data_view_layer_class_init           (GwyDataViewLayerClass *klass);
static void     gwy_data_view_layer_init                 (GwyDataViewLayer *layer);
static void     gwy_data_view_layer_finalize             (GObject *object);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

GType
gwy_data_view_layer_get_type(void)
{
    static GType gwy_data_view_layer_type = 0;

    if (!gwy_data_view_layer_type) {
        static const GTypeInfo gwy_data_view_layer_info = {
            sizeof(GwyDataViewLayerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_data_view_layer_class_init,
            NULL,
            NULL,
            sizeof(GwyDataViewLayer),
            0,
            (GInstanceInitFunc)gwy_data_view_layer_init,
            NULL,
        };
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_data_view_layer_type
            = g_type_register_static(GTK_TYPE_OBJECT,
                                     GWY_DATA_VIEW_LAYER_TYPE_NAME,
                                     &gwy_data_view_layer_info,
                                     0);
    }

    return gwy_data_view_layer_type;
}

static void
gwy_data_view_layer_class_init(GwyDataViewLayerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_data_view_layer_finalize;

    klass->paint = NULL;
    klass->draw = NULL;

    klass->button_press = NULL;
    klass->button_release = NULL;
    klass->motion_notify = NULL;
    klass->key_press = NULL;
    klass->key_release = NULL;
}

static void
gwy_data_view_layer_init(GwyDataViewLayer *layer)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    layer->parent = NULL;
    layer->data = NULL;
    layer->gc = NULL;
    layer->layout = NULL;
    layer->palette = NULL;
}

static void
gwy_data_view_layer_finalize(GObject *object)
{
    GwyDataViewLayer *layer;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(object));

    layer = GWY_DATA_VIEW_LAYER(object);

    if (layer->gc)
        g_object_unref(layer->gc);
    if (layer->layout)
        g_object_unref(layer->layout);
    if (layer->palette)
        g_object_unref(layer->palette);
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "    child pixbuf ref count %d", G_OBJECT(layer->pixbuf)->ref_count);
    if (layer->pixbuf)
        g_object_unref(layer->pixbuf);
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "    child data ref count %d", G_OBJECT(layer->data)->ref_count);
    if (layer->data)
        g_object_unref(layer->data);
    layer->gc = NULL;
    layer->layout = NULL;
    layer->palette = NULL;
    layer->pixbuf = NULL;
    layer->data = NULL;

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

gboolean
gwy_data_view_layer_is_vector(GwyDataViewLayer *layer)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    g_assert(layer_class->paint || layer_class->draw);
    return !layer_class->paint;
}

void
gwy_data_view_layer_draw(GwyDataViewLayer *layer,
                         GdkPixbuf *pixbuf)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    g_return_if_fail(layer_class->draw);
    layer_class->draw(layer, pixbuf);
}

GdkPixbuf*
gwy_data_view_layer_paint(GwyDataViewLayer *layer)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    g_return_val_if_fail(layer_class->paint, NULL);
    return layer_class->paint(layer);
}

gboolean
gwy_data_view_layer_button_press(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->button_press)
        return layer_class->button_press(layer, event);
    return FALSE;
}

gboolean
gwy_data_view_layer_button_release(GwyDataViewLayer *layer,
                                   GdkEventButton *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->button_release)
        return layer_class->button_release(layer, event);
    return FALSE;
}

gboolean
gwy_data_view_layer_motion_notify(GwyDataViewLayer *layer,
                                  GdkEventMotion *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->motion_notify)
        return layer_class->motion_notify(layer, event);
    return FALSE;
}

gboolean
gwy_data_view_layer_key_press(GwyDataViewLayer *layer,
                              GdkEventKey *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->key_press)
        return layer_class->key_press(layer, event);
    return FALSE;
}

gboolean
gwy_data_view_layer_key_release(GwyDataViewLayer *layer,
                                GdkEventKey *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->key_release)
        return layer_class->key_release(layer, event);
    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
