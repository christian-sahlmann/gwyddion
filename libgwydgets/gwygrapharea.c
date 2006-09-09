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
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <libgwydgets/gwydgetutils.h>
#include <libgwydgets/gwygraphselections.h>
#include "gwygraphareadialog.h"
#include "gwygraphlabeldialog.h"


enum {
    COLOR_FG = 0,
    COLOR_BG,
    COLOR_SELECTION,
    COLOR_LAST
};

enum {
    BORDER_NONE = 0,
    BORDER_MIN = 1,
    BORDER_MAX = 2
};

enum {
    STATUS_CHANGED,
    LAST_SIGNAL
};

static guint area_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations - widget related */
static void     gwy_graph_area_finalize      (GObject *object);
static void     gwy_graph_area_realize       (GtkWidget *widget);
static void     gwy_graph_area_unrealize     (GtkWidget *widget);
static void     gwy_graph_area_size_allocate (GtkWidget *widget,
                                              GtkAllocation *allocation);
static gboolean gwy_graph_area_expose        (GtkWidget *widget,
                                              GdkEventExpose *event);
static gboolean gwy_graph_area_button_press  (GtkWidget *widget,
                                              GdkEventButton *event);
static gboolean gwy_graph_area_button_release(GtkWidget *widget,
                                              GdkEventButton *event);
static gboolean gwy_graph_area_leave_notify  (GtkWidget *widget,
                                              GdkEventCrossing *event);
static gint     gwy_graph_area_find_curve    (GwyGraphArea *area,
                                              gdouble x,
                                              gdouble y);
static gint     gwy_graph_area_find_selection(GwyGraphArea *area,
                                              gdouble x,
                                              gdouble y,
                                              int *btype);
static gint     gwy_graph_area_find_point    (GwyGraphArea *area,
                                              gdouble x,
                                              gdouble y);
static gint     gwy_graph_area_find_line     (GwyGraphArea *area,
                                              gdouble position);
static void     gwy_graph_area_draw_zoom     (GdkDrawable *drawable,
                                              GdkGC *gc,
                                              GwyGraphArea *area);
static void     selection_changed_cb         (GwyGraphArea *area);

/* Forward declarations - area related */
static gdouble    scr_to_data_x               (GtkWidget *widget,
                                               gint scr);
static gdouble    scr_to_data_y               (GtkWidget *widget,
                                               gint scr);
static gint       data_to_scr_x               (GtkWidget *widget,
                                               gdouble data);
static gint       data_to_scr_y               (GtkWidget *widget,
                                               gdouble data);
static void       gwy_graph_area_entry_cb     (GwyGraphAreaDialog *dialog,
                                               gint arg1,
                                               gpointer user_data);
static void       gwy_graph_label_entry_cb    (GwyGraphLabelDialog *dialog,
                                               gint arg1,
                                               gpointer user_data);
static void       label_geometry_changed_cb   (GtkWidget *area,
                                               GtkAllocation *label_allocation);
static void       gwy_graph_area_repos_label  (GwyGraphArea *area,
                                               GtkAllocation *area_allocation,
                                               GtkAllocation *label_allocation);
static gboolean   gwy_graph_area_motion_notify(GtkWidget *widget,
                                               GdkEventMotion *event);
static GtkWidget* gwy_graph_area_find_child   (GwyGraphArea *area,
                                               gint x,
                                               gint y);
static void gwy_graph_area_draw_child_rectangle(GwyGraphArea *area);
static void gwy_graph_area_clamp_coords_for_child(GwyGraphArea *area,
                                                  gint *x,
                                                  gint *y);

G_DEFINE_TYPE(GwyGraphArea, gwy_graph_area, GTK_TYPE_LAYOUT)

static void
gwy_graph_area_class_init(GwyGraphAreaClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class;
    GtkObjectClass *object_class;

    widget_class = (GtkWidgetClass*)klass;
    object_class = (GtkObjectClass*)klass;

    gobject_class->finalize = gwy_graph_area_finalize;

    widget_class->realize = gwy_graph_area_realize;
    widget_class->unrealize = gwy_graph_area_unrealize;
    widget_class->expose_event = gwy_graph_area_expose;
    widget_class->size_allocate = gwy_graph_area_size_allocate;

    widget_class->button_press_event = gwy_graph_area_button_press;
    widget_class->button_release_event = gwy_graph_area_button_release;
    widget_class->motion_notify_event = gwy_graph_area_motion_notify;
    widget_class->leave_notify_event = gwy_graph_area_leave_notify;

    klass->status_changed = NULL;
    area_signals[STATUS_CHANGED] =
          g_signal_new("status-changed",
                     G_OBJECT_CLASS_TYPE(object_class),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(GwyGraphAreaClass, status_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__INT,
                     G_TYPE_NONE, 1, G_TYPE_INT);

}

static void
gwy_graph_area_init(GwyGraphArea *area)
{
    gwy_debug("");
    area->gc = NULL;

    area->selecting = FALSE;
    area->mouse_present = FALSE;

    area->pointsdata = GWY_SELECTION(g_object_new(
                                    GWY_TYPE_SELECTION_GRAPH_POINT, NULL));
    gwy_selection_set_max_objects(area->pointsdata, 10);
    g_signal_connect_swapped(area->pointsdata, "changed",
                     G_CALLBACK(selection_changed_cb), area);

    area->xseldata = GWY_SELECTION(g_object_new(
                                    GWY_TYPE_SELECTION_GRAPH_1DAREA, NULL));
    gwy_selection_set_max_objects(area->xseldata, 1);
    g_signal_connect_swapped(area->xseldata, "changed",
                     G_CALLBACK(selection_changed_cb), area);


    area->yseldata = GWY_SELECTION(g_object_new(
                                    GWY_TYPE_SELECTION_GRAPH_1DAREA, NULL));
    gwy_selection_set_max_objects(area->yseldata, 1);
    g_signal_connect_swapped(area->yseldata, "changed",
                     G_CALLBACK(selection_changed_cb), area);


    area->xlinesdata = GWY_SELECTION(g_object_new(
                                    GWY_TYPE_SELECTION_GRAPH_LINE, NULL));
    gwy_selection_set_max_objects(area->xlinesdata, 10);
    g_signal_connect_swapped(area->xlinesdata, "changed",
                     G_CALLBACK(selection_changed_cb), area);


    area->ylinesdata = GWY_SELECTION(g_object_new(
                                    GWY_TYPE_SELECTION_GRAPH_LINE, NULL));
    gwy_selection_set_max_objects(area->ylinesdata, 10);
    g_signal_connect_swapped(area->ylinesdata, "changed",
                     G_CALLBACK(selection_changed_cb), area);


    area->zoomdata = GWY_SELECTION(g_object_new(
                                    GWY_TYPE_SELECTION_GRAPH_ZOOM, NULL));
    gwy_selection_set_max_objects(area->zoomdata, 1);
    g_signal_connect_swapped(area->zoomdata, "changed",
                     G_CALLBACK(selection_changed_cb), area);


    area->actual_cursor_data = g_new(GwyGraphStatus_CursorData, 1);

    area->x_grid_data = g_array_new(FALSE, FALSE, sizeof(gdouble));
    area->y_grid_data = g_array_new(FALSE, FALSE, sizeof(gdouble));

    area->rx0 = 1;
    area->ry0 = 0;

    area->enable_user_input = TRUE;

    area->lab = GWY_GRAPH_LABEL(gwy_graph_label_new());
    g_signal_connect_swapped(GTK_WIDGET(area->lab), "size-allocate",
                     G_CALLBACK(label_geometry_changed_cb), area);
    gtk_layout_put(GTK_LAYOUT(area), GTK_WIDGET(area->lab),
                   GTK_WIDGET(area)->allocation.width
                            - GTK_WIDGET(area->lab)->allocation.width - 5,
                   5);
}

/**
 * gwy_graph_area_new:
 * @hadjustment: horizontal adjustment
 *               (assigns lower and upper bounds as well as increments
 *               to the horizontal axis of the new graph area)
 * @vadjustment: vertical adjustment
 *               (assigns lower and upper bounds as well as increments
 *               to the vertical axis of the new graph area)
 *
 * Creates a graph area widget.
 *
 * Returns: new #GwyGraphArea widget.
 **/
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

    area->area_dialog = _gwy_graph_area_dialog_new();
    g_signal_connect(area->area_dialog, "response",
                     G_CALLBACK(gwy_graph_area_entry_cb), area);

    area->label_dialog = _gwy_graph_label_dialog_new();
    g_signal_connect(area->label_dialog, "response",
                     G_CALLBACK(gwy_graph_label_entry_cb), area);


    return GTK_WIDGET(area);
}

