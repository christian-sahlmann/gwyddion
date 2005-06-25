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
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"

#define GWY_GRAPH_TYPE_NAME "GwyGraph"

enum {
    SELECTED_SIGNAL,
    MOUSEMOVED_SIGNAL,
    ZOOMED_SIGNAL,
    LAST_SIGNAL
};


static void     gwy_graph_class_init           (GwyGraphClass *klass);
static void     gwy_graph_init                 (GwyGraph *grapher);
static void     gwy_graph_size_request         (GtkWidget *widget,
                                                GtkRequisition *requisition);
static void     gwy_graph_size_allocate        (GtkWidget *widget,
                                                GtkAllocation *allocation);
static void     rescaled_cb                    (GtkWidget *widget,
                                                GwyGraph *grapher);
static void     replot_cb                        (GObject *gobject, 
                                                  GParamSpec *arg1, 
                                                  GwyGraph *grapher);
static void     zoomed_cb                         (GwyGraph *grapher);

static GtkWidgetClass *parent_class = NULL;
static guint gwygraph_signals[LAST_SIGNAL] = { 0 };



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

    klass->selected = NULL;
    klass->mousemoved = NULL;
    klass->zoomed = NULL;
    
    gwygraph_signals[SELECTED_SIGNAL]
                = g_signal_new ("selected",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_STRUCT_OFFSET (GwyGraphClass, selected),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

    gwygraph_signals[MOUSEMOVED_SIGNAL]
                = g_signal_new ("mousemoved",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_STRUCT_OFFSET (GwyGraphClass, mousemoved),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

    gwygraph_signals[ZOOMED_SIGNAL]
                = g_signal_new ("zoomed",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_STRUCT_OFFSET (GwyGraphClass, zoomed),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
}


static void
gwy_graph_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition);
    requisition->width = 300;
    requisition->height = 200;
}

static void
gwy_graph_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGraph *grapher;
    gwy_debug("");

    grapher = GWY_GRAPH(widget);
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);
}


static void
gwy_graph_init(GwyGraph *grapher)
{
    gwy_debug("");


    gtk_table_resize (GTK_TABLE (grapher), 3, 3);
    gtk_table_set_homogeneous (GTK_TABLE (grapher), FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (grapher), 0);
    gtk_table_set_col_spacings (GTK_TABLE (grapher), 0);

    grapher->axis_top = GWY_AXISER(gwy_axiser_new(GTK_POS_TOP, 2.24, 5.21, "x"));
    grapher->axis_bottom = GWY_AXISER(gwy_axiser_new(GTK_POS_BOTTOM, 2.24, 5.21, "x"));
    grapher->axis_left = GWY_AXISER(gwy_axiser_new(GTK_POS_LEFT, 100, 500, "y"));
    grapher->axis_right = GWY_AXISER(gwy_axiser_new(GTK_POS_RIGHT, 100, 500, "y"));

    g_signal_connect(grapher->axis_left, "rescaled", G_CALLBACK(rescaled_cb), grapher);
    g_signal_connect(grapher->axis_bottom, "rescaled", G_CALLBACK(rescaled_cb), grapher);
    

    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_top), 1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_bottom), 1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_left), 2, 3, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->axis_right), 0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show(GTK_WIDGET(grapher->axis_top));
    gtk_widget_show(GTK_WIDGET(grapher->axis_bottom));
    gtk_widget_show(GTK_WIDGET(grapher->axis_left));
    gtk_widget_show(GTK_WIDGET(grapher->axis_right));

    grapher->corner_tl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    grapher->corner_bl = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    grapher->corner_tr = GWY_GRAPH_CORNER(gwy_graph_corner_new());
    grapher->corner_br = GWY_GRAPH_CORNER(gwy_graph_corner_new());


    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_tl), 0, 1, 0, 1,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_bl), 2, 3, 0, 1,
                     GTK_FILL, GTK_FILL , 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_tr), 0, 1, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->corner_br), 2, 3, 2, 3,
                     GTK_FILL, GTK_FILL, 0, 0);

    gtk_widget_show(GTK_WIDGET(grapher->corner_tl));
    gtk_widget_show(GTK_WIDGET(grapher->corner_bl));
    gtk_widget_show(GTK_WIDGET(grapher->corner_tr));
    gtk_widget_show(GTK_WIDGET(grapher->corner_br));

    grapher->area = GWY_GRAPH_AREA(gwy_graph_area_new(NULL,NULL));

    grapher->area->status = GWY_GRAPH_STATUS_PLAIN;
    grapher->enable_user_input = TRUE;

    g_signal_connect_swapped(grapher->area, "selected",
                     G_CALLBACK(gwy_graph_signal_selected), grapher);

    g_signal_connect_swapped(grapher->area, "mousemoved",
                     G_CALLBACK(gwy_graph_signal_mousemoved), grapher);
     
    g_signal_connect_swapped(grapher->area, "zoomed",
                     G_CALLBACK(zoomed_cb), grapher);

    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);

    gtk_widget_show_all(GTK_WIDGET(grapher->area));

}


