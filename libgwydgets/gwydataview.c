/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libprocess/datafield.h>
#include "gwydataview.h"

#define _(x) x
#define gwy_object_unref(x) if (x) g_object_unref(x); (x) = NULL

#define GWY_DATA_VIEW_TYPE_NAME "GwyDataView"

#define BITS_PER_SAMPLE 8

enum {
    PROP_0,
    PROP_LAST
};

/* Forward declarations */

static void     gwy_data_view_class_init           (GwyDataViewClass *klass);
static void     gwy_data_view_init                 (GwyDataView *data_view);
GtkWidget*      gwy_data_view_new                  (GwyContainer *data);
static void     gwy_data_view_destroy              (GtkObject *object);
static void     gwy_data_view_finalize             (GObject *object);
static void     gwy_data_view_set_property         (GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_data_view_get_property         (GObject*object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_data_view_realize              (GtkWidget *widget);
static void     gwy_data_view_unrealize            (GtkWidget *widget);
static void     gwy_data_view_size_request         (GtkWidget *widget,
                                                    GtkRequisition *requisition);
static void     gwy_data_view_size_allocate        (GtkWidget *widget,
                                                    GtkAllocation *allocation);
static void     simple_gdk_pixbuf_composite        (GdkPixbuf *source,
                                                    GdkPixbuf *dest);
static void     simple_gdk_pixbuf_scale_or_copy    (GdkPixbuf *source,
                                                    GdkPixbuf *dest);
static void     gwy_data_view_make_pixmap          (GwyDataView *data_view);
static void     gwy_data_view_paint                (GwyDataView *data_view);
static gboolean gwy_data_view_expose               (GtkWidget *widget,
                                                    GdkEventExpose *event);
static gboolean gwy_data_view_button_press         (GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean gwy_data_view_button_release       (GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean gwy_data_view_motion_notify        (GtkWidget *widget,
                                                    GdkEventMotion *event);
static gboolean gwy_data_view_key_press            (GtkWidget *widget,
                                                    GdkEventKey *event);
static gboolean gwy_data_view_key_release          (GtkWidget *widget,
                                                    GdkEventKey *event);
static void     gwy_data_view_update               (GwyDataView *data_view);
static void     gwy_data_view_set_layer            (GwyDataView *data_view,
                                                    GwyDataViewLayer **which,
                                                    GwyDataViewLayer *layer);


/* Local data */

static GtkWidgetClass *parent_class = NULL;

GType
gwy_data_view_get_type(void)
{
    static GType gwy_data_view_type = 0;

    if (!gwy_data_view_type) {
        static const GTypeInfo gwy_data_view_info = {
            sizeof(GwyDataViewClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_data_view_class_init,
            NULL,
            NULL,
            sizeof(GwyDataView),
            0,
            (GInstanceInitFunc)gwy_data_view_init,
            NULL,
        };
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
        gwy_data_view_type = g_type_register_static(GTK_TYPE_WIDGET,
                                                    GWY_DATA_VIEW_TYPE_NAME,
                                                    &gwy_data_view_info,
                                                    0);
    }

    return gwy_data_view_type;
}

static void
gwy_data_view_class_init(GwyDataViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    object_class = (GtkObjectClass*)klass;
    widget_class = (GtkWidgetClass*)klass;

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_data_view_finalize;
    gobject_class->set_property = gwy_data_view_set_property;
    gobject_class->get_property = gwy_data_view_get_property;

    object_class->destroy = gwy_data_view_destroy;

    widget_class->realize = gwy_data_view_realize;
    widget_class->expose_event = gwy_data_view_expose;
    widget_class->size_request = gwy_data_view_size_request;
    widget_class->unrealize = gwy_data_view_unrealize;
    widget_class->size_allocate = gwy_data_view_size_allocate;
    /* user-interaction events */
    widget_class->button_press_event = gwy_data_view_button_press;
    widget_class->button_release_event = gwy_data_view_button_release;
    widget_class->motion_notify_event = gwy_data_view_motion_notify;
    widget_class->key_press_event = gwy_data_view_key_press;
    widget_class->key_release_event = gwy_data_view_key_release;
}

static void
gwy_data_view_init(GwyDataView *data_view)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    data_view->data = NULL;
    data_view->top_layer = NULL;
    data_view->alpha_layer = NULL;
    data_view->base_layer = NULL;
    data_view->pixbuf = NULL;
    data_view->base_pixbuf = NULL;
    data_view->zoom = 1.0;
    data_view->newzoom = 1.0;
    data_view->xmeasure = -1.0;
    data_view->ymeasure = -1.0;
    data_view->xoff = 0;
    data_view->yoff = 0;
}

/**
 * gwy_data_view_new:
 * @data: A #GwyContainer containing the data to display.
 *
 * Creates a new data-displaying widget for @data.
 *
 * A newly created #GwyDataView doesn't display anything.  You have to add
 * some layers to it, at least a base layer with
 * gwy_data_view_set_base_layer(), and possibly others with
 * gwy_data_view_set_alpha_layer() and gwy_data_view_set_top_layer().
 *
 * The top layer is special. It must be a vector layer and can receive
 * mouse and keyboard events.
 *
 * The base layer it also special. It must be always present, and must not be
 * transparent or vector.
 *
 * Returns: A newly created data view as a #GtkWidget.
 **/
GtkWidget*
gwy_data_view_new(GwyContainer *data)
{
    GtkWidget *data_view;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(GWY_IS_CONTAINER(data), NULL);

    data_view = gtk_widget_new(GWY_TYPE_DATA_VIEW, NULL);

    g_object_ref(data);
    GWY_DATA_VIEW(data_view)->data = data;

    return data_view;
}

static void
gwy_data_view_destroy(GtkObject *object)
{
    GwyDataView *data_view;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "destroying a GwyDataView (refcount = %u)",
          G_OBJECT(object)->ref_count);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW(object));

    data_view = GWY_DATA_VIEW(object);
    gwy_data_view_set_layer(data_view, &data_view->top_layer, NULL);
    gwy_data_view_set_layer(data_view, &data_view->alpha_layer, NULL);
    gwy_data_view_set_layer(data_view, &data_view->base_layer, NULL);

    if (GTK_OBJECT_CLASS(parent_class)->destroy)
        (*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}

static void
gwy_data_view_finalize(GObject *object)
{
    GwyDataView *data_view;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "finalizing a GwyDataView (refcount = %u)",
          object->ref_count);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW(object));

    data_view = GWY_DATA_VIEW(object);

    gwy_object_unref(data_view->base_layer);
    gwy_object_unref(data_view->alpha_layer);
    gwy_object_unref(data_view->top_layer);
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "    child data ref count %d", G_OBJECT(data_view->data)->ref_count);
    gwy_object_unref(data_view->data);
}

