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
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/file.h>
#include <app/app.h>

    
/* Data for this function.*/

typedef struct {
    GtkWidget *xlabel;
    GtkWidget *ylabel;
    GPtrArray *pointx;
    GPtrArray *pointy;
    GPtrArray *distx;
    GPtrArray *disty;
    GPtrArray *slope;
    GPtrArray *labpoint;
    gint n;
} PointsControls;


static gboolean    module_register              (const gchar *name);
static gboolean    points                       (GwyGraph *graph);
static gboolean    points_dialog                (GwyGraph *graph);
static void        selection_updated_cb         (GtkWidget *widget, gpointer data);
static void        clear                        (GtkWidget *widget, gpointer data);
static void        points_dialog_closed_cb      (GtkWidget *widget, gpointer data);
static void        points_dialog_response_cb    (GtkWidget *widget, gint arg1, gpointer data);

static PointsControls controls;
static GtkWidget *dialog = NULL;
static gulong response_id = 0;
static gulong selection_id = 0;

static gint NMAX = 10;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "graph_points",
    "Measure distances between points",
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyGraphFuncInfo points_func_info = {
        "graph_points",
        "/_Measure distances",
        (GwyGraphFunc)&points,
    };

    gwy_graph_func_register(name, &points_func_info);

    return TRUE;
}

static gboolean
points(GwyGraph *graph)
{
 
     
    if (!graph) {
        if (dialog) gtk_widget_destroy(dialog);
        dialog = NULL;    
        return 1;
    }
  
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_POINTS);
    if (!dialog) points_dialog(graph);
    gtk_widget_queue_draw(GTK_WIDGET(graph)); 
    
    return 1;
}


