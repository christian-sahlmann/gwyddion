/* @(#) $Id$ */

#include <string.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwylayer-points.h"
#include "gwydataview.h"

#define GWY_LAYER_POINTS_TYPE_NAME "GwyLayerPoints"

#define PROXIMITY_DISTANCE 8
#define CROSS_SIZE 8

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_points_class_init      (GwyLayerPointsClass *klass);
static void       gwy_layer_points_init            (GwyLayerPoints *layer);
static void       gwy_layer_points_finalize        (GObject *object);
static void       gwy_layer_points_draw            (GwyDataViewLayer *layer,
                                                    GdkDrawable *drawable);
static void       gwy_layer_points_draw_point      (GwyDataViewLayer *layer,
                                                    GdkDrawable *drawable,
                                                    gint i);
static gboolean   gwy_layer_points_motion_notify   (GwyDataViewLayer *layer,
                                                    GdkEventMotion *event);
static gboolean   gwy_layer_points_button_pressed  (GwyDataViewLayer *layer,
                                                    GdkEventButton *event);
static gboolean   gwy_layer_points_button_released (GwyDataViewLayer *layer,
                                                    GdkEventButton *event);
static void       gwy_layer_points_plugged         (GwyDataViewLayer *layer);
static void       gwy_layer_points_unplugged       (GwyDataViewLayer *layer);
static void       gwy_layer_points_save            (GwyDataViewLayer *layer,
                                                    gint i);
static void       gwy_layer_points_restore         (GwyDataViewLayer *layer);

static int        gwy_layer_points_near_point      (GwyLayerPoints *layer,
                                                    gdouble xreal,
                                                    gdouble yreal);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_points_get_type(void)
{
    static GType gwy_layer_points_type = 0;

    if (!gwy_layer_points_type) {
        static const GTypeInfo gwy_layer_points_info = {
            sizeof(GwyLayerPointsClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_points_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerPoints),
            0,
            (GInstanceInitFunc)gwy_layer_points_init,
            NULL,
        };
        gwy_debug("%s", __FUNCTION__);
        gwy_layer_points_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_POINTS_TYPE_NAME,
                                     &gwy_layer_points_info,
                                     0);
    }

    return gwy_layer_points_type;
}

static void
gwy_layer_points_class_init(GwyLayerPointsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_points_finalize;

    layer_class->draw = gwy_layer_points_draw;
    layer_class->motion_notify = gwy_layer_points_motion_notify;
    layer_class->button_press = gwy_layer_points_button_pressed;
    layer_class->button_release = gwy_layer_points_button_released;
    layer_class->plugged = gwy_layer_points_plugged;
    layer_class->unplugged = gwy_layer_points_unplugged;

    klass->near_cursor = NULL;
    klass->move_cursor = NULL;
}

static void
gwy_layer_points_init(GwyLayerPoints *layer)
{
    GwyLayerPointsClass *klass;

    gwy_debug("%s", __FUNCTION__);

    klass = GWY_LAYER_POINTS_GET_CLASS(layer);
    gwy_layer_cursor_new_or_ref(&klass->near_cursor, GDK_FLEUR);
    gwy_layer_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);

    layer->npoints = 3;
    layer->nselected = 0;
    layer->near = -1;
    layer->points = g_new(gdouble, 2*layer->npoints);
}

static void
gwy_layer_points_finalize(GObject *object)
{
    GwyLayerPointsClass *klass;
    GwyLayerPoints *layer;

    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_LAYER_POINTS(object));

    layer = (GwyLayerPoints*)object;
    klass = GWY_LAYER_POINTS_GET_CLASS(object);
    gwy_layer_cursor_free_or_unref(&klass->near_cursor);
    gwy_layer_cursor_free_or_unref(&klass->move_cursor);

    g_free(layer->points);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_points_new:
 *
 * Creates a new point selection layer.
 *
 * The default number of points to select is three.
 *
 * Container keys: "/0/points/0/x", "/0/points/0/y", "/0/points/1/x",
 * "/0/points/1/y", etc., and "/0/points/nselected".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_points_new(void)
{
    GtkObject *object;

    gwy_debug("%s", __FUNCTION__);
    object = g_object_new(GWY_TYPE_LAYER_POINTS, NULL);

    return object;
}

/**
 * gwy_layer_points_set_max_points:
 * @layer: A #GwyLayerPoints.
 * @npoints: The number of points to select.
 *
 * Sets the number of points to @npoints.
 *
 * This is the maximum number of points user can select and also the number of
 * points to be selected to emit the "finished" signal.
 **/
void
gwy_layer_points_set_max_points(GwyDataViewLayer *layer,
                                gint npoints)
{
    GwyLayerPoints *points_layer;

    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    g_return_if_fail(npoints > 0 && npoints < 1024);

    points_layer = (GwyLayerPoints*)layer;
    points_layer->npoints = npoints;
    points_layer->nselected = MIN(points_layer->nselected, npoints);
    if (points_layer->near >= npoints)
        points_layer->near = -1;
    points_layer->points = g_renew(gdouble, points_layer->points,
                                   2*points_layer->npoints);
}

