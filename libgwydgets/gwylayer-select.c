/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>

#include "gwylayer-select.h"
#include "gwydataview.h"
#include <libdraw/gwypixfield.h>

#define _(x) x
#define gwy_object_unref(x) if (x) g_object_unref(x); (x) = NULL

#define GWY_LAYER_SELECT_TYPE_NAME "GwyLayerSelect"

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
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_select_finalize;

    layer_class->draw = gwy_layer_select_draw;
    layer_class->motion_notify = gwy_layer_select_motion_notify;
    layer_class->button_press = gwy_layer_select_button_pressed;
    layer_class->button_release = gwy_layer_select_button_released;
    layer_class->plugged = gwy_layer_select_plugged;
    layer_class->unplugged = gwy_layer_select_unplugged;
}

static void
gwy_layer_select_init(GwyLayerSelect *layer)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    layer->selected = FALSE;
}

static void
gwy_layer_select_finalize(GObject *object)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_LAYER_SELECT(object));

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_select_new:
 *
 * Creates a new rectangular selection layer.
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_select_new(void)
{
    GtkObject *object;
    GwyDataViewLayer *layer;
    GwyLayerSelect *select_layer;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

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

    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s [%g,%g] to [%g,%g]",
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
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s [%d,%d] to [%d,%d]",
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

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->button)
        return FALSE;

    gwy_layer_select_draw(layer, layer->parent->window);

    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "motion: %g %g", event->x, event->y);
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y,
                                    &select_layer->x1, &select_layer->y1);

    gwy_layer_select_draw(layer, layer->parent->window);

    return FALSE;
}

static gboolean
gwy_layer_select_button_pressed(GwyDataViewLayer *layer,
                                GdkEventButton *event)
{
    GwyLayerSelect *select_layer;
    gint x, y;

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    select_layer = (GwyLayerSelect*)layer;
    if (select_layer->button)
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
              "unexpected mouse button press when already pressed");
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s [%d,%d]",
          __FUNCTION__, x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;
    /* undraw a previous selection */
    if (select_layer->selected)
        gwy_layer_select_draw(layer, layer->parent->window);
    select_layer->button = event->button;
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y,
                                    &select_layer->x0, &select_layer->y0);
    select_layer->x1 = select_layer->x0;
    select_layer->y1 = select_layer->y0;
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s [%g,%g] to [%g,%g]",
          __FUNCTION__,
          select_layer->x0, select_layer->y0,
          select_layer->x1, select_layer->y1);
    select_layer->selected = TRUE;
    #if DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
          "selected == %d", select_layer->selected);
    #endif

    return FALSE;
}

static gboolean
gwy_layer_select_button_released(GwyDataViewLayer *layer,
                                 GdkEventButton *event)
{
    GwyLayerSelect *select_layer;
    gint x, y;
    gdouble tmp;

    select_layer = (GwyLayerSelect*)layer;
    if (!select_layer->button)
        return FALSE;
    select_layer->button = 0;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y,
                                    &select_layer->x1, &select_layer->y1);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    select_layer->x0, select_layer->y0,
                                    &x, &y);
    select_layer->selected = x != event->x && y != event->y;
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

    /* XXX: this assures no artefacts ...  */
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

    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
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
}

static void
gwy_layer_select_unplugged(GwyDataViewLayer *layer)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    GWY_LAYER_SELECT(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