static gboolean
points_dialog(GwyGraph *graph)
{
    gint i;
    GtkWidget *label;
    GtkWidget *table;
    gint response;
    
    dialog = gtk_dialog_new_with_buttons(_("Measure distances"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         _("Clear"), GTK_RESPONSE_REJECT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);

    g_signal_connect(dialog, "delete_event",
                     G_CALLBACK(points_dialog_closed_cb), graph);
    response_id = g_signal_connect(dialog, "response",
                     G_CALLBACK(points_dialog_response_cb), graph);

    g_signal_connect(graph, "destroy",
                     G_CALLBACK(points_dialog_closed_cb), graph);
    
    table = gtk_table_new(6, 13, FALSE);
    
    controls.xlabel = gtk_label_new("x");
    controls.ylabel = gtk_label_new("y");


    gtk_table_attach(GTK_TABLE(table), controls.xlabel, 1, 2, 0, 1,
                           GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

    gtk_table_attach(GTK_TABLE(table), controls.ylabel, 2, 3, 0, 1,
                           GTK_FILL, GTK_FILL | GTK_EXPAND, 2, 2);

    
    selection_id = g_signal_connect(graph->area, "selected", G_CALLBACK(selection_updated_cb), graph); 

    controls.labpoint = g_ptr_array_new();   
    controls.pointx = g_ptr_array_new();
    controls.pointy = g_ptr_array_new();
    controls.distx = g_ptr_array_new();
    controls.disty = g_ptr_array_new();
    controls.slope = g_ptr_array_new();
 
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Mouse:</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>Points:</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>x</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>y</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 2, 3, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>length</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 3, 4, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>height</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 4, 5, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
    label = gtk_label_new("");
    gtk_label_set_markup(GTK_LABEL(label), "<b>angle</b>");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 5, 6, 1, 2,
                     GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
     
    for (i=0; i<NMAX; i++)
    {
        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
        g_ptr_array_add(controls.labpoint, label);
        
        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
        g_ptr_array_add(controls.pointx, label);
         
        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
        g_ptr_array_add(controls.pointy, label);
         
        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
        g_ptr_array_add(controls.distx, label);
         
        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 4, 5, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
        g_ptr_array_add(controls.disty, label);
         
        label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 5, 6, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
        g_ptr_array_add(controls.slope, label);
    }  
 
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);   
 
    gtk_widget_show_all(dialog);
    
    return TRUE;
}

static void
selection_updated_cb(GtkWidget *widget, gpointer data)
{
    GwyGraph *graph;
    GwyGraphStatus_PointsData *cd;
    GwyGraphDataPoint pnt, ppnt;
    gchar buffer[50];
    gint i, n;
    
    graph = (GwyGraph *) data;
    g_return_if_fail(GWY_IS_GRAPH(graph));    

    if (gwy_graph_get_status(graph) != GWY_GRAPH_STATUS_POINTS) return;
        /*FIXME TODO XXX this must be changed XXX TODO FIXME*/
    g_assert(gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_POINTS);
    
    cd = (GwyGraphStatus_PointsData*)gwy_graph_get_status_data(graph);

    /*update mouse data*/
    if (cd->actual_data_point.x_unit != NULL)
        g_snprintf(buffer, sizeof(buffer), "x = %.3f %s", cd->actual_data_point.x, cd->actual_data_point.x_unit);
    else
        g_snprintf(buffer, sizeof(buffer), "x = %.3f", cd->actual_data_point.x);
    
    gtk_label_set_text(GTK_LABEL(controls.xlabel), buffer);

    if (cd->actual_data_point.y_unit != NULL)
        g_snprintf(buffer, sizeof(buffer), "y = %.3f %s", cd->actual_data_point.y, cd->actual_data_point.y_unit);
    else
        g_snprintf(buffer, sizeof(buffer), "y = %.3f", cd->actual_data_point.y);
     
    gtk_label_set_text(GTK_LABEL(controls.ylabel), buffer);

    /*update points data*/
    n = cd->n;
    for (i=0; i<NMAX; i++)
    {
        if (i<n)
        {
            pnt = g_array_index(cd->data_points, GwyGraphDataPoint, i);
            g_snprintf(buffer, sizeof(buffer), "%.3f", pnt.x);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.pointx, i)), buffer);
        
            g_snprintf(buffer, sizeof(buffer), "%.3f", pnt.y);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.pointy, i)), buffer);
        
            if (i>0)
            {
                ppnt = g_array_index(cd->data_points, GwyGraphDataPoint, i-1);
                g_snprintf(buffer, sizeof(buffer), "%.3f", pnt.x - ppnt.x);
                gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.distx, i)), buffer);
            
                g_snprintf(buffer, sizeof(buffer), "%.3f", pnt.y - ppnt.y);
                gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.disty, i)), buffer);

                g_snprintf(buffer, sizeof(buffer), "%.3f", 180.0*atan2((pnt.y - ppnt.y),(pnt.x - ppnt.x))/3.141592);
                gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.slope, i)), buffer);
             }
        }
        else
        {
            g_snprintf(buffer, sizeof(buffer), "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.pointx, i)), buffer);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.pointy, i)), buffer);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.distx, i)), buffer);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.disty, i)), buffer);
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.slope, i)), buffer);
        }
    }
  
}

static void        
points_dialog_closed_cb(GtkWidget *widget, gpointer data)
{
    GwyGraph *graph;
    graph = (GwyGraph *) data;
    
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);
    gtk_widget_queue_draw(GTK_WIDGET(graph));

    if (dialog) 
    {
        g_signal_handler_disconnect(dialog, response_id);
        g_signal_handler_disconnect(graph->area, selection_id);
        response_id = 0;
        selection_id = 0;
        gtk_widget_destroy(dialog);
        dialog = NULL;
        g_ptr_array_free(controls.pointx, TRUE);
        g_ptr_array_free(controls.pointy, TRUE);
        g_ptr_array_free(controls.distx, TRUE);
        g_ptr_array_free(controls.disty, TRUE);
        g_ptr_array_free(controls.slope, TRUE);
        g_ptr_array_free(controls.labpoint, TRUE);
        
    }
}


static void
points_dialog_response_cb(GtkWidget *widget, gint arg1, gpointer data)
{
    GwyGraph *graph;
    graph = (GwyGraph *) data;
    
    if (arg1==GTK_RESPONSE_REJECT)
    {
        gwy_graph_set_status(graph, GWY_GRAPH_STATUS_POINTS);
        gtk_widget_queue_draw(GTK_WIDGET(graph));
        selection_updated_cb(widget, data);
    }
    else
    points_dialog_closed_cb(widget, data);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
