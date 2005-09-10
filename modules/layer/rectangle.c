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
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>

#include "layer.h"

#define GWY_TYPE_LAYER_RECTANGLE            (gwy_layer_rectangle_get_type())
#define GWY_LAYER_RECTANGLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_RECTANGLE, GwyLayerRectangle))
#define GWY_IS_LAYER_RECTANGLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_RECTANGLE))
#define GWY_LAYER_RECTANGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_RECTANGLE, GwyLayerRectangleClass))

#define GWY_TYPE_SELECTION_RECTANGLE            (gwy_layer_rectangle_get_type())
#define GWY_SELECTION_RECTANGLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_RECTANGLE, GwySelectionRectangle))
#define GWY_IS_SELECTION_RECTANGLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_RECTANGLE))
#define GWY_SELECTION_RECTANGLE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_RECTANGLE, GwySelectionRectangleClass))

enum {
    PROP_0,
    PROP_IS_CROP,
    PROP_LAST
};

typedef struct _GwyLayerRectangle          GwyLayerRectangle;
typedef struct _GwyLayerRectangleClass     GwyLayerRectangleClass;
typedef struct _GwySelectionRectangle      GwySelectionRectangle;
typedef struct _GwySelectionRectangleClass GwySelectionRectangleClass;

struct _GwyLayerRectangle {
    GwyVectorLayer parent_instance;

    gboolean is_crop;
};

struct _GwyLayerRectangleClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *corner_cursor[4];
    GdkCursor *resize_cursor;
};

struct _GwySelectionRectangle {
    GwySelection parent_instance;
};

