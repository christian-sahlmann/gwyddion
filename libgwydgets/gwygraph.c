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
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <stdio.h>

#include <libgwyddion/gwymacros.h>
#include "gwygraph.h"

#define GWY_GRAPH_TYPE_NAME "GwyGraph"


static void     gwy_graph_class_init           (GwyGraphClass *klass);
static void     gwy_graph_init                 (GwyGraph *graph);
static void     gwy_graph_size_request         (GtkWidget *widget,
                                                GtkRequisition *requisition);
static void     gwy_graph_size_allocate        (GtkWidget *widget,
                                                GtkAllocation *allocation);

static void gwy_graph_make_curve_data(GwyGraph *graph, GwyGraphAreaCurve *curve, gdouble *xvals, gdouble *yvals, gint n);
static void     gwy_graph_synchronize          (GwyGraph *graph);

static void     zoomed_cb                      (GtkWidget *widget);

static GtkWidgetClass *parent_class = NULL;


GType
gwy_graph_get_type(void)
{
    static GType gwy_graph_type = 0;
    if (!gwy_graph_type) {
        static const GTypeInfo gwy_graph_info = {
         sizeof(GwyGraphClass),
         NULL,
         NULL,
         (GClassInitFunc)gwy_graph_class_init,
         NULL,
         NULL,
         sizeof(GwyGraph),
         0,
         (GInstanceInitFunc)gwy_graph_init,
         NULL,
         };
        gwy_debug("");
        gwy_graph_type = g_type_register_static (GTK_TYPE_TABLE,
                                                 GWY_GRAPH_TYPE_NAME,
                                                 &gwy_graph_info,
                                                 0);

    }

    return gwy_graph_type;
}

static void
gwy_graph_class_init(GwyGraphClass *klass)
{
    GtkWidgetClass *widget_class;

    gwy_debug("");

    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    widget_class->size_request = gwy_graph_size_request;
    widget_class->size_allocate = gwy_graph_size_allocate;

}


static void
gwy_graph_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition);
    requisition->width = 500;
    requisition->height = 400;

    gwy_graph_synchronize(GWY_GRAPH(widget));
}

static void
gwy_graph_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraph *graph;
    gwy_debug("");

    graph = GWY_GRAPH(widget);
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);

    /*synchronize axis and area (axis range can change)*/
    gwy_graph_synchronize(graph);
}

static void
gwy_graph_synchronize(GwyGraph *graph)
{
    graph->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    graph->x_min = gwy_axis_get_minimum(graph->axis_bottom);
    graph->y_max = gwy_axis_get_maximum(graph->axis_left);
    graph->y_min = gwy_axis_get_minimum(graph->axis_left);
    gwy_graph_area_set_boundaries(graph->area, graph->x_min,
                                  graph->x_max, graph->y_min, graph->y_max);
}

