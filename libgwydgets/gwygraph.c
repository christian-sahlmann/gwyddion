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
#include <string.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwydgets/gwygraph.h>
#include <libgwydgets/gwygraphmodel.h>

static void gwy_graph_finalize          (GObject *object);
static void gwy_graph_refresh_all       (GwyGraph *graph);
static void gwy_graph_model_notify      (GwyGraph *graph,
                                         GParamSpec *pspec,
                                         GwyGraphModel *gmodel);
static void gwy_graph_curve_data_changed(GwyGraph *graph,
                                         gint i);
static void gwy_graph_refresh_x_range   (GwyGraph *graph);
static void gwy_graph_refresh_y_range   (GwyGraph *graph);
static void gwy_graph_axis_rescaled     (GwyAxis *axis,
                                         GwyGraph *graph);
static void gwy_graph_zoomed            (GwyGraph *graph);
static void gwy_graph_label_updated     (GwyAxis *axis,
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

    gwy_signal_handler_disconnect(graph->zoom_selection,
                                  graph->zoom_finished_id);
    gwy_signal_handler_disconnect(graph->graph_model, graph->model_notify_id);
    gwy_signal_handler_disconnect(graph->graph_model,
                                  graph->curve_data_changed_id);
    gwy_object_unref(graph->graph_model);
    gwy_object_unref(graph->zoom_selection);
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
    GwyGraph *graph;
    guint i;

    gwy_debug("");

    graph = GWY_GRAPH(g_object_new(GWY_TYPE_GRAPH, NULL));

    graph->area = GWY_GRAPH_AREA(gwy_graph_area_new());
    graph->area->status = GWY_GRAPH_STATUS_PLAIN;
    graph->enable_user_input = TRUE;

    gtk_table_resize(GTK_TABLE(graph), 3, 3);
    gtk_table_set_homogeneous(GTK_TABLE(graph), FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(graph), 0);
    gtk_table_set_col_spacings(GTK_TABLE(graph), 0);

    for (i = GTK_POS_LEFT; i <= GTK_POS_BOTTOM; i++)
        graph->axis[i] = GWY_AXIS(gwy_axis_new(i));

    gwy_graph_set_axis_visible(graph, GTK_POS_RIGHT, FALSE);
    gwy_graph_set_axis_visible(graph, GTK_POS_TOP, FALSE);

    /* XXX: Is there any reason why we never connect to "rescaled" of top and
     * right axes? */
    /* Axis signals never disconnected, we assume axes are not reparented
     * elsewhere */
    graph->rescaled_id[GTK_POS_LEFT]
        = g_signal_connect(graph->axis[GTK_POS_LEFT], "rescaled",
                           G_CALLBACK(gwy_graph_axis_rescaled), graph);
    graph->rescaled_id[GTK_POS_BOTTOM]
        = g_signal_connect(graph->axis[GTK_POS_BOTTOM], "rescaled",
                           G_CALLBACK(gwy_graph_axis_rescaled), graph);

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
                         G_CALLBACK(gwy_graph_label_updated), graph);
        gtk_widget_show(GTK_WIDGET(graph->axis[i]));
    }

    for (i = 0; i < 4; i++) {
        graph->corner[i] = GWY_GRAPH_CORNER(gwy_graph_corner_new());
        gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->corner[i]),
                         2*(i/2), 2*(i/2) + 1, 2*(i%2), 2*(i%2) + 1,
                         GTK_FILL, GTK_FILL, 0, 0);
        gtk_widget_show(GTK_WIDGET(graph->corner[i]));
    }

    graph->zoom_selection = gwy_graph_area_get_selection(graph->area,
                                                         GWY_GRAPH_STATUS_ZOOM);
    g_object_ref(graph->zoom_selection);
    graph->zoom_finished_id
        = g_signal_connect_swapped(graph->zoom_selection, "finished",
                                   G_CALLBACK(gwy_graph_zoomed), graph);

    gtk_table_attach(GTK_TABLE(graph), GTK_WIDGET(graph->area), 1, 2, 1, 2,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     GTK_FILL | GTK_EXPAND | GTK_SHRINK,
                     0, 0);

    gtk_widget_show_all(GTK_WIDGET(graph->area));

    if (gmodel)
        gwy_graph_set_model(GWY_GRAPH(graph), gmodel);

    return GTK_WIDGET(graph);
}

