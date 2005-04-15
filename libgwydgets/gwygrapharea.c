/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include "gwyvectorlayer.h"
#include "gwygrapharea.h"
#include "gwygraphlabel.h"
#include "gwydgetutils.h"

#define GWY_GRAPH_AREA_TYPE_NAME "GwyGraphArea"

enum {
    SELECTED_SIGNAL,
    ZOOMED_SIGNAL,
    LAST_SIGNAL
};

enum {
    COLOR_FG = 0,
    COLOR_BG,
    COLOR_SELECTION,
    COLOR_LAST
};

/* Forward declarations - widget related*/
static void     gwy_graph_area_class_init           (GwyGraphAreaClass *klass);
static void     gwy_graph_area_init                 (GwyGraphArea *area);
static void     gwy_graph_area_finalize             (GObject *object);

static void     gwy_graph_area_realize              (GtkWidget *widget);
static void     gwy_graph_area_unrealize            (GtkWidget *widget);
static void     gwy_graph_area_size_allocate        (GtkWidget *widget, GtkAllocation *allocation);

static gboolean gwy_graph_area_expose               (GtkWidget *widget,
                                                      GdkEventExpose *event);
static gboolean gwy_graph_area_button_press         (GtkWidget *widget,
                                                      GdkEventButton *event);
static gboolean gwy_graph_area_button_release       (GtkWidget *widget,
                                                      GdkEventButton *event);

/* Forward declarations - area related*/
static void     gwy_graph_area_draw_area            (GtkWidget *widget);
static void     gwy_graph_area_draw_curves          (GtkWidget *widget);
static void     gwy_graph_area_draw_selection       (GtkWidget *widget);
static void     gwy_graph_area_draw_selection_points(GtkWidget *widget);
static void     gwy_graph_area_draw_zoom            (GtkWidget *widget);
static void     gwy_graph_area_plot_refresh         (GwyGraphArea *area);

static gdouble  scr_to_data_x                       (GtkWidget *widget, gint scr);
static gdouble  scr_to_data_y                       (GtkWidget *widget, gint scr);
static gint     data_to_scr_x                       (GtkWidget *widget, gdouble data);
static gint     data_to_scr_y                       (GtkWidget *widget, gdouble data);
    


static void     zoom                                (GtkWidget *widget);
/* Local data */


typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
    GtkWidget *widget;
    gint x;
    gint y;
};

const gint N_MAX_POINTS = 10;

static gboolean        gwy_graph_area_motion_notify     (GtkWidget *widget,
                                                        GdkEventMotion *event);
static GtkLayoutChild* gwy_graph_area_find_child        (GwyGraphArea *area,
                                                        gint x,
                                                        gint y);
static void            gwy_graph_area_draw_child_rectangle  (GwyGraphArea *area);
static void            gwy_graph_area_clamp_coords_for_child(GwyGraphArea *area,
                                                        gint *x,
                                                        gint *y);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint gwygrapharea_signals[LAST_SIGNAL] = { 0 };


GType
gwy_graph_area_get_type(void)
{
    static GType gwy_graph_area_type = 0;

    if (!gwy_graph_area_type) {
        static const GTypeInfo gwy_graph_area_info = {
            sizeof(GwyGraphAreaClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_area_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphArea),
            0,
            (GInstanceInitFunc)gwy_graph_area_init,
            NULL,
        };
        gwy_debug("");
        gwy_graph_area_type = g_type_register_static(GTK_TYPE_LAYOUT,
                                                      GWY_GRAPH_AREA_TYPE_NAME,
                                                      &gwy_graph_area_info,
                                                      0);
    }

    return gwy_graph_area_type;
}

static void
gwy_graph_area_class_init(GwyGraphAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;


    gwy_debug("");

    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);
    gobject_class->finalize = gwy_graph_area_finalize;

    widget_class->realize = gwy_graph_area_realize;
    widget_class->unrealize = gwy_graph_area_unrealize;
    widget_class->expose_event = gwy_graph_area_expose;
    widget_class->size_allocate = gwy_graph_area_size_allocate;

    widget_class->button_press_event = gwy_graph_area_button_press;
    widget_class->button_release_event = gwy_graph_area_button_release;
    widget_class->motion_notify_event = gwy_graph_area_motion_notify;

    klass->selected = NULL;
    klass->zoomed = NULL;
    klass->cross_cursor = NULL;
    klass->arrow_cursor = NULL;
    gwygrapharea_signals[SELECTED_SIGNAL]
        = g_signal_new ("selected",
                        G_TYPE_FROM_CLASS (klass),
                        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                        G_STRUCT_OFFSET (GwyGraphAreaClass, selected),
                        NULL,
                        NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);

    gwygrapharea_signals[ZOOMED_SIGNAL]
        = g_signal_new ("zoomed",
                        G_TYPE_FROM_CLASS (klass),
                        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                        G_STRUCT_OFFSET (GwyGraphAreaClass, zoomed),
                        NULL,
                        NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);
}

