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
#include <stdio.h>
#include <glib.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include "gwygraph.h"

#define GWY_GRAPH_TYPE_NAME "GwyGraph"

static void     gwy_graph_class_init           (GwyGraphClass *klass);
static void     gwy_graph_init                 (GwyGraph *graph);
static void     gwy_graph_finalize             (GObject *object);
static void     gwy_graph_size_request         (GtkWidget *widget,
                                                GtkRequisition *requisition);
static void     gwy_graph_size_allocate        (GtkWidget *widget,
                                                GtkAllocation *allocation);

static void gwy_graph_make_curve_data(GwyGraph *graph, GwyGraphAreaCurve *curve, gdouble *xvals, gdouble *yvals, gint n);
static void     gwy_graph_synchronize          (GwyGraph *graph);
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
        gwy_debug("%s", __FUNCTION__);
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

    gwy_debug("%s", __FUNCTION__);

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
    gwy_debug("%s", __FUNCTION__);

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
    gwy_debug("%s", __FUNCTION__);

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
    graph->x_reqmax = G_MINDOUBLE;
    graph->y_reqmax = G_MINDOUBLE;
    graph->x_reqmin = G_MAXDOUBLE;
    graph->x_reqmin = G_MAXDOUBLE;
    graph->has_x_unit = 0;
    graph->has_y_unit = 0;
    graph->x_unit = NULL;
    graph->y_unit = NULL;

    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show_all(GTK_WIDGET(graph->area));

}

GtkWidget *
gwy_graph_new()
{
    gwy_debug("%s", __FUNCTION__);
     return GTK_WIDGET (g_object_new (gwy_graph_get_type (), NULL));
}


void
gwy_graph_add_dataline(GwyGraph *graph, GwyDataLine *dataline,
                       gdouble shift, GString *label, GwyGraphAreaCurveParams *params)
{
    gwy_debug("%s", __FUNCTION__);

    gdouble *xdata, *ydata;
    gint n, i;
    
    n = gwy_data_line_get_res(dataline);
    
    xdata = (gdouble *) g_malloc(n*sizeof(gdouble));
    for (i=0; i<n; i++) xdata[i] = i*gwy_data_line_get_real(dataline)/(gdouble)n;

    gwy_graph_add_datavalues(graph, xdata, dataline->data,
                             n, label, NULL);

    g_free(xdata);
}

void
gwy_graph_add_dataline_with_units(GwyGraph *graph, GwyDataLine *dataline,
                       gdouble shift, GString *label, GwyGraphAreaCurveParams *params,
                       gdouble x_order, gdouble y_order, char *x_unit, char *y_unit)
{
    gwy_debug("%s", __FUNCTION__);

    gdouble *xdata, *ydata;
    gint n, i;
    
    n = gwy_data_line_get_res(dataline);
    
    /*prepare values (divide by orders)*/
    xdata = (gdouble *) g_malloc(n*sizeof(gdouble));
    ydata = (gdouble *) g_malloc(n*sizeof(gdouble));
    for (i=0; i<n; i++) 
    {
        xdata[i] = (gdouble)i*gwy_data_line_get_real(dataline)/((gdouble)n)/x_order;
        ydata[i] = gwy_data_line_get_val(dataline, i)/y_order;
    }
    
    /*add values*/
    gwy_graph_add_datavalues(graph, xdata, ydata,
                             n, label, NULL);

    /*add unit to graph axis*/
    if (x_unit != NULL)
    {
        graph->x_unit = g_strdup(x_unit);
        graph->has_x_unit = 1;

        gwy_axis_set_unit(graph->axis_top, graph->x_unit);
        gwy_axis_set_unit(graph->axis_bottom, graph->x_unit);
    }
    if (y_unit != NULL)
    {
        graph->y_unit = g_strdup(y_unit);
        graph->has_y_unit = 1;
        gwy_axis_set_unit(graph->axis_left, graph->y_unit);
        gwy_axis_set_unit(graph->axis_right, graph->y_unit);
    }
    
    g_free(xdata);
    g_free(ydata);
}

void
gwy_graph_add_datavalues(GwyGraph *graph, gdouble *xvals, gdouble *yvals,
                         gint n, GString *label, GwyGraphAreaCurveParams *params)
{
    gint i, isdiff;
    GwyGraphAreaCurve curve;

    gwy_debug("%s", __FUNCTION__);

    /*look whether label maximum or minium will be changed*/
    isdiff=0;
    for (i=0; i<n; i++)
    {
       if (xvals[i] > graph->x_reqmax)
       {
          graph->x_reqmax = xvals[i];
          isdiff=1;
       }
       if (xvals[i] < graph->x_reqmin)
       {
          graph->x_reqmin = xvals[i]; /*printf("New x minimum at %f (index %d)\n", xvals[i], i);*/
          isdiff=1;
       }
       if (yvals[i] > graph->y_reqmax)
       {
          graph->y_reqmax = yvals[i];
          isdiff=1;
       }
       if (yvals[i] < graph->y_reqmin)
       {
          graph->y_reqmin = yvals[i];
          isdiff=1;
       }
    }
    if (isdiff == 1)
    {
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
    if (params == NULL)
    {
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
    else
    {
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
    if (params == NULL) graph->n_of_autocurves++;
}

static void
gwy_graph_make_curve_data(GwyGraph *graph, GwyGraphAreaCurve *curve, gdouble *xvals, gdouble *yvals, gint n)
{
    gint i;

    curve->data.xvals = (gdouble *) g_try_malloc(n*sizeof(gdouble));
    curve->data.yvals = (gdouble *) g_try_malloc(n*sizeof(gdouble));
    curve->data.N = n;
    for (i=0; i<n; i++)
    {
        curve->data.xvals[i] = xvals[i];
        curve->data.yvals[i] = yvals[i];
    }
}

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
  graph->x_reqmax = G_MINDOUBLE;
  graph->y_reqmax = G_MINDOUBLE;
  graph->x_reqmin = G_MAXDOUBLE;
  graph->x_reqmin = G_MAXDOUBLE;
                                  
}

void
gwy_graph_set_autoproperties(GwyGraph *graph, GwyGraphAutoProperties *autoproperties)
{
  graph->autoproperties = *autoproperties;
}

void
gwy_graph_get_autoproperties(GwyGraph *graph, GwyGraphAutoProperties *autoproperties)
{
  *autoproperties = graph->autoproperties;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
