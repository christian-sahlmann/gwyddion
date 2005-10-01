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
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>

#include <stdio.h>
enum {
    SELECTED_SIGNAL,
    MOUSE_MOVED_SIGNAL,
    ZOOMED_SIGNAL,
    LAST_SIGNAL
};


static void gwy_graph_size_request (GtkWidget *widget,
                                    GtkRequisition *requisition);
static void gwy_graph_size_allocate(GtkWidget *widget,
                                    GtkAllocation *allocation);
static void rescaled_cb            (GtkWidget *widget,
                                    GwyGraph *graph);
static void replot_cb              (GObject *gobject,
                                    GParamSpec *arg1,
                                    GwyGraph *graph);
static void zoomed_cb              (GwyGraph *graph);
static void label_updated_cb       (GwyAxis *axis,
                                    GwyGraph *graph);
static void gwy_graph_finalize          (GObject *object);
static void gwy_graph_signal_mouse_moved(GwyGraph *graph);
static void gwy_graph_signal_zoomed     (GwyGraph *graph);



static guint gwygraph_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyGraph, gwy_graph, GTK_TYPE_TABLE)

static void
gwy_graph_class_init(GwyGraphClass *klass)
{
    GtkWidgetClass *widget_class;
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    
    widget_class = (GtkWidgetClass*)klass;

    widget_class->size_request = gwy_graph_size_request;
    widget_class->size_allocate = gwy_graph_size_allocate;
    gobject_class->finalize = gwy_graph_finalize;
    
    klass->selected = NULL;
    klass->mouse_moved = NULL;
    klass->zoomed = NULL;

    gwygraph_signals[SELECTED_SIGNAL]
                = g_signal_new("selected",
                               G_TYPE_FROM_CLASS(klass),
                               G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                               G_STRUCT_OFFSET(GwyGraphClass, selected),
                               NULL,
                               NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    gwygraph_signals[MOUSE_MOVED_SIGNAL]
                = g_signal_new("mouse-moved",
                               G_TYPE_FROM_CLASS(klass),
                               G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                               G_STRUCT_OFFSET(GwyGraphClass, mouse_moved),
                               NULL,
                               NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);

    gwygraph_signals[ZOOMED_SIGNAL]
                = g_signal_new("zoomed",
                               G_TYPE_FROM_CLASS(klass),
                               G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                               G_STRUCT_OFFSET(GwyGraphClass, zoomed),
                               NULL,
                               NULL,
                               g_cclosure_marshal_VOID__VOID,
                               G_TYPE_NONE, 0);
}


static void
gwy_graph_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(gwy_graph_parent_class)->size_request(widget, requisition);
    requisition->width = 300;
    requisition->height = 200;
}

static void
gwy_graph_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraph *graph;
    gwy_debug("");

    graph = GWY_GRAPH(widget);
    GTK_WIDGET_CLASS(gwy_graph_parent_class)->size_allocate(widget, allocation);
}


static void
gwy_graph_init(G_GNUC_UNUSED GwyGraph *graph)
{
    gwy_debug("");

}

static void 
gwy_graph_finalize(GObject *object)
{
    GwyGraph *graph = GWY_GRAPH(object);
    if (graph->graph_model) 
    {
        gwy_object_unref(graph->graph_model);
    }
}


/**
 * gwy_graph_new:
 * @gmodel: A graph model.
 *
 * Creates graph widget based on information in model.
 *
 * Returns: new graph widget.
 **/