/**
 * gwy_graph_new:
 * @gmodel: A grapher model.
 * @enable: Enable or disable user to change label
 *
 * Creates grapher widget based on information in model. 
 *
 * Returns: new grapher widget.
 **/
GtkWidget*
gwy_graph_new(GwyGraphModel *gmodel)
{
    GtkWidget *grapher = GTK_WIDGET(g_object_new(gwy_graph_get_type(), NULL));
    gwy_debug("");

    if (gmodel != NULL)
    {
       gwy_graph_change_model(GWY_GRAPH(grapher), gmodel);    
    
       g_signal_connect_swapped(gmodel, "value_changed",
                     G_CALLBACK(gwy_graph_refresh), grapher);

       gwy_graph_refresh(grapher);
    }
    
    return grapher;
}



/**
 * gwy_graph_enable_axis_label_update:
 * @grapher: A grapher widget.
 * @enable: Enable or disable user to change label
 *
 * Enables/disables user to interact with grapher label by clickig on it and
 * changing text.
 **/
void
gwy_graph_enable_axis_label_edit(GwyGraph *grapher, gboolean enable)
{
    gwy_axiser_enable_label_edit(grapher->axis_top, enable);
    gwy_axiser_enable_label_edit(grapher->axis_bottom, enable);
    gwy_axiser_enable_label_edit(grapher->axis_left, enable);
    gwy_axiser_enable_label_edit(grapher->axis_right, enable);
}



/**
 * gwy_graph_refresh:
 * @grapher: A grapher widget.
 *
 * Refresh all the graph widgets according to the model.
 *
 **/
void       
gwy_graph_refresh(GwyGraph *grapher)
{
    GwyGraphModel *model;
    GwyGraphCurveModel *curvemodel;
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gint i, j;
   
    if (grapher->graph_model == NULL) return;
    model = GWY_GRAPH_MODEL(grapher->graph_model);

    gwy_axiser_set_unit(grapher->axis_top, model->x_unit);
    gwy_axiser_set_unit(grapher->axis_bottom, model->x_unit);
    gwy_axiser_set_unit(grapher->axis_left, model->y_unit);
    gwy_axiser_set_unit(grapher->axis_right, model->y_unit);
    if (model->ncurves > 0)
    {
    
        /*refresh axis and reset axis requirements*/
        x_reqmin = y_reqmin = G_MAXDOUBLE;
        x_reqmax = y_reqmax = -G_MAXDOUBLE;
        for (i=0; i<model->ncurves; i++)
        {
            curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[i]);
            for (j=0; j<curvemodel->n; j++)
            {
                if (x_reqmin > curvemodel->xdata[j]) x_reqmin = curvemodel->xdata[j];
                if (y_reqmin > curvemodel->ydata[j]) y_reqmin = curvemodel->ydata[j];
                if (x_reqmax < curvemodel->xdata[j]) x_reqmax = curvemodel->xdata[j];
                if (y_reqmax < curvemodel->ydata[j]) y_reqmax = curvemodel->ydata[j];
            }
        }
        gwy_axiser_set_req(grapher->axis_top, x_reqmin, x_reqmax);
        gwy_axiser_set_req(grapher->axis_bottom, x_reqmin, x_reqmax);
        gwy_axiser_set_req(grapher->axis_left, y_reqmin, y_reqmax);
        gwy_axiser_set_req(grapher->axis_right, y_reqmin, y_reqmax);

        model->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
        model->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
        model->y_max = gwy_axiser_get_maximum(grapher->axis_left);
        model->y_min = gwy_axiser_get_minimum(grapher->axis_left);
    }

    /*refresh widgets*/
    gwy_graph_area_refresh(grapher->area);
    
}

/**
 * gwy_graph_refresh_reset:
 * @grapher: A grapher widget.
 *
 * Refresh all the graph widgets according to the model.
 * The axis will be set to display all data in a best way.
 **/
void       
gwy_graph_refresh_reset(GwyGraph *grapher)
{
    /*refresh widgets*/
    gwy_graph_area_refresh(grapher->area);
    
}
static void 
replot_cb(GObject *gobject, GParamSpec *arg1, GwyGraph *grapher)
{
    if (grapher == NULL || grapher->graph_model == NULL) return;
    gwy_graph_refresh(grapher);
}

/**
 * gwy_graph_change_model:
 * @grapher: A grapher widget.
 * @gmodel: new grapher model 
 *
 * Changes the grapher model. Everything in grapher widgets will
 * be reset to the new data (from the model).
 *
 **/