static void
gwy_graph_init(GwyGraph *graph)
{
    gwy_debug("");

    graph->n_of_curves = 0;
    graph->n_of_autocurves = 0;

    graph->autoproperties.is_line = 1;
    graph->autoproperties.is_point = 1;
    graph->autoproperties.point_size = 8;
    graph->autoproperties.line_size = 1;

    gtk_table_resize (GTK_TABLE (graph), 3, 3);
    gtk_table_set_homogeneous (GTK_TABLE (graph), FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (graph), 0);
    gtk_table_set_col_spacings (GTK_TABLE (graph), 0);

    graph->axis_top = GWY_AXIS (gwy_axis_new(GWY_AXIS_SOUTH, 2.24, 5.21, "x"));
    graph->axis_bottom =  GWY_AXIS (gwy_axis_new(GWY_AXIS_NORTH, 2.24, 5.21, "x"));
    graph->axis_left =  GWY_AXIS (gwy_axis_new(GWY_AXIS_EAST, 100, 500, "y"));
    graph->axis_right =  GWY_AXIS (gwy_axis_new(GWY_AXIS_WEST, 100, 500, "y"));

    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->axis_top), 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->axis_bottom), 1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->axis_left), 2, 3, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->axis_right), 0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show(GTK_WIDGET(graph->axis_top));
    gtk_widget_show(GTK_WIDGET(graph->axis_bottom));
    gtk_widget_show(GTK_WIDGET(graph->axis_left));
    gtk_widget_show(GTK_WIDGET(graph->axis_right));

    graph->corner_tl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_bl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_tr = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    graph->corner_br = GWY_GRAPH_CORNER(gwy_graph_corner_new());


    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->corner_tl), 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->corner_bl), 2, 3, 0, 1,
                     GTK_FILL, GTK_FILL , 0, 0);
    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->corner_tr), 0, 1, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->corner_br), 2, 3, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);

    gtk_widget_show(GTK_WIDGET(graph->corner_tl));
    gtk_widget_show(GTK_WIDGET(graph->corner_bl));
    gtk_widget_show(GTK_WIDGET(graph->corner_tr));
    gtk_widget_show(GTK_WIDGET(graph->corner_br));

    graph->area = GWY_GRAPH_AREA(gwy_graph_area_new(NULL,NULL));
    graph->x_max = 0;
    graph->y_max = 0;
    graph->x_min = 0;
    graph->x_min = 0;
    graph->x_reqmax = -G_MAXDOUBLE;
    graph->y_reqmax = -G_MAXDOUBLE;
    graph->x_reqmin = G_MAXDOUBLE;
    graph->y_reqmin = G_MAXDOUBLE;
    graph->has_x_unit = 0;
    graph->has_y_unit = 0;
    graph->x_unit = NULL;
    graph->y_unit = NULL;

    graph->area->status = GWY_GRAPH_STATUS_PLAIN;

    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    g_signal_connect(graph->area, "zoomed", G_CALLBACK(zoomed_cb), NULL);
    
    gtk_widget_show_all(GTK_WIDGET(graph->area));

}

GtkWidget*
gwy_graph_new()
{
    gwy_debug("");
    return GTK_WIDGET(g_object_new(gwy_graph_get_type(), NULL));
}


/**
 * gwy_graph_add_dataline:
 * @graph: graph widget 
 * @dataline: dataline to be added
 * @shift: x shift (dataline starts allways at zero)
 * @label: curve label
 * @params: parameters of curve (lines/points etc.)
 *
 * Adds a dataline into graph.
 **/
void
gwy_graph_add_dataline(GwyGraph *graph, GwyDataLine *dataline,
                       G_GNUC_UNUSED gdouble shift, GString *label,
                       G_GNUC_UNUSED GwyGraphAreaCurveParams *params)
{


    gdouble *xdata;
    gint n, i;

    gwy_debug("");
    n = gwy_data_line_get_res(dataline);

    xdata = (gdouble *) g_malloc(n*sizeof(gdouble));
    for (i = 0; i < n; i++)
    {
        xdata[i] = i*gwy_data_line_get_real(dataline)/(gdouble)n;
    }


    gwy_graph_add_datavalues(graph, xdata, dataline->data,
                             n, label, NULL);

    g_free(xdata);
}

/**
 * gwy_graph_add_dataline_with_units:
 * @graph: graph widget 
 * @dataline: dataline to be added
 * @shift: x shift (dataline starts allways at zero)
 * @label: curve label
 * @params: parameters of curve (lines/points etc.)
 * @x_order: division factor to obtain values corresponding to units
 * @y_order: division factor to obtain values corresponding to units
 * @x_unit: unit at x axis
 * @y_unit: unit at y axis
 *
 * Adds a datalien into graph, setting units. Original dataline data
 * will be divided by @x_order and @y_order factors and axis labels
 * will have requested units.
 **/
