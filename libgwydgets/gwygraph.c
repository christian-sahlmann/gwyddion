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

static void gwy_graph_finalize    (GObject *object);
static void gwy_graph_refresh     (GwyGraph *graph);
static void set_graph_model_ranges(GwyGraph *graph);
static void rescaled_cb           (GtkWidget *widget,
                                   GwyGraph *graph);
static void zoomed_cb             (GwyGraph *graph);
static void label_updated_cb      (GwyAxis *axis,
                                   GwyGraph *graph);

G_DEFINE_TYPE(GwyGraph, gwy_graph, GTK_TYPE_TABLE)

static void
gwy_graph_class_init(GwyGraphClass *klass)
{
    GtkWidgetClass *widget_class;
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    widget_class = (GtkWidgetClass*)klass;

    gobject_class->finalize = gwy_graph_finalize;

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

    gwy_signal_handler_disconnect(graph->graph_model, graph->notify_id);
    gwy_object_unref(graph->graph_model);
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
    GwySelection *selection;
    GwyGraph *graph;
    const gchar *label;
    guint i;

    gwy_debug("");

    graph = GWY_GRAPH(g_object_new(GWY_TYPE_GRAPH, NULL));

    graph->area = GWY_GRAPH_AREA(gwy_graph_area_new(NULL, NULL));
    graph->area->status = GWY_GRAPH_STATUS_PLAIN;
    graph->enable_user_input = TRUE;

    if (gmodel)
        gwy_graph_set_model(GWY_GRAPH(graph), gmodel);

    gtk_table_resize(GTK_TABLE(graph), 3, 3);
    gtk_table_set_homogeneous(GTK_TABLE(graph), FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(graph), 0);
    gtk_table_set_col_spacings(GTK_TABLE(graph), 0);

    graph->grid_type = GWY_GRAPH_GRID_AUTO;

    for (i = GTK_POS_LEFT; i <= GTK_POS_BOTTOM; i++) {
        graph->axis[i] = GWY_AXIS(gwy_axis_new(i));
        if (gmodel) {
            label = gwy_graph_model_get_axis_label(gmodel, i);
            gwy_axis_set_label(graph->axis[i], label);
        }
    }

    gwy_graph_set_axis_visible(graph, GTK_POS_RIGHT, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_TOP, FALSE);

    /* XXX: Is there any reason why we never connect to "rescaled" of top and
     * right axes? */
    graph->rescaled_id[GTK_POS_LEFT]
        = g_signal_connect(graph->axis[GTK_POS_LEFT], "rescaled",
                           G_CALLBACK(rescaled_cb), graph);
    graph->rescaled_id[GTK_POS_BOTTOM]
        = g_signal_connect(graph->axis[GTK_POS_BOTTOM], "rescaled",
                           G_CALLBACK(rescaled_cb), graph);

    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis[GTK_POS_LEFT]),
                     0, 1, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis[GTK_POS_RIGHT]),
                     2, 3, 1, 2,
                     GTK_FILL, GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis[GTK_POS_TOP]),
                     1, 2, 0, 1,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);
    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->axis[GTK_POS_BOTTOM]),
                     1, 2, 2, 3,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK, GTK_FILL, 0, 0);

    for (i = GTK_POS_LEFT; i <= GTK_POS_BOTTOM; i++) {
        g_signal_connect(graph->axis[i], "label-updated",
                         G_CALLBACK(label_updated_cb), graph);
        gtk_widget_show(GTK_WIDGET(graph->axis[i]));
    }

    for (i = 0; i < 4; i++) {
        graph->corner[i] = GWY_GRAPH_CORNER(gwy_graph_corner_new());
        gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner[i]),
                         2*(i/2), 2*(i/2) + 1, 2*(i%2), 2*(i%2) + 1,
                         GTK_FILL, GTK_FILL, 0, 0);
        gtk_widget_show(GTK_WIDGET(graph->corner[i]));
    }

    selection = gwy_graph_area_get_selection(graph->area,
                                             GWY_GRAPH_STATUS_ZOOM);
    g_signal_connect_swapped(selection, "finished",
                             G_CALLBACK(zoomed_cb), graph);

    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     0, 0);

    gtk_widget_show_all(GTK_WIDGET(graph->area));

    gwy_graph_refresh(graph);

    return GTK_WIDGET(graph);
}


