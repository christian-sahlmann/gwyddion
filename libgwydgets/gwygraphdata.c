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
#include <string.h>
#include <gtk/gtk.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraphdata.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwydgetutils.h>

enum {
    /* The maximum value width */
    COL_WIDTH = sizeof("-0.12345e+308")
};

typedef struct {
    GwyGraphCurveModel *gcmodel;
    gulong changed_id;
} GwyGraphDataCurve;

static void gwy_graph_data_destroy       (GtkObject *object);
static void gwy_graph_data_finalize      (GObject *object);
static void gwy_graph_data_update_headers(GwyGraphData *graph_data);
static void gwy_graph_data_update_nrows  (GwyGraphData *graph_data);
static void gwy_graph_data_update_ncurves(GwyGraphData *graph_data);
static void gwy_graph_data_model_notify  (GwyGraphData *graph_data,
                                          const GParamSpec *pspec);

static GQuark quark_id = 0;

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

    if (!quark_id)
        quark_id = g_quark_from_static_string("gwy-graph-data-column-id");

    graph_data->store = gwy_null_store_new(0);
    graph_data->curves = g_array_new(FALSE, FALSE, sizeof(GwyGraphDataCurve));

    treeview = GTK_TREE_VIEW(graph_data);
    gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(graph_data->store));
    gtk_tree_view_set_fixed_height_mode(treeview, TRUE);
}

static void
gwy_graph_data_finalize(GObject *object)
{
    GwyGraphData *graph_data;

    graph_data = GWY_GRAPH_DATA(object);

    g_array_free(graph_data->curves, TRUE);
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
                                  graph_data->notify_id);
    gwy_object_unref(graph_data->graph_model);
    gwy_debug("setting model to: %p", gmodel);
    graph_data->graph_model = gmodel;
    if (gmodel) {
        g_object_ref(gmodel);
        graph_data->notify_id
            = g_signal_connect_swapped(gmodel, "notify",
                                       G_CALLBACK(gwy_graph_data_model_notify),
                                       graph_data);
    }
    gwy_graph_data_update_ncurves(graph_data);
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
gwy_graph_data_model_notify(GwyGraphData *graph_data,
                            const GParamSpec *pspec)
{
    gwy_debug("notify::%s from model", pspec->name);

    if (gwy_strequal(pspec->name, "n-curves")) {
        gwy_graph_data_update_ncurves(graph_data);
        return;
    }

    if (gwy_strequal(pspec->name, "axis-label-bottom")
        || gwy_strequal(pspec->name, "axis-label-left")) {
        gwy_graph_data_update_headers(graph_data);
        return;
    }
}

static void
render_data(GtkTreeViewColumn *column,
            GtkCellRenderer *renderer,
            GtkTreeModel *model,
            GtkTreeIter *iter,
            gpointer data)
{
    GwyGraphData *graph_data = GWY_GRAPH_DATA(data);
    const GwyGraphDataCurve *curve;
    const gdouble *d;
    gchar buffer[32];
    gint id, row;

    id = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(column), quark_id));
    /* Be fault-tolerant */
    if (!graph_data->graph_model || id >= graph_data->curves->len)
        return;

    curve = &g_array_index(graph_data->curves, GwyGraphDataCurve, id);
    gtk_tree_model_get(model, iter, 0, &row, -1);
    if (row >= gwy_graph_curve_model_get_ndata(curve->gcmodel)) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }

    if (g_object_get_qdata(G_OBJECT(renderer), quark_id))
        d = gwy_graph_curve_model_get_ydata(curve->gcmodel);
    else
        d = gwy_graph_curve_model_get_xdata(curve->gcmodel);

    /* TODO: improve formatting using some value format (may be hard to do
     * for all value range) */
    g_snprintf(buffer, sizeof(buffer), "%g", d[row]);
    g_object_set(renderer, "text", buffer, NULL);
}

static inline void
gwy_graph_data_pack_renderer(GwyGraphData *graph_data,
                             GtkTreeViewColumn *column,
                             gint id)
{
    GtkCellRenderer *renderer;

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(renderer, COL_WIDTH, -1);
    gtk_cell_renderer_text_set_fixed_height_from_font
                                        (GTK_CELL_RENDERER_TEXT(renderer), 1);
    g_object_set(renderer, "xalign", 1.0, NULL);
    if (id)
        g_object_set_qdata(G_OBJECT(renderer), quark_id, GINT_TO_POINTER(id));

    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(column, renderer,
                                            render_data, graph_data, NULL);
}