static void
gwy_graph_area_finalize(GObject *object)
{
    GwyGraphArea *area;

    area = GWY_GRAPH_AREA(object);

    gtk_widget_destroy(GTK_WIDGET(area->area_dialog));
    gtk_widget_destroy(GTK_WIDGET(area->label_dialog));

    G_OBJECT_CLASS(gwy_graph_area_parent_class)->finalize(object);
}


static void
gwy_graph_area_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraphArea *area;
    GtkAllocation *lab_alloc;

    area = GWY_GRAPH_AREA(widget);
    lab_alloc = &GTK_WIDGET(area->lab)->allocation;

    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->size_allocate(widget,
                                                                 allocation);

    gwy_graph_area_repos_label(area, allocation, lab_alloc);

    area->old_width = allocation->width;
    area->old_height = allocation->height;
    area->label_old_width = lab_alloc->width;
    area->label_old_height = lab_alloc->height;
}

static void
gwy_graph_area_repos_label(GwyGraphArea *area, 
                           GtkAllocation *area_allocation,
                           GtkAllocation *label_allocation)
{
    
    gint posx, posy;
    posx = (gint)(area->rx0*area_allocation->width);                            
    posy = (gint)(area->ry0*area_allocation->height);
    posx = CLAMP(posx, 5, area_allocation->width - label_allocation->width - 5);
    posy = CLAMP(posy, 5, area_allocation->height - label_allocation->height - 5); 
    
    if (((area->old_width != area_allocation->width
          || area->old_height != area_allocation->height)
         || (area->label_old_width != label_allocation->width
          || area->label_old_height != label_allocation->height))) {
        gtk_layout_move(GTK_LAYOUT(area), GTK_WIDGET(area->lab), posx, posy);
    }
    
}


static void
gwy_graph_area_realize(GtkWidget *widget)
{
    GdkDisplay *display;
    GwyGraphArea *area;

    if (GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->realize)
        GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->realize(widget);

    area = GWY_GRAPH_AREA(widget);
    area->gc = gdk_gc_new(GTK_LAYOUT(widget)->bin_window);

    display = gtk_widget_get_display(widget);
    area->cross_cursor = gdk_cursor_new_for_display(display,
                                                    GDK_CROSS);
    area->fleur_cursor = gdk_cursor_new_for_display(display,
                                                    GDK_FLEUR);
    area->harrow_cursor = gdk_cursor_new_for_display(display,
                                                     GDK_SB_H_DOUBLE_ARROW);
    area->varrow_cursor = gdk_cursor_new_for_display(display,
                                                     GDK_SB_V_DOUBLE_ARROW);

    gdk_gc_set_rgb_bg_color(area->gc, &widget->style->white);
    gdk_gc_set_rgb_fg_color(area->gc, &widget->style->black);
}

static void
gwy_graph_area_unrealize(GtkWidget *widget)
{
    GwyGraphArea *area;

    area = GWY_GRAPH_AREA(widget);

    gwy_object_unref(area->gc);
    gdk_cursor_unref(area->cross_cursor);
    gdk_cursor_unref(area->fleur_cursor);
    gdk_cursor_unref(area->harrow_cursor);
    gdk_cursor_unref(area->varrow_cursor);

    if (GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->unrealize)
        GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->unrealize(widget);
}

static gboolean
gwy_graph_area_expose(GtkWidget *widget,
                      GdkEventExpose *event)
{
    GwyGraphArea *area;
    GdkDrawable *drawable;

    gwy_debug("");

    area = GWY_GRAPH_AREA(widget);
    drawable = GTK_LAYOUT(widget)->bin_window;

    gdk_gc_set_rgb_fg_color(area->gc, &widget->style->white);
    gdk_draw_rectangle(drawable, area->gc, TRUE,
                       0, 0,
                       widget->allocation.width, widget->allocation.height);
    gdk_gc_set_rgb_fg_color(area->gc, &widget->style->black);

    gwy_graph_area_draw_area_on_drawable(drawable, area->gc,
                                         0, 0,
                                         widget->allocation.width,
                                         widget->allocation.height,
                                         area);

    if (area->status == GWY_GRAPH_STATUS_ZOOM
        && (area->selecting != 0))
        gwy_graph_area_draw_zoom(drawable, area->gc, area);

    gtk_widget_queue_draw(GTK_WIDGET(area->lab));
    GTK_WIDGET_CLASS(gwy_graph_area_parent_class)->expose_event(widget, event);

    return TRUE;
}

