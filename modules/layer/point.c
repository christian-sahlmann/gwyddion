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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule-layer.h>

#include "layer.h"

#define GWY_TYPE_LAYER_POINT            (gwy_layer_point_get_type())
#define GWY_LAYER_POINT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_POINT, GwyLayerPoint))
#define GWY_IS_LAYER_POINT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_POINT))
#define GWY_LAYER_POINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_POINT, GwyLayerPointClass))

#define GWY_TYPE_SELECTION_POINT            (gwy_selection_point_get_type())
#define GWY_SELECTION_POINT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_POINT, GwySelectionPoint))
#define GWY_IS_SELECTION_POINT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_POINT))
#define GWY_SELECTION_POINT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_POINT, GwySelectionPointClass))

enum {
    OBJECT_SIZE = 2
};

enum {
    PROP_0,
    PROP_DRAW_MARKER,
    PROP_MARKER_RADIUS,
    PROP_DRAW_AS_VECTOR,
};

typedef struct _GwyLayerPoint          GwyLayerPoint;
typedef struct _GwyLayerPointClass     GwyLayerPointClass;
typedef struct _GwySelectionPoint      GwySelectionPoint;
typedef struct _GwySelectionPointClass GwySelectionPointClass;

struct _GwyLayerPoint {
    GwyVectorLayer parent_instance;

    GdkCursor *near_cursor;
    GdkCursor *move_cursor;

    /* Properties */
    gboolean draw_marker;
    guint marker_radius;
    gboolean draw_as_vector;
};

struct _GwyLayerPointClass {
    GwyVectorLayerClass parent_class;
};

struct _GwySelectionPoint {
    GwySelection parent_instance;
};

struct _GwySelectionPointClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                   (void);
static GType    gwy_layer_point_get_type          (void) G_GNUC_CONST;
static GType    gwy_selection_point_get_type      (void) G_GNUC_CONST;
static gboolean gwy_selection_point_crop_object   (GwySelection *selection,
                                                   gint i,
                                                   gpointer user_data);
static void     gwy_selection_point_crop          (GwySelection *selection,
                                                   gdouble xmin,
                                                   gdouble ymin,
                                                   gdouble xmax,
                                                   gdouble ymax);
