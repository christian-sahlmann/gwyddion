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

#include "config.h"
#include <math.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include "gwyvectorlayer.h"
#include "gwygraph.h"
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"
#include "gwydgetutils.h"
#include "gwygraphselections.h"

enum {
    SELECTED_SIGNAL,
    ZOOMED_SIGNAL,
    MOUSE_MOVED_SIGNAL,
    LAST_SIGNAL
};

enum {
    COLOR_FG = 0,
    COLOR_BG,
    COLOR_SELECTION,
    COLOR_LAST
};

/* Forward declarations - widget related*/
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
static gboolean gwy_graph_area_leave_notify         (GtkWidget *widget,
                                                      GdkEventCrossing *event);

static gint     gwy_graph_area_find_curve           (GwyGraphArea *area,
                                                      gdouble x, gdouble y);
static gint     gwy_graph_area_find_selection       (GwyGraphArea *area,
                                                      gdouble x, gdouble y);
static gint     gwy_graph_area_find_point           (GwyGraphArea *area,
                                                      gdouble x, gdouble y);
static gint     gwy_graph_area_find_line            (GwyGraphArea *area,
                                                      gdouble position);
static void     gwy_graph_area_draw_zoom            (GdkDrawable *drawable,
                                                       GdkGC *gc,
                                                       GwyGraphArea *area);
static void     gwy_graph_area_signal_selected      (GwyGraphArea *area);
static void     gwy_graph_area_signal_zoomed        (GwyGraphArea *area);

/* Forward declarations - area related*/
static gdouble  scr_to_data_x                       (GtkWidget *widget, gint scr);
static gdouble  scr_to_data_y                       (GtkWidget *widget, gint scr);
static gint     data_to_scr_x                       (GtkWidget *widget, gdouble data);
static gint     data_to_scr_y                       (GtkWidget *widget, gdouble data);
static void     gwy_graph_area_entry_cb           (GwyGraphAreaDialog *dialog,
                                                     gint arg1,
                                                     gpointer user_data);
static void     gwy_graph_label_entry_cb          (GwyGraphLabelDialog *dialog,
                                                     gint arg1,
                                                     gpointer user_data);

/*static void     zoom                                (GtkWidget *widget);*/
/* Local data */


typedef struct _GtkLayoutChild   GtkLayoutChild;

struct _GtkLayoutChild {
    GtkWidget *widget;
    gint x;
    gint y;
};

static const gint N_MAX_POINTS = 10;

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

static guint gwygrapharea_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyGraphArea, gwy_graph_area, GTK_TYPE_LAYOUT)

static void
gwy_graph_area_class_init(GwyGraphAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;

    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_graph_area_finalize;

    widget_class->realize = gwy_graph_area_realize;
    widget_class->unrealize = gwy_graph_area_unrealize;
    widget_class->expose_event = gwy_graph_area_expose;
    widget_class->size_allocate = gwy_graph_area_size_allocate;

    widget_class->button_press_event = gwy_graph_area_button_press;
    widget_class->button_release_event = gwy_graph_area_button_release;
    widget_class->motion_notify_event = gwy_graph_area_motion_notify;
    widget_class->leave_notify_event = gwy_graph_area_leave_notify;

    klass->selected = NULL;
    klass->zoomed = NULL;
    klass->mouse_moved = NULL;
    klass->cross_cursor = NULL;
    klass->arrow_cursor = NULL;
    gwygrapharea_signals[SELECTED_SIGNAL]
        = g_signal_new("selected",
                       G_TYPE_FROM_CLASS (klass),
                       G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET(GwyGraphAreaClass, selected),
                       NULL,
                       NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    gwygrapharea_signals[ZOOMED_SIGNAL]
        = g_signal_new("zoomed",
                       G_TYPE_FROM_CLASS (klass),
                       G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET(GwyGraphAreaClass, zoomed),
                       NULL,
                       NULL,
                       g_cclosure_marshal_VOID__VOID,
                       G_TYPE_NONE, 0);

    gwygrapharea_signals[MOUSE_MOVED_SIGNAL]
        = g_signal_new("mouse-moved",
                       G_TYPE_FROM_CLASS (klass),
                       G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                       G_STRUCT_OFFSET(GwyGraphAreaClass, mouse_moved),
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

    area->selecting = FALSE;
    area->mouse_present = FALSE;

    area->pointsdata = g_object_new(GWY_TYPE_SELECTION_GRAPH_POINT, NULL);
    gwy_selection_set_max_objects(GWY_SELECTION(area->pointsdata), 10);
    g_signal_connect_swapped(GWY_SELECTION(area->pointsdata), "changed",
                     G_CALLBACK(gwy_graph_area_signal_selected), area);

    area->areasdata = g_object_new(GWY_TYPE_SELECTION_GRAPH_AREA, NULL);
    gwy_selection_set_max_objects(GWY_SELECTION(area->areasdata), 10);
    g_signal_connect_swapped(GWY_SELECTION(area->areasdata), "changed",
                     G_CALLBACK(gwy_graph_area_signal_selected), area);

    area->linesdata = g_object_new(GWY_TYPE_SELECTION_GRAPH_LINE, NULL);
    gwy_selection_set_max_objects(GWY_SELECTION(area->linesdata), 10);
    g_signal_connect_swapped(GWY_SELECTION(area->linesdata), "changed",
                     G_CALLBACK(gwy_graph_area_signal_selected), area);

    area->zoomdata = g_new(GwyGraphStatus_ZoomData, 1);
    area->actual_cursor_data = g_new(GwyGraphStatus_CursorData, 1);

    area->x_grid_data = g_array_new(FALSE, FALSE, sizeof(gdouble));
    area->y_grid_data = g_array_new(FALSE, FALSE, sizeof(gdouble));


    area->colors = NULL;
    area->enable_user_input = TRUE;
    gwy_graph_area_set_selection_limit(area, 10);

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
                          | GDK_POINTER_MOTION_MASK
                          | GDK_LEAVE_NOTIFY_MASK);

    area->area_dialog = GWY_GRAPH_AREA_DIALOG(gwy_graph_area_dialog_new());
    g_signal_connect(area->area_dialog, "response",
                     G_CALLBACK(gwy_graph_area_entry_cb), area);

    area->label_dialog = GWY_GRAPH_LABEL_DIALOG(gwy_graph_label_dialog_new());
    g_signal_connect(area->label_dialog, "response",
                     G_CALLBACK(gwy_graph_label_entry_cb), area);

    return GTK_WIDGET(area);
}

static void
gwy_graph_area_finalize(GObject *object)
{
    GwyGraphAreaClass *klass;
    GwyGraphArea *area;

    gwy_debug("finalizing a GwyGraphArea (refcount = %u)", object->ref_count);

    g_return_if_fail(GWY_IS_GRAPH_AREA(object));

    area = GWY_GRAPH_AREA(object);

    klass = GWY_GRAPH_AREA_GET_CLASS(area);
    gwy_gdk_cursor_free_or_unref(&klass->cross_cursor);
    gwy_gdk_cursor_free_or_unref(&klass->arrow_cursor);

    gtk_widget_destroy(GTK_WIDGET(area->area_dialog));
    gtk_widget_destroy(GTK_WIDGET(area->label_dialog));


    G_OBJECT_CLASS(gwy_graph_area_parent_class)->finalize(object);
}

static void
gwy_graph_area_adjust_label(GwyGraphArea *area)
{
    /*GtkAllocation *lab_alloc;
    lab_alloc = &GTK_WIDGET(area->lab)->allocation;*/
    gtk_layout_move(GTK_LAYOUT(area), GTK_WIDGET(area->lab),
                    GTK_WIDGET(area)->allocation.width - area->lab->reqwidth - 5, 5);
    area->newline = 0;
}

static void
gwy_graph_area_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraphArea *area;
    GtkAllocation *lab_alloc;
    gwy_debug("");

    area = GWY_GRAPH_AREA(widget);
    lab_alloc = &GTK_WIDGET(area->lab)->allocation;

    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->size_allocate(widget,
                                                                 allocation);
    if (((area->old_width != widget->allocation.width
          || area->old_height != widget->allocation.height)
         || area->newline == 1)
        && (lab_alloc->x != widget->allocation.width - lab_alloc->width - 5
            || lab_alloc->y != 5)) {
        gwy_graph_area_adjust_label(area);
        area->newline = 0;
    }

    area->old_width = widget->allocation.width;
    area->old_height = widget->allocation.height;
}

