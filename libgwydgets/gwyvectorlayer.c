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

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include "gwyvectorlayer.h"

#define GWY_VECTOR_LAYER_TYPE_NAME "GwyVectorLayer"

#define GWY_SCROLL_DELAY_LENGTH  300
#define BITS_PER_SAMPLE 8

enum {
    SELECTION_FINISHED,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_UPDATE_POLICY,
    PROP_LAST
};

/* Forward declarations */

static void     gwy_vector_layer_class_init   (GwyVectorLayerClass *klass);
static void     gwy_vector_layer_init         (GwyVectorLayer *layer);
static void     gwy_vector_layer_finalize     (GObject *object);
static void     gwy_vector_layer_set_property (GObject *object,
                                               guint prop_id,
                                               const GValue *value,
                                               GParamSpec *pspec);
static void     gwy_vector_layer_get_property (GObject*object,
                                               guint prop_id,
                                               GValue *value,
                                               GParamSpec *pspec);
static void     gwy_vector_layer_plugged      (GwyDataViewLayer *layer);
static void     gwy_vector_layer_unplugged    (GwyDataViewLayer *layer);
static void     gwy_vector_layer_real_updated (GwyDataViewLayer *layer);
static gboolean gwy_vector_layer_timer        (GwyVectorLayer *layer);

/* Local data */

static GwyDataViewLayerClass *parent_class = NULL;

static guint vector_layer_signals[LAST_SIGNAL] = { 0 };

GType
gwy_vector_layer_get_type(void)
{
    static GType gwy_vector_layer_type = 0;

    if (!gwy_vector_layer_type) {
        static const GTypeInfo gwy_vector_layer_info = {
            sizeof(GwyVectorLayerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_vector_layer_class_init,
            NULL,
            NULL,
            sizeof(GwyVectorLayer),
            0,
            (GInstanceInitFunc)gwy_vector_layer_init,
            NULL,
        };
        gwy_debug("");
        gwy_vector_layer_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_VECTOR_LAYER_TYPE_NAME,
                                     &gwy_vector_layer_info,
                                     0);
    }

    return gwy_vector_layer_type;
}

