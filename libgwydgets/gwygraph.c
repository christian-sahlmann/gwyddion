/* @(#) $Id$ */

#include <math.h>
#include <stdio.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "gwygraph.h"

#define GWY_GRAPH_TYPE_NAME "GwyGraph"

static void     gwy_graph_class_init           (GwyGraphClass *klass);
static void     gwy_graph_init                 (GwyGraph *graph);
static void     gwy_graph_finalize             (GObject *object);
static void     gwy_graph_size_request         (GtkWidget *widget,
						GtkRequisition *requisition);

static void gwy_graph_make_curve_data(GwyGraph *graph, GwyGraphAreaCurve *curve, gdouble *xvals, gdouble *yvals, gint n);

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
        #ifdef DEBUG
        g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
        #endif
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
    
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif

    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);
    
    widget_class->size_request = gwy_graph_size_request;
}


static void
gwy_graph_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition); 
    requisition->width = 500;
    requisition->height = 400;
}

static void
gwy_graph_init(GwyGraph *graph)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    
    graph->n_of_curves = 0;
    graph->n_of_autocurves = 0;

    graph->autoproperties.is_line = 1;
    graph->autoproperties.is_point = 1;
    graph->autoproperties.point_size = 5;
    graph->autoproperties.line_size = 1;
    
    gtk_table_resize (GTK_TABLE (graph), 3, 3);
    gtk_table_set_homogeneous (GTK_TABLE (graph), FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (graph), 0);
    gtk_table_set_col_spacings (GTK_TABLE (graph), 0);

    graph->axis_top = GWY_AXIS (gwy_axis_new(GWY_AXIS_SOUTH, 2.24, 5.21, "blo "));
    graph->axis_bottom =  GWY_AXIS (gwy_axis_new(GWY_AXIS_NORTH, 2.24, 5.21, "ble"));
    graph->axis_left =  GWY_AXIS (gwy_axis_new(GWY_AXIS_EAST, 100, 500, "bla"));
    graph->axis_right =  GWY_AXIS (gwy_axis_new(GWY_AXIS_WEST, 100, 500, "blu "));

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
   
    gtk_table_attach(GTK_TABLE (graph), GTK_WIDGET(graph->area), 1, 2, 1, 2, 
		     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show_all(GTK_WIDGET(graph->area));
    
}

GtkWidget *
gwy_graph_new()
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
     return GTK_WIDGET (g_object_new (gwy_graph_get_type (), NULL));
}


void 
gwy_graph_add_dataline(GwyGraph *graph, GwyDataLine *dataline, gdouble shift)
{
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
    

}

void
gwy_graph_add_datavalues(GwyGraph *graph, gdouble *xvals, gdouble *yvals, gint n)
{
    gint i, isdiff;
    gdouble x_new_max, x_new_min, y_new_max, y_new_min;
    GwyGraphAreaCurve curve;
    
    #ifdef DEBUG
    g_log(GWY_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", __FUNCTION__);
    #endif
     
    /*look whether label maximum or minium will be changed*/
    isdiff=0;
    for (i=0; i<n; i++)
    {
	if (xvals[i] > graph->x_max) 
	{
	    graph->x_max = xvals[i];
	    isdiff=1;
	}
	if (xvals[i] < graph->x_min) 
	{
	    graph->x_min = xvals[i];
	    isdiff=1;
	}
	if (yvals[i] > graph->y_max) 
	{
	    graph->y_max = yvals[i];
	    isdiff=1;
	}
	if (yvals[i] < graph->y_min) 
	{
	    graph->y_min = yvals[i];
	    isdiff=1;
	}
    }
    if (isdiff == 1) 
    {
	gwy_axis_set_req(graph->axis_top, graph->x_min, graph->x_max);
	gwy_axis_set_req(graph->axis_bottom, graph->x_min, graph->x_max);
	gwy_axis_set_req(graph->axis_left, graph->y_min, graph->y_max);
	gwy_axis_set_req(graph->axis_right, graph->y_min, graph->y_max);
	
	graph->x_max = gwy_axis_get_maximum(graph->axis_bottom);
	graph->x_min = gwy_axis_get_minimum(graph->axis_bottom);
	graph->y_max = gwy_axis_get_maximum(graph->axis_left);
	graph->y_min = gwy_axis_get_minimum(graph->axis_left);
    }
	
    /*make curve (precompute screeni coordinates of points)*/
    gwy_graph_make_curve_data(graph, &curve, xvals, yvals, n);
    
    /*configure curve plot properties*/
    curve.params.is_line = graph->autoproperties.is_line;
    curve.params.is_point = graph->autoproperties.is_point;
    curve.params.point_size = graph->autoproperties.point_size;
    curve.params.description = g_string_new("Ble");
    /***** PROVISORY ***************/
    if (graph->n_of_curves == 0) curve.params.color.pixel = 0x00000000;
    if (graph->n_of_curves == 1) curve.params.color.pixel = 0x00990099;
    if (graph->n_of_curves == 2) curve.params.color.pixel = 0x09909900;
    if (graph->n_of_curves == 3) curve.params.color.pixel = 0x000ddd00;
    if (graph->n_of_curves == 4) curve.params.color.pixel = 0x00ff0055;
    /**** END OF PROVISORY ******/
    
    /*put curve and (new) boundaries into the plotter*/
    gwy_graph_area_add_curve(graph->area, &curve);
gwy_graph_area_set_boundaries(graph->area, graph->x_min, graph->x_max, graph->y_min, graph->y_max);
    
    g_free(curve.data.xvals);
    g_free(curve.data.yvals);

    graph->n_of_curves++;
    graph->n_of_autocurves++;
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


