/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libprocess/datafield.h>
#include "gwydataview.h"

#define GWY_DATA_VIEW_TYPE_NAME "GwyDataView"

#define BITS_PER_SAMPLE 8

enum {
    UPDATED,
    REDRAWN,
    LAST_SIGNAL
};

/* Forward declarations */

static void     gwy_data_view_class_init           (GwyDataViewClass *klass);
static void     gwy_data_view_init                 (GwyDataView *data_view);
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
static void     gwy_data_view_set_layer            (GwyDataView *data_view,
                                                    gpointer which,
                                                    gulong *hid,
                                                    GwyDataViewLayer *layer);


/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint data_view_signals[LAST_SIGNAL] = { 0 };

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
        gwy_debug(" ");
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

    gwy_debug(" ");

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

    klass->redrawn = NULL;

/**
 * GwyDataView::redrawn:
 * @gwydataview: The #GwyDataView which received the signal.
 *
 * The ::redrawn signal is emitted when #GwyDataView redraws pixbufs after an
 * update.  That is, when it's the right time to get a new pixbuf from
 * gwy_data_view_get_thumbnail() or gwy_data_view_get_pixbuf().
 */
    data_view_signals[REDRAWN]
        = g_signal_new("redrawn",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyDataViewClass, redrawn),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);
}

static void
gwy_data_view_init(GwyDataView *data_view)
{
    gwy_debug(" ");

    data_view->zoom = 1.0;
    data_view->newzoom = 1.0;
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

    gwy_debug(" ");
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

    gwy_debug("destroying a GwyDataView %p (refcount = %u)",
              object, G_OBJECT(object)->ref_count);

    g_return_if_fail(GWY_IS_DATA_VIEW(object));

    data_view = GWY_DATA_VIEW(object);
    gwy_data_view_set_layer(data_view, &data_view->top_layer, NULL, NULL);
    gwy_data_view_set_layer(data_view, &data_view->alpha_layer,
                            &data_view->alpha_hid, NULL);
    gwy_data_view_set_layer(data_view, &data_view->base_layer,
                            &data_view->base_hid, NULL);

    if (GTK_OBJECT_CLASS(parent_class)->destroy)
        (*GTK_OBJECT_CLASS(parent_class)->destroy)(object);
}

static void
gwy_data_view_finalize(GObject *object)
{
    GwyDataView *data_view;

    gwy_debug("finalizing a GwyDataView (refcount = %u)",
              object->ref_count);

    g_return_if_fail(GWY_IS_DATA_VIEW(object));

    data_view = GWY_DATA_VIEW(object);

    gwy_object_unref(data_view->base_layer);
    gwy_object_unref(data_view->alpha_layer);
    gwy_object_unref(data_view->top_layer);
    gwy_debug("    child data ref count %d", G_OBJECT(data_view->data)->ref_count);
    gwy_object_unref(data_view->data);

    G_OBJECT_CLASS(parent_class)->finalize(object);
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

    gwy_debug("realizing a GwyDataView (%ux%u)",
              widget->allocation.width, widget->allocation.height);

    g_return_if_fail(widget != NULL);

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
    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
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
    const gchar *key;

    gwy_debug(" ");

    data_view = GWY_DATA_VIEW(widget);
    data = data_view->data;
    requisition->width = requisition->height = 2;
    if (!data_view->base_layer)
        return;
    key = gwy_pixmap_layer_get_data_key(data_view->base_layer);
    if (!key)
        return;
    data_field = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, key));
    if (!data_field)
        return;
    requisition->width = data_view->newzoom
                         * gwy_data_field_get_xres(data_field);
    requisition->height = data_view->newzoom
                          * gwy_data_field_get_yres(data_field);

    gwy_debug("requesting %d x %d",
              requisition->width, requisition->height);
}

static void
gwy_data_view_size_allocate(GtkWidget *widget,
                            GtkAllocation *allocation)
{
    GwyDataView *data_view;

    gwy_debug("allocating %d x %d",
              allocation->width, allocation->height);

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
    const gchar *key;
    gint width, height, scwidth, scheight, src_width, src_height;

    key = gwy_pixmap_layer_get_data_key(data_view->base_layer);
    data_field
        = GWY_DATA_FIELD(gwy_container_get_object_by_name(data_view->data,
                                                          key));
    src_width = gwy_data_field_get_xres(data_field);
    src_height = gwy_data_field_get_yres(data_field);

    if (!data_view->base_pixbuf) {
        data_view->base_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB,
                                                FALSE,
                                                BITS_PER_SAMPLE,
                                                src_width, src_height);
        gwy_debug_objects_creation(G_OBJECT(data_view->base_pixbuf));
    }

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
                                           FALSE,
                                           BITS_PER_SAMPLE,
                                           scwidth, scheight);
        gwy_debug_objects_creation(G_OBJECT(data_view->pixbuf));
        gdk_pixbuf_fill(data_view->pixbuf, 0x00000000);
        gwy_data_view_paint(data_view);
    }
}

