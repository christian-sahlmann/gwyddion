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

#include <math.h>
#include <string.h>
#include <glib-object.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include "gwylayer-lines.h"
#include "gwydataview.h"

#define GWY_LAYER_LINES_TYPE_NAME "GwyLayerLines"

#define PROXIMITY_DISTANCE 8
#define CROSS_SIZE 8

#define BITS_PER_SAMPLE 8

/* Forward declarations */

static void       gwy_layer_lines_class_init      (GwyLayerLinesClass *klass);
static void       gwy_layer_lines_init            (GwyLayerLines *layer);
static void       gwy_layer_lines_finalize        (GObject *object);
static void       gwy_layer_lines_draw            (GwyDataViewLayer *layer,
                                                   GdkDrawable *drawable);
static void       gwy_layer_lines_draw_line       (GwyDataViewLayer *layer,
                                                   GdkDrawable *drawable,
                                                   gint i);
static gboolean   gwy_layer_lines_motion_notify   (GwyDataViewLayer *layer,
                                                   GdkEventMotion *event);
static gboolean   gwy_layer_lines_do_move_line    (GwyDataViewLayer *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);
static gboolean   gwy_layer_lines_button_pressed  (GwyDataViewLayer *layer,
                                                   GdkEventButton *event);
static gboolean   gwy_layer_lines_button_released (GwyDataViewLayer *layer,
                                                   GdkEventButton *event);
static void       gwy_layer_lines_plugged         (GwyDataViewLayer *layer);
static void       gwy_layer_lines_unplugged       (GwyDataViewLayer *layer);
static void       gwy_layer_lines_save            (GwyDataViewLayer *layer,
                                                   gint i);
static void       gwy_layer_lines_restore         (GwyDataViewLayer *layer);

static int        gwy_layer_lines_near_line       (GwyLayerLines *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);
static int        gwy_layer_lines_near_point      (GwyLayerLines *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);

/* Local data */

static GtkObjectClass *parent_class = NULL;

GType
gwy_layer_lines_get_type(void)
{
    static GType gwy_layer_lines_type = 0;

    if (!gwy_layer_lines_type) {
        static const GTypeInfo gwy_layer_lines_info = {
            sizeof(GwyLayerLinesClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_layer_lines_class_init,
            NULL,
            NULL,
            sizeof(GwyLayerLines),
            0,
            (GInstanceInitFunc)gwy_layer_lines_init,
            NULL,
        };
        gwy_debug("");
        gwy_layer_lines_type
            = g_type_register_static(GWY_TYPE_DATA_VIEW_LAYER,
                                     GWY_LAYER_LINES_TYPE_NAME,
                                     &gwy_layer_lines_info,
                                     0);
    }

    return gwy_layer_lines_type;
}

static void
gwy_layer_lines_class_init(GwyLayerLinesClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_lines_finalize;

    layer_class->draw = gwy_layer_lines_draw;
    layer_class->motion_notify = gwy_layer_lines_motion_notify;
    layer_class->button_press = gwy_layer_lines_button_pressed;
    layer_class->button_release = gwy_layer_lines_button_released;
    layer_class->plugged = gwy_layer_lines_plugged;
    layer_class->unplugged = gwy_layer_lines_unplugged;

    klass->near_cursor = NULL;
    klass->nearline_cursor = NULL;
    klass->move_cursor = NULL;
}

static void
gwy_layer_lines_init(GwyLayerLines *layer)
{
    GwyLayerLinesClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_LINES_GET_CLASS(layer);
    gwy_layer_cursor_new_or_ref(&klass->near_cursor, GDK_DOTBOX);
    gwy_layer_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);
    gwy_layer_cursor_new_or_ref(&klass->nearline_cursor, GDK_FLEUR);

    layer->nlines = 3;
    layer->nselected = 0;
    layer->inear = -1;
    layer->lines = g_new(gdouble, 4*layer->nlines);
}