void
gwy_graph_change_model(GwyGraph *grapher, GwyGraphModel *gmodel)
{
    grapher->graph_model = gmodel;

    g_signal_connect(gmodel, "notify", G_CALLBACK(replot_cb), grapher);
    gwy_graph_area_change_model(grapher->area, gmodel);
}

static void     
rescaled_cb(GtkWidget *widget, GwyGraph *grapher)
{   
    GwyGraphModel *model;
    if (grapher->graph_model == NULL) return;
    model = GWY_GRAPH_MODEL(grapher->graph_model);
    model->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
    model->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
    model->y_max = gwy_axiser_get_maximum(grapher->axis_left);
    model->y_min = gwy_axiser_get_minimum(grapher->axis_left);

    gwy_graph_area_refresh(grapher->area);
}

/**
 * gwy_graph_get_model:
 * @grapher: A grapher widget.
 *
 * Returns: GraphModel associated with this grapher widget.
 **/
GwyGraphModel *gwy_graph_get_model(GwyGraph *grapher)
{
    return  grapher->graph_model;
}

/**
 * gwy_graph_set_status:
 * @grapher: A grapher widget.
 * @status: new grapher model 
 *
 * Set status of the grapher widget. Status determines the way how the grapher
 * reacts on mouse events, basically. This includes point or area selectiuon and zooming.
 *
 **/
void
gwy_graph_set_status(GwyGraph *grapher, GwyGraphStatusType status)
{
    grapher->area->status = status;
}

GwyGraphStatusType  
gwy_graph_get_status(GwyGraph *grapher)
{
    return grapher->area->status;
}

/**
 * gwy_graph_get_selection_number:
 * @grapher: A grapher widget.
 *
 * Set status of the grapher widget. Status determines the way how the grapher
 * reacts on mouse events, basically. This includes point or area selectiuon and zooming.
 *
 * Returns: number of selections
 **/
gint       
gwy_graph_get_selection_number(GwyGraph *grapher)
{
    if (grapher->area->status == GWY_GRAPH_STATUS_XSEL)
        return grapher->area->areasdata->data_areas->len;
    else if (grapher->area->status ==  GWY_GRAPH_STATUS_POINTS)
        return grapher->area->pointsdata->data_points->len;
    else return 0;
}

void
gwy_graph_get_selection(GwyGraph *grapher, gdouble *selection)
{
    gint i;
    GwyGraphDataArea *data_area;
    GwyGraphDataPoint *data_point;
    
    if (selection == NULL) return;

    switch (grapher->area->status)
    {
        case GWY_GRAPH_STATUS_XSEL:    
        for (i = 0; i < grapher->area->areasdata->data_areas->len; i++)
        {
            data_area = &g_array_index(grapher->area->areasdata->data_areas, GwyGraphDataArea, i);
            selection[2*i] = data_area->xmin;
            selection[2*i + 1] = data_area->xmax;
        }
        break;

        case GWY_GRAPH_STATUS_YSEL:
        for (i = 0; i < grapher->area->areasdata->data_areas->len; i++)
        {
            data_area = &g_array_index(grapher->area->areasdata->data_areas, GwyGraphDataArea, i);
            selection[2*i] = data_area->ymin;
            selection[2*i + 1] = data_area->ymax;
        }
        break;

        case GWY_GRAPH_STATUS_POINTS:
        for (i = 0; i < grapher->area->pointsdata->data_points->len; i++)
        {
            data_point = &g_array_index(grapher->area->pointsdata->data_points, GwyGraphDataPoint, i);
            selection[2*i] = data_point->x;
            selection[2*i + 1] = data_point->y;
        }
        break;

        case GWY_GRAPH_STATUS_ZOOM:
        if (grapher->area->zoomdata->width>0)
        {
            selection[0] = grapher->area->zoomdata->xmin;
            selection[1] = grapher->area->zoomdata->width;
        }
        else
        {
            selection[0] = grapher->area->zoomdata->xmin + grapher->area->zoomdata->width;
            selection[1] = -grapher->area->zoomdata->width;
        }

        if (grapher->area->zoomdata->height>0)
        {
            selection[2] = grapher->area->zoomdata->ymin;
            selection[3] = grapher->area->zoomdata->height;
        }
        else
        {
            selection[2] = grapher->area->zoomdata->ymin + grapher->area->zoomdata->height;
            selection[3] = -grapher->area->zoomdata->height;
        }
         break;
        
        default:
        g_assert_not_reached();   
    }
}

void       
gwy_graph_clear_selection(GwyGraph *grapher)
{
    gwy_graph_area_clear_selection(grapher->area);
}

