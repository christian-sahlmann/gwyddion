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
#include "gwygrapher.h"
#include "gwygraphermodel.h"
#include "gwygraphercurvemodel.h"

#define GWY_GRAPHER_AREA_TYPE_NAME "GwyGrapherArea"

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
static void     gwy_grapher_area_class_init           (GwyGrapherAreaClass *klass);
static void     gwy_grapher_area_init                 (GwyGrapherArea *area);
static void     gwy_grapher_area_finalize             (GObject *object);

static void     gwy_grapher_area_realize              (GtkWidget *widget);
static void     gwy_grapher_area_unrealize            (GtkWidget *widget);
static void     gwy_grapher_area_size_allocate        (GtkWidget *widget, GtkAllocation *allocation);

static gboolean gwy_grapher_area_expose               (GtkWidget *widget,
                                                      GdkEventExpose *event);
static gboolean gwy_grapher_area_button_press         (GtkWidget *widget,
                                                      GdkEventButton *event);
static gboolean gwy_grapher_area_button_release       (GtkWidget *widget,
                                                      GdkEventButton *event);
static gint     gwy_grapher_area_find_curve           (GwyGrapherArea *area, 
                                                      gdouble x, gdouble y);

/* Forward declarations - area related*/
static gdouble  scr_to_data_x                       (GtkWidget *widget, gint scr);
static gdouble  scr_to_data_y                       (GtkWidget *widget, gint scr);
static gint     data_to_scr_x                       (GtkWidget *widget, gdouble data);
static gint     data_to_scr_y                       (GtkWidget *widget, gdouble data);
static void     gwy_grapher_area_entry_cb           (GwyGrapherAreaDialog *dialog,
                                                     gint arg1,
                                                     gpointer user_data);
static void     gwy_grapher_label_entry_cb          (GwyGrapherLabelDialog *dialog,
                                                     gint arg1,
                                                     gpointer user_data);

static void     zoom                                (GtkWidget *widget);
/* Local data */


typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
    GtkWidget *widget;
    gint x;
    gint y;
};

static const gint N_MAX_POINTS = 10;

static gboolean        gwy_grapher_area_motion_notify     (GtkWidget *widget,
                                                        GdkEventMotion *event);
static GtkLayoutChild* gwy_grapher_area_find_child        (GwyGrapherArea *area,
                                                        gint x,
                                                        gint y);
static void            gwy_grapher_area_draw_child_rectangle  (GwyGrapherArea *area);
static void            gwy_grapher_area_clamp_coords_for_child(GwyGrapherArea *area,
                                                        gint *x,
                                                        gint *y);

/* Local data */

static GtkWidgetClass *parent_class = NULL;

static guint gwygrapherarea_signals[LAST_SIGNAL] = { 0 };


GType
gwy_grapher_area_get_type(void)
{
    static GType gwy_grapher_area_type = 0;

    if (!gwy_grapher_area_type) {
        static const GTypeInfo gwy_grapher_area_info = {
            sizeof(GwyGrapherAreaClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_grapher_area_class_init,
            NULL,
            NULL,
            sizeof(GwyGrapherArea),
            0,
            (GInstanceInitFunc)gwy_grapher_area_init,
            NULL,
        };
        gwy_debug("");
        gwy_grapher_area_type = g_type_register_static(GTK_TYPE_LAYOUT,
                                                      GWY_GRAPHER_AREA_TYPE_NAME,
                                                      &gwy_grapher_area_info,
                                                      0);
    }

    return gwy_grapher_area_type;
}

static void
gwy_grapher_area_class_init(GwyGrapherAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;


    gwy_debug("");

    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);
    gobject_class->finalize = gwy_grapher_area_finalize;

    widget_class->realize = gwy_grapher_area_realize;
    widget_class->unrealize = gwy_grapher_area_unrealize;
    widget_class->expose_event = gwy_grapher_area_expose;
    widget_class->size_allocate = gwy_grapher_area_size_allocate;

    widget_class->button_press_event = gwy_grapher_area_button_press;
    widget_class->button_release_event = gwy_grapher_area_button_release;
    widget_class->motion_notify_event = gwy_grapher_area_motion_notify;

    klass->selected = NULL;
    klass->zoomed = NULL;
    klass->cross_cursor = NULL;
    klass->arrow_cursor = NULL;
    gwygrapherarea_signals[SELECTED_SIGNAL]
        = g_signal_new ("selected",
                        G_TYPE_FROM_CLASS (klass),
                        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                        G_STRUCT_OFFSET (GwyGrapherAreaClass, selected),
                        NULL,
                        NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);

    gwygrapherarea_signals[ZOOMED_SIGNAL]
        = g_signal_new ("zoomed",
                        G_TYPE_FROM_CLASS (klass),
                        G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                        G_STRUCT_OFFSET (GwyGrapherAreaClass, zoomed),
                        NULL,
                        NULL,
                        g_cclosure_marshal_VOID__VOID,
                        G_TYPE_NONE, 0);
}