static void
simple_gdk_pixbuf_scale_or_copy(GdkPixbuf *source, GdkPixbuf *dest)
{
    gint height, width, src_height, src_width;

    src_height = gdk_pixbuf_get_height(source);
    src_width = gdk_pixbuf_get_width(source);
    height = gdk_pixbuf_get_height(dest);
    width = gdk_pixbuf_get_width(dest);

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
    src_width = gdk_pixbuf_get_width(source);
    height = gdk_pixbuf_get_height(dest);
    width = gdk_pixbuf_get_width(dest);

    gdk_pixbuf_composite(source, dest, 0, 0, width, height, 0.0, 0.0,
                         (gdouble)width/src_width, (gdouble)height/src_height,
                         GDK_INTERP_TILES, 0xff);
}

static void
gwy_data_view_paint(GwyDataView *data_view)
{
    GdkPixbuf *src_pixbuf;

    gwy_debug(" ");
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(data_view->base_layer));

    /* base layer is always present
     * top layer is always vector, if any
     */
    if (!data_view->alpha_layer) {
        /* scale base directly to final pixbuf */
        src_pixbuf
            = gwy_pixmap_layer_paint(GWY_PIXMAP_LAYER(data_view->base_layer));
        simple_gdk_pixbuf_scale_or_copy(src_pixbuf, data_view->pixbuf);
    }
    else {
        /* base */
        src_pixbuf
            = gwy_pixmap_layer_paint(GWY_PIXMAP_LAYER(data_view->base_layer));
        simple_gdk_pixbuf_scale_or_copy(src_pixbuf, data_view->base_pixbuf);

        /* composite with alpha */
        src_pixbuf
            = gwy_pixmap_layer_paint(GWY_PIXMAP_LAYER(data_view->alpha_layer));
        simple_gdk_pixbuf_composite(src_pixbuf, data_view->base_pixbuf);

        /* scale both */
        simple_gdk_pixbuf_scale_or_copy(data_view->base_pixbuf,
                                        data_view->pixbuf);
    }
}

static gboolean
gwy_data_view_expose(GtkWidget *widget,
                     GdkEventExpose *event)
{
    GwyDataView *data_view;
    gint xs, ys, xe, ye, w, h;
    GdkRectangle rect;
    gboolean emit_redrawn = FALSE;

    data_view = GWY_DATA_VIEW(widget);

    gdk_region_get_clipbox(event->region, &rect);
    gwy_debug("bbox = %dx%d  at (%d,%d)",
              rect.width, rect.height, rect.x, rect.y);
    w = gdk_pixbuf_get_width(data_view->pixbuf);
    h = gdk_pixbuf_get_height(data_view->pixbuf);
    xs = MAX(rect.x, data_view->xoff) - data_view->xoff;
    ys = MAX(rect.y, data_view->yoff) - data_view->yoff;
    xe = MIN(rect.x + rect.width, data_view->xoff + w) - data_view->xoff;
    ye = MIN(rect.y + rect.height, data_view->yoff + h) - data_view->yoff;
    gwy_debug("going to draw: %dx%d  at (%d,%d)",
              xe - xs, ye - ys, xs, ys);
    if (xs >= xe || ys >= ye)
        return FALSE;

    if (data_view->layers_changed
        || (data_view->base_layer
            && gwy_pixmap_layer_wants_repaint(data_view->base_layer))
        || (data_view->alpha_layer
            && gwy_pixmap_layer_wants_repaint(data_view->alpha_layer))) {
        gwy_data_view_paint(data_view);
        emit_redrawn = TRUE;
        data_view->layers_changed = FALSE;
    }

    gdk_draw_pixbuf(widget->window,
                    NULL,
                    data_view->pixbuf,
                    xs, ys,
                    xs + data_view->xoff, ys + data_view->yoff,
                    xe - xs, ye - ys,
                    GDK_RGB_DITHER_NORMAL,
                    0, 0);

    if (data_view->top_layer)
        gwy_vector_layer_draw(GWY_VECTOR_LAYER(data_view->top_layer),
                              widget->window);

    if (emit_redrawn)
        g_signal_emit(data_view, data_view_signals[REDRAWN], 0);

    return FALSE;
}

static gboolean
gwy_data_view_button_press(GtkWidget *widget,
                           GdkEventButton *event)
{
    GwyDataView *data_view;

    gwy_debug(" ");
    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_button_press(GWY_VECTOR_LAYER(data_view->top_layer), event);
}