/**
 * gwy_layer_points_get_max_points:
 * @layer: A #GwyLayerPoints.
 *
 * Returns the number of selection points for this layer.
 *
 * Returns: The number of points to select.
 **/
gint
gwy_layer_points_get_max_points(GwyDataViewLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_POINTS(layer), 0);

    return GWY_LAYER_POINTS(layer)->npoints;
}

static void
gwy_layer_points_setup_gc(GwyDataViewLayer *layer)
{
    GdkColor fg, bg;

    if (!GTK_WIDGET_REALIZED(layer->parent))
        return;

    layer->gc = gdk_gc_new(layer->parent->window);
    gdk_gc_set_function(layer->gc, GDK_INVERT);
    fg.pixel = 0xFFFFFFFF;
    bg.pixel = 0x00000000;
    gdk_gc_set_foreground(layer->gc, &fg);
    gdk_gc_set_background(layer->gc, &bg);
}

static void
gwy_layer_points_draw(GwyDataViewLayer *layer,
                      GdkDrawable *drawable)
{
    GwyLayerPoints *points_layer;
    gint i;

    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    points_layer = (GwyLayerPoints*)layer;
    for (i = 0; i < points_layer->nselected; i++)
        gwy_layer_points_draw_point(layer, drawable, i);
}

static void
gwy_layer_points_draw_point(GwyDataViewLayer *layer,
                            GdkDrawable *drawable,
                            gint i)
{
    GwyLayerPoints *points_layer;
    gint xc, yc, xmin, xmax, ymin, ymax;

    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    points_layer = (GwyLayerPoints*)layer;
    g_return_if_fail(i >= 0 && i < points_layer->nselected);

    if (!layer->gc)
        gwy_layer_points_setup_gc(layer);

    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    points_layer->points[2*i],
                                    points_layer->points[2*i + 1],
                                    &xc, &yc);
    xmin = xc - CROSS_SIZE + 1;
    xmax = xc + CROSS_SIZE - 1;
    ymin = yc - CROSS_SIZE + 1;
    ymax = yc + CROSS_SIZE - 1;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &xmin, &ymin);
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &xmax, &ymax);
    gdk_draw_line(drawable, layer->gc, xmin, yc, xmax, yc);
    gdk_draw_line(drawable, layer->gc, xc, ymin, xc, ymax);
}