static void
gwy_data_view_unrealize(GtkWidget *widget)
{
    GwyDataView *data_view = GWY_DATA_VIEW(widget);

    gwy_object_unref(data_view->pixbuf);
    gwy_object_unref(data_view->base_pixbuf);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}


static void
gwy_data_view_set_property(GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
    g_assert(G_IS_VALUE(value));
}

static void
gwy_data_view_get_property(GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
    switch (prop_id) {
        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
    g_assert(G_IS_VALUE(value));
}

static void
gwy_data_view_realize(GtkWidget *widget)
{
    GwyDataView *data_view;
    GdkWindowAttr attributes;
    gint attributes_mask;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "realizing a GwyDataView (%ux%u)",
          widget->allocation.width, widget->allocation.height);
    #endif

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW(widget));

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    data_view = GWY_DATA_VIEW(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget)
                            | GDK_EXPOSURE_MASK
                            | GDK_BUTTON_PRESS_MASK
                            | GDK_BUTTON_RELEASE_MASK
                            | GDK_KEY_PRESS_MASK
                            | GDK_KEY_RELEASE_MASK
                            | GDK_POINTER_MOTION_MASK
                            | GDK_POINTER_MOTION_HINT_MASK;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(widget->parent->window,
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    gwy_data_view_make_pixmap(data_view);
}

static void
gwy_data_view_size_request(GtkWidget *widget,
                           GtkRequisition *requisition)
{
    GwyDataView *data_view;
    GwyContainer *data;
    GwyDataField *data_field;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    data_view = GWY_DATA_VIEW(widget);
    data = data_view->data;
    /* TODO Container */
    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(data,
                                                      "/0/data"));
    requisition->width = data_view->newzoom
                         * gwy_data_field_get_xres(data_field);
    requisition->height = data_view->newzoom
                          * gwy_data_field_get_yres(data_field);

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "%s requesting %d x %d",
          __FUNCTION__, requisition->width, requisition->height);
    #endif
}

