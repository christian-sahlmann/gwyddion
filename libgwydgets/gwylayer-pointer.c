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

#include <string.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libprocess/datafield.h>
#include "gwylayer-pointer.h"
#include "gwydataview.h"

#define GWY_LAYER_POINTER_TYPE_NAME "GwyLayerPointer"

#define PROXIMITY_DISTANCE 8

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_pointer_class_init        (GwyLayerPointerClass *klass);
static void       gwy_layer_pointer_init              (GwyLayerPointer *layer);
static void       gwy_layer_pointer_finalize          (GObject *object);
static void       gwy_layer_pointer_draw              (GwyDataViewLayer *layer,
                                                       GdkDrawable *drawable);
static gboolean   gwy_layer_pointer_motion_notify     (GwyDataViewLayer *layer,
                                                       GdkEventMotion *event);
static gboolean   gwy_layer_pointer_button_pressed    (GwyDataViewLayer *layer,
                                                       GdkEventButton *event);
static gboolean   gwy_layer_pointer_button_released   (GwyDataViewLayer *layer,
                                                       GdkEventButton *event);
static void       gwy_layer_pointer_plugged           (GwyDataViewLayer *layer);
static void       gwy_layer_pointer_unplugged         (GwyDataViewLayer *layer);
static void       gwy_layer_pointer_save              (GwyDataViewLayer *layer);
static void       gwy_layer_pointer_restore           (GwyDataViewLayer *layer);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_pointer_get_type(void)
{
    static GType gwy_layer_pointer_type = 0;

    if (!gwy_layer_pointer_type) {
        static const GTypeInfo gwy_layer_pointer_info = {
            sizeof(GwyLayerPointerClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_pointer_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerPointer),
            0,
            (GInstanceInitFunc)gwy_layer_pointer_init,
            NULL,
        };
        gwy_debug("");
        gwy_layer_pointer_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_POINTER_TYPE_NAME,
                                     &gwy_layer_pointer_info,
                                     0);
    }

    return gwy_layer_pointer_type;
}

static void
gwy_layer_pointer_class_init(GwyLayerPointerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_pointer_finalize;

    layer_class->draw = gwy_layer_pointer_draw;
    layer_class->motion_notify = gwy_layer_pointer_motion_notify;
    layer_class->button_press = gwy_layer_pointer_button_pressed;
    layer_class->button_release = gwy_layer_pointer_button_released;
    layer_class->plugged = gwy_layer_pointer_plugged;
    layer_class->unplugged = gwy_layer_pointer_unplugged;

    klass->point_cursor = NULL;
}

static void
gwy_layer_pointer_init(GwyLayerPointer *layer)
{
    GwyLayerPointerClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_POINTER_GET_CLASS(layer);
    gwy_layer_cursor_new_or_ref(&klass->point_cursor, GDK_CROSS);
    layer->x = 0.0;
    layer->y = 0.0;
    layer->selected = FALSE;
}