static void
gwy_graph_area_init(GwyGraphArea *area)
{
    GwyGraphAreaClass *klass;

    gwy_debug("");
    area->gc = NULL;
    area->active = NULL;
    area->x_min = 0;
    area->x_max = 0;
    area->y_min = 0;
    area->y_max = 0;
    area->old_width = 0;
    area->old_height = 0;

    area->selecting = 0;

    area->seldata = g_new(GwyGraphStatus_SelData, 1);
    area->pointsdata = g_new(GwyGraphStatus_PointsData, 1);
    area->cursordata = g_new(GwyGraphStatus_CursorData, 1);
    area->zoomdata = g_new(GwyGraphStatus_ZoomData, 1);

    area->seldata->scr_start = 0;
    area->seldata->scr_end = 0;
    area->seldata->data_start = 0;
    area->seldata->data_end = 0;

    area->pointsdata->scr_points = g_array_new(0, 1, sizeof(GwyGraphScrPoint));
    area->pointsdata->data_points = g_array_new(0, 1, sizeof(GwyGraphDataPoint));
    area->pointsdata->n = 0;

    area->zoomdata->x = 0;
    area->zoomdata->y = 0;
    area->zoomdata->width = 0;
    area->zoomdata->height = 0;

    area->colors = NULL;

    area->lab = GWY_GRAPH_LABEL(gwy_graph_label_new());
    gtk_layout_put(GTK_LAYOUT(area), GTK_WIDGET(area->lab), 90, 90);

    klass = GWY_GRAPH_AREA_GET_CLASS(area);
    gwy_gdk_cursor_new_or_ref(&klass->cross_cursor, GDK_CROSS);
    gwy_gdk_cursor_new_or_ref(&klass->arrow_cursor, GDK_LEFT_PTR);
}

GtkWidget*
gwy_graph_area_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment)
{
    GwyGraphArea *area;

    gwy_debug("");

    area = (GwyGraphArea*)gtk_widget_new(GWY_TYPE_GRAPH_AREA,
                                         "hadjustment", hadjustment,
                                         "vadjustment", vadjustment,
                                         NULL);

    gtk_widget_add_events(GTK_WIDGET(area), GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_BUTTON_MOTION_MASK
                          | GDK_POINTER_MOTION_MASK);

    area->curves = g_ptr_array_new();

    return GTK_WIDGET(area);
}

static void
gwy_graph_area_finalize(GObject *object)
{
    GwyGraphAreaClass *klass;
    GwyGraphArea *area;
    GwyGraphAreaCurve *pcurve;
    gsize i;

    gwy_debug("finalizing a GwyGraphArea (refcount = %u)", object->ref_count);

    g_return_if_fail(GWY_IS_GRAPH_AREA(object));

    area = GWY_GRAPH_AREA(object);

    klass = GWY_GRAPH_AREA_GET_CLASS(area);
    gwy_gdk_cursor_free_or_unref(&klass->cross_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->arrow_cursor);

    for (i = 0; i < area->curves->len; i++)
    {
        pcurve = g_ptr_array_index (area->curves, i);
        g_free(pcurve->data.xvals);
        g_free(pcurve->data.yvals);
        g_free(pcurve->points);
        g_string_free(pcurve->params.description, TRUE);
    }

    g_ptr_array_free(area->curves, TRUE);
    g_array_free(area->pointsdata->scr_points, TRUE);
    g_array_free(area->pointsdata->data_points, TRUE);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}


static void
gwy_graph_area_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraphArea *area;
    GtkAllocation *lab_alloc;
    gwy_debug("");

    area = GWY_GRAPH_AREA(widget);
    lab_alloc = &GTK_WIDGET(area->lab)->allocation;

    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
    if (((area->old_width != widget->allocation.width
          || area->old_height != widget->allocation.height)
         || area->newline == 1)
        && (lab_alloc->x != widget->allocation.width - lab_alloc->width - 5
            || lab_alloc->y != 5)) {
        gtk_layout_move(GTK_LAYOUT(area), GTK_WIDGET(area->lab),
                        widget->allocation.width - lab_alloc->width - 5, 5);
        area->newline = 0;
    }
    gwy_graph_area_plot_refresh(area);

    area->old_width = widget->allocation.width;
    area->old_height = widget->allocation.height;
}