static void
gwy_layer_lines_finalize(GObject *object)
{
    GwyLayerLinesClass *klass;
    GwyLayerLines *layer;

    gwy_debug("");

    g_return_if_fail(object != NULL);
    g_return_if_fail(GWY_IS_LAYER_LINES(object));

    layer = (GwyLayerLines*)object;
    klass = GWY_LAYER_LINES_GET_CLASS(object);
    gwy_layer_cursor_free_or_unref(&klass->near_cursor);
    gwy_layer_cursor_free_or_unref(&klass->move_cursor);
    gwy_layer_cursor_free_or_unref(&klass->nearline_cursor);

    g_free(layer->lines);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

/**
 * gwy_layer_lines_new:
 *
 * Creates a new line selection layer.
 *
 * The default number of lines to select is three.
 *
 * Container keys: "/0/select/lines/0/x0", "/0/select/lines/0/y0",
 * "/0/select/lines/0/x1", "/0/select/lines/0/y1", "/0/select/lines/1/x0",
 * "/0/select/lines/1/y0", etc., and "/0/select/lines/nselected".
 *
 * Returns: The newly created layer.
 **/
GtkObject*
gwy_layer_lines_new(void)
{
    GtkObject *object;

    gwy_debug("");
    object = g_object_new(GWY_TYPE_LAYER_LINES, NULL);

    return object;
}

/**
 * gwy_layer_lines_set_max_lines:
 * @layer: A #GwyLayerLines.
 * @nlines: The number of lines to select.
 *
 * Sets the number of lines to @nlines.
 *
 * This is the maximum number of lines user can select and also the number of
 * lines to be selected to emit the "finished" signal.
 **/
void
gwy_layer_lines_set_max_lines(GwyDataViewLayer *layer,
                              gint nlines)
{
    GwyLayerLines *lines_layer;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    g_return_if_fail(nlines > 0 && nlines < 1024);

    lines_layer = (GwyLayerLines*)layer;
    lines_layer->nlines = nlines;
    lines_layer->nselected = MIN(lines_layer->nselected, nlines);
    if (lines_layer->inear >= nlines)
        lines_layer->inear = -1;
    lines_layer->lines = g_renew(gdouble, lines_layer->lines,
                                 4*lines_layer->nlines);
}

/**
 * gwy_layer_lines_get_max_lines:
 * @layer: A #GwyLayerLines.
 *
 * Returns the number of selection lines for this layer.
 *
 * Returns: The number of lines to select.
 **/
gint
gwy_layer_lines_get_max_lines(GwyDataViewLayer *layer)
{
    g_return_val_if_fail(GWY_IS_LAYER_LINES(layer), 0);

    return GWY_LAYER_LINES(layer)->nlines;
}

static void
gwy_layer_lines_setup_gc(GwyDataViewLayer *layer)
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
gwy_layer_lines_draw(GwyDataViewLayer *layer,
                     GdkDrawable *drawable)
{
    GwyLayerLines *lines_layer;
    gint i;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    lines_layer = (GwyLayerLines*)layer;
    for (i = 0; i < lines_layer->nselected; i++)
        gwy_layer_lines_draw_line(layer, drawable, i);
}

static void
gwy_layer_lines_draw_line(GwyDataViewLayer *layer,
                          GdkDrawable *drawable,
                          gint i)
{
    GwyLayerLines *lines_layer;
    gint x0, y0, x1, y1;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    lines_layer = (GwyLayerLines*)layer;
    g_return_if_fail(i >= 0 && i < lines_layer->nselected);

    if (!layer->gc)
        gwy_layer_lines_setup_gc(layer);

    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    lines_layer->lines[4*i],
                                    lines_layer->lines[4*i + 1],
                                    &x0, &y0);
    gwy_data_view_coords_real_to_xy(GWY_DATA_VIEW(layer->parent),
                                    lines_layer->lines[4*i + 2],
                                    lines_layer->lines[4*i + 3],
                                    &x1, &y1);
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x0, &y0);
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x1, &y1);
    gdk_draw_line(drawable, layer->gc, x0, y0, x1, y1);
}

