/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libprocess/datafield.h>
#include "gwydataview.h"

#define _(x) x

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
static gboolean gwy_data_view_paint_layer          (GwyDataView *data_view,
                                                    GwyDataViewLayer *layer,
                                                    gboolean to_base,
                                                    gboolean is_base,
                                                    gboolean is_top);
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
}

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

    if (data_view->base_layer)
        g_object_unref(data_view->base_layer);
    if (data_view->alpha_layer)
        g_object_unref(data_view->alpha_layer);
    if (data_view->top_layer)
        g_object_unref(data_view->top_layer);
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "    child data ref count %d", G_OBJECT(data_view->data)->ref_count);
    if (data_view->data)
        g_object_unref(data_view->data);
    data_view->alpha_layer = NULL;
    data_view->top_layer = NULL;
    data_view->base_layer = NULL;
    data_view->data = NULL;
}

static void
gwy_data_view_unrealize(GtkWidget *widget)
{
    GwyDataView *data_view = GWY_DATA_VIEW(widget);

    if (data_view->pixbuf)
        g_object_unref(data_view->pixbuf);
    if (data_view->base_pixbuf)
        g_object_unref(data_view->base_pixbuf);
    data_view->pixbuf = NULL;
    data_view->base_pixbuf = NULL;

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
    requisition->width = data_view->zoom * gwy_data_field_get_xres(data_field);
    requisition->height = data_view->zoom * gwy_data_field_get_yres(data_field);

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
    scwidth = floor(src_width * data_view->zoom + 0.000001);
    scheight = floor(src_height * data_view->zoom + 0.000001);
    if (scwidth != width || scheight != height) {
        if (data_view->pixbuf)
            g_object_unref(data_view->pixbuf);
        data_view->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                           TRUE,
                                           BITS_PER_SAMPLE,
                                           scwidth, scheight);
        gdk_pixbuf_fill(data_view->pixbuf, 0x00000000);
        gwy_data_view_paint(data_view);
    }
}