static gboolean
gwy_data_view_button_release(GtkWidget *widget,
                             GdkEventButton *event)
{
    GwyDataView *data_view;

    gwy_debug(" ");
    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_button_release(GWY_VECTOR_LAYER(data_view->top_layer), event);
}

static gboolean
gwy_data_view_motion_notify(GtkWidget *widget,
                            GdkEventMotion *event)
{
    GwyDataView *data_view;

    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_motion_notify(GWY_VECTOR_LAYER(data_view->top_layer), event);
}

static gboolean
gwy_data_view_key_press(GtkWidget *widget,
                        GdkEventKey *event)
{
    GwyDataView *data_view;

    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_key_press(GWY_VECTOR_LAYER(data_view->top_layer), event);
}

static gboolean
gwy_data_view_key_release(GtkWidget *widget,
                          GdkEventKey *event)
{
    GwyDataView *data_view;

    g_return_val_if_fail(event, FALSE);

    data_view = GWY_DATA_VIEW(widget);
    if (!data_view->top_layer)
        return FALSE;

    return gwy_vector_layer_key_release(GWY_VECTOR_LAYER(data_view->top_layer), event);
}

/**
 * gwy_data_view_update:
 * @data_view: A #GwyDataView.
 *
 * Instructs a data view to update self and repaint.
 *
 * XXX: Deprecated.  It merely causes an expose event on the data view now.
 **/
void
gwy_data_view_update(GwyDataView *data_view)
{
    GtkWidget *widget;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));

    widget = GTK_WIDGET(data_view);
    if (widget->window)
        gdk_window_invalidate_rect(widget->window, NULL, TRUE);
}

/**
 * gwy_data_view_get_base_layer:
 * @data_view: A #GwyDataView.
 *
 * Returns the base layer this data view currently uses.
 *
 * A base layer should be always present.
 *
 * Returns: The currently used base layer.
 **/
GwyPixmapLayer*
gwy_data_view_get_base_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);

    return GWY_PIXMAP_LAYER(data_view->base_layer);
}

/**
 * gwy_data_view_get_alpha_layer:
 * @data_view: A #GwyDataView.
 *
 * Returns the alpha layer this data view currently uses, or %NULL if none
 * is present.
 *
 * Returns: The currently used alpha layer.
 **/
GwyPixmapLayer*
gwy_data_view_get_alpha_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);

    return GWY_PIXMAP_LAYER(data_view->alpha_layer);
}

/**
 * gwy_data_view_get_top_layer:
 * @data_view: A #GwyDataView.
 *
 * Returns the top layer this data view currently uses, or %NULL if none
 * is present.
 *
 * Returns: The currently used top layer.
 **/
GwyVectorLayer*
gwy_data_view_get_top_layer(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);

    return GWY_VECTOR_LAYER(data_view->top_layer);
}

static void
gwy_data_view_set_layer(GwyDataView *data_view,
                        gpointer which,
                        gulong *hid,
                        GwyDataViewLayer *layer)
{
    GwyDataViewLayer **which_layer;

    g_return_if_fail(GWY_IS_DATA_VIEW(data_view));
    g_return_if_fail(which);

    which_layer = (GwyDataViewLayer**)which;
    if (layer == *which_layer)
        return;
    if (*which_layer) {
        if (hid) {
            g_signal_handler_disconnect(*which_layer, *hid);
            *hid = 0;
        }
        gwy_data_view_layer_unplugged(*which_layer);
        (*which_layer)->parent = NULL;
        g_object_unref(*which_layer);
    }
    if (layer) {
        g_assert(layer->parent == NULL);
        g_object_ref(layer);
        gtk_object_sink(GTK_OBJECT(layer));
        layer->parent = (GtkWidget*)data_view;
        if (hid)
            *hid = g_signal_connect_swapped(layer, "updated",
                                            G_CALLBACK(gwy_data_view_update),
                                            data_view);
        gwy_data_view_layer_plugged(layer);
    }
    data_view->layers_changed = TRUE;
    *which_layer = layer;
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
                             GwyPixmapLayer *layer)
{
    g_return_if_fail(!layer || GWY_IS_PIXMAP_LAYER(layer));
    gwy_data_view_set_layer(data_view,
                            &data_view->base_layer,
                            &data_view->base_hid,
                            GWY_DATA_VIEW_LAYER(layer));
    gtk_widget_queue_resize(GTK_WIDGET(data_view));
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
                              GwyPixmapLayer *layer)
{
    g_return_if_fail(!layer || GWY_IS_PIXMAP_LAYER(layer));
    gwy_data_view_set_layer(data_view,
                            &data_view->alpha_layer,
                            &data_view->alpha_hid,
                            GWY_DATA_VIEW_LAYER(layer));
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
                            GwyVectorLayer *layer)
{
    g_return_if_fail(!layer || GWY_IS_VECTOR_LAYER(layer));
    gwy_data_view_set_layer(data_view, &data_view->top_layer, NULL,
                            GWY_DATA_VIEW_LAYER(layer));
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
    /* XXX: what was this meant for?
    if (!data_view->pixbuf || !data_view->base_pixbuf)
        return;
        */

    gwy_debug("zoom = %g, new = %g", data_view->newzoom, zoom);
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
 * When a resize is queued, the new zoom value is returned.
 *
 * Returns: The zoom.
 **/
gdouble
gwy_data_view_get_zoom(GwyDataView *data_view)
{
    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), 1.0);
    return data_view->newzoom;
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