void
gwy_graph_add_dataline_with_units(GwyGraph *graph, GwyDataLine *dataline,
                                  G_GNUC_UNUSED gdouble shift, GString *label,
                                  G_GNUC_UNUSED GwyGraphAreaCurveParams *params,
                                  gdouble x_order, gdouble y_order,
                                  char *x_unit, char *y_unit)
{

    gdouble *xdata, *ydata;
    gint n, i;

    gwy_debug("");
    n = gwy_data_line_get_res(dataline);

    /*prepare values (divide by orders)*/
    xdata = (gdouble *) g_malloc(n*sizeof(gdouble));
    ydata = (gdouble *) g_malloc(n*sizeof(gdouble));
    for (i = 0; i < n; i++) {
        xdata[i] = i*gwy_data_line_get_real(dataline)/((gdouble)n)/x_order;
        ydata[i] = gwy_data_line_get_val(dataline, i)/y_order;
    }

    /*add values*/
    gwy_graph_add_datavalues(graph, xdata, ydata,
                             n, label, NULL);

    /*add unit to graph axis*/
    if (x_unit != NULL) {
        graph->x_unit = g_strdup(x_unit);
        graph->has_x_unit = 1;

        gwy_axis_set_unit(graph->axis_top, graph->x_unit);
        gwy_axis_set_unit(graph->axis_bottom, graph->x_unit);
    }
    if (y_unit != NULL) {
        graph->y_unit = g_strdup(y_unit);
        graph->has_y_unit = 1;
        gwy_axis_set_unit(graph->axis_left, graph->y_unit);
        gwy_axis_set_unit(graph->axis_right, graph->y_unit);
    }

    g_free(xdata);
    g_free(ydata);
}

/**
 * gwy_graph_add_datavalues:
 * @graph: graph widget 
 * @xvals: x values
 * @yvals: y values
 * @n: number of values
 * @label: curve label
 * @params: arameters of curve (lines/points etc.)
 *
 * Adds raw data to the graph. Data are represented by two arrays
 * of same size.
 **/