static void
gwy_graph_refresh(GwyGraph *graph)
{
    GwyGraphModel *model;
    GwyGraphCurveModel *curvemodel;
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    gint i, j, nc, ndata;
    const gdouble *xdata, *ydata;
    gboolean has_data, lg;
    GwySIUnit *siunit;

    if (!graph->graph_model)
        return;

    model = GWY_GRAPH_MODEL(graph->graph_model);

    g_object_get(model, "si-unit-x", &siunit, "x-logarithmic", &lg, NULL);
    for (i = GTK_POS_TOP; i <= GTK_POS_BOTTOM; i++)  {
        gwy_axis_set_logarithmic(graph->axis[i], lg);
        gwy_axis_set_unit(graph->axis[i], siunit);
    }
    g_object_unref(siunit);

    g_object_get(model, "si-unit-y", &siunit, "y-logarithmic", &lg, NULL);
    for (i = GTK_POS_LEFT; i <= GTK_POS_RIGHT; i++)  {
        gwy_axis_set_logarithmic(graph->axis[i], lg);
        gwy_axis_set_unit(graph->axis[i], siunit);
    }
    g_object_unref(siunit);

    nc = gwy_graph_model_get_n_curves(model);
    if (nc > 0) {
        /*refresh axis and reset axis requirements*/
        x_reqmin = y_reqmin = G_MAXDOUBLE;
        x_reqmax = y_reqmax = -G_MAXDOUBLE;
        has_data = FALSE;
        for (i = 0; i < nc; i++) {
            curvemodel = gwy_graph_model_get_curve(model, i);
            ndata = gwy_graph_curve_model_get_ndata(curvemodel);
            xdata = gwy_graph_curve_model_get_xdata(curvemodel);
            ydata = gwy_graph_curve_model_get_ydata(curvemodel);
            for (j = 0; j < ndata; j++) {
                if (x_reqmin > xdata[j])
                    x_reqmin = xdata[j];
                if (y_reqmin > ydata[j])
                    y_reqmin = ydata[j];
                if (x_reqmax < xdata[j])
                    x_reqmax = xdata[j];
                if (y_reqmax < ydata[j])
                    y_reqmax = ydata[j];
                has_data = TRUE;
            }
        }
        if (!has_data) {
            x_reqmin = y_reqmin = 0;
            x_reqmax = y_reqmax = 1;
        }

        for (i = GTK_POS_LEFT; i <= GTK_POS_RIGHT; i++)
            gwy_axis_set_req(graph->axis[i], y_reqmin, y_reqmax);
        for (i = GTK_POS_TOP; i <= GTK_POS_BOTTOM; i++)
            gwy_axis_set_req(graph->axis[i], x_reqmin, x_reqmax);

    }
    else {
        for (i = GTK_POS_LEFT; i <= GTK_POS_BOTTOM; i++)
            gwy_axis_set_req(graph->axis[i], 0.0, 1.0);
    }

    set_graph_model_ranges(graph);

    /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
}

/**
 * gwy_graph_set_model:
 * @graph: A graph widget.
 * @gmodel: new graph model
 *
 * Changes the graph model.
 *
 * Everything in graph widgets will be reset to reflect the new data.
 * @gmodel is duplicated.
 **/
void
gwy_graph_set_model(GwyGraph *graph, GwyGraphModel *gmodel)
{
    gwy_signal_handler_disconnect(graph->graph_model, graph->notify_id);

    if (gmodel)
        g_object_ref(gmodel);
    gwy_object_unref(graph->graph_model);
    graph->graph_model = gmodel;

    if (gmodel) {
        graph->notify_id
            = g_signal_connect_swapped(gmodel, "notify",
                                       G_CALLBACK(gwy_graph_refresh), graph);
    }

    gwy_graph_area_set_model(graph->area, gmodel);
}

static void
rescaled_cb(G_GNUC_UNUSED GtkWidget *widget, GwyGraph *graph)
{
    GArray *array;

    if (graph->graph_model == NULL)
        return;

    array = g_array_new(FALSE, FALSE, sizeof(gdouble));
    set_graph_model_ranges(graph);

    if (graph->grid_type == GWY_GRAPH_GRID_AUTO) {
        gwy_axis_set_grid_data(graph->axis[GTK_POS_LEFT], array);
        gwy_graph_area_set_x_grid_data(graph->area, array);
        gwy_axis_set_grid_data(graph->axis[GTK_POS_BOTTOM], array);
        gwy_graph_area_set_y_grid_data(graph->area, array);

        g_array_free(array, TRUE);
    }

    gwy_graph_area_refresh(graph->area);
}

