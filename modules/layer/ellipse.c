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
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>

#include "layer.h"

#define GWY_TYPE_LAYER_ELLIPSE            (gwy_layer_ellipse_get_type())
#define GWY_LAYER_ELLIPSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_ELLIPSE, GwyLayerEllipse))
#define GWY_IS_LAYER_ELLIPSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_ELLIPSE))
#define GWY_LAYER_ELLIPSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_ELLIPSE, GwyLayerEllipseClass))

#define GWY_TYPE_SELECTION_ELLIPSE            (gwy_selection_ellipse_get_type())
#define GWY_SELECTION_ELLIPSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_ELLIPSE, GwySelectionEllipse))
#define GWY_IS_SELECTION_ELLIPSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_ELLIPSE))
#define GWY_SELECTION_ELLIPSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_ELLIPSE, GwySelectionEllipseClass))

enum {
    OBJECT_SIZE = 4
};

enum {
    PROP_0,
    PROP_DRAW_REFLECTION
};

typedef struct _GwyLayerEllipse          GwyLayerEllipse;
typedef struct _GwyLayerEllipseClass     GwyLayerEllipseClass;
typedef struct _GwySelectionEllipse      GwySelectionEllipse;
typedef struct _GwySelectionEllipseClass GwySelectionEllipseClass;

struct _GwyLayerEllipse {
    GwyVectorLayer parent_instance;

    /* Properties */
    gboolean draw_reflection;

    /* Dynamic state */
    gboolean circle;
};

struct _GwyLayerEllipseClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *corner_cursor[4];
    GdkCursor *resize_cursor;
};

struct _GwySelectionEllipse {
    GwySelection parent_instance;
};