static void
gwy_grapher_area_init(GwyGrapherArea *area)
{
    GwyGrapherAreaClass *klass;

    gwy_debug("");
    area->gc = NULL;
    
    area->selecting = FALSE;

    area->pointdata = g_new(GwyGrapherStatus_PointData, 1);
    area->pointsdata = g_new(GwyGrapherStatus_PointsData, 1);
    area->areadata = g_new(GwyGrapherStatus_AreaData, 1);
    area->areasdata = g_new(GwyGrapherStatus_AreasData, 1);     
    area->cursordata = g_new(GwyGrapherStatus_CursorData, 1);
    area->zoomdata = g_new(GwyGrapherStatus_ZoomData, 1);

    area->colors = NULL;

    area->lab = GWY_GRAPHER_LABEL(gwy_grapher_label_new());
    gtk_layout_put(GTK_LAYOUT(area), GTK_WIDGET(area->lab), 90, 90);

    klass = GWY_GRAPHER_AREA_GET_CLASS(area);
    gwy_vector_layer_cursor_new_or_ref(&klass->cross_cursor, GDK_CROSS);
    gwy_vector_layer_cursor_new_or_ref(&klass->arrow_cursor, GDK_LEFT_PTR);
}

GtkWidget*
gwy_grapher_area_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment)
{
    GwyGrapherArea *area;

    gwy_debug("");

    area = (GwyGrapherArea*)gtk_widget_new(GWY_TYPE_GRAPHER_AREA,
                                         "hadjustment", hadjustment,
                                         "vadjustment", vadjustment,
                                         NULL);

    gtk_widget_add_events(GTK_WIDGET(area), GDK_BUTTON_PRESS_MASK
                          | GDK_BUTTON_RELEASE_MASK
                          | GDK_BUTTON_MOTION_MASK
                          | GDK_POINTER_MOTION_MASK);

    area->area_dialog = gwy_grapher_area_dialog_new();
    g_signal_connect(area->area_dialog, "response",
                     G_CALLBACK(gwy_grapher_area_entry_cb), area);
    area->label_dialog = gwy_grapher_label_dialog_new();
    g_signal_connect(area->label_dialog, "response",
                     G_CALLBACK(gwy_grapher_label_entry_cb), area);
     
    return GTK_WIDGET(area);
}

