/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include "gwylayer-select.h"
#include "gwydataview.h"

#define GWY_LAYER_SELECT_TYPE_NAME "GwyLayerSelect"

#define PROXIMITY_DISTANCE 8

#define BITS_PER_SAMPLE 8

enum {
    PROP_0,
    PROP_IS_CROP,
    PROP_LAST
};

/* Forward declarations */

static void       gwy_layer_select_class_init        (GwyLayerSelectClass *klass);
static void       gwy_layer_select_init              (GwyLayerSelect *layer);
static void       gwy_layer_select_finalize          (GObject *object);
static void       gwy_layer_select_draw              (GwyVectorLayer *layer,
                                                      GdkDrawable *drawable);
static gboolean   gwy_layer_select_motion_notify     (GwyVectorLayer *layer,
                                                      GdkEventMotion *event);
static gboolean   gwy_layer_select_button_pressed    (GwyVectorLayer *layer,
                                                      GdkEventButton *event);
static gboolean   gwy_layer_select_button_released   (GwyVectorLayer *layer,
                                                      GdkEventButton *event);
static gint       gwy_layer_select_get_nselected     (GwyVectorLayer *layer);
static void       gwy_layer_select_unselect          (GwyVectorLayer *layer);
static void       gwy_layer_select_plugged           (GwyDataViewLayer *layer);
static void       gwy_layer_select_unplugged         (GwyDataViewLayer *layer);
static void       gwy_layer_select_save              (GwyLayerSelect *layer);
static void       gwy_layer_select_restore           (GwyLayerSelect *layer);
static gint       gwy_layer_select_near_point        (GwyLayerSelect *layer,
                                                      gdouble xreal,
                                                      gdouble yreal);

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
        gwy_debug("");
        gwy_layer_select_type
            = g_type_register_static(GWY_TYPE_VECTOR_LAYER,
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
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_select_finalize;

    layer_class->plugged = gwy_layer_select_plugged;
    layer_class->unplugged = gwy_layer_select_unplugged;

    vector_class->draw = gwy_layer_select_draw;
    vector_class->motion_notify = gwy_layer_select_motion_notify;
    vector_class->button_press = gwy_layer_select_button_pressed;
    vector_class->button_release = gwy_layer_select_button_released;
    vector_class->get_nselected = gwy_layer_select_get_nselected;
    vector_class->unselect = gwy_layer_select_unselect;

    memset(klass->corner_cursor, 0, 4*sizeof(GdkCursor*));
    klass->resize_cursor = NULL;
}

static void
gwy_layer_select_init(GwyLayerSelect *layer)
{
    GwyLayerSelectClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_SELECT_GET_CLASS(layer);
    gwy_vector_layer_cursor_new_or_ref(&klass->resize_cursor, GDK_CROSS);
    gwy_vector_layer_cursor_new_or_ref(&klass->corner_cursor[0], GDK_UL_ANGLE);
    gwy_vector_layer_cursor_new_or_ref(&klass->corner_cursor[1], GDK_LL_ANGLE);
    gwy_vector_layer_cursor_new_or_ref(&klass->corner_cursor[2], GDK_UR_ANGLE);
    gwy_vector_layer_cursor_new_or_ref(&klass->corner_cursor[3], GDK_LR_ANGLE);
    layer->selected = FALSE;
    layer->is_crop = FALSE;
}