static void
gwy_graph_refresh_all(GwyGraph *graph)
{
    GwyGraphModel *gmodel;
    GwySIUnit *siunit;
    guint i;

    if (!graph->graph_model)
        return;

    gmodel = GWY_GRAPH_MODEL(graph->graph_model);

    g_object_get(gmodel, "si-unit-x", &siunit, NULL);
    gwy_axis_set_si_unit(graph->axis[GTK_POS_BOTTOM], siunit);
    gwy_axis_set_si_unit(graph->axis[GTK_POS_TOP], siunit);
    g_object_unref(siunit);

    g_object_get(gmodel, "si-unit-y", &siunit, NULL);
    gwy_axis_set_si_unit(graph->axis[GTK_POS_LEFT], siunit);
    gwy_axis_set_si_unit(graph->axis[GTK_POS_RIGHT], siunit);
    g_object_unref(siunit);

    for (i = GTK_POS_LEFT; i <= GTK_POS_BOTTOM; i++) {
        const gchar *label;

        label = gmodel ? gwy_graph_model_get_axis_label(gmodel, i) : NULL;
        gwy_axis_set_label(graph->axis[i], label);
    }

    gwy_graph_refresh_x_range(graph);
    gwy_graph_refresh_y_range(graph);
}

/**
 * gwy_graph_set_model:
 * @graph: A graph widget.
 * @gmodel: New graph model
 *
 * Changes the model a graph displays.
 *
 * Everything in graph widgets will be reset to reflect the new data.
 **/
void
gwy_graph_set_model(GwyGraph *graph, GwyGraphModel *gmodel)
{
    g_return_if_fail(GWY_IS_GRAPH(graph));
    g_return_if_fail(!gmodel || GWY_IS_GRAPH_MODEL(gmodel));

    if (graph->graph_model == gmodel)
        return;

    gwy_signal_handler_disconnect(graph->graph_model, graph->model_notify_id);
    gwy_signal_handler_disconnect(graph->graph_model,
                                  graph->curve_data_changed_id);

    if (gmodel)
        g_object_ref(gmodel);
    gwy_object_unref(graph->graph_model);
    graph->graph_model = gmodel;

    if (gmodel) {
        graph->model_notify_id
            = g_signal_connect_swapped(gmodel, "notify",
                                       G_CALLBACK(gwy_graph_model_notify),
                                       graph);
        graph->curve_data_changed_id
            = g_signal_connect_swapped(gmodel, "curve-data-changed",
                                       G_CALLBACK(gwy_graph_curve_data_changed),
                                       graph);
    }

    gwy_graph_area_set_model(graph->area, gmodel);
    gwy_graph_refresh_all(graph);
}