static void
gwy_layer_pointer_finalize(GObject *object)
{
    GwyLayerPointerClass *klass;

    gwy_debug("");

    g_return_if_fail(GWY_IS_LAYER_POINTER(object));

    klass = GWY_LAYER_POINTER_GET_CLASS(object);
    gwy_layer_cursor_free_or_unref(&klass->point_cursor);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_pointer_new:
 *
 * Creates a new pointer layer.
 *
 * Container keys: "/0/select/pointer/x", "/0/select/pointer/y".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_pointer_new(void)
{
    GtkObject *object;

    gwy_debug("");
    object = g_object_new(GWY_TYPE_LAYER_POINTER, NULL);

    return object;
}

static void
gwy_layer_pointer_draw(GwyDataViewLayer *layer,
                       GdkDrawable *drawable)
{
    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    return;
}

static gboolean
gwy_layer_pointer_motion_notify(GwyDataViewLayer *layer,
                                GdkEventMotion *event)
{
    GwyLayerPointer *pointer_layer;
    gint x, y;
    gdouble oldx, oldy, xreal, yreal;

    pointer_layer = (GwyLayerPointer*)layer;
    if (!pointer_layer->button)
        return FALSE;
    oldx = pointer_layer->x;
    oldy = pointer_layer->y;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    if (xreal == oldx && yreal == oldy)
        return FALSE;

    pointer_layer->x = xreal;
    pointer_layer->y = yreal;
    gwy_layer_pointer_save(layer);
    gwy_data_view_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_pointer_button_pressed(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyLayerPointerClass *klass;
    GwyLayerPointer *pointer_layer;
    gint x, y;
    gdouble xreal, yreal;

    gwy_debug("");
    pointer_layer = (GwyLayerPointer*)layer;
    if (pointer_layer->button)
        g_warning("unexpected mouse button press when already pressed");

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    pointer_layer->button = event->button;
    pointer_layer->x = xreal;
    pointer_layer->y = yreal;
    pointer_layer->selected = TRUE;
    klass = GWY_LAYER_POINTER_GET_CLASS(layer);
    gdk_window_set_cursor(layer->parent->window, klass->point_cursor);

    return FALSE;
}

static gboolean
gwy_layer_pointer_button_released(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyLayerPointerClass *klass;
    GwyLayerPointer *pointer_layer;
    gint x, y;
    gdouble xreal, yreal;

    pointer_layer = (GwyLayerPointer*)layer;
    if (!pointer_layer->button)
        return FALSE;
    pointer_layer->button = 0;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    pointer_layer->x = xreal;
    pointer_layer->y = yreal;
    pointer_layer->selected = TRUE;
    gwy_layer_pointer_save(layer);
    gwy_data_view_layer_updated(layer);
    gwy_data_view_layer_finished(layer);

    klass = GWY_LAYER_POINTER_GET_CLASS(pointer_layer);
    gdk_window_set_cursor(layer->parent->window, NULL);

    return FALSE;
}

/**
 * gwy_layer_pointer_get_point:
 * @layer: A #GwyLayerPointer.
 * @x: Where the x-coordinate should be stored.
 * @y: Where the y-coordinate should be stored.
 *
 * Obtains the point in real (i.e., physical) coordinates.
 *
 * The @x and @y arguments can be NULL if you are not interested in the
 * particular coordinate.
 *
 * Returns: %TRUE when there is some selection present (and some values were
 *          stored), %FALSE
 **/
gboolean
gwy_layer_pointer_get_point(GwyDataViewLayer *layer,
                            gdouble *x,
                            gdouble *y)
{
    GwyLayerPointer *pointer_layer;

    g_return_val_if_fail(GWY_IS_LAYER_POINTER(layer), FALSE);
    pointer_layer = (GwyLayerPointer*)layer;
    if (!pointer_layer->selected)
        return FALSE;

    if (x)
        *x = pointer_layer->x;
    if (y)
        *y = pointer_layer->y;
    return TRUE;
}

/**
 * gwy_layer_lines_get_nselected:
 * @layer: A #GwyLayerPointer.
 *
 * Returns the number of selected points in @layer.
 *
 * Returns: The number of selected points (i.e., 0 or 1).
 **/
gint
gwy_layer_pointer_get_nselected(GwyDataViewLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_POINTER(layer), 0);
    return GWY_LAYER_POINTER(layer)->selected;
}

/**
 * gwy_layer_pointer_unselect:
 * @layer: A #GwyLayerPointer.
 *
 * Clears the selected point.
 **/
void
gwy_layer_pointer_unselect(GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));

    if (!GWY_LAYER_POINTER(layer)->selected)
        return;

    GWY_LAYER_POINTER(layer)->selected = FALSE;
    gwy_layer_pointer_save(layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_pointer_plugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));

    GWY_LAYER_POINTER(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_pointer_restore(layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_pointer_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_POINTER(layer));

    GWY_LAYER_POINTER(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_pointer_save(GwyDataViewLayer *layer)
{
    GwyLayerPointer *s = GWY_LAYER_POINTER(layer);

    /* TODO Container */
    gwy_container_set_double_by_name(layer->data, "/0/select/pointer/x", s->x);
    gwy_container_set_double_by_name(layer->data, "/0/select/pointer/y", s->y);
}

static void
gwy_layer_pointer_restore(GwyDataViewLayer *layer)
{
    GwyLayerPointer *s = GWY_LAYER_POINTER(layer);
    GwyDataField *dfield;
    gdouble xreal, yreal;

    /* TODO Container */
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(layer->data,
                                                             "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);

    if (gwy_container_contains_by_name(layer->data, "/0/select/pointer/x")) {
        s->x = gwy_container_get_double_by_name(layer->data,
                                                "/0/select/pointer/x");
        s->x = CLAMP(s->x, 0.0, xreal);
    }
    if (gwy_container_contains_by_name(layer->data, "/0/select/pointer/y")) {
        s->y = gwy_container_get_double_by_name(layer->data,
                                                "/0/select/pointer/y");
        s->y = CLAMP(s->y, 0.0, yreal);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