static void
gwy_layer_select_finalize(GObject *object)
{
    GwyLayerSelectClass *klass;
    gint i;

    gwy_debug("");

    g_return_if_fail(GWY_IS_LAYER_SELECT(object));

    klass = GWY_LAYER_SELECT_GET_CLASS(object);
    gwy_vector_layer_cursor_free_or_unref(&klass->resize_cursor);
    for (i = 0; i < 4; i++)
        gwy_vector_layer_cursor_free_or_unref(&klass->corner_cursor[i]);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_select_new:
 *
 * Creates a new rectangular selection layer.
 *
 * Container keys: "/0/select/rect/x0", "/0/select/x1", "/0/select/y0",
 * "/0/select/rect/y1", and "/0/select/selected".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_select_new(void)
{
    GtkObject *object;

    gwy_debug("");
    object = g_object_new(GWY_TYPE_LAYER_SELECT, NULL);

    return object;
}

static void
gwy_layer_select_draw(GwyVectorLayer *layer,
                      GdkDrawable *drawable)
{
    GwyDataView *data_view;
    GwyLayerSelect *select_layer;
    gint xmin, ymin, xmax, ymax;

    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    select_layer = GWY_LAYER_SELECT(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(data_view);

    if (!select_layer->selected)
        return;

    if (!layer->gc)
        gwy_vector_layer_setup_gc(layer);

    gwy_debug("[%g,%g] to [%g,%g]",
              select_layer->x0, select_layer->y0,
              select_layer->x1, select_layer->y1);
    gwy_data_view_coords_real_to_xy(data_view,
                                    select_layer->x0, select_layer->y0,
                                    &xmin, &ymin);
    gwy_data_view_coords_real_to_xy(data_view,
                                    select_layer->x1, select_layer->y1,
                                    &xmax, &ymax);
    if (xmax < xmin)
        GWY_SWAP(gint, xmin, xmax);
    if (ymax < ymin)
        GWY_SWAP(gint, ymin, ymax);

    gwy_debug("[%d,%d] to [%d,%d]", xmin, ymin, xmax, ymax);
    if (select_layer->is_crop) {
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
gwy_layer_select_motion_notify(GwyVectorLayer *layer,
                               GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerSelectClass *klass;
    GwyLayerSelect *select_layer;
    GdkWindow *window;
    gint x, y, i;
    gdouble oldx, oldy, xreal, yreal;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    select_layer = GWY_LAYER_SELECT(layer);
    oldx = select_layer->x1;
    oldy = select_layer->y1;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (xreal == oldx && yreal == oldy)
        return FALSE;

    klass = GWY_LAYER_SELECT_GET_CLASS(select_layer);
    if (!select_layer->button) {
        i = gwy_layer_select_near_point(select_layer, xreal, yreal);
        select_layer->inear = i;
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->corner_cursor[i]);
        return FALSE;
    }

    gwy_layer_select_draw(layer, window);
    select_layer->x1 = xreal;
    select_layer->y1 = yreal;

    gwy_layer_select_save(select_layer);
    gwy_layer_select_draw(layer, window);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));

    return FALSE;
}

static gboolean
gwy_layer_select_button_pressed(GwyVectorLayer *layer,
                                GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerSelectClass *klass;
    GwyLayerSelect *select_layer;
    gint x, y;
    gdouble xreal, yreal;
    gboolean keep_old = FALSE;

    gwy_debug("");
    select_layer = GWY_LAYER_SELECT(layer);
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
    klass = GWY_LAYER_SELECT_GET_CLASS(select_layer);
    if (select_layer->selected) {
        gint i;

        gwy_layer_select_draw(layer, window);
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
    select_layer->selected = TRUE;
    if (!keep_old) {
        select_layer->x0 = xreal;
        select_layer->y0 = yreal;
        if (select_layer->is_crop)
            gwy_layer_select_draw(layer, window);
    }
    else
        gwy_layer_select_draw(layer, window);
    gwy_debug("[%g,%g] to [%g,%g]",
              select_layer->x0, select_layer->y0,
              select_layer->x1, select_layer->y1);

    gdk_window_set_cursor(window, klass->resize_cursor);

    return FALSE;
}

static gboolean
gwy_layer_select_button_released(GwyVectorLayer *layer,
                                 GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerSelectClass *klass;
    GwyLayerSelect *select_layer;
    gint x, y, i;
    gdouble xreal, yreal;

    select_layer = GWY_LAYER_SELECT(layer);
    if (!select_layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    select_layer->button = 0;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    select_layer->x1 = xreal;
    select_layer->y1 = yreal;
    gwy_data_view_coords_real_to_xy(data_view,
                                    select_layer->x0, select_layer->y0,
                                    &x, &y);
    select_layer->selected = (x != event->x) && (y != event->y);
    if (select_layer->selected) {
        if (select_layer->x1 < select_layer->x0)
            GWY_SWAP(gdouble, select_layer->x0, select_layer->x1);
        if (select_layer->y1 < select_layer->y0)
            GWY_SWAP(gdouble, select_layer->y0, select_layer->y1);
    }
    gwy_layer_select_save(select_layer);
    gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
    if (select_layer->selected)
        gwy_vector_layer_selection_finished(layer);

    klass = GWY_LAYER_SELECT_GET_CLASS(select_layer);
    i = gwy_layer_select_near_point(select_layer, xreal, yreal);
    gdk_window_set_cursor(window, i == -1 ? NULL : klass->corner_cursor[i]);

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
 * The @xmin, @ymin, @xmax, @ymax arguments can be NULL if you are not
 * interested in the particular coordinates.
 *
 * Returns: %TRUE when there is some selection present (and some values were
 *          stored), %FALSE
 **/
gboolean
gwy_layer_select_get_selection(GwyLayerSelect *layer,
                               gdouble *xmin, gdouble *ymin,
                               gdouble *xmax, gdouble *ymax)
{
    g_return_val_if_fail(GWY_IS_LAYER_SELECT(layer), FALSE);

    if (!layer->selected)
        return FALSE;

    if (xmin)
        *xmin = layer->x0;
    if (ymin)
        *ymin = layer->y0;
    if (xmax)
        *xmax = layer->x1;
    if (ymax)
        *ymax = layer->y1;

    return TRUE;
}

static gint
gwy_layer_select_get_nselected(GwyVectorLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_SELECT(layer), 0);
    return GWY_LAYER_SELECT(layer)->selected;
}

static void
gwy_layer_select_unselect(GwyVectorLayer *layer)
{
    GwyLayerSelect *select_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));
    select_layer = GWY_LAYER_SELECT(layer);

    if (!GWY_LAYER_SELECT(layer)->selected)
        return;

    parent = GWY_DATA_VIEW_LAYER(layer)->parent;
    /* this is in fact undraw */
    if (parent)
        gwy_layer_select_draw(layer, parent->window);
    select_layer->selected = FALSE;
    gwy_layer_select_save(select_layer);
}

/**
 * gwy_layer_select_set_is_crop:
 * @layer: A #GwyLayerSelect.
 * @is_crop: %TRUE if @layer should be crop-style, %FALSE for select-style.
 *
 * Sets crop-style (lines) or select-style (rectangle) of @layer.
 **/
void
gwy_layer_select_set_is_crop(GwyLayerSelect *layer,
                             gboolean is_crop)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    if (is_crop == layer->is_crop)
        return;

    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;
    if (parent)
        gwy_layer_select_draw(vector_layer, parent->window);
    layer->is_crop = is_crop;
    if (parent)
        gwy_layer_select_draw(vector_layer, parent->window);
}

