#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include "gwylayer-basic.h"
#include <libdraw/gwypixfield.h>

#define _(x) x

#define GWY_LAYER_BASIC_TYPE_NAME "GwyLayerBasic"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_basic_class_init        (GwyLayerBasicClass *klass);
static void       gwy_layer_basic_init              (GwyLayerBasic *layer);
static void       gwy_layer_basic_finalize          (GObject *object);
static GdkPixbuf* gwy_layer_basic_paint             (GwyDataViewLayer *layer);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

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
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_basic_finalize;

    layer_class->paint = gwy_layer_basic_paint;
}

static void
gwy_layer_basic_init(GwyLayerBasic *layer)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
}

static void
gwy_layer_basic_finalize(GObject *object)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_LAYER_BASIC(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

GtkObject*
gwy_layer_basic_new(GwyContainer *data)
{
    GtkObject *object;
    GwyDataViewLayer *layer;
    GwyDataField *data_field;
    gint width, height;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    object = g_object_new(GWY_TYPE_LAYER_BASIC, NULL);
    layer = (GwyDataViewLayer*)object;

    g_object_ref(data);
    layer->data = data;

    layer->palette = (GwyPalette*)gwy_palette_new(512);
    gwy_palette_setup_predef(layer->palette, GWY_PALETTE_OLIVE); /* XXX: fun */

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/data"));
    g_return_val_if_fail(data_field, NULL);

    width = gwy_data_field_get_xres(data_field);
    height = gwy_data_field_get_yres(data_field);
    layer->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                                   BITS_PER_SAMPLE, width, height);

    return object;
}

static GdkPixbuf*
gwy_layer_basic_paint(GwyDataViewLayer *layer)
{
    GwyDataField *data_field;

    g_return_val_if_fail(layer, NULL);
    g_return_val_if_fail(GWY_IS_LAYER_BASIC(layer), NULL);

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/data"));
    g_return_val_if_fail(data_field, layer->pixbuf);
    gwy_pixfield_do(layer->pixbuf, data_field, layer->palette);

    return layer->pixbuf;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