static gboolean
gwy_data_view_paint_layer(GwyDataView *data_view,
                          GwyDataViewLayer *layer,
                          gboolean to_base,
                          gboolean is_base,
                          gboolean is_top)
{
    gint height, width, src_height, src_width;
    GdkPixbuf *pixbuf, *src_pixbuf;
    gboolean copy_base_to_top = FALSE;
    gboolean is_vector = FALSE;
    GTimer *timer = NULL;

    if (is_base)
        g_assert(layer);

    if (!layer) {
        if (is_top && to_base)
            copy_base_to_top = TRUE;
        else
            return to_base;
    }
    else {
        is_vector = gwy_data_view_layer_is_vector(layer);
        g_assert(!is_base || !is_vector);
        if (is_vector && to_base)
            copy_base_to_top = TRUE;
    }

    pixbuf = data_view->pixbuf;
    /* we have to rescale base to final pixbuf once, at least at the end */
    if (copy_base_to_top) {
        timer = g_timer_new();
        src_pixbuf = data_view->base_pixbuf;
        src_width = gdk_pixbuf_get_width(src_pixbuf);
        src_height = gdk_pixbuf_get_height(src_pixbuf);
        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);
        gdk_pixbuf_scale(src_pixbuf, pixbuf,
                         0, 0, width, height,
                         0.0, 0.0,
                         (double)width/src_width, (double)height/src_height,
                         GDK_INTERP_TILES);
        g_message("%s: b->t %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
        g_timer_destroy(timer);
    }
    if (!layer)
        return FALSE;

    /* layer draws on existing pixbufs */
    if (is_vector) {
        /* FIXME: wrong */
        gwy_data_view_layer_draw(layer, pixbuf);
        return FALSE;
    }

    /* layer paints pixbufs */
    src_pixbuf = gwy_data_view_layer_paint(layer);
    src_width = gdk_pixbuf_get_width(src_pixbuf);
    src_height = gdk_pixbuf_get_height(src_pixbuf);
    if (to_base) {
        timer = g_timer_new();
        /* compose to base, no need for scaling */
        pixbuf = data_view->base_pixbuf;
        width = gdk_pixbuf_get_width(pixbuf);
        g_assert(width == src_width);
        height = gdk_pixbuf_get_height(pixbuf);
        g_assert(height == src_height);
        if (is_base)
            gdk_pixbuf_copy_area(src_pixbuf, 0, 0, src_width, src_height,
                                 pixbuf, 0, 0);
        else
            gdk_pixbuf_composite(src_pixbuf, pixbuf,
                                 0, 0, width, height,
                                 0.0, 0.0,
                                 1.0, 1.0,
                                 GDK_INTERP_TILES, 0x255);
        g_message("%s: %s %gs",
                  __FUNCTION__, is_base ? "copy" : "b1:1",
                  g_timer_elapsed(timer, NULL));
        g_timer_destroy(timer);
        return TRUE;
    }
    /* compose to scaled */
    g_assert(!is_base);
    timer = g_timer_new();
    pixbuf = data_view->pixbuf;
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    gdk_pixbuf_composite(src_pixbuf, pixbuf,
                         0, 0, width, height,
                         0.0, 0.0,
                         (double)width/src_width, (double)height/src_height,
                         GDK_INTERP_TILES, 0x255);
    g_message("%s: sZ:Z %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);
    return FALSE;
}

static void
gwy_data_view_paint(GwyDataView *data_view)
{
    gboolean to_base = TRUE;
    GTimer *timer = NULL;

    timer = g_timer_new();
    to_base = gwy_data_view_paint_layer(data_view, data_view->base_layer,
                                        to_base, TRUE, FALSE);
    g_message("%s: base %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    g_timer_reset(timer);
    /* there could be any number of these */
    to_base = gwy_data_view_paint_layer(data_view, data_view->alpha_layer,
                                        to_base, FALSE, FALSE);
    g_message("%s: alpha %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    g_timer_reset(timer);
    to_base = gwy_data_view_paint_layer(data_view, data_view->top_layer,
                                        to_base, FALSE, TRUE);
    g_message("%s: top %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);
}

static gboolean
gwy_data_view_expose(GtkWidget *widget,
                     GdkEventExpose *event)
{
    GwyDataView *data_view;
    gint xc, yc;
    GTimer *timer = NULL;

    g_return_val_if_fail(widget, FALSE);
    g_return_val_if_fail(GWY_IS_DATA_VIEW(widget), FALSE);

    if (event->count > 0)
        return FALSE;

    gdk_window_set_back_pixmap(widget->window, NULL, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    /* FIXME: ask the layers, if they want to repaint themselves */
    if (gwy_data_view_layer_wants_repaint(data_view->base_layer)
        || gwy_data_view_layer_wants_repaint(data_view->alpha_layer)
        || gwy_data_view_layer_wants_repaint(data_view->top_layer))
        gwy_data_view_paint(data_view);

    xc = (widget->allocation.width
          - gdk_pixbuf_get_width(data_view->pixbuf))/2;
    yc = (widget->allocation.height
          - gdk_pixbuf_get_height(data_view->pixbuf))/2;
    timer = g_timer_new();
    gdk_draw_pixbuf(widget->window,
                    NULL,
                    data_view->pixbuf,
                    0, 0,
                    xc, yc,
                    -1, -1,
                    GDK_RGB_DITHER_NORMAL,
                    0, 0);
    g_message("%s: buf->map %gs", __FUNCTION__, g_timer_elapsed(timer, NULL));
    g_timer_destroy(timer);

    return FALSE;
}

static gboolean
gwy_data_view_button_press(GtkWidget *widget,
                           GdkEventButton *event)
{
    GwyDataView *data_view;

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
    g_return_if_fail(!layer || GWY_IS_DATA_VIEW_LAYER(layer));

    if (layer == *which)
        return;
    if (*which) {
        g_object_unref(*which);
    }
    if (layer) {
        g_assert(layer->parent == NULL);
        g_object_ref(layer);
        gtk_object_sink(GTK_OBJECT(layer));
        layer->parent = (GtkWidget*)data_view;
    }
    *which = layer;
    gwy_data_view_update(data_view);
}

void
gwy_data_view_set_base_layer(GwyDataView *data_view,
                             GwyDataViewLayer *layer)
{
    gwy_data_view_set_layer(data_view, &data_view->base_layer, layer);
}

void
gwy_data_view_set_alpha_layer(GwyDataView *data_view,
                              GwyDataViewLayer *layer)
{
    gwy_data_view_set_layer(data_view, &data_view->alpha_layer, layer);
}

void
gwy_data_view_set_top_layer(GwyDataView *data_view,
                            GwyDataViewLayer *layer)
{
    gwy_data_view_set_layer(data_view, &data_view->top_layer, layer);
}

gdouble
gwy_data_view_get_hexcess(GwyDataView* data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 0);
    if (!data_view->pixbuf)
        return 0;

    return (gdouble)GTK_WIDGET(data_view)->allocation.width
                    / gdk_pixbuf_get_width(data_view->pixbuf) - 1.0;
}

gdouble
gwy_data_view_get_vexcess(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 0);
    if (!data_view->pixbuf)
        return 0;

    return (gdouble)GTK_WIDGET(data_view)->allocation.height
                    / gdk_pixbuf_get_height(data_view->pixbuf) - 1.0;
}

void
gwy_data_view_set_zoom(GwyDataView *data_view,
                       gdouble zoom)
{
    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    if (!data_view->pixbuf || !data_view->base_pixbuf)
        return;

    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "%s: zoom = %g, new = %g", __FUNCTION__, data_view->zoom, zoom);
    if (fabs(data_view->zoom - zoom) < 0.01)
        return;

    data_view->zoom = zoom;
    gtk_widget_queue_resize(GTK_WIDGET(data_view));
}

gdouble
gwy_data_view_get_zoom(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->zoom;
}

GwyContainer*
gwy_data_view_get_data(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    return data_view->data;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
