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

#include "config.h"
#include <string.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocesstypes.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>

#include "layer.h"

#define GWY_SELECTION_AXIS_TYPE_NAME "GwySelectionAxis"

#define GWY_TYPE_LAYER_AXIS            (gwy_layer_axis_get_type())
#define GWY_LAYER_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_AXIS, GwyLayerAxis))
#define GWY_IS_LAYER_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_AXIS))
#define GWY_LAYER_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_AXIS, GwyLayerAxisClass))

#define GWY_TYPE_SELECTION_AXIS            (gwy_selection_axis_get_type())
#define GWY_SELECTION_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_AXIS, GwySelectionAxis))
#define GWY_IS_SELECTION_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_AXIS))
#define GWY_SELECTION_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_AXIS, GwySelectionAxisClass))

enum {
    OBJECT_SIZE = 1
};

enum {
    PROP_0,
    PROP_ORIENTATION
};

typedef struct _GwyLayerAxis          GwyLayerAxis;
typedef struct _GwyLayerAxisClass     GwyLayerAxisClass;
typedef struct _GwySelectionAxis      GwySelectionAxis;
typedef struct _GwySelectionAxisClass GwySelectionAxisClass;

struct _GwyLayerAxis {
    GwyVectorLayer parent_instance;
};

struct _GwyLayerAxisClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *near_cursor;
    GdkCursor *move_cursor;
};

struct _GwySelectionAxis {
    GwySelection parent_instance;

    GwyOrientation orientation;
};

