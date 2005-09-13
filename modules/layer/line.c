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

#define GWY_TYPE_LAYER_LINE            (gwy_layer_line_get_type())
#define GWY_LAYER_LINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_LINE, GwyLayerLine))
#define GWY_IS_LAYER_LINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_LINE))
#define GWY_LAYER_LINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_LINE, GwyLayerLineClass))

#define GWY_TYPE_SELECTION_LINE            (gwy_selection_line_get_type())
#define GWY_SELECTION_LINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_SELECTION_LINE, GwySelectionLine))
#define GWY_IS_SELECTION_LINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_SELECTION_LINE))
#define GWY_SELECTION_LINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_SELECTION_LINE, GwySelectionLineClass))

enum {
    OBJECT_SIZE = 4
};

enum {
    PROP_0,
    PROP_LINE_NUMBERS
};

typedef struct _GwyLayerLine          GwyLayerLine;
typedef struct _GwyLayerLineClass     GwyLayerLineClass;
typedef struct _GwySelectionLine      GwySelectionLine;
typedef struct _GwySelectionLineClass GwySelectionLineClass;

struct _GwyLayerLine {
    GwyVectorLayer parent_instance;

    /* Properties */
    gboolean line_numbers;

    /* Dynamic state */
    gboolean moving_line;
    gdouble lmove_x;
    gdouble lmove_y;
    GPtrArray *line_labels;
};

struct _GwyLayerLinesClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *near_cursor;
    GdkCursor *nearline_cursor;
    GdkCursor *move_cursor;
};

struct _GwySelectionLine {
    GwySelection parent_instance;
};

struct _GwySelectionLineClass {
    GwySelectionClass parent_class;
};

static gboolean module_register                (const gchar *name);
static GType    gwy_layer_line_get_type        (void) G_GNUC_CONST;
static GType    gwy_selection_line_get_type    (void) G_GNUC_CONST;
static void     gwy_layer_line_set_property    (GObject *object,
                                                guint prop_id,
                                                const GValue *value,
                                                GParamSpec *pspec);
static void     gwy_layer_line_get_property    (GObject*object,
                                                guint prop_id,
                                                GValue *value,
                                                GParamSpec *pspec);
static void     gwy_layer_line_draw            (GwyVectorLayer *layer,
                                                GdkDrawable *drawable);
static void     gwy_layer_line_draw_object     (GwyLayerLine *layer,
                                                GdkDrawable *drawable,
                                                gint i);
static void     gwy_layer_line_setup_label     (GwyLayerLine *layer,
                                                GdkDrawable *drawable,
                                                gint i);
static gboolean gwy_layer_line_motion_notify   (GwyVectorLayer *layer,
                                                GdkEventMotion *event);
static gboolean gwy_layer_line_move_line       (GwyLayerLine *layer,
                                                gdouble xreal,
                                                gdouble yreal);
static gboolean gwy_layer_line_button_pressed  (GwyVectorLayer *layer,
                                                GdkEventButton *event);
static gboolean gwy_layer_line_button_released (GwyVectorLayer *layer,
                                                GdkEventButton *event);
static void     gwy_layer_line_set_line_numbers(GwyLayerLine *layer,
                                                gboolean line_numbers);
static void     gwy_layer_line_plugged         (GwyDataViewLayer *layer);
static void     gwy_layer_line_unplugged       (GwyDataViewLayer *layer);
static gint     gwy_layer_line_near_line       (GwyLayerLine *layer,
                                                gdouble xreal,
                                                gdouble yreal);
static gint     gwy_layer_line_near_point      (GwyLayerLine *layer,
                                                gdouble xreal,
                                                gdouble yreal);

/* Allow to express intent. */
#define gwy_layer_line_undraw        gwy_layer_line_draw
#define gwy_layer_line_undraw_object gwy_layer_line_draw_object

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of arbitrary straight lines."),
    "Yeti <yeti@gwyddion.net>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

G_DEFINE_TYPE(GwySelectionLine, gwy_selection_line, GWY_TYPE_SELECTION)
G_DEFINE_TYPE(GwyLayerLine, gwy_layer_line, GWY_TYPE_VECTOR_LAYER)