static void
gwy_graph_area_realize(GtkWidget *widget)
{
    GdkColormap *cmap;
    GwyGraphArea *area;
    gboolean success[COLOR_LAST];

    if (GTK_WIDGET_CLASS(parent_class)->realize)
        GTK_WIDGET_CLASS(parent_class)->realize(widget);

    area = GWY_GRAPH_AREA(widget);
    area->gc = gdk_gc_new(GTK_LAYOUT(widget)->bin_window);

    /* FIXME: what about Gtk+ theme??? */
    cmap = gdk_gc_get_colormap(area->gc);
    area->colors = g_new(GdkColor, COLOR_LAST);

    area->colors[COLOR_FG].red = 0x0000;
    area->colors[COLOR_FG].green = 0x0000;
    area->colors[COLOR_FG].blue = 0x0000;

    area->colors[COLOR_BG].red = 0xffff;
    area->colors[COLOR_BG].green = 0xffff;
    area->colors[COLOR_BG].blue = 0xffff;

    area->colors[COLOR_SELECTION].red = 0xaaaa;
    area->colors[COLOR_SELECTION].green = 0x5555;
    area->colors[COLOR_SELECTION].blue = 0xffff;

    /* FIXME: we what to do with @success? */
    gdk_colormap_alloc_colors(cmap, area->colors, COLOR_LAST, FALSE, TRUE,
                              success);
    gdk_gc_set_foreground(area->gc, area->colors + COLOR_FG);
    gdk_gc_set_background(area->gc, area->colors + COLOR_BG);
}

static void
gwy_graph_area_unrealize(GtkWidget *widget)
{
    GwyGraphArea *area;
    GdkColormap *cmap;

    area = GWY_GRAPH_AREA(widget);

    cmap = gdk_gc_get_colormap(area->gc);
    gdk_colormap_free_colors(cmap, area->colors, COLOR_LAST);

    gwy_object_unref(area->gc);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static gboolean
gwy_graph_area_expose(GtkWidget *widget,
                      GdkEventExpose *event)
{
    GwyGraphArea *area;

    gwy_debug("");


    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    area = GWY_GRAPH_AREA(widget);

    gdk_window_clear_area(GTK_LAYOUT (widget)->bin_window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

    gwy_graph_area_draw_selection(widget);
    gwy_graph_area_draw_curves(widget);
    gwy_graph_area_draw_selection_points(widget);
    gwy_graph_area_draw_zoom(widget);
    gwy_graph_area_draw_area(widget);

    gtk_widget_queue_draw(GTK_WIDGET(area->lab));

    GTK_WIDGET_CLASS(parent_class)->expose_event(widget, event);
    return FALSE;
}


static void
gwy_graph_area_draw_area(GtkWidget *widget)
{
    GwyGraphArea *area;

    area = GWY_GRAPH_AREA(widget);

    /*plot boundary*/
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                  0, 0, 0, widget->allocation.height-1);
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                  0, 0, widget->allocation.width-1, 0);
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                  widget->allocation.width-1, 0, widget->allocation.width-1,
                  widget->allocation.height-1);
    gdk_draw_line(GTK_LAYOUT (widget)->bin_window, area->gc,
                  0, widget->allocation.height-1, widget->allocation.width-1,
                  widget->allocation.height-1);

}