static void
gwy_graph_area_realize(GtkWidget *widget)
{
    GdkColormap *cmap;
    GwyGraphArea *area;
    gboolean success[COLOR_LAST];

    if (GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->realize)
        GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->realize(widget);

    area = GWY_GRAPH_AREA(widget);
    area->gc = gdk_gc_new(GTK_LAYOUT(widget)->bin_window);

    cmap = gdk_gc_get_colormap(area->gc);
    area->colors = g_new(GdkColor, COLOR_LAST);

    /* FIXME: use Gtk+ theme */
    area->colors[COLOR_FG].red = 0x0000;
    area->colors[COLOR_FG].green = 0x0000;
    area->colors[COLOR_FG].blue = 0x0000;

    /* FIXME: use Gtk+ theme */
    area->colors[COLOR_BG].red = 0xffff;
    area->colors[COLOR_BG].green = 0xffff;
    area->colors[COLOR_BG].blue = 0xffff;

    /* FIXME: use Gtk+ theme */
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

    area = GWY_GRAPH_AREA(widget);

    gwy_object_unref(area->gc);

    if (GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->unrealize(widget);
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

    gwy_graph_area_draw_area_on_drawable(GTK_LAYOUT (widget)->bin_window, area->gc,
                                           0, 0, widget->allocation.width, widget->allocation.height,
                                           area);

    if (area->status == GWY_GRAPH_STATUS_ZOOM
                                     && (area->selecting != 0))
                gwy_graph_area_draw_zoom(GTK_LAYOUT (widget)->bin_window, area->gc, area);

    gtk_widget_queue_draw(GTK_WIDGET(area->lab));

    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->expose_event(widget, event);
    return FALSE;
}

void
gwy_graph_area_draw_area_on_drawable(GdkDrawable *drawable, GdkGC *gc,
                                       gint x, gint y, gint width, gint height,
                                       GwyGraphArea *area)
{
    gint nc, i;
    GwyGraphActiveAreaSpecs specs;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;
    GdkColor fg;

    model = GWY_GRAPH_MODEL(area->graph_model);
    specs.xmin = x;
    specs.ymin = y;
    specs.height = height;
    specs.width = width;
    specs.real_xmin = model->x_min;
    specs.real_ymin = model->y_min;
    specs.real_width = model->x_max - model->x_min;
    specs.real_height = model->y_max - model->y_min;
    specs.log_x = model->x_is_logarithmic;
    specs.log_y = model->y_is_logarithmic;
    /*draw continuous selection*/
    if (area->status == GWY_GRAPH_STATUS_XSEL
        || area->status == GWY_GRAPH_STATUS_YSEL)
        gwy_graph_draw_selection_areas(drawable,
                                       gc, &specs,
                                       area->areasdata);


    /*FIXME gc should be different and should be set to gray drawing*/
    gwy_graph_draw_grid(drawable, gc, &specs,
                        area->x_grid_data, area->y_grid_data);

    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve_by_index(model, i);
        gwy_graph_draw_curve(drawable, gc,
                             &specs, G_OBJECT(curvemodel));
    }

    /*draw discrete selection (points)*/
    if (area->status == GWY_GRAPH_STATUS_POINTS
        || area->status == GWY_GRAPH_STATUS_ZOOM)
        gwy_graph_draw_selection_points(drawable,
                                         gc, &specs,
                                         area->pointsdata);

    /*draw discrete selection (points)*/
    if (area->status == GWY_GRAPH_STATUS_XLINES)
        gwy_graph_draw_selection_lines(drawable,
                                         gc, &specs,
                                         area->linesdata,
                                         GTK_ORIENTATION_VERTICAL);

    /*draw discrete selection (points)*/
    if (area->status == GWY_GRAPH_STATUS_YLINES)
        gwy_graph_draw_selection_lines(drawable,
                                         gc, &specs,
                                         area->linesdata,
                                         GTK_ORIENTATION_HORIZONTAL);

    /*draw area boundaries*/
    /* FIXME: use Gtk+ theme */
    fg.red = fg.green = fg.blue = 0;
    gdk_gc_set_rgb_fg_color(gc, &fg);
    gdk_gc_set_line_attributes(gc, 1,
                                GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
    gdk_draw_line(drawable, gc, x, y, x + width-1, y);
    gdk_draw_line(drawable, gc, x + width-1, y, x + width-1, y + height-1);
    gdk_draw_line(drawable, gc, x + width-1, y + height-1, x, y + height-1);
    gdk_draw_line(drawable, gc, x, y + height-1, x, y);

}

static void
gwy_graph_area_draw_zoom(GdkDrawable *drawable, GdkGC *gc, GwyGraphArea *area)
{
    gint xmin, ymin, xmax, ymax;

    if (area->zoomdata->width == 0 || area->zoomdata->height == 0) return;
    gdk_gc_set_function(gc, GDK_INVERT);

    if (area->zoomdata->width < 0)
    {
        xmin = data_to_scr_x(GTK_WIDGET(area), area->zoomdata->xmin + area->zoomdata->width);
        xmax = data_to_scr_x(GTK_WIDGET(area), area->zoomdata->xmin);
    }
    else
    {
        xmin = data_to_scr_x(GTK_WIDGET(area), area->zoomdata->xmin);
        xmax = data_to_scr_x(GTK_WIDGET(area), area->zoomdata->xmin + area->zoomdata->width);
    }

    if (area->zoomdata->height > 0)
    {
        ymin = data_to_scr_y(GTK_WIDGET(area), area->zoomdata->ymin + area->zoomdata->height);
        ymax = data_to_scr_y(GTK_WIDGET(area), area->zoomdata->ymin);
    }
    else
    {
        ymin = data_to_scr_y(GTK_WIDGET(area), area->zoomdata->ymin);
        ymax = data_to_scr_y(GTK_WIDGET(area), area->zoomdata->ymin + area->zoomdata->height);
    }

    gdk_draw_rectangle(drawable, area->gc, 0,
                       xmin,
                       ymin,
                       xmax - xmin,
                       ymax - ymin);

    gdk_gc_set_function(area->gc, GDK_COPY);
}



static gboolean
gwy_graph_area_button_press(GtkWidget *widget, GdkEventButton *event)
{
    GwyGraphArea *area;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *cmodel;
    GtkLayoutChild *child;
    gint x, y, curve, selection, nc;
    gdouble selection_data[2];
    gdouble selection_areadata[4];
    gdouble dx, dy;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);

    area = GWY_GRAPH_AREA(widget);
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
    dx = scr_to_data_x(widget, x);
    dy = scr_to_data_y(widget, y);

    gmodel = GWY_GRAPH_MODEL(area->graph_model);
    nc = gwy_graph_model_get_n_curves(gmodel);
    child = gwy_graph_area_find_child(area, x, y);
    if (child) {
        if (event->type == GDK_2BUTTON_PRESS && area->enable_user_input == TRUE)
        {
            gwy_graph_label_dialog_set_graph_data(GTK_WIDGET(area->label_dialog), G_OBJECT(gmodel));
            gtk_widget_show_all(GTK_WIDGET(area->label_dialog));
        }
        else
        {
            area->active = child->widget;
            area->x0 = x;
            area->y0 = y;
            area->xoff = 0;
            area->yoff = 0;
            gwy_graph_area_draw_child_rectangle(area);
        }
        return FALSE;
    }

    if (area->status == GWY_GRAPH_STATUS_PLAIN && nc > 0
        && area->enable_user_input == TRUE) {
        curve = gwy_graph_area_find_curve(area, dx, dy);
        if (curve >= 0) {
            cmodel = gwy_graph_model_get_curve_by_index(gmodel, curve);
            gwy_graph_area_dialog_set_curve_data(GTK_WIDGET(area->area_dialog),
                                                 G_OBJECT(cmodel));
            gtk_widget_show_all(GTK_WIDGET(area->area_dialog));
        }
    }

    if (area->status == GWY_GRAPH_STATUS_POINTS)
    {
        if (event->button == 1 && !gwy_selection_is_full(GWY_SELECTION(area->pointsdata))) /*add selection*/
        {
            selection_data[0] = dx;
            selection_data[1] = dy;
            area->selecting = TRUE;
            gwy_selection_set_object(GWY_SELECTION(area->pointsdata), -1, selection_data);
        }
        else
        {
            selection = gwy_graph_area_find_point(area, dx, dy);
            if (selection >= 0)
                gwy_selection_delete_object(GWY_SELECTION(area->pointsdata), selection);
            gwy_selection_finished(GWY_SELECTION(area->pointsdata));
        }
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->status == GWY_GRAPH_STATUS_XSEL || area->status == GWY_GRAPH_STATUS_YSEL)
    {
        if (event->button == 1 && !gwy_selection_is_full(GWY_SELECTION(area->areasdata))) /*add selection*/
        {
            if (area->status == GWY_GRAPH_STATUS_XSEL)
            {
                selection_areadata[0] = dx;
                selection_areadata[2] = dx;
                selection_areadata[1] = gmodel->y_min;
                selection_areadata[3] = gmodel->y_max;
            }
            else
            {
                selection_areadata[0] = gmodel->x_min;
                selection_areadata[2] = gmodel->x_max;
                selection_areadata[1] = dy;
                selection_areadata[3] = dy;
            }
            gwy_selection_set_object(GWY_SELECTION(area->areasdata), -1, selection_areadata);
            area->selecting = TRUE;
        }
        else /*remove selection*/
        {
            selection = gwy_graph_area_find_selection(area, dx, dy);
            if (selection >= 0)
                gwy_selection_delete_object(GWY_SELECTION(area->areasdata), selection);
            gwy_selection_finished(GWY_SELECTION(area->areasdata));
        }
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->status == GWY_GRAPH_STATUS_XLINES)
    {
        if (event->button == 1 && !gwy_selection_is_full(GWY_SELECTION(area->linesdata))) /*add selection*/
        {
            gwy_selection_set_object(GWY_SELECTION(area->linesdata), -1, &dx);
            area->selecting = TRUE;
        }
        else
        {
            selection = gwy_graph_area_find_line(area, dx);
            if (selection >= 0)
                gwy_selection_delete_object(GWY_SELECTION(area->linesdata), selection);
        }
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->status == GWY_GRAPH_STATUS_YLINES)
    {
        if (event->button == 1 && !gwy_selection_is_full(GWY_SELECTION(area->linesdata))) /*add selection*/
        {
            gwy_selection_set_object(GWY_SELECTION(area->linesdata), -1, &dy);
            area->selecting = TRUE;
        }
        else
        {
            selection = gwy_graph_area_find_line(area, dy);
            if (selection >= 0)
                gwy_selection_delete_object(GWY_SELECTION(area->linesdata), selection);
        }
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }



    if (area->status == GWY_GRAPH_STATUS_ZOOM)
    {
        area->zoomdata->xmin = dx;
        area->zoomdata->ymin = dy;
        area->zoomdata->width = 0;
        area->zoomdata->height = 0;
        area->selecting = 1;
    }


    return TRUE;
}