static gboolean
gwy_layer_lines_motion_notify(GwyDataViewLayer *layer,
                              GdkEventMotion *event)
{
    GwyLayerLinesClass *klass;
    GwyLayerLines *lines_layer;
    gint x, y, i, j;
    gdouble xreal, yreal;

    lines_layer = (GwyLayerLines*)layer;
    i = lines_layer->inear;
    x = event->x;
    y = event->y;
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    if (lines_layer->button && lines_layer->moving_line)
        return gwy_layer_lines_do_move_line(layer, xreal, yreal);
    if (i > -1
        && xreal == lines_layer->lines[2*i]
        && yreal == lines_layer->lines[2*i + 1])
        return FALSE;

    if (!lines_layer->button) {
        klass = GWY_LAYER_LINES_GET_CLASS(lines_layer);
        j = gwy_layer_lines_near_line(lines_layer, xreal, yreal);
        i = gwy_layer_lines_near_point(lines_layer, xreal, yreal);
        if (i == -1 && j >= 0)
            gdk_window_set_cursor(layer->parent->window,
                                  klass->nearline_cursor);
        else
            gdk_window_set_cursor(layer->parent->window,
                                  i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(lines_layer->inear != -1);
    gwy_layer_lines_draw_line(layer, layer->parent->window, i/2);
    lines_layer->lines[2*i] = xreal;
    lines_layer->lines[2*i + 1] = yreal;
    gwy_layer_lines_save(layer, i/2);
    gwy_layer_lines_draw_line(layer, layer->parent->window, i/2);
    gwy_data_view_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_lines_do_move_line(GwyDataViewLayer *layer,
                             gdouble xreal, gdouble yreal)
{
    GwyLayerLines *lines_layer;
    GwyDataView *data_view;
    gdouble coords[4];
    gdouble *line;
    gint x, y, i;

    lines_layer = GWY_LAYER_LINES(layer);
    data_view = GWY_DATA_VIEW(layer->parent);
    g_return_val_if_fail(lines_layer->inear != -1, FALSE);

    i = lines_layer->inear;
    line = lines_layer->lines + 4*i;

    /* compute wanted new coordinates of the first endpoint */
    coords[0] = xreal + lines_layer->lmove_x;
    coords[1] = yreal + lines_layer->lmove_y;
    if (coords[0] == line[0] && coords[1] == line[1])
        return FALSE;

    /* clamp them to get best possible */
    gwy_data_view_coords_real_to_xy(data_view, coords[0], coords[1], &x, &y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &coords[0], &coords[1]);

    /* compute wanted new coordinates of the second endpoint */
    coords[2] = (coords[0] - line[0]) + line[2];
    coords[3] = (coords[1] - line[1]) + line[3];

    if (coords[2] == line[2] && coords[3] == line[3])
        return FALSE;

    /* clamp them to get best possible */
    gwy_data_view_coords_real_to_xy(data_view, coords[2], coords[3], &x, &y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &coords[2], &coords[3]);

    /* recompute the first ones */
    coords[0] = (coords[2] - line[2]) + line[0];
    coords[1] = (coords[3] - line[3]) + line[1];

    if (coords[0] == line[0] && coords[1] == line[1])
        return FALSE;

    gwy_layer_lines_draw_line(layer, layer->parent->window, i);
    gwy_debug("%d %g %g %g %g", i, coords[0], coords[1], coords[2], coords[3]);
    memcpy(line, coords, 4*sizeof(gdouble));
    gwy_layer_lines_save(layer, i);
    gwy_layer_lines_draw_line(layer, layer->parent->window, i);
    gwy_data_view_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_lines_button_pressed(GwyDataViewLayer *layer,
                               GdkEventButton *event)
{
    GwyLayerLines *lines_layer;
    gint x, y, i, j;
    gdouble xreal, yreal;

    gwy_debug("");
    lines_layer = (GwyLayerLines*)layer;
    if (lines_layer->button)
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
    /* handle existing lines */
    j = gwy_layer_lines_near_line(lines_layer, xreal, yreal);
    i = gwy_layer_lines_near_point(lines_layer, xreal, yreal);
    if (i == -1 && j >= 0) {
        lines_layer->inear = j;
        lines_layer->moving_line = TRUE;
        lines_layer->lmove_x = lines_layer->lines[4*j] - xreal;
        lines_layer->lmove_y = lines_layer->lines[4*j + 1] - yreal;
        i = 2*j;
    }
    else {
        if (i >= 0) {
            lines_layer->inear = i;
            gwy_layer_lines_draw_line(layer, layer->parent->window, i/2);
        }
        else {
            /* add a line, or do nothing when maximum is reached */
            if (lines_layer->nselected == lines_layer->nlines)
                return FALSE;
            i = lines_layer->inear = 2*lines_layer->nselected;
            lines_layer->nselected++;
            lines_layer->lines[2*i] = xreal;
            lines_layer->lines[2*i + 1] = yreal;
            i++;
        }
        lines_layer->moving_line = FALSE;
        lines_layer->lines[2*i] = xreal;
        lines_layer->lines[2*i + 1] = yreal;
        gwy_layer_lines_draw_line(layer, layer->parent->window, i/2);
    }
    lines_layer->button = event->button;

    gdk_window_set_cursor(layer->parent->window,
                          GWY_LAYER_LINES_GET_CLASS(layer)->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_lines_button_released(GwyDataViewLayer *layer,
                                GdkEventButton *event)
{
    GwyLayerLinesClass *klass;
    GwyLayerLines *lines_layer;
    gint x, y, i, j;
    gdouble xreal, yreal;
    gboolean outside;

    lines_layer = (GwyLayerLines*)layer;
    if (!lines_layer->button)
        return FALSE;
    lines_layer->button = 0;
    x = event->x;
    y = event->y;
    i = lines_layer->inear;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(GWY_DATA_VIEW(layer->parent), &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(GWY_DATA_VIEW(layer->parent),
                                    x, y, &xreal, &yreal);
    if (lines_layer->moving_line)
        gwy_layer_lines_do_move_line(layer, xreal, yreal);
    else {
        gwy_layer_lines_draw_line(layer, layer->parent->window, i/2);
        lines_layer->lines[2*i] = xreal;
        lines_layer->lines[2*i + 1] = yreal;
        gwy_layer_lines_save(layer, i/2);
        gwy_layer_lines_draw_line(layer, layer->parent->window, i/2);
        gwy_data_view_layer_updated(layer);
    }
    if (lines_layer->nselected == lines_layer->nlines)
        gwy_data_view_layer_finished(layer);

    klass = GWY_LAYER_LINES_GET_CLASS(lines_layer);
    j = gwy_layer_lines_near_line(lines_layer, xreal, yreal);
    i = gwy_layer_lines_near_point(lines_layer, xreal, yreal);
    if (i == -1 && j >= 0)
        gdk_window_set_cursor(layer->parent->window,
                              klass->nearline_cursor);
    else
        gdk_window_set_cursor(layer->parent->window,
                              i == -1 ? NULL : klass->near_cursor);

    return FALSE;
}

/**
 * gwy_layer_lines_get_lines:
 * @layer: A #GwyLayerLines.
 * @lines: Where the point coordinates should be stored in.
 *
 * Obtains the selected lines.
 *
 * The @lines array should be four times the size of
 * gwy_layer_lines_get_max_lines(), lines are stored as x0, y0, x1, y1.
 * If less than gwy_layer_lines_get_max_lines() lines are actually selected
 * the remaining items will not have meaningful values.
 *
 * Returns: The number of actually selected lines.
 **/
gint
gwy_layer_lines_get_lines(GwyDataViewLayer *layer,
                          gdouble *lines)
{
    GwyLayerLines *lines_layer;

    g_return_val_if_fail(GWY_IS_LAYER_LINES(layer), 0);
    g_return_val_if_fail(lines, 0);

    lines_layer = (GwyLayerLines*)layer;
    if (!lines_layer->nselected)
        return 0;

    memcpy(lines, lines_layer->lines,
           4*lines_layer->nselected*sizeof(gdouble));
    return lines_layer->nselected;
}

/**
 * gwy_layer_lines_unselect:
 * @layer: A #GwyLayerLines.
 *
 * Clears the selected lines.
 *
 * Note: may have unpredictable effects when called while user is dragging
 * some lines.
 **/
void
gwy_layer_lines_unselect(GwyDataViewLayer *layer)
{
    g_return_if_fail(GWY_IS_LAYER_LINES(layer));

    if (GWY_LAYER_LINES(layer)->nselected == 0)
        return;

    /* this is in fact undraw */
    if (layer->parent)
        gwy_layer_lines_draw(layer, layer->parent->window);
    GWY_LAYER_LINES(layer)->nselected = 0;
    gwy_layer_lines_save(layer, -1);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_lines_plugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_LINES(layer));

    GWY_LAYER_LINES(layer)->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_lines_restore(layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_lines_unplugged(GwyDataViewLayer *layer)
{
    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_LINES(layer));

    GWY_LAYER_LINES(layer)->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_lines_save(GwyDataViewLayer *layer,
                     gint i)
{
    GwyLayerLines *l = GWY_LAYER_LINES(layer);
    gchar key[64];
    gint from, to, n;

    /* TODO Container */
    gwy_container_set_int32_by_name(layer->data, "/0/select/lines/nselected",
                                    l->nselected);
    if (i < 0) {
        from = 0;
        to = l->nselected - 1;
    }
    else
        from = to = i;

    for (i = from; i <= to; i++) {
        gdouble *coords = l->lines + 4*i;

        gwy_debug("%d %g %g %g %g", i, coords[0], coords[1], coords[2], coords[3]);
        n = g_snprintf(key, sizeof(key), "/0/select/lines/%d/x0", i);
        gwy_container_set_double_by_name(layer->data, key, coords[0]);
        key[n-2] = 'y';
        gwy_container_set_double_by_name(layer->data, key, coords[1]);
        key[n-1] = '1';
        gwy_container_set_double_by_name(layer->data, key, coords[3]);
        key[n-2] = 'x';
        gwy_container_set_double_by_name(layer->data, key, coords[2]);
    }
}

static void
gwy_layer_lines_restore(GwyDataViewLayer *layer)
{
    GwyLayerLines *l = GWY_LAYER_LINES(layer);
    GwyDataField *dfield;
    gchar key[24];
    gdouble xreal, yreal;
    gint i, n, nsel;

    /* TODO Container */
    if (!gwy_container_contains_by_name(layer->data,
                                        "/0/select/lines/nselected"))
        return;

    nsel = gwy_container_get_int32_by_name(layer->data,
                                           "/0/select/lines/nselected");
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(layer->data,
                                                             "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    for (i = l->nselected = 0; i < nsel && l->nselected < l->nlines; i++) {
        gdouble *coords = l->lines + 4*l->nselected;

        n = g_snprintf(key, sizeof(key), "/0/select/lines/%d/x0", i);
        coords[0] = gwy_container_get_double_by_name(layer->data, key);
        key[n-2] = 'y';
        coords[1] = gwy_container_get_double_by_name(layer->data, key);
        key[n-1] = '1';
        coords[3] = gwy_container_get_double_by_name(layer->data, key);
        key[n-2] = 'x';
        coords[2] = gwy_container_get_double_by_name(layer->data, key);
        if (coords[0] >= 0.0 && coords[0] <= xreal
            && coords[1] >= 0.0 && coords[1] <= yreal
            && coords[2] >= 0.0 && coords[2] <= xreal
            && coords[3] >= 0.0 && coords[3] <= yreal)
            l->nselected++;
    }
}

static int
gwy_layer_lines_near_line(GwyLayerLines *layer,
                          gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble d2min;
    gint i;

    if (!layer->nselected)
        return -1;

    i = gwy_math_find_nearest_line(xreal, yreal, &d2min,
                                   layer->nselected, layer->lines);
    if (i == -1)
        return -1;

    dlayer = (GwyDataViewLayer*)layer;
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
             *gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

static int
gwy_layer_lines_near_point(GwyLayerLines *layer,
                           gdouble xreal, gdouble yreal)
{
    GwyDataViewLayer *dlayer;
    gdouble d2min;
    gint i;

    if (!layer->nselected)
        return -1;

    /* note we search for an *endpoint*, not whole line, so divide i by 2
     * to get line index */
    i = gwy_math_find_nearest_point(xreal, yreal, &d2min,
                                    2*layer->nselected, layer->lines);

    dlayer = (GwyDataViewLayer*)layer;
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(GWY_DATA_VIEW(dlayer->parent))
             *gwy_data_view_get_ymeasure(GWY_DATA_VIEW(dlayer->parent));

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
