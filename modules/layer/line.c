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

/* XXX: This layer uses @selecting field to index _endpoints_ not objects.
 * When whole line is moving, @selecting is imply 2*line_number. */

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
    gboolean restricted;
    gdouble lmove_x;
    gdouble lmove_y;
    GPtrArray *line_labels;
};

struct _GwyLayerLineClass {
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
static void     gwy_layer_line_draw_object     (GwyVectorLayer *layer,
                                                GdkDrawable *drawable,
                                                gint i);
static void     gwy_layer_line_setup_label     (GwyLayerLine *layer,
                                                GdkDrawable *drawable,
                                                gint i);
static gboolean gwy_layer_line_motion_notify   (GwyVectorLayer *layer,
                                                GdkEventMotion *event);
static gboolean gwy_layer_line_move_line       (GwyVectorLayer *layer,
                                                gdouble xreal,
                                                gdouble yreal);
static gboolean gwy_layer_line_button_pressed  (GwyVectorLayer *layer,
                                                GdkEventButton *event);
static gboolean gwy_layer_line_button_released (GwyVectorLayer *layer,
                                                GdkEventButton *event);
static void     gwy_layer_line_set_line_numbers(GwyLayerLine *layer,
                                                gboolean line_numbers);
static void     gwy_layer_line_realize         (GwyDataViewLayer *layer);
static void     gwy_layer_line_unrealize       (GwyDataViewLayer *layer);
static gint     gwy_layer_line_near_line       (GwyVectorLayer *layer,
                                                gdouble xreal,
                                                gdouble yreal);
static gint     gwy_layer_line_near_point      (GwyVectorLayer *layer,
                                                gdouble xreal,
                                                gdouble yreal);
static void     gwy_layer_line_restrict_angle  (GwyDataView *data_view,
                                                gint endpoint,
                                                gint x, gint y,
                                                gdouble *xy);

/* Allow to express intent. */
#define gwy_layer_line_undraw        gwy_layer_line_draw
#define gwy_layer_line_undraw_object gwy_layer_line_draw_object

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of arbitrary straight lines."),
    "Yeti <yeti@gwyddion.net>",
    "2.1",
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
gwy_selection_line_class_init(GwySelectionLineClass *klass)
{
    GwySelectionClass *sel_class = GWY_SELECTION_CLASS(klass);

    sel_class->object_size = OBJECT_SIZE;
}

static void
gwy_layer_line_class_init(GwyLayerLineClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GwyDataViewLayerClass *layer_class = GWY_DATA_VIEW_LAYER_CLASS(klass);
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    gobject_class->set_property = gwy_layer_line_set_property;
    gobject_class->get_property = gwy_layer_line_get_property;

    layer_class->realize = gwy_layer_line_realize;
    layer_class->unrealize = gwy_layer_line_unrealize;

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
gwy_selection_line_init(GwySelectionLine *selection)
{
    g_array_set_size(GWY_SELECTION(selection)->objects, OBJECT_SIZE);
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
    gint i, n;

    g_return_if_fail(GWY_IS_LAYER_LINE(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    n = gwy_selection_get_data(layer->selection, NULL);
    for (i = 0; i < n; i++)
        gwy_layer_line_draw_object(layer, drawable, i);
}

static void
gwy_layer_line_draw_object(GwyVectorLayer *layer,
                           GdkDrawable *drawable,
                           gint i)
{
    GwyDataView *data_view;
    GwyLayerLine *layer_line;
    gint xi0, yi0, xi1, yi1, xt, yt;
    gdouble xy[OBJECT_SIZE];
    gboolean has_object;

    g_return_if_fail(GDK_IS_DRAWABLE(drawable));
    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);

    has_object = gwy_selection_get_object(layer->selection, i, xy);
    g_return_if_fail(has_object);

    gwy_data_view_coords_real_to_xy(data_view, xy[0], xy[1], &xi0, &yi0);
    gwy_data_view_coords_real_to_xy(data_view, xy[2], xy[3], &xi1, &yi1);
    gwy_data_view_coords_xy_clamp(data_view, &xi0, &yi0);
    gwy_data_view_coords_xy_clamp(data_view, &xi1, &yi1);
    gdk_draw_line(drawable, layer->gc, xi0, yi0, xi1, yi1);

    layer_line = GWY_LAYER_LINE(layer);
    if (!layer_line->line_numbers)
        return;

    gwy_layer_line_setup_label(layer_line, drawable, i);
    xt = (xi0 + xi1)/2 + 1;
    yt = (yi0 + yi1)/2;
    gdk_draw_drawable(drawable, layer->gc,
                      g_ptr_array_index(layer_line->line_labels, i),
                      0, 0, xt, yt, -1, -1);
}

static void
gwy_layer_line_setup_label(GwyLayerLine *layer_line,
                           GdkDrawable *drawable,
                           gint i)
{
    GwyVectorLayer *layer;
    PangoRectangle rect;
    GdkPixmap *pixmap;
    GdkGC *gc;
    GdkColor color;
    gchar buffer[8];

    if (!layer_line->line_labels)
        layer_line->line_labels = g_ptr_array_new();

    if (i < layer_line->line_labels->len
        && GDK_IS_DRAWABLE(g_ptr_array_index(layer_line->line_labels, i)))
        return;

    if (i >= layer_line->line_labels->len)
        g_ptr_array_set_size(layer_line->line_labels, i+1);

    layer = GWY_VECTOR_LAYER(layer_line);
    if (!layer->layout) {
        PangoFontDescription *fontdesc;

        layer->layout = gtk_widget_create_pango_layout
                                   (GWY_DATA_VIEW_LAYER(layer)->parent,
                                    "");
        /* FIXME: just set ,bigger and bold`, not particular font */
        fontdesc = pango_font_description_from_string("Helvetica bold 12");
        pango_layout_set_font_description(layer->layout, fontdesc);
        pango_font_description_free(fontdesc);
    }

    g_snprintf(buffer, sizeof(buffer), "%d", i+1);
    pango_layout_set_text(layer->layout, buffer, -1);
    pango_layout_get_pixel_extents(layer->layout, NULL, &rect);

    pixmap = gdk_pixmap_new(drawable, rect.width, rect.height, -1);
    g_ptr_array_index(layer_line->line_labels, i) = pixmap;

    gc = gdk_gc_new(GDK_DRAWABLE(pixmap));
    gdk_gc_set_function(gc, GDK_COPY);
    color.red = 0;
    color.green = 0;
    color.blue = 0;
    gdk_gc_set_rgb_fg_color(gc, &color);
    gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, rect.width, rect.height);
    gdk_draw_layout(pixmap, layer->gc, -rect.x, -rect.y, layer->layout);
    g_object_unref(gc);
}

static gboolean
gwy_layer_line_motion_notify(GwyVectorLayer *layer,
                             GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerLineClass *klass;
    GdkWindow *window;
    gint x, y, i, j;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean restricted;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    i = layer->selecting;

    if (event->is_hint)
        gdk_window_get_pointer(window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    restricted = event->state & GDK_SHIFT_MASK;
    gwy_debug("x = %d, y = %d", x, y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    if (layer->button && GWY_LAYER_LINE(layer)->moving_line)
        return gwy_layer_line_move_line(layer, xreal, yreal);

    if (i > -1)
        gwy_selection_get_object(layer->selection, i/2, xy);
    /* FIXME: here is normally something like
    if (i > -1 && xreal == xy[2] && yreal == xy[3])
        return FALSE;
       but we need to know which endpoint is moving...
     */

    if (!layer->button) {
        j = gwy_layer_line_near_line(layer, xreal, yreal);
        i = gwy_layer_line_near_point(layer, xreal, yreal);
        klass = GWY_LAYER_LINE_GET_CLASS(layer);
        if (i == -1 && j >= 0)
            gdk_window_set_cursor(window, klass->nearline_cursor);
        else
            gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(layer->selecting != -1);
    GWY_LAYER_LINE(layer)->restricted = restricted;
    gwy_layer_line_undraw_object(layer, window, i/2);
    if (restricted)
        gwy_layer_line_restrict_angle(data_view, i % 2,
                                      event->x, event->y, xy);
    else {
        xy[2*(i % 2) + 0] = xreal;
        xy[2*(i % 2) + 1] = yreal;
    }
    gwy_selection_set_object(layer->selection, i/2, xy);
    gwy_layer_line_draw_object(layer, window, i/2);

    return FALSE;
}

static gboolean
gwy_layer_line_move_line(GwyVectorLayer *layer,
                         gdouble xreal, gdouble yreal)
{
    GwyDataView *data_view;
    GdkWindow *window;
    gdouble coords[OBJECT_SIZE], xy[OBJECT_SIZE];
    gint x, y, i;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;
    g_return_val_if_fail(layer->selecting != -1, FALSE);

    i = layer->selecting/2;
    gwy_selection_get_object(layer->selection, i, xy);

    /* calculate wanted new coordinates of the first endpoint */
    coords[0] = xreal + GWY_LAYER_LINE(layer)->lmove_x;
    coords[1] = yreal + GWY_LAYER_LINE(layer)->lmove_y;
    if (coords[0] == xy[0] && coords[1] == xy[1])
        return FALSE;

    /* clamp them to get best possible */
    gwy_data_view_coords_real_to_xy(data_view, coords[0], coords[1], &x, &y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &coords[0], &coords[1]);

    /* calculate wanted new coordinates of the second endpoint */
    coords[2] = (coords[0] - xy[0]) + xy[2];
    coords[3] = (coords[1] - xy[1]) + xy[3];

    if (coords[2] == xy[2] && coords[3] == xy[3])
        return FALSE;

    /* clamp them to get best possible */
    gwy_data_view_coords_real_to_xy(data_view, coords[2], coords[3], &x, &y);
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y, &coords[2], &coords[3]);

    /* recompute the first point */
    coords[0] = (coords[2] - xy[2]) + xy[0];
    coords[1] = (coords[3] - xy[3]) + xy[1];

    if (coords[0] == xy[0] && coords[1] == xy[1])
        return FALSE;

    gwy_layer_line_undraw_object(layer, window, i);
    gwy_debug("%d %g %g %g %g", i, coords[0], coords[1], coords[2], coords[3]);
    gwy_selection_set_object(layer->selection, i, coords);
    gwy_layer_line_draw_object(layer, window, i);

    return FALSE;
}

static gboolean
gwy_layer_line_button_pressed(GwyVectorLayer *layer,
                              GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerLineClass *klass;
    GwyLayerLine *layer_line;
    gint x, y, i, j;
    gdouble xreal, yreal, xy[OBJECT_SIZE];
    gboolean restricted;

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    x = event->x;
    y = event->y;
    restricted = event->state & GDK_SHIFT_MASK;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_debug("[%d,%d]", x, y);
    /* do nothing when we are outside */
    if (x != event->x || y != event->y)
        return FALSE;

    gwy_data_view_coords_xy_to_real(data_view, x, y, &xreal, &yreal);
    layer_line = GWY_LAYER_LINE(layer);

    /* handle existing selection */
    j = gwy_layer_line_near_line(layer, xreal, yreal);
    i = gwy_layer_line_near_point(layer, xreal, yreal);
    if (i == -1 && j >= 0) {
        gwy_selection_get_object(layer->selection, j, xy);
        layer->selecting = i = 2*j;
        gwy_layer_line_undraw_object(layer, window, j);
        layer_line->moving_line = TRUE;
        layer_line->lmove_x = xy[0] - xreal;
        layer_line->lmove_y = xy[1] - yreal;
    }
    else {
        if (i >= 0) {
            layer->selecting = i;
            gwy_layer_line_undraw_object(layer, window, i/2);
            if (restricted)
                gwy_layer_line_restrict_angle(data_view, i % 2,
                                              event->x, event->y, xy);
            else {
                xy[2*(i % 2) + 0] = xreal;
                xy[2*(i % 2) + 1] = yreal;
            }
        }
        else {
            xy[2] = xy[0] = xreal;
            xy[3] = xy[1] = yreal;

            /* add an object, or do nothing when maximum is reached */
            i = -2;
            if (gwy_selection_is_full(layer->selection)) {
                if (gwy_selection_get_max_objects(layer->selection) > 1)
                    return FALSE;
                i = 0;
                gwy_layer_line_undraw_object(layer, window, i/2);
            }
            layer->selecting = 0;    /* avoid "update" signal emission */
            layer->selecting = gwy_selection_set_object(layer->selection, i/2,
                                                        xy);
            /* When we start a new selection, second endpoint is moving */
            layer->selecting = 2*layer->selecting + 1;
        }
    }
    GWY_LAYER_LINE(layer)->restricted = restricted;
    layer->button = event->button;
    gwy_layer_line_draw_object(layer, window, layer->selecting/2);

    klass = GWY_LAYER_LINE_GET_CLASS(layer);
    gdk_window_set_cursor(window, klass->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_line_button_released(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerLineClass *klass;
    gint x, y, i, j;
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
    if (GWY_LAYER_LINE(layer)->moving_line)
        gwy_layer_line_move_line(layer, xreal, yreal);
    else {
        gwy_selection_get_object(layer->selection, i/2, xy);
        gwy_layer_line_undraw_object(layer, window, i/2);
        if (GWY_LAYER_LINE(layer)->restricted)
            gwy_layer_line_restrict_angle(data_view, i % 2,
                                          event->x, event->y, xy);
        else {
            xy[2*(i % 2) + 0] = xreal;
            xy[2*(i % 2) + 1] = yreal;
        }
        /* XXX this can happen also with rounding errors */
        if (xy[0] == xy[2] && xy[1] == xy[3])
            gwy_selection_delete_object(layer->selection, i/2);
        else
            gwy_layer_line_draw_object(layer, window, i/2);
    }

    GWY_LAYER_LINE(layer)->moving_line = FALSE;
    layer->selecting = -1;
    klass = GWY_LAYER_LINE_GET_CLASS(layer);
    j = gwy_layer_line_near_line(layer, xreal, yreal);
    i = gwy_layer_line_near_point(layer, xreal, yreal);
    if (outside)
        gdk_window_set_cursor(window, NULL);
    else if (i == -1 && j >= 0)
        gdk_window_set_cursor(window, klass->nearline_cursor);
    else
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);
    gwy_selection_finished(layer->selection);

    return FALSE;
}

static void
gwy_layer_line_realize(GwyDataViewLayer *layer)
{
    GwyLayerLineClass *klass;

    gwy_debug("");
    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_line_parent_class)->realize(layer);

    klass = GWY_LAYER_LINE_GET_CLASS(layer);
    gwy_gdk_cursor_new_or_ref(&klass->near_cursor, GDK_DOTBOX);
    gwy_gdk_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);
    gwy_gdk_cursor_new_or_ref(&klass->nearline_cursor, GDK_FLEUR);
}

static void
gwy_layer_line_unrealize(GwyDataViewLayer *layer)
{
    GwyLayerLine *lines_layer;
    GwyLayerLineClass *klass;
    guint i;

    gwy_debug("");
    lines_layer = GWY_LAYER_LINE(layer);

    klass = GWY_LAYER_LINE_GET_CLASS(layer);
    gwy_gdk_cursor_free_or_unref(&klass->near_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->move_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->nearline_cursor);

    if (lines_layer->line_labels) {
        for (i = 0; i < lines_layer->line_labels->len; i++)
            gwy_object_unref(g_ptr_array_index(lines_layer->line_labels, i));
        g_ptr_array_free(lines_layer->line_labels, TRUE);
        lines_layer->line_labels = NULL;
    }

    GWY_DATA_VIEW_LAYER_CLASS(gwy_layer_line_parent_class)->unrealize(layer);
}

static gint
gwy_layer_line_near_line(GwyVectorLayer *layer,
                         gdouble xreal, gdouble yreal)
{
    GwyDataView *view;
    gdouble d2min, *xy;
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL)))
        return -1;