GtkWidget*
gwy_graph_new(GwyGraphModel *gmodel)
{
    GwyGraph *graph = GWY_GRAPH(g_object_new(gwy_graph_get_type(), NULL));
    gwy_debug("");

    if (gmodel != NULL) 
    {
       graph->graph_model = gmodel;
       g_object_ref(gmodel);
    }
    
    gtk_table_resize(GTK_TABLE(graph), 3, 3);
    gtk_table_set_homogeneous(GTK_TABLE(graph), FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(graph), 0);
    gtk_table_set_col_spacings(GTK_TABLE(graph), 0);

    graph->grid_type = GWY_GRAPH_GRID_AUTO;   

    if (gmodel != NULL) {
        graph->axis_top
            = GWY_AXIS(gwy_axis_new(GTK_POS_TOP, 2.24, 5.21,
                                    graph->graph_model->top_label->str));
        graph->axis_bottom
            = GWY_AXIS(gwy_axis_new(GTK_POS_BOTTOM, 2.24, 5.21,
                                    graph->graph_model->bottom_label->str));
        graph->axis_left
            = GWY_AXIS(gwy_axis_new(GTK_POS_LEFT, 100, 500,
                                    graph->graph_model->left_label->str));
        graph->axis_right
            = GWY_AXIS(gwy_axis_new(GTK_POS_RIGHT, 100, 500,
                                    graph->graph_model->right_label->str));
    }

    gwy_graph_set_axis_visible(graph, GTK_POS_LEFT, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_TOP, FALSE);
    
    g_signal_connect(graph->axis_left, "rescaled",
                     G_CALLBACK(rescaled_cb), graph);
    g_signal_connect(graph->axis_bottom, "rescaled",
                     G_CALLBACK(rescaled_cb), graph);

    g_signal_connect(graph->axis_left, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);
    g_signal_connect(graph->axis_right, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);
    g_signal_connect(graph->axis_top, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);
    g_signal_connect(graph->axis_bottom, "label-updated",
                     G_CALLBACK(label_updated_cb), graph);


    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_top),
                     1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_bottom),
                     1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_left),
                     2, 3, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis_right),
                     0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show(GTK_WIDGET(graph->axis_top));
    gtk_widget_show(GTK_WIDGET(graph->axis_bottom));
    gtk_widget_show(GTK_WIDGET(graph->axis_left));
    gtk_widget_show(GTK_WIDGET(graph->axis_right));

    graph->corner_tl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_bl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_tr = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_br = GWY_GRAPH_CORNER(gwy_graph_corner_new());


    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_tl), 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_bl), 2, 3, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_tr), 0, 1, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner_br), 2, 3, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);

    gtk_widget_show(GTK_WIDGET(graph->corner_tl));
    gtk_widget_show(GTK_WIDGET(graph->corner_bl));
    gtk_widget_show(GTK_WIDGET(graph->corner_tr));
    gtk_widget_show(GTK_WIDGET(graph->corner_br));

    graph->area = GWY_GRAPH_AREA(gwy_graph_area_new(NULL, NULL));

    graph->area->status = GWY_GRAPH_STATUS_PLAIN;
    graph->enable_user_input = TRUE;

    g_signal_connect_swapped(graph->area, "selected",
                     G_CALLBACK(gwy_graph_signal_selected), graph);

    g_signal_connect_swapped(graph->area, "mouse-moved",
                     G_CALLBACK(gwy_graph_signal_mouse_moved), graph);

    g_signal_connect_swapped(graph->area, "zoomed",
                     G_CALLBACK(zoomed_cb), graph);

    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     0, 0);

    gtk_widget_show_all(GTK_WIDGET(graph->area));

    if (gmodel != NULL) {
       gwy_graph_set_model(GWY_GRAPH(graph), gmodel);

       g_signal_connect_swapped(gmodel, "notify",
                                G_CALLBACK(gwy_graph_refresh), graph);

       g_signal_connect_swapped(gmodel, "layout-updated",
                                G_CALLBACK(gwy_graph_refresh), graph);

      }

    gwy_graph_refresh(graph);

    return GTK_WIDGET(graph);
}



/* XXX If this function needs to be public, then either GwyGraph or
 * GwyGraphModel is broken. XXX */
/**
 * gwy_graph_refresh:
 * @graph: A graph widget.
 *
 * Refresh all the graph widgets according to the model.
 *
 **/