static gboolean
module_register(const gchar *name)
{
    static GwyLayerFuncInfo func_info = {
        "line",
        0,
    };

    func_info.type = gwy_layer_line_get_type();
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static void
gwy_selection_line_class_init(GwySelectionAxisClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_layer_line_class_init(GwyLayerLinesClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_line_set_property;
    gobject_class->get_property = gwy_layer_line_get_property;

    layer_class->plugged = gwy_layer_line_plugged;
    layer_class->unplugged = gwy_layer_line_unplugged;

    vector_class->selection_type = GWY_TYPE_SELECTION_LINE;
    vector_class->draw = gwy_layer_line_draw;
    vector_class->motion_notify = gwy_layer_line_motion_notify;
    vector_class->button_press = gwy_layer_line_button_pressed;
    vector_class->button_release = gwy_layer_line_button_released;

    g_object_class_install_property(
        gobject_class,
        PROP_LINE_NUMBERS,
        g_param_spec_boolean("line-numbers",
                             "Number lines",
                             "Whether to attach line numbers to the lines.",
                             TRUE,
                             G_PARAM_READWRITE));
}

static void
gwy_layer_line_init(GwyLayerLine *layer)
{
    layer->line_numbers = TRUE;
}

static void
gwy_layer_line_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerLine *layer = GWY_LAYER_LINE(object);

    switch (prop_id) {
        case PROP_LINE_NUMBERS:
        gwy_layer_line_set_line_numbers(layer, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_line_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerLine *layer = GWY_LAYER_LINE(object);

    switch (prop_id) {
        case PROP_LINE_NUMBERS:
        g_value_set_boolean(value, layer->line_numbers);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_line_set_line_numbers(GwyLayerLine *layer,
                                gboolean line_numbers)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_LINE(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (line_numbers == layer->line_numbers)
        return;

    if (parent)
        gwy_layer_line_undraw(vector_layer, parent->window);
    layer->line_numbers = line_numbers;
    if (parent)
        gwy_layer_line_draw(vector_layer, parent->window);
    g_object_notify(G_OBJECT(layer), "line-numbers");
}

static void
gwy_layer_line_draw(GwyVectorLayer *layer,
                     GdkDrawable *drawable)
{
    GwyLayerLine *lines_layer;
    gint i;

    g_return_if_fail(GWY_IS_LAYER_LINE(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    lines_layer = GWY_LAYER_LINE(layer);
    for (i = 0; i < lines_layer->nselected; i++)
        gwy_layer_line_draw_object(lines_layer, drawable, i);
}

static void
gwy_layer_line_draw_object(GwyLayerLine *layer,
                          GdkDrawable *drawable,
                          gint i)
{
    GwyDataView *data_view;
    GwyVectorLayer *vector_layer;
    gint xi0, yi0, xi1, yi1, xt, yt;

    g_return_if_fail(GWY_IS_LAYER_LINE(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    vector_layer = GWY_VECTOR_LAYER(layer);
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    g_return_if_fail(data_view);
    g_return_if_fail(i >= 0 && i < layer->nselected);

    if (!vector_layer->gc)
        gwy_vector_layer_setup_gc(vector_layer);

    gwy_data_view_coords_real_to_xy(data_view,
                                    layer->lines[4*i],
                                    layer->lines[4*i + 1],
                                    &xi0, &yi0);
    gwy_data_view_coords_real_to_xy(data_view,
                                    layer->lines[4*i + 2],
                                    layer->lines[4*i + 3],
                                    &xi1, &yi1);
    gwy_data_view_coords_xy_clamp(data_view, &xi0, &yi0);
    gwy_data_view_coords_xy_clamp(data_view, &xi1, &yi1);
    gdk_draw_line(drawable, vector_layer->gc, xi0, yi0, xi1, yi1);

    if (!layer->line_numbers)
        return;

    gwy_layer_line_setup_label(layer, drawable, i);
    xt = (xi0 + xi1)/2 + 1;
    yt = (yi0 + yi1)/2;
    gdk_draw_drawable(drawable, vector_layer->gc, layer->line_labels[i],
                      0, 0, xt, yt, -1, -1);
}

static void
gwy_layer_line_setup_label(GwyLayerLine *layer,
                            GdkDrawable *drawable,
                            gint i)
{
    GwyVectorLayer *vector_layer;
    PangoRectangle rect;
    GdkPixmap *pixmap;
    GdkGC *gc;
    GdkColor color;
    gchar buffer[8];

    if (GDK_IS_DRAWABLE(layer->line_labels[i]))
        return;

    vector_layer = GWY_VECTOR_LAYER(layer);
    if (!vector_layer->layout) {
        PangoFontDescription *fontdesc;

        vector_layer->layout = gtk_widget_create_pango_layout
                                   (GWY_DATA_VIEW_LAYER(vector_layer)->parent,
                                    "");
        fontdesc = pango_font_description_from_string("Helvetica bold 12");
        pango_layout_set_font_description(vector_layer->layout, fontdesc);
        pango_font_description_free(fontdesc);
    }

    g_snprintf(buffer, sizeof(buffer), "%d", i+1);
    pango_layout_set_text(vector_layer->layout, buffer, -1);
    pango_layout_get_pixel_extents(vector_layer->layout, NULL, &rect);

    layer->line_labels[i] = pixmap = gdk_pixmap_new(drawable,
                                                    rect.width, rect.height,
                                                    -1);
    gc = gdk_gc_new(GDK_DRAWABLE(pixmap));
    gdk_gc_set_function(gc, GDK_COPY);
    color.red = 0;
    color.green = 0;
    color.blue = 0;
    gdk_gc_set_rgb_fg_color (gc, &color);
    gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, rect.width, rect.height);
    gdk_draw_layout(pixmap, vector_layer->gc, -rect.x, -rect.y,
                    vector_layer->layout);
    g_object_unref(gc);
}

static gboolean
gwy_layer_line_motion_notify(GwyVectorLayer *layer,
                              GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerLinesClass *klass;
    GwyLayerLine *lines_layer;
    GdkWindow *window;
    gint x, y, i, j;
    gdouble xreal, yreal;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    lines_layer = GWY_LAYER_LINE(layer);
    i = lines_layer->inear;
    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (lines_layer->button && lines_layer->moving_line)
        return gwy_layer_line_move_line(lines_layer, xreal, yreal);
    if (i > -1
        && xreal == lines_layer->lines[2*i]
        && yreal == lines_layer->lines[2*i + 1])
        return FALSE;

    if (!lines_layer->button) {
        klass = GWY_LAYER_LINE_GET_CLASS(lines_layer);
        j = gwy_layer_line_near_line(lines_layer, xreal, yreal);
        i = gwy_layer_line_near_point(lines_layer, xreal, yreal);
        if (i == -1 && j >= 0)
            gdk_window_set_cursor(window, klass->nearline_cursor);
        else
            gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(lines_layer->inear != -1);
    gwy_layer_line_undraw_object(lines_layer, window, i/2);
    lines_layer->lines[2*i] = xreal;
    lines_layer->lines[2*i + 1] = yreal;
    gwy_layer_line_save(lines_layer, i/2);
    gwy_layer_line_draw_object(lines_layer, window, i/2);
    gwy_vector_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_line_move_line(GwyLayerLine *layer,
                             gdouble xreal, gdouble yreal)
{
    GwyDataView *data_view;
    GdkWindow *window;
    gdouble coords[4];
    gdouble *line;
    gint x, y, i;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;
    g_return_val_if_fail(layer->inear != -1, FALSE);

    i = layer->inear;
    line = layer->lines + 4*i;

    /* compute wanted new coordinates of the first endpoint */
    coords[0] = xreal + layer->lmove_x;
    coords[1] = yreal + layer->lmove_y;
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

    gwy_layer_line_undraw_object(layer, window, i);
    gwy_debug("%d %g %g %g %g", i, coords[0], coords[1], coords[2], coords[3]);
    memcpy(line, coords, 4*sizeof(gdouble));
    gwy_layer_line_save(layer, i);
    gwy_layer_line_draw_object(layer, window, i);
    gwy_vector_layer_updated(GWY_VECTOR_LAYER(layer));

    return FALSE;
}

static gboolean
gwy_layer_line_button_pressed(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerLinesClass *klass;
    GwyLayerLine *lines_layer;
    gint x, y, i, j;
    gdouble xreal, yreal;

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    lines_layer = GWY_LAYER_LINE(layer);
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
    /* handle existing lines */
    klass = GWY_LAYER_LINE_GET_CLASS(lines_layer);
    j = gwy_layer_line_near_line(lines_layer, xreal, yreal);
    i = gwy_layer_line_near_point(lines_layer, xreal, yreal);
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
            gwy_layer_line_undraw_object(lines_layer, window, i/2);
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
        gwy_layer_line_draw_object(lines_layer, window, i/2);
    }
    lines_layer->button = event->button;

    layer->in_selection = TRUE;
    gdk_window_set_cursor(window, klass->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_line_button_released(GwyVectorLayer *layer,
                                GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerLinesClass *klass;
    GwyLayerLine *lines_layer;
    gint x, y, i, j;
    gdouble xreal, yreal;
    gboolean outside;

    lines_layer = GWY_LAYER_LINE(layer);
    if (!lines_layer->button)
        return FALSE;
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    lines_layer->button = 0;
    x = event->x;
    y = event->y;
    i = lines_layer->inear;
    gwy_debug("i = %d", i);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    outside = (event->x != x) || (event->y != y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (lines_layer->moving_line)
        gwy_layer_line_move_line(lines_layer, xreal, yreal);
    else {
        gwy_layer_line_undraw_object(lines_layer, window, i/2);
        lines_layer->lines[2*i] = xreal;
        lines_layer->lines[2*i + 1] = yreal;
        /* XXX this can happen also with rounding errors */
        if (lines_layer->lines[2*i] == lines_layer->lines[2*i + 2]
            && lines_layer->lines[2*i + 1] == lines_layer->lines[2*i + 3]) {
            lines_layer->nselected--;
        }
        else {
            gwy_layer_line_save(lines_layer, i/2);
            gwy_layer_line_draw_object(lines_layer, window, i/2);
        }
        gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
    }

    layer->in_selection = FALSE;
    klass = GWY_LAYER_LINE_GET_CLASS(lines_layer);
    j = gwy_layer_line_near_line(lines_layer, xreal, yreal);
    i = gwy_layer_line_near_point(lines_layer, xreal, yreal);
    if (i == -1 && j >= 0)
        gdk_window_set_cursor(window, klass->nearline_cursor);
    else
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);

    if (lines_layer->nselected == lines_layer->nlines)
        gwy_vector_layer_selection_finished(layer);

    return FALSE;
}

static void
gwy_layer_line_plugged(GwyDataViewLayer *layer)
{
    GwyLayerLineClass *klass;

    gwy_debug("");
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_line_parent_class)->plugged(layer);

    klass = GWY_LAYER_LINE_GET_CLASS(layer);
    gwy_gdk_cursor_new_or_ref(&klass->near_cursor, GDK_DOTBOX);
    gwy_gdk_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);
    gwy_gdk_cursor_new_or_ref(&klass->nearline_cursor, GDK_FLEUR);
}

static void
gwy_layer_line_unplugged(GwyDataViewLayer *layer)
{
    GwyLayerLine *lines_layer;
    GwyLayerLineClass *klass;
    guint i;

    gwy_debug("");
    lines_layer = GWY_LAYER_LINE(layer);

    klass = GWY_LAYER_LINE_GET_CLASS(object);
    gwy_gdk_cursor_free_or_unref(&klass->near_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->move_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->nearline_cursor);

    if (lines_layer->line_labels) {
        for (i = 0; i < lines_layer->line_labels->len; i++)
            gwy_object_unref(g_ptr_array_index(lines_layer->line_labels, i));
        g_ptr_array_free(lines_layer->line_labels, TRUE);
        lines_layer->line_labels = NULL;
    }

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_line_parent_class)->unplugged(layer);
}

static gint
gwy_layer_line_near_line(GwyLayerLine *layer,
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

static gint
gwy_layer_line_near_point(GwyLayerLine *layer,
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