struct _GwySelectionAxisClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                   (const gchar *name);
static GType    gwy_layer_axis_get_type           (void) G_GNUC_CONST;
static GType    gwy_selection_axis_get_type       (void) G_GNUC_CONST;
static void   gwy_selection_axis_serializable_init(GwySerializableIface *iface);
static void     gwy_selection_axis_set_property   (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_selection_axis_get_property   (GObject*object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static GByteArray* gwy_selection_axis_serialize   (GObject *serializable,
                                                   GByteArray *buffer);
static GObject* gwy_selection_axis_deserialize    (const guchar *buffer,
                                                   gsize size,
                                                   gsize *position);
static GObject* gwy_selection_axis_duplicate      (GObject *object);
static void     gwy_selection_axis_clone          (GObject *source,
                                                   GObject *copy);
static void     gwy_selection_axis_set_orientation(GwySelectionAxis *selection,
                                                   GwyOrientation orientation);
static void     gwy_layer_axis_draw               (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable);
static void     gwy_layer_axis_draw_object        (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable,
                                                   gint i);
static gboolean gwy_layer_axis_motion_notify      (GwyVectorLayer *layer,
                                                   GdkEventMotion *event);
static gboolean gwy_layer_axis_button_pressed     (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static gboolean gwy_layer_axis_button_released    (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static void     gwy_layer_axis_plugged            (GwyDataViewLayer *layer);
static void     gwy_layer_axis_unplugged          (GwyDataViewLayer *layer);
static gint     gwy_layer_axis_near_point         (GwyVectorLayer *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);

/* Allow to express intent. */
#define gwy_layer_axis_undraw        gwy_layer_axis_draw
#define gwy_layer_axis_undraw_object gwy_layer_axis_draw_object

static GwySerializableIface *gwy_selection_axis_serializable_parent_iface;

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of horizontal or vertical lines."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE_EXTENDED
    (GwySelectionAxis, gwy_selection_axis, GWY_TYPE_SELECTION, 0,
     GWY_IMPLEMENT_SERIALIZABLE(gwy_selection_axis_serializable_init))
G_DEFINE_TYPE(GwyLayerAxis, gwy_layer_axis, GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(const gchar *name)
{
    GwyLayerFuncInfo func_info = {
        "axis",
        0,
    };

    func_info.type = gwy_layer_axis_get_type();
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static void
gwy_selection_axis_class_init(GwySelectionAxisClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    gobject_class->set_property = gwy_selection_axis_set_property;
    gobject_class->get_property = gwy_selection_axis_get_property;

    sel_class->object_size = OBJECT_SIZE;

    g_object_class_install_property(
        gobject_class,
        PROP_ORIENTATION,
        g_param_spec_enum("orientation",
                          "Orientation",
                          "Orientation of selected lines",
                          GWY_TYPE_ORIENTATION,
                          GWY_ORIENTATION_HORIZONTAL,
                          G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_selection_axis_serializable_init(GwySerializableIface *iface)
{
    gwy_selection_axis_serializable_parent_iface
        = g_type_interface_peek_parent(iface);

    iface->serialize = gwy_selection_axis_serialize;
    iface->deserialize = gwy_selection_axis_deserialize;
    iface->duplicate = gwy_selection_axis_duplicate;
    iface->clone = gwy_selection_axis_clone;
}

static void
gwy_layer_axis_class_init(GwyLayerAxisClass *klass)
{
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    layer_class->plugged = gwy_layer_axis_plugged;
    layer_class->unplugged = gwy_layer_axis_unplugged;

    vector_class->selection_type = GWY_TYPE_SELECTION_AXIS;
    vector_class->draw = gwy_layer_axis_draw;
    vector_class->motion_notify = gwy_layer_axis_motion_notify;
    vector_class->button_press = gwy_layer_axis_button_pressed;
    vector_class->button_release = gwy_layer_axis_button_released;
}

static void
gwy_selection_axis_init(GwySelectionAxis *selection)
{
    selection->orientation = GWY_ORIENTATION_HORIZONTAL;
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

static void
gwy_layer_axis_init(G_GNUC_UNUSED GwyLayerAxis *layer)
{
}

static void
gwy_selection_axis_set_property(GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
    GwySelectionAxis *selection = GWY_SELECTION_AXIS(object);

    switch (prop_id) {
        case PROP_ORIENTATION:
        gwy_selection_axis_set_orientation(selection, g_value_get_enum(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_selection_axis_get_property(GObject*object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    GwySelectionAxis *selection = GWY_SELECTION_AXIS(object);

    switch (prop_id) {
        case PROP_ORIENTATION:
        g_value_set_enum(value, selection->orientation);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GByteArray*
gwy_selection_axis_serialize(GObject *serializable,
                             GByteArray *buffer)
{
    GwySelection *selection;

    g_return_val_if_fail(GWY_IS_SELECTION_AXIS(serializable), NULL);

    selection = GWY_SELECTION(serializable);
    {
        guint32 len = selection->n * OBJECT_SIZE;
        guint32 max = selection->objects->len/OBJECT_SIZE;
        guint32 orientation = GWY_SELECTION_AXIS(selection)->orientation;
        gpointer pdata = len ? &selection->objects->data : NULL;
        GwySerializeSpec spec[] = {
            { 'i', "max", &max, NULL, },
            { 'i', "orientation", &orientation, NULL, },
            { 'D', "data", pdata, &len, },
        };

        return gwy_serialize_pack_object_struct(buffer,
                                                GWY_SELECTION_AXIS_TYPE_NAME,
                                                G_N_ELEMENTS(spec), spec);
    }
}

static GObject*
gwy_selection_axis_deserialize(const guchar *buffer,
                               gsize size,
                               gsize *position)
{
    gdouble *data = NULL;
    guint32 len = 0, max = 0, orientation = GWY_ORIENTATION_HORIZONTAL;
    GwySerializeSpec spec[] = {
        { 'i', "max", &max, NULL },
        { 'i', "orientation", &orientation, NULL, },
        { 'D', "data", &data, &len, },
    };
    GwySelection *selection;

    g_return_val_if_fail(buffer, NULL);

    if (!gwy_serialize_unpack_object_struct(buffer, size, position,
                                            GWY_SELECTION_AXIS_TYPE_NAME,
                                            G_N_ELEMENTS(spec), spec)) {
        g_free(data);
        return NULL;
    }

    selection = g_object_new(GWY_TYPE_SELECTION_AXIS, NULL);
    GWY_SELECTION_AXIS(selection)->orientation = orientation;
    g_array_set_size(selection->objects, 0);
    if (data && len) {
        if (len % OBJECT_SIZE)
            g_warning("Selection data size not multiple of object size. "
                      "Ignoring it.");
        else {
            g_array_append_vals(selection->objects, data, len);
            selection->n = len/OBJECT_SIZE;
        }
        g_free(data);
    }
    if (max > selection->n)
        g_array_set_size(selection->objects, max*OBJECT_SIZE);

    return (GObject*)selection;
}

static GObject*
gwy_selection_axis_duplicate(GObject *object)
{
    GObject *copy;

    copy = gwy_selection_axis_serializable_parent_iface->duplicate(object);
    GWY_SELECTION_AXIS(copy)->orientation
        = GWY_SELECTION_AXIS(object)->orientation;

    return copy;
}

static void
gwy_selection_axis_clone(GObject *source,
                         GObject *copy)
{
    gwy_selection_axis_serializable_parent_iface->clone(source, copy);
    GWY_SELECTION_AXIS(copy)->orientation
        = GWY_SELECTION_AXIS(source)->orientation;
}

static void
gwy_selection_axis_set_orientation(GwySelectionAxis *selection,
                                   GwyOrientation orientation)
{
    g_return_if_fail(orientation == GWY_ORIENTATION_HORIZONTAL
                     || orientation == GWY_ORIENTATION_VERTICAL);
    if (orientation == selection->orientation)
        return;

    gwy_selection_clear(GWY_SELECTION(selection));
    selection->orientation = orientation;
    g_object_notify(G_OBJECT(selection), "orientation");
}

static void
gwy_layer_axis_draw(GwyVectorLayer *layer,
                    GdkDrawable *drawable)
{
    gint i, n;

    g_return_if_fail(GWY_IS_LAYER_AXIS(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    n = gwy_selection_get_data(layer->selection, NULL);
    for (i = 0; i < n; i++)
        gwy_layer_axis_draw_object(layer, drawable, i);
}

static void
gwy_layer_axis_draw_object(GwyVectorLayer *layer,
                           GdkDrawable *drawable,
                           gint i)
{
    GwyDataView *data_view;
    gint coord, width, height;
    gdouble xy[OBJECT_SIZE];
    gboolean has_object;

    g_return_if_fail(GWY_IS_LAYER_AXIS(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    has_object = gwy_selection_get_object(layer->selection, i, xy);
    g_return_if_fail(has_object);

    gwy_vector_layer_setup_gc(layer);
    gdk_drawable_get_size(drawable, &width, &height);
    switch (GWY_SELECTION_AXIS(layer->selection)->orientation) {
        case GWY_ORIENTATION_HORIZONTAL:
        gwy_data_view_coords_real_to_xy(data_view, 0.0, xy[0], NULL, &coord);
        gdk_draw_line(drawable, layer->gc, 0, coord, width, coord);
        break;

        case GWY_ORIENTATION_VERTICAL:
        gwy_data_view_coords_real_to_xy(data_view, xy[0], 0.0, &coord, NULL);
        gdk_draw_line(drawable, layer->gc, coord, 0, coord, height);
        break;

        default:
        g_assert_not_reached();
        break;
    }
}

static gboolean
gwy_layer_axis_motion_notify(GwyVectorLayer *layer,
                             GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyOrientation orientation;
    GwyLayerAxisClass *klass;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, rcoord, xy[OBJECT_SIZE];

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    i = layer->selecting;
    if (i > -1)
        gwy_selection_get_object(layer->selection, i, xy);

    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    orientation = GWY_SELECTION_AXIS(layer->selection)->orientation;
    rcoord = (orientation == GWY_ORIENTATION_VERTICAL) ? xreal : yreal;
    if (i > -1 && rcoord == xy[0])
        return FALSE;

    if (!layer->button) {
        i = gwy_layer_axis_near_point(layer, xreal, yreal);
        klass = GWY_LAYER_AXIS_GET_CLASS(layer);
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(layer->selecting != -1);
    gwy_layer_axis_undraw_object(layer, window, i);
    xy[0] = rcoord;
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_axis_draw_object(layer, window, i);

    return FALSE;
}

static gboolean
gwy_layer_axis_button_pressed(GwyVectorLayer *layer,
                              GdkEventButton *event)
{
    GwyDataView *data_view;
    GwyOrientation orientation;
    GwyLayerAxisClass *klass;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    orientation = GWY_SELECTION_AXIS(layer->selection)->orientation;
    xy[0] = (orientation == GWY_ORIENTATION_VERTICAL) ? xreal : yreal;

    /* handle existing selection */
    i = gwy_layer_axis_near_point(layer, xreal, yreal);
    if (i >= 0) {
        layer->selecting = i;
        gwy_layer_axis_undraw_object(layer, window, layer->selecting);
    }
    else {
        /* add an object, or do nothing when maximum is reached */
        i = -1;
        if (gwy_selection_is_full(layer->selection)) {
            if (gwy_selection_get_max_objects(layer->selection) > 1)
                return FALSE;
            i = 0;
            gwy_layer_axis_undraw_object(layer, window, i);
        }
        layer->selecting = 0;    /* avoid "update" signal emission */
        layer->selecting = gwy_selection_set_object(layer->selection, i, xy);
    }
    layer->button = event->button;
    gwy_layer_axis_draw_object(layer, window, layer->selecting);

    klass = GWY_LAYER_AXIS_GET_CLASS(layer);
    gdk_window_set_cursor(window, klass->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_axis_button_released(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyOrientation orientation;
    GwyLayerAxisClass *klass;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean outside;

    if (!layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    layer->button = 0;
    x = event->x;
    y = event->y;
    i = layer->selecting;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    gwy_layer_axis_undraw_object(layer, window, i);
    orientation = GWY_SELECTION_AXIS(layer->selection)->orientation;
    xy[0] = (orientation == GWY_ORIENTATION_VERTICAL) ? xreal : yreal;
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_axis_draw_object(layer, window, i);

    layer->selecting = -1;
    klass = GWY_LAYER_AXIS_GET_CLASS(layer);
    i = gwy_layer_axis_near_point(layer, xreal, yreal);
    outside = outside || (i == -1);
    gdk_window_set_cursor(window, outside ? NULL : klass->near_cursor);
    gwy_selection_finished(layer->selection);

    return FALSE;
}

static void
gwy_layer_axis_plugged(GwyDataViewLayer *layer)
{
    GwyLayerAxisClass *klass;

    gwy_debug("");
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_axis_parent_class)->plugged(layer);

    klass = GWY_LAYER_AXIS_GET_CLASS(layer);
    gwy_gdk_cursor_new_or_ref(&klass->near_cursor, GDK_FLEUR);
    gwy_gdk_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);
}

static void
gwy_layer_axis_unplugged(GwyDataViewLayer *layer)
{
    GwyLayerAxisClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_AXIS_GET_CLASS(layer);
    gwy_gdk_cursor_free_or_unref(&klass->near_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->move_cursor);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_axis_parent_class)->unplugged(layer);
}

static gint
gwy_layer_axis_near_point(GwyVectorLayer *layer,
                          gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    GwyOrientation orientation;
    gdouble dmin, rcoord, d, *xy;
    gint i, m, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL)))
        return -1;

    xy = g_newa(gdouble, n*OBJECT_SIZE);
    gwy_selection_get_data(layer->selection, xy);

    orientation = GWY_SELECTION_AXIS(layer->selection)->orientation;
    rcoord = (orientation == GWY_ORIENTATION_VERTICAL) ? xreal : yreal;
    m = 0;
    dmin = fabs(rcoord - xy[0]);
    for (i = 1; i < n; i++) {
        d = fabs(rcoord - xy[i]);
        if (d < dmin) {
            dmin = d;
            m = i;
        }
    }

    dlayer = (GwyDataViewLayer*)layer;
    dmin /= (orientation == GWY_ORIENTATION_VERTICAL)
            ? gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
            : gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    return (dmin > PROXIMITY_DISTANCE) ? -1 : m;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