static void
gwy_data_view_size_allocate(GtkWidget *widget,
                            GtkAllocation *allocation)
{
    GwyDataView *data_view;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "%s allocating %d x %d",
          __FUNCTION__, allocation->width, allocation->height);
    #endif

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        data_view = GWY_DATA_VIEW(widget);

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
        gwy_data_view_make_pixmap(data_view);
    }
}

static void
gwy_data_view_make_pixmap(GwyDataView *data_view)
{
    GtkWidget *widget;
    GwyDataField *data_field;
    gint width, height, scwidth, scheight, src_width, src_height;

    data_field = GWY_DATA_FIELD(
                     gwy_container_get_object_by_name(data_view->data,
                                                      "/0/data"));
    src_width = gwy_data_field_get_xres(data_field);
    src_height = gwy_data_field_get_yres(data_field);

    if (!data_view->base_pixbuf)
        data_view->base_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                                FALSE,
                                                BITS_PER_SAMPLE,
                                                src_width, src_height);

    if (data_view->pixbuf) {
        width = gdk_pixbuf_get_width(data_view->pixbuf);
        height = gdk_pixbuf_get_height(data_view->pixbuf);
    }
    else
        width = height = -1;

    widget = GTK_WIDGET(data_view);
    data_view->zoom = MIN((gdouble)widget->allocation.width/src_width,
                          (gdouble)widget->allocation.height/src_height);
    data_view->newzoom = data_view->zoom;
    scwidth = floor(src_width * data_view->zoom + 0.000001);
    scheight = floor(src_height * data_view->zoom + 0.000001);
    data_view->xmeasure = gwy_data_field_get_xreal(data_field)/scwidth;
    data_view->ymeasure = gwy_data_field_get_yreal(data_field)/scheight;
    data_view->xoff = (widget->allocation.width - scwidth)/2;
    data_view->yoff = (widget->allocation.height - scheight)/2;
    if (scwidth != width || scheight != height) {
        gwy_object_unref(data_view->pixbuf);
        data_view->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                           TRUE,
                                           BITS_PER_SAMPLE,
                                           scwidth, scheight);
        gdk_pixbuf_fill(data_view->pixbuf, 0x00000000);
        gwy_data_view_paint(data_view);
    }
}

static void
simple_gdk_pixbuf_scale_or_copy(GdkPixbuf *source, GdkPixbuf *dest)
{
    gint height, width, src_height, src_width;

    src_height = gdk_pixbuf_get_height(source);
    src_width = gdk_pixbuf_get_height(source);
    height = gdk_pixbuf_get_height(dest);
    width = gdk_pixbuf_get_height(dest);

    if (src_width == width && src_height == height)
        gdk_pixbuf_copy_area(source, 0, 0, src_width, src_height,
                             dest, 0, 0);
    else
        gdk_pixbuf_scale(source, dest, 0, 0, width, height, 0.0, 0.0,
                         (gdouble)width/src_width, (gdouble)height/src_height,
                         GDK_INTERP_TILES);
}

static void
simple_gdk_pixbuf_composite(GdkPixbuf *source, GdkPixbuf *dest)
{
    gint height, width, src_height, src_width;

    src_height = gdk_pixbuf_get_height(source);
    src_width = gdk_pixbuf_get_height(source);
    height = gdk_pixbuf_get_height(dest);
    width = gdk_pixbuf_get_height(dest);

    gdk_pixbuf_composite(source, dest, 0, 0, width, height, 0.0, 0.0,
                         (gdouble)width/src_width, (gdouble)height/src_height,
                         GDK_INTERP_TILES, 0x255);
}

