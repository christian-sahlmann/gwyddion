/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libdraw/gwypixfield.h>
#include "gwylayer-select.h"
#include "gwydataview.h"

#define GWY_LAYER_SELECT_TYPE_NAME "GwyLayerSelect"

#define PROXIMITY_DISTANCE 8

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_select_class_init        (GwyLayerSelectClass *klass);
static void       gwy_layer_select_init              (GwyLayerSelect *layer);
static void       gwy_layer_select_finalize          (GObject *object);
static void       gwy_layer_select_draw              (GwyDataViewLayer *layer,
                                                      GdkDrawable *drawable);
static gboolean   gwy_layer_select_motion_notify     (GwyDataViewLayer *layer,
                                                      GdkEventMotion *event);
static gboolean   gwy_layer_select_button_pressed    (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);
static gboolean   gwy_layer_select_button_released   (GwyDataViewLayer *layer,
                                                      GdkEventButton *event);
static void       gwy_layer_select_plugged           (GwyDataViewLayer *layer);
static void       gwy_layer_select_unplugged         (GwyDataViewLayer *layer);

static int        gwy_layer_select_near_point        (GwyLayerSelect *layer,
                                                      gdouble xreal,
                                                      gdouble yreal);
static int        gwy_math_find_nearest_point        (gdouble x,
                                                      gdouble y,
                                                      gdouble *d2min,
                                                      gint n,
                                                      gdouble *coords);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_select_get_type(void)
{
    static GType gwy_layer_select_type = 0;

    if (!gwy_layer_select_type) {
        static const GTypeInfo gwy_layer_select_info = {
            sizeof(GwyLayerSelectClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_select_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerSelect),
            0,
            (GInstanceInitFunc)gwy_layer_select_init,
            NULL,
        };
        gwy_debug("%s", __FUNCTION__);
        gwy_layer_select_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_SELECT_TYPE_NAME,
                                     &gwy_layer_select_info,
                                     0);
    }

    return gwy_layer_select_type;
}

static void
gwy_layer_select_class_init(GwyLayerSelectClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("%s", __FUNCTION__);

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_select_finalize;

    layer_class->draw = gwy_layer_select_draw;
    layer_class->motion_notify = gwy_layer_select_motion_notify;
    layer_class->button_press = gwy_layer_select_button_pressed;
    layer_class->button_release = gwy_layer_select_button_released;
    layer_class->plugged = gwy_layer_select_plugged;
    layer_class->unplugged = gwy_layer_select_unplugged;

    klass->near_cursor = NULL;
    klass->resize_cursor = NULL;
}

static void
gwy_layer_select_init(GwyLayerSelect *layer)
{
    GwyLayerSelectClass *klass;

    gwy_debug("%s", __FUNCTION__);

    klass = GWY_LAYER_SELECT_GET_CLASS(layer);
    if (!klass->near_cursor) {
        g_assert(!klass->resize_cursor);
        klass->near_cursor = gdk_cursor_new(GDK_DOTBOX);
        klass->resize_cursor = gdk_cursor_new(GDK_SIZING);
    }
    else {
        g_assert(klass->resize_cursor);
        gdk_cursor_ref(klass->near_cursor);
        gdk_cursor_ref(klass->resize_cursor);
    }
    layer->selected = FALSE;
}

static void
gwy_layer_select_finalize(GObject *object)
{
    GwyLayerSelectClass *klass;

    gwy_debug("%s", __FUNCTION__);

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_LAYER_SELECT(object));

    klass = GWY_LAYER_SELECT_GET_CLASS(object);
    gdk_cursor_unref(klass->near_cursor);
    gdk_cursor_unref(klass->resize_cursor);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_select_new:
 *
 * Creates a new rectangular selection layer.
 *
 * Container keys: "/0/select/x0", "/0/select/x1", "/0/select/y0",
 * "/0/select/y1", and "/0/select/selected".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_select_new(void)
{
    GtkObject *object;
    GwyDataViewLayer *layer;
    GwyLayerSelect *select_layer;

    gwy_debug("%s", __FUNCTION__);

    object = g_object_new(GWY_TYPE_LAYER_SELECT, NULL);
    layer = (GwyDataViewLayer*)object;
    select_layer = (GwyLayerSelect*)layer;


    return object;
}

