/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwydataview.h"
#include "gwydataviewlayer.h"

#define GWY_DATA_VIEW_LAYER_TYPE_NAME "GwyDataViewLayer"

#define BITS_PER_SAMPLE 8

enum {
    PLUGGED,
    UNPLUGGED,
    UPDATED,
    FINISHED,
    LAST_SIGNAL
};

/* Forward declarations */

static void     gwy_data_view_layer_class_init   (GwyDataViewLayerClass *klass);
static void     gwy_data_view_layer_init         (GwyDataViewLayer *layer);
static void     gwy_data_view_layer_finalize     (GObject *object);
static void     gwy_data_view_layer_real_plugged (GwyDataViewLayer *layer);
static void     gwy_data_view_layer_real_unplugged (GwyDataViewLayer *layer);

/* Local data */

static GtkObjectClass *parent_class = NULL;

static guint data_view_layer_signals[LAST_SIGNAL] = { 0 };

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
        gwy_debug("%s", __FUNCTION__);
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
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_data_view_layer_finalize;

    klass->paint = NULL;
    klass->draw = NULL;
    klass->wants_repaint = NULL;

    klass->button_press = NULL;
    klass->button_release = NULL;
    klass->motion_notify = NULL;
    klass->key_press = NULL;
    klass->key_release = NULL;

    klass->plugged = gwy_data_view_layer_real_plugged;
    klass->unplugged = gwy_data_view_layer_real_unplugged;
    klass->updated = NULL;
    klass->finished = NULL;

    data_view_layer_signals[PLUGGED] =
        g_signal_new("plugged",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, plugged),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
    data_view_layer_signals[UNPLUGGED] =
        g_signal_new("unplugged",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, unplugged),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
    data_view_layer_signals[UPDATED] =
        g_signal_new("updated",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, updated),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
    data_view_layer_signals[FINISHED] =
        g_signal_new("finished",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyDataViewLayerClass, finished),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void
gwy_data_view_layer_init(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);

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

    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(object));

    layer = GWY_DATA_VIEW_LAYER(object);

    gwy_object_unref(layer->gc);
    gwy_object_unref(layer->layout);
    gwy_object_unref(layer->palette);
    gwy_object_unref(layer->pixbuf);
    gwy_object_unref(layer->data);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_data_view_layer_is_vector:
 * @layer: A data view layer.
 *
 * Tests whether a layer is a vector layer (otherwise it's a pixmap layer).
 *
 * Returns: %TRUE when @layer is a vector layer, %FALSE otherwise.
 **/
gboolean
gwy_data_view_layer_is_vector(GwyDataViewLayer *layer)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    g_assert(layer_class->paint || layer_class->draw);
    return !layer_class->paint;
}

/**
 * gwy_data_view_layer_wants_repaint:
 * @layer: A data view layer.
 *
 * Checks whether a layer wants repaint.
 * FIXME FIXME FIXME  This is probably flawed and will be replaced by
 * a signal.
 *
 * Returns: %TRUE if the the layer wants repaint itself, %FALSE otherwise.
 **/
gboolean
gwy_data_view_layer_wants_repaint(GwyDataViewLayer *layer)
{
    GwyDataViewLayerClass *layer_class;

    if (!layer)
        return FALSE;

    layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);
    /* when a layer doesn't have wants_repaint, assume it always wants */
    if (!layer_class->wants_repaint)
        return TRUE;

    return layer_class->wants_repaint(layer);
}

/**
 * gwy_data_view_layer_draw:
 * @layer: A data view layer.
 * @drawable: A drawable to draw on.
 *
 * Draws @layer on given drawable (which should be a #GwyDataView window).
 *
 * The layer must be a vector layer.  Use gwy_data_view_layer_paint()
 * for pixmap layers.
 **/
void
gwy_data_view_layer_draw(GwyDataViewLayer *layer,
                         GdkDrawable *drawable)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    g_return_if_fail(layer_class->draw);
    layer_class->draw(layer, drawable);
}

/**
 * gwy_data_view_layer_paint:
 * @layer: A data view layer.
 *
 * Returns a pixbuf with painted pixmap layer @layer.
 *
 * Returns: The pixbuf.  It should not be modified or freed.  The layer must
 * be a pixmap layer.  Use gwy_data_view_layer_draw() for vector layers.
 **/
GdkPixbuf*
gwy_data_view_layer_paint(GwyDataViewLayer *layer)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    g_return_val_if_fail(layer_class->paint, NULL);
    return layer_class->paint(layer);
}