static void
gwy_graph_area_draw_curves(GtkWidget *widget)
{
    GwyGraphArea *area;
    GwyGraphAreaCurve *pcurve;
    guint i;
    gint j;

    area = GWY_GRAPH_AREA(widget);

    for (i = 0; i < area->curves->len; i++) {
        pcurve = g_ptr_array_index (area->curves, i);
        gdk_gc_set_foreground(area->gc, &(pcurve->params.color));

        if (pcurve->params.is_line) {
            gdk_gc_set_line_attributes(area->gc, pcurve->params.line_size,
                                       pcurve->params.line_style,
                                       GDK_CAP_ROUND, GDK_JOIN_MITER);
            gdk_draw_lines(GTK_LAYOUT (widget)->bin_window, area->gc,
                           pcurve->points, pcurve->data.N);
        }
        if (pcurve->params.is_point) {
            for (j = 0; j < pcurve->data.N; j++) {
                gwy_graph_draw_point(GTK_LAYOUT(widget)->bin_window, area->gc,
                                     pcurve->points[j].x,
                                     pcurve->points[j].y,
                                     pcurve->params.point_type,
                                     pcurve->params.point_size,
                                     &(pcurve->params.color),
                                     pcurve->params.is_line);
            }
        }
    }
    gdk_gc_set_line_attributes(area->gc, 1,
                               GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
    gdk_gc_set_foreground(area->gc, area->colors + COLOR_FG);
}


static void
gwy_graph_area_draw_selection(GtkWidget *widget)
{
    GwyGraphArea *area;
    gint start, end;

    area = GWY_GRAPH_AREA(widget);

    if (area->status == GWY_GRAPH_STATUS_XSEL
        || area->status == GWY_GRAPH_STATUS_YSEL) {
        if (area->status == GWY_GRAPH_STATUS_XSEL) {
            area->seldata->scr_start = data_to_scr_x(widget, area->seldata->data_start);
            area->seldata->scr_end = data_to_scr_x(widget, area->seldata->data_end);
        }
        else if (area->status == GWY_GRAPH_STATUS_YSEL) {
            area->seldata->scr_start = data_to_scr_y(widget, area->seldata->data_start);
            area->seldata->scr_end = data_to_scr_y(widget, area->seldata->data_end);
        }
        start = area->seldata->scr_start;
        end = area->seldata->scr_end;
        if (start > end)
            GWY_SWAP(gint, start, end);
        else if (start == end)
            return;

        gdk_gc_set_foreground(area->gc, area->colors + COLOR_SELECTION);
        if (area->status == GWY_GRAPH_STATUS_XSEL) {
            gdk_draw_rectangle(GTK_LAYOUT(widget)->bin_window, area->gc, TRUE,
                               start, 0,
                               end-start, widget->allocation.height-1);
        }
        else if (area->status == GWY_GRAPH_STATUS_YSEL) {
            gdk_draw_rectangle(GTK_LAYOUT(widget)->bin_window, area->gc, TRUE,
                               0, start,
                               widget->allocation.width-1, end-start);
        }
        gdk_gc_set_foreground(area->gc, area->colors + COLOR_FG);
    }


}

static void
gwy_graph_area_draw_selection_points(GtkWidget *widget)
{
    GwyGraphArea *area;
    GwyGraphScrPoint scrpnt;
    GwyGraphDataPoint datpnt;
    guint n;

    area = GWY_GRAPH_AREA(widget);

    if (area->status == GWY_GRAPH_STATUS_POINTS) {
        gdk_gc_set_foreground(area->gc, area->colors + COLOR_SELECTION);

        for (n = 0; n < area->pointsdata->data_points->len; n++) {
            datpnt = g_array_index(area->pointsdata->data_points,
                                   GwyGraphDataPoint, n);
            scrpnt.i = data_to_scr_x(widget, datpnt.x);
            scrpnt.j = data_to_scr_y(widget, datpnt.y);
            gdk_draw_line(GTK_LAYOUT(widget)->bin_window, area->gc,
                          scrpnt.i-5,
                          scrpnt.j,
                          scrpnt.i+5,
                          scrpnt.j);
            gdk_draw_line(GTK_LAYOUT(widget)->bin_window, area->gc,
                          scrpnt.i,
                          scrpnt.j-5,
                          scrpnt.i,
                          scrpnt.j+5);
        }
        gdk_gc_set_foreground(area->gc, area->colors + COLOR_FG);
    }
}

static void
gwy_graph_area_draw_zoom(GtkWidget *widget)
{
    GwyGraphArea *area;

    gint x, y;
    area = GWY_GRAPH_AREA(widget);

    if (area->status == GWY_GRAPH_STATUS_ZOOM
        && area->zoomdata->width != 0
        && area->zoomdata->height != 0) {
        gdk_gc_set_function(area->gc, GDK_INVERT);

        if (area->zoomdata->width < 0)
            x = area->zoomdata->x + area->zoomdata->width;
        else
            x = area->zoomdata->x;

        if (area->zoomdata->height < 0)
            y = area->zoomdata->y + area->zoomdata->height;
        else
            y = area->zoomdata->y;

        gdk_draw_rectangle(GTK_LAYOUT(widget)->bin_window, area->gc, 0,
                           x,
                           y,
                           fabs(area->zoomdata->width),
                           fabs(area->zoomdata->height));
        gdk_gc_set_function(area->gc, GDK_COPY);
    }
}

static gboolean
gwy_graph_area_button_press(GtkWidget *widget, GdkEventButton *event)
{
    GwyGraphArea *area;
    GtkLayoutChild *child;
    GwyGraphScrPoint scrpnt;
    GwyGraphDataPoint datpnt;
    gint x, y;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);

    area = GWY_GRAPH_AREA(widget);
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;


    child = gwy_graph_area_find_child(area, x, y);
    if (child) {
        area->active = child->widget;
        area->x0 = x;
        area->y0 = y;
        area->xoff = 0;
        area->yoff = 0;
        gwy_graph_area_draw_child_rectangle(area);
        return FALSE;
    }

    if (area->status == GWY_GRAPH_STATUS_XSEL
        || area->status == GWY_GRAPH_STATUS_YSEL) {
        if (area->status == GWY_GRAPH_STATUS_XSEL) {
            area->seldata->scr_start = x;
            area->seldata->scr_end = x;
            area->seldata->data_start = scr_to_data_x(widget, x);
            area->seldata->data_end = scr_to_data_x(widget, x);
        }
        else if (area->status == GWY_GRAPH_STATUS_YSEL) {
            area->seldata->scr_start = y;
            area->seldata->scr_end = y;
            area->seldata->data_start = scr_to_data_y(widget, y);
            area->seldata->data_end = scr_to_data_y(widget, y);
        }
        area->selecting = 1;
        gwy_graph_area_signal_selected(area);
        gtk_widget_queue_draw(widget);
    }

    if (area->status == GWY_GRAPH_STATUS_POINTS) {
        if (event->button == 1) {
            if (area->pointsdata->n < N_MAX_POINTS) {
                scrpnt.i = x;
                scrpnt.j = y;
                datpnt.x = scr_to_data_x(widget, x);
                datpnt.y = scr_to_data_y(widget, y);
                datpnt.x_unit = NULL;
                datpnt.y_unit = NULL;

                g_array_append_val(area->pointsdata->scr_points, scrpnt);
                g_array_append_val(area->pointsdata->data_points, datpnt);
                area->pointsdata->n++;
            }
        }
        else {
            g_array_free(area->pointsdata->scr_points, 1);
            g_array_free(area->pointsdata->data_points, 1);

            area->pointsdata->scr_points
                = g_array_new(0, 1, sizeof(GwyGraphScrPoint));
            area->pointsdata->data_points
                = g_array_new(0, 1, sizeof(GwyGraphDataPoint));
            area->pointsdata->n = 0;
        }
        gwy_graph_area_signal_selected(area);

        gtk_widget_queue_draw(widget);
    }
    else if (area->status == GWY_GRAPH_STATUS_ZOOM) {
        area->zoomdata->x = x;
        area->zoomdata->y = y;
        area->zoomdata->width = 0;
        area->zoomdata->height = 0;
        area->selecting = 1;
        /*TODO start drawing rectangle*/

    }

    return FALSE;
}