static void     gwy_layer_point_set_property      (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_layer_point_get_property      (GObject*object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void     gwy_layer_point_draw              (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable,
                                                   GwyRenderingTarget target);
static void     gwy_layer_point_draw_object       (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable,
                                                   GwyRenderingTarget target,
                                                   gint i);
static void     gwy_layer_point_draw_point        (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable,
                                                   GwyDataView *data_view,
                                                   GwyRenderingTarget target,
                                                   const gdouble *xy);
static void     gwy_layer_point_draw_vector       (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable,
                                                   GwyDataView *data_view,
                                                   GwyRenderingTarget target,
                                                   const gdouble *xy);
static gboolean gwy_layer_point_motion_notify     (GwyVectorLayer *layer,
                                                   GdkEventMotion *event);
static gboolean gwy_layer_point_button_pressed    (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static gboolean gwy_layer_point_button_released   (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static void     gwy_layer_point_set_draw_marker   (GwyLayerPoint *layer,
                                                   gboolean draw_marker);
static void     gwy_layer_point_set_marker_radius (GwyLayerPoint *layer,
                                                   guint radius);
static void     gwy_layer_point_set_draw_as_vector(GwyLayerPoint *layer,
                                                   gboolean draw_as_vector);
static void     gwy_layer_point_realize           (GwyDataViewLayer *dlayer);
static void     gwy_layer_point_unrealize         (GwyDataViewLayer *dlayer);
static gint     gwy_layer_point_near_point        (GwyVectorLayer *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);

/* Allow to express intent. */
#define gwy_layer_point_undraw        gwy_layer_point_draw
#define gwy_layer_point_undraw_object gwy_layer_point_draw_object

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of several points, displayed as crosses "
       "or inivisible."),
    "Yeti <yeti@gwyddion.net>",
    "3.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionPoint, gwy_selection_point, GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerPoint, gwy_layer_point, GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(void)
{
    gwy_layer_func_register(GWY_TYPE_LAYER_POINT);
    return TRUE;
}

static void
gwy_selection_point_class_init(GwySelectionPointClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
    sel_class->crop = gwy_selection_point_crop;
}

static void
gwy_layer_point_class_init(GwyLayerPointClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_point_set_property;
    gobject_class->get_property = gwy_layer_point_get_property;

    layer_class->realize = gwy_layer_point_realize;
    layer_class->unrealize = gwy_layer_point_unrealize;

    vector_class->selection_type = GWY_TYPE_SELECTION_POINT;
    vector_class->draw = gwy_layer_point_draw;
    vector_class->motion_notify = gwy_layer_point_motion_notify;
    vector_class->button_press = gwy_layer_point_button_pressed;
    vector_class->button_release = gwy_layer_point_button_released;

    g_object_class_install_property
        (gobject_class,
         PROP_DRAW_MARKER,
         g_param_spec_boolean("draw-marker",
                              "Draw marker",
                              "Whether to draw point marker(s)",
                              TRUE,
                              G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_MARKER_RADIUS,
         g_param_spec_uint("marker-radius",
                           "Marker radius",
                           "Radius of marker to draw",
                           0, 50, 0,
                           G_PARAM_READWRITE));

    g_object_class_install_property
        (gobject_class,
         PROP_DRAW_AS_VECTOR,
         g_param_spec_boolean("draw-as-vector",
                              "Draw as vector",
                              "Whether to draw makers as lines from the origin",
                              FALSE,
                              G_PARAM_READWRITE));
}

static void
gwy_selection_point_init(GwySelectionPoint *selection)
{
    /* Set max. number of objects to one */
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
}

static gboolean
gwy_selection_point_crop_object(GwySelection *selection,
                                gint i,
                                gpointer user_data)
{
    const gdouble *minmax = (const gdouble*)user_data;
    gdouble xy[OBJECT_SIZE];

    gwy_selection_get_object(selection, i, xy);
    return (xy[1] >= minmax[1] && xy[1] <= minmax[3]
            && xy[0] >= minmax[0] && xy[0] <= minmax[2]);
}

static void
gwy_selection_point_crop(GwySelection *selection,
                         gdouble xmin,
                         gdouble ymin,
                         gdouble xmax,
                         gdouble ymax)
{
    gdouble minmax[4] = { xmin, ymin, xmax, ymax };

    gwy_selection_filter(selection, gwy_selection_point_crop_object, minmax);
}

static void
gwy_layer_point_init(GwyLayerPoint *layer)
{
    layer->draw_marker = TRUE;
    layer->marker_radius = 0;
}

static void
gwy_layer_point_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerPoint *layer = GWY_LAYER_POINT(object);

    switch (prop_id) {
        case PROP_DRAW_MARKER:
        gwy_layer_point_set_draw_marker(layer, g_value_get_boolean(value));
        break;

        case PROP_MARKER_RADIUS:
        gwy_layer_point_set_marker_radius(layer, g_value_get_uint(value));
        break;

        case PROP_DRAW_AS_VECTOR:
        gwy_layer_point_set_draw_as_vector(layer, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_point_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerPoint *layer = GWY_LAYER_POINT(object);

    switch (prop_id) {
        case PROP_DRAW_MARKER:
        g_value_set_boolean(value, layer->draw_marker);
        break;

        case PROP_MARKER_RADIUS:
        g_value_set_uint(value, layer->marker_radius);
        break;

        case PROP_DRAW_AS_VECTOR:
        g_value_set_boolean(value, layer->draw_as_vector);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_point_draw(GwyVectorLayer *layer,
                     GdkDrawable *drawable,
                     GwyRenderingTarget target)
{
    gint i, n;

    if (!layer->selection)
        return;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    if (!GWY_LAYER_POINT(layer)->draw_marker)
        return;

    n = gwy_selection_get_data(layer->selection, NULL);
    for (i = 0; i < n; i++)
        gwy_layer_point_draw_object(layer, drawable, target, i);
}

static void
gwy_layer_point_draw_object(GwyVectorLayer *layer,
                            GdkDrawable *drawable,
                            GwyRenderingTarget target,
                            gint i)
{
    GwyDataView *data_view;
    gdouble xy[OBJECT_SIZE];
    gboolean has_object;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(data_view);

    if (!GWY_LAYER_POINT(layer)->draw_marker)
        return;

    gwy_debug("%d", i);
    has_object = gwy_selection_get_object(layer->selection, i, xy);
    g_return_if_fail(has_object);

    if (GWY_LAYER_POINT(layer)->draw_as_vector)
        gwy_layer_point_draw_vector(layer, drawable, data_view, target, xy);
    else
        gwy_layer_point_draw_point(layer, drawable, data_view, target, xy);
}

static void
gwy_layer_point_draw_point(GwyVectorLayer *layer,
                           GdkDrawable *drawable,
                           GwyDataView *data_view,
                           GwyRenderingTarget target,
                           const gdouble *xy)
{
    gint xc, yc, xmin, xmax, ymin, ymax, size, radius;
    gint dwidth, dheight, xsize, ysize, xr, yr;
    gdouble xreal, yreal, xm, ym;

    radius = GWY_LAYER_POINT(layer)->marker_radius;

    gdk_drawable_get_size(drawable, &dwidth, &dheight);
    gwy_data_view_get_pixel_data_sizes(data_view, &xsize, &ysize);
    switch (target) {
        case GWY_RENDERING_TARGET_SCREEN:
        xm = dwidth/(xsize*(gwy_data_view_get_hexcess(data_view) + 1.0));
        ym = dheight/(ysize*(gwy_data_view_get_vexcess(data_view) + 1.0));
        gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xc, &yc);
        xmin = xc - CROSS_SIZE + 1;
        xmax = xc + CROSS_SIZE - 1;
        ymin = yc - CROSS_SIZE + 1;
        ymax = yc + CROSS_SIZE - 1;
        gwy_data_view_coords_xy_clamp(data_view, &xmin, &ymin);
        gwy_data_view_coords_xy_clamp(data_view, &xmax, &ymax);
        break;

        case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
        xm = (gdouble)dwidth/xsize;
        ym = (gdouble)dheight/ysize;
        size = GWY_ROUND(MAX(sqrt(xm*ym)*(CROSS_SIZE - 1), 1.0));
        gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
        xc = floor(xy[0]*dwidth/xreal);
        yc = floor(xy[1]*dheight/yreal);

        xmin = xc - size;
        xmax = xc + size;
        ymin = yc - size;
        ymax = yc + size;
        break;

        default:
        g_return_if_reached();
        break;
    }
    xr = GWY_ROUND(MAX(xm*(radius - 1), 1.0));
    yr = GWY_ROUND(MAX(ym*(radius - 1), 1.0));

    gdk_draw_line(drawable, layer->gc, xmin, yc, xmax, yc);
    gdk_draw_line(drawable, layer->gc, xc, ymin, xc, ymax);

    if (radius > 0)
        gdk_draw_arc(drawable, layer->gc, FALSE,
                     xc - xr, yc - yr, 2*xr, 2*yr, 0, 64*360);
}

static void
gwy_layer_point_draw_vector(GwyVectorLayer *layer,
                            GdkDrawable *drawable,
                            GwyDataView *data_view,
                            GwyRenderingTarget target,
                            const gdouble *xy)
{
    gint xi0, yi0, xi1, yi1, width, height;
    gdouble xreal, yreal, xoff, yoff;

    gwy_data_view_get_real_data_sizes(data_view, &xreal, &yreal);
    gwy_data_view_get_real_data_offsets(data_view, &xoff, &yoff);
    gdk_drawable_get_size(drawable, &width, &height);

    switch (target) {
        case GWY_RENDERING_TARGET_SCREEN:
        gwy_data_view_coords_real_to_xy(data_view, -xoff, -yoff, &xi0, &yi0);
        gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xi1, &yi1);
        gwy_data_view_coords_xy_cut_line(data_view, &xi0, &yi0, &xi1, &yi1);
        break;

        case GWY_RENDERING_TARGET_PIXMAP_IMAGE:
        xi0 = floor(0.0*width/xreal);
        yi0 = floor(0.0*height/yreal);
        xi1 = floor(xy[0]*width/xreal);
        yi1 = floor(xy[1]*height/yreal);
        break;

        default:
        g_return_if_reached();
        break;
    }
    gdk_draw_line(drawable, layer->gc, xi0, yi0, xi1, yi1);
}

static gboolean
gwy_layer_point_motion_notify(GwyVectorLayer *layer,
                              GdkEventMotion *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GdkCursor *cursor;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];

    if (!layer->selection)
        return FALSE;

    /* FIXME: No cursor change hint -- a bit too crude? */
    if (!layer->editable)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    i = layer->selecting;
    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (i > -1)
        gwy_selection_get_object(layer->selection, i, xy);
    if (i > -1 && xreal == xy[0] && yreal == xy[1])
        return FALSE;

    if (!layer->button) {
        if (!GWY_LAYER_POINT(layer)->draw_marker)
            return FALSE;

        i = gwy_layer_point_near_point(layer, xreal, yreal);
        cursor = GWY_LAYER_POINT(layer)->near_cursor;
        gdk_window_set_cursor(window, i == -1 ? NULL : cursor);
        return FALSE;
    }

    g_assert(layer->selecting != -1);
    if (GWY_LAYER_POINT(layer)->draw_as_vector)
        gwy_layer_point_undraw_object(layer, window,
                                      GWY_RENDERING_TARGET_SCREEN, i);
    xy[0] = xreal;
    xy[1] = yreal;
    gwy_selection_set_object(layer->selection, i, xy);
    if (GWY_LAYER_POINT(layer)->draw_as_vector)
        gwy_layer_point_draw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN, i);

    return FALSE;
}

static gboolean
gwy_layer_point_button_pressed(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];

    if (!layer->selection)
        return FALSE;

    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    xy[0] = xreal;
    xy[1] = yreal;

    i = gwy_layer_point_near_point(layer, xreal, yreal);
    /* just emit "object-chosen" when selection is not editable */
    if (!layer->editable) {
        if (i >= 0)
            gwy_vector_layer_object_chosen(layer, i);
        return FALSE;
    }
    /* handle existing selection */
    if (i >= 0) {
        layer->selecting = i;
        gwy_layer_point_undraw_object(layer, window,
                                      GWY_RENDERING_TARGET_SCREEN,
                                      layer->selecting);
    }
    else {
        /* add an object, or do nothing when maximum is reached */
        i = -1;
        if (gwy_selection_is_full(layer->selection)) {
            if (gwy_selection_get_max_objects(layer->selection) > 1)
                return FALSE;
            i = 0;
            gwy_layer_point_undraw_object(layer, window,
                                          GWY_RENDERING_TARGET_SCREEN, i);
        }
        layer->selecting = 0;    /* avoid "update" signal emission */
        layer->selecting = gwy_selection_set_object(layer->selection, i, xy);
    }
    layer->button = event->button;
    if (GWY_LAYER_POINT(layer)->draw_as_vector)
        gwy_layer_point_draw_object(layer, window,
                                    GWY_RENDERING_TARGET_SCREEN,
                                    layer->selecting);

    gdk_window_set_cursor(window, GWY_LAYER_POINT(layer)->move_cursor);
    gwy_vector_layer_object_chosen(layer, layer->selecting);

    return FALSE;
}

static gboolean
gwy_layer_point_button_released(GwyVectorLayer *layer,
                                GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GdkCursor *cursor;
    gint x, y, i;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean outside;

    if (!layer->selection)
        return FALSE;

    if (!layer->button)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_val_if_fail(data_view, FALSE);
    window = GTK_WIDGET(data_view)->window;

    layer->button = 0;
    x = event->x;
    y = event->y;
    i = layer->selecting;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (GWY_LAYER_POINT(layer)->draw_as_vector)
        gwy_layer_point_undraw_object(layer, window,
                                      GWY_RENDERING_TARGET_SCREEN, i);
    xy[0] = xreal;
    xy[1] = yreal;
    gwy_selection_set_object(layer->selection, i, xy);
    gwy_layer_point_draw_object(layer, window, GWY_RENDERING_TARGET_SCREEN, i);

    layer->selecting = -1;
    i = gwy_layer_point_near_point(layer, xreal, yreal);
    outside = outside || (i == -1) || !GWY_LAYER_POINT(layer)->draw_marker;
    cursor = GWY_LAYER_POINT(layer)->near_cursor;
    gdk_window_set_cursor(window, outside ? NULL : cursor);
    gwy_selection_finished(layer->selection);

    return FALSE;
}

static void
gwy_layer_point_set_draw_marker(GwyLayerPoint *layer,
                                gboolean draw_marker)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_POINT(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (draw_marker == layer->draw_marker)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_point_undraw(vector_layer, parent->window,
                               GWY_RENDERING_TARGET_SCREEN);
    layer->draw_marker = draw_marker;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_point_draw(vector_layer, parent->window,
                             GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "draw-marker");
}

static void
gwy_layer_point_set_marker_radius(GwyLayerPoint *layer, guint radius)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_POINT(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (radius == layer->marker_radius)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_point_undraw(vector_layer, parent->window,
                               GWY_RENDERING_TARGET_SCREEN);
    layer->marker_radius = radius;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_point_draw(vector_layer, parent->window,
                             GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "marker-radius");
}

static void
gwy_layer_point_set_draw_as_vector(GwyLayerPoint *layer,
                                   gboolean draw_as_vector)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_POINT(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (draw_as_vector == layer->draw_as_vector)
        return;

    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_point_undraw(vector_layer, parent->window,
                               GWY_RENDERING_TARGET_SCREEN);
    layer->draw_as_vector = draw_as_vector;
    if (parent && GTK_WIDGET_REALIZED(parent))
        gwy_layer_point_draw(vector_layer, parent->window,
                             GWY_RENDERING_TARGET_SCREEN);
    g_object_notify(G_OBJECT(layer), "draw-as-vector");
}

static void
gwy_layer_point_realize(GwyDataViewLayer *dlayer)
{
    GwyLayerPoint *layer;
    GdkDisplay *display;

    gwy_debug("");

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_point_parent_class)->realize(dlayer);
    layer = GWY_LAYER_POINT(dlayer);
    display = gtk_widget_get_display(dlayer->parent);
    layer->near_cursor = gdk_cursor_new_for_display(display, GDK_FLEUR);
    layer->move_cursor = gdk_cursor_new_for_display(display, GDK_CROSS);
}

static void
gwy_layer_point_unrealize(GwyDataViewLayer *dlayer)
{
    GwyLayerPoint *layer;

    gwy_debug("");

    layer = GWY_LAYER_POINT(dlayer);
    gdk_cursor_unref(layer->near_cursor);
    gdk_cursor_unref(layer->move_cursor);

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_point_parent_class)->unrealize(dlayer);
}

static gint
gwy_layer_point_near_point(GwyVectorLayer *layer,
                           gdouble xreal, gdouble yreal)
{
    gdouble d2min, metric[4];
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL))
        || layer->focus >= n)
        return -1;

    gwy_data_view_get_metric(GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent),
                             metric);
    if (layer->focus >= 0) {
        gdouble xy[OBJECT_SIZE];

        gwy_selection_get_object(layer->selection, layer->focus, xy);
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 1, xy, metric);
    }
    else {
        gdouble *xy = g_newa(gdouble, n*OBJECT_SIZE);

        gwy_selection_get_data(layer->selection, xy);
        i = gwy_math_find_nearest_point(xreal, yreal, &d2min, n, xy, metric);
    }
    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
