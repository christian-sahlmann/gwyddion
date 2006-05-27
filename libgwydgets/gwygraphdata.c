/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraphdata.h>
#include <libgwydgets/gwygraphmodel.h>

static void gwy_graph_data_destroy     (GtkObject *object);
static void gwy_graph_data_finalize    (GObject *object);
static void gwy_graph_data_update_nrows(GwyGraphData *graph_data);
static void gwy_graph_data_ncurves_cb  (GwyGraphData *graph_data);

static GQuark id_quark = 0;

G_DEFINE_TYPE(GwyGraphData, gwy_graph_data, GTK_TYPE_TREE_VIEW)

static void
gwy_graph_data_class_init(GwyGraphDataClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_graph_data_finalize;

    object_class->destroy = gwy_graph_data_destroy;
}

static void
gwy_graph_data_init(GwyGraphData *graph_data)
{
    GtkTreeView *treeview;

    if (!id_quark)
        id_quark = g_quark_from_static_string("gwy-graph-data-column-id");

    graph_data->store = gwy_null_store_new(0);
    graph_data->curves = g_ptr_array_new();

    treeview = GTK_TREE_VIEW(graph_data);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(graph_data->store));
    /*gtk_tree_view_set_fixed_height_mode(treeview, TRUE);*/
}

static void
gwy_graph_data_finalize(GObject *object)
{
    GwyGraphData *graph_data;

    graph_data = GWY_GRAPH_DATA(object);

    g_ptr_array_free(graph_data->curves, TRUE);
}

static void
gwy_graph_data_destroy(GtkObject *object)
{
    GwyGraphData *graph_data;

    graph_data = GWY_GRAPH_DATA(object);

    gwy_graph_data_set_model(graph_data, NULL);
    gwy_object_unref(graph_data->store);
}

/**
 * gwy_graph_data_new:
 * @gmodel: A graph_data model.  It can be %NULL.
 *
 * Creates graph_data widget based on information in graph model.
 *
 * The #GtkTreeModel and the columns follow the graph model and must not be
 * changed manually.
 *
 * Returns: A new graph_data widget.
 **/
GtkWidget*
gwy_graph_data_new(GwyGraphModel *gmodel)
{
    GwyGraphData *graph_data;

    graph_data = g_object_new(gwy_graph_data_get_type(), NULL);
    g_return_val_if_fail(!gmodel || GWY_IS_GRAPH_MODEL(gmodel),
                         (GtkWidget*)graph_data);

    if (gmodel)
       gwy_graph_data_set_model(graph_data, gmodel);

    return (GtkWidget*)graph_data;
}

/**
 * gwy_graph_data_set_model:
 * @graph_data: A graph data widget.
 * @gmodel: New graph_data model.
 *
 * Changes the graph model a graph data table displays.
 **/
void
gwy_graph_data_set_model(GwyGraphData *graph_data,
                         GwyGraphModel *gmodel)
{
    g_return_if_fail(GWY_IS_GRAPH_DATA(graph_data));
    g_return_if_fail(!gmodel || GWY_IS_GRAPH_MODEL(gmodel));

    gwy_signal_handler_disconnect(graph_data->graph_model,
                                  graph_data->ncurves_id);
    gwy_object_unref(graph_data->graph_model);
    gwy_debug("setting model to: %p", gmodel);
    graph_data->graph_model = gmodel;
    if (gmodel) {
        g_object_ref(gmodel);
        graph_data->ncurves_id
            = g_signal_connect_swapped(gmodel, "notify::n-curves",
                                       G_CALLBACK(gwy_graph_data_ncurves_cb),
                                       graph_data);
    }
    gwy_graph_data_ncurves_cb(graph_data);
}

/**
 * gwy_graph_data_get_model:
 * @graph_data: A graph_data widget.
 *
 * Gets the graph model a graph data table displays.
 *
 * Returns: The graph model associated with this #GwyGraphData widget.
 **/
GwyGraphModel*
gwy_graph_data_get_model(GwyGraphData *graph_data)
{
    g_return_val_if_fail(GWY_IS_GRAPH_DATA(graph_data), NULL);

    return graph_data->graph_model;
}