/**
 * gwy_graph_area_draw_area_on_drawable:
 * @drawable: a #GdkDrawable (destination for graphics operations)
 * @gc: a #GdkGC graphics context
 * @x: X position in @drawable where the graph area should be drawn
 * @y: Y position in @drawable where the graph area should be drawn
 * @width: width of the graph area on the drawable
 * @height: height of the graph area on the drawable
 * @area: the graph area to draw
 *
 * Draws the graph area to a #GdkDrawable.
 **/
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
    specs.real_xmin = gwy_graph_model_get_xmin(area->graph_model);
    specs.real_ymin = gwy_graph_model_get_ymin(area->graph_model);
    specs.real_width = gwy_graph_model_get_xmax(area->graph_model)
        - gwy_graph_model_get_xmin(area->graph_model);
    specs.real_height = gwy_graph_model_get_ymax(area->graph_model)
        - gwy_graph_model_get_ymin(area->graph_model);

    specs.log_x = gwy_graph_model_get_direction_logarithmic(model,
                                                 GTK_ORIENTATION_HORIZONTAL);
    specs.log_y = gwy_graph_model_get_direction_logarithmic(model,
                                                 GTK_ORIENTATION_VERTICAL);
    /*draw continuous selection*/
    if (area->status == GWY_GRAPH_STATUS_XSEL)
        gwy_graph_draw_selection_xareas(drawable, gc, &specs,
                                  GWY_SELECTION_GRAPH_1DAREA(area->xseldata));
    if (area->status == GWY_GRAPH_STATUS_YSEL)
        gwy_graph_draw_selection_yareas(drawable, gc, &specs,
                                  GWY_SELECTION_GRAPH_1DAREA(area->yseldata));


    /*FIXME gc should be different and should be set to gray drawing*/
    gwy_graph_draw_grid(drawable, gc, &specs,
                        area->x_grid_data, area->y_grid_data);

    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve(model, i);
        gwy_graph_draw_curve(drawable, gc,
                             &specs, G_OBJECT(curvemodel));
    }

    switch (area->status) {
        case GWY_GRAPH_STATUS_POINTS:
        case GWY_GRAPH_STATUS_ZOOM:
        gwy_graph_draw_selection_points
                                 (drawable, gc, &specs,
                                  GWY_SELECTION_GRAPH_POINT(area->pointsdata));
        break;

        case GWY_GRAPH_STATUS_XLINES:
        gwy_graph_draw_selection_lines
                                 (drawable, gc, &specs,
                                  GWY_SELECTION_GRAPH_LINE(area->xlinesdata),
                                  GTK_ORIENTATION_VERTICAL);
        break;

        case GWY_GRAPH_STATUS_YLINES:
        gwy_graph_draw_selection_lines
                                 (drawable, gc, &specs,
                                  GWY_SELECTION_GRAPH_LINE(area->ylinesdata),
                                  GTK_ORIENTATION_HORIZONTAL);
        break;


        default:
        /* PLAIN */
        break;
    }

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
    gint xmin, ymin, xmax, ymax, n_of_zooms;
    gdouble selection_zoomdata[4];

    n_of_zooms = gwy_selection_get_data(area->zoomdata, NULL);

    if (n_of_zooms != 1) return;

    gwy_selection_get_object(area->zoomdata, 0, selection_zoomdata);

    if (selection_zoomdata[2] == 0 || selection_zoomdata[3] == 0) return;
    gdk_gc_set_function(gc, GDK_INVERT);

    if (selection_zoomdata[2] < 0) {
        xmin = data_to_scr_x(GTK_WIDGET(area), selection_zoomdata[0]
                             + selection_zoomdata[2]);
        xmax = data_to_scr_x(GTK_WIDGET(area), selection_zoomdata[0]);
    }
    else {
        xmin = data_to_scr_x(GTK_WIDGET(area), selection_zoomdata[0]);
        xmax = data_to_scr_x(GTK_WIDGET(area), selection_zoomdata[0]
                             + selection_zoomdata[2]);
    }

    if (selection_zoomdata[3] > 0) {
        ymin = data_to_scr_y(GTK_WIDGET(area), selection_zoomdata[1]
                             + selection_zoomdata[3]);
        ymax = data_to_scr_y(GTK_WIDGET(area), selection_zoomdata[1]);
    }
    else {
        ymin = data_to_scr_y(GTK_WIDGET(area), selection_zoomdata[1]);
        ymax = data_to_scr_y(GTK_WIDGET(area), selection_zoomdata[1]
                             + selection_zoomdata[3]);
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
    GtkAllocation *allocation;
    GtkWidget *child;
    gint x, y, curve, selection, nc;
    gdouble dx, dy, selection_pointdata[2], selection_areadata[2];
    gdouble selection_zoomdata[4];

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
        if (event->type == GDK_2BUTTON_PRESS
            && area->enable_user_input == TRUE) {
            _gwy_graph_label_dialog_set_graph_data(area->label_dialog,
                                                   G_OBJECT(gmodel));
            gtk_widget_show_all(GTK_WIDGET(area->label_dialog));
        }
        else {
            area->active = child;
            area->x0 = x;
            area->y0 = y;
            area->xoff = 0;
            area->yoff = 0;
            allocation = &area->active->allocation;
            area->rxoff = x - allocation->x;
            area->ryoff = y - allocation->y;
            gwy_graph_area_draw_child_rectangle(area);
        }
        return FALSE;
    }

    if (area->status == GWY_GRAPH_STATUS_PLAIN && nc > 0
        && area->enable_user_input == TRUE) {
        curve = gwy_graph_area_find_curve(area, dx, dy);
        if (curve >= 0) {
            cmodel = gwy_graph_model_get_curve(gmodel, curve);
            _gwy_graph_area_dialog_set_curve_data(GTK_WIDGET(area->area_dialog),
                                                  cmodel);
            gtk_widget_show_all(GTK_WIDGET(area->area_dialog));
            gtk_window_present(GTK_WINDOW(area->area_dialog));
        }
    }

    if (area->status == GWY_GRAPH_STATUS_POINTS
        && gwy_selection_get_max_objects(area->pointsdata) == 1)
        gwy_selection_clear(area->pointsdata);

    if (area->status == GWY_GRAPH_STATUS_XLINES
        && gwy_selection_get_max_objects(area->xlinesdata) == 1)
        gwy_selection_clear(area->xlinesdata);

    if (area->status == GWY_GRAPH_STATUS_YLINES
        && gwy_selection_get_max_objects(area->ylinesdata) == 1)
        gwy_selection_clear(area->ylinesdata);

    if (area->status == GWY_GRAPH_STATUS_YSEL
        && gwy_selection_get_max_objects(area->yseldata) == 1)
        gwy_selection_clear(area->yseldata);

    if (area->status == GWY_GRAPH_STATUS_POINTS) {
        if (event->button == 1)
        {
            area->selected_object_index = gwy_graph_area_find_point(area, dx, dy);

            if (!(gwy_selection_is_full(area->pointsdata) &&
                area->selected_object_index == -1))
            {

                selection_pointdata[0] = dx;
                selection_pointdata[1] = dy;
                area->selecting = TRUE;
                gwy_selection_set_object(area->pointsdata,
                                     area->selected_object_index, selection_pointdata);
                if (area->selected_object_index == -1)
                    area->selected_object_index =
                                gwy_selection_get_data(area->pointsdata, NULL) - 1;
            }
        }
        else
        {
            selection = gwy_graph_area_find_point(area, dx, dy);
            if (selection >= 0)
                gwy_selection_delete_object(area->pointsdata,
                                            selection);
            gwy_selection_finished(area->pointsdata);
        }
    }

    if (area->status == GWY_GRAPH_STATUS_XSEL) {
        if (event->button == 1)
        {
            area->selected_object_index = gwy_graph_area_find_selection(area,
                                                                   dx, dy, &area->selected_border);
            if (gwy_selection_get_max_objects(area->xseldata) == 1
                                        && (area->selected_object_index == -1))
                gwy_selection_clear(area->xseldata);

            if (!(gwy_selection_is_full(area->xseldata) && area->selected_object_index == -1))
            {
                if (area->selected_object_index != -1) {
                    gwy_selection_get_object(area->xseldata,
                                     area->selected_object_index, selection_areadata);
                }

                if (area->selected_border == BORDER_MIN || area->selected_object_index == -1)
                {
                    selection_areadata[0] = dx;
                }
                if (area->selected_border == BORDER_NONE || area->selected_border == BORDER_MAX)
                {
                    selection_areadata[1] = dx;
                }
                gwy_selection_set_object(area->xseldata,
                                     area->selected_object_index, selection_areadata);
                area->selecting = TRUE;

                if (area->selected_object_index == -1)
                    area->selected_object_index =
                                gwy_selection_get_data(area->xseldata, NULL) - 1;
             }
        }
        else /*remove selection*/
        {
            selection = gwy_graph_area_find_selection(area, dx, dy, &area->selected_border);
            if (selection >= 0)
                gwy_selection_delete_object(area->xseldata,
                                            selection);
            gwy_selection_finished(area->xseldata);
        }
    }
    if (area->status == GWY_GRAPH_STATUS_YSEL) {
        if (event->button == 1
            && !gwy_selection_is_full(area->yseldata)) /*add selection*/
        {
            selection_areadata[0] = dy;
            selection_areadata[1] = dy;
            gwy_selection_set_object(area->yseldata,
                                     -1, selection_areadata);
            area->selecting = TRUE;
        }
        else /*remove selection*/
        {
            selection = gwy_graph_area_find_selection(area, dx, dy, &area->selected_border);
            if (selection >= 0)
                gwy_selection_delete_object(area->yseldata,
                                            selection);
            gwy_selection_finished(area->yseldata);
        }
    }


    if (area->status == GWY_GRAPH_STATUS_XLINES)
    {
        if (event->button == 1)
        {
            area->selected_object_index = gwy_graph_area_find_line(area, dx);

            if (!(gwy_selection_is_full(area->xlinesdata) &&
                area->selected_object_index == -1))
            {
                gwy_selection_set_object(area->xlinesdata,
                                     area->selected_object_index,
                                     &dx);

                area->selecting = TRUE;
                if (area->selected_object_index == -1)
                    area->selected_object_index =
                                gwy_selection_get_data(area->xlinesdata, NULL) - 1;
            }

        }
        else {
            selection = gwy_graph_area_find_line(area, dx);
            if (selection >= 0)
                gwy_selection_delete_object(area->xlinesdata,
                                            selection);
        }
    }

    if (area->status == GWY_GRAPH_STATUS_YLINES)
    {
        if (event->button == 1)
        {
            area->selected_object_index = gwy_graph_area_find_line(area, dy);

            if (!(gwy_selection_is_full(area->ylinesdata) &&
                area->selected_object_index == -1))
            {
                gwy_selection_set_object(area->ylinesdata,
                                     area->selected_object_index,
                                     &dy);

                area->selecting = TRUE;
                if (area->selected_object_index == -1)
                    area->selected_object_index =
                                gwy_selection_get_data(area->ylinesdata, NULL) - 1;
            }

        }
        else {
            selection = gwy_graph_area_find_line(area, dy);
            if (selection >= 0)
                gwy_selection_delete_object(area->ylinesdata,
                                            selection);
        }
    }

    if (area->status == GWY_GRAPH_STATUS_ZOOM)
    {
        gwy_selection_clear(area->zoomdata);
        selection_zoomdata[0] = dx;
        selection_zoomdata[1] = dy;
        selection_zoomdata[2] = 0;
        selection_zoomdata[3] = 0;
        gwy_selection_set_object(area->zoomdata, -1, selection_zoomdata);

        area->selecting = TRUE;
    }

    return TRUE;
}