static gboolean
gwy_graph_area_button_release(GtkWidget *widget, GdkEventButton *event)
{
    GwyGraphArea *area;
    gint x, y, ispos;

    gwy_debug("");
    area = GWY_GRAPH_AREA(widget);

    ispos = 0;

    if ((area->status == GWY_GRAPH_STATUS_XSEL
         || area->status == GWY_GRAPH_STATUS_YSEL)
        && area->selecting==1) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        if (area->status == GWY_GRAPH_STATUS_XSEL) {
            area->seldata->scr_end = x;
            area->seldata->data_end = scr_to_data_x(widget, x);
        }
        else if (area->status == GWY_GRAPH_STATUS_YSEL) {
            area->seldata->scr_end = y;
            area->seldata->data_end = scr_to_data_y(widget, y);
        }
        area->selecting = 0;
        gwy_graph_area_signal_selected(area);
        gtk_widget_queue_draw(widget);
    }

    if (area->active) {
        gwy_graph_area_draw_child_rectangle(area);

        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        gwy_graph_area_clamp_coords_for_child(area, &x, &y);
        if (x != area->x0 || y != area->y0) {
            x -= area->x0 - area->active->allocation.x;
            y -= area->y0 - area->active->allocation.y;
            gtk_layout_move(GTK_LAYOUT(area), area->active, x, y);
        }

        area->active = NULL;
    }
    else if (area->status == GWY_GRAPH_STATUS_ZOOM && (area->selecting != 0)) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
         /*TODO delete rectangle*/
        area->zoomdata->width = x - area->zoomdata->x;
        area->zoomdata->height = y - area->zoomdata->y;
        zoom(widget);

        area->zoomdata->x = 0;
        area->zoomdata->y = 0;
        area->zoomdata->width = 0;
        area->zoomdata->height = 0;
        area->selecting = 0;
    }


    return FALSE;
}