static gboolean
gwy_layer_points_motion_notify(GwyDataViewLayer *layer,
                               GdkEventMotion *event)
{
    GwyLayerPointsClass *klass;
    GwyLayerPoints *points_layer;
    gint x, y, i;
    gdouble xreal, yreal;

    points_layer = (GwyLayerPoints*)layer;
    i = points_layer->near;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    if (i > -1
        && xreal == points_layer->points[2*i]
        && yreal == points_layer->points[2*i + 1])
        return FALSE;

    if (!points_layer->button) {
        i = gwy_layer_points_near_point(points_layer, xreal, yreal);
        klass = GWY_LAYER_POINTS_GET_CLASS(points_layer);
        gdk_window_set_cursor(layer->parent->window,
                              i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(points_layer->near != -1);
    /*gwy_layer_points_draw_point(layer, layer->parent->window, i);*/
    points_layer->points[2*i] = xreal;
    points_layer->points[2*i + 1] = yreal;
    gwy_layer_points_save(layer, i);

    /*gwy_layer_points_draw_point(layer, layer->parent->window, i);*/
    gwy_data_view_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_points_button_pressed(GwyDataViewLayer *layer,
                                GdkEventButton *event)
{
    GwyLayerPoints *points_layer;
    gint x, y, i;
    gdouble xreal, yreal;

    gwy_debug("%s", __FUNCTION__);
    points_layer = (GwyLayerPoints*)layer;
    if (points_layer->button)
        g_warning("unexpected mouse button press when already pressed");

    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_debug("%s [%d,%d]", __FUNCTION__, x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    /* handle existing points */
    i = gwy_layer_points_near_point(points_layer, xreal, yreal);
    if (i >= 0) {
        points_layer->near = i;
        gwy_layer_points_draw_point(layer, layer->parent->window, i);
    }
    else {
        /* add a point, or do nothing when maximum is reached */
        if (points_layer->nselected == points_layer->npoints)
            return FALSE;
        i = points_layer->near = points_layer->nselected;
        points_layer->nselected++;
    }
    points_layer->button = event->button;
    points_layer->points[2*i] = xreal;
    points_layer->points[2*i + 1] = yreal;
    /*gwy_layer_points_draw_point(layer, layer->parent->window, i);*/

    gdk_window_set_cursor(layer->parent->window,
                          GWY_LAYER_POINTS_GET_CLASS(layer)->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_points_button_released(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyLayerPointsClass *klass;
    GwyLayerPoints *points_layer;
    gint x, y, i;
    gdouble xreal, yreal;
    gboolean outside;

    points_layer = (GwyLayerPoints*)layer;
    if (!points_layer->button)
        return FALSE;
    points_layer->button = 0;
    x = event->x;
    y = event->y;
    i = points_layer->near;
    gwy_debug("%s: i = %d", __FUNCTION__, i);
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    points_layer->points[2*i] = xreal;
    points_layer->points[2*i + 1] = yreal;
    gwy_layer_points_save(layer, i);
    gwy_layer_points_draw_point(layer, layer->parent->window, i);
    gwy_data_view_layer_updated(layer);
    if (points_layer->nselected == points_layer->npoints)
        gwy_data_view_layer_finished(layer);

    klass = GWY_LAYER_POINTS_GET_CLASS(points_layer);
    i = gwy_layer_points_near_point(points_layer, xreal, yreal);
    gdk_window_set_cursor(layer->parent->window,
                          (i == -1 || outside) ? NULL : klass->near_cursor);

    return FALSE;
}

/**
 * gwy_layer_points_get_points:
 * @layer: A #GwyLayerPoints.
 * @points: Where the point coordinates should be stored in.
 *
 * Obtains the selected points.
 *
 * The @points array should be twice the size of
 * gwy_layer_points_get_max_points(), points are stored as x, y.
 * If less than gwy_layer_points_get_max_points() points are actually selected
 * the remaining items will not have meaningful values.
 *
 * Returns: The number of actually selected points.
 **/
gint
gwy_layer_points_get_points(GwyDataViewLayer *layer,
                            gdouble *points)
{
    GwyLayerPoints *points_layer;

    g_return_val_if_fail(GWY_IS_LAYER_POINTS(layer), 0);
    g_return_val_if_fail(points, 0);

    points_layer = (GwyLayerPoints*)layer;
    if (!points_layer->nselected)
        return 0;

    memcpy(points, points_layer->points,
           2*points_layer->nselected*sizeof(gdouble));
    return points_layer->nselected;
}

/**
 * gwy_layer_points_unselect:
 * @layer: A #GwyLayerPoints.
 *
 * Clears the selected points.
 *
 * Note: may have unpredictable effects when called while user is dragging
 * some points.
 **/
void
gwy_layer_points_unselect(GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));

    GWY_LAYER_POINTS(layer)->nselected = 0;
    gwy_layer_points_save(layer, -1);
}

static void
gwy_layer_points_plugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));

    GWY_LAYER_POINTS(layer)->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_points_restore(layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_points_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_POINTS(layer));

    GWY_LAYER_POINTS(layer)->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_points_save(GwyDataViewLayer *layer,
                      gint i)
{
    GwyLayerPoints *p = GWY_LAYER_POINTS(layer);
    gchar key[64];
    gint from, to, n;

    /* TODO Container */
    gwy_container_set_int32_by_name(layer->data, "/0/points/nselected",
                                    p->nselected);
    if (i < 0) {
        from = 0;
        to = p->nselected - 1;
    }
    else
        from = to = i;

    for (i = from; i <= to; i++) {
        gdouble *coords = p->points + 2*i;

        n = g_snprintf(key, sizeof(key), "/0/points/%d/x", i);
        gwy_container_set_double_by_name(layer->data, key, coords[0]);
        key[n-1] = 'y';
        gwy_container_set_double_by_name(layer->data, key, coords[1]);
    }
}

static void
gwy_layer_points_restore(GwyDataViewLayer *layer)
{
    GwyLayerPoints *p = GWY_LAYER_POINTS(layer);
    gchar key[64];
    gint i, n;

    /* TODO Container */
    if (!gwy_container_contains_by_name(layer->data, "/0/points/nselected"))
        return;

    p->nselected = gwy_container_get_int32_by_name(layer->data,
                                                   "/0/points/nselected");
    p->nselected = MIN(p->nselected, p->npoints);
    for (i = 0; i < p->nselected; i++) {
        gdouble *coords = p->points + 2*i;

        n = g_snprintf(key, sizeof(key), "/0/points/%d/x", i);
        coords[0] = gwy_container_get_double_by_name(layer->data, key);
        key[n-1] = 'y';
        coords[1] = gwy_container_get_double_by_name(layer->data, key);
    }
}

static int
gwy_layer_points_near_point(GwyLayerPoints *layer,
                            gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble d2min;
    gint i;

    if (!layer->nselected)
        return -1;

    i = gwy_math_find_nearest_point(xreal, yreal, &d2min,
                                    2*layer->nselected, layer->points);

    dlayer = (GwyDataViewLayer*)layer;
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
             *gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