static void
gwy_graph_model_notify(GwyGraph *graph,
                       GParamSpec *pspec,
                       GwyGraphModel *gmodel)
{
    /* Axis labels */
    if (g_str_has_prefix(pspec->name, "axis-label-")) {
        const gchar *name = pspec->name + strlen("axis-label-");
        gchar *label = NULL;

        g_object_get(gmodel, pspec->name, &label, NULL);
        if (gwy_strequal(name, "left"))
            gwy_axis_set_label(graph->axis[GTK_POS_LEFT], label);
        else if (gwy_strequal(name, "bottom"))
            gwy_axis_set_label(graph->axis[GTK_POS_BOTTOM], label);
        else if (gwy_strequal(name, "right"))
            gwy_axis_set_label(graph->axis[GTK_POS_RIGHT], label);
        else if (gwy_strequal(name, "top"))
            gwy_axis_set_label(graph->axis[GTK_POS_TOP], label);
        g_free(label);

        return;
    }

    /* Units */
    if (g_str_has_prefix(pspec->name, "si-unit")) {
        const gchar *name = pspec->name + strlen("si-unit-");
        GwySIUnit *unit = NULL;

        /* Both model and axis assign units by value so this is correct */
        g_object_get(gmodel, pspec->name, &unit, NULL);
        if (gwy_strequal(name, "x")) {
            gwy_axis_set_si_unit(graph->axis[GTK_POS_BOTTOM], unit);
            gwy_axis_set_si_unit(graph->axis[GTK_POS_TOP], unit);
        }
        else if (gwy_strequal(name, "y")) {
            gwy_axis_set_si_unit(graph->axis[GTK_POS_LEFT], unit);
            gwy_axis_set_si_unit(graph->axis[GTK_POS_RIGHT], unit);
        }
        g_object_unref(unit);

        return;
    }

    /* Ranges */
    if (g_str_has_prefix(pspec->name, "x-")) {
        gwy_graph_refresh_x_range(graph);
        return;
    }
    if (g_str_has_prefix(pspec->name, "y-")) {
        gwy_graph_refresh_y_range(graph);
        return;
    }

    /* Number of curves */
    if (gwy_strequal(pspec->name, "n-curves")) {
        gwy_graph_curve_data_changed(graph, -1);
        return;
    }

    gwy_debug("ignoring changed model property <%s>", pspec->name);
}

static void
gwy_graph_curve_data_changed(GwyGraph *graph,
                             G_GNUC_UNUSED gint i)
{
    gwy_graph_refresh_x_range(graph);
    gwy_graph_refresh_y_range(graph);
}

static void
gwy_graph_refresh_x_range(GwyGraph *graph)
{
    GwyGraphModel *gmodel = graph->graph_model;
    gdouble min, max;
    gboolean lg;

    g_object_get(gmodel, "x-logarithmic", &lg, NULL);
    gwy_axis_set_logarithmic(graph->axis[GTK_POS_BOTTOM], lg);
    gwy_axis_set_logarithmic(graph->axis[GTK_POS_TOP], lg);

    /* Request range */
    if (!gwy_graph_model_get_x_range(gmodel, &min, &max)) {
        min = 0.0;
        max = 1.0;
    }
    gwy_debug("%p: req: %g, %g", graph, min, max);

    gwy_axis_request_range(graph->axis[GTK_POS_BOTTOM], min, max);
    gwy_axis_request_range(graph->axis[GTK_POS_TOP], min, max);  /* XXX */

    /* Fetch the range axes actually decided to use */
    gwy_axis_get_range(graph->axis[GTK_POS_BOTTOM], &min, &max);
    gwy_debug("%p: axis: %g, %g", graph, min, max);

    gwy_graph_area_set_x_range(graph->area, min, max);

    /* TODO */
    if (TRUE/* grid_type == GWY_GRAPH_GRID_AUTO */) {
        const gdouble *ticks;
        guint nticks;

        ticks = gwy_axis_get_major_ticks(graph->axis[GTK_POS_BOTTOM], &nticks);
        gwy_graph_area_set_x_grid_data(graph->area, nticks, ticks);
    }
}