static void
gwy_vector_layer_class_init(GwyVectorLayerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_vector_layer_finalize;
    gobject_class->set_property = gwy_vector_layer_set_property;
    gobject_class->get_property = gwy_vector_layer_get_property;

    layer_class->plugged = gwy_vector_layer_plugged;
    layer_class->unplugged = gwy_vector_layer_unplugged;
    layer_class->wants_repaint = NULL;  /* always wants */
    layer_class->updated = gwy_vector_layer_real_updated;

    klass->draw = NULL;

    klass->button_press = NULL;
    klass->button_release = NULL;
    klass->motion_notify = NULL;
    klass->key_press = NULL;
    klass->key_release = NULL;

    klass->selection_finished = NULL;
    klass->get_selection = NULL;
    klass->set_selection = NULL;
    klass->unselect = NULL;

    vector_layer_signals[SELECTION_FINISHED] =
        g_signal_new("selection_finished",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyVectorLayerClass, selection_finished),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    g_object_class_install_property(
        gobject_class,
        PROP_UPDATE_POLICY,
        g_param_spec_enum("update_policy",
                          _("Update Policy"),
                          _("When value changed causes signal emission"),
                          GTK_TYPE_UPDATE_TYPE,
                          GTK_UPDATE_CONTINUOUS,
                          G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_vector_layer_init(GwyVectorLayer *layer)
{
    gwy_debug("");

    layer->gc = NULL;
    layer->layout = NULL;
    layer->timer = 0;
    layer->update_policy = GTK_UPDATE_CONTINUOUS;
    layer->in_selection = FALSE;
}

static void
gwy_vector_layer_finalize(GObject *object)
{
    GwyVectorLayer *layer;

    gwy_debug("");

    g_return_if_fail(GWY_IS_VECTOR_LAYER(object));

    layer = GWY_VECTOR_LAYER(object);

    gwy_object_unref(layer->gc);
    gwy_object_unref(layer->layout);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_vector_layer_set_property(GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    GwyVectorLayer *layer = GWY_VECTOR_LAYER(object);

    switch (prop_id) {
        case PROP_UPDATE_POLICY:
        gwy_vector_layer_set_update_policy(layer, g_value_get_enum(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_vector_layer_get_property(GObject*object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    GwyVectorLayer *layer = GWY_VECTOR_LAYER(object);

    switch (prop_id) {
        case PROP_UPDATE_POLICY:
        g_value_set_enum(value, layer->update_policy);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * gwy_vector_layer_draw:
 * @layer: A vector data view layer.
 * @drawable: A drawable to draw on.
 *
 * Draws @layer on given drawable (which should be a #GwyDataView window).
 **/
void
gwy_vector_layer_draw(GwyVectorLayer *layer,
                      GdkDrawable *drawable)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    g_return_if_fail(layer_class->draw);
    layer_class->draw(layer, drawable);
}

/**
 * gwy_vector_layer_button_press:
 * @layer: A vector data view layer.
 * @event: A Gdk mouse button event.
 *
 * Sends a mouse button press event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_vector_layer_button_press(GwyVectorLayer *layer,
                              GdkEventButton *event)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    gwy_debug("");
    g_assert(layer_class);
    if (layer_class->button_press)
        return layer_class->button_press(layer, event);
    return FALSE;
}

/**
 * gwy_vector_layer_button_release:
 * @layer: A vector data view layer.
 * @event: A Gdk mouse button event.
 *
 * Sends a mouse button release event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_vector_layer_button_release(GwyVectorLayer *layer,
                                GdkEventButton *event)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    if (layer_class->button_release)
        return layer_class->button_release(layer, event);
    return FALSE;
}

/**
 * gwy_vector_layer_motion_notify:
 * @layer: A vector data view layer.
 * @event: A Gdk mouse pointer motion notification event.
 *
 * Sends a mouse pointer motion notification event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_vector_layer_motion_notify(GwyVectorLayer *layer,
                               GdkEventMotion *event)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    if (layer_class->motion_notify)
        return layer_class->motion_notify(layer, event);
    return FALSE;
}

/**
 * gwy_vector_layer_key_press:
 * @layer: A vector data view layer.
 * @event: A Gdk key event.
 *
 * Sends a key press event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_vector_layer_key_press(GwyVectorLayer *layer,
                           GdkEventKey *event)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    if (layer_class->key_press)
        return layer_class->key_press(layer, event);
    return FALSE;
}

/**
 * gwy_vector_layer_key_release:
 * @layer: A vector data view layer.
 * @event: A Gdk key event.
 *
 * Sends a key release event to a layer.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_vector_layer_key_release(GwyVectorLayer *layer,
                                GdkEventKey *event)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    if (layer_class->key_release)
        return layer_class->key_release(layer, event);
    return FALSE;
}

/**
 * gwy_vector_layer_get_selection:
 * @layer: A vector data view layer.
 * @selection: An array where the coordinates should be stored in, or %NULL
 *             to get only the number of selected objects. If not %NULL it
 *             must be long enough to hold all the coordinates.
 *
 * Obtains the selection.
 *
 * The selection is a sequence of coordinates whose precise interpretation
 * is layer-dependent.
 *
 * Returns: The number of selected objects. Usually this is NOT
 *          the number of coordinates in @selection (a point takes two
 *          coordinates, a line four)
 **/
gint
gwy_vector_layer_get_selection(GwyVectorLayer *layer,
                               gdouble *selection)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    if (layer_class->get_selection)
        return layer_class->get_selection(layer, selection);
    return FALSE;
}

/**
 * gwy_vector_layer_set_selection:
 * @layer: A vector data view layer.
 * @nselected: The number of objects in @selection.
 * @selection: An array with the selection coordinates. The number of objects
 *             (not coordinates) there is @nselected.
 *
 * Sets the selection.
 *
 * Do not use this function while user is drawing a selection.
 *
 * See gwy_vector_layer_get_selection() for some selection format discussion.
 **/
void
gwy_vector_layer_set_selection(GwyVectorLayer *layer,
                               gint nselected,
                               gdouble *selection)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    g_return_if_fail(!layer->in_selection);

    if (layer_class->set_selection)
        layer_class->set_selection(layer, nselected, selection);
    else
        g_warning("%s doesn't support set_selection()",
                  g_type_name(G_TYPE_FROM_INSTANCE(layer)));
}

/**
 * gwy_vector_layer_unselect:
 * @layer: A vector data view layer.
 *
 * Clears the selection.
 *
 * Do not use this function while user is drawing a selection.
 **/
void
gwy_vector_layer_unselect(GwyVectorLayer *layer)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_assert(layer_class);
    g_return_if_fail(!layer->in_selection);

    if (layer_class->unselect) {
        layer_class->unselect(layer);
        gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
    }
}

/**
 * gwy_vector_layer_selection_finished:
 * @layer: A vector data view layer.
 *
 * Emits a "selection_finished" singal on a layer.
 **/
void
gwy_vector_layer_selection_finished(GwyVectorLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_VECTOR_LAYER(layer));
    g_signal_emit(layer, vector_layer_signals[SELECTION_FINISHED], 0);
}

/**
 * gwy_vector_layer_set_update_policy:
 * @layer: A vector data view layer.
 * @policy: the update policy the vector layer should use.
 *
 * Sets update policy for a vector layer @layer.
 **/
void
gwy_vector_layer_set_update_policy(GwyVectorLayer *layer,
                                   GtkUpdateType policy)
{
    g_return_if_fail(GWY_IS_VECTOR_LAYER(layer));

    layer->update_policy = policy;
}

/**
 * gwy_vector_layer_get_update_policy:
 * @layer: A vector data view layer.
 *
 * Returns the update policy of a vector layer @layer.
 *
 * Returns: The update policy.
 **/
GtkUpdateType
gwy_vector_layer_get_update_policy(GwyVectorLayer *layer)
{
    g_return_val_if_fail(GWY_IS_VECTOR_LAYER(layer), 0);

    return layer->update_policy;
}

/**
 * gwy_vector_layer_updated:
 * @layer: A vector data view layer.
 *
 * Maybe emit the "updated" signal on @layer.
 *
 * Unlike gwy_data_view_layer_updated(), this function honours vector layer
 * update policy, so the "update" signal may not be actually emited right
 * now.
 **/
void
gwy_vector_layer_updated(GwyVectorLayer *layer)
{
    switch (layer->update_policy) {
        case GTK_UPDATE_CONTINUOUS:
        g_signal_emit_by_name(layer, "updated");
        break;

        case GTK_UPDATE_DELAYED:
        if (layer->timer)
            gtk_timeout_remove(layer->timer);
        layer->timer = gtk_timeout_add(GWY_SCROLL_DELAY_LENGTH,
                                       (GtkFunction)gwy_vector_layer_timer,
                                       layer);
        break;

        case GTK_UPDATE_DISCONTINUOUS:
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static gboolean
gwy_vector_layer_timer(GwyVectorLayer *layer)
{
    if (layer->update_policy == GTK_UPDATE_DELAYED)
        g_signal_emit_by_name(layer, "updated");

    layer->timer = 0;
    return FALSE;
}

static void
gwy_vector_layer_real_updated(GwyDataViewLayer *layer)
{
    GwyVectorLayer *vector_layer;

    vector_layer = GWY_VECTOR_LAYER(layer);
    if (vector_layer->timer) {
        gtk_timeout_remove(vector_layer->timer);
        vector_layer->timer = 0;
    }
}

static void
gwy_vector_layer_plugged(GwyDataViewLayer *layer)
{
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
}

static void
gwy_vector_layer_unplugged(GwyDataViewLayer *layer)
{
    GwyVectorLayer *vector_layer;

    gwy_debug("");

    vector_layer = GWY_VECTOR_LAYER(layer);
    gwy_object_unref(vector_layer->gc);
    if (vector_layer->timer) {
        gtk_timeout_remove(vector_layer->timer);
        vector_layer->timer = 0;
    }

    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

/**
 * gwy_vector_layer_setup_gc:
 * @layer: A vector data view layer.
 *
 * Sets up Gdk graphic context of the vector layer for its parent window.
 *
 * This function is intended only for layer implementation.
 **/
void
gwy_vector_layer_setup_gc(GwyVectorLayer *layer)
{
    GtkWidget *parent;
    GdkColor fg, bg;

    g_return_if_fail(GWY_IS_VECTOR_LAYER(layer));
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;
    if (!GTK_WIDGET_REALIZED(parent))
        return;

    if (!layer->gc)
        layer->gc = gdk_gc_new(parent->window);
    gdk_gc_set_function(layer->gc, GDK_INVERT);
    fg.pixel = 0xFFFFFFFF;
    bg.pixel = 0x00000000;
    gdk_gc_set_foreground(layer->gc, &fg);
    gdk_gc_set_background(layer->gc, &bg);
}

/**
 * gwy_vector_layer_cursor_new_or_ref:
 * @cursor: A Gdk cursor, or %NULL.
 * @type: Cursor type to eventually create.
 *
 * Increments reference count of a given Gdk cursor or creates a new one
 * (if @cursor is NULL) of type @cursor_type.
 *
 * This function is intended only for layer implementation.
 **/
void
gwy_vector_layer_cursor_new_or_ref(GdkCursor **cursor,
                                   GdkCursorType type)
{
    g_return_if_fail(cursor);

    if (*cursor)
        gdk_cursor_ref(*cursor);
    else
        *cursor = gdk_cursor_new(type);
}

/**
 * gwy_vector_layer_cursor_free_or_unref:
 * @cursor: A Gdk cursor.
 *
 * Decrements reference count of a Gdk cursor, possibly freeing it.
 *
 * This function is intended only for layer implementation.
 **/
void
gwy_vector_layer_cursor_free_or_unref(GdkCursor **cursor)
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