/**
 * gwy_data_view_layer_button_press:
 * @layer: A data view layer.
 * @event: A Gdk mouse button event.
 *
 * Sends a mouse button press event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_data_view_layer_button_press(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    gwy_debug("%s", __FUNCTION__);
    if (layer_class->button_press)
        return layer_class->button_press(layer, event);
    return FALSE;
}

/**
 * gwy_data_view_layer_button_release:
 * @layer: A data view layer.
 * @event: A Gdk mouse button event.
 *
 * Sends a mouse button release event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_data_view_layer_button_release(GwyDataViewLayer *layer,
                                   GdkEventButton *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->button_release)
        return layer_class->button_release(layer, event);
    return FALSE;
}

/**
 * gwy_data_view_layer_motion_notify:
 * @layer: A data view layer.
 * @event: A Gdk mouse pointer motion notification event.
 *
 * Sends a mouse pointer motion notification event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_data_view_layer_motion_notify(GwyDataViewLayer *layer,
                                  GdkEventMotion *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->motion_notify)
        return layer_class->motion_notify(layer, event);
    return FALSE;
}

/**
 * gwy_data_view_layer_key_press:
 * @layer: A data view layer.
 * @event: A Gdk key event.
 *
 * Sends a key press event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_data_view_layer_key_press(GwyDataViewLayer *layer,
                              GdkEventKey *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->key_press)
        return layer_class->key_press(layer, event);
    return FALSE;
}

/**
 * gwy_data_view_layer_key_release:
 * @layer: A data view layer.
 * @event: A Gdk key event.
 *
 * Sends a key release event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_data_view_layer_key_release(GwyDataViewLayer *layer,
                                GdkEventKey *event)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_GET_CLASS(layer);

    if (layer_class->key_release)
        return layer_class->key_release(layer, event);
    return FALSE;
}

/**
 * gwy_data_view_layer_plugged:
 * @layer: A data view layer.
 *
 * Emits a "plugged" singal on a layer.
 **/
void
gwy_data_view_layer_plugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[PLUGGED], 0);
}

/**
 * gwy_data_view_layer_unplugged:
 * @layer: A data view layer.
 *
 * Emits a "unplugged" singal on a layer.
 **/
void
gwy_data_view_layer_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[UNPLUGGED], 0);
}

/**
 * gwy_data_view_layer_updated:
 * @layer: A data view layer.
 *
 * Emits a "updated" singal on a layer.
 **/
void
gwy_data_view_layer_updated(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[UPDATED], 0);
}

/**
 * gwy_data_view_layer_finished:
 * @layer: A data view layer.
 *
 * Emits a "finished" singal on a layer.
 **/
void
gwy_data_view_layer_finished(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_DATA_VIEW_LAYER(layer));
    g_signal_emit(layer, data_view_layer_signals[FINISHED], 0);
}

static void
gwy_data_view_layer_real_plugged(GwyDataViewLayer *layer)
{
    GwyContainer *data;

    gwy_debug("%s", __FUNCTION__);

    gwy_object_unref(layer->data);

    data = gwy_data_view_get_data(GWY_DATA_VIEW(layer->parent));
    g_return_if_fail(GWY_IS_CONTAINER(data));
    g_object_ref(data);
    layer->data = data;
}

static void
gwy_data_view_layer_real_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);

    gwy_object_unref(layer->gc);
    gwy_object_unref(layer->data);
}

/**
 * gwy_layer_cursor_new_or_ref:
 * @cursor: A Gdk cursor, or %NULL.
 * @type: Cursor type to eventually create.
 *
 * Increments reference count of a given Gdk cursor or creates a new one
 * (if @cursor is NULL) of type @cursor_type.
 **/
void
gwy_layer_cursor_new_or_ref(GdkCursor **cursor,
                            GdkCursorType type)
{
    g_return_if_fail(cursor);

    if (*cursor)
        gdk_cursor_ref(*cursor);
    else
        *cursor = gdk_cursor_new(type);
}

/**
 * gwy_layer_cursor_free_or_unref:
 * @cursor: A Gdk cursor.
 *
 * Decrements reference count of a Gdk cursor, possibly freeing it.
 **/
void
gwy_layer_cursor_free_or_unref(GdkCursor **cursor)
{
    int refcount;

    g_return_if_fail(cursor);
    g_return_if_fail(*cursor);

    refcount = (*cursor)->ref_count - 1;
    gdk_cursor_unref(*cursor);
    if (!refcount)
        *cursor = NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