void
gwy_graph_add_datavalues(GwyGraph *graph, gdouble *xvals, gdouble *yvals,
                         gint n, GString *label,
                         GwyGraphAreaCurveParams *params)
{
    gint i;
    gboolean isdiff;
    GwyGraphAreaCurve curve;

    gwy_debug("");

    /*look whether label maximum or minium will be changed*/
    isdiff = FALSE;
    for (i = 0; i < n; i++)
    {
       if (xvals[i] > graph->x_reqmax) {
          graph->x_reqmax = xvals[i];
          isdiff = TRUE;
       }
       if (xvals[i] < graph->x_reqmin) {
          graph->x_reqmin = xvals[i];
          /*printf("New x minimum at %f (index %d)\n", xvals[i], i);*/
          isdiff = TRUE;
       }
       if (yvals[i] > graph->y_reqmax) {
          graph->y_reqmax = yvals[i];
          isdiff = TRUE;
       }
       if (yvals[i] < graph->y_reqmin) {
          graph->y_reqmin = yvals[i];
          isdiff = TRUE;
       }
    }

    if (graph->y_reqmax > 1e20 || graph->y_reqmax < -1e20 
       || graph->y_reqmin > 1e20 || graph->y_reqmin < -1e20)
    {
        g_warning("Data values are corrupted. Curve not added.");
        return;
    }
    
    if (isdiff) {
      /*  printf("x requirement changed: %f, %f\n", graph->x_reqmin, graph->x_reqmax);*/
       gwy_axis_set_req(graph->axis_top, graph->x_reqmin, graph->x_reqmax);
       gwy_axis_set_req(graph->axis_bottom, graph->x_reqmin, graph->x_reqmax);
       gwy_axis_set_req(graph->axis_left, graph->y_reqmin, graph->y_reqmax);
       gwy_axis_set_req(graph->axis_right, graph->y_reqmin, graph->y_reqmax);

       graph->x_max = gwy_axis_get_maximum(graph->axis_bottom);
       graph->x_min = gwy_axis_get_minimum(graph->axis_bottom);
       graph->y_max = gwy_axis_get_maximum(graph->axis_left);
       graph->y_min = gwy_axis_get_minimum(graph->axis_left);
       graph->x_reqmax = gwy_axis_get_reqmaximum(graph->axis_bottom);
       graph->x_reqmin = gwy_axis_get_reqminimum(graph->axis_bottom);
       graph->y_reqmax = gwy_axis_get_reqmaximum(graph->axis_left);
       graph->y_reqmin = gwy_axis_get_reqminimum(graph->axis_left);
     }

    /*make curve (precompute screeni coordinates of points)*/
    gwy_graph_make_curve_data(graph, &curve, xvals, yvals, n);

    /*configure curve plot properties*/
    if (params == NULL) {
      curve.params.is_line = graph->autoproperties.is_line;
      curve.params.is_point = graph->autoproperties.is_point;
      curve.params.point_size = graph->autoproperties.point_size;
      curve.params.line_size = graph->autoproperties.line_size;
      curve.params.line_style = GDK_LINE_SOLID;
      curve.params.description = g_string_new(label->str);
      /***** PROVISORY ***************/
      if (graph->n_of_autocurves == 0) {curve.params.color.pixel = 0x00000000;
       curve.params.point_type = GWY_GRAPH_POINT_TRIANGLE_UP;}
      if (graph->n_of_autocurves == 1) {curve.params.color.pixel = 0x00990099;
        curve.params.point_type = GWY_GRAPH_POINT_TRIANGLE_DOWN;}
      if (graph->n_of_autocurves == 2) {curve.params.color.pixel = 0x09909900;
      curve.params.point_type = GWY_GRAPH_POINT_CIRCLE;}
      if (graph->n_of_autocurves == 3) {curve.params.color.pixel = 0x000ddd00;
      curve.params.point_type = GWY_GRAPH_POINT_DIAMOND;}
      if (graph->n_of_autocurves == 4) {curve.params.color.pixel = 0x00ff0055;
        curve.params.point_type = GWY_GRAPH_POINT_TIMES;}
      /**** END OF PROVISORY ******/
    }
    else {
      curve.params.is_line = params->is_line;
      curve.params.is_point = params->is_point;
      curve.params.point_size = params->point_size;
      curve.params.line_size = params->line_size;
      curve.params.line_style = params->line_style;
      curve.params.description = g_string_new(label->str);
      curve.params.point_type = params->point_type;
      curve.params.color = params->color;
    }

    /*put curve and (new) boundaries into the plotter*/
    gwy_graph_area_add_curve(graph->area, &curve);
    gwy_graph_area_set_boundaries(graph->area, graph->x_min,
                                  graph->x_max, graph->y_min, graph->y_max);

    g_free(curve.data.xvals);
    g_free(curve.data.yvals);

    graph->n_of_curves++;
    if (params == NULL)
        graph->n_of_autocurves++;

    /* FIXME: why here? why not... */
    gtk_widget_queue_draw(GTK_WIDGET(graph));
}

static void
gwy_graph_make_curve_data(G_GNUC_UNUSED GwyGraph *graph,
                          GwyGraphAreaCurve *curve,
                          gdouble *xvals, gdouble *yvals, gint n)
{
    gint i;

    curve->data.xvals = (gdouble *) g_try_malloc(n*sizeof(gdouble));
    curve->data.yvals = (gdouble *) g_try_malloc(n*sizeof(gdouble));
    curve->data.N = n;
    for (i = 0; i < n; i++) {
        curve->data.xvals[i] = xvals[i];
        curve->data.yvals[i] = yvals[i];
    }
}

/**
 * gwy_graph_clear:
 * @graph: graph widget 
 *
 * Remove all curves.
 **/
void
gwy_graph_clear(GwyGraph *graph)
{
    gwy_graph_area_clear(graph->area);
    graph->n_of_autocurves = 0;
    graph->n_of_curves = 0;
    graph->x_max = 0;
    graph->y_max = 0;
    graph->x_min = 0;
    graph->x_min = 0;
    graph->x_reqmax = -G_MAXDOUBLE;
    graph->y_reqmax = -G_MAXDOUBLE;
    graph->x_reqmin = G_MAXDOUBLE;
    graph->y_reqmin = G_MAXDOUBLE;

    /* FIXME: why here? why not... */
    gtk_widget_queue_draw(GTK_WIDGET(graph));
}