static gboolean
gwy_graph_area_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    GwyGraphArea *area;
    GwyGraphAreaClass *klass;
    gint x, y, ispos;

    area = GWY_GRAPH_AREA(widget);

    ispos = 0;

    /*cursor shape*/
    klass = GWY_GRAPH_AREA_GET_CLASS(area);
    if (area->status == GWY_GRAPH_STATUS_ZOOM)
        gdk_window_set_cursor(GTK_LAYOUT(area)->bin_window,
                              klass->cross_cursor);
    else
        gdk_window_set_cursor(GTK_LAYOUT(area)->bin_window,
                              klass->arrow_cursor);


    /*cursor position*/
    if (area->status == GWY_GRAPH_STATUS_CURSOR
        || area->status == GWY_GRAPH_STATUS_POINTS) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        if (area->status == GWY_GRAPH_STATUS_CURSOR) {
            area->cursordata->scr_point.i = x;
            area->cursordata->scr_point.j = y;
            area->cursordata->data_point.x = scr_to_data_x(widget, x);
            area->cursordata->data_point.y = scr_to_data_y(widget, y);
            area->cursordata->data_point.x_unit = NULL;
            area->cursordata->data_point.y_unit = NULL;
        }
        else {
            area->pointsdata->actual_scr_point.i = x;
            area->pointsdata->actual_scr_point.j = y;
            area->pointsdata->actual_data_point.x = scr_to_data_x(widget, x);
            area->pointsdata->actual_data_point.y = scr_to_data_y(widget, y);
            area->pointsdata->actual_data_point.x_unit = NULL;
            area->pointsdata->actual_data_point.y_unit = NULL;
        }
        gwy_graph_area_signal_selected(area);
    }

    if ((area->status == GWY_GRAPH_STATUS_XSEL
         || area->status == GWY_GRAPH_STATUS_YSEL)
        && area->selecting == 1) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        if (area->status == GWY_GRAPH_STATUS_XSEL) {
            area->seldata->scr_end = x;
            area->seldata->data_end = scr_to_data_x(widget, x);
        }
        else if (area->status == GWY_GRAPH_STATUS_YSEL) {
            area->seldata->scr_end = scr_to_data_y(widget, y);
            area->seldata->data_end = 0;
        }
        gwy_graph_area_signal_selected(area);
        gtk_widget_queue_draw(widget);
    }
    else if (area->status == GWY_GRAPH_STATUS_ZOOM
             && (area->selecting != 0)) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
         /*TODO repaint rectangle...*/
        area->zoomdata->width = x - area->zoomdata->x;
        area->zoomdata->height = y - area->zoomdata->y;


        gtk_widget_queue_draw(widget);
     }


    /*widget (label) movement*/
    if (area->active) {

        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        gwy_graph_area_clamp_coords_for_child(area, &x, &y);
        /* don't draw when we can't move */

        if (x - area->x0 == area->xoff
            && y - area->y0 == area->yoff)
            return FALSE;

        gwy_graph_area_draw_child_rectangle(area);
        area->xoff = x - area->x0;
        area->yoff = y - area->y0;
        gwy_graph_area_draw_child_rectangle(area);
    }

    return FALSE;
}

static GtkLayoutChild*
gwy_graph_area_find_child(GwyGraphArea *area, gint x, gint y)
{
    GList *chpl;
    for (chpl = GTK_LAYOUT(area)->children; chpl; chpl = g_list_next(chpl)) {
        GtkLayoutChild *child;
        GtkAllocation *allocation;

        child = (GtkLayoutChild*)chpl->data;
        allocation = &child->widget->allocation;
        if (x >= allocation->x
            && x < allocation->x + allocation->width
            && y >= allocation->y
            && y < allocation->y + allocation->height)
            return child;
    }
    return NULL;
}

static void
gwy_graph_area_clamp_coords_for_child(GwyGraphArea *area,
                                 gint *x,
                                 gint *y)
{
    GtkAllocation *allocation;
    gint min, max;

    allocation = &area->active->allocation;

    min = area->x0 - allocation->x;
    max = GTK_WIDGET(area)->allocation.width
          - (allocation->width - min) - 1;
    *x = CLAMP(*x, min, max);

    min = area->y0 - allocation->y;
    max = GTK_WIDGET(area)->allocation.height
          - (allocation->height - min) - 1;
    *y = CLAMP(*y, min, max);
}