/**
 * gwy_graph_get_model:
 * @graph: A graph widget.
 *
 * Returns: Graph model associated with this graph widget (do not free).
 **/
/* XXX: Malformed documentation. */
GwyGraphModel*
gwy_graph_get_model(GwyGraph *graph)
{
    return  graph->graph_model;
}

/**
 * gwy_graph_get_axis:
 * @graph: A graph widget.
 * @type: Axis orientation
 *
 * Returns: the #GwyAxis (of given orientation) within @graph (do not free).
 **/
/* XXX: Malformed documentation. */
GwyAxis*
gwy_graph_get_axis(GwyGraph *graph, GtkPositionType type)
{
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);
    g_return_val_if_fail(type <= GTK_POS_BOTTOM, NULL);

    return graph->axis[type];
}

/**
 * gwy_graph_set_axis_visible:
 * @graph: A graph widget.
 * @type: Axis orientation
 * @is_visible: set/unset axis visibility within graph widget
 *
 * Sets the visibility of graph axis of given orientation. Visibility
 * can be set also directly using GwyAxis API.
 **/
void
gwy_graph_set_axis_visible(GwyGraph *graph,
                           GtkPositionType type,
                           gboolean is_visible)
{
    g_return_if_fail(GWY_IS_GRAPH(graph));
    g_return_if_fail(type <= GTK_POS_BOTTOM);

    gwy_axis_set_visible(graph->axis[type], is_visible);
}

/**
 * gwy_graph_get_area:
 * @graph: A graph widget.
 *
 * Returns: the #GwyGraphArea within @graph (do not free).
 **/
/* XXX: Malformed documentation. */
GtkWidget*
gwy_graph_get_area(GwyGraph *graph)
{
    return GTK_WIDGET(graph->area);
}

/**
 * gwy_graph_set_status:
 * @graph: A graph widget.
 * @status: graph status
 *
 * Set status of the graph widget. Status determines how the graph
 * reacts on mouse events. This includes point or area selection and zooming.
 *
 **/
/* XXX: Malformed documentation. */
void
gwy_graph_set_status(GwyGraph *graph, GwyGraphStatusType status)
{
    gwy_graph_area_set_status(GWY_GRAPH_AREA(graph->area), status);
}

/**
 * gwy_graph_get_status:
 * @graph: A graph widget.
 *
 * Get status of the graph widget. Status determines how the graph
 * reacts on mouse events. This includes point or area selection and zooming.
 *
 * Returns: graph status
 **/
GwyGraphStatusType
gwy_graph_get_status(GwyGraph *graph)
{
    return graph->area->status;
}

/**
 * gwy_graph_request_x_range:
 * @graph: A graph widget.
 * @x_min_req: x minimum request
 * @x_max_req: x maximum request
 *
 * Ask graph to set the axis and area ranges to the requested values.
 * Note that the axis scales must have reasonably aligned ticks, therefore
 * the result might not exactly match the requested values.
 * Use gwy_graph_get_x_range() if you want to know the result.
 **/
/* XXX: Malformed documentation. */
/* XXX: Remove? Or at least move to GwyGraphModel, it does not belong here. */
void
gwy_graph_request_x_range(GwyGraph *graph,
                          gdouble x_min_req,
                          gdouble x_max_req)
{
    if (!graph->graph_model)
        return;

    g_object_set(graph->graph_model,
                 "x-min", x_min_req, "x-min-set", TRUE,
                 "x-max", x_max_req, "x-max-set", TRUE,
                 NULL);

#if 0
    gwy_axis_set_req(graph->axis[GTK_POS_TOP], x_min_req, x_max_req);
    gwy_axis_set_req(graph->axis[GTK_POS_BOTTOM], x_min_req, x_max_req);

    gwy_graph_model_set_xmax(model,
                             gwy_axis_get_maximum(graph->axis[GTK_POS_BOTTOM]));
    gwy_graph_model_set_xmin(model,
                             gwy_axis_get_minimum(graph->axis[GTK_POS_BOTTOM]));

    /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
#endif
}