static void
gwy_layer_select_setup_gc(GwyDataViewLayer *layer)
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
gwy_layer_select_draw(GwyDataViewLayer *layer, GdkDrawable *drawable)
{
    GwyLayerSelect *select_layer;
    gint xmin, ymin, xmax, ymax, tmp;

    g_return_if_fail(layer);
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    select_layer = (GwyLayerSelect*)layer;

    if (!select_layer->selected)
        return;

    if (!layer->gc)
        gwy_layer_select_setup_gc(layer);

    gwy_debug("%s [%g,%g] to [%g,%g]",
              __FUNCTION__,
              select_layer->x0, select_layer->y0,
              select_layer->x1, select_layer->y1);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    select_layer->x0, select_layer->y0,
                                    &xmin, &ymin);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    select_layer->x1, select_layer->y1,
                                    &xmax, &ymax);
    if (xmax < xmin) {
        tmp = xmax;
        xmax = xmin;
        xmin = tmp;
    }
    if (ymax < ymin) {
        tmp = ymax;
        ymax = ymin;
        ymin = tmp;
    }
    gwy_debug("%s [%d,%d] to [%d,%d]",
              __FUNCTION__, xmin, ymin, xmax, ymax);
    gdk_draw_rectangle(drawable, layer->gc, FALSE,
                       xmin, ymin, xmax - xmin, ymax - ymin);

}

static gboolean
gwy_layer_select_motion_notify(GwyDataViewLayer *layer,
                               GdkEventMotion *event)
{
    GwyLayerSelect *select_layer;
    gint x, y;
    gdouble oldx, oldy, xreal, yreal;

    select_layer = (GwyLayerSelect*)layer;
    oldx = select_layer->x1;
    oldy = select_layer->y1;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    if (xreal == oldx && yreal == oldy)
        return FALSE;

    if (!select_layer->button) {
        gint i = gwy_layer_select_near_point(select_layer, xreal, yreal);

        if (select_layer->near != i) {
            GwyLayerSelectClass *klass;

            select_layer->near = i;
            klass = GWY_LAYER_SELECT_GET_CLASS(select_layer);
            gdk_window_set_cursor(GTK_WIDGET(layer->parent)->window,
                                  i == -1 ? NULL : klass->near_cursor);
        }
        return FALSE;
    }

    gwy_layer_select_draw(layer, layer->parent->window);
    select_layer->x1 = xreal;
    select_layer->y1 = yreal;

    /* TODO Container */
    gwy_container_set_double_by_name(layer->data, "/0/select/x0",
                                     select_layer->x0);
    gwy_container_set_double_by_name(layer->data, "/0/select/y0",
                                     select_layer->y0);
    gwy_container_set_double_by_name(layer->data, "/0/select/x1",
                                     select_layer->x1);
    gwy_container_set_double_by_name(layer->data, "/0/select/y1",
                                     select_layer->y1);

    gwy_layer_select_draw(layer, layer->parent->window);
    gwy_data_view_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_select_button_pressed(GwyDataViewLayer *layer,
                                GdkEventButton *event)
{
    GwyLayerSelect *select_layer;
    gint x, y;
    gdouble xreal, yreal;
    gboolean keep_old = FALSE;

    gwy_debug("%s", __FUNCTION__);
    select_layer = (GwyLayerSelect*)layer;
    if (select_layer->button)
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
    /* handle a previous selection:
     * when we are near a corner, resize the existing one
     * otherwise forget it and start from scratch */
    if (select_layer->selected) {
        gint i;

        gwy_layer_select_draw(layer, layer->parent->window);
        i = gwy_layer_select_near_point(select_layer, xreal, yreal);
        if (i >= 0) {
            keep_old = TRUE;
            if (i/2)
                select_layer->x0 = MIN(select_layer->x0, select_layer->x1);
            else
                select_layer->x0 = MAX(select_layer->x0, select_layer->x1);
            if (i%2)
                select_layer->y0 = MIN(select_layer->y0, select_layer->y1);
            else
                select_layer->y0 = MAX(select_layer->y0, select_layer->y1);
        }
    }
    select_layer->button = event->button;
    select_layer->x1 = xreal;
    select_layer->y1 = yreal;
    if (!keep_old) {
        select_layer->x0 = xreal;
        select_layer->y0 = yreal;
    }
    else
        gwy_layer_select_draw(layer, layer->parent->window);
    gwy_debug("%s [%g,%g] to [%g,%g]",
              __FUNCTION__,
              select_layer->x0, select_layer->y0,
              select_layer->x1, select_layer->y1);
    select_layer->selected = TRUE;
    gwy_debug("selected == %d", select_layer->selected);

    gdk_window_set_cursor(GTK_WIDGET(layer->parent)->window,
                          GWY_LAYER_SELECT_GET_CLASS(layer)->resize_cursor);

    return FALSE;
}

static gboolean
gwy_layer_select_button_released(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyLayerSelect *select_layer;
    gint x, y;
    gdouble tmp;
    gboolean outside;

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->button)
        return FALSE;
    select_layer->button = 0;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    outside = (x != event->x) || (y != event->y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y,
                                    &select_layer->x1, &select_layer->y1);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    select_layer->x0, select_layer->y0,
                                    &x, &y);
    select_layer->selected = (x != event->x) && (y != event->y);
    if (select_layer->selected) {
        if (select_layer->x1 < select_layer->x0) {
            tmp = select_layer->x1;
            select_layer->x1 = select_layer->x0;
            select_layer->x0 = tmp;
        }
        if (select_layer->y1 < select_layer->y0) {
            tmp = select_layer->y1;
            select_layer->y1 = select_layer->y0;
            select_layer->y0 = tmp;
        }

        /* TODO Container */
        gwy_container_set_double_by_name(layer->data, "/0/select/x0",
                                         select_layer->x0);
        gwy_container_set_double_by_name(layer->data, "/0/select/y0",
                                         select_layer->y0);
        gwy_container_set_double_by_name(layer->data, "/0/select/x1",
                                         select_layer->x1);
        gwy_container_set_double_by_name(layer->data, "/0/select/y1",
                                         select_layer->y1);
    }
    /* TODO Container */
    gwy_container_set_boolean_by_name(layer->data, "/0/select/selected",
                                      select_layer->selected);
    gwy_data_view_layer_updated(layer);

    gdk_window_set_cursor(GTK_WIDGET(layer->parent)->window,
                          outside ? NULL
                            : GWY_LAYER_SELECT_GET_CLASS(layer)->near_cursor);

    /* XXX: this assures no artifacts ...  */
    gtk_widget_queue_draw(layer->parent);

    return FALSE;
}