static void
gwy_graph_area_draw_child_rectangle(GwyGraphArea *area)
{
    GtkAllocation *allocation;

    if (!area->active)
        return;

    gdk_gc_set_function(area->gc, GDK_INVERT);
    allocation = &area->active->allocation;
    gdk_draw_rectangle(GTK_LAYOUT(area)->bin_window, area->gc, FALSE,
                       allocation->x + area->xoff,
                       allocation->y + area->yoff,
                       allocation->width,
                       allocation->height);
    gdk_gc_set_function(area->gc, GDK_COPY);
}

/**
 * gwy_graph_area_set_boundaries:
 * @area: graph area
 * @x_min: x minimum
 * @x_max: x maximum
 * @y_min: y minimim
 * @y_max: y maximum
 *
 * Sets the boudaries of graph area and calls for recomputation of
 * actual curve screen representation and its redrawing. used for example by
 * zoom to change curve screen representation.
 **/
void
gwy_graph_area_set_boundaries(GwyGraphArea *area, gdouble x_min, gdouble x_max, gdouble y_min, gdouble y_max)
{
    gwy_debug("");
    area->x_min = x_min;
    area->y_min = y_min;
    area->x_max = x_max;
    area->y_max = y_max;
    gwy_graph_area_plot_refresh(area);
    gtk_widget_queue_draw(GTK_WIDGET(area));
}

/**
 * gwy_graph_area_plot_refresh:
 * @area: graph area
 *
 * Recomputes all data corresponding to actual curve screen representation
 * to be plotted according to window size.
 **/
static void
gwy_graph_area_plot_refresh(GwyGraphArea *area)
{
    guint i;
    gint j;
    GwyGraphAreaCurve *pcurve;
    GtkWidget *widget;

    gwy_debug("");

    widget = GTK_WIDGET(area);
    /*recompute all data to be plotted according to window size*/
    area->x_shift = area->x_min;
    area->y_shift = area->y_min ;
    area->x_multiply = widget->allocation.width/(area->x_max - area->x_min);
    area->y_multiply = widget->allocation.height/(area->y_max - area->y_min);

    for (i=0; i<area->curves->len; i++)
    {
        pcurve = g_ptr_array_index (area->curves, i);
        for (j=0; j<pcurve->data.N; j++)
        {
            pcurve->points[j].x = (pcurve->data.xvals[j] - area->x_shift)*area->x_multiply;
            pcurve->points[j].y = widget->allocation.height - 1 - (pcurve->data.yvals[j] - area->y_shift)*area->y_multiply;
        }
    }
}

/**
 * gwy_graph_area_add_curve:
 * @area: graph area
 * @curve: curve to be added
 *
 * Adds a curve to graph. Adds the curve data values, but the recomputation
 * of actual screen points representing curve must be called after
 * setting the boundaries of the graph area (and complete graph).
 **/
void
gwy_graph_area_add_curve(GwyGraphArea *area, GwyGraphAreaCurve *curve)
{
    gint i;

    GwyGraphAreaCurve *pcurve;

    gwy_debug("");
    /*alloc one element, make deep copy and add it to array*/

    pcurve = g_new(GwyGraphAreaCurve, 1);
    pcurve->data.xvals = (gdouble *) g_try_malloc(curve->data.N*sizeof(gdouble));
    pcurve->data.yvals = (gdouble *) g_try_malloc(curve->data.N*sizeof(gdouble));
    pcurve->points = (GdkPoint *) g_try_malloc(curve->data.N*sizeof(GdkPoint));

    pcurve->params.is_line = curve->params.is_line;
    pcurve->params.is_point = curve->params.is_point;
    pcurve->params.line_style = curve->params.line_style;
    pcurve->params.line_size = curve->params.line_size;
    pcurve->params.point_type = curve->params.point_type;
    pcurve->params.point_size = curve->params.point_size;
    pcurve->params.color = curve->params.color;
    pcurve->params.description = g_string_new(curve->params.description->str);

    pcurve->data.N = curve->data.N;
    for (i=0; i<curve->data.N; i++)
    {
        pcurve->data.xvals[i] = curve->data.xvals[i];
        pcurve->data.yvals[i] = curve->data.yvals[i];
        pcurve->points[i].x = 0;
        pcurve->points[i].y = 0;
    }

    g_ptr_array_add(area->curves, (gpointer)(pcurve));
    gwy_graph_label_add_curve(area->lab, &(pcurve->params));
    area->newline = 1;

}

/**
 * gwy_graph_area_clear:
 * @area: graph area
 *
 * clear graph area
 **/