void
gwy_graph_refresh(GwyGraph *graph)
{
    
    GwyGraphModel *model;
    GwyGraphCurveModel *curvemodel;
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gint i, j;

    if (graph->graph_model == NULL)
        return;
    model = GWY_GRAPH_MODEL(graph->graph_model);

    gwy_axis_set_logarithmic(graph->axis_left,   model->y_is_logarithmic);
    gwy_axis_set_logarithmic(graph->axis_right,  model->y_is_logarithmic);
    gwy_axis_set_logarithmic(graph->axis_top,    model->x_is_logarithmic);
    gwy_axis_set_logarithmic(graph->axis_bottom, model->x_is_logarithmic);
    
    gwy_axis_set_unit(graph->axis_top, model->x_unit);
    gwy_axis_set_unit(graph->axis_bottom, model->x_unit);
    gwy_axis_set_unit(graph->axis_left, model->y_unit);
    gwy_axis_set_unit(graph->axis_right, model->y_unit);
    if (model->ncurves > 0) {

        /*refresh axis and reset axis requirements*/
        x_reqmin = y_reqmin = G_MAXDOUBLE;
        x_reqmax = y_reqmax = -G_MAXDOUBLE;
        for (i = 0; i < model->ncurves; i++) {
            curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[i]);
            for (j = 0; j < curvemodel->n; j++) {
                if (x_reqmin > curvemodel->xdata[j])
                    x_reqmin = curvemodel->xdata[j];
                if (y_reqmin > curvemodel->ydata[j])
                    y_reqmin = curvemodel->ydata[j];
                if (x_reqmax < curvemodel->xdata[j])
                    x_reqmax = curvemodel->xdata[j];
                if (y_reqmax < curvemodel->ydata[j])
                    y_reqmax = curvemodel->ydata[j];
            }
        }
        gwy_axis_set_req(graph->axis_top, x_reqmin, x_reqmax);
        gwy_axis_set_req(graph->axis_bottom, x_reqmin, x_reqmax);
        gwy_axis_set_req(graph->axis_left, y_reqmin, y_reqmax);
        gwy_axis_set_req(graph->axis_right, y_reqmin, y_reqmax);

        model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
        model->x_min = gwy_axis_get_minimum(graph->axis_bottom);
        model->y_max = gwy_axis_get_maximum(graph->axis_left);
        model->y_min = gwy_axis_get_minimum(graph->axis_left);
    }


    /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);

}

static void
replot_cb(G_GNUC_UNUSED GObject *gobject,
          G_GNUC_UNUSED GParamSpec *arg1,
          GwyGraph *graph)
{
    if (graph == NULL || graph->graph_model == NULL)
        return;
    gwy_graph_refresh(graph);
}

/**
 * gwy_graph_set_model:
 * @graph: A graph widget.
 * @gmodel: new graph model
 *
 * Changes the graph model. Everything in graph widgets will
 * be reset to the new data (from the model).
 *
 **/
void
gwy_graph_set_model(GwyGraph *graph, GwyGraphModel *gmodel)
{
    graph->graph_model = gmodel;

    g_signal_connect(gmodel, "notify", G_CALLBACK(replot_cb), graph);
    gwy_graph_area_set_model(graph->area, gmodel);
}

static void
rescaled_cb(G_GNUC_UNUSED GtkWidget *widget, GwyGraph *graph)
{
    GArray *array = g_array_new(FALSE, FALSE, sizeof(gdouble));
    GwyGraphModel *model;
    
    if (graph->graph_model == NULL)
        return;
    model = GWY_GRAPH_MODEL(graph->graph_model);
    model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    model->x_min = gwy_axis_get_minimum(graph->axis_bottom);
    model->y_max = gwy_axis_get_maximum(graph->axis_left);
    model->y_min = gwy_axis_get_minimum(graph->axis_left);

    if (graph->grid_type == GWY_GRAPH_GRID_AUTO)
    {
        gwy_axis_set_grid_data(graph->axis_left, array);
        gwy_graph_area_set_x_grid_data(graph->area, array);
        gwy_axis_set_grid_data(graph->axis_bottom, array);
        gwy_graph_area_set_y_grid_data(graph->area, array);
    
        g_array_free(array, TRUE);
    }
     
    gwy_graph_area_refresh(graph->area);
}