/**
 * gwy_layer_select_get_is_crop:
 * @layer: A #GwyLayerSelect.
 *
 * Returns whether @layer style is crop (lines) or select (rectange).
 *
 * Returns: %TRUE when @layer is crop-style, %FALSE when @layer is
 *          select-style.
 **/
gboolean
gwy_layer_select_get_is_crop(GwyLayerSelect *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_SELECT(layer), FALSE);
    return layer->is_crop;
}

static void
gwy_layer_select_plugged(GwyDataViewLayer *layer)
{
    GwyLayerSelect *select_layer;

    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));
    select_layer = GWY_LAYER_SELECT(layer);

    select_layer->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_select_restore(select_layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_select_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_SELECT(layer));

    GWY_LAYER_SELECT(layer)->selected = FALSE;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_select_save(GwyLayerSelect *layer)
{
    GwyContainer *data;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    gwy_container_set_boolean_by_name(data, "/0/select/rect/selected",
                                      layer->selected);
    if (!layer->selected)
        return;
    gwy_container_set_double_by_name(data, "/0/select/rect/x0", layer->x0);
    gwy_container_set_double_by_name(data, "/0/select/rect/y0", layer->y0);
    gwy_container_set_double_by_name(data, "/0/select/rect/x1", layer->x1);
    gwy_container_set_double_by_name(data, "/0/select/rect/y1", layer->y1);
}

static void
gwy_layer_select_restore(GwyLayerSelect *layer)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xreal, yreal;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    if (!gwy_container_contains_by_name(data, "/0/select/rect/selected"))
        return;
    layer->selected
        = gwy_container_get_boolean_by_name(data, "/0/select/rect/selected");
    if (!layer->selected)
        return;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    layer->x0 = gwy_container_get_double_by_name(data, "/0/select/rect/x0");
    layer->x0 = CLAMP(layer->x0, 0.0, xreal);
    layer->y0 = gwy_container_get_double_by_name(data, "/0/select/rect/y0");
    layer->y0 = CLAMP(layer->y0, 0.0, yreal);
    layer->x1 = gwy_container_get_double_by_name(data, "/0/select/rect/x1");
    layer->x1 = CLAMP(layer->x1, 0.0, xreal);
    layer->y1 = gwy_container_get_double_by_name(data, "/0/select/rect/y1");
    layer->y1 = CLAMP(layer->y1, 0.0, yreal);
    if (layer->x0 == layer->x1 || layer->y0 == layer->y1)
        layer->selected = FALSE;
}

static int
gwy_layer_select_near_point(GwyLayerSelect *layer,
                            gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble coords[8], d2min;
    gint i;

    if (!layer->selected)
        return -1;

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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