static void
render_data(GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            gpointer data)
{
    GwyGraphData *graph_data = GWY_GRAPH_DATA(data);
    const gdouble *d;
    GwyGraphCurveModel *gcmodel;
    gchar buffer[32];
    gint id, row;

    id = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(column), id_quark));
    /* Be very fault-tolerant */
    if (!graph_data->graph_model
        || id/2 >= gwy_graph_model_get_n_curves(graph_data->graph_model))
        return;

    gcmodel = gwy_graph_model_get_curve(graph_data->graph_model, id/2);
    gtk_tree_model_get(model, iter, 0, &row, -1);
    if (row >= gwy_graph_curve_model_get_ndata(gcmodel)) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }

    if (id & 1)
        d = gwy_graph_curve_model_get_ydata(gcmodel);
    else
        d = gwy_graph_curve_model_get_xdata(gcmodel);

    /* TODO: improve formatting */
    g_snprintf(buffer, sizeof(buffer), "%g", d[row]);
    g_object_set(renderer, "text", buffer, NULL);
}

static void
gwy_graph_data_ncurves_cb(GwyGraphData *graph_data)
{
    GwyGraphCurveModel *gcmodel;
    GtkTreeView *treeview;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GString *str;
    gint i, ncurves = 0;

    gwy_debug("old ncurves: %d", (gint)graph_data->curves->len);

    for (i = 0; i < (gint)graph_data->curves->len; i++) {
        gcmodel = g_ptr_array_index(graph_data->curves, i);
        g_signal_handlers_disconnect_by_func(gcmodel,
                                             gwy_graph_data_update_nrows,
                                             graph_data);
        g_object_unref(gcmodel);
    }
    g_ptr_array_set_size(graph_data->curves, 0);

    if (graph_data->graph_model) {
        ncurves = gwy_graph_model_get_n_curves(graph_data->graph_model);
        for (i = 0; i < ncurves; i++) {
            gcmodel = gwy_graph_model_get_curve(graph_data->graph_model, i);
            g_object_ref(gcmodel);
            g_ptr_array_add(graph_data->curves, gcmodel);
            g_signal_connect_swapped(gcmodel, "data-changed",
                                     G_CALLBACK(gwy_graph_data_update_nrows),
                                     graph_data);
        }
    }
    gwy_debug("ncurves: %d", ncurves);

    treeview = GTK_TREE_VIEW(graph_data);

    while (graph_data->ncolumns/2 > ncurves) {
        gwy_debug("removing column %d", graph_data->ncolumns-1);
        column = gtk_tree_view_get_column(treeview, graph_data->ncolumns-1);
        gtk_tree_view_remove_column(treeview, column);
        graph_data->ncolumns--;

        gwy_debug("removing column %d", graph_data->ncolumns-1);
        column = gtk_tree_view_get_column(treeview, graph_data->ncolumns-1);
        gtk_tree_view_remove_column(treeview, column);
        graph_data->ncolumns--;
    }

    while (graph_data->ncolumns/2 < ncurves) {
        gwy_debug("adding column %d", graph_data->ncolumns);
        renderer = gtk_cell_renderer_text_new();
        /* gtk_cell_renderer_text_set_fixed_height_from_font
                                       (GTK_CELL_RENDERER_TEXT(renderer), 1);*/
        g_object_set(renderer, "xalign", 1.0, NULL);
        column = gtk_tree_view_column_new();
        /*g_object_set(column, "sizing", GTK_TREE_VIEW_COLUMN_FIXED, NULL);*/
        g_object_set_qdata(G_OBJECT(column), id_quark,
                           GINT_TO_POINTER(graph_data->ncolumns));
        gtk_tree_view_column_pack_start(column, renderer, FALSE);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                                                render_data, graph_data, NULL);
        gtk_tree_view_append_column(treeview, column);
        graph_data->ncolumns++;
    }

    /* TODO: set title from curve description, maybe units */
    str = g_string_new("");
    for (i = 0; i < graph_data->ncolumns; i++) {
        g_string_printf(str, "%c%d", (i & 1) ? 'y' : 'x', i/2 + 1);
        column = gtk_tree_view_get_column(treeview, i);
        gtk_tree_view_column_set_title(column, str->str);
    }
    g_string_free(str, TRUE);

    if (graph_data->store)
        gwy_graph_data_update_nrows(graph_data);
}

static void
gwy_graph_data_update_nrows(GwyGraphData *graph_data)
{
    GwyGraphCurveModel *gcmodel;
    gint i, nc, n;
    gint max = 0;

    if (graph_data->graph_model) {
        nc = gwy_graph_model_get_n_curves(graph_data->graph_model);
        for (i = 0; i < nc; i++) {
            gcmodel = gwy_graph_model_get_curve(graph_data->graph_model, i);
            n = gwy_graph_curve_model_get_ndata(gcmodel);
            if (n > max)
                max = n;
        }
    }

    gwy_debug("nrows: %d", max);
    gwy_null_store_set_n_rows(graph_data->store, max);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphdata
 * @title: GwyGraphData
 * @short_description: Graph data table
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