/**
 * gwy_graph_get_model:
 * @graph: A graph widget.
 *
 * Returns: GraphModel associated with this graph widget.
 **/
GwyGraphModel *gwy_graph_get_model(GwyGraph *graph)
{
    return  graph->graph_model;
}

/**
 * gwy_graph_get_axis:
 * @graph: A graph widget.
 * @type: Axis orientation
 *
 * Returns: Graph axis of given orientation.
 **/
GwyAxis*
gwy_graph_get_axis(GwyGraph *graph, GtkPositionType type)
{
    switch (type) {
        case GTK_POS_TOP:
        return graph->axis_top;
        break;

        case GTK_POS_BOTTOM:
        return graph->axis_bottom;
        break;

        case GTK_POS_LEFT:
        return graph->axis_left;
        break;

        case GTK_POS_RIGHT:
        return graph->axis_right;
        break;
    }

    g_return_val_if_reached(NULL);
}
/**
 * gwy_graph_set_axis_visible:
 * @graph: A graph widget.
 * @type: Axis orientation
 * @is_visible: set/unset axis visibility within graph widget
 *
 * Returns: Sets the visibility of graph axis of given orientation.
 **/
void
gwy_graph_set_axis_visible(GwyGraph *graph,
                           GtkPositionType type,
                           gboolean is_visible)
{
    switch (type) {
        case GTK_POS_TOP:
        gwy_axis_set_visible(graph->axis_top, is_visible);
        break;

        case GTK_POS_BOTTOM:
        gwy_axis_set_visible(graph->axis_bottom, is_visible);
        break;

        case GTK_POS_LEFT:
        gwy_axis_set_visible(graph->axis_left, is_visible);
        break;

        case GTK_POS_RIGHT:
        gwy_axis_set_visible(graph->axis_right, is_visible);
        break;
    }
}

/**
 * gwy_graph_get_area:
 * @graph: A graph widget.
 *
 * Returns: Graph area.
 **/
GtkWidget*
gwy_graph_get_area(GwyGraph *graph)
{
    return GTK_WIDGET(graph->area);
}



/**
 * gwy_graph_set_status:
 * @graph: A graph widget.
 * @status: new graph model
 *
 * Set status of the graph widget. Status determines the way how the graph
 * reacts on mouse events, basically. This includes point or area selectiuon and zooming.
 *
 **/
void
gwy_graph_set_status(GwyGraph *graph, GwyGraphStatusType status)
{
    graph->area->status = status;
}

/**
 * gwy_graph_get_status:
 * @graph: A graph widget.
 *
 * Get status of the graph widget. Status determines the way how the graph
 * reacts on mouse events, basically. This includes point or area selectiuon and zooming.
 *
 * Returns: graph status
 **/
GwyGraphStatusType
gwy_graph_get_status(GwyGraph *graph)
{
    return graph->area->status;
}

/**
 * gwy_graph_get_selection_number:
 * @graph: A graph widget.
 *
 * Gets number of selections selected by user.
 *
 * Returns: number of selections
 **/
gint
gwy_graph_get_selection_number(GwyGraph *graph)
{
    if (graph->area->status == GWY_GRAPH_STATUS_XSEL)
        return graph->area->areasdata->data_areas->len;
    else if (graph->area->status ==  GWY_GRAPH_STATUS_POINTS)
        return GWY_SELECTION(graph->area->pointsdata)->n;
    else if (graph->area->status ==  GWY_GRAPH_STATUS_XLINES
             || graph->area->status ==  GWY_GRAPH_STATUS_YLINES)
        return graph->area->linesdata->data_lines->len;
    else
        return 0;
}

/**
 * gwy_graph_set_selection_limit:
 * @graph: A graph widget
 * @limit: maximum muber of selections
 *
 * Set maximum number of selections done by mouse
*/
void
gwy_graph_set_selection_limit(GwyGraph *graph, gint limit)
{
    gwy_graph_area_set_selection_limit(graph->area, limit);
}

