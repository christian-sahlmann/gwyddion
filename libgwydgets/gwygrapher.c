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
#include "gwygrapher.h"

#define GWY_GRAPHER_TYPE_NAME "GwyGrapher"


static void     gwy_grapher_class_init           (GwyGrapherClass *klass);
static void     gwy_grapher_init                 (GwyGrapher *grapher);
static void     gwy_grapher_size_request         (GtkWidget *widget,
                                                GtkRequisition *requisition);
static void     gwy_grapher_size_allocate        (GtkWidget *widget,
                                                GtkAllocation *allocation);
static void     gwy_grapher_make_curve_data      (GwyGrapher *grapher,
                                                GwyGrapherAreaCurve *curve,
                                                gdouble *xvals,
                                                gdouble *yvals,
                                                gint n);
static void     gwy_grapher_synchronize          (GwyGrapher *grapher);
static void     zoomed_cb                      (GtkWidget *widget);

static GtkWidgetClass *parent_class = NULL;


GType
gwy_grapher_get_type(void)
{
    static GType gwy_grapher_type = 0;
    if (!gwy_grapher_type) {
        static const GTypeInfo gwy_grapher_info = {
         sizeof(GwyGrapherClass),
         NULL,
         NULL,
         (GClassInitFunc)gwy_grapher_class_init,
         NULL,
         NULL,
         sizeof(GwyGrapher),
         0,
         (GInstanceInitFunc)gwy_grapher_init,
         NULL,
         };
        gwy_debug("");
        gwy_grapher_type = g_type_register_static (GTK_TYPE_TABLE,
                                                 GWY_GRAPHER_TYPE_NAME,
                                                 &gwy_grapher_info,
                                                 0);

    }

    return gwy_grapher_type;
}

static void
gwy_grapher_class_init(GwyGrapherClass *klass)
{
    GtkWidgetClass *widget_class;

    gwy_debug("");

    widget_class = (GtkWidgetClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    widget_class->size_request = gwy_grapher_size_request;
    widget_class->size_allocate = gwy_grapher_size_allocate;

}


static void
gwy_grapher_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
    GTK_WIDGET_CLASS(parent_class)->size_request(widget, requisition);
    requisition->width = 500;
    requisition->height = 400;

    gwy_grapher_synchronize(GWY_GRAPHER(widget));
}

static void
gwy_grapher_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    GwyGrapher *grapher;
    gwy_debug("");

    grapher = GWY_GRAPHER(widget);
    GTK_WIDGET_CLASS(parent_class)->size_allocate(widget, allocation);

    /*synchronize axis and area (axis range can change)*/
    gwy_grapher_synchronize(grapher);
}

static void
gwy_grapher_synchronize(GwyGrapher *grapher)
{
    /*
    gwy_grapher_area_set_boundaries(
                                    gwy_axiser_get_minimum(grapher->axis_bottom),
                                    gwy_axiser_get_maximum(grapher->axis_bottom),
                                    gwy_axiser_get_minimum(grapher->axis_left),
                                    gwy_axiser_get_maximum(grapher->axis_left));
*/
}