static gboolean
gwy_graph_area_button_release(GtkWidget *widget, GdkEventButton *event)
{
    GwyGraphArea *area;
    GwyGraphModel *gmodel;
    gint x, y, ispos = 0, nselected;
    gdouble dx, dy, selection_pointdata[2], selection_areadata[2];
    gdouble selection_zoomdata[4], selection_linedata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);

    area = GWY_GRAPH_AREA(widget);
    gdk_window_get_position(event->window, &x, &y);
    x += (gint)event->x;
    y += (gint)event->y;
    dx = scr_to_data_x(widget, x);
    dy = scr_to_data_y(widget, y);

    gmodel = GWY_GRAPH_MODEL(area->graph_model);

    switch (area->status) {
        case GWY_GRAPH_STATUS_XSEL:
        if (area->selecting &&
            gwy_selection_get_object(area->xseldata,
                                     area->selected_object_index,
                                     selection_areadata)) {
            if (area->selected_border == BORDER_MIN)
                selection_areadata[0] = dx;
            if (area->selected_border == BORDER_NONE
                || area->selected_border == BORDER_MAX)
                selection_areadata[1] = dx;

            gwy_selection_set_object(area->xseldata,
                                     area->selected_object_index,
                                     selection_areadata);
            if (selection_areadata[1] == selection_areadata[0])
                gwy_selection_delete_object(area->xseldata,
                                            area->selected_object_index);
            area->selecting = FALSE;
        }
        break;

        case GWY_GRAPH_STATUS_YSEL:
        nselected = gwy_selection_get_data(area->yseldata, NULL);
        if (area->selecting && nselected) {
            gwy_selection_get_object(area->yseldata, nselected - 1,
                                     selection_areadata);
            selection_areadata[1] = dy;

            gwy_selection_set_object(area->yseldata, nselected - 1,
                                     selection_areadata);
            if (selection_areadata[2] == selection_areadata[0]
                || selection_areadata[3] == selection_areadata[1])
                gwy_selection_delete_object(area->yseldata, nselected - 1);
            area->selecting = FALSE;
        }
        break;

        case GWY_GRAPH_STATUS_XLINES:
        if (area->selecting && gwy_selection_get_data(area->xlinesdata, NULL)) {
            selection_linedata = dx;
            area->selecting = FALSE;
            gwy_selection_set_object(area->xlinesdata,
                                     area->selected_object_index,
                                     &selection_linedata);
            gwy_selection_finished(area->xlinesdata);
        }
        break;

        case GWY_GRAPH_STATUS_YLINES:
        if (area->selecting && gwy_selection_get_data(area->ylinesdata, NULL)) {
            selection_linedata = dy;
            area->selecting = FALSE;
            gwy_selection_set_object(area->ylinesdata,
                                     area->selected_object_index,
                                     &selection_linedata);
            gwy_selection_finished(area->ylinesdata);
        }
        break;

        case GWY_GRAPH_STATUS_POINTS:
        if (area->selecting) {
            selection_pointdata[0] = dx;
            selection_pointdata[1] = dy;
            gwy_selection_set_object(area->pointsdata,
                                     area->selected_object_index,
                                     selection_pointdata);
            area->selecting = FALSE;
            gwy_selection_finished(area->pointsdata);
        }

        case GWY_GRAPH_STATUS_ZOOM:
        nselected = gwy_selection_get_data(area->zoomdata, NULL);
        if (area->selecting && nselected) {
            gwy_selection_get_object(area->zoomdata, nselected - 1,
                                     selection_zoomdata);

            selection_zoomdata[2] = dx - selection_zoomdata[0];
            selection_zoomdata[3] = dy - selection_zoomdata[1];

            gwy_selection_set_object(area->zoomdata, nselected - 1,
                                     selection_zoomdata);

            area->selecting = FALSE;
            gwy_selection_finished(area->zoomdata);
        }
        break;

        default:
        /* PLAIN */
        break;
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
        area->rx0 = ((gdouble)event->x - area->rxoff)/(gdouble)area->old_width;
        area->ry0 = ((gdouble)event->y - area->ryoff)/(gdouble)area->old_height;
        
        area->active = NULL;
    }
    return FALSE;
}