/**
 * gwy_layer_select_get_selection:
 * @layer: A #GwyLayerSelect.
 * @xmin: Where the upper left corner x-coordinate should be stored.
 * @ymin: Where the upper left corner y-coordinate should be stored.
 * @xmax: Where the lower right corner x-coordinate should be stored.
 * @ymax: Where the lower right corner x-coordinate should be stored.
 *
 * Obtains the selected rectangle in real (i.e., physical) coordinates.
 *
 * FIXME: this should be done through container.
 *
 * Returns: %TRUE when there is some selection present (and some values were
 *          stored), %FALSE
 **/
gboolean
gwy_layer_select_get_selection(GwyDataViewLayer *layer,
                               gdouble *xmin, gdouble *ymin,
                               gdouble *xmax, gdouble *ymax)
{
    GwyLayerSelect *select_layer;

    g_return_val_if_fail(GWY_IS_LAYER_SELECT(layer), FALSE);

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->selected)
        return FALSE;

    if (*xmin)
        *xmin = select_layer->x0;
    if (*ymin)
        *ymin = select_layer->y0;
    if (*xmax)
        *xmax = select_layer->x1;
    if (*ymax)
        *ymax = select_layer->y1;
    return TRUE;
}

static void
gwy_layer_select_plugged(GwyDataViewLayer *layer)
{
    GwyLayerSelect *select_layer;

    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));
    select_layer = (GwyLayerSelect*)layer;

    select_layer->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    /* TODO Container */
    if (gwy_container_contains_by_name(layer->data, "/0/select/selected")) {
        select_layer->selected
            = gwy_container_get_boolean_by_name(layer->data,
                                                "/0/select/selected");
        if (select_layer->selected) {
            select_layer->x0 = gwy_container_get_double_by_name(layer->data,
                                                                "/0/select/x0");
            select_layer->y0 = gwy_container_get_double_by_name(layer->data,
                                                                "/0/select/y0");
            select_layer->x1 = gwy_container_get_double_by_name(layer->data,
                                                                "/0/select/x1");
            select_layer->y1 = gwy_container_get_double_by_name(layer->data,
                                                                "/0/select/y1");
        }
    }
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_select_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("%s", __FUNCTION__);
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    GWY_LAYER_SELECT(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static int
gwy_layer_select_near_point(GwyLayerSelect *layer,
                            gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble coords[8], d2min;
    gint i;

    coords[0] = coords[2] = layer->x0;
    coords[1] = coords[5] = layer->y0;
    coords[4] = coords[6] = layer->x1;
    coords[3] = coords[7] = layer->y1;
    i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 4, coords);

    dlayer = (GwyDataViewLayer*)layer;
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
             *gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/*********** FIXME: move it somewhere ************/
static int
gwy_math_find_nearest_point(gdouble x, gdouble y,
                            gdouble *d2min,
                            gint n, gdouble *coords)
{
    gint i, m;

    *d2min = G_MAXDOUBLE;
    g_return_val_if_fail(n > 0, 0);
    g_return_val_if_fail(coords, 0);

    m = 0;
    for (i = 0; i < n; i++) {
        gdouble xx = *(coords++);
        gdouble yy = *(coords++);
        gdouble d;

        d = (xx - x)*(xx - x) + (yy - y)*(yy - y);
        if (d < *d2min) {
            *d2min = d;
            m = i;
        }
    }
    return m;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