/**
 * gwy_graph_get_selection_limit:
 * @graph: A graph widget
 *
 * Returns: maximum number of selections done by mouse
*/
gint
gwy_graph_get_selection_limit(GwyGraph *graph)
{
    return gwy_graph_area_get_selection_limit(graph->area);
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
gwy_graph_get_selection(GwyGraph *graph, gdouble *selection)
{
    gint i;
    GwyGraphDataArea *data_area;
    GwyGraphDataPoint *data_point;
    gdouble data_value;

    if (selection == NULL)
        return;

    switch (graph->area->status) {
        case GWY_GRAPH_STATUS_XSEL:
        for (i = 0; i < graph->area->areasdata->data_areas->len; i++) {
            data_area = &g_array_index(graph->area->areasdata->data_areas,
                                       GwyGraphDataArea, i);
            selection[2*i] = data_area->xmin;
            selection[2*i + 1] = data_area->xmax;
        }
        break;

        case GWY_GRAPH_STATUS_YSEL:
        for (i = 0; i < graph->area->areasdata->data_areas->len; i++) {
            data_area = &g_array_index(graph->area->areasdata->data_areas,
                                       GwyGraphDataArea, i);
            selection[2*i] = data_area->ymin;
            selection[2*i + 1] = data_area->ymax;
        }
        break;

        case GWY_GRAPH_STATUS_XLINES:
        case GWY_GRAPH_STATUS_YLINES:
        for (i = 0; i < graph->area->linesdata->data_lines->len; i++) {
            data_value = g_array_index(graph->area->linesdata->data_lines,
                                       gdouble, i);
            selection[i] = data_value;
        }
        break;

        case GWY_GRAPH_STATUS_POINTS:
        gwy_selection_get_data(GWY_SELECTION(graph->area->pointsdata), selection);
        //for (i = 0; i < graph->area->pointsdata->data_points->len; i++) {
        //    data_point = &g_array_index(graph->area->pointsdata->data_points,
        //                                GwyGraphDataPoint, i);
        //    selection[2*i] = data_point->x;
        //    selection[2*i + 1] = data_point->y;
        //}
        break;

        case GWY_GRAPH_STATUS_ZOOM:
        if (graph->area->zoomdata->width > 0) {
            selection[0] = graph->area->zoomdata->xmin;
            selection[1] = graph->area->zoomdata->width;
        }
        else {
            selection[0] = graph->area->zoomdata->xmin
                           + graph->area->zoomdata->width;
            selection[1] = -graph->area->zoomdata->width;
        }

        if (graph->area->zoomdata->height > 0) {
            selection[2] = graph->area->zoomdata->ymin;
            selection[3] = graph->area->zoomdata->height;
        }
        else {
            selection[2] = graph->area->zoomdata->ymin
                           + graph->area->zoomdata->height;
            selection[3] = -graph->area->zoomdata->height;
        }
        break;

        default:
        g_assert_not_reached();
    }
}

/**
 * gwy_graph_clear_selection:
 * @graph: A graph widget.
 *
 * Clear all selections from the graph widget.
 *
 **/
void
gwy_graph_clear_selection(GwyGraph *graph)
{
    gwy_graph_area_clear_selection(graph->area);
}

/**
 * gwy_graph_request_x_range:
 * @graph: A graph widget.
 * @x_min_req: x minimum requisition
 * @x_max_req: x maximum requisition
 *
 * Ask graph for setting the axis and area ranges for requested values.
 * Note that the axis scales to have reasonably aligned ticks, therefore
 * the result does need to match exactly the requsition falues.
 * Use gwy_graph_get_x_range() if you want to know the result.
 **/
void
gwy_graph_request_x_range(GwyGraph *graph,
                          gdouble x_min_req,
                          gdouble x_max_req)
{
    GwyGraphModel *model;

    if (graph->graph_model == NULL)
        return;
    model = GWY_GRAPH_MODEL(graph->graph_model);

    gwy_axis_set_req(graph->axis_top, x_min_req, x_max_req);
    gwy_axis_set_req(graph->axis_bottom, x_min_req, x_max_req);

    model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    model->x_min = gwy_axis_get_minimum(graph->axis_bottom);

    /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
 }

/**
 * gwy_graph_request_y_range:
 * @graph: A graph widget.
 * @y_min_req: y minimum requisition
 * @y_max_req: y maximum requisition
 *
 * Ask graph for setting the axis and area ranges for requested values.
 * Note that the axis scales to have reasonably aligned ticks, therefore
 * the result does need to match exactly the requsition falues.
 * Use gwy_graph_get_y_range() if you want to know the result.
 **/
void
gwy_graph_request_y_range(GwyGraph *graph,
                          gdouble y_min_req,
                          gdouble y_max_req)
{
    GwyGraphModel *model;

    if (graph->graph_model == NULL)
        return;
    model = GWY_GRAPH_MODEL(graph->graph_model);

    gwy_axis_set_req(graph->axis_left, y_min_req, y_max_req);
    gwy_axis_set_req(graph->axis_right, y_min_req, y_max_req);

    model->y_max = gwy_axis_get_maximum(graph->axis_left);
    model->y_min = gwy_axis_get_minimum(graph->axis_left);

     /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
 }

/**
 * gwy_graph_get_x_range:
 * @graph: A graph widget.
 * @x_min: x minimum
 * @x_max: x maximum
 *
 * Get the actual boudaries of graph area and axis in the x direction.
 **/
void
gwy_graph_get_x_range(GwyGraph *graph, gdouble *x_min, gdouble *x_max)
{
    *x_min = gwy_axis_get_minimum(graph->axis_bottom);
    *x_max = gwy_axis_get_maximum(graph->axis_bottom);
}

/**
 * gwy_graph_get_y_range:
 * @graph: A graph widget.
 * @y_min: y minimum
 * @y_max: y maximum
 *
 * Get the actual boudaries of graph area and axis in the y direction.
 **/
void
gwy_graph_get_y_range(GwyGraph *graph, gdouble *y_min, gdouble *y_max)
{
    *y_min = gwy_axis_get_minimum(graph->axis_left);
    *y_max = gwy_axis_get_maximum(graph->axis_left);
}


/**
 * gwy_graph_enable_user_input:
 * @graph: A graph widget.
 * @enable: whether to enable user input
 *
 * Enables/disables all the graph/curve settings dialogs to be invoked by mouse clicks.
 **/
void
gwy_graph_enable_user_input(GwyGraph *graph, gboolean enable)
{
    graph->enable_user_input = enable;
    gwy_graph_area_enable_user_input(graph->area, enable);
    gwy_axis_enable_label_edit(graph->axis_top, enable);
    gwy_axis_enable_label_edit(graph->axis_bottom, enable);
    gwy_axis_enable_label_edit(graph->axis_left, enable);
    gwy_axis_enable_label_edit(graph->axis_right, enable);
}

void
gwy_graph_signal_selected(GwyGraph *graph)
{
    g_signal_emit(G_OBJECT(graph), gwygraph_signals[SELECTED_SIGNAL], 0);
}

static void
gwy_graph_signal_mouse_moved(GwyGraph *graph)
{
    g_signal_emit(G_OBJECT(graph), gwygraph_signals[MOUSE_MOVED_SIGNAL], 0);
}

static void
gwy_graph_signal_zoomed(GwyGraph *graph)
{
    g_signal_emit(G_OBJECT(graph), gwygraph_signals[ZOOMED_SIGNAL], 0);
}

/**
 * gwy_graph_get_cursor:
 * @graph: A graph widget.
 * @x_cursor: x position of cursor
 * @y_cursor: y position of cursor
 *
 * Get the mouse pointer position within the graph area. Values are
 * in physical units corresponding to the graph axes.
 **/
void
gwy_graph_get_cursor(GwyGraph *graph, gdouble *x_cursor, gdouble *y_cursor)
{
    gwy_graph_area_get_cursor(graph->area, x_cursor, y_cursor);
}

/**
 * gwy_graph_zoom_in:
 * @graph: A graph widget.
 *
 * Switch to zoom status. Graph will expect zoom selection
 * and will zoom afterwards automatically.
 **/
void
gwy_graph_zoom_in(GwyGraph *graph)
{
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_ZOOM);
}