static void
gwy_grapher_init(GwyGrapher *grapher)
{
    gwy_debug("");

    grapher->n_of_autocurves = 0;

    grapher->autoproperties.is_line = 1;
    grapher->autoproperties.is_point = 1;
    grapher->autoproperties.point_size = 8;
    grapher->autoproperties.line_size = 1;

    gtk_table_resize (GTK_TABLE (grapher), 3, 3);
    gtk_table_set_homogeneous (GTK_TABLE (grapher), FALSE);
    gtk_table_set_row_spacings (GTK_TABLE (grapher), 0);
    gtk_table_set_col_spacings (GTK_TABLE (grapher), 0);

    grapher->axis_top = GWY_AXISER(gwy_axiser_new(GWY_AXISER_SOUTH, 2.24, 5.21, "x"));
    grapher->axis_bottom = GWY_AXISER(gwy_axiser_new(GWY_AXISER_NORTH, 2.24, 5.21, "x"));
    grapher->axis_left = GWY_AXISER(gwy_axiser_new(GWY_AXISER_EAST, 100, 500, "y"));
    grapher->axis_right = GWY_AXISER(gwy_axiser_new(GWY_AXISER_WEST, 100, 500, "y"));

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

    grapher->corner_tl = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());
    grapher->corner_bl = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());
    grapher->corner_tr = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());
    grapher->corner_br = GWY_GRAPHER_CORNER(gwy_grapher_corner_new());


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

    grapher->area = GWY_GRAPHER_AREA(gwy_grapher_area_new(NULL,NULL));

    grapher->area->status = GWY_GRAPHER_STATUS_PLAIN;

    gtk_table_attach(GTK_TABLE (grapher), GTK_WIDGET(grapher->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    g_signal_connect(grapher->area, "zoomed", G_CALLBACK(zoomed_cb), NULL);

    gtk_widget_show_all(GTK_WIDGET(grapher->area));

}

GtkWidget*
gwy_grapher_new()
{
    gwy_debug("");
    return GTK_WIDGET(g_object_new(gwy_grapher_get_type(), NULL));
}


/**
 * gwy_grapher_add_dataline:
 * @grapher: grapher widget
 * @dataline: dataline to be added
 * @shift: x shift (dataline starts allways at zero)
 * @label: curve label
 * @params: parameters of curve (lines/points etc.)
 *
 * Adds a dataline into grapher.
 *
void
gwy_grapher_add_dataline(GwyGrapher *grapher, GwyDataLine *dataline,
                       G_GNUC_UNUSED gdouble shift, GString *label,
                       G_GNUC_UNUSED GwyGrapherAreaCurveParams *params)
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


    gwy_grapher_add_datavalues(grapher, xdata, dataline->data,
                             n, label, NULL);

    g_free(xdata);
    */


/**
 * gwy_grapher_add_dataline_with_units:
 * @grapher: grapher widget
 * @dataline: dataline to be added
 * @shift: x shift (dataline starts allways at zero)
 * @label: curve label
 * @params: parameters of curve (lines/points etc.)
 * @x_order: division factor to obtain values corresponding to units
 * @y_order: division factor to obtain values corresponding to units
 * @x_unit: unit at x axis
 * @y_unit: unit at y axis
 *
 * Adds a datalien into grapher, setting units. Original dataline data
 * will be divided by @x_order and @y_order factors and axis labels
 * will have requested units.
 *
void
gwy_grapher_add_dataline_with_units(GwyGrapher *grapher, GwyDataLine *dataline,
                                  G_GNUC_UNUSED gdouble shift, GString *label,
                                  G_GNUC_UNUSED GwyGrapherAreaCurveParams *params,
                                  gdouble x_order, gdouble y_order,
                                  char *x_unit, char *y_unit)


    gdouble *xdata, *ydata;
    gint n, i;

    gwy_debug("");
    n = gwy_data_line_get_res(dataline);

    prepare values (divide by orders)*/
 /*   xdata = (gdouble *) g_malloc(n*sizeof(gdouble));
    ydata = (gdouble *) g_malloc(n*sizeof(gdouble));
    for (i = 0; i < n; i++) {
        xdata[i] = i*gwy_data_line_get_real(dataline)/((gdouble)n)/x_order;
        ydata[i] = gwy_data_line_get_val(dataline, i)/y_order;
    }

    add values*/
    /*
    gwy_grapher_add_datavalues(grapher, xdata, ydata,
                             n, label, NULL);

    add unit to grapher axis*/
    /*
    if (x_unit != NULL) {
        grapher->x_unit = g_strdup(x_unit);
        grapher->has_x_unit = 1;

        gwy_axiser_set_unit(grapher->axis_top, grapher->x_unit);
        gwy_axiser_set_unit(grapher->axis_bottom, grapher->x_unit);
    }
    if (y_unit != NULL) {
        grapher->y_unit = g_strdup(y_unit);
        grapher->has_y_unit = 1;
        gwy_axiser_set_unit(grapher->axis_left, grapher->y_unit);
        gwy_axiser_set_unit(grapher->axis_right, grapher->y_unit);
    }

    g_free(xdata);
    g_free(ydata);
    */


/**
 * gwy_grapher_add_datavalues:
 * @grapher: grapher widget
 * @xvals: x values
 * @yvals: y values
 * @n: number of values
 * @label: curve label
 * @params: arameters of curve (lines/points etc.)
 *
 * Adds raw data to the grapher. Data are represented by two arrays
 * of same size.
 *
void
gwy_grapher_add_datavalues(GwyGrapher *grapher, gdouble *xvals, gdouble *yvals,
                         gint n, GString *label,
                         GwyGrapherAreaCurveParams *params)
{
    
    gint i;
    gboolean isdiff;
    GwyGrapherAreaCurve curve;
    GdkColormap *cmap;

    gwy_debug("");

    look whether label maximum or minium will be changed*/
    /*isdiff = FALSE;
    for (i = 0; i < n; i++)
    {
       if (xvals[i] > grapher->x_reqmax) {
          grapher->x_reqmax = xvals[i];
          isdiff = TRUE;
       }
       if (xvals[i] < grapher->x_reqmin) {
          grapher->x_reqmin = xvals[i];
          
          isdiff = TRUE;
       }
       if (yvals[i] > grapher->y_reqmax) {
          grapher->y_reqmax = yvals[i];
          isdiff = TRUE;
       }
       if (yvals[i] < grapher->y_reqmin) {
          grapher->y_reqmin = yvals[i];
          isdiff = TRUE;
       }
    }

    if (grapher->y_reqmax > 1e20 || grapher->y_reqmax < -1e20
       || grapher->y_reqmin > 1e20 || grapher->y_reqmin < -1e20)
    {
        g_warning("Data values are corrupted. Curve not added.");
        return;
    }

    if (isdiff) {
      
       gwy_axiser_set_req(grapher->axis_top, grapher->x_reqmin, grapher->x_reqmax);
       gwy_axiser_set_req(grapher->axis_bottom, grapher->x_reqmin, grapher->x_reqmax);
       gwy_axiser_set_req(grapher->axis_left, grapher->y_reqmin, grapher->y_reqmax);
       gwy_axiser_set_req(grapher->axis_right, grapher->y_reqmin, grapher->y_reqmax);

       grapher->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
       grapher->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
       grapher->y_max = gwy_axiser_get_maximum(grapher->axis_left);
       grapher->y_min = gwy_axiser_get_minimum(grapher->axis_left);
       grapher->x_reqmax = gwy_axiser_get_reqmaximum(grapher->axis_bottom);
       grapher->x_reqmin = gwy_axiser_get_reqminimum(grapher->axis_bottom);
       grapher->y_reqmax = gwy_axiser_get_reqmaximum(grapher->axis_left);
       grapher->y_reqmin = gwy_axiser_get_reqminimum(grapher->axis_left);
     }

    make curve (precompute screeni coordinates of points)*/
    /*
    gwy_grapher_make_curve_data(grapher, &curve, xvals, yvals, n);

    configure curve plot properties*/
    /*cmap =  gdk_colormap_get_system();
    if (params == NULL) {
      curve.params.is_line = grapher->autoproperties.is_line;
      curve.params.is_point = grapher->autoproperties.is_point;
      curve.params.point_size = grapher->autoproperties.point_size;
      curve.params.line_size = grapher->autoproperties.line_size;
      curve.params.line_style = GDK_LINE_SOLID;
      curve.params.description = g_string_new(label->str);
      ***** FIXME PROVISORY ***************
      if (grapher->n_of_autocurves == 0) {
	  
          curve.params.color.red = 0x0000;
	  curve.params.color.green = 0x0000;
	  curve.params.color.blue = 0x0000;
          curve.params.point_type = GWY_GRAPHER_POINT_TRIANGLE_UP;
      }
      if (grapher->n_of_autocurves == 1) {
          curve.params.color.red = 0xaaaa;
	  curve.params.color.green = 0x0000;
	  curve.params.color.blue = 0x0000;
          curve.params.point_type = GWY_GRAPHER_POINT_TRIANGLE_DOWN;
      }
      if (grapher->n_of_autocurves == 2) {
          curve.params.color.red = 0x0000;
	  curve.params.color.green = 0xaaaa;
	  curve.params.color.blue = 0x0000;
          curve.params.point_type = GWY_GRAPHER_POINT_CIRCLE;
      }
      if (grapher->n_of_autocurves == 3) {
          curve.params.color.red = 0x0000;
	  curve.params.color.green = 0x0000;
	  curve.params.color.blue = 0xaaaa;
           curve.params.point_type = GWY_GRAPHER_POINT_DIAMOND;
      }
      if (grapher->n_of_autocurves == 4) {
          curve.params.color.red = 0x8888;
	  curve.params.color.green = 0x0000;
	  curve.params.color.blue = 0x8888;
          curve.params.point_type = GWY_GRAPHER_POINT_TIMES;
      }
       gdk_colormap_alloc_color(cmap, &curve.params.color, FALSE, TRUE);
      
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
      gdk_colormap_alloc_color(cmap, &curve.params.color, FALSE, TRUE);
    }

    put curve and (new) boundaries into the plotter
    gwy_grapher_area_add_curve(grapher->area, &curve);
    gwy_grapher_area_set_boundaries(grapher->area, grapher->x_min,
                                  grapher->x_max, grapher->y_min, grapher->y_max);

    g_free(curve.data.xvals);
    g_free(curve.data.yvals);

    grapher->n_of_curves++;
    if (params == NULL)
        grapher->n_of_autocurves++;

    
    gtk_widget_queue_draw(GTK_WIDGET(grapher));
}
*/
static void
gwy_grapher_make_curve_data(G_GNUC_UNUSED GwyGrapher *grapher,
                          GwyGrapherAreaCurve *curve,
                          gdouble *xvals, gdouble *yvals, gint n)
{
    /*
    curve->data.N = n;
    curve->data.xvals = g_memdup(xvals, n*sizeof(gdouble));
    curve->data.yvals = g_memdup(yvals, n*sizeof(gdouble));
    */
}

/**
 * gwy_grapher_clear:
 * @grapher: grapher widget
 *
 * Remove all curves.
 **/
void
gwy_grapher_clear(GwyGrapher *grapher)
{
    /*
    gwy_grapher_area_clear(grapher->area);
    grapher->n_of_autocurves = 0;
    grapher->n_of_curves = 0;
    grapher->x_max = 0;
    grapher->y_max = 0;
    grapher->x_min = 0;
    grapher->x_min = 0;
    grapher->x_reqmax = -G_MAXDOUBLE;
    grapher->y_reqmax = -G_MAXDOUBLE;
    grapher->x_reqmin = G_MAXDOUBLE;
    grapher->y_reqmin = G_MAXDOUBLE;
*/
}

void
gwy_grapher_set_boundaries(GwyGrapher *grapher, gdouble x_min, gdouble x_max, gdouble y_min, gdouble y_max)
{
    /*
    set the grapher requisition*/
    
    /*grapher->x_reqmin = x_min;
    grapher->x_reqmax = x_max;
    grapher->y_reqmin = y_min;
    grapher->y_reqmax = y_max;

    ask axis, what does she thinks about the requisitions*/
    /*gwy_axiser_set_req(grapher->axis_top, grapher->x_reqmin, grapher->x_reqmax);
    gwy_axiser_set_req(grapher->axis_bottom, grapher->x_reqmin, grapher->x_reqmax);
    gwy_axiser_set_req(grapher->axis_left, grapher->y_reqmin, grapher->y_reqmax);
    gwy_axiser_set_req(grapher->axis_right, grapher->y_reqmin, grapher->y_reqmax);

    of course, axis is never satisfied..*/
    /*grapher->x_max = gwy_axiser_get_maximum(grapher->axis_bottom);
    grapher->x_min = gwy_axiser_get_minimum(grapher->axis_bottom);
    grapher->y_max = gwy_axiser_get_maximum(grapher->axis_left);
    grapher->y_min = gwy_axiser_get_minimum(grapher->axis_left);
    grapher->x_reqmax = gwy_axiser_get_reqmaximum(grapher->axis_bottom);
    grapher->x_reqmin = gwy_axiser_get_reqminimum(grapher->axis_bottom);
    grapher->y_reqmax = gwy_axiser_get_reqmaximum(grapher->axis_left);
    grapher->y_reqmin = gwy_axiser_get_reqminimum(grapher->axis_left);

    refresh grapher*/
    /*gwy_grapher_area_set_boundaries(grapher->area, grapher->x_min,
                     grapher->x_max, grapher->y_min, grapher->y_max);
*/

}

/**
 * gwy_grapher_unzoom:
 * @grapher: grapher widget
 *
 * resets zoom. Fits all curves into grapher.
 **/
void
gwy_grapher_unzoom(GwyGrapher *grapher)
{
    /*
    GwyGrapherAreaCurve *pcurve;
    gdouble xmax, xmin, ymax, ymin;
    gsize i;
    gint j;

    xmin = G_MAXDOUBLE;
    ymin = G_MAXDOUBLE;
    xmax = -G_MAXDOUBLE;
    ymax = -G_MAXDOUBLE;

    gwy_debug("");

    
    for (i = 0; i < grapher->area->curves->len; i++) {
        pcurve = g_ptr_array_index(grapher->area->curves, i);
        for (j = 0; j < pcurve->data.N; j++) {
            if (pcurve->data.xvals[j] > xmax)
                xmax = pcurve->data.xvals[j];
            if (pcurve->data.yvals[j] > ymax)
                ymax = pcurve->data.yvals[j];
            if (pcurve->data.xvals[j] < xmin)
                xmin = pcurve->data.xvals[j];
            if (pcurve->data.yvals[j] < ymin)
                ymin = pcurve->data.yvals[j];
        }
    }
    gwy_grapher_set_boundaries(grapher, xmin, xmax, ymin, ymax);
    gtk_widget_queue_draw(GTK_WIDGET(grapher));
    */
}

static void
zoomed_cb(GtkWidget *widget)
{
    /*
    GwyGrapher *grapher;
    gwy_debug("");

    grapher = GWY_GRAPHER(gtk_widget_get_parent(widget));

    gwy_grapher_set_boundaries(grapher,
                             grapher->area->zoomdata->xmin,
                             grapher->area->zoomdata->xmax,
                             grapher->area->zoomdata->ymin,
                             grapher->area->zoomdata->ymax);
    gtk_widget_queue_draw(GTK_WIDGET(grapher));
    */
}

/**
 * gwy_grapher_enable_axis_label_update:
 * @grapher: A grapher widget.
 * @enable: Enable or disable user to change label
 *
 * Enables/disables user to interact with grapher label by clickig on it and
 * changing text.
 *
 * Since: 1.3.
 **/
void
gwy_grapher_enable_axis_label_edit(GwyGrapher *grapher, gboolean enable)
{
    gwy_axiser_enable_label_edit(grapher->axis_top, enable);
    gwy_axiser_enable_label_edit(grapher->axis_bottom, enable);
    gwy_axiser_enable_label_edit(grapher->axis_left, enable);
    gwy_axiser_enable_label_edit(grapher->axis_right, enable);
}



/*refresh everything in graph according to the model: reset axis requirements,
 reset all points etc.*/
void       
gwy_grapher_refresh(GwyGrapher *grapher)
{
    /*refresh axis and reset axis requirements*/
    
    /*refresh widgets*/
    gwy_grapher_area_refresh(grapher->area);
    
}


void
gwy_grapher_change_model(GwyGrapher *grapher, gpointer *gmodel)
{
    grapher->grapher_model = gmodel;
    gwy_grapher_area_change_model(grapher->area, gmodel);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