static void
gwy_grapher_area_finalize(GObject *object)
{
    GwyGrapherAreaClass *klass;
    GwyGrapherArea *area;
    GwyGrapherAreaCurve *pcurve;
    gsize i;

    gwy_debug("finalizing a GwyGrapherArea (refcount = %u)", object->ref_count);

    g_return_if_fail(GWY_IS_GRAPHER_AREA(object));

    area = GWY_GRAPHER_AREA(object);

    klass = GWY_GRAPHER_AREA_GET_CLASS(area);
    gwy_vector_layer_cursor_free_or_unref(&klass->cross_cursor);
    gwy_vector_layer_cursor_free_or_unref(&klass->arrow_cursor);

    gtk_widget_destroy(area->area_dialog);
    gtk_widget_destroy(area->label_dialog);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_grapher_area_adjust_label(GwyGrapherArea *area)
{
    GtkAllocation *lab_alloc;
    lab_alloc = &GTK_WIDGET(area->lab)->allocation;
     
    gtk_layout_move(GTK_LAYOUT(area), GTK_WIDGET(area->lab),
                        GTK_WIDGET(area)->allocation.width - lab_alloc->width - 5, 5);
    area->newline = 0;
}

static void
gwy_grapher_area_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGrapherArea *area;
    GtkAllocation *lab_alloc;
    gwy_debug("");

    area = GWY_GRAPHER_AREA(widget);
    lab_alloc = &GTK_WIDGET(area->lab)->allocation;

    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
    if (((area->old_width != widget->allocation.width
          || area->old_height != widget->allocation.height)
         || area->newline == 1)
        && (lab_alloc->x != widget->allocation.width - lab_alloc->width - 5
            || lab_alloc->y != 5)) {
        gwy_grapher_area_adjust_label(area);
        area->newline = 0;
    }

    area->old_width = widget->allocation.width;
    area->old_height = widget->allocation.height;    
}