static gboolean
gwy_graph_area_button_release(GtkWidget *widget, GdkEventButton *event)
{
    GwyGraphArea *area;
    GwyGraphModel *gmodel;
    gint x, y, ispos = 0;
    gdouble dx, dy, selection_data[2], selection_areadata[4], selection_linedata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);

    area = GWY_GRAPH_AREA(widget);
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
    dx = scr_to_data_x(widget, x);
    dy = scr_to_data_y(widget, y);

    gmodel = GWY_GRAPH_MODEL(area->graph_model);


    if (area->selecting && (area->status == GWY_GRAPH_STATUS_XSEL || area->status == GWY_GRAPH_STATUS_YSEL)
        && GWY_SELECTION(area->areasdata)->n)
    {
         gwy_selection_get_object(GWY_SELECTION(area->areasdata), GWY_SELECTION(area->areasdata)->n - 1, selection_areadata);
         if (area->status == GWY_GRAPH_STATUS_XSEL)
            selection_areadata[2] = dx;
         else
            selection_areadata[3] = dy;

         gwy_selection_set_object(GWY_SELECTION(area->areasdata), GWY_SELECTION(area->areasdata)->n - 1, selection_areadata);
         if (selection_areadata[2] == selection_areadata[0] || selection_areadata[3] == selection_areadata[1])
             gwy_selection_delete_object(GWY_SELECTION(area->areasdata), GWY_SELECTION(area->areasdata)->n - 1);
         area->selecting = FALSE;
         gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->selecting && (area->status == GWY_GRAPH_STATUS_XLINES || area->status == GWY_GRAPH_STATUS_YLINES)
        && GWY_SELECTION(area->linesdata)->n)
    {
        gwy_selection_get_object(GWY_SELECTION(area->linesdata), GWY_SELECTION(area->linesdata)->n - 1, &selection_linedata);
        if (area->status == GWY_GRAPH_STATUS_XLINES)
            selection_linedata = dx;
        else
            selection_linedata = dy;

        area->selecting = FALSE;
        gwy_selection_set_object(GWY_SELECTION(area->linesdata), GWY_SELECTION(area->linesdata)->n - 1, &selection_linedata);
        gwy_selection_finished(GWY_SELECTION(area->linesdata));
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->selecting && area->status == GWY_GRAPH_STATUS_POINTS)
    {
        gwy_selection_get_object(GWY_SELECTION(area->pointsdata), GWY_SELECTION(area->pointsdata)->n - 1, selection_data);
        selection_data[0] = dx;
        selection_data[1] = dy;
        gwy_selection_set_object(GWY_SELECTION(area->pointsdata), GWY_SELECTION(area->pointsdata)->n - 1, selection_data);
        area->selecting = FALSE;
        gtk_widget_queue_draw(GTK_WIDGET(area));
        gwy_selection_finished(GWY_SELECTION(area->pointsdata));
    }




    if (area->status == GWY_GRAPH_STATUS_ZOOM
                             && (area->selecting != 0)) {
         area->selecting = FALSE;
         gwy_graph_area_signal_zoomed(area);
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
    return FALSE;
}