struct _GwySelectionRectangleClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                    (const gchar *name);
static GType    gwy_layer_rectangle_get_type       (void) G_GNUC_CONST;
static GType    gwy_selection_rectangle_get_type   (void) G_GNUC_CONST;
static void     gwy_layer_rectangle_set_property   (GObject *object,
                                                    guint prop_id,
                                                    const GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_layer_rectangle_get_property   (GObject*object,
                                                    guint prop_id,
                                                    GValue *value,
                                                    GParamSpec *pspec);
static void     gwy_layer_rectangle_draw           (GwyVectorLayer *layer,
                                                    GdkDrawable *drawable);
static gboolean gwy_layer_rectangle_motion_notify  (GwyVectorLayer *layer,
                                                    GdkEventMotion *event);
static gboolean gwy_layer_rectangle_button_pressed (GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static gboolean gwy_layer_rectangle_button_released(GwyVectorLayer *layer,
                                                    GdkEventButton *event);
static void     gwy_layer_rectangle_set_is_crop    (GwyLayerRectangle *layer,
                                                    gboolean is_crop);
static void     gwy_layer_rectangle_plugged        (GwyDataViewLayer *layer);
static void     gwy_layer_rectangle_unplugged      (GwyDataViewLayer *layer);
static gint     gwy_layer_rectangle_near_point     (GwyVectorLayer *layer,
                                                    gdouble xreal,
                                                    gdouble yreal);

/* Allow to express intent. */
#define gwy_layer_rectangle_undraw      gwy_layer_rectangle_draw

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of rectangular areas."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionRectangle, gwy_selection_rectangle,
              GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerRectangle, gwy_layer_rectangle,
              GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(const gchar *name)
{
    static GwyLayerFuncInfo func_info = {
        "rectangle",
        0,
    };

    func_info.type = GWY_TYPE_LAYER_RECTANGLE;
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static void
gwy_layer_rectangle_class_init(GwyLayerRectangleClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_rectangle_set_property;
    gobject_class->get_property = gwy_layer_rectangle_get_property;

    layer_class->plugged = gwy_layer_rectangle_plugged;
    layer_class->unplugged = gwy_layer_rectangle_unplugged;

    vector_class->selection_type = GWY_TYPE_SELECTION_RECTANGLE;
    vector_class->draw = gwy_layer_rectangle_draw;
    vector_class->motion_notify = gwy_layer_rectangle_motion_notify;
    vector_class->button_press = gwy_layer_rectangle_button_pressed;
    vector_class->button_release = gwy_layer_rectangle_button_released;

    g_object_class_install_property(
        gobject_class,
        PROP_IS_CROP,
        g_param_spec_boolean("is-crop",
                             "Crop style",
                             "Whether the selection is crop-style instead of "
                             "plain rectangle",
                             FALSE,
                             G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_selection_rectangle_class_init(GwySelectionRectangleClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = 4;
}

static void
gwy_layer_rectangle_init(G_GNUC_UNUSED GwyLayerRectangle *layer)
{
}

static void
gwy_selection_rectangle_init(G_GNUC_UNUSED GwySelectionRectangle *selection)
{
}

static void
gwy_layer_rectangle_set_property(GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
    GwyLayerRectangle *layer = GWY_LAYER_RECTANGLE(object);

    switch (prop_id) {
        case PROP_IS_CROP:
        gwy_layer_rectangle_set_is_crop(layer, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_rectangle_get_property(GObject*object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
    GwyLayerRectangle *layer = GWY_LAYER_RECTANGLE(object);

    switch (prop_id) {
        case PROP_IS_CROP:
        g_value_set_boolean(value, layer->is_crop);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_rectangle_draw(GwyVectorLayer *layer,
                         GdkDrawable *drawable)
{
    GwyDataView *data_view;
    GwyLayerRectangle *layer_rectangle;
    gint xmin, ymin, xmax, ymax;
    gdouble xy[4];

    g_return_if_fail(GWY_IS_LAYER_RECTANGLE(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    layer_rectangle = GWY_LAYER_RECTANGLE(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(data_view);

    if (!gwy_selection_get_object(layer->selection, 0, xy))
        return;

    gwy_vector_layer_setup_gc(layer);
    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xmin, &ymin);
    gwy_data_view_coords_real_to_xy(data_view, xy[2], xy[3], &xmax, &ymax);
    if (xmax < xmin)
        GWY_SWAP(gint, xmin, xmax);
    if (ymax < ymin)
        GWY_SWAP(gint, ymin, ymax);

    gwy_debug("[%d,%d] to [%d,%d]", xmin, ymin, xmax, ymax);
    if (layer_rectangle->is_crop) {
        gint width, height;

        gdk_drawable_get_size(drawable, &width, &height);
        gdk_draw_line(drawable, layer->gc, 0, ymin, width, ymin);
        if (ymin != ymax)
            gdk_draw_line(drawable, layer->gc, 0, ymax, width, ymax);
        gdk_draw_line(drawable, layer->gc, xmin, 0, xmin, height);
        if (xmin != xmax)
            gdk_draw_line(drawable, layer->gc, xmax, 0, xmax, height);
    }
    else
        gdk_draw_rectangle(drawable, layer->gc, FALSE,
                           xmin, ymin, xmax - xmin, ymax - ymin);
}

static gboolean
gwy_layer_rectangle_motion_notify(GwyVectorLayer *layer,
                                  GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerRectangleClass *klass;
    GwyLayerRectangle *layer_rectangle;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[4];

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;
    layer_rectangle = GWY_LAYER_RECTANGLE(layer);

    if (!gwy_selection_get_object(layer->selection, 0, xy))
        return FALSE;

    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (xreal == xy[2] && yreal == xy[3])
        return FALSE;

    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer_rectangle);
    if (!layer->button) {
        i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->corner_cursor[i]);
        return FALSE;
    }

    gwy_layer_rectangle_undraw(layer, window);
    xy[2] = xreal;
    xy[3] = yreal;
    gwy_selection_set_object(layer->selection, 0, xy);
    gwy_layer_rectangle_draw(layer, window);

    return FALSE;
}

static gboolean
gwy_layer_rectangle_button_pressed(GwyVectorLayer *layer,
                                   GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerRectangleClass *klass;
    GwyLayerRectangle *layer_rectangle;
    gint x, y;
    gdouble xreal, yreal, xy[4];
    gboolean keep_old = FALSE;

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    layer_rectangle = GWY_LAYER_RECTANGLE(layer);
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
    /* handle a previous selection:
     * when we are near a corner, resize the existing one
     * otherwise forget it and start from scratch */
    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer_rectangle);
    if (gwy_selection_get_data(layer->selection, NULL)) {
        gint i;

        gwy_layer_rectangle_undraw(layer, window);
        i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
        if (i >= 0) {
            keep_old = TRUE;
            gwy_selection_get_object(layer->selection, 0, xy);
            if (i/2)
                xy[0] = MIN(xy[0], xy[2]);
            else
                xy[0] = MAX(xy[0], xy[2]);

            if (i%2)
                xy[1] = MIN(xy[1], xy[3]);
            else
                xy[1] = MAX(xy[1], xy[3]);
        }
    }
    layer->button = event->button;
    xy[2] = xreal;
    xy[3] = yreal;
    if (!keep_old) {
        xy[0] = xreal;
        xy[1] = yreal;
    }
    gwy_layer_rectangle_draw(layer, window);
    gdk_window_set_cursor(window, klass->resize_cursor);

    layer->selecting = 0;    /* TODO */
    gwy_selection_set_object(layer->selection, 0, xy);

    return FALSE;
}

static gboolean
gwy_layer_rectangle_button_released(GwyVectorLayer *layer,
                                    GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerRectangleClass *klass;
    GwyLayerRectangle *layer_rectangle;
    gint x, y, i;
    gdouble xreal, yreal, xy[4];

    layer_rectangle = GWY_LAYER_RECTANGLE(layer);
    if (!layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    if (gwy_selection_get_object(layer->selection, 0, xy))
        gwy_layer_rectangle_undraw(layer, window);

    layer->button = 0;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    xy[2] = xreal;
    xy[3] = yreal;
    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &x, &y);
    if (x != event->x || y != event->y)
        gwy_selection_delete_object(layer->selection, 0);
    else {
        if (xy[2] < xy[0])
            GWY_SWAP(gdouble, xy[0], xy[2]);
        if (xy[3] < xy[1])
            GWY_SWAP(gdouble, xy[1], xy[3]);

        gwy_selection_set_object(layer->selection, 0, xy);
        gwy_layer_rectangle_draw(layer, window);
        gwy_selection_finished(layer->selection);
    }
    layer->selecting = -1;

    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer_rectangle);
    i = gwy_layer_rectangle_near_point(layer, xreal, yreal);
    gdk_window_set_cursor(window, i == -1 ? NULL : klass->corner_cursor[i]);

    return FALSE;
}

static void
gwy_layer_rectangle_set_is_crop(GwyLayerRectangle *layer,
                                gboolean is_crop)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_RECTANGLE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (is_crop == layer->is_crop)
        return;

    if (parent)
        gwy_layer_rectangle_undraw(vector_layer, parent->window);
    layer->is_crop = is_crop;
    if (parent)
        gwy_layer_rectangle_draw(vector_layer, parent->window);
    g_object_notify(G_OBJECT(layer), "is-crop");
}

static void
gwy_layer_rectangle_plugged(GwyDataViewLayer *layer)
{
    GwyLayerRectangle *layer_rectangle;
    GwyLayerRectangleClass *klass;

    gwy_debug("");

    layer_rectangle = GWY_LAYER_RECTANGLE(layer);
    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer_rectangle);

    gwy_gdk_cursor_new_or_ref(&klass->resize_cursor, GDK_CROSS);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[0], GDK_UL_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[1], GDK_LL_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[2], GDK_UR_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[3], GDK_LR_ANGLE);

    /* XXX: why this was here? layer_rectangle->selected = FALSE; */
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_rectangle_parent_class)->plugged(layer);
    /* XXX gwy_layer_rectangle_restore(layer_rectangle); */
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_rectangle_unplugged(GwyDataViewLayer *layer)
{
    GwyLayerRectangleClass *klass;
    gint i;

    gwy_debug("");

    klass = GWY_LAYER_RECTANGLE_GET_CLASS(layer);
    gwy_gdk_cursor_free_or_unref(&klass->resize_cursor);
    for (i = 0; i < 4; i++)
        gwy_gdk_cursor_free_or_unref(&klass->corner_cursor[i]);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_rectangle_parent_class)->unplugged(layer);
}

static int
gwy_layer_rectangle_near_point(GwyVectorLayer *layer,
                               gdouble xreal, gdouble yreal)
{
    GwyDataView *view;
    gdouble coords[8], xy[4], d2min;
    gint i;

    if (!gwy_selection_get_object(layer->selection, 0, xy))
        return -1;

    coords[0] = coords[2] = xy[0];
    coords[1] = coords[5] = xy[1];
    coords[4] = coords[6] = xy[2];
    coords[3] = coords[7] = xy[3];
    i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 4, coords);

    view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(view)*gwy_data_view_get_ymeasure(view);

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