/**
 * gwy_graph_set_autoproperties:
 * @graph: graph widget 
 * @autoproperties: autoproperties of graph
 *
 * Sets the autoproperties - properties of curves
 * added with no specification
 * (color, point/line draw, etc.).
 **/
void
gwy_graph_set_autoproperties(GwyGraph *graph,
                             GwyGraphAutoProperties *autoproperties)
{
    graph->autoproperties = *autoproperties;
}

/**
 * gwy_graph_get_autoproperties:
 * @graph: graph widget 
 * @autoproperties: autoproperties of graph
 *
 * Gets the autoproperties - properties of curves
 * added with no specification (color, point/line draw, etc.).
 **/
void
gwy_graph_get_autoproperties(GwyGraph *graph,
                             GwyGraphAutoProperties *autoproperties)
{
    *autoproperties = graph->autoproperties;
}

/**
 * gwy_graph_set_status:
 * @graph: graph widget 
 * @status: graph status to be set
 *
 * sets the graph status. The status is related with ability
 * to do different mouse selections.
 **/
void
gwy_graph_set_status(GwyGraph *graph,
                     GwyGraphStatusType status)
{

    /*reset points if status changing*/
    if (graph->area->status == GWY_GRAPH_STATUS_POINTS)
    {
        g_array_free(graph->area->pointsdata->scr_points, 1);
        g_array_free(graph->area->pointsdata->data_points, 1);

        graph->area->pointsdata->scr_points = g_array_new(0, 1, sizeof(GwyGraphScrPoint));
        graph->area->pointsdata->data_points = g_array_new(0, 1, sizeof(GwyGraphDataPoint));
        graph->area->pointsdata->n = 0;
    }                                                          

    graph->area->status = status;
}

/**
 * gwy_graph_get_status:
 * @graph: graph widget 
 *
 * gets the graph status. The status is related with ability
 * to do different mouse selections.
 * Returns: current graph status
 **/
GwyGraphStatusType
gwy_graph_get_status(GwyGraph *graph)
{
    return graph->area->status;
}

/**
 * gwy_graph_get_status_data:
 * @graph: graph widget 
 *
 * gets the graph status data - data corresponding
 * to mouse selections done by user. Actual contain
 * cooresponds on status type.
 *
 * Returns: pointer to status data.
 **/
gpointer
gwy_graph_get_status_data(GwyGraph *graph)
{
    switch (graph->area->status) {
        case GWY_GRAPH_STATUS_PLAIN:
        return NULL;
        break;

        case GWY_GRAPH_STATUS_CURSOR:
        graph->area->cursordata->data_point.x_unit
            = graph->has_x_unit ? graph->x_unit : NULL;
        graph->area->cursordata->data_point.y_unit
            = graph->has_y_unit ? graph->y_unit : NULL;
        return graph->area->cursordata;
        break;

        case GWY_GRAPH_STATUS_XSEL:
        case GWY_GRAPH_STATUS_YSEL:
        return graph->area->seldata;
        break;

        case GWY_GRAPH_STATUS_POINTS:
        return graph->area->pointsdata;
        break;

        default:
        g_assert_not_reached();
        break;
    }
    return NULL;
}

/**
 * gwy_graph_get_boundaries:
 * @graph: graph widget 
 * @x_min: x axis minimum value
 * @x_max: x axis maximum value
 * @y_min: y axis minimum value
 * @y_max: y axis maximum value
 *
 *
 **/
void 
gwy_graph_get_boundaries(GwyGraph *graph, gdouble *x_min, gdouble *x_max, gdouble *y_min, gdouble *y_max)
{
    *x_min = graph->x_min;
    *x_max = graph->x_max;
    *y_min = graph->y_min;
    *y_max = graph->y_max;
}

