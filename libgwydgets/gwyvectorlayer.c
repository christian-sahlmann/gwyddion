/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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

/* Note: We do not provide getter for @chosen, because it is not presisent
 * UI-wise (yet?).  Therefore the API should nout encourage treating it as
 * persisent. */

#include "config.h"
#include <string.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwydebugobjects.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwydataview.h>

#define connect_swapped_after(obj, signal, cb, data) \
    g_signal_connect_object(obj, signal, G_CALLBACK(cb), data, \
                            G_CONNECT_SWAPPED | G_CONNECT_AFTER);

enum {
    OBJECT_CHOSEN,
    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_SELECTION_KEY,
    PROP_EDITABLE,
    PROP_FOCUS,
    PROP_LAST
};

static void     gwy_vector_layer_destroy             (GtkObject *object);
static void     gwy_vector_layer_set_property        (GObject *object,
                                                      guint prop_id,
                                                      const GValue *value,
                                                      GParamSpec *pspec);
static void     gwy_vector_layer_get_property        (GObject *object,
                                                      guint prop_id,
                                                      GValue *value,
                                                      GParamSpec *pspec);
static gboolean gwy_vector_layer_set_focus_default   (GwyVectorLayer *layer,
                                                      gint focus);
static void     gwy_vector_layer_plugged             (GwyDataViewLayer *layer);
static void     gwy_vector_layer_unplugged           (GwyDataViewLayer *layer);
static void     gwy_vector_layer_realize             (GwyDataViewLayer *layer);
static void     gwy_vector_layer_unrealize           (GwyDataViewLayer *layer);
static void     gwy_vector_layer_update_context      (GwyVectorLayer *layer);
static void     gwy_vector_layer_container_connect   (GwyVectorLayer *layer,
                                                      const gchar *key);
static void     gwy_vector_layer_selection_connect   (GwyVectorLayer *layer);
static void     gwy_vector_layer_selection_disconnect(GwyVectorLayer *layer);
static void     gwy_vector_layer_item_changed        (GwyVectorLayer *layer);
static void     gwy_vector_layer_selection_changed   (GwyVectorLayer *layer,
                                                      gint hint);

static guint vector_layer_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_ABSTRACT_TYPE(GwyVectorLayer, gwy_vector_layer,
                       GWY_TYPE_DATA_VIEW_LAYER)