struct _GwySelectionEllipseClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                  (const gchar *name);
static GType    gwy_layer_ellipse_get_type       (void) G_GNUC_CONST;
static GType    gwy_selection_ellipse_get_type   (void) G_GNUC_CONST;
static void     gwy_layer_ellipse_set_property   (GObject *object,
                                                  guint prop_id,
                                                  const GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_layer_ellipse_get_property   (GObject*object,
                                                  guint prop_id,
                                                  GValue *value,
                                                  GParamSpec *pspec);
static void     gwy_layer_ellipse_draw           (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable,
                                                  GwyRenderingTarget target);
static void     gwy_layer_ellipse_draw_object    (GwyVectorLayer *layer,
                                                  GdkDrawable *drawable,
                                                  GwyRenderingTarget target,
                                                  gint i);
static void     gwy_layer_ellipse_draw_ellipse   (GwyVectorLayer *layer,
                                                  GwyDataView *data_view,
                                                  GdkDrawable *drawable,
                                                  GwyRenderingTarget target,
                                                  const gdouble *xy);
static gboolean gwy_layer_ellipse_motion_notify  (GwyVectorLayer *layer,
                                                  GdkEventMotion *event);
static gboolean gwy_layer_ellipse_button_pressed (GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static gboolean gwy_layer_ellipse_button_released(GwyVectorLayer *layer,
                                                  GdkEventButton *event);
static void     gwy_layer_ellipse_set_reflection (GwyLayerEllipse *layer,
                                                  gboolean draw_reflection);
static void     gwy_layer_ellipse_realize        (GwyDataViewLayer *layer);
static void     gwy_layer_ellipse_unrealize      (GwyDataViewLayer *layer);
static gint     gwy_layer_ellipse_near_point     (GwyVectorLayer *layer,
                                                  gdouble xreal,
                                                  gdouble yreal);
static void     gwy_layer_ellipse_squarize       (GwyDataView *data_view,
                                                  gint x,
                                                  gint y,
                                                  gdouble *xy);

/* Allow to express intent. */
#define gwy_layer_ellipse_undraw         gwy_layer_ellipse_draw
#define gwy_layer_ellipse_undraw_object  gwy_layer_ellipse_draw_object

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of elliptic areas."),
    "Yeti <yeti@gwyddion.net>",
    "1.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionEllipse, gwy_selection_ellipse,
              GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerEllipse, gwy_layer_ellipse,
              GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(const gchar *name)
{
    GwyLayerFuncInfo func_info = {
        "ellipse",
        0,
    };

    func_info.type = GWY_TYPE_LAYER_ELLIPSE;
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static void
gwy_selection_ellipse_class_init(GwySelectionEllipseClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_layer_ellipse_class_init(GwyLayerEllipseClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_ellipse_set_property;
    gobject_class->get_property = gwy_layer_ellipse_get_property;

    layer_class->realize = gwy_layer_ellipse_realize;
    layer_class->unrealize = gwy_layer_ellipse_unrealize;

    vector_class->selection_type = GWY_TYPE_SELECTION_ELLIPSE;
    vector_class->draw = gwy_layer_ellipse_draw;
    vector_class->motion_notify = gwy_layer_ellipse_motion_notify;
    vector_class->button_press = gwy_layer_ellipse_button_pressed;
    vector_class->button_release = gwy_layer_ellipse_button_released;

    g_object_class_install_property(
        gobject_class,
        PROP_DRAW_REFLECTION,
        g_param_spec_boolean("draw-reflection",
                             "Draw reflection",
                             "Whether central reflection of selection should "
                             "be drawn too",
                             FALSE,
                             G_PARAM_READWRITE));
}

static void
gwy_selection_ellipse_init(GwySelectionEllipse *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

static void
gwy_layer_ellipse_init(G_GNUC_UNUSED GwyLayerEllipse *layer)
{
}

static void
gwy_layer_ellipse_set_property(GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
    GwyLayerEllipse *layer = GWY_LAYER_ELLIPSE(object);

    switch (prop_id) {
        case PROP_DRAW_REFLECTION:
        gwy_layer_ellipse_set_reflection(layer, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_ellipse_get_property(GObject*object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
    GwyLayerEllipse *layer = GWY_LAYER_ELLIPSE(object);

    switch (prop_id) {
        case PROP_DRAW_REFLECTION:
        g_value_set_boolean(value, layer->draw_reflection);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_ellipse_draw(GwyVectorLayer *layer,
                       GdkDrawable *drawable,
                       GwyRenderingTarget target)
{
    gint i, n;

    g_return_if_fail(GWY_IS_LAYER_ELLIPSE(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    n = gwy_selection_get_data(layer->selection, NULL);
    for (i = 0; i < n; i++)
        gwy_layer_ellipse_draw_object(layer, drawable, target, i);
}

static void
gwy_layer_ellipse_draw_object(GwyVectorLayer *layer,
                              GdkDrawable *drawable,
                              GwyRenderingTarget target,
                              gint i)
{
    GwyDataView *data_view;
    gdouble xy[OBJECT_SIZE];
    gdouble xreal, yreal;
    gboolean has_object;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(data_view);

    has_object = gwy_selection_get_object(layer->selection, i, xy);
    g_return_if_fail(has_object);

    gwy_layer_ellipse_draw_ellipse(layer, data_view, drawable, target, xy);
    if (GWY_LAYER_ELLIPSE(layer)->draw_reflection) {
        gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
        xy[0] = xreal - xy[0];
        xy[1] = yreal - xy[1];
        xy[2] = xreal - xy[2];
        xy[3] = yreal - xy[3];
        gwy_layer_ellipse_draw_ellipse(layer, data_view, drawable, target, xy);
    }
}

static void
gwy_layer_ellipse_draw_ellipse(GwyVectorLayer *layer,
                               GwyDataView *data_view,
                               GdkDrawable *drawable,
                               GwyRenderingTarget target,
                               const gdouble *xy)
{
    gint xmin, ymin, xmax, ymax, width, height;
    gdouble xreal, yreal;

    switch (target) {
        case GWY_RENDERING_TARGET_SCREEN:
        gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xmin, &ymin);
        gwy_data_view_coords_real_to_xy(data_view, xy[2], xy[3], &xmax, &ymax);
        break;

        case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
        gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
        gdk_drawable_get_size(drawable, &width, &height);
        xmin = floor(xy[0]*width/xreal);
        ymin = floor(xy[1]*height/yreal);
        xmax = floor(xy[2]*width/xreal);
        ymax = floor(xy[3]*height/yreal);
        break;

        default:
        g_return_if_reached();
        break;
    }

    if (xmax < xmin)
        GWY_SWAP(gint, xmin, xmax);
    if (ymax < ymin)
        GWY_SWAP(gint, ymin, ymax);

    gdk_draw_arc(drawable, layer->gc, FALSE,
                 xmin, ymin, xmax - xmin, ymax - ymin,
                 0, 64*360);
}

static gboolean
gwy_layer_ellipse_motion_notify(GwyVectorLayer *layer,
                                GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerEllipseClass *klass;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean circle;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    i = layer->selecting;

    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    circle = event->state & GDK_SHIFT_MASK;
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (i > -1)
        gwy_selection_get_object(layer->selection, i, xy);
    if (i > -1 && xreal == xy[2] && yreal == xy[3])
        return FALSE;

    if (!layer->button) {
        i = gwy_layer_ellipse_near_point(layer, xreal, yreal);
        if (i > 0)
            i = i % OBJECT_SIZE;
        klass = GWY_LAYER_ELLIPSE_GET_CLASS(layer);
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->corner_cursor[i]);
        return FALSE;
    }

    g_assert(layer->selecting != -1);
    GWY_LAYER_ELLIPSE(layer)->circle = circle;
    gwy_layer_ellipse_undraw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN, i);
    if (circle)
        gwy_layer_ellipse_squarize(data_view, x, y, xy);
    else {
        xy[2] = xreal;
        xy[3] = yreal;
    }
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_ellipse_draw_object(layer, window,
                                  GWY_RENDERING_TARGET_SCREEN, i);

    return FALSE;
}

static gboolean
gwy_layer_ellipse_button_pressed(GwyVectorLayer *layer,
                                 GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerEllipseClass *klass;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean circle;

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    circle = event->state & GDK_SHIFT_MASK;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);

    /* handle existing selection */
    i = gwy_layer_ellipse_near_point(layer, xreal, yreal);
    if (i >= 0) {
        layer->selecting = i/4;
        gwy_layer_ellipse_undraw_object(layer, window,
                                        GWY_RENDERING_TARGET_SCREEN,
                                        layer->selecting);

        gwy_selection_get_object(layer->selection, layer->selecting, xy);
        if (i/2)
            xy[0] = MIN(xy[0], xy[2]);
        else
            xy[0] = MAX(xy[0], xy[2]);

        if (i%2)
            xy[1] = MIN(xy[1], xy[3]);
        else
            xy[1] = MAX(xy[1], xy[3]);

        if (circle)
            gwy_layer_ellipse_squarize(data_view, x, y, xy);
        else {
            xy[2] = xreal;
            xy[3] = yreal;
        }
        gwy_selection_set_object(layer->selection, layer->selecting, xy);
    }
    else {
        xy[2] = xy[0] = xreal;
        xy[3] = xy[1] = yreal;

        /* add an object, or do nothing when maximum is reached */
        i = -1;
        if (gwy_selection_is_full(layer->selection)) {
            if (gwy_selection_get_max_objects(layer->selection) > 1)
                return FALSE;
            i = 0;
            gwy_layer_ellipse_undraw_object(layer, window,
                                            GWY_RENDERING_TARGET_SCREEN, i);
        }
        layer->selecting = 0;    /* avoid "update" signal emission */
        layer->selecting = gwy_selection_set_object(layer->selection, i, xy);
    }
    GWY_LAYER_ELLIPSE(layer)->circle = circle;
    layer->button = event->button;
    gwy_layer_ellipse_draw_object(layer, window,
                                  GWY_RENDERING_TARGET_SCREEN,
                                  layer->selecting);

    klass = GWY_LAYER_ELLIPSE_GET_CLASS(layer);
    gdk_window_set_cursor(window, klass->resize_cursor);

    return FALSE;
}

static gboolean
gwy_layer_ellipse_button_released(GwyVectorLayer *layer,
                                  GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerEllipseClass *klass;
    gint x, y, xx, yy, i;
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
    gwy_layer_ellipse_undraw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN, i);
    gwy_selection_get_object(layer->selection, i, xy);
    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xx, &yy);
    gwy_debug("event: [%f, %f], xy: [%d, %d]", event->x, event->y, xx, yy);
    if (xx == event->x || yy == event->y)
        gwy_selection_delete_object(layer->selection, i);
    else {
        if (GWY_LAYER_ELLIPSE(layer)->circle)
            gwy_layer_ellipse_squarize(data_view, x, y, xy);
        else {
            xy[2] = xreal;
            xy[3] = yreal;
        }

        if (xy[2] < xy[0])
            GWY_SWAP(gdouble, xy[0], xy[2]);
        if (xy[3] < xy[1])
            GWY_SWAP(gdouble, xy[1], xy[3]);

        gwy_selection_set_object(layer->selection, i, xy);
        gwy_layer_ellipse_draw_object(layer, window,
                                      GWY_RENDERING_TARGET_SCREEN, i);
    }

    layer->selecting = -1;
    klass = GWY_LAYER_ELLIPSE_GET_CLASS(layer);
    i = gwy_layer_ellipse_near_point(layer, xreal, yreal);
    if (i > 0)
        i = i % OBJECT_SIZE;
    outside = outside || (i == -1);
    gdk_window_set_cursor(window, outside ? NULL : klass->corner_cursor[i]);
    gwy_selection_finished(layer->selection);

    return FALSE;
}

static void
gwy_layer_ellipse_set_reflection(GwyLayerEllipse *layer,
                                 gboolean draw_reflection)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_ELLIPSE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (draw_reflection == layer->draw_reflection)
        return;

    if (parent)
        gwy_layer_ellipse_undraw(vector_layer, parent->window,
                                 GWY_RENDERING_TARGET_SCREEN);
    layer->draw_reflection = draw_reflection;
    if (parent)
        gwy_layer_ellipse_draw(vector_layer, parent->window,
                               GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "draw-reflection");
}

static void
gwy_layer_ellipse_realize(GwyDataViewLayer *layer)
{
    GwyLayerEllipseClass *klass;

    gwy_debug("");
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_ellipse_parent_class)->realize(layer);

    klass = GWY_LAYER_ELLIPSE_GET_CLASS(layer);
    gwy_gdk_cursor_new_or_ref(&klass->resize_cursor, GDK_CROSS);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[0], GDK_UL_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[1], GDK_LL_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[2], GDK_UR_ANGLE);
    gwy_gdk_cursor_new_or_ref(&klass->corner_cursor[3], GDK_LR_ANGLE);
}

static void
gwy_layer_ellipse_unrealize(GwyDataViewLayer *layer)
{
    GwyLayerEllipseClass *klass;
    gint i;

    gwy_debug("");

    klass = GWY_LAYER_ELLIPSE_GET_CLASS(layer);
    gwy_gdk_cursor_free_or_unref(&klass->resize_cursor);
    for (i = 0; i < 4; i++)
        gwy_gdk_cursor_free_or_unref(&klass->corner_cursor[i]);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_ellipse_parent_class)->unrealize(layer);
}

#if 0
/* FIXME: This is a fake.  For circle-like ellpises it's good, but for
 * eccentric ellipses it's sometimes too large */
static gdouble
gwy_layer_ellipse_distance(gdouble *coords,
                           gdouble x,
                           gdouble y,
                           gint *quadrant)
{
    gdouble a, b, theta, r;

    a = (coords[2] - coords[0])/2;
    b = (coords[3] - coords[1])/2;
    x -= (coords[0] + coords[2])/2;
    y -= (coords[1] + coords[3])/2;
    theta = atan2(y, x);
    r = a*b/hypot(b*cos(theta), a*sin(theta));

    *quadrant = ((x >= 0) ? 2 : 0) | ((y >= 0) ? 1 : 0);

    return fabs(hypot(x, y) - r);
}

static int
gwy_layer_ellipse_near_point(GwyVectorLayer *layer,
                             gdouble xreal, gdouble yreal)
{
    GwyDataView *view;
    gdouble d, dmin, xy[OBJECT_SIZE];
    gint i, n, imin, quadrant;

    if (!(n = gwy_selection_get_data(layer->selection, NULL)))
        return -1;

    dmin = G_MAXDOUBLE;
    imin = -1;
    for (i = 0; i < n; i++) {
        gwy_selection_get_object(layer->selection, i, xy);
        d = gwy_layer_ellipse_distance(xy, xreal, yreal, &quadrant);
        if (d < dmin) {
            imin = 4*i + quadrant;
            dmin = d;
        }
    }

    view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    /* FIXME: this is simply nonsense when x measure != y measure */
    dmin /= sqrt(gwy_data_view_get_xmeasure(view)
                 *gwy_data_view_get_ymeasure(view));

    if (dmin > PROXIMITY_DISTANCE)
        return -1;
    return imin;
}
#endif

static int
gwy_layer_ellipse_near_point(GwyVectorLayer *layer,
                             gdouble xreal, gdouble yreal)
{
    GwyDataView *view;
    gdouble *coords, d2min, xy[OBJECT_SIZE];
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL)))
        return -1;

    coords = g_newa(gdouble, 8*n);
    for (i = 0; i < n; i++) {
        gwy_selection_get_object(layer->selection, i, xy);
        coords[8*i + 0] = coords[8*i + 2] = xy[0];
        coords[8*i + 1] = coords[8*i + 5] = xy[1];
        coords[8*i + 4] = coords[8*i + 6] = xy[2];
        coords[8*i + 3] = coords[8*i + 7] = xy[3];
    }
    i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 4, coords);

    view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(view)*gwy_data_view_get_ymeasure(view);

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

static void
gwy_layer_ellipse_squarize(GwyDataView *data_view,
                           gint x, gint y,
                           gdouble *xy)
{
    gint size, xb, yb, xx, yy;

    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xb, &yb);
    size = MAX(ABS(x - xb), ABS(y - yb));
    x = xx = (x >= xb) ? xb + size : xb - size;
    y = yy = (y >= yb) ? yb + size : yb - size;
    gwy_data_view_coords_xy_clamp(data_view, &xx, &yy);
    if (xx != x || yy != y) {
        size = MIN(ABS(xx - xb), ABS(yy - yb));
        x = (xx >= xb) ? xb + size : xb - size;
        y = (yy >= yb) ? yb + size : yb - size;
    }
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xy[2], &xy[3]);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