/**
 * gwy_graph_set_boundaries:
 * @graph: graph widget 
 * @x_min: x axis minimum value
 * @x_max: x axis maximum value
 * @y_min: y axis minimum value
 * @y_max: y axis maximum value
 *
 * Sets actual axis boundaries of graph. Recomputes and redisplays all
 * necessary things.
 **/
void 
gwy_graph_set_boundaries(GwyGraph *graph, gdouble x_min, gdouble x_max, gdouble y_min, gdouble y_max)
{
    /*set the graph requisition*/
    graph->x_reqmin = x_min;
    graph->x_reqmax = x_max;
    graph->y_reqmin = y_min;
    graph->y_reqmax = y_max;
  
    /*ask axis, what does she thinks about the requisitions*/
    gwy_axis_set_req(graph->axis_top, graph->x_reqmin, graph->x_reqmax);
    gwy_axis_set_req(graph->axis_bottom, graph->x_reqmin, graph->x_reqmax);
    gwy_axis_set_req(graph->axis_left, graph->y_reqmin, graph->y_reqmax);
    gwy_axis_set_req(graph->axis_right, graph->y_reqmin, graph->y_reqmax);

    /*of course, axis is never satisfied..*/
    graph->x_max = gwy_axis_get_maximum(graph->axis_bottom);
    graph->x_min = gwy_axis_get_minimum(graph->axis_bottom);
    graph->y_max = gwy_axis_get_maximum(graph->axis_left);
    graph->y_min = gwy_axis_get_minimum(graph->axis_left);
    graph->x_reqmax = gwy_axis_get_reqmaximum(graph->axis_bottom);
    graph->x_reqmin = gwy_axis_get_reqminimum(graph->axis_bottom);
    graph->y_reqmax = gwy_axis_get_reqmaximum(graph->axis_left);
    graph->y_reqmin = gwy_axis_get_reqminimum(graph->axis_left);
    
    /*refresh graph*/
    gwy_graph_area_set_boundaries(graph->area, graph->x_min,
                     graph->x_max, graph->y_min, graph->y_max);

  
}

/**
 * gwy_graph_unzoom:
 * @graph: graph widget 
 *
 * resets zoom. Fits all curves into graph.
 **/
void
gwy_graph_unzoom(GwyGraph *graph)
{
    GwyGraphAreaCurve *pcurve;
    gdouble xmax, xmin, ymax, ymin;
    gint i, j;

    xmin = G_MAXDOUBLE;
    ymin = G_MAXDOUBLE;
    xmax = -G_MAXDOUBLE;
    ymax = -G_MAXDOUBLE;

    gwy_debug("");

    /*find extrema*/
    for (i=0; i<graph->area->curves->len; i++)
    {
        pcurve = g_ptr_array_index (graph->area->curves, i);
        for (j=0; j<pcurve->data.N; j++)
        {        
            if (pcurve->data.xvals[j] > xmax) xmax = pcurve->data.xvals[j];
            if (pcurve->data.yvals[j] > ymax) ymax = pcurve->data.yvals[j];
            if (pcurve->data.xvals[j] < xmin) xmin = pcurve->data.xvals[j];
            if (pcurve->data.yvals[j] < ymin) ymin = pcurve->data.yvals[j];
        }
    }
    gwy_graph_set_boundaries(graph, xmin, xmax, ymin, ymax);                                      
    gtk_widget_queue_draw(GTK_WIDGET(graph));
}

static void 
zoomed_cb(GtkWidget *widget)
{
    GwyGraph *graph;
    gwy_debug("");

    graph = GWY_GRAPH(gtk_widget_get_parent(widget));
        
    gwy_graph_set_boundaries(graph, 
                             graph->area->zoomdata->xmin, 
                             graph->area->zoomdata->xmax, 
                             graph->area->zoomdata->ymin, 
                             graph->area->zoomdata->ymax);
    gtk_widget_queue_draw(GTK_WIDGET(graph));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