static gboolean
gwy_graph_area_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    GwyGraphArea *area;
    GwyGraphModel *gmodel;
    GdkWindow *window;
    gint x, y, wx, wy, ispos = 0, border, nselected;
    gdouble dx, dy, selection_pointdata[2], selection_areadata[2];
    gdouble selection_zoomdata[4], selection_linedata;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH_AREA(widget), FALSE);

    area = GWY_GRAPH_AREA(widget);
    if (event->is_hint)
        gdk_window_get_pointer(widget->window, &x, &y, NULL);
    else {
        x = event->x;
        y = event->y;
    }
    gdk_window_get_position(event->window, &wx, &wy);
    x += wx;
    y += wy;
    dx = scr_to_data_x(widget, x);
    dy = scr_to_data_y(widget, y);

    gmodel = GWY_GRAPH_MODEL(area->graph_model);

    area->mouse_present = TRUE;
    area->actual_cursor_data->data_point.x = dx;
    area->actual_cursor_data->data_point.y = dy;

    window = widget->window;

    switch (area->status) {
        case GWY_GRAPH_STATUS_XSEL:
        gwy_graph_area_find_selection(area, dx, dy, &border);
        if (area->selecting || border != BORDER_NONE)
            gdk_window_set_cursor(window, area->harrow_cursor);
        else
            gdk_window_set_cursor(window, area->cross_cursor);

        if (area->selecting
            && gwy_selection_get_object(area->xseldata,
                                        area->selected_object_index,
                                        selection_areadata)) {
            if (area->selected_border == BORDER_MIN)
                selection_areadata[0] = dx;
            else if (area->selected_border == BORDER_NONE
                || area->selected_border == BORDER_MAX)
                selection_areadata[1] = dx;

            gwy_selection_set_object(area->xseldata,
                                     area->selected_object_index,
                                     selection_areadata);
        }
        break;

        case GWY_GRAPH_STATUS_YSEL:
        nselected = gwy_selection_get_data(area->yseldata, NULL);
        if (area->selecting && nselected) {
            gwy_selection_get_object(area->yseldata, nselected - 1,
                                     selection_areadata);
            selection_areadata[1] = dy;
            gwy_selection_set_object(area->yseldata, nselected - 1,
                                     selection_areadata);
        }
        break;

        case GWY_GRAPH_STATUS_XLINES:
        if (area->selecting || gwy_graph_area_find_line(area, dx) != -1)
            gdk_window_set_cursor(window, area->harrow_cursor);
        else
            gdk_window_set_cursor(window, area->cross_cursor);

        if (area->selecting && gwy_selection_get_data(area->xlinesdata, NULL)) {
            selection_linedata = dx;

            gwy_selection_set_object(area->xlinesdata,
                                     area->selected_object_index,
                                     &selection_linedata);
            gwy_selection_finished(area->xlinesdata);
        }
        break;

        case GWY_GRAPH_STATUS_YLINES:
        if (area->selecting || gwy_graph_area_find_line(area, dy) != -1)
            gdk_window_set_cursor(window, area->varrow_cursor);
        else
            gdk_window_set_cursor(window, area->cross_cursor);

        if (area->selecting && gwy_selection_get_data(area->ylinesdata, NULL)) {
            selection_linedata = dy;

            gwy_selection_set_object(area->ylinesdata,
                                     area->selected_object_index,
                                     &selection_linedata);
            gwy_selection_finished(area->ylinesdata);
        }
        break;

        case GWY_GRAPH_STATUS_POINTS:
        if (area->selecting || gwy_graph_area_find_point(area, dx, dy) != -1)
            gdk_window_set_cursor(window, area->fleur_cursor);
        else
            gdk_window_set_cursor(window, area->cross_cursor);

        if (area->selecting) {
            selection_pointdata[0] = dx;
            selection_pointdata[1] = dy;
            gwy_selection_set_object(area->pointsdata,
                                     area->selected_object_index,
                                     selection_pointdata);
        }
        break;

        case GWY_GRAPH_STATUS_ZOOM:
        nselected = gwy_selection_get_data(area->zoomdata, NULL);
        if (area->selecting && nselected) {
            gwy_selection_get_object(area->zoomdata, nselected - 1,
                                     selection_zoomdata);

            selection_zoomdata[2] = dx - selection_zoomdata[0];
            selection_zoomdata[3] = dy - selection_zoomdata[1];

            gwy_selection_set_object(area->zoomdata, nselected - 1,
                                     selection_zoomdata);
        }
        break;

        default:
        /* PLAIN */
        break;
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
       

        area->rx0 = ((gdouble)event->x - area->rxoff)/(gdouble)area->old_width;
        area->ry0 = ((gdouble)event->y - area->ryoff)/(gdouble)area->old_height;
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
        curvemodel = gwy_graph_model_get_curve(model, i);
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
    if (fabs(closestdistance/(gwy_graph_model_get_ymax(model)
                              - gwy_graph_model_get_ymin(model))) < 0.05)
        return closestid;
    else
        return -1;
}