/**
 * gwy_graph_zoom_out:
 * @graph: A graph widget.
 *
 * Zoom out to see all the data points.
 **/
void
gwy_graph_zoom_out(GwyGraph *graph)
{
    gwy_graph_refresh(graph);
}

static void
zoomed_cb(GwyGraph *graph)
{
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gdouble selection[4];

    if (graph->area->status != GWY_GRAPH_STATUS_ZOOM)
        return;
    gwy_graph_get_selection(graph, selection);

    x_reqmin = selection[0];
    x_reqmax = selection[0] + selection[1];
    y_reqmin = selection[2];
    y_reqmax = selection[2] + selection[3];

    gwy_axis_set_req(graph->axis_top, x_reqmin, x_reqmax);
    gwy_axis_set_req(graph->axis_bottom, x_reqmin, x_reqmax);
    gwy_axis_set_req(graph->axis_left, y_reqmin, y_reqmax);
    gwy_axis_set_req(graph->axis_right, y_reqmin, y_reqmax);

    graph->graph_model->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    graph->graph_model->x_min = gwy_axis_get_minimum(graph->axis_bottom);
    graph->graph_model->y_max = gwy_axis_get_maximum(graph->axis_left);
    graph->graph_model->y_min = gwy_axis_get_minimum(graph->axis_left);

    /*refresh widgets*/
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);
    gwy_graph_area_refresh(graph->area);
    gwy_graph_signal_zoomed(graph);
}