/**
 * gwy_data_view_get_thumbnail:
 * @data_view: A #GwyDataView.
 * @size: Requested thumbnail size.
 *
 * Creates and returns a thumbnail of the data view.
 *
 * If the data is not square, it is centered onto the pixbuf, with transparent
 * borders.  The returned pixbuf always has an alpha channel, even if it fits
 * exactly.
 *
 * Returns: The thumbnail as a newly created #GdkPixbuf, which should be freed
 *          when no longer needed.
 **/
GdkPixbuf*
gwy_data_view_get_thumbnail(GwyDataView *data_view,
                            gint size)
{
    GdkPixbuf *pixbuf;
    gint width, height, width_scaled, height_scaled;
    gdouble scale;

    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    g_return_val_if_fail(data_view->pixbuf, NULL);
    g_return_val_if_fail(size > 0, NULL);
    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE,
                            BITS_PER_SAMPLE, size, size);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gdk_pixbuf_fill(pixbuf, 0x00000000);
    width = gdk_pixbuf_get_width(data_view->pixbuf);
    height = gdk_pixbuf_get_height(data_view->pixbuf);
    scale = MIN((gdouble)size/width, (gdouble)size/height);
    width_scaled = CLAMP((gint)(scale*width), 1, size);
    height_scaled = CLAMP((gint)(scale*height), 1, size);
    gdk_pixbuf_scale(data_view->pixbuf, pixbuf,
                     (size - width_scaled)/2, (size - height_scaled)/2,
                     width_scaled, height_scaled,
                     (size - width_scaled)/2, (size - height_scaled)/2,
                     scale, scale, GDK_INTERP_TILES);

    return pixbuf;
}

/**
 * gwy_data_view_get_pixbuf:
 * @data_view: A #GwyDataView.
 * @max_width: Pixbuf width that should not be exceeeded.  Value smaller than
 *             1 means unlimited size.
 * @max_height: Pixbuf height that should not be exceeeded.  Value smaller than
 *              1 means unlimited size.
 *
 * Creates and returns a pixbuf from the data view.
 *
 * If the data is not square, the resulting pixbuf is also nonsquare, this is
 * different from gwy_data_view_get_thumbnail().  The returned pixbuf also
 * never has alpha channel.
 *
 * Returns: The pixbuf as a newly created #GdkPixbuf, it should be freed
 *          when no longer needed.  It is never larger than the actual data
 *          size, as @max_width and @max_height are only upper limits.
 **/
GdkPixbuf*
gwy_data_view_get_pixbuf(GwyDataView *data_view,
                         gint max_width,
                         gint max_height)
{
    GdkPixbuf *pixbuf;
    gint width, height, width_scaled, height_scaled;
    gdouble xscale, yscale, scale;

    g_return_val_if_fail(GWY_IS_DATA_VIEW(data_view), NULL);
    g_return_val_if_fail(data_view->pixbuf, NULL);

    width = gdk_pixbuf_get_width(data_view->pixbuf);
    height = gdk_pixbuf_get_height(data_view->pixbuf);
    xscale = (max_width > 0) ? (gdouble)max_width/width : 1.0;
    yscale = (max_height > 0) ? (gdouble)max_height/height : 1.0;
    scale = MIN(MIN(xscale, yscale), 1.0);
    width_scaled = CLAMP((gint)(scale*width), 1, max_width);
    height_scaled = CLAMP((gint)(scale*height), 1, max_height);

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE,
                            BITS_PER_SAMPLE, width_scaled, height_scaled);
    gwy_debug_objects_creation(G_OBJECT(pixbuf));
    gdk_pixbuf_scale(data_view->pixbuf, pixbuf, 0, 0,
                     width_scaled, height_scaled, 0.0, 0.0,
                     scale, scale, GDK_INTERP_TILES);

    return pixbuf;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
