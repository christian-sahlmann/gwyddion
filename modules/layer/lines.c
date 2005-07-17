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
#include <libgwyddion/gwymacros.h>
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwydataview.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwymodule/gwymodule.h>

#define GWY_TYPE_LAYER_LINES            (gwy_layer_lines_get_type())
#define GWY_LAYER_LINES(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_LINES, GwyLayerLines))
#define GWY_LAYER_LINES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_LINES, GwyLayerLinesClass))
#define GWY_IS_LAYER_LINES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_LINES))
#define GWY_IS_LAYER_LINES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_LINES))
#define GWY_LAYER_LINES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_LINES, GwyLayerLinesClass))

#define GWY_LAYER_LINES_TYPE_NAME "GwyLayerLines"

#define PROXIMITY_DISTANCE 8
#define CROSS_SIZE 8

#define BITS_PER_SAMPLE 8

enum {
    PROP_0,
    PROP_MAX_LINES,
    PROP_LINE_NUMBERS,
    PROP_LAST
};

typedef struct _GwyLayerLines      GwyLayerLines;
typedef struct _GwyLayerLinesClass GwyLayerLinesClass;

struct _GwyLayerLines {
    GwyVectorLayer parent_instance;

    gint nlines;
    gint nselected;
    gint inear;
    gboolean moving_line;
    gdouble lmove_x;
    gdouble lmove_y;
    guint button;
    gboolean line_numbers;
    gdouble *lines;
    GdkPixmap **line_labels;
};

struct _GwyLayerLinesClass {
    GwyVectorLayerClass parent_class;

    GdkCursor *near_cursor;
    GdkCursor *nearline_cursor;
    GdkCursor *move_cursor;
};

/* Forward declarations */

static gboolean   module_register                 (const gchar *name);
static GType      gwy_layer_lines_get_type        (void);
static void       gwy_layer_lines_class_init      (GwyLayerLinesClass *klass);
static void       gwy_layer_lines_init            (GwyLayerLines *layer);
static void       gwy_layer_lines_finalize        (GObject *object);
static void       gwy_layer_lines_set_property    (GObject *object,
                                                   guint prop_id,
                                                   const GValue *value,
                                                   GParamSpec *pspec);
static void       gwy_layer_lines_get_property    (GObject*object,
                                                   guint prop_id,
                                                   GValue *value,
                                                   GParamSpec *pspec);
static void       gwy_layer_lines_draw            (GwyVectorLayer *layer,
                                                   GdkDrawable *drawable);
static void       gwy_layer_lines_draw_line       (GwyLayerLines *layer,
                                                   GdkDrawable *drawable,
                                                   gint i);
static void       gwy_layer_lines_setup_label     (GwyLayerLines *layer,
                                                   GdkDrawable *drawable,
                                                   gint i);
static gboolean   gwy_layer_lines_motion_notify   (GwyVectorLayer *layer,
                                                   GdkEventMotion *event);