static gint
gwy_graph_area_find_selection(GwyGraphArea *area, gdouble x, gdouble y, int *btype)
{
    gint i;
    GwyGraphModel *model;
    gdouble selection_areadata[2];
    gdouble min, max, xoff, yoff;

    model = GWY_GRAPH_MODEL(area->graph_model);
    xoff = (gwy_graph_model_get_xmax(model)
            - gwy_graph_model_get_xmin(model))/50;
    yoff = (gwy_graph_model_get_ymax(model)
            - gwy_graph_model_get_ymin(model))/50;


    *btype = BORDER_NONE;
    if (area->status == GWY_GRAPH_STATUS_XSEL)
    {
        for (i = 0; i < gwy_selection_get_data(area->xseldata, NULL); i++) {
            gwy_selection_get_object(area->xseldata,
                                 gwy_selection_get_data(area->xseldata, NULL) - 1,
                                 selection_areadata);
            min = MIN(selection_areadata[0], selection_areadata[1]);
            max = MAX(selection_areadata[0], selection_areadata[1]);

            if (min < x && min >= (x-xoff)) {
                *btype = BORDER_MIN;
                return i;
            } else if (max > x && max <= (x+xoff)) {
                *btype = BORDER_MAX;
                return i;
            } else if (min < x && max > x) return i;
        }
    }
    else if (area->status == GWY_GRAPH_STATUS_YSEL)
    {
        for (i = 0; i < gwy_selection_get_data(area->yseldata, NULL); i++) {
            gwy_selection_get_object(area->yseldata,
                                 gwy_selection_get_data(area->yseldata, NULL) - 1,
                                 selection_areadata);
            min = MIN(selection_areadata[0], selection_areadata[1]);
            max = MAX(selection_areadata[0], selection_areadata[1]);

            if (min < y && max > y) return i;
        }

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
    xoff = (gwy_graph_model_get_xmax(model)
            - gwy_graph_model_get_xmin(model))/50;
    yoff = (gwy_graph_model_get_ymax(model)
            - gwy_graph_model_get_ymin(model))/50;

    for (i = 0; i < gwy_selection_get_data(area->pointsdata, NULL); i++) {
        gwy_selection_get_object(area->pointsdata,
                                 i, selection_data);

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

    if (area->status == GWY_GRAPH_STATUS_XLINES) {

        xoff = (gwy_graph_model_get_xmax(model)
            - gwy_graph_model_get_xmin(model))/100;

        for (i = 0; i < gwy_selection_get_data(area->xlinesdata, NULL); i++) {
            gwy_selection_get_object(area->xlinesdata,
                                 i, &selection_data);

            min = selection_data - xoff;
            max = selection_data + xoff;
            if (min <= position && max >= position)
                return i;
        }
    }
    else if (area->status == GWY_GRAPH_STATUS_YLINES) {

        yoff = (gwy_graph_model_get_ymax(model)
            - gwy_graph_model_get_ymin(model))/100;


        for (i = 0; i < gwy_selection_get_data(area->ylinesdata, NULL); i++) {
            gwy_selection_get_object(area->ylinesdata,
                                 i, &selection_data);

            min = selection_data - yoff;
            max = selection_data + yoff;
            if (min <= position && max >= position)
                return i;
        }
     }
    return -1;
}

static GtkWidget*
gwy_graph_area_find_child(GwyGraphArea *area, gint x, gint y)
{
    GList *children, *l;

    children = gtk_container_get_children(GTK_CONTAINER(area));
    for (l = children; l; l = g_list_next(l)) {
        GtkWidget *child;
        GtkAllocation *alloc;

        child = GTK_WIDGET(l->data);
        alloc = &child->allocation;
        if (x >= alloc->x && x < alloc->x + alloc->width
            && y >= alloc->y && y < alloc->y + alloc->height) {
            g_list_free(children);
            return child;
        }
    }
    g_list_free(children);
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
    if (!gwy_graph_model_get_direction_logarithmic(model,
                                                   GTK_ORIENTATION_HORIZONTAL))
    {
        return gwy_graph_model_get_xmin(model)
           + scr*(gwy_graph_model_get_xmax(model)
                  - gwy_graph_model_get_xmin(model))/(widget->allocation.width-1);
    }
    else
        return pow(10, log10(gwy_graph_model_get_xmin(model))
           + scr*(log10(gwy_graph_model_get_xmax(model)
                  - log10(gwy_graph_model_get_xmin(model))))/(widget->allocation.width-1));
}

static gint
data_to_scr_x(GtkWidget *widget, gdouble data)
{
    GwyGraphArea *area;
    GwyGraphModel *model;

    area = GWY_GRAPH_AREA(widget);
    model = GWY_GRAPH_MODEL(area->graph_model);

    if (!gwy_graph_model_get_direction_logarithmic(model,
                                                   GTK_ORIENTATION_HORIZONTAL))
        return (data - gwy_graph_model_get_xmin(model))
           /((gwy_graph_model_get_xmax(model)
              - gwy_graph_model_get_xmin(model))/(widget->allocation.width-1));
    else
       return (log10(data) - log10(gwy_graph_model_get_xmin(model)))
           /((log10(gwy_graph_model_get_xmax(model))
              - log10(gwy_graph_model_get_xmin(model)))/(widget->allocation.width-1));
}

static gdouble
scr_to_data_y(GtkWidget *widget, gint scr)
{
    GwyGraphArea *area;
    GwyGraphModel *model;

    area = GWY_GRAPH_AREA(widget);
    model = GWY_GRAPH_MODEL(area->graph_model);

    scr = CLAMP(scr, 0, widget->allocation.height-1);
    if (!gwy_graph_model_get_direction_logarithmic(model, GTK_ORIENTATION_VERTICAL))
        return gwy_graph_model_get_ymin(model)
           + (widget->allocation.height - scr)*
           (gwy_graph_model_get_ymax(model) - gwy_graph_model_get_ymin(model))
             /(widget->allocation.height-1);
    else
        return pow(10, log10(gwy_graph_model_get_ymin(model))
                   + (widget->allocation.height - scr)*
                   (log10(gwy_graph_model_get_ymax(model)) - log10(gwy_graph_model_get_ymin(model)))
                                                        /(widget->allocation.height-1));
}

static gint
data_to_scr_y(GtkWidget *widget, gdouble data)
{
    GwyGraphArea *area;
    GwyGraphModel *model;

    area = GWY_GRAPH_AREA(widget);
    model = GWY_GRAPH_MODEL(area->graph_model);
    if (!gwy_graph_model_get_direction_logarithmic(model, GTK_ORIENTATION_VERTICAL))
        return widget->allocation.height
           - (data - gwy_graph_model_get_ymin(model))
             /((gwy_graph_model_get_ymax(model)
                - gwy_graph_model_get_ymin(model))/((gdouble)widget->allocation.height-1));
    else
        return widget->allocation.height
            - (log10(data) - log10(gwy_graph_model_get_ymin(model)))
            /((log10(gwy_graph_model_get_ymax(model))
               - log10(gwy_graph_model_get_ymin(model)))/((gdouble)widget->allocation.height-1));
}


/**
 * gwy_graph_area_refresh:
 * @area: graph area
 *
 * Refreshes the area with respect to graph model.
 **/
void
gwy_graph_area_refresh(GwyGraphArea *area)
{
    /*refresh label*/
    if (gwy_graph_model_get_label_visible(GWY_GRAPH_MODEL(area->graph_model)))
    {
        gtk_widget_show(GTK_WIDGET(area->lab));
        gwy_graph_label_refresh(area->lab);
    }
    else
        gtk_widget_hide(GTK_WIDGET(area->lab));

    /*repaint area data*/
    gtk_widget_queue_draw(GTK_WIDGET(area));
}

static void    
label_geometry_changed_cb(GtkWidget *area,
                          GtkAllocation *label_allocation)
{
    gwy_graph_area_repos_label(GWY_GRAPH_AREA(area), &(area->allocation), label_allocation);
}


/**
 * gwy_graph_area_set_model:
 * @area: graph area
 * @gmodel: new graph model
 *
 * Changes the graph model. Calls refresh afterwards. @gmodel
 * is duplicated.
 **/
void
gwy_graph_area_set_model(GwyGraphArea *area, gpointer gmodel)
{
    gint i;

    area->graph_model = gmodel;
    gwy_graph_label_set_model(area->lab, gmodel);

    g_signal_connect_swapped(GWY_GRAPH_MODEL(gmodel), "notify",
    G_CALLBACK(gwy_graph_area_refresh), area);

    for (i = 0; i < gwy_graph_model_get_n_curves(GWY_GRAPH_MODEL(gmodel)); i++)
    {
            g_signal_connect_swapped(
                             gwy_graph_model_get_curve(GWY_GRAPH_MODEL(gmodel), i),
                             "notify",
                             G_CALLBACK(gwy_graph_area_refresh), area);
    }
 

    gwy_graph_area_refresh(area);
}

static void
gwy_graph_area_entry_cb(GwyGraphAreaDialog *dialog,
                        gint response_id,
                        G_GNUC_UNUSED gpointer user_data)
{
    if (response_id == GTK_RESPONSE_CLOSE)
        gtk_widget_hide(GTK_WIDGET(dialog));
}

static void
gwy_graph_label_entry_cb(GwyGraphLabelDialog *dialog,
                         gint response_id,
                         G_GNUC_UNUSED gpointer user_data)
{
    if (response_id == GTK_RESPONSE_CLOSE)
        gtk_widget_hide(GTK_WIDGET(dialog));
}


/**
 * gwy_graph_area_enable_user_input:
 * @area: graph area
 * @enable: enable/disable user input
 *
 * Enables/disables all the user input dialogs (to be invoked by clicking the mouse).
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
   * Gets mouse cursor related values within graph area.
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

static gboolean
gwy_graph_area_leave_notify(GtkWidget *widget, G_GNUC_UNUSED GdkEventCrossing *event)
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

/**
 * gwy_graph_area_export_vector:
 * @area: the graph area to export
 *
 **/
GString*
gwy_graph_area_export_vector(GwyGraphArea *area,
                                      gint x, gint y,
                                      gint width, gint height)
{
    gint i, j, nc;
    GwyGraphCurveModel *curvemodel;
    GwyGraphModel *model;
    GString *out;
    gdouble xmult, ymult;
    const GwyRGBA *color;
    gint pointsize;
    gint linesize;

    out = g_string_new("%%Area\n");

    model = GWY_GRAPH_MODEL(area->graph_model);
    if ((gwy_graph_model_get_xmax(model) - gwy_graph_model_get_xmin(model))==0
        || (gwy_graph_model_get_ymax(model) - gwy_graph_model_get_ymin(model))==0)
    {
        g_warning("Graph null range.\n");
        return out;
    }

    xmult = width/(gwy_graph_model_get_xmax(model) - gwy_graph_model_get_xmin(model));
    ymult = height/(gwy_graph_model_get_ymax(model) - gwy_graph_model_get_ymin(model));

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

    /*plot grid*/
    /*
    g_string_append_printf(out, "%d setlinewidth\n", 1);
    for (i = 0; i < area->x_grid_data->len; i++) {
        pvalue = &g_array_index(area->x_grid_data, gdouble, i);
        pos = (gint)((*pvalue)*ymult) + y;
        g_string_append_printf(out, "%d %d M\n", x, height - pos);
        g_string_append_printf(out, "%d %d L\n", x + width, height - pos);
        g_string_append_printf(out, "stroke\n");
    }

    for (i = 0; i < area->y_grid_data->len; i++) {
        pvalue = &g_array_index(area->y_grid_data, gdouble, i);
        pos = (gint)((*pvalue)*xmult) + x;
        g_string_append_printf(out, "%d %d M\n", pos, y);
        g_string_append_printf(out, "%d %d L\n", pos, y + height);
        g_string_append_printf(out, "stroke\n");
    }
    */



    nc = gwy_graph_model_get_n_curves(model);
    for (i = 0; i < nc; i++) {
        curvemodel = gwy_graph_model_get_curve(model, i);
        pointsize = gwy_graph_curve_model_get_point_size(curvemodel);
        linesize = gwy_graph_curve_model_get_line_size(curvemodel);
        color = gwy_graph_curve_model_get_color(curvemodel);
        g_string_append_printf(out, "/hpt %d def\n", pointsize);
        g_string_append_printf(out, "/vpt %d def\n", pointsize);
        g_string_append_printf(out, "/hpt2 hpt 2 mul def\n");
        g_string_append_printf(out, "/vpt2 vpt 2 mul def\n");
        g_string_append_printf(out, "%d setlinewidth\n", linesize);
        g_string_append_printf(out, "%f %f %f setrgbcolor\n",
                               color->r, color->g, color->b);

        for (j = 0; j < curvemodel->n - 1; j++) {
            if (curvemodel->mode == GWY_GRAPH_CURVE_LINE
                || curvemodel->mode == GWY_GRAPH_CURVE_LINE_POINTS)
            {
                if (j==0) g_string_append_printf(out, "%d %d M\n",
                                   (gint)(x + (curvemodel->xdata[j]
                                               - gwy_graph_model_get_xmin(model))*xmult),
                                   (gint)(y + (curvemodel->ydata[j]
                                               - gwy_graph_model_get_ymin(model))*ymult));
                else
                {
                    g_string_append_printf(out, "%d %d M\n",
                                   (gint)(x + (curvemodel->xdata[j-1]
                                               - gwy_graph_model_get_xmin(model))*xmult),
                                   (gint)(y + (curvemodel->ydata[j-1]
                                               - gwy_graph_model_get_ymin(model))*ymult));
                    g_string_append_printf(out, "%d %d L\n",
                                   (gint)(x + (curvemodel->xdata[j]
                                               - gwy_graph_model_get_xmin(model))*xmult),
                                   (gint)(y + (curvemodel->ydata[j]
                                               - gwy_graph_model_get_ymin(model))*ymult));
                }
            }
            if (curvemodel->mode == GWY_GRAPH_CURVE_POINTS
                || curvemodel->mode == GWY_GRAPH_CURVE_LINE_POINTS)
            {
                g_string_append_printf(out, "%d %d %s\n",
                          (gint)(x + (curvemodel->xdata[j]
                                      - gwy_graph_model_get_xmin(model))*xmult),
                          (gint)(y + (curvemodel->ydata[j]
                                      - gwy_graph_model_get_ymin(model))*ymult),
                          symbols[curvemodel->point_type]);

            }
        }
        g_string_append_printf(out, "stroke\n");
    }
    g_string_append_printf(out, "grestore\n");

    /*plot boundary*/
    g_string_append_printf(out, "%d setlinewidth\n", 2);
    g_string_append_printf(out, "%d %d M\n", x, y);
    g_string_append_printf(out, "%d %d L\n", x + width, y);
    g_string_append_printf(out, "%d %d L\n", x + width, y + height);
    g_string_append_printf(out, "%d %d L\n", x, y + height);
    g_string_append_printf(out, "%d %d L\n", x, y);
    g_string_append_printf(out, "stroke\n");

    return out;
}


/**
 * gwy_graph_area_get_label:
 * @area: graph area
 *
 * Returns: the #GwyGraphLabel within @area (do not free).
 **/
/* XXX: Malformed documentation. */
GtkWidget*
gwy_graph_area_get_label(GwyGraphArea *area)
{
    return GTK_WIDGET(area->lab);
}

/**
 * gwy_graph_area_set_x_grid_data:
 * @area: graph area
 * @grid_data: array of grid data on the x axis
 *
 * Sets the grid data on the x-axis of the graph area
 **/
void
gwy_graph_area_set_x_grid_data(GwyGraphArea *area, GArray *grid_data)
{
    guint i;
    gdouble *pdata, *psetdata;
    g_array_set_size(area->x_grid_data, grid_data->len);

    for (i = 0; i < grid_data->len; i++){
        pdata = &g_array_index(area->x_grid_data, gdouble, i);
        psetdata = &g_array_index(grid_data, gdouble, i);
        *pdata = *psetdata;
    }
}

/**
 * gwy_graph_area_set_y_grid_data:
 * @area: graph area
 * @grid_data: array of grid data on the y axis
 *
 * Sets the grid data on the y-axis of the graph area
 **/
void
gwy_graph_area_set_y_grid_data(GwyGraphArea *area, GArray *grid_data)
{
    guint i;
    gdouble *pdata, *psetdata;
    g_array_set_size(area->y_grid_data, grid_data->len);

    for (i = 0; i < grid_data->len; i++){
        pdata = &g_array_index(area->y_grid_data, gdouble, i);
        psetdata = &g_array_index(grid_data, gdouble, i);
        *pdata = *psetdata;
    }
}

/**
 * gwy_graph_area_get_x_grid_data:
 * @area: graph area
 *
 * Returns: the grid data on the x-axis of the graph area
 * as a #GArray (do not free).
 **/
/* XXX: Malformed documentation. */
const GArray*
gwy_graph_area_get_x_grid_data(GwyGraphArea *area)
{
    return area->x_grid_data;
}

/**
 * gwy_graph_area_get_y_grid_data:
 * @area: graph area
 *
 * Returns: the grid data on the y-axis of the graph area
 * as a #GArray (do not free).
 **/
/* XXX: Malformed documentation. */
const GArray*
gwy_graph_area_get_y_grid_data(GwyGraphArea *area)
{

    return area->y_grid_data;
}


GwySelection*
gwy_graph_area_get_selection(GwyGraphArea *area, GwyGraphStatusType status_type)
{
    if (status_type == GWY_GRAPH_STATUS_PLAIN)
        status_type = area->status;

    switch (status_type) {
        case GWY_GRAPH_STATUS_PLAIN:
        return NULL;

        case GWY_GRAPH_STATUS_XSEL:
        return area->xseldata;

        case GWY_GRAPH_STATUS_YSEL:
        return area->yseldata;

        case GWY_GRAPH_STATUS_POINTS:
        return area->pointsdata;

        case GWY_GRAPH_STATUS_ZOOM:
        return area->zoomdata;

        case GWY_GRAPH_STATUS_XLINES:
        return area->xlinesdata;

        case GWY_GRAPH_STATUS_YLINES:
        return area->ylinesdata;
    }

    g_return_val_if_reached(NULL);
}

static void
selection_changed_cb(GwyGraphArea *area)
{
    gtk_widget_queue_draw(GTK_WIDGET(area));
}

void
gwy_graph_area_set_status(GwyGraphArea *area, GwyGraphStatusType status_type)
{
    area->status = status_type;
    g_signal_emit(area, area_signals[STATUS_CHANGED], 0, (gint)area->status);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygrapharea
 * @title: GwyGraphArea
 * @short_description: Layout for drawing graph curves
 *
 * #GwyGraphArea is the central part of #GwyGraph widget. It plots a set of
 * data curves with the given plot properties.
 *
 * It is recommended to use it within #GwyGraph, however, it can also be used
 * separately.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