static void
gwy_vector_layer_class_init(GwyVectorLayerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gobject_class->set_property = gwy_vector_layer_set_property;
    gobject_class->get_property = gwy_vector_layer_get_property;

    object_class->destroy = gwy_vector_layer_destroy;

    layer_class->plugged = gwy_vector_layer_plugged;
    layer_class->unplugged = gwy_vector_layer_unplugged;
    layer_class->realize = gwy_vector_layer_realize;
    layer_class->unrealize = gwy_vector_layer_unrealize;

    klass->set_focus = gwy_vector_layer_set_focus_default;

    /**
     * GwyVectorLayer::object-chosen:
     * @gwyvectorlayer: The #GwyVectorLayer which received the signal.
     *
     * The ::object-chosen signal is emitted when user starts interacting
     * with a selection object, even before modifying it.  It is emitted
     * even if the layer is not editable, but it is not emitted when focus
     * is set and the user attempts to choose a different object.
     **/
    vector_layer_signals[OBJECT_CHOSEN]
        = g_signal_new("object-chosen",
                       G_OBJECT_CLASS_TYPE(object_class),
                       G_SIGNAL_RUN_FIRST,
                       G_STRUCT_OFFSET(GwyVectorLayerClass, object_chosen),
                       NULL, NULL,
                       g_cclosure_marshal_VOID__INT,
                       G_TYPE_NONE, 1, G_TYPE_INT);

    g_object_class_install_property
        (gobject_class,
         PROP_SELECTION_KEY,
         g_param_spec_string("selection-key",
                             "Selection key",
                             "The key identifying selection object in "
                             "data container",
                             NULL,
                             G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_EDITABLE,
         g_param_spec_boolean("editable",
                              "Editable",
                              "Whether selection objects are editable",
                              TRUE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_FOCUS,
         g_param_spec_int("focus",
                          "Focus",
                          "The id of focused object, or -1 if all objects "
                          "are active",
                          -1, G_MAXINT, -1,
                          G_PARAM_READWRITE));
}

static void
gwy_vector_layer_init(GwyVectorLayer *layer)
{
    gwy_debug_objects_creation(G_OBJECT(layer));

    layer->editable = TRUE;
    layer->selecting = -1;
    layer->focus = -1;
    layer->chosen = -1;
}

static void
gwy_vector_layer_destroy(GtkObject *object)
{
    GwyVectorLayer *layer;

    layer = GWY_VECTOR_LAYER(object);
    gwy_object_unref(layer->gc);
    gwy_object_unref(layer->layout);

    GTK_OBJECT_CLASS(gwy_vector_layer_parent_class)->destroy(object);
}

static void
gwy_vector_layer_set_property(GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    GwyVectorLayer *vector_layer = GWY_VECTOR_LAYER(object);

    switch (prop_id) {
        case PROP_SELECTION_KEY:
        gwy_vector_layer_set_selection_key(vector_layer,
                                           g_value_get_string(value));
        break;

        case PROP_EDITABLE:
        gwy_vector_layer_set_editable(vector_layer, g_value_get_boolean(value));
        break;

        case PROP_FOCUS:
        gwy_vector_layer_set_focus(vector_layer, g_value_get_int(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_vector_layer_get_property(GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
    GwyVectorLayer *vector_layer = GWY_VECTOR_LAYER(object);

    switch (prop_id) {
        case PROP_SELECTION_KEY:
        g_value_set_static_string(value,
                                  g_quark_to_string(vector_layer->selection_key));
        break;

        case PROP_EDITABLE:
        g_value_set_boolean(value, vector_layer->editable);
        break;

        case PROP_FOCUS:
        g_value_set_int(value, vector_layer->focus);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/* Most layers want this unchanged.  They should reset the method to NULL if
 * they don't implement focus though */
static gboolean
gwy_vector_layer_set_focus_default(GwyVectorLayer *layer,
                                   gint focus)
{
    /* Unfocus is always possible */
    if (focus < 0) {
        if (layer->focus >= 0) {
            layer->focus = -1;
            g_object_notify(G_OBJECT(layer), "focus");
        }
        return TRUE;
    }
    /* Setting focus is possible only when user is selecting nothing or the
     * same object */
    if (layer->selecting < 0 || focus == layer->selecting) {
        if (focus != layer->focus) {
            layer->focus = focus;
            g_object_notify(G_OBJECT(layer), "focus");
        }
        return TRUE;
    }
    return FALSE;
}

/**
 * gwy_vector_layer_set_focus:
 * @layer: A vector data view layer.
 * @focus: Index of object to focus on, use -1 to unfocus (allow interaction
 *         with any object).
 *
 * Focues on one selection object.
 *
 * When a selection object is focused, it becomes the only one user can
 * interact with.  More precisely, "object-chosen" signal is emitted only
 * for this object, and if the layer is editable only this object can be
 * modified by the user.
 *
 * Returns: %TRUE if the object was focused, %FALSE on failure.  Failure can
 *          be caused by user currently moving another object, wrong object
 *          index, or the feature being unimplemented in @layer.
 **/
gboolean
gwy_vector_layer_set_focus(GwyVectorLayer *layer,
                           gint focus)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    g_return_val_if_fail(GWY_IS_VECTOR_LAYER(layer), FALSE);
    if (layer_class->set_focus)
        return layer_class->set_focus(layer, focus);
    return FALSE;
}

/**
 * gwy_vector_layer_get_focus:
 * @layer: A vector data view layer.
 *
 * Gets focused object index.
 *
 * Returns: Focued object index, or -1 if no object is focused.
 **/
gint
gwy_vector_layer_get_focus(GwyVectorLayer *layer)
{
    g_return_val_if_fail(GWY_IS_VECTOR_LAYER(layer), -1);
    return layer->focus;
}

/**
 * gwy_vector_layer_draw:
 * @layer: A vector data view layer.
 * @drawable: A drawable to draw on.
 * @target: Rendering target.
 *
 * Draws @layer on given drawable (which should be a #GwyDataView window).
 **/
void
gwy_vector_layer_draw(GwyVectorLayer *layer,
                      GdkDrawable *drawable,
                      GwyRenderingTarget target)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);
    GdkGC *gc = NULL;

    g_assert(layer_class);
    g_return_if_fail(layer_class->draw);

    if (target == GWY_RENDERING_TARGET_PIXMAP_IMAGE) {
        GwyDataView *data_view;
        GdkGCValues gcvalues;
        gint xres, yres, w, h;
        gdouble zoom;

        data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
        g_return_if_fail(data_view);

        gwy_data_view_get_pixel_data_sizes(data_view, &xres, &yres);
        gdk_drawable_get_size(drawable, &w, &h);
        zoom = sqrt(((gdouble)(w*h))/(xres*yres));

        gc = gdk_gc_new(drawable);
        gdk_gc_get_values(gc, &gcvalues);
        gcvalues.line_width = ROUND(MAX(zoom, 1.0));
        gcvalues.function = GDK_SET;
        gdk_gc_set_values(gc, &gcvalues, GDK_GC_LINE_WIDTH | GDK_GC_FUNCTION);

        GWY_SWAP(GdkGC*, layer->gc, gc);
    }

    layer_class->draw(layer, drawable, target);

    if (target == GWY_RENDERING_TARGET_PIXMAP_IMAGE) {
        GWY_SWAP(GdkGC*, layer->gc, gc);
        g_object_unref(gc);
    }
}

/**
 * gwy_vector_layer_button_press:
 * @layer: A vector data view layer.
 * @event: A Gdk mouse button event.
 *
 * Sends a mouse button press event to a layer.
 *
 * This method primarily exists for #GwyDataView to forward events to
 * layers.  You should rarely need it.
 *
 * Returns: %TRUE if the event was handled.  In practice, it returns %FALSE.
 **/
gboolean
gwy_vector_layer_button_press(GwyVectorLayer *layer,
                              GdkEventButton *event)
{
    GwyVectorLayerClass *layer_class = GWY_VECTOR_LAYER_GET_CLASS(layer);

    gwy_debug(" ");
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
 * This method primarily exists for #GwyDataView to forward events to
 * layers.  You should rarely need it.
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
 * @event: A Gdk mouse pointer motion notification event.  It can be a hint.
 *
 * Sends a mouse pointer motion notification event to a layer.
 *
 * This method primarily exists for #GwyDataView to forward events to
 * layers.  You should rarely need it.
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
 * This method primarily exists for #GwyDataView to forward events to
 * layers.  You should rarely need it.
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
 * This method primarily exists for #GwyDataView to forward events to
 * layers.  You should rarely need it.
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

static void
gwy_vector_layer_update_context(GwyVectorLayer *layer)
{
    gwy_debug(" ");

    if (layer->layout)
        pango_layout_context_changed(layer->layout);
}

/**
 * gwy_vector_layer_set_selection_key:
 * @layer: A vector layer.
 * @key: Container string key identifying the selection object.
 *
 * Sets the selection object to use by a vector layer.
 **/
void
gwy_vector_layer_set_selection_key(GwyVectorLayer *layer,
                                   const gchar *key)
{
    GwyDataViewLayer *view_layer;
    GQuark quark;

    g_return_if_fail(GWY_IS_VECTOR_LAYER(layer));
    quark = key ? g_quark_from_string(key) : 0;
    if (layer->selection_key == quark)
        return;
    view_layer = GWY_DATA_VIEW_LAYER(layer);
    if (!view_layer->data) {
        layer->selection_key = quark;
        g_object_notify(G_OBJECT(layer), "selection-key");
        return;
    }
    gwy_signal_handler_disconnect(view_layer->data, layer->item_changed_id);
    gwy_vector_layer_selection_disconnect(layer);
    layer->selection_key = quark;
    gwy_vector_layer_selection_connect(layer);
    gwy_vector_layer_container_connect(layer, key);

    /* XXX: copied from pixmap layer */
    g_object_notify(G_OBJECT(layer), "selection-key");
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
}

static void
gwy_vector_layer_container_connect(GwyVectorLayer *layer,
                                   const gchar *key)
{
    GwyDataViewLayer *view_layer;
    gchar *detailed_signal;

    g_return_if_fail(key);
    view_layer = GWY_DATA_VIEW_LAYER(layer);
    detailed_signal = g_newa(gchar, sizeof("item-changed::") + strlen(key));
    g_stpcpy(g_stpcpy(detailed_signal, "item-changed::"), key);

    layer->item_changed_id
        = connect_swapped_after(view_layer->data, detailed_signal,
                                gwy_vector_layer_item_changed, view_layer);
}

/**
 * gwy_vector_layer_selection_connect:
 * @layer: A vector layer.
 *
 * Eventually connects to new selection's "changed" signal.
 **/
static void
gwy_vector_layer_selection_connect(GwyVectorLayer *layer)
{
    GwyDataViewLayer *view_layer;

    g_return_if_fail(!layer->selection);
    if (!layer->selection_key)
        return;

    view_layer = GWY_DATA_VIEW_LAYER(layer);
    if (!gwy_container_gis_object(view_layer->data, layer->selection_key,
                                  &layer->selection)) {
        GwyVectorLayerClass *klass = GWY_VECTOR_LAYER_GET_CLASS(layer);

        layer->selection = g_object_new(klass->selection_type, NULL);
        gwy_container_set_object(view_layer->data, layer->selection_key,
                                 layer->selection);

        /* Terminate possible recusive invocation caused by container
         * "item-changed" callback. */
        if (layer->selection_changed_id)
            return;
    }
    else
        g_object_ref(layer->selection);

    layer->selecting = -1;
    layer->selection_changed_id
        = g_signal_connect_swapped(layer->selection,
                                   "changed",
                                   G_CALLBACK(gwy_vector_layer_selection_changed),
                                   layer);
}

/**
 * gwy_vector_layer_selection_disconnect:
 * @layer: A vector layer.
 *
 * Disconnects from all data field's signals and drops reference to it.
 **/
static void
gwy_vector_layer_selection_disconnect(GwyVectorLayer *layer)
{
    if (!layer->selection)
        return;

    gwy_signal_handler_disconnect(layer->selection,
                                  layer->selection_changed_id);
    layer->selecting = -1;
    gwy_object_unref(layer->selection);
}

/**
 * gwy_vector_layer_get_selection_key:
 * @layer: A vector layer.
 *
 * Gets the key identifying selection this vector layer displays.
 *
 * Returns: The string key, or %NULL if it isn't set.
 **/
const gchar*
gwy_vector_layer_get_selection_key(GwyVectorLayer *layer)
{
    g_return_val_if_fail(GWY_IS_VECTOR_LAYER(layer), NULL);
    return g_quark_to_string(layer->selection_key);
}

static void
gwy_vector_layer_plugged(GwyDataViewLayer *layer)
{
    GwyVectorLayer *vector_layer;
    const gchar *key;

    g_signal_connect_swapped(layer->parent, "style-set",
                             G_CALLBACK(gwy_vector_layer_update_context),
                             layer);
    g_signal_connect_swapped(layer->parent, "direction-changed",
                             G_CALLBACK(gwy_vector_layer_update_context),
                             layer);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_vector_layer_parent_class)->plugged(layer);
    vector_layer = GWY_VECTOR_LAYER(layer);
    if (!vector_layer->selection_key)
        return;

    key = g_quark_to_string(vector_layer->selection_key);
    gwy_vector_layer_container_connect(vector_layer, key);
    gwy_vector_layer_selection_connect(vector_layer);
}

static void
gwy_vector_layer_unplugged(GwyDataViewLayer *layer)
{
    GwyVectorLayer *vector_layer;

    vector_layer = GWY_VECTOR_LAYER(layer);

    g_signal_handlers_disconnect_matched(layer->parent,
                                         G_SIGNAL_MATCH_FUNC
                                            | G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL,
                                         gwy_vector_layer_update_context,
                                         layer);

    gwy_vector_layer_selection_disconnect(vector_layer);
    gwy_signal_handler_disconnect(layer->data, vector_layer->item_changed_id);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_vector_layer_parent_class)->unplugged(layer);
}

static void
gwy_vector_layer_realize(GwyDataViewLayer *layer)
{
    void (*method)(GwyDataViewLayer*);
    GwyVectorLayer *vector_layer;
    GdkColor color;

    method = GWY_DATA_VIEW_LAYER_CLASS(gwy_vector_layer_parent_class)->realize;
    if (method)
        method(layer);

    g_return_if_fail(layer->parent && GTK_WIDGET_REALIZED(layer->parent));
    vector_layer = GWY_VECTOR_LAYER(layer);

    vector_layer->gc = gdk_gc_new(layer->parent->window);
    gdk_gc_set_function(vector_layer->gc, GDK_XOR);

    color.red = color.green = color.blue = 0xffff;
    gdk_gc_set_rgb_fg_color(vector_layer->gc, &color);

    color.red = color.green = color.blue = 0x0000;
    gdk_gc_set_rgb_bg_color(vector_layer->gc, &color);
}

static void
gwy_vector_layer_unrealize(GwyDataViewLayer *layer)
{
    GwyVectorLayer *vector_layer;
    void (*method)(GwyDataViewLayer*);

    vector_layer = GWY_VECTOR_LAYER(layer);

    gwy_object_unref(vector_layer->gc);
    gwy_object_unref(vector_layer->layout);

    method
        = GWY_DATA_VIEW_LAYER_CLASS(gwy_vector_layer_parent_class)->unrealize;
    if (method)
        method(layer);
}

/**
 * gwy_vector_layer_item_changed:
 * @layer: A vector data view layer.
 * @data: Container with the data field this vector layer display.
 *
 * Reconnects signals to a new data field when it was replaced in the
 * container.
 **/
static void
gwy_vector_layer_item_changed(GwyVectorLayer *vector_layer)
{
    gwy_vector_layer_selection_disconnect(vector_layer);
    gwy_vector_layer_selection_connect(vector_layer);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(vector_layer));
}

static void
gwy_vector_layer_selection_changed(GwyVectorLayer *layer,
                                   G_GNUC_UNUSED gint hint)
{
    gwy_debug("selecting: %d", layer->selecting);
    if (layer->selecting >= 0)
        return;
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
}

/**
 * gwy_vector_layer_get_editable:
 * @layer: A vector data view layer.
 *
 * Gets editability of a vector layer.
 *
 * Returns: %TRUE if layer is edtiable, %FALSE if it is not editable.
 **/
gboolean
gwy_vector_layer_get_editable(GwyVectorLayer *layer)
{
    g_return_val_if_fail(GWY_IS_VECTOR_LAYER(layer), FALSE);
    return layer->editable;
}

/**
 * gwy_vector_layer_set_editable:
 * @layer: A vector data view layer.
 * @editable: %TRUE to set layer editable, %FALSE to set it noneditable.
 *
 * Sets a vector layer editability.
 *
 * It is an error to attempt to set a layer non-editabile while it is being
 * edited.
 *
 * When a layer is set noneditable, the user cannot change the selection.
 * However, "object-chosen" signal is still emitted.
 **/
void
gwy_vector_layer_set_editable(GwyVectorLayer *layer,
                              gboolean editable)
{
    g_return_if_fail(GWY_IS_VECTOR_LAYER(layer));

    if (editable == layer->editable)
        return;

    if (layer->selecting >= 0) {
        g_warning("Setting layer noneditable when it's being edited does not "
                  "work");
        return;
    }

    layer->editable = editable;
    /* Reset cursor for the case it's currently non-default.
     * FIXME: How to properly handle the reverse? Although it is less serious
     * as it gets fixed automatically once user moves the cursor -- unlike
     * this case. */
    if (!layer->editable) {
        GwyDataViewLayer *dlayer;

        dlayer = GWY_DATA_VIEW_LAYER(layer);
        if (dlayer->parent && GTK_WIDGET_REALIZED(dlayer->parent))
            gdk_window_set_cursor(dlayer->parent->window, NULL);
    }
    g_object_notify(G_OBJECT(layer), "editable");
}

/**
 * gwy_vector_layer_object_chosen:
 * @layer: A vector data view layer.
 * @id: Index of the chosen object.
 *
 * Emits "object-chosen" signal on a vector layer.
 *
 * This function is primarily intended for layer implementations.
 **/
void
gwy_vector_layer_object_chosen(GwyVectorLayer *layer,
                               gint id)
{
    g_return_if_fail(GWY_IS_VECTOR_LAYER(layer));

    layer->chosen = id;
    g_signal_emit(layer, vector_layer_signals[OBJECT_CHOSEN], 0, id);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyvectorlayer
 * @title: GwyVectorLayer
 * @short_description: Base class for #GwyDataView vector (interactive) layers
 * @see_also: <link linkend="standard-vector-layers">Standard vector
 *            layers</link>
 *
 * #GwyVectorLayer is a base class for #GwyDataViewLayer<!-- -->s displaying
 * selections and handling user input.  It is a #GwyDataView component and it
 * is not normally usable outside of it.
 *
 * The layer takes the selection to display from its parent #GwyDataView's
 * container. The key under which the selection is found must be set with
 * gwy_vector_layer_set_selection_key().
 *
 * #GwyVectorLayer provides two independent means to restrict user interaction
 * with displayed selection: editability and focus.  When a layer is set
 * noneditable (see gwy_vector_layer_set_editable()), the user cannot modify
 * displayed objects, however "object-chosen" is emitted normally.  Focus
 * (see gwy_vector_layer_set_focus()) on the other hand limits all possible
 * interactions to a single selection object.  It is possible although not
 * very useful to combine both restrictions.
 *
 * The other methods are rarely useful outside #GwyDataView and/or layer
 * implementation.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