/**
 * gwy_graph_request_y_range:
 * @graph: A graph widget.
 * @y_min_req: y minimum request
 * @y_max_req: y maximum request
 *
 * Ask graph to set the axis and area ranges to the requested values.
 * Note that the axis scales must have reasonably aligned ticks, therefore
 * the result might not exactly match the requested values.
 * Use gwy_graph_get_y_range() if you want to know the result.
 **/
/* XXX: Malformed documentation. */
/* XXX: Remove? Or at least move to GwyGraphModel, it does not belong here. */
void
gwy_graph_request_y_range(GwyGraph *graph,
                          gdouble y_min_req,
                          gdouble y_max_req)
{
    if (!graph->graph_model)
        return;

    g_object_set(graph->graph_model,
                 "y-min", y_min_req, "y-min-set", TRUE,
                 "y-max", y_max_req, "y-max-set", TRUE,
                 NULL);

#if 0
    gwy_axis_set_req(graph->axis[GTK_POS_LEFT], y_min_req, y_max_req);
    gwy_axis_set_req(graph->axis[GTK_POS_RIGHT], y_min_req, y_max_req);

    gwy_graph_model_set_ymax(model,
                             gwy_axis_get_maximum(graph->axis[GTK_POS_LEFT]));
    gwy_graph_model_set_ymin(model,
                             gwy_axis_get_minimum(graph->axis[GTK_POS_LEFT]));

     /*refresh widgets*/
    gwy_graph_area_refresh(graph->area);
#endif
}

/**
 * gwy_graph_get_x_range:
 * @graph: A graph widget.
 * @x_min: Location to store x minimum.
 * @x_max: Location to store x maximum.
 *
 * Gets the actual boudaries of graph area and axis in the x direction.
 **/
void
gwy_graph_get_x_range(GwyGraph *graph,
                      gdouble *x_min,
                      gdouble *x_max)
{
    *x_min = gwy_axis_get_minimum(graph->axis[GTK_POS_BOTTOM]);
    *x_max = gwy_axis_get_maximum(graph->axis[GTK_POS_BOTTOM]);
}

/**
 * gwy_graph_get_y_range:
 * @graph: A graph widget.
 * @y_min: Location to store y minimum.
 * @y_max: Location to store y maximum.
 *
 * Gets the actual boudaries of graph area and axis in the y direction.
 **/
void
gwy_graph_get_y_range(GwyGraph *graph,
                      gdouble *y_min,
                      gdouble *y_max)
{
    *y_min = gwy_axis_get_minimum(graph->axis[GTK_POS_LEFT]);
    *y_max = gwy_axis_get_maximum(graph->axis[GTK_POS_LEFT]);
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
    guint i;

    graph->enable_user_input = enable;
    gwy_graph_area_enable_user_input(graph->area, enable);

    for (i = GTK_POS_LEFT; i <= GTK_POS_BOTTOM; i++)
        gwy_axis_enable_label_edit(graph->axis[i], enable);
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
    gdouble selection_zoomdata[4];
    guint i;

    if (graph->area->status != GWY_GRAPH_STATUS_ZOOM ||
        gwy_selection_get_data(gwy_graph_area_get_selection(GWY_GRAPH_AREA(graph->area), GWY_GRAPH_STATUS_ZOOM), NULL) != 1)
        return;

    gwy_selection_get_object(GWY_SELECTION((graph->area)->zoomdata),
                             gwy_selection_get_data(GWY_SELECTION((graph->area)->zoomdata), NULL) - 1,
                             selection_zoomdata);

    x_reqmin = MIN(selection_zoomdata[0],
                   selection_zoomdata[0] + selection_zoomdata[2]);
    x_reqmax = MAX(selection_zoomdata[0],
                   selection_zoomdata[0] + selection_zoomdata[2]);
    y_reqmin = MIN(selection_zoomdata[1],
                   selection_zoomdata[1] + selection_zoomdata[3]);
    y_reqmax = MAX(selection_zoomdata[1],
                   selection_zoomdata[1] + selection_zoomdata[3]);

    for (i = GTK_POS_LEFT; i <= GTK_POS_RIGHT; i++)
        gwy_axis_set_req(graph->axis[i], x_reqmin, x_reqmax);
    for (i = GTK_POS_TOP; i <= GTK_POS_BOTTOM; i++)
        gwy_axis_set_req(graph->axis[i], y_reqmin, y_reqmax);

    set_graph_model_ranges(graph);

    /*refresh widgets*/
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);
    gwy_graph_area_refresh(graph->area);
}