    xy = g_newa(gdouble, OBJECT_SIZE*n);
    gwy_selection_get_data(layer->selection, xy);
    i = gwy_math_find_nearest_line(xreal, yreal, &d2min, n, xy);
    if (i == -1)
        return -1;

    view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(view)*gwy_data_view_get_ymeasure(view);

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

static gint
gwy_layer_line_near_point(GwyVectorLayer *layer,
                          gdouble xreal, gdouble yreal)
{
    GwyDataView *view;
    gdouble d2min, *xy;
    gint i, n;

    if (!(n = gwy_selection_get_data(layer->selection, NULL)))
        return -1;

    xy = g_newa(gdouble, OBJECT_SIZE*n);
    gwy_selection_get_data(layer->selection, xy);
    /* note we search for an endpoint, that is a half of a line */
    i = gwy_math_find_nearest_point(xreal, yreal, &d2min, 2*n, xy);

    view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    /* FIXME: this is simply nonsense when x measure != y measure */
    d2min /= gwy_data_view_get_xmeasure(view)*gwy_data_view_get_ymeasure(view);

    if (d2min > PROXIMITY_DISTANCE*PROXIMITY_DISTANCE)
        return -1;
    return i;
}

/**
 * gwy_layer_line_restrict_angle:
 * @data_view: Data view widget to use for coordinate conversion and
 *             restriction.
 * @endpoint: Which endpoint to move (0 for first, 1, for second).
 * @x: Unrestricted screen endpoint x-coordinate.
 * @y: Unrestricted screen endpoint y-coordinate.
 * @xy: Selection object to update.
 *
 * Restricts line endpoint to force line angle to be a multiple of 15 degrees.
 **/
static void
gwy_layer_line_restrict_angle(GwyDataView *data_view,
                              gint endpoint,
                              gint x, gint y,
                              gdouble *xy)
{
    gint length, phi, xb, yb, xx, yy, ept = endpoint;
    gdouble c, s;

    gwy_data_view_coords_real_to_xy(data_view,
                                    xy[2*(1 - ept) + 0], xy[2*(1 - ept) + 1],
                                    &xb, &yb);
    phi = ROUND(atan2(y - yb, x - xb)*12.0/G_PI);
    s = sin(phi*G_PI/12.0);
    c = cos(phi*G_PI/12.0);
    length = ROUND(hypot(x - xb, y - yb));
    if (!length) {
        xy[2*ept + 0] = xy[2*(1 - ept) + 0];
        xy[2*ept + 1] = xy[2*(1 - ept) + 1];
        return;
    }

    x = xx = xb + length*c;
    y = yy = yb + length*s;
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    if (x != xx && y != yy) {
        xx = xb + (y - yb)*c/s;
        yy = yb + (x - xb)*s/c;
        if (hypot(xx - xb, y - yb) < hypot(x - xb, yy - yb))
            x = xx;
        else
            y = yy;
    }
    else if (x == xx && y != yy)
        x = xb + (y - yb)*c/s;
    else if (x != xx && y == yy)
        y = yb + (x - xb)*s/c;
    /* Final clamp to fix eventual rounding errors */
    gwy_data_view_coords_xy_clamp(data_view, &x, &y);
    gwy_data_view_coords_xy_to_real(data_view, x, y,
                                    &xy[2*ept + 0], &xy[2*ept + 1]);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
