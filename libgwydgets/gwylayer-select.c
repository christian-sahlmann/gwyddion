/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include "gwylayer-select.h"
#include <libdraw/gwypixfield.h>

#define _(x) x

#define GWY_LAYER_SELECT_TYPE_NAME "GwyLayerSelect"

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_select_class_init        (GwyLayerSelectClass *klass);
static void       gwy_layer_select_init              (GwyLayerSelect *layer);
static void       gwy_layer_select_finalize          (GObject *object);
static void       gwy_layer_select_draw              (GwyDataViewLayer *layer,
                                                      GdkDrawable *drawable);
static gboolean   motion_notify                      (GwyDataViewLayer *layer,
                                                      GdkEventMotion *event);
static gboolean   button_pressed                     (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);
static gboolean   button_released                    (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

GType
gwy_layer_select_get_type(void)
{
    static GType gwy_layer_select_type = 0;

    if (!gwy_layer_select_type) {
        static const GTypeInfo gwy_layer_select_info = {
            sizeof(GwyLayerSelectClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_select_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerSelect),
            0,
            (GInstanceInitFunc)gwy_layer_select_init,
            NULL,
        };
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_layer_select_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_SELECT_TYPE_NAME,
                                     &gwy_layer_select_info,
                                     0);
    }

    return gwy_layer_select_type;
}

static void
gwy_layer_select_class_init(GwyLayerSelectClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_select_finalize;

    layer_class->draw = gwy_layer_select_draw;
    layer_class->motion_notify = motion_notify;
    layer_class->button_press = button_pressed;
    layer_class->button_release = button_released;
}

static void
gwy_layer_select_init(GwyLayerSelect *layer)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
}

static void
gwy_layer_select_finalize(GObject *object)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_LAYER_SELECT(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

GtkObject*
gwy_layer_select_new(GwyContainer *data)
{
    GtkObject *object;
    GwyDataViewLayer *layer;
    GwyLayerSelect *select_layer;
    GwyDataField *data_field;
    gint width, height;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    object = g_object_new(GWY_TYPE_LAYER_SELECT, NULL);
    layer = (GwyDataViewLayer*)object;
    select_layer = (GwyLayerSelect*)layer;

    g_object_ref(data);
    layer->data = data;

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/data"));
    g_return_val_if_fail(data_field, NULL);

    width = gwy_data_field_get_xres(data_field);
    height = gwy_data_field_get_yres(data_field);

    select_layer->selected = FALSE;

    return object;
}

static void
gwy_layer_select_draw(GwyDataViewLayer *layer, GdkDrawable *drawable)
{
    GwyDataField *data_field;
    GwyLayerSelect *select_layer;
    GdkGC *gc;
    gint xmin, ymin, xmax, ymax;

    g_return_if_fail(layer);
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    select_layer = (GwyLayerSelect*)layer;

    #if DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "selected == %d", select_layer->selected);
    #endif
    if (!select_layer->selected)
        return;

    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(layer->data,
                                                      "/0/data"));

    gc = layer->parent->style->fg_gc[GTK_STATE_NORMAL];
    #if DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "drawing from %d,%d size %d,%d",
          select_layer->x, select_layer->y,
          select_layer->width, select_layer->height);
    #endif
    xmin = MIN(select_layer->x, select_layer->x + select_layer->width);
    xmax = MAX(select_layer->x, select_layer->x + select_layer->width);
    ymin = MIN(select_layer->y, select_layer->y + select_layer->height);
    ymax = MAX(select_layer->y, select_layer->y + select_layer->height);
    gdk_draw_rectangle(drawable, gc, FALSE,
                       xmin, ymin, xmax - xmin, ymax - ymin);

}

static gboolean
motion_notify(GwyDataViewLayer *layer, GdkEventMotion *event)
{
    GwyLayerSelect *select_layer;

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->button)
        return FALSE;

    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "motion: %g %g", event->x, event->y);
    select_layer->width = event->x - select_layer->x;
    select_layer->height = event->y - select_layer->y;

    gdk_window_invalidate_rect(layer->parent->window, NULL, TRUE);

    return FALSE;
}

static gboolean
button_pressed(GwyDataViewLayer *layer, GdkEventButton *event)
{
    GwyLayerSelect *select_layer;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    select_layer = (GwyLayerSelect*)layer;
    if (select_layer->button)
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
              "unexpected mouse button press when already pressed");
    select_layer->button = event->button;
    select_layer->x = event->x;
    select_layer->y = event->y;
    select_layer->width = 0;
    select_layer->height = 0;
    select_layer->selected = TRUE;
    #if DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "selected == %d", select_layer->selected);
    #endif

    return FALSE;
}

static gboolean
button_released(GwyDataViewLayer *layer, GdkEventButton *event)
{
    GwyLayerSelect *select_layer;

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->button)
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
              "unexpected mouse button release when not pressed");
    select_layer->button = 0;
    select_layer->width = event->x - select_layer->x;
    select_layer->height = event->y - select_layer->y;
    select_layer->selected = select_layer->width || select_layer->height;

    gdk_window_invalidate_rect(layer->parent->window, NULL, TRUE);

    return FALSE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