/* XXX What the f**k is this? XXX
 *
 * Why it does exactly the thing GString attempts to avoid?  Shouldn't it do:
 * g_string_assign(graph->graph_model->some_label, gwy_axis_set_label(axis));
 * Except that GwyGraphModel should have a *method* for setting axis labels. */
static void
label_updated_cb(GwyAxis *axis, GwyGraph *graph)
{
    switch (axis->orientation)
    {
        case GTK_POS_TOP:
        if (graph->graph_model->top_label)
            g_string_free(graph->graph_model->top_label, TRUE);
        graph->graph_model->top_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;

        case GTK_POS_BOTTOM:
        if (graph->graph_model->bottom_label)
            g_string_free(graph->graph_model->bottom_label, TRUE);
        graph->graph_model->bottom_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;

        case GTK_POS_LEFT:
        if (graph->graph_model->left_label)
            g_string_free(graph->graph_model->left_label, TRUE);
        graph->graph_model->left_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;

        case GTK_POS_RIGHT:
        if (graph->graph_model->right_label)
            g_string_free(graph->graph_model->right_label, TRUE);
        graph->graph_model->right_label
            = g_string_new((gwy_axis_get_label(axis))->str);
        break;
    }
}

void       
gwy_graph_set_grid_type(GwyGraph *graph, GwyGraphGridType grid_type)
{
    graph->grid_type = grid_type;
    gwy_graph_refresh(graph);
}

GwyGraphGridType 
gwy_graph_get_grid_type(GwyGraph *graph)
{
    return graph->grid_type;
}

void 
gwy_graph_set_x_grid_data(GwyGraph *graph, GArray *grid_data)
{
    gwy_graph_area_set_x_grid_data(graph->area, grid_data);
}

void 
gwy_graph_set_y_grid_data(GwyGraph *graph, GArray *grid_data)
{
    gwy_graph_area_set_y_grid_data(graph->area, grid_data);
}

const GArray* 
gwy_graph_get_x_grid_data(GwyGraph *graph)
{
    return gwy_graph_get_x_grid_data(graph->area);
}

const GArray* 
gwy_graph_get_y_grid_data(GwyGraph *graph)
{
    return gwy_graph_get_y_grid_data(graph->area);
}




/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