static gboolean
gwy_graph_area_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    GwyGraphArea *area;
    GwyGraphModel *gmodel;
    gint x, y, ispos = 0;
    gdouble dx, dy, selection_data[2], selection_areadata[4], selection_linedata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);

    area = GWY_GRAPH_AREA(widget);
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
    dx = scr_to_data_x(widget, x);
    dy = scr_to_data_y(widget, y);

    gmodel = GWY_GRAPH_MODEL(area->graph_model);

    area->mouse_present = TRUE;
    area->actual_cursor_data->data_point.x = dx;
    area->actual_cursor_data->data_point.y = dy;
    g_signal_emit(G_OBJECT(area), gwygrapharea_signals[MOUSE_MOVED_SIGNAL], 0);

    if (area->selecting && (area->status == GWY_GRAPH_STATUS_XSEL || area->status == GWY_GRAPH_STATUS_YSEL)
        && GWY_SELECTION(area->areasdata)->n)
    {
         gwy_selection_get_object(GWY_SELECTION(area->areasdata), GWY_SELECTION(area->areasdata)->n - 1, selection_areadata);
         if (area->status == GWY_GRAPH_STATUS_XSEL)
            selection_areadata[2] = dx;
         else
            selection_areadata[3] = dy;

         gwy_selection_set_object(GWY_SELECTION(area->areasdata), GWY_SELECTION(area->areasdata)->n - 1, selection_areadata);
         gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->selecting && (area->status == GWY_GRAPH_STATUS_XLINES || area->status == GWY_GRAPH_STATUS_YLINES)
        && GWY_SELECTION(area->linesdata)->n)
    {
        gwy_selection_get_object(GWY_SELECTION(area->linesdata), GWY_SELECTION(area->linesdata)->n - 1, &selection_linedata);
        if (area->status == GWY_GRAPH_STATUS_XLINES)
            selection_linedata = dx;
        else
            selection_linedata = dy;

        gwy_selection_set_object(GWY_SELECTION(area->linesdata), GWY_SELECTION(area->linesdata)->n - 1, &selection_linedata);
        gwy_selection_finished(GWY_SELECTION(area->linesdata));
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->selecting && area->status == GWY_GRAPH_STATUS_POINTS)
    {
        gwy_selection_get_object(GWY_SELECTION(area->pointsdata), GWY_SELECTION(area->pointsdata)->n - 1, selection_data);
        selection_data[0] = dx;
        selection_data[1] = dy;
        gwy_selection_set_object(GWY_SELECTION(area->pointsdata), GWY_SELECTION(area->pointsdata)->n - 1, selection_data);
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    if (area->status == GWY_GRAPH_STATUS_ZOOM
                             && (area->selecting != 0)) {
         area->zoomdata->width = dx - area->zoomdata->xmin;
         area->zoomdata->height = dy - area->zoomdata->ymin;
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

static gint
gwy_graph_area_find_curve(GwyGraphArea *area, gdouble x, gdouble y)
{
    gint i, j, nc, ndata;
    gint closestid = -1;
    gdouble closestdistance, distance = 0.0;
    const gdouble *xdata, *ydata;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;

    closestdistance = G_MAXDOUBLE;
    model = GWY_GRAPH_MODEL(area->graph_model);
    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve_by_index(model, i);
        ndata = gwy_graph_curve_model_get_ndata(curvemodel);
        xdata = gwy_graph_curve_model_get_xdata(curvemodel);
        ydata = gwy_graph_curve_model_get_ydata(curvemodel);
        for (j = 0; j < ndata - 1; j++) {
            if (xdata[j] <= x && xdata[j + 1] >= x) {
                distance = fabs(y - ydata[j]
                                + (x - xdata[j])*(ydata[j + 1] - ydata[j])
                                  /(xdata[j + 1] - xdata[j]));
                if (distance < closestdistance) {
                    closestdistance = distance;
                    closestid = i;
                }
                break;
            }
        }
    }
    if (fabs(closestdistance/(model->y_max - model->y_min)) < 0.05)
        return closestid;
    else
        return -1;
}

static gint
gwy_graph_area_find_selection(GwyGraphArea *area, gdouble x, gdouble y)
{
    gint i;
    gdouble selection_areadata[4];
    gdouble xmin, ymin, xmax, ymax;

    for (i=0; i<GWY_SELECTION(area->areasdata)->n; i++)
    {
        gwy_selection_get_object(GWY_SELECTION(area->areasdata), GWY_SELECTION(area->areasdata)->n - 1, selection_areadata);
        xmin = MIN(selection_areadata[0], selection_areadata[2]);
        xmax = MAX(selection_areadata[0], selection_areadata[2]);
        ymin = MIN(selection_areadata[1], selection_areadata[3]);
        ymax = MAX(selection_areadata[1], selection_areadata[3]);

        if (xmin < x && xmax > x && ymin < y && ymax > y) return i;
    }
    return -1;
}

static gint
gwy_graph_area_find_point(GwyGraphArea *area, gdouble x, gdouble y)
{
    gint i;
    GwyGraphModel *model;
    gdouble xmin, ymin, xmax, ymax, xoff, yoff, selection_data[2];

    model = GWY_GRAPH_MODEL(area->graph_model);
    xoff = (model->x_max - model->x_min)/100;
    yoff = (model->y_max - model->y_min)/100;

    for (i=0; i<GWY_SELECTION(area->pointsdata)->n; i++)
    {
        gwy_selection_get_object(GWY_SELECTION(area->pointsdata), i, selection_data);

        xmin = selection_data[0] - xoff;
        xmax = selection_data[0] + xoff;
        ymin = selection_data[1] - yoff;
        ymax = selection_data[1] + yoff;

        if (xmin <= x && xmax >= x && ymin <= y && ymax >= y) return i;
    }
    return -1;
}

static gint
gwy_graph_area_find_line(GwyGraphArea *area, gdouble position)
{
    gint i;
    GwyGraphModel *model;
    gdouble min = 0, max = 0, xoff, yoff, selection_data;

    model = GWY_GRAPH_MODEL(area->graph_model);
    xoff = (model->x_max - model->x_min)/100;
    yoff = (model->y_max - model->y_min)/100;

    for (i=0; i<GWY_SELECTION(area->linesdata)->n; i++)
    {
        gwy_selection_get_object(GWY_SELECTION(area->linesdata), i, &selection_data);

        if (area->status == GWY_GRAPH_STATUS_XLINES)
        {
            min = selection_data - xoff;
            max = selection_data + xoff;
        }
        else if (area->status == GWY_GRAPH_STATUS_YLINES)
        {
            min = selection_data - yoff;
            max = selection_data + yoff;
        }
        if (min <= position && max >= position) return i;
    }
    return -1;
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


static gdouble
scr_to_data_x(GtkWidget *widget, gint scr)
{
    GwyGraphArea *area;
    GwyGraphModel *model;

    area = GWY_GRAPH_AREA(widget);
    model = GWY_GRAPH_MODEL(area->graph_model);

    scr = CLAMP(scr, 0, widget->allocation.width-1);
    if (!model->x_is_logarithmic)
        return model->x_min
           + scr*(model->x_max - model->x_min)/(widget->allocation.width-1);
    else
        return pow(10, log10(model->x_min)
           + scr*(log10(model->x_max) - log10(model->x_min))/(widget->allocation.width-1));
}

static gint
data_to_scr_x(GtkWidget *widget, gdouble data)
{
    GwyGraphArea *area;
    GwyGraphModel *model;

    area = GWY_GRAPH_AREA(widget);
    model = GWY_GRAPH_MODEL(area->graph_model);

    if (!model->x_is_logarithmic)
        return (data - model->x_min)
           /((model->x_max - model->x_min)/(widget->allocation.width-1));
    else
       return (log10(data) - log10(model->x_min))
           /((log10(model->x_max) - log10(model->x_min))/(widget->allocation.width-1));
}

static gdouble
scr_to_data_y(GtkWidget *widget, gint scr)
{
    GwyGraphArea *area;
    GwyGraphModel *model;

    area = GWY_GRAPH_AREA(widget);
    model = GWY_GRAPH_MODEL(area->graph_model);

    scr = CLAMP(scr, 0, widget->allocation.height-1);
    if (!model->y_is_logarithmic)
        return model->y_min
           + (widget->allocation.height - scr)*(model->y_max - model->y_min)
             /(widget->allocation.height-1);
    else
        return pow(10, log10(model->y_min)
                   + (widget->allocation.height - scr)*(log10(model->y_max) - log10(model->y_min))
                                                        /(widget->allocation.height-1));
}

static gint
data_to_scr_y(GtkWidget *widget, gdouble data)
{
    GwyGraphArea *area;
    GwyGraphModel *model;

    area = GWY_GRAPH_AREA(widget);
    model = GWY_GRAPH_MODEL(area->graph_model);
    if (!model->y_is_logarithmic)
        return widget->allocation.height
           - (data - model->y_min)
             /((model->y_max - model->y_min)/((gdouble)widget->allocation.height-1));
    else
        return widget->allocation.height
            - (log10(data) - log10(model->y_min))
            /((log10(model->y_max) - log10(model->y_min))/((gdouble)widget->allocation.height-1));
}

/**
 * gwy_graph_area_signal_selected:
 * @area: graph area
 *
 * emit signal that something was selected by mouse. "Something" depends on the
 * actual graph status (points, horizontal selection, etc.).
 **/
static void
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
static void
gwy_graph_area_signal_zoomed(GwyGraphArea *area)
{
    g_signal_emit (G_OBJECT (area), gwygrapharea_signals[ZOOMED_SIGNAL], 0);
}


/**
 * gwy_graph_area_signal_refresh:
 * @area: graph area
 *
 * Refreshes the area with respect to graph model.
 **/
void
gwy_graph_area_refresh(GwyGraphArea *area)
{
    /*refresh label*/
    if (GWY_GRAPH_MODEL(area->graph_model)->label_visible)
    {
        gtk_widget_show(GTK_WIDGET(area->lab));

        gwy_graph_label_refresh(area->lab);
        /*re-adjust label position*/
        gwy_graph_area_adjust_label(area);
    }
    else
        gtk_widget_hide(GTK_WIDGET(area->lab));


    /*repaint area data*/
    gtk_widget_queue_draw(GTK_WIDGET(area));
}

/**
 * gwy_graph_area_set_model:
 * @area: graph area
 * @gmodel: new graph model
 *
 * Changes the graph model. Calls refresh afterwards.
 **/
void
gwy_graph_area_set_model(GwyGraphArea *area, gpointer gmodel)
{
    area->graph_model = gmodel;
    gwy_graph_label_set_model(area->lab, gmodel);
    gwy_graph_area_refresh(area);
}

static void
gwy_graph_area_entry_cb(GwyGraphAreaDialog *dialog, gint arg1, gpointer user_data)
{
    if (arg1 == GTK_RESPONSE_APPLY) {
        gwy_graph_area_refresh(GWY_GRAPH_AREA(user_data));
    }
    else if (arg1 == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
}

static void
gwy_graph_label_entry_cb(GwyGraphLabelDialog *dialog, gint arg1, gpointer user_data)
{
    if (arg1 == GTK_RESPONSE_APPLY) {
        gwy_graph_area_refresh(GWY_GRAPH_AREA(user_data));
    }
    else if (arg1 == GTK_RESPONSE_CLOSE) {
        gtk_widget_hide(GTK_WIDGET(dialog));
    }
}

/**
 * gwy_graph_area_clear_selection:
 * @area: graph area
 *
 * Clear all the selections. If you use grapher area as a part of
 * #GwyGrapher use the #GwyGrapher clear selection function preferably.
 **/
void
gwy_graph_area_clear_selection(GwyGraphArea *area)
{

    if (area->status == GWY_GRAPH_STATUS_XSEL || area->status == GWY_GRAPH_STATUS_YSEL)
        gwy_selection_clear(GWY_SELECTION(area->areasdata));
    else if (area->status == GWY_GRAPH_STATUS_XLINES || area->status == GWY_GRAPH_STATUS_YLINES)
        gwy_selection_clear(GWY_SELECTION(area->linesdata));
    else if (area->status == GWY_GRAPH_STATUS_POINTS || area->status == GWY_GRAPH_STATUS_ZOOM)
        gwy_selection_clear(GWY_SELECTION(area->pointsdata));


    gwy_graph_area_signal_selected(area);
    gtk_widget_queue_draw(GTK_WIDGET(area));
}

/**
 * gwy_graph_area_set_selection:
 * @area: graph area
 * @status: status of selection to be set
 * @selection: selection data field
 * @n_of_selections: number of selections to be set
 *
 * Set selection data for given values and status.
 * Refresh graph area after doing this to see any change.
 **/
void
gwy_graph_area_set_selection(GwyGraphArea *area, GwyGraphStatusType status,
                             gdouble* selection, gint n_of_selections)
{
    gint i;

    area->status = status;

    switch (area->status) {
        case GWY_GRAPH_STATUS_XSEL:
        for (i = 0; i < n_of_selections; i++) {
            gdouble selection_areadata[4];

            selection_areadata[0] = selection[2*i];
            selection_areadata[2] = selection[2*i + 1];
            selection_areadata[1] = GWY_GRAPH_MODEL(area->graph_model)->y_min;
            selection_areadata[3] = GWY_GRAPH_MODEL(area->graph_model)->y_max;
            gwy_selection_set_object(GWY_SELECTION(area->areasdata),
                                     i, selection_areadata);
        }
        break;

        case GWY_GRAPH_STATUS_YSEL:
        for (i = 0; i < n_of_selections; i++) {
            gdouble selection_areadata[4];

            selection_areadata[0] = GWY_GRAPH_MODEL(area->graph_model)->x_min;
            selection_areadata[2] = GWY_GRAPH_MODEL(area->graph_model)->x_max;
            selection_areadata[1] = selection[2*i];
            selection_areadata[3] = selection[2*i + 1];
            gwy_selection_set_object(GWY_SELECTION(area->areasdata),
                                     i, selection_areadata);
            }
        break;

        case GWY_GRAPH_STATUS_POINTS:
        gwy_selection_set_data(GWY_SELECTION(area->pointsdata),
                               n_of_selections, selection);
        break;

        case GWY_GRAPH_STATUS_XLINES:
        case GWY_GRAPH_STATUS_YLINES:
        gwy_selection_set_data(GWY_SELECTION(area->linesdata),
                               n_of_selections, selection);
        break;

        default:
        g_return_if_reached();
        break;
    }
}


/**
 * gwy_graph_area_enable_user_input:
 * @area: graph area
 * @enable: enable/disable user input
 *
 * Enables/disables all the user input dialogs to be invoked by clicking by mouse.
 **/
void
gwy_graph_area_enable_user_input(GwyGraphArea *area, gboolean enable)
{
    area->enable_user_input = enable;
    gwy_graph_label_enable_user_input(area->lab, enable);
}

/**
   * gwy_graph_area_get_cursor:
   * @area: graph area
   * @x_cursor: x value corresponding to cursor position
   * @y_cursor: y value corresponding to cursor position
   *
   * Gets mouse cursor related values withing graph area
   */
void
gwy_graph_area_get_cursor(GwyGraphArea *area, gdouble *x_cursor, gdouble *y_cursor)
{
    if (area->mouse_present)
    {
        *x_cursor = area->actual_cursor_data->data_point.x;
        *y_cursor = area->actual_cursor_data->data_point.y;

    }
    else
    {
        *x_cursor = 0;
        *y_cursor = 0;
    }
}

static
gboolean gwy_graph_area_leave_notify(GtkWidget *widget, G_GNUC_UNUSED GdkEventCrossing *event)
{
    GwyGraphArea *area = GWY_GRAPH_AREA(widget);

    area->mouse_present = FALSE;

    return FALSE;
}


static gchar *symbols[] =
{
    "Box",
    "Cross",
    "Circle",
    "Star",
    "Times",
    "TriU",
    "TriD",
    "Dia",
};

GString* gwy_graph_area_export_vector(GwyGraphArea *area,
                                      gint x, gint y,
                                      gint width, gint height)
{
    gint i, j, nc;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;
    GString *out;
    gdouble xmult, ymult;
    GwyRGBA *color;
    gint pointsize;
    gint linesize;

    out = g_string_new("%%Area\n");

    model = GWY_GRAPH_MODEL(area->graph_model);
    if ((model->x_max - model->x_min)==0 || (model->y_max - model->y_min)==0)
    {
        g_warning("Graph null range.\n");
        return out;
    }

    xmult = width/(model->x_max - model->x_min);
    ymult = height/(model->y_max - model->y_min);

    g_string_append_printf(out, "/box {\n"
                           "newpath\n"
                           "%d %d M\n"
                           "%d %d L\n"
                           "%d %d L\n"
                           "%d %d L\n"
                           "closepath\n"
                           "} def\n",
                           x, y,
                           x + width, y,
                           x + width, y + height,
                           x, y + height);

    g_string_append_printf(out, "gsave\n");
    g_string_append_printf(out, "box\n");
    g_string_append_printf(out, "clip\n");

    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve_by_index(model, i);
        pointsize = gwy_graph_curve_model_get_curve_point_size(curvemodel);
        linesize = gwy_graph_curve_model_get_curve_line_size(curvemodel);
        color = gwy_graph_curve_model_get_curve_color(curvemodel);
        g_string_append_printf(out, "/hpt %d def\n", pointsize);
        g_string_append_printf(out, "/vpt %d def\n", pointsize);
        g_string_append_printf(out, "/hpt2 hpt 2 mul def\n");
        g_string_append_printf(out, "/vpt2 vpt 2 mul def\n");
        g_string_append_printf(out, "%d setlinewidth\n", linesize);
        g_string_append_printf(out, "%f %f %f setrgbcolor\n",
                               color->r, color->g, color->b);

        for (j = 0; j < curvemodel->n - 1; j++) {
            if (curvemodel->type == GWY_GRAPH_CURVE_LINE
                || curvemodel->type == GWY_GRAPH_CURVE_LINE_POINTS)
            {
                if (j==0) g_string_append_printf(out, "%d %d M\n",
                                   (gint)(x + (curvemodel->xdata[j] - model->x_min)*xmult),
                                   (gint)(y + (curvemodel->ydata[j] - model->y_min)*ymult));
                else
                {
                    g_string_append_printf(out, "%d %d M\n",
                                   (gint)(x + (curvemodel->xdata[j-1] - model->x_min)*xmult),
                                   (gint)(y + (curvemodel->ydata[j-1] - model->y_min)*ymult));
                    g_string_append_printf(out, "%d %d L\n",
                                   (gint)(x + (curvemodel->xdata[j] - model->x_min)*xmult),
                                   (gint)(y + (curvemodel->ydata[j] - model->y_min)*ymult));
                }
            }
            if (curvemodel->type == GWY_GRAPH_CURVE_POINTS || curvemodel->type == GWY_GRAPH_CURVE_LINE_POINTS)
            {
                g_string_append_printf(out, "%d %d %s\n",
                          (gint)(x + (curvemodel->xdata[j] - model->x_min)*xmult),
                          (gint)(y + (curvemodel->ydata[j] - model->y_min)*ymult),
                          symbols[curvemodel->point_type]);

            }
        }
        g_string_append_printf(out, "stroke\n");
    }
    g_string_append_printf(out, "grestore\n");

    return out;
}

/**
   * gwy_graph_area_set_selection_limit:
   * @area: graph area
   * @limit: maximum muber of selections
   *
   * Set maximum number of selections done by mouse
   */
void
gwy_graph_area_set_selection_limit(GwyGraphArea *area, gint limit)
{
    area->selection_limit = limit;
    gwy_selection_set_max_objects(GWY_SELECTION(area->pointsdata), limit);
    gwy_selection_set_max_objects(GWY_SELECTION(area->areasdata), limit);
    gwy_selection_set_max_objects(GWY_SELECTION(area->linesdata), limit);
}

/**
   * gwy_graph_area_get_selection_limit:
   * @area: graph area
   *
   * Returns: maximum number of selections done by mouse
   */
gint
gwy_graph_area_get_selection_limit(GwyGraphArea *area)
{
    return area->selection_limit;
}


/**
   * gwy_graph_area_get_label:
   * @area: graph area
   *
   * Returns: graph area label
   */
GtkWidget*
gwy_graph_area_get_label(GwyGraphArea *area)
{
    return GTK_WIDGET(area->lab);
}


void
gwy_graph_area_set_x_grid_data(GwyGraphArea *area, GArray *grid_data)
{
    guint i;
    gdouble *pdata, *psetdata;
    g_array_set_size(area->x_grid_data, grid_data->len);

    for (i=0; i<grid_data->len; i++){
        pdata = &g_array_index(area->x_grid_data, gdouble, i);
        psetdata = &g_array_index(grid_data, gdouble, i);
        *pdata = *psetdata;
    }
}

void
gwy_graph_area_set_y_grid_data(GwyGraphArea *area, GArray *grid_data)
{
    guint i;
    gdouble *pdata, *psetdata;
    g_array_set_size(area->y_grid_data, grid_data->len);

    for (i=0; i<grid_data->len; i++){
        pdata = &g_array_index(area->y_grid_data, gdouble, i);
        psetdata = &g_array_index(grid_data, gdouble, i);
        *pdata = *psetdata;
    }
}

const GArray*
gwy_graph_area_get_x_grid_data(GwyGraphArea *area)
{
    return area->x_grid_data;
}

const GArray*
gwy_graph_area_get_y_grid_data(GwyGraphArea *area)
{
    return area->y_grid_data;
}

/**
  * gwy_graph_area_get_selection_number:
  * @area: A graph area widget.
  *
  * Gets number of selections selected by user.
  *
  * Returns: number of selections
 **/
gint
gwy_graph_area_get_selection_number(GwyGraphArea *area)
{
    if (area->status == GWY_GRAPH_STATUS_XSEL)
         return GWY_SELECTION(area->areasdata)->n;
    else if (area->status ==  GWY_GRAPH_STATUS_POINTS)
         return GWY_SELECTION(area->pointsdata)->n;
    else if (area->status ==  GWY_GRAPH_STATUS_XLINES
         || area->status ==  GWY_GRAPH_STATUS_YLINES)
         return GWY_SELECTION(area->linesdata)->n;
    else
         return 0;
}

/**
  * gwy_graph_get_selection:
  * @graph: A graph widget.
  * @selection: allocated field of gdoubles
  *
  * Fills the @selection field with current selection values.
  * The values of selections are written to the field
  * as (start_selection_1, end_selection_1, start_selection_2, ...)
  * for GWY_GRAPH_STATUS_XSEL and GWY_GRAPH_STATUS_YSEL type selections,
  * as (x1, y1, x2, y2,...) for GWY_GRAPH_STATUS_POINTS or GWY_GRAPH_STATUS_CURSOR
  * type selections, as (x_start, y_start, width, height) for GWY_GRAPH_STATUS_ZOOM.
  * The field mus be allready allocated, therefore the field size should
  * match the maximum number of selections (that is  by default 10 for each type
  * and can be set by gwy_graph_set_selection_limit() function).
  *
 **/
void
gwy_graph_area_get_selection(GwyGraphArea *area, gdouble *selection)
{
    gint i;
    gdouble data_value, area_selection[4];

    if (selection == NULL) return;

    switch (area->status) {
        case GWY_GRAPH_STATUS_XSEL:
        for (i = 0; i < GWY_SELECTION(area->areasdata)->n; i++) {
            gwy_selection_get_object(GWY_SELECTION(area->areasdata), i, area_selection);
            selection[2*i] = area_selection[0];
            selection[2*i + 1] = area_selection[2];
        }
        break;

        case GWY_GRAPH_STATUS_YSEL:
        for (i = 0; i < GWY_SELECTION(area->areasdata)->n; i++) {
            gwy_selection_get_object(GWY_SELECTION(area->areasdata), i, area_selection);
            selection[2*i] = area_selection[1];
            selection[2*i + 1] = area_selection[3];
        }
        break;

        case GWY_GRAPH_STATUS_XLINES:
        case GWY_GRAPH_STATUS_YLINES:
        gwy_selection_get_data(GWY_SELECTION(area->linesdata), selection);
        break;

        case GWY_GRAPH_STATUS_POINTS:
        gwy_selection_get_data(GWY_SELECTION(area->pointsdata), selection);
        break;

        case GWY_GRAPH_STATUS_ZOOM:
        if (area->zoomdata->width > 0) {
            selection[0] = area->zoomdata->xmin;
            selection[1] = area->zoomdata->width;
        }
        else {
            selection[0] = area->zoomdata->xmin
                           + area->zoomdata->width;
            selection[1] = -area->zoomdata->width;
        }

        if (area->zoomdata->height > 0) {
            selection[2] = area->zoomdata->ymin;
            selection[3] = area->zoomdata->height;
        }
        else {
            selection[2] = area->zoomdata->ymin
                           + area->zoomdata->height;
            selection[3] = -area->zoomdata->height;
        }
        break;

        default:
        g_assert_not_reached();
    }                                                                                    
}
                
   


/************************** Documentation ****************************/

/**
 * SECTION:gwygrapharea
 * @title: GwyGraphArea
 * @short_description: Layout for drawing graph curves
 *
 * #GwyGraphArea is the central part of #GwyGraph widget. It plots a set of
 * data curves with a given plot properties.
 *
 * It is recommended to use it within #GwyGraph, however, it can be used
 * separately.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