static gboolean   gwy_layer_lines_do_move_line    (GwyLayerLines *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);
static gboolean   gwy_layer_lines_button_pressed  (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static gboolean   gwy_layer_lines_button_released (GwyVectorLayer *layer,
                                                   GdkEventButton *event);
static void       gwy_layer_lines_set_max_lines   (GwyLayerLines *layer,
                                                   gint nlines);
static void       gwy_layer_lines_set_line_numbers(GwyLayerLines *layer,
                                                   gboolean line_numbers);
static gint       gwy_layer_lines_get_selection   (GwyVectorLayer *layer,
                                                   gdouble *selection);
static void       gwy_layer_lines_unselect        (GwyVectorLayer *layer);
static void       gwy_layer_lines_plugged         (GwyDataViewLayer *layer);
static void       gwy_layer_lines_unplugged       (GwyDataViewLayer *layer);
static void       gwy_layer_lines_save            (GwyLayerLines *layer,
                                                   gint i);
static void       gwy_layer_lines_restore         (GwyLayerLines *layer);

static gint       gwy_layer_lines_near_line       (GwyLayerLines *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);
static gint       gwy_layer_lines_near_point      (GwyLayerLines *layer,
                                                   gdouble xreal,
                                                   gdouble yreal);

/* Allow to express intent. */
#define gwy_layer_lines_undraw      gwy_layer_lines_draw
#define gwy_layer_lines_undraw_line gwy_layer_lines_draw_line

/* Local data */

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Layer allowing selection of arbitrary straight lines."),
    "Yeti <yeti@gwyddion.net>",
    "1.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static GtkObjectClass *parent_class = NULL;

static gboolean
module_register(const gchar *name)
{
    static GwyLayerFuncInfo func_info = {
        "lines",
        0,
    };

    func_info.type = gwy_layer_lines_get_type();
    gwy_layer_func_register(name, &func_info);

    return TRUE;
}

static GType
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
            = g_type_register_static(GWY_TYPE_VECTOR_LAYER,
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
    GwyVectorLayerClass *vector_class = GWY_VECTOR_LAYER_CLASS(klass);

    gwy_debug("");

    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_layer_lines_finalize;
    gobject_class->set_property = gwy_layer_lines_set_property;
    gobject_class->get_property = gwy_layer_lines_get_property;

    layer_class->plugged = gwy_layer_lines_plugged;
    layer_class->unplugged = gwy_layer_lines_unplugged;

    vector_class->draw = gwy_layer_lines_draw;
    vector_class->motion_notify = gwy_layer_lines_motion_notify;
    vector_class->button_press = gwy_layer_lines_button_pressed;
    vector_class->button_release = gwy_layer_lines_button_released;
    vector_class->get_selection = gwy_layer_lines_get_selection;
    vector_class->unselect = gwy_layer_lines_unselect;

    klass->near_cursor = NULL;
    klass->nearline_cursor = NULL;
    klass->move_cursor = NULL;

    g_object_class_install_property(
        gobject_class,
        PROP_MAX_LINES,
        g_param_spec_int("max-lines",
                         "Maximum number of lines",
                         "The maximum number of lines that can be selected",
                         1, 1024, 3,
                         G_PARAM_READABLE | G_PARAM_WRITABLE));
    g_object_class_install_property(
        gobject_class,
        PROP_LINE_NUMBERS,
        g_param_spec_boolean("line-numbers",
                             "Number lines",
                             "Whether to attach line numbers to the lines.",
                             TRUE,
                             G_PARAM_READABLE | G_PARAM_WRITABLE));
}

static void
gwy_layer_lines_init(GwyLayerLines *layer)
{
    GwyLayerLinesClass *klass;

    gwy_debug("");

    klass = GWY_LAYER_LINES_GET_CLASS(layer);
    gwy_gdk_cursor_new_or_ref(&klass->near_cursor, GDK_DOTBOX);
    gwy_gdk_cursor_new_or_ref(&klass->move_cursor, GDK_CROSS);
    gwy_gdk_cursor_new_or_ref(&klass->nearline_cursor, GDK_FLEUR);

    layer->nlines = 3;
    layer->nselected = 0;
    layer->inear = -1;
    layer->lines = g_new(gdouble, 4*layer->nlines);
    layer->line_labels = g_new0(GdkPixmap*, layer->nlines);
    layer->line_numbers = TRUE;
}

static void
gwy_layer_lines_finalize(GObject *object)
{
    GwyLayerLinesClass *klass;
    GwyLayerLines *layer;

    gwy_debug("");

    g_return_if_fail(GWY_IS_LAYER_LINES(object));

    layer = (GwyLayerLines*)object;
    klass = GWY_LAYER_LINES_GET_CLASS(object);
    gwy_gdk_cursor_free_or_unref(&klass->near_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->move_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->nearline_cursor);

    g_free(layer->line_labels);
    g_free(layer->lines);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_layer_lines_set_property(GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerLines *layer = GWY_LAYER_LINES(object);

    switch (prop_id) {
        case PROP_MAX_LINES:
        gwy_layer_lines_set_max_lines(layer, g_value_get_int(value));
        break;

        case PROP_LINE_NUMBERS:
        gwy_layer_lines_set_line_numbers(layer, g_value_get_boolean(value));
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_lines_get_property(GObject*object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    GwyLayerLines *layer = GWY_LAYER_LINES(object);

    switch (prop_id) {
        case PROP_MAX_LINES:
        g_value_set_int(value, layer->nlines);
        break;

        case PROP_LINE_NUMBERS:
        g_value_set_boolean(value, layer->line_numbers);
        break;

        default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gwy_layer_lines_set_max_lines(GwyLayerLines *layer,
                              gint nlines)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;
    gint oldnlines, i;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    g_return_if_fail(nlines > 0 && nlines < 1024);
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (layer->nlines == nlines)
        return;
    oldnlines = layer->nlines;

    if (parent)
        gwy_layer_lines_undraw(vector_layer, parent->window);
    layer->nlines = nlines;
    layer->nselected = MIN(layer->nselected, nlines);
    if (layer->inear >= nlines)
        layer->inear = -1;

    /* resize label array */
    layer->lines = g_renew(gdouble, layer->lines, 4*layer->nlines);
    for (i = nlines; i < oldnlines; i++)
        gwy_object_unref(layer->line_labels[i]);
    layer->line_labels = g_renew(GdkPixmap*, layer->line_labels, layer->nlines);
    for (i = oldnlines; i < nlines; i++)
        layer->line_labels[i] = NULL;

    if (parent)
        gwy_layer_lines_draw(vector_layer, parent->window);
    g_object_notify(G_OBJECT(layer), "max_lines");
}

static void
gwy_layer_lines_set_line_numbers(GwyLayerLines *layer,
                                 gboolean line_numbers)
{
    GwyVectorLayer *vector_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    vector_layer = GWY_VECTOR_LAYER(layer);
    parent = GWY_DATA_VIEW_LAYER(layer)->parent;

    if (line_numbers == layer->line_numbers)
        return;

    if (parent)
        gwy_layer_lines_undraw(vector_layer, parent->window);
    layer->line_numbers = line_numbers;
    if (parent)
        gwy_layer_lines_draw(vector_layer, parent->window);
    g_object_notify(G_OBJECT(layer), "line_numbers");
}

static void
gwy_layer_lines_draw(GwyVectorLayer *layer,
                     GdkDrawable *drawable)
{
    GwyLayerLines *lines_layer;
    gint i;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    g_return_if_fail(GDK_IS_DRAWABLE(drawable));

    lines_layer = GWY_LAYER_LINES(layer);
    for (i = 0; i < lines_layer->nselected; i++)
        gwy_layer_lines_draw_line(lines_layer, drawable, i);
}

static void
gwy_layer_lines_draw_line(GwyLayerLines *layer,
                          GdkDrawable *drawable,
                          gint i)
{
    GwyDataView *data_view;
    GwyVectorLayer *vector_layer;
    gint xi0, yi0, xi1, yi1, xt, yt;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
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

    gwy_layer_lines_setup_label(layer, drawable, i);
    xt = (xi0 + xi1)/2 + 1;
    yt = (yi0 + yi1)/2;
    gdk_draw_drawable(drawable, vector_layer->gc, layer->line_labels[i],
                      0, 0, xt, yt, -1, -1);
}

static void
gwy_layer_lines_setup_label(GwyLayerLines *layer,
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
    gdk_color_black(gdk_gc_get_colormap(gc), &color);
    gdk_gc_set_foreground(gc, &color);
    gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, rect.width, rect.height);
    gdk_draw_layout(pixmap, vector_layer->gc, -rect.x, -rect.y,
                    vector_layer->layout);
    g_object_unref(gc);
}

static gboolean
gwy_layer_lines_motion_notify(GwyVectorLayer *layer,
                              GdkEventMotion *event)
{
    GwyDataView *data_view;
    GwyLayerLinesClass *klass;
    GwyLayerLines *lines_layer;
    GdkWindow *window;
    gint x, y, i, j;
    gdouble xreal, yreal;

    data_view = GWY_DATA_VIEW(GWY_DATA_VIEW_LAYER(layer)->parent);
    window = GTK_WIDGET(data_view)->window;

    lines_layer = GWY_LAYER_LINES(layer);
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
        return gwy_layer_lines_do_move_line(lines_layer, xreal, yreal);
    if (i > -1
        && xreal == lines_layer->lines[2*i]
        && yreal == lines_layer->lines[2*i + 1])
        return FALSE;

    if (!lines_layer->button) {
        klass = GWY_LAYER_LINES_GET_CLASS(lines_layer);
        j = gwy_layer_lines_near_line(lines_layer, xreal, yreal);
        i = gwy_layer_lines_near_point(lines_layer, xreal, yreal);
        if (i == -1 && j >= 0)
            gdk_window_set_cursor(window, klass->nearline_cursor);
        else
            gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);
        return FALSE;
    }

    g_assert(lines_layer->inear != -1);
    gwy_layer_lines_undraw_line(lines_layer, window, i/2);
    lines_layer->lines[2*i] = xreal;
    lines_layer->lines[2*i + 1] = yreal;
    gwy_layer_lines_save(lines_layer, i/2);
    gwy_layer_lines_draw_line(lines_layer, window, i/2);
    gwy_vector_layer_updated(layer);

    return FALSE;
}

static gboolean
gwy_layer_lines_do_move_line(GwyLayerLines *layer,
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

    gwy_layer_lines_undraw_line(layer, window, i);
    gwy_debug("%d %g %g %g %g", i, coords[0], coords[1], coords[2], coords[3]);
    memcpy(line, coords, 4*sizeof(gdouble));
    gwy_layer_lines_save(layer, i);
    gwy_layer_lines_draw_line(layer, window, i);
    gwy_vector_layer_updated(GWY_VECTOR_LAYER(layer));

    return FALSE;
}

static gboolean
gwy_layer_lines_button_pressed(GwyVectorLayer *layer,
                               GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerLinesClass *klass;
    GwyLayerLines *lines_layer;
    gint x, y, i, j;
    gdouble xreal, yreal;

    gwy_debug("");
    if (event->button != 1)
        return FALSE;

    lines_layer = GWY_LAYER_LINES(layer);
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
    klass = GWY_LAYER_LINES_GET_CLASS(lines_layer);
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
            gwy_layer_lines_undraw_line(lines_layer, window, i/2);
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
        gwy_layer_lines_draw_line(lines_layer, window, i/2);
    }
    lines_layer->button = event->button;

    layer->in_selection = TRUE;
    gdk_window_set_cursor(window, klass->move_cursor);

    return FALSE;
}

static gboolean
gwy_layer_lines_button_released(GwyVectorLayer *layer,
                                GdkEventButton *event)
{
    GwyDataView *data_view;
    GdkWindow *window;
    GwyLayerLinesClass *klass;
    GwyLayerLines *lines_layer;
    gint x, y, i, j;
    gdouble xreal, yreal;
    gboolean outside;

    lines_layer = GWY_LAYER_LINES(layer);
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
        gwy_layer_lines_do_move_line(lines_layer, xreal, yreal);
    else {
        gwy_layer_lines_undraw_line(lines_layer, window, i/2);
        lines_layer->lines[2*i] = xreal;
        lines_layer->lines[2*i + 1] = yreal;
        /* XXX this can happen also with rounding errors */
        if (lines_layer->lines[2*i] == lines_layer->lines[2*i + 2]
            && lines_layer->lines[2*i + 1] == lines_layer->lines[2*i + 3]) {
            lines_layer->nselected--;
        }
        else {
            gwy_layer_lines_save(lines_layer, i/2);
            gwy_layer_lines_draw_line(lines_layer, window, i/2);
        }
        gwy_data_view_layer_updated(GWY_DATA_VIEW_LAYER(layer));
    }

    layer->in_selection = FALSE;
    klass = GWY_LAYER_LINES_GET_CLASS(lines_layer);
    j = gwy_layer_lines_near_line(lines_layer, xreal, yreal);
    i = gwy_layer_lines_near_point(lines_layer, xreal, yreal);
    if (i == -1 && j >= 0)
        gdk_window_set_cursor(window, klass->nearline_cursor);
    else
        gdk_window_set_cursor(window, i == -1 ? NULL : klass->near_cursor);

    if (lines_layer->nselected == lines_layer->nlines)
        gwy_vector_layer_selection_finished(layer);

    return FALSE;
}

static gint
gwy_layer_lines_get_selection(GwyVectorLayer *layer,
                              gdouble *selection)
{
    GwyLayerLines *lines_layer;

    g_return_val_if_fail(GWY_IS_LAYER_LINES(layer), 0);
    lines_layer = GWY_LAYER_LINES(layer);

    if (selection && lines_layer->nselected)
        memcpy(selection,
               lines_layer->lines, 4*lines_layer->nselected*sizeof(gdouble));

    return lines_layer->nselected;
}

static void
gwy_layer_lines_unselect(GwyVectorLayer *layer)
{
    GwyLayerLines *lines_layer;
    GtkWidget *parent;

    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    lines_layer = GWY_LAYER_LINES(layer);

    if (lines_layer->nselected == 0)
        return;

    parent = GWY_DATA_VIEW_LAYER(layer)->parent;
    if (parent)
        gwy_layer_lines_undraw(layer, parent->window);
    lines_layer->nselected = 0;
    gwy_layer_lines_save(lines_layer, -1);
}

static void
gwy_layer_lines_plugged(GwyDataViewLayer *layer)
{
    GwyLayerLines *lines_layer;

    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    lines_layer = GWY_LAYER_LINES(layer);

    lines_layer->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->plugged(layer);
    gwy_layer_lines_restore(lines_layer);
    gwy_data_view_layer_updated(layer);
}

static void
gwy_layer_lines_unplugged(GwyDataViewLayer *layer)
{
    GwyLayerLines *lines_layer;
    gint i;

    gwy_debug("");
    g_return_if_fail(GWY_IS_LAYER_LINES(layer));
    lines_layer = GWY_LAYER_LINES(layer);

    for (i = 0; i < lines_layer->nlines; i++)
        gwy_object_unref(lines_layer->line_labels[i]);
    lines_layer->nselected = 0;
    GWY_DATA_VIEW_LAYER_CLASS(parent_class)->unplugged(layer);
}

static void
gwy_layer_lines_save(GwyLayerLines *layer,
                     gint i)
{
    GwyContainer *data;
    gchar key[32];
    gint from, to, n;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    gwy_container_set_int32_by_name(data, "/0/select/lines/nselected",
                                    layer->nselected);
    if (i < 0) {
        from = 0;
        to = layer->nselected - 1;
    }
    else
        from = to = i;

    for (i = from; i <= to; i++) {
        gdouble *coords = layer->lines + 4*i;

        gwy_debug("%d %g %g %g %g",
                  i, coords[0], coords[1], coords[2], coords[3]);
        n = g_snprintf(key, sizeof(key), "/0/select/lines/%d/x0", i);
        gwy_container_set_double_by_name(data, key, coords[0]);
        key[n-2] = 'y';
        gwy_container_set_double_by_name(data, key, coords[1]);
        key[n-1] = '1';
        gwy_container_set_double_by_name(data, key, coords[3]);
        key[n-2] = 'x';
        gwy_container_set_double_by_name(data, key, coords[2]);
    }
}

static void
gwy_layer_lines_restore(GwyLayerLines *layer)
{
    GwyContainer *data;
    GwyDataField *dfield;
    gchar key[24];
    gdouble xreal, yreal;
    gint i, n, nsel;

    data = GWY_DATA_VIEW_LAYER(layer)->data;
    /* TODO Container */
    if (!gwy_container_contains_by_name(data, "/0/select/lines/nselected"))
        return;

    nsel = gwy_container_get_int32_by_name(data, "/0/select/lines/nselected");
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    for (i = layer->nselected = 0;
         i < nsel && layer->nselected < layer->nlines;
         i++) {
        gdouble *coords = layer->lines + 4*layer->nselected;

        n = g_snprintf(key, sizeof(key), "/0/select/lines/%d/x0", i);
        coords[0] = gwy_container_get_double_by_name(data, key);
        key[n-2] = 'y';
        coords[1] = gwy_container_get_double_by_name(data, key);
        key[n-1] = '1';
        coords[3] = gwy_container_get_double_by_name(data, key);
        key[n-2] = 'x';
        coords[2] = gwy_container_get_double_by_name(data, key);
        if (coords[0] >= 0.0 && coords[0] <= xreal
            && coords[1] >= 0.0 && coords[1] <= yreal
            && coords[2] >= 0.0 && coords[2] <= xreal
            && coords[3] >= 0.0 && coords[3] <= yreal)
            layer->nselected++;
    }
}

static gint
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

static gint
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