static void
gwy_graph_data_update_ncurves(GwyGraphData *graph_data)
{
    GwyGraphDataCurve *curve;
    GtkTreeView *treeview;
    GtkTreeViewColumn *column;
    GtkWidget *table, *label;
    guint i, ncolumns, ncurves = 0;

    ncolumns = graph_data->curves->len;
    gwy_debug("old ncurves: %d", ncolumns);

    /* Reconnect all signals just to be sure.
     * GraphModel is a bit cagey when changes in its curves are regarded */
    for (i = 0; i < graph_data->curves->len; i++) {
        curve = &g_array_index(graph_data->curves, GwyGraphDataCurve, i);
        gwy_signal_handler_disconnect(curve->gcmodel, curve->changed_id);
        gwy_object_unref(curve->gcmodel);
    }
    g_array_set_size(graph_data->curves, 0);

    if (graph_data->graph_model) {
        GwyGraphDataCurve newcurve;

        ncurves = gwy_graph_model_get_n_curves(graph_data->graph_model);
        for (i = 0; i < ncurves; i++) {
            newcurve.gcmodel
                = gwy_graph_model_get_curve(graph_data->graph_model, i);
            g_object_ref(newcurve.gcmodel);
            newcurve.changed_id = g_signal_connect_swapped
                                      (newcurve.gcmodel, "data-changed",
                                       G_CALLBACK(gwy_graph_data_update_nrows),
                                       graph_data);
            g_array_append_val(graph_data->curves, newcurve);
        }
    }
    gwy_debug("ncurves: %d", ncurves);

    /* Update the number of columns. */
    treeview = GTK_TREE_VIEW(graph_data);

    while (ncolumns > ncurves) {
        ncolumns--;
        gwy_debug("removing column %d", ncolumns);
        column = gtk_tree_view_get_column(treeview, ncolumns);
        gtk_tree_view_remove_column(treeview, column);
    }

    while (ncolumns < ncurves) {
        GtkRequisition req;

        gwy_debug("adding column %d", ncolumns);
        column = gtk_tree_view_column_new();
        g_object_set_qdata(G_OBJECT(column), quark_id,
                           GINT_TO_POINTER(ncolumns));

        gwy_graph_data_pack_renderer(graph_data, column, 0);
        gwy_graph_data_pack_renderer(graph_data, column, 1);

        table = gtk_table_new(2, 2, TRUE);
        label = gtk_label_new(NULL);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 2, 0, 1);
        label = gtk_label_new(NULL);
        gtk_label_set_width_chars(GTK_LABEL(label), COL_WIDTH);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
        label = gtk_label_new(NULL);
        gtk_label_set_width_chars(GTK_LABEL(label), COL_WIDTH);
        gtk_table_attach_defaults(GTK_TABLE(table), label, 1, 2, 1, 2);
        gtk_widget_show_all(table);
        gtk_tree_view_column_set_widget(column, table);

        gtk_widget_size_request(table, &req);

        g_object_set(column,
                     "sizing", GTK_TREE_VIEW_COLUMN_FIXED,
                     "fixed-width", req.width,
                     NULL);
        gtk_tree_view_append_column(treeview, column);
        ncolumns++;
    }

    if (graph_data->graph_model)
        gwy_graph_data_update_headers(graph_data);

    if (graph_data->store)
        gwy_graph_data_update_nrows(graph_data);
}

static void
gwy_graph_data_update_headers(GwyGraphData *graph_data)
{
    const GwyGraphDataCurve *curve;
    const gchar *s, *xlabel, *ylabel;
    GtkTreeView *treeview;
    GtkTreeViewColumn *column;
    GtkWidget *table, *label;
    GwySIUnit *siunit;
    gchar *sx, *sy;
    GString *str;
    guint i;

    treeview = GTK_TREE_VIEW(graph_data);
    xlabel = gwy_graph_model_get_axis_label(graph_data->graph_model,
                                            GTK_POS_BOTTOM);
    ylabel = gwy_graph_model_get_axis_label(graph_data->graph_model,
                                            GTK_POS_LEFT);

    siunit = gwy_graph_model_get_si_unit_x(graph_data->graph_model);
    sx = gwy_si_unit_get_string(siunit, GWY_SI_UNIT_FORMAT_MARKUP);
    siunit = gwy_graph_model_get_si_unit_y(graph_data->graph_model);
    sy = gwy_si_unit_get_string(siunit, GWY_SI_UNIT_FORMAT_MARKUP);

    str = g_string_new("");
    for (i = 0; i < graph_data->curves->len; i++) {
        curve = &g_array_index(graph_data->curves, GwyGraphDataCurve, i);
        column = gtk_tree_view_get_column(treeview, i);
        table = gtk_tree_view_column_get_widget(column);

        label = gwy_table_get_child_widget(table, 0, 0);
        s = gwy_graph_curve_model_get_description(curve->gcmodel);
        gtk_label_set_markup(GTK_LABEL(label), s);

        label = gwy_table_get_child_widget(table, 1, 0);
        g_string_assign(str, xlabel);
        if (sx && *sx) {
            g_string_append(str, " [");
            g_string_append(str, sx);
            g_string_append(str, "]");
        }
        gtk_label_set_markup(GTK_LABEL(label), str->str);

        label = gwy_table_get_child_widget(table, 1, 1);
        g_string_assign(str, ylabel);
        if (sy && *sy) {
            g_string_append(str, " [");
            g_string_append(str, sy);
            g_string_append(str, "]");
        }
        gtk_label_set_markup(GTK_LABEL(label), str->str);
    }

    g_string_free(str, TRUE);
    g_free(sx);
    g_free(sy);
}

static void
gwy_graph_data_update_nrows(GwyGraphData *graph_data)
{
    const GwyGraphDataCurve *curve;
    guint i, n, max;

    max = 0;
    for (i = 0; i < graph_data->curves->len; i++) {
        curve = &g_array_index(graph_data->curves, GwyGraphDataCurve, i);
        n = gwy_graph_curve_model_get_ndata(curve->gcmodel);
        if (n > max)
            max = n;
    }

    gwy_debug("nrows: %d", max);
    gwy_null_store_set_n_rows(graph_data->store, max);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphdata
 * @title: GwyGraphData
 * @short_description: Graph data table
 *
 * #GwyGraphData displays data values from #GwyGraphModel curves in a table.
 * While it is a #GtkTreeView, it uses a dummy tree model (#GwyNullStore)
 * and its content is determined by the graph model.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