void
gwy_graph_area_clear(GwyGraphArea *area)
{
    guint i;
    GwyGraphAreaCurve *pcurve;

    gwy_debug("");
            /*dealloc everything*/
    for (i = 0; i < area->curves->len; i++) {
        pcurve = g_ptr_array_index (area->curves, i);
        g_free(pcurve->data.xvals);
        g_free(pcurve->data.yvals);
        g_free(pcurve->points);
        g_string_free(pcurve->params.description, TRUE);
    }

    gwy_graph_label_clear(area->lab);

    g_ptr_array_free(area->curves, 1);
    area->curves = g_ptr_array_new();
}

static gdouble
scr_to_data_x(GtkWidget *widget, gint scr)
{
    GwyGraphArea *area;
    area = GWY_GRAPH_AREA(widget);

    scr = CLAMP(scr, 0, widget->allocation.width-1);
    return area->x_min
           + scr*(area->x_max - area->x_min)/(widget->allocation.width-1);
}

static gint
data_to_scr_x(GtkWidget *widget, gdouble data)
{
    GwyGraphArea *area;
    area = GWY_GRAPH_AREA(widget);

    return (data - area->x_min)
           /((area->x_max - area->x_min)/(widget->allocation.width-1));
}

static gdouble
scr_to_data_y(GtkWidget *widget, gint scr)
{
    GwyGraphArea *area;
    area = GWY_GRAPH_AREA(widget);

    scr = CLAMP(scr, 0, widget->allocation.height-1);
    return area->y_min
           + (widget->allocation.height - scr)*(area->y_max - area->y_min)
             /(widget->allocation.height-1);
}

static gint
data_to_scr_y(GtkWidget *widget, gdouble data)
{
    GwyGraphArea *area;
    area = GWY_GRAPH_AREA(widget);

    return widget->allocation.height
           - (data - area->y_min)
             /((area->y_max - area->y_min)/(widget->allocation.height-1));
}

/**
 * gwy_graph_area_signal_selected:
 * @area: graph area
 *
 * emit signal that something was selected by mouse. "Something" depends on the
 * actual graph status (points, horizontal selection, etc.).
 **/
void
gwy_graph_area_signal_selected(GwyGraphArea *area)
{
    g_signal_emit (G_OBJECT (area), gwygrapharea_signals[SELECTED_SIGNAL], 0);
}

/**
 * gwy_graph_area_signal_zoomed:
 * @area: graph area
 *
 * emit signal that user finished drawing zoom rectangle by mouse.
 **/
void
gwy_graph_area_signal_zoomed(GwyGraphArea *area)
{
    g_signal_emit (G_OBJECT (area), gwygrapharea_signals[ZOOMED_SIGNAL], 0);
}


void
zoom(GtkWidget *widget)
{
    GwyGraphArea *area;
    gdouble x, y, swap;

    area = GWY_GRAPH_AREA(widget);

    if (area->zoomdata->width<0) x = area->zoomdata->x + area->zoomdata->width;
    else x = area->zoomdata->x;
    if (area->zoomdata->height<0) y = area->zoomdata->y + area->zoomdata->height;
    else y = area->zoomdata->y;

    area->zoomdata->xmin = scr_to_data_x(widget, x);
    area->zoomdata->ymin = scr_to_data_y(widget, y);
    area->zoomdata->xmax = scr_to_data_x(widget, x + fabs(area->zoomdata->width));
    area->zoomdata->ymax = scr_to_data_y(widget, y + fabs(area->zoomdata->height));
    swap = area->zoomdata->ymax; area->zoomdata->ymax = area->zoomdata->ymin; area->zoomdata->ymin = swap;

    gwy_graph_area_signal_zoomed(area);
    area->status = GWY_GRAPH_STATUS_PLAIN;
}

void 
gwy_graph_area_set_selection(GwyGraphArea *area, gdouble from, gdouble to)
{
    if (area->status == GWY_GRAPH_STATUS_XSEL
         || area->status == GWY_GRAPH_STATUS_YSEL) {
        if (area->status == GWY_GRAPH_STATUS_XSEL) {
            area->seldata->data_start = from;
            area->seldata->data_end = to;
            area->seldata->scr_start = data_to_scr_x(GTK_WIDGET(area), from);
            area->seldata->scr_end = data_to_scr_x(GTK_WIDGET(area), to);
        }
        else if (area->status == GWY_GRAPH_STATUS_YSEL) {
            area->seldata->data_start = from;
            area->seldata->data_end = to;
            area->seldata->scr_start = data_to_scr_y(GTK_WIDGET(area), from);
            area->seldata->scr_end = data_to_scr_y(GTK_WIDGET(area), to);
         }
        gwy_graph_area_signal_selected(area);
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