static void
gwy_graph_refresh_y_range(GwyGraph *graph)
{
    GwyGraphModel *gmodel = graph->graph_model;
    gdouble min, max;
    gboolean lg;

    g_object_get(gmodel, "y-logarithmic", &lg, NULL);
    gwy_axis_set_logarithmic(graph->axis[GTK_POS_LEFT], lg);
    gwy_axis_set_logarithmic(graph->axis[GTK_POS_RIGHT], lg);

    /* Request range */
    if (!gwy_graph_model_get_y_range(gmodel, &min, &max)) {
        min = 0.0;
        max = 1.0;
    }
    gwy_debug("%p: req: %g, %g", graph, min, max);

    gwy_axis_request_range(graph->axis[GTK_POS_LEFT], min, max);
    gwy_axis_request_range(graph->axis[GTK_POS_RIGHT], min, max);  /* XXX */

    /* Fetch the range axes actually decided to use */
    gwy_axis_get_range(graph->axis[GTK_POS_LEFT], &min, &max);
    gwy_debug("%p: axis: %g, %g", graph, min, max);

    gwy_graph_area_set_y_range(graph->area, min, max);

    /* TODO */
    if (TRUE/* grid_type == GWY_GRAPH_GRID_AUTO */) {
        const gdouble *ticks;
        guint nticks;

        ticks = gwy_axis_get_major_ticks(graph->axis[GTK_POS_LEFT], &nticks);
        gwy_graph_area_set_y_grid_data(graph->area, nticks, ticks);
    }
}

static void
gwy_graph_axis_rescaled(GwyAxis *axis, GwyGraph *graph)
{
    GwyGraphGridType grid_type;
    gdouble min, max;
    GArray *array;

    if (graph->graph_model == NULL)
        return;

    gwy_debug("%p: axis %p", graph, axis);
    g_object_get(graph->graph_model, "grid-type", &grid_type, NULL);
    if (grid_type == GWY_GRAPH_GRID_AUTO) {
        const gdouble *ticks;
        guint nticks;

        ticks = gwy_axis_get_major_ticks(axis, &nticks);
        if (axis == graph->axis[GTK_POS_BOTTOM])
            gwy_graph_area_set_x_grid_data(graph->area, nticks, ticks);
        if (axis == graph->axis[GTK_POS_LEFT])
            gwy_graph_area_set_y_grid_data(graph->area, nticks, ticks);
    }

    gwy_axis_get_range(axis, &min, &max);
    if (axis == graph->axis[GTK_POS_BOTTOM])
        gwy_graph_area_set_x_range(graph->area, min, max);
    if (axis == graph->axis[GTK_POS_LEFT])
        gwy_graph_area_set_y_range(graph->area, min, max);
}

/**
 * gwy_graph_get_model:
 * @graph: A graph widget.
 *
 * Gets the model of a graph.
 *
 * Returns: Graph model associated with this graph widget (do not free).
 **/
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
 * gwy_graph_enable_user_input:
 * @graph: A graph widget.
 * @enable: whether to enable user input
 *
 * Enables/disables all the graph/curve settings dialogs to be invoked by
 * mouse clicks.
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

static void
gwy_graph_zoomed(GwyGraph *graph)
{
    gdouble x_reqmin, x_reqmax, y_reqmin, y_reqmax;
    GwySelection *selection;
    gdouble zoomdata[4];

    selection = graph->zoom_selection;
    if (graph->area->status != GWY_GRAPH_STATUS_ZOOM ||
        gwy_selection_get_data(selection, NULL) != 1)
        return;

    gwy_selection_get_object(selection, 0, zoomdata);
    x_reqmin = MIN(zoomdata[0], zoomdata[0] + zoomdata[2]);
    x_reqmax = MAX(zoomdata[0], zoomdata[0] + zoomdata[2]);
    y_reqmin = MIN(zoomdata[1], zoomdata[1] + zoomdata[3]);
    y_reqmax = MAX(zoomdata[1], zoomdata[1] + zoomdata[3]);
    /* This in turn causes graph refresh including axes rescale */
    g_object_set(graph->graph_model,
                 "x-min", x_reqmin, "x-min-set", TRUE,
                 "x-max", x_reqmax, "x-max-set", TRUE,
                 "y-min", y_reqmin, "y-min-set", TRUE,
                 "y-max", y_reqmax, "y-max-set", TRUE,
                 NULL);

    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);
}

static void
gwy_graph_label_updated(GwyAxis *axis, GwyGraph *graph)
{
    if (graph->graph_model)
        gwy_graph_model_set_axis_label(graph->graph_model,
                                       gwy_axis_get_orientation(axis),
                                       gwy_axis_get_label(axis));
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