void       
gwy_graph_request_x_range(GwyGraph *grapher, gdouble x_min_req, gdouble x_max_req)
{
    GwyGraphModel *model;
    
    if (grapher->graph_model == NULL) return;
    model = GWY_GRAPH_MODEL(grapher->graph_model);

    gwy_axiser_set_req(grapher->axis_top, x_min_req, x_max_req);
    gwy_axiser_set_req(grapher->axis_bottom, x_min_req, x_max_req);

    model->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
    model->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);

    /*refresh widgets*/
    gwy_graph_area_refresh(grapher->area);
 }

void       
gwy_graph_request_y_range(GwyGraph *grapher, gdouble y_min_req, gdouble y_max_req)
{
    GwyGraphModel *model;
    
    if (grapher->graph_model == NULL) return;
    model = GWY_GRAPH_MODEL(grapher->graph_model);

    gwy_axiser_set_req(grapher->axis_left, y_min_req, y_max_req);
    gwy_axiser_set_req(grapher->axis_right, y_min_req, y_max_req);

    model->y_max = gwy_axiser_get_maximum(grapher->axis_left);
    model->y_min = gwy_axiser_get_minimum(grapher->axis_left);

    /*refresh widgets*/
    gwy_graph_area_refresh(grapher->area);
 }

void       
gwy_graph_get_x_range(GwyGraph *grapher, gdouble *x_min, gdouble *x_max)
{
    *x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
    *x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
}

void       
gwy_graph_get_y_range(GwyGraph *grapher, gdouble *y_min, gdouble *y_max)
{
    *y_min = gwy_axiser_get_minimum(grapher->axis_left);
    *y_max = gwy_axiser_get_maximum(grapher->axis_left);
}


/**
 * gwy_graph_enable_user_input:
 * @grapher: A grapher widget.
 * @enable: whether to enable user input
 *
 * Enables/disables all the graph/curve settings dialogs to be invoked by mouse clicks.
 **/
void
gwy_graph_enable_user_input(GwyGraph *grapher, gboolean enable)
{
    grapher->enable_user_input = enable;
    gwy_graph_area_enable_user_input(grapher->area, enable);
    gwy_axiser_enable_label_edit(grapher->axis_top, enable);
    gwy_axiser_enable_label_edit(grapher->axis_bottom, enable);
    gwy_axiser_enable_label_edit(grapher->axis_left, enable);
    gwy_axiser_enable_label_edit(grapher->axis_right, enable);
    
    
}


void       
gwy_graph_signal_selected(GwyGraph *grapher)
{
    g_signal_emit (G_OBJECT (grapher), gwygraph_signals[SELECTED_SIGNAL], 0);
}

void       
gwy_graph_signal_mousemoved(GwyGraph *grapher)
{
    g_signal_emit (G_OBJECT (grapher), gwygraph_signals[MOUSEMOVED_SIGNAL], 0);
}

void       
gwy_graph_signal_zoomed(GwyGraph *grapher)
{
    g_signal_emit (G_OBJECT (grapher), gwygraph_signals[ZOOMED_SIGNAL], 0);
}

void       
gwy_graph_get_cursor(GwyGraph *grapher, gdouble *x_cursor, gdouble *y_cursor)
{
    gwy_graph_area_get_cursor(grapher->area, x_cursor, y_cursor);
}


void       
gwy_graph_zoom_in(GwyGraph *grapher)
{
    gwy_graph_set_status(grapher, GWY_GRAPH_STATUS_ZOOM);
}

void       
gwy_graph_zoom_out(GwyGraph *grapher)
{
    gwy_graph_refresh(grapher);
}

static void
zoomed_cb(GwyGraph *grapher)
{
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gdouble selection[4];
    
    if (grapher->area->status != GWY_GRAPH_STATUS_ZOOM) return;
    gwy_graph_get_selection(grapher, selection);
   
    x_reqmin = selection[0];
    x_reqmax = selection[0] + selection[1];
    y_reqmin = selection[2];
    y_reqmax = selection[2] + selection[3];
         
    gwy_axiser_set_req(grapher->axis_top, x_reqmin, x_reqmax);
    gwy_axiser_set_req(grapher->axis_bottom, x_reqmin, x_reqmax);
    gwy_axiser_set_req(grapher->axis_left, y_reqmin, y_reqmax);
    gwy_axiser_set_req(grapher->axis_right, y_reqmin, y_reqmax);

    grapher->graph_model->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
    grapher->graph_model->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
    grapher->graph_model->y_max = gwy_axiser_get_maximum(grapher->axis_left);
    grapher->graph_model->y_min = gwy_axiser_get_minimum(grapher->axis_left);

    /*refresh widgets*/
    gwy_graph_set_status(grapher, GWY_GRAPH_STATUS_PLAIN);
    gwy_graph_area_refresh(grapher->area);
    gwy_graph_signal_zoomed(grapher);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