static void
gwy_data_view_paint(GwyDataView *data_view)
{
    GdkPixbuf *src_pixbuf;
    GTimer *timer = NULL;

    g_return_if_fail(data_view->base_layer);
    timer = g_timer_new();

    /* base layer is always present
     * top layer is always vector, if any
     */
    if (!data_view->alpha_layer) {
        /* scale base directly to final pixbuf */
        src_pixbuf = gwy_data_view_layer_paint(data_view->base_layer);
        simple_gdk_pixbuf_scale_or_copy(src_pixbuf, data_view->pixbuf);
        g_message("%s: BASE %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    }
    else {
        /* base */
        src_pixbuf = gwy_data_view_layer_paint(data_view->base_layer);
        simple_gdk_pixbuf_scale_or_copy(src_pixbuf, data_view->base_pixbuf);
        g_message("%s: base %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
        g_timer_reset(timer);

        /* composite with alpha */
        src_pixbuf = gwy_data_view_layer_paint(data_view->alpha_layer);
        simple_gdk_pixbuf_composite(src_pixbuf, data_view->base_pixbuf);
        g_message("%s: alpha %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
        g_timer_reset(timer);

        /* scale both */
        simple_gdk_pixbuf_scale_or_copy(data_view->pixbuf,
                                        data_view->base_pixbuf);
        g_message("%s: BOTH %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    }
    g_timer_destroy(timer);
}

static gboolean
gwy_data_view_expose(GtkWidget *widget,
                     GdkEventExpose *event)
{
    GwyDataView *data_view;
    GTimer *timer = NULL;

    g_return_val_if_fail(widget, FALSE);
    g_return_val_if_fail(GWY_IS_DATA_VIEW(widget), FALSE);

    if (event->count > 0)
        return FALSE;

    gdk_window_set_back_pixmap(widget->window, NULL, FALSE);

    timer = g_timer_new();
    data_view = GWY_DATA_VIEW(widget);
    /* FIXME: ask the layers, if they want to repaint themselves */
    if (gwy_data_view_layer_wants_repaint(data_view->base_layer)
        || gwy_data_view_layer_wants_repaint(data_view->alpha_layer))
        gwy_data_view_paint(data_view);

    gdk_draw_pixbuf(widget->window,
                    NULL,
                    data_view->pixbuf,
                    0, 0,
                    data_view->xoff, data_view->yoff,
                    -1, -1,
                    GDK_RGB_DITHER_NORMAL,
                    0, 0);
    g_message("%s: buf->map %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));

    if (data_view->top_layer) {
        g_timer_reset(timer);
        gwy_data_view_layer_draw(data_view->top_layer, widget->window);
        g_message("%s: DRAW %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    }
    g_timer_destroy(timer);

    return FALSE;
}

static gboolean
gwy_data_view_button_press(GtkWidget *widget,
                           GdkEventButton *event)
{
    GwyDataView *data_view;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(GWY_IS_DATA_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_data_view_layer_button_press(data_view->top_layer, event);
}

static gboolean
gwy_data_view_button_release(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyDataView *data_view;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_val_if_fail(GWY_IS_DATA_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_data_view_layer_button_release(data_view->top_layer, event);
}

static gboolean
gwy_data_view_motion_notify(GtkWidget *widget,
                            GdkEventMotion *event)
{
    GwyDataView *data_view;

    g_return_val_if_fail(GWY_IS_DATA_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_data_view_layer_motion_notify(data_view->top_layer, event);
}

static gboolean
gwy_data_view_key_press(GtkWidget *widget,
                        GdkEventKey *event)
{
    GwyDataView *data_view;

    g_return_val_if_fail(GWY_IS_DATA_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_data_view_layer_key_press(data_view->top_layer, event);
}

static gboolean
gwy_data_view_key_release(GtkWidget *widget,
                          GdkEventKey *event)
{
    GwyDataView *data_view;

    g_return_val_if_fail(GWY_IS_DATA_VIEW(widget), FALSE);
    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_data_view_layer_key_release(data_view->top_layer, event);
}

void
gwy_data_view_update(GwyDataView *data_view)
{
    GtkWidget *widget;

    g_return_if_fail(data_view != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    widget = GTK_WIDGET(data_view);
    if (widget->window)
        gdk_window_invalidate_rect(widget->window, NULL, TRUE);
}

GwyDataViewLayer*
gwy_data_view_get_base_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);

    return data_view->base_layer;
}

GwyDataViewLayer*
gwy_data_view_get_alpha_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);

    return data_view->alpha_layer;
}

GwyDataViewLayer*
gwy_data_view_get_top_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);

    return data_view->top_layer;
}

static void
gwy_data_view_set_layer(GwyDataView *data_view,
                        GwyDataViewLayer **which,
                        GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (layer == *which)
        return;
    if (*which) {
        (*which)->parent = NULL;
        gwy_data_view_layer_unplugged(*which);
        g_object_unref(*which);
    }
    if (layer) {
        g_assert(layer->parent == NULL);
        g_object_ref(layer);
        gtk_object_sink(GTK_OBJECT(layer));
        layer->parent = (GtkWidget*)data_view;
        gwy_data_view_layer_plugged(layer);
    }
    *which = layer;
    gwy_data_view_update(data_view);
}

/**
 * gwy_data_view_set_base_layer:
 * @data_view: A #GwyDataView.
 * @layer: A layer to be used as the base layer for @data_view.
 *
 * Plugs @layer to @data_view as the base layer.
 *
 * If another base layer is present, it's unplugged.
 *
 * The layer must not be a vector layer.  Theoretically, it can be %NULL to
 * use no base layer, but then @data_view will probably display garbage.
 **/
void
gwy_data_view_set_base_layer(GwyDataView *data_view,
                             GwyDataViewLayer *layer)
{
    g_return_if_fail(!layer || GWY_IS_DATA_VIEW_LAYER(layer));
    g_return_if_fail(!gwy_data_view_layer_is_vector(layer));
    gwy_data_view_set_layer(data_view, &data_view->base_layer, layer);
}

/**
 * gwy_data_view_set_alpha_layer:
 * @data_view: A #GwyDataView.
 * @layer: A layer to be used as the alpha layer for @data_view.
 *
 * Plugs @layer to @data_view as the alpha layer.
 *
 * If another alpha layer is present, it's unplugged.
 *
 * The layer must not be a vector layer.  It can be %NULL, meaning no alpha
 * layer is to be used.
 **/
void
gwy_data_view_set_alpha_layer(GwyDataView *data_view,
                              GwyDataViewLayer *layer)
{
    g_return_if_fail(!layer || GWY_IS_DATA_VIEW_LAYER(layer));
    g_return_if_fail(!gwy_data_view_layer_is_vector(layer));
    gwy_data_view_set_layer(data_view, &data_view->alpha_layer, layer);
}

/**
 * gwy_data_view_set_top_layer:
 * @data_view: A #GwyDataView.
 * @layer: A layer to be used as the top layer for @data_view.
 *
 * Plugs @layer to @data_view as the top layer.
 *
 * If another top layer is present, it's unplugged.
 *
 * The layer must be a vector layer.  It can be %NULL, meaning no top
 * layer is to be used.
 **/
void
gwy_data_view_set_top_layer(GwyDataView *data_view,
                            GwyDataViewLayer *layer)
{
    g_return_if_fail(!layer || GWY_IS_DATA_VIEW_LAYER(layer));
    g_return_if_fail(gwy_data_view_layer_is_vector(layer));
    gwy_data_view_set_layer(data_view, &data_view->top_layer, layer);
}

/**
 * gwy_data_view_get_hexcess:
 * @data_view: A #GwyDataView.
 *
 * Return the horizontal excess of widget size to data size.
 *
 * Do not use.  Only useful for #GwyDataWindow implementation.
 *
 * Returns: The execess.
 **/
gdouble
gwy_data_view_get_hexcess(GwyDataView* data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 0);
    if (!data_view->pixbuf)
        return 0;

    return (gdouble)GTK_WIDGET(data_view)->allocation.width
                    / gdk_pixbuf_get_width(data_view->pixbuf) - 1.0;
}

/**
 * gwy_data_view_get_vexcess:
 * @data_view: A #GwyDataView.
 *
 * Return the vertical excess of widget size to data size.
 *
 * Do not use.  Only useful for #GwyDataWindow implementation.
 *
 * Returns: The execess.
 **/
gdouble
gwy_data_view_get_vexcess(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 0);
    if (!data_view->pixbuf)
        return 0;

    return (gdouble)GTK_WIDGET(data_view)->allocation.height
                    / gdk_pixbuf_get_height(data_view->pixbuf) - 1.0;
}

/**
 * gwy_data_view_set_zoom:
 * @data_view: A #GwyDataView.
 * @zoom: A new zoom value.
 *
 * Sets zoom of @data_view to @zoom.
 *
 * Zoom greater than 1 means larger image on screen and vice versa.
 *
 * Note window manager can prevent the window from resize and thus the zoom
 * from change.
 **/
void
gwy_data_view_set_zoom(GwyDataView *data_view,
                       gdouble zoom)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    if (!data_view->pixbuf || !data_view->base_pixbuf)
        return;

    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "%s: zoom = %g, new = %g", __FUNCTION__, data_view->newzoom, zoom);
    if (fabs(log(data_view->newzoom/zoom)) < 0.001)
        return;

    data_view->newzoom = zoom;
    gtk_widget_queue_resize(GTK_WIDGET(data_view));
}

/**
 * gwy_data_view_get_zoom:
 * @data_view: A #GwyDataView.
 *
 * Returns current zoom of @data_view.
 *
 * Returns: The zoom.
 **/
gdouble
gwy_data_view_get_zoom(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->zoom;
}

/**
 * gwy_data_view_get_xmeasure:
 * @data_view: A #GwyDataView.
 *
 * Returns the ratio between horizontal physical lengths and horizontal
 * screen lengths in pixels.
 *
 * Returns: The horizontal measure.
 **/
gdouble
gwy_data_view_get_xmeasure(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->xmeasure;
}

/**
 * gwy_data_view_get_ymeasure:
 * @data_view: A #GwyDataView.
 *
 * Returns the ratio between vertical physical lengths and horizontal
 * screen lengths in pixels.
 *
 * Returns: The vertical measure.
 **/
gdouble
gwy_data_view_get_ymeasure(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->ymeasure;
}

/**
 * gwy_data_view_get_data:
 * @data_view: A #GwyDataView.
 *
 * Returns the data container used by @data_view.
 *
 * Returns: The data as a #GwyContainer.
 **/
GwyContainer*
gwy_data_view_get_data(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    return data_view->data;
}

/**
 * gwy_data_view_coords_xy_clamp:
 * @data_view: A #GwyDataView.
 * @xscr: A screen x-coordinate relative to widget origin.
 * @yscr: A screen y-coordinate relative to widget origin.
 *
 * Fixes screen coordinates @xscr and @yscr to be inside the data-displaying
 * area (which can be smaller than widget size).
 **/
void
gwy_data_view_coords_xy_clamp(GwyDataView *data_view,
                              gint *xscr, gint *yscr)
{
    GtkWidget *widget;
    gint size;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    widget = GTK_WIDGET(data_view);

    if (xscr) {
        size = gdk_pixbuf_get_width(data_view->pixbuf);
        *xscr = CLAMP(*xscr, data_view->xoff, data_view->xoff + size-1);
    }
    if (yscr) {
        size = gdk_pixbuf_get_height(data_view->pixbuf);
        *yscr = CLAMP(*yscr, data_view->yoff, data_view->yoff + size-1);
    }
}

/**
 * gwy_data_view_coords_xy_to_real:
 * @data_view: A #GwyDataView.
 * @xscr: A screen x-coordinate relative to widget origin.
 * @yscr: A screen y-coordinate relative to widget origin.
 * @xreal: Where the physical x-coordinate in the data sample should be stored.
 * @yreal: Where the physical y-coordinate in the data sample should be stored.
 *
 * Recomputes screen coordinates relative to widget origin to physical
 * coordinates in the sample.
 **/
void
gwy_data_view_coords_xy_to_real(GwyDataView *data_view,
                                gint xscr, gint yscr,
                                gdouble *xreal, gdouble *yreal)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (xreal)
        *xreal = (xscr - data_view->xoff) * data_view->xmeasure;
    if (yreal)
        *yreal = (yscr - data_view->yoff) * data_view->ymeasure;
}

/**
 * gwy_data_view_coords_real_to_xy:
 * @data_view: A #GwyDataView.
 * @xreal: A physical x-coordinate in the data sample..
 * @yreal: A physical y-coordinate in the data sample.
 * @xscr: Where the screen x-coordinate relative to widget origin should be
 *        stored.
 * @yscr: Where the screen y-coordinate relative to widget origin should be
 *        stored.
 *
 * Recomputes physical coordinate in the sample to screen coordinate relative
 * to widget origin.
 **/
void
gwy_data_view_coords_real_to_xy(GwyDataView *data_view,
                                gdouble xreal, gdouble yreal,
                                gint *xscr, gint *yscr)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    if (xscr)
        *xscr = floor(xreal/data_view->xmeasure + 0.5) + data_view->xoff;
    if (yscr)
        *yscr = floor(yreal/data_view->ymeasure + 0.5) + data_view->yoff;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