/* XXX: This should not bloody work this bloody way.  Requests are users
 * request, and if the graph recomputes it, then it's no request any more. */
static void
set_graph_model_ranges(GwyGraph *graph)
{
    /* Just don't do anything.  This direction of range info flow should not
     * exist.
    gwy_graph_model_set_xmax(graph->graph_model,
                             gwy_axis_get_maximum(graph->axis[GTK_POS_BOTTOM]));
    gwy_graph_model_set_xmin(graph->graph_model,
                             gwy_axis_get_minimum(graph->axis[GTK_POS_BOTTOM]));
    gwy_graph_model_set_ymax(graph->graph_model,
                             gwy_axis_get_maximum(graph->axis[GTK_POS_LEFT]));
    gwy_graph_model_set_ymin(graph->graph_model,
                             gwy_axis_get_minimum(graph->axis[GTK_POS_LEFT]));
                             */
}

static void
label_updated_cb(GwyAxis *axis, GwyGraph *graph)
{
    if (graph->graph_model)
        gwy_graph_model_set_axis_label(graph->graph_model,
                                       gwy_axis_get_orientation(axis),
                                       gwy_axis_get_label(axis));
}

/**
 * gwy_graph_set_grid_type:
 * @graph: A graph widget.
 * @grid_type: The type of grid the graph should be set to
 *
 * Sets the graph to a particular grid type.
 **/
void
gwy_graph_set_grid_type(GwyGraph *graph, GwyGraphGridType grid_type)
{
    graph->grid_type = grid_type;
    gwy_graph_refresh(graph);
}

/**
 * gwy_graph_get_grid_type:
 * @graph: A graph widget.
 *
 * Return: The grid type of the graph.
 **/
/* XXX: Malformed documentation. */
GwyGraphGridType
gwy_graph_get_grid_type(GwyGraph *graph)
{
    return graph->grid_type;
}

/**
 * gwy_graph_set_x_grid_data:
 * @graph: A graph widget.
 * @grid_data: An array of grid data
 *
 * Sets the grid data for the x-axis of the graph area. @grid_data
 * is duplicated.
 **/
/* XXX: Malformed documentation. */
void
gwy_graph_set_x_grid_data(GwyGraph *graph, GArray *grid_data)
{
    gwy_graph_area_set_x_grid_data(graph->area, grid_data);
}

/**
 * gwy_graph_set_y_grid_data:
 * @graph: A graph widget.
 * @grid_data: An array of grid data
 *
 * Sets the grid data for the y-axis of the graph area. @grid_data
 * is duplicated.
 **/
/* XXX: Malformed documentation. */
void
gwy_graph_set_y_grid_data(GwyGraph *graph, GArray *grid_data)
{
    gwy_graph_area_set_y_grid_data(graph->area, grid_data);
}

/**
 * gwy_graph_get_x_grid_data:
 * @graph: A graph widget.
 *
 * Return: Array of grid data for the x-axis of the graph area (do not free).
 **/
/* XXX: Malformed documentation. */
const GArray*
gwy_graph_get_x_grid_data(GwyGraph *graph)
{
    return gwy_graph_area_get_x_grid_data(graph->area);
}

/**
 * gwy_graph_get_y_grid_data:
 * @graph: A graph widget.
 *
 * Return: Array of grid data for the y-axis of the graph area (do not free).
 **/
/* XXX: Malformed documentation. */
const GArray*
gwy_graph_get_y_grid_data(GwyGraph *graph)
{
    return gwy_graph_area_get_y_grid_data(graph->area);
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraph
 * @title: GwyGraph
 * @short_description: Widget for displaying graphs
 *
 * #GwyGraph is a basic widget for displaying graphs.
 * It consists of several widgets that can also be used separately.
 * However, it is recomended (and it should be easy)
 * to use the whole #GwyGraph widget and its API for most purposes.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
