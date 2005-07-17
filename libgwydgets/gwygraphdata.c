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
#include "gwygraphdata.h"
#include "gwygraphmodel.h"
#include "gwygraphcurvemodel.h"

enum {
    SELECTED_SIGNAL,
    LAST_SIGNAL
};

static void     clean_gtk_tree_view                  (GtkTreeView *widget);

static guint gwygraph_data_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(GwyGraphData, gwy_graph_data, GTK_TYPE_TREE_VIEW)

static void
gwy_graph_data_class_init(GwyGraphDataClass *klass)
{
    GtkTreeViewClass *widget_class;

    widget_class = (GtkTreeViewClass*)klass;

    klass->selected = NULL;
    gwygraph_data_signals[SELECTED_SIGNAL]
                = g_signal_new ("selected",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_STRUCT_OFFSET (GwyGraphDataClass, selected),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);
}


static void
gwy_graph_data_init(G_GNUC_UNUSED GwyGraphData *graph_data)
{
    gwy_debug("");
}


/**
 * gwy_graph_data_new:
 * @gmodel: A graph_data model.
 *
 * Creates graph_data widget based on information in model. 
 *
 * Returns: new graph_data widget.
 **/
GtkWidget*
gwy_graph_data_new(GwyGraphModel *gmodel)
{
    GtkWidget *graph_data = GTK_WIDGET(g_object_new(gwy_graph_data_get_type(), NULL));
    gwy_debug("");

    if (gmodel != NULL)
    {
       gwy_graph_data_change_model(GWY_GRAPH_DATA(graph_data), gmodel);    
    
       g_signal_connect_swapped(gmodel, "value-changed",
                     G_CALLBACK(gwy_graph_data_refresh), graph_data);

       gwy_graph_data_refresh(GWY_GRAPH_DATA(graph_data));
    }
    
    return graph_data;
}

static gint
get_max_n(GwyGraphModel *model)
{
    GwyGraphCurveModel *curvemodel;
    gint i;
    gint max = 0;
    
    for (i=0; i<model->ncurves; i++)
    {
        curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[i]);
        if (curvemodel->n > max) max = curvemodel->n;
    }
    return max;
}

static gchar *
get_cell_string(GwyGraphModel *model, gint curve_index, gint data_index, gint axis_index)
{
    GwyGraphCurveModel *curvemodel;

    if (curve_index >= model->ncurves) return g_strdup(" ");
    else
    {
        curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[curve_index]);
        if (data_index >= curvemodel->n) return g_strdup(" ");
        
        if (axis_index == 0) return g_strdup_printf("%g", curvemodel->xdata[data_index]);
        else return g_strdup_printf("%g", curvemodel->ydata[data_index]);
    }
}

/**
 * gwy_graph_data_refresh:
 * @graph_data: A graph_data widget.
 *
 * Refresh all the graph widgets according to the model.
 *
 **/