static void
gwy_grapher_area_realize(GtkWidget *widget)
{
    GdkColormap *cmap;
    GwyGrapherArea *area;
    gboolean success[COLOR_LAST];

    if (GTK_WIDGET_CLASS(parent_class)->realize)
        GTK_WIDGET_CLASS(parent_class)->realize(widget);

    area = GWY_GRAPHER_AREA(widget);
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
gwy_grapher_area_unrealize(GtkWidget *widget)
{
    GwyGrapherArea *area;
    GdkColormap *cmap;

    area = GWY_GRAPHER_AREA(widget);

    cmap = gdk_gc_get_colormap(area->gc);
    gdk_colormap_free_colors(cmap, area->colors, COLOR_LAST);

    gwy_object_unref(area->gc);

    if (GTK_WIDGET_CLASS(parent_class)->unrealize)
        GTK_WIDGET_CLASS(parent_class)->unrealize(widget);
}



static gboolean
gwy_grapher_area_expose(GtkWidget *widget,
                      GdkEventExpose *event)
{
    GwyGrapherArea *area;

    gwy_debug("");


    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPHER_AREA(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    area = GWY_GRAPHER_AREA(widget);

    gdk_window_clear_area(GTK_LAYOUT (widget)->bin_window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

    gwy_grapher_area_draw_area_on_drawable(GTK_LAYOUT (widget)->bin_window, area->gc,
                                           0, 0, widget->allocation.width, widget->allocation.height,
                                           area);

    gtk_widget_queue_draw(GTK_WIDGET(area->lab));

    GTK_WIDGET_CLASS(parent_class)->expose_event(widget, event);
    return FALSE;
}

void
gwy_grapher_area_draw_area_on_drawable(GdkDrawable *drawable, GdkGC *gc,
                                       gint x, gint y, gint width, gint height,
                                       GwyGrapherArea *area)
{
    gint i;
    GwyGrapherActiveAreaSpecs specs;
    GwyGrapherCurveModel *curvemodel;
    GwyGrapherModel *model;
    GdkColor fg;
    GdkColormap* cmap;

    model = GWY_GRAPHER_MODEL(area->grapher_model);
    cmap = gdk_colormap_get_system();
    specs.xmin = 0;
    specs.ymin = 0;
    specs.height = height;
    specs.width = width;
    specs.real_xmin = model->x_min;
    specs.real_ymin = model->y_min;
    specs.real_width = model->x_max - model->x_min;
    specs.real_height = model->y_max - model->y_min;
    specs.log_x = specs.log_y = FALSE;

    /*printf("x axis: %g ... %g,    y axis: %g ... %g\n", model->x_min, model->x_max, model->y_min, model->y_max);*/
    /*draw continuous selection*/
    if (area->status == GWY_GRAPHER_STATUS_XSEL || area->status == GWY_GRAPHER_STATUS_YSEL)
        gwy_grapher_draw_selection_areas(drawable,
                                         gc, &specs,
                                         (GwyGrapherDataArea *)area->areasdata->data_areas->data, 
                                         area->areasdata->data_areas->len);
    
    for (i=0; i<model->ncurves; i++)
    {
        curvemodel = GWY_GRAPHER_CURVE_MODEL(model->curves[i]);
        gwy_grapher_draw_curve (drawable, gc,
                                &specs, G_OBJECT(curvemodel));
    }

    /*draw discrete selection (points)*/
    if (area->status == GWY_GRAPHER_STATUS_POINTS)
        gwy_grapher_draw_selection_points(drawable,
                                         gc, &specs,
                                         (GwyGrapherDataPoint *)area->pointsdata->data_points->data, 
                                         area->pointsdata->data_points->len);
         
 
    /*draw area boundaries*/
    fg.red = 0;
    fg.green = 0;
    fg.blue = 0;
    gdk_colormap_alloc_color(cmap, &fg, TRUE, TRUE);
    gdk_gc_set_foreground(gc, &fg);
    gdk_draw_line(drawable, gc, 0, 0, width-1, 0);
    gdk_draw_line(drawable, gc, width-1, 0, width-1, height-1);
    gdk_draw_line(drawable, gc, width-1, height-1, 0, height-1);
    gdk_draw_line(drawable, gc, 0, height-1, 0, 0);
                
}


static gboolean
gwy_grapher_area_button_press(GtkWidget *widget, GdkEventButton *event)
{
    GwyGrapherArea *area;
    GwyGrapherModel *gmodel;
    GtkLayoutChild *child;
    GwyGrapherDataPoint datpnt;
    gint x, y, curve;
    gdouble dx, dy;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPHER_AREA(widget), FALSE);

    area = GWY_GRAPHER_AREA(widget);
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
    dx = scr_to_data_x(widget, x);
    dy = scr_to_data_y(widget, y);

    gmodel = GWY_GRAPHER_MODEL(area->grapher_model);

    child = gwy_grapher_area_find_child(area, x, y);
    if (child) {
        if (event->type == GDK_2BUTTON_PRESS)
        {
            gwy_grapher_label_dialog_set_graph_data(area->label_dialog, G_OBJECT(gmodel));
            gtk_widget_show_all(area->label_dialog);
        }
        else
        {
            area->active = child->widget;
            area->x0 = x;
            area->y0 = y;
            area->xoff = 0;
            area->yoff = 0;
            gwy_grapher_area_draw_child_rectangle(area);
        }
        return FALSE;
    }

    if (gmodel->ncurves > 0)
    {
        curve = gwy_grapher_area_find_curve(area, dx, dy);
        if (curve >= 0)
        {
            gwy_grapher_area_dialog_set_curve_data(area->area_dialog, gmodel->curves[curve]);
            gtk_widget_show_all(area->area_dialog);
        }
    }
 

    
/*
    if (area->status == GWY_GRAPHER_STATUS_XSEL
        || area->status == GWY_GRAPHER_STATUS_YSEL) {
        if (area->status == GWY_GRAPHER_STATUS_XSEL) {
            area->seldata->scr_start = x;
            area->seldata->scr_end = x;
            area->seldata->data_start = scr_to_data_x(widget, x);
            area->seldata->data_end = scr_to_data_x(widget, x);
        }
        else if (area->status == GWY_GRAPHER_STATUS_YSEL) {
            area->seldata->scr_start = y;
            area->seldata->scr_end = y;
            area->seldata->data_start = scr_to_data_y(widget, y);
            area->seldata->data_end = scr_to_data_y(widget, y);
        }
        area->selecting = 1;
        gwy_grapher_area_signal_selected(area);
        gtk_widget_queue_draw(widget);
    }

    if (area->status == GWY_GRAPHER_STATUS_POINTS) {
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
                = g_array_new(0, 1, sizeof(GwyGrapherScrPoint));
            area->pointsdata->data_points
                = g_array_new(0, 1, sizeof(GwyGrapherDataPoint));
            area->pointsdata->n = 0;
        }
        gwy_grapher_area_signal_selected(area);

        gtk_widget_queue_draw(widget);
    }
    else if (area->status == GWY_GRAPHER_STATUS_ZOOM) {
        area->zoomdata->x = x;
        area->zoomdata->y = y;
        area->zoomdata->width = 0;
        area->zoomdata->height = 0;
        area->selecting = 1;
        

    }
    */
    return TRUE;
}

static gboolean
gwy_grapher_area_button_release(GtkWidget *widget, GdkEventButton *event)
{
    GwyGrapherArea *area;
    gint x, y, ispos;

    gwy_debug("");
    area = GWY_GRAPHER_AREA(widget);

    
    ispos = 0;

    /*
    if ((area->status == GWY_GRAPHER_STATUS_XSEL
         || area->status == GWY_GRAPHER_STATUS_YSEL)
        && area->selecting==1) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        if (area->status == GWY_GRAPHER_STATUS_XSEL) {
            area->seldata->scr_end = x;
            area->seldata->data_end = scr_to_data_x(widget, x);
        }
        else if (area->status == GWY_GRAPHER_STATUS_YSEL) {
            area->seldata->scr_end = y;
            area->seldata->data_end = scr_to_data_y(widget, y);
        }
        area->selecting = 0;
        gwy_grapher_area_signal_selected(area);
        gtk_widget_queue_draw(widget);
    }
*/
    if (area->active) {
        gwy_grapher_area_draw_child_rectangle(area);

        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        gwy_grapher_area_clamp_coords_for_child(area, &x, &y);
        if (x != area->x0 || y != area->y0) {
            x -= area->x0 - area->active->allocation.x;
            y -= area->y0 - area->active->allocation.y;
            gtk_layout_move(GTK_LAYOUT(area), area->active, x, y);
        }

        area->active = NULL;
    }

    /*
    else if (area->status == GWY_GRAPHER_STATUS_ZOOM && (area->selecting != 0)) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        
        area->zoomdata->width = x - area->zoomdata->x;
        area->zoomdata->height = y - area->zoomdata->y;
        zoom(widget);

        area->zoomdata->x = 0;
        area->zoomdata->y = 0;
        area->zoomdata->width = 0;
        area->zoomdata->height = 0;
        area->selecting = 0;
    }
    */


    return FALSE;
}

static gboolean
gwy_grapher_area_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    GwyGrapherArea *area;
    GwyGrapherAreaClass *klass;
    gint x, y, ispos;

    area = GWY_GRAPHER_AREA(widget);

    
    ispos = 0;

    /*cursor shape
    klass = GWY_GRAPHER_AREA_GET_CLASS(area);
    if (area->status == GWY_GRAPHER_STATUS_ZOOM)
        gdk_window_set_cursor(GTK_LAYOUT(area)->bin_window,
                              klass->cross_cursor);
    else
        gdk_window_set_cursor(GTK_LAYOUT(area)->bin_window,
                              klass->arrow_cursor);

*/
    /*cursor position*/
/*    if (area->status == GWY_GRAPHER_STATUS_CURSOR
        || area->status == GWY_GRAPHER_STATUS_POINTS) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        if (area->status == GWY_GRAPHER_STATUS_CURSOR) {
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
        gwy_grapher_area_signal_selected(area);
    }

    if ((area->status == GWY_GRAPHER_STATUS_XSEL
         || area->status == GWY_GRAPHER_STATUS_YSEL)
        && area->selecting == 1) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        if (area->status == GWY_GRAPHER_STATUS_XSEL) {
            area->seldata->scr_end = x;
            area->seldata->data_end = scr_to_data_x(widget, x);
        }
        else if (area->status == GWY_GRAPHER_STATUS_YSEL) {
            area->seldata->scr_end = scr_to_data_y(widget, y);
            area->seldata->data_end = 0;
        }
        gwy_grapher_area_signal_selected(area);
        gtk_widget_queue_draw(widget);
    }
    else if (area->status == GWY_GRAPHER_STATUS_ZOOM
             && (area->selecting != 0)) {
        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        area->zoomdata->width = x - area->zoomdata->x;
        area->zoomdata->height = y - area->zoomdata->y;


        gtk_widget_queue_draw(widget);
     }

*/
    /*widget (label) movement*/
    if (area->active) {

        if (!ispos) {
            gdk_window_get_position(event->window, &x, &y);
            x += (gint)event->x;
            y += (gint)event->y;
            ispos = 1;
        }
        gwy_grapher_area_clamp_coords_for_child(area, &x, &y);

        if (x - area->x0 == area->xoff
            && y - area->y0 == area->yoff)
            return FALSE;

        gwy_grapher_area_draw_child_rectangle(area);
        area->xoff = x - area->x0;
        area->yoff = y - area->y0;
        gwy_grapher_area_draw_child_rectangle(area);
    }

    return FALSE;
}

static gint
gwy_grapher_area_find_curve(GwyGrapherArea *area, gdouble x, gdouble y)
{
    gdouble dx, dy;
    gint i, j;
    gint closestid;
    gdouble closestdistance, distance=0;
    GwyGrapherCurveModel *curvemodel;
    GwyGrapherModel *model;
 
    closestdistance = G_MAXDOUBLE;
    model = GWY_GRAPHER_MODEL(area->grapher_model);
    for (i=0; i<model->ncurves; i++)
    {
        curvemodel = GWY_GRAPHER_CURVE_MODEL(model->curves[i]);
        for (j=0; j<(curvemodel->n - 1); j++)
        {
            if (curvemodel->xdata[j] <= x && curvemodel->xdata[j + 1] >= x)
            {
                distance = fabs(y - curvemodel->ydata[j] + (x - curvemodel->xdata[j])*
                                (curvemodel->ydata[j + 1] - curvemodel->ydata[j])/
                                (curvemodel->xdata[j + 1] - curvemodel->xdata[j]));
                if (distance < closestdistance) {
                    closestdistance = distance;
                    closestid = i;
                }
                break;
            }
        }
    }
    if (fabs(closestdistance/(model->y_max - model->y_min)) < 0.05) return closestid;
    else return -1;
}

static GtkLayoutChild*
gwy_grapher_area_find_child(GwyGrapherArea *area, gint x, gint y)
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
gwy_grapher_area_clamp_coords_for_child(GwyGrapherArea *area,
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
gwy_grapher_area_draw_child_rectangle(GwyGrapherArea *area)
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


static gdouble
scr_to_data_x(GtkWidget *widget, gint scr)
{
    GwyGrapherArea *area;
    GwyGrapherModel *model;
 
    area = GWY_GRAPHER_AREA(widget);
    model = GWY_GRAPHER_MODEL(area->grapher_model);

    scr = CLAMP(scr, 0, widget->allocation.width-1);
    return model->x_min
           + scr*(model->x_max - model->x_min)/(widget->allocation.width-1);
}

static gint
data_to_scr_x(GtkWidget *widget, gdouble data)
{
    GwyGrapherArea *area;
    GwyGrapherModel *model;
 
    area = GWY_GRAPHER_AREA(widget);
    model = GWY_GRAPHER_MODEL(area->grapher_model);

    return (data - model->x_min)
           /((model->x_max - model->x_min)/(widget->allocation.width-1));
}

static gdouble
scr_to_data_y(GtkWidget *widget, gint scr)
{
    GwyGrapherArea *area;
    GwyGrapherModel *model;
 
    area = GWY_GRAPHER_AREA(widget);
    model = GWY_GRAPHER_MODEL(area->grapher_model);

    scr = CLAMP(scr, 0, widget->allocation.height-1);
    return model->y_min
           + (widget->allocation.height - scr)*(model->y_max - model->y_min)
             /(widget->allocation.height-1);
}

static gint
data_to_scr_y(GtkWidget *widget, gdouble data)
{
    GwyGrapherArea *area;
    GwyGrapherModel *model;
 
    area = GWY_GRAPHER_AREA(widget);
    model = GWY_GRAPHER_MODEL(area->grapher_model);

    return widget->allocation.height
           - (data - model->y_min)
             /((model->y_max - model->y_min)/(widget->allocation.height-1));
}

/**
 * gwy_grapher_area_signal_selected:
 * @area: grapher area
 *
 * emit signal that something was selected by mouse. "Something" depends on the
 * actual grapher status (points, horizontal selection, etc.).
 **/
void
gwy_grapher_area_signal_selected(GwyGrapherArea *area)
{
    g_signal_emit (G_OBJECT (area), gwygrapherarea_signals[SELECTED_SIGNAL], 0);
}

/**
 * gwy_grapher_area_signal_zoomed:
 * @area: grapher area
 *
 * emit signal that user finished drawing zoom rectangle by mouse.
 **/
void
gwy_grapher_area_signal_zoomed(GwyGrapherArea *area)
{
    g_signal_emit (G_OBJECT (area), gwygrapherarea_signals[ZOOMED_SIGNAL], 0);
}


void
zoom(GtkWidget *widget)
{
    GwyGrapherArea *area;
    gdouble x, y, swap;

    area = GWY_GRAPHER_AREA(widget);
/*
    if (area->zoomdata->width<0) x = area->zoomdata->x + area->zoomdata->width;
    else x = area->zoomdata->x;
    if (area->zoomdata->height<0) y = area->zoomdata->y + area->zoomdata->height;
    else y = area->zoomdata->y;

    area->zoomdata->xmin = scr_to_data_x(widget, x);
    area->zoomdata->ymin = scr_to_data_y(widget, y);
    area->zoomdata->xmax = scr_to_data_x(widget, x + fabs(area->zoomdata->width));
    area->zoomdata->ymax = scr_to_data_y(widget, y + fabs(area->zoomdata->height));
    swap = area->zoomdata->ymax; area->zoomdata->ymax = area->zoomdata->ymin; area->zoomdata->ymin = swap;

    gwy_grapher_area_signal_zoomed(area);
    area->status = GWY_GRAPHER_STATUS_PLAIN;
*/
}
/*
void 
gwy_grapher_area_set_selection(GwyGrapherArea *area, gdouble from, gdouble to)
{
    
    if (area->status == GWY_GRAPHER_STATUS_XSEL
         || area->status == GWY_GRAPHER_STATUS_YSEL) {
        if (area->status == GWY_GRAPHER_STATUS_XSEL) {
            area->seldata->data_start = from;
            area->seldata->data_end = to;
            area->seldata->scr_start = data_to_scr_x(GTK_WIDGET(area), from);
            area->seldata->scr_end = data_to_scr_x(GTK_WIDGET(area), to);
        }
        else if (area->status == GWY_GRAPHER_STATUS_YSEL) {
            area->seldata->data_start = from;
            area->seldata->data_end = to;
            area->seldata->scr_start = data_to_scr_y(GTK_WIDGET(area), from);
            area->seldata->scr_end = data_to_scr_y(GTK_WIDGET(area), to);
         }
        gwy_grapher_area_signal_selected(area);
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }
    
}
*/


void 
gwy_grapher_area_refresh(GwyGrapherArea *area)
{
    /*recompute curve points*/
    
    /*refresh label*/
    gwy_grapher_label_refresh(area->lab);
    /*re-adjust label position*/
    gwy_grapher_area_adjust_label(area);

    /*repaint area data*/
    gtk_widget_queue_draw(GTK_WIDGET(area));
}

void
gwy_grapher_area_change_model(GwyGrapherArea *area, gpointer gmodel)
{
    area->grapher_model = gmodel;
    gwy_grapher_label_change_model(area->lab, gmodel);
}

static void     
gwy_grapher_area_entry_cb(GwyGrapherAreaDialog *dialog, gint arg1, gpointer user_data)
{
    if (arg1 == GTK_RESPONSE_APPLY) {
        gwy_grapher_area_refresh(GWY_GRAPHER_AREA(user_data));
    }
    else if (arg1 == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
}

static void     
gwy_grapher_label_entry_cb(GwyGrapherLabelDialog *dialog, gint arg1, gpointer user_data)
{
    if (arg1 == GTK_RESPONSE_APPLY) {
        gwy_grapher_area_refresh(GWY_GRAPHER_AREA(user_data));
    }
    else if (arg1 == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
}




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