void       
gwy_graph_data_refresh(GwyGraphData *graph_data)
{
    enum { X1_COLUMN, Y1_COLUMN, 
        X2_COLUMN, Y2_COLUMN, 
        X3_COLUMN, Y3_COLUMN, 
        X4_COLUMN, Y4_COLUMN,
        X5_COLUMN, Y5_COLUMN,
        N_COLUMNS };
    GwyGraphModel *model;
    GwyGraphCurveModel *curvemodel;
    GtkTreeView *tview;
    GtkTreeIter iter;
    GtkTreeViewColumn *xcolumn, *ycolumn;
    GtkCellRenderer *renderer;
    GtkListStore *store;   
    gint i;
    GString *description;
   
    if (graph_data->graph_model == NULL) return;
    model = GWY_GRAPH_MODEL(graph_data->graph_model);
    tview = GTK_TREE_VIEW(graph_data);

 
    clean_gtk_tree_view(tview);
    gtk_tree_view_set_model(tview, NULL);

    store = gtk_list_store_new(10, G_TYPE_STRING, G_TYPE_STRING,
                               G_TYPE_STRING, G_TYPE_STRING,
                               G_TYPE_STRING, G_TYPE_STRING,
                               G_TYPE_STRING, G_TYPE_STRING,
                               G_TYPE_STRING, G_TYPE_STRING);
    
    for (i = 0; i<get_max_n(model); i++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                       X1_COLUMN, get_cell_string(model, 0, i, 0),
                       Y1_COLUMN, get_cell_string(model, 0, i, 1),
                       X2_COLUMN, get_cell_string(model, 1, i, 0),
                       Y2_COLUMN, get_cell_string(model, 1, i, 1),
                       X3_COLUMN, get_cell_string(model, 2, i, 0),
                       Y3_COLUMN, get_cell_string(model, 2, i, 1),
                       X4_COLUMN, get_cell_string(model, 3, i, 0),
                       Y4_COLUMN, get_cell_string(model, 3, i, 1),
                       X5_COLUMN, get_cell_string(model, 4, i, 0),
                       Y5_COLUMN, get_cell_string(model, 4, i, 1),
                       -1);
 
    }
     
    gtk_tree_view_set_model(tview, GTK_TREE_MODEL(store));
  
    /*make two columns for each curve*/
    for (i=0; i<model->ncurves; i++)
    {
        curvemodel = GWY_GRAPH_CURVE_MODEL(model->curves[i]);
     
        description = g_string_new(curvemodel->description->str);
        description = g_string_prepend(description, "x (");
        description = g_string_append(description, ")");
        renderer = gtk_cell_renderer_text_new();
        xcolumn = gtk_tree_view_column_new_with_attributes (description->str,
                                                       renderer,
                                                       "text", 2*i,
                                                       NULL);
        g_string_free(description, TRUE);

        description = g_string_new(curvemodel->description->str);
        description = g_string_prepend(description, "y (");
        description = g_string_append(description, ")");
        renderer = gtk_cell_renderer_text_new();
        ycolumn = gtk_tree_view_column_new_with_attributes (description->str,
                                                       renderer,
                                                       "text", 2*i+1,
                                                       NULL);
        gtk_tree_view_append_column(tview, xcolumn);
        gtk_tree_view_append_column(tview, ycolumn);
        g_string_free(description, TRUE);
    }
}


/* Remove all columns from a GtkTreeView */
/* XXX: use gtk_list_store_clear() */
static void
clean_gtk_tree_view (GtkTreeView *widget)
{
    GList *columns, *list;
    gint n, i;
    GtkTreeViewColumn *column;
                    
    columns = gtk_tree_view_get_columns (widget);
                        
    list = columns;
                            
    n = g_list_length (columns);
                                
    for (i = n; i > 0; i--) {
       column = gtk_tree_view_get_column (widget, i-1);
       gtk_tree_view_remove_column (widget, column);
    }

    g_list_free (columns);
}

/**
 * gwy_graph_data_change_model:
 * @graph_data: A graph_data widget.
 * @gmodel: new graph_data model 
 *
 * Changes the graph_data model. Everything in graph_data widgets will
 * be reset to the new data (from the model).
 *
 **/
void
gwy_graph_data_change_model(GwyGraphData *graph_data, GwyGraphModel *gmodel)
{
    graph_data->graph_model = gmodel;

    g_signal_connect_swapped(gmodel, "notify", G_CALLBACK(gwy_graph_data_refresh), graph_data);
}


/**
 * gwy_graph_data_get_model:
 * @graph_data: A graph_data widget.
 *
 * Returns: GraphModel associated with this graph_data widget.
 **/
GwyGraphModel *gwy_graph_data_get_model(GwyGraphData *graph_data)
{
    return  graph_data->graph_model;
}


void       
gwy_graph_data_signal_selected(GwyGraphData *graph_data)
{
    g_signal_emit (G_OBJECT (graph_data), gwygraph_data_signals[SELECTED_SIGNAL], 0);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
