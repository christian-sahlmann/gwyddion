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
#include <string.h>
#include <gtk/gtk.h>

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
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
    gdouble x_mag;
    gdouble y_mag;
} PointsControls;


static gboolean    module_register              (const gchar *name);
static gboolean    points                       (GwyGrapher *graph);
static gboolean    points_dialog                (GwyGrapher *graph);
static void        selection_updated_cb         (gpointer data);
static void        points_dialog_closed_cb      (gpointer data);
static void        points_dialog_response_cb    (gpointer data,
                                                 gint response);
static gdouble     get_unit_multiplicator       (gchar *unit);

static PointsControls controls;
static GtkWidget *dialog = NULL;
static gulong response_id = 0;
static gulong selection_id = 0;

static gint NMAX = 10;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Measure distances between points"),
    "Petr Klapetek <klapetek@gwyddion.net>",
    "1.1.1",
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
        N_("/_Measure Distances"),
        (GwyGraphFunc)&points,
    };

    gwy_graph_func_register(name, &points_func_info);

    return TRUE;
}

static gboolean
points(GwyGrapher *graph)
{
    if (!graph) {
        if (dialog)
            gtk_widget_destroy(dialog);
        dialog = NULL;
        return TRUE;
    }

    gwy_grapher_set_status(graph, GWY_GRAPHER_STATUS_POINTS);
    if (!dialog)
        points_dialog(graph);
    gtk_widget_queue_draw(GTK_WIDGET(graph));

    return TRUE;
}

static void
value_label(GtkWidget *label, gdouble value, GString *str)
{
    if ((fabs(value) <= 1e5 && fabs(value) > 1e-2) || fabs(value) == 0)
        g_string_printf(str, "%.2f", value);
    else
        g_string_printf(str, "%.2e", value);

    gtk_label_set_text(GTK_LABEL(label), str->str);
}

static void
header_label(GtkWidget *table, gint row, gint col,
             const gchar *header, const gchar *unit,
             GString *str)
{
    GtkWidget *label;

    label = gtk_label_new(NULL);
    if (unit)
        g_string_printf(str, "<b>%s</b> [%s]", header, unit);
    else
        g_string_printf(str, "<b>%s</b>", header);
    gtk_label_set_markup(GTK_LABEL(label), str->str);

    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, col, col+1, row, row+1,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);
}

static gboolean
points_dialog(GwyGrapher *graph)
{
    gint i;
    GtkWidget *label;
    GtkWidget *table;
    GString *str;

    dialog = gtk_dialog_new_with_buttons(_("Measure distances"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLEAR, GTK_RESPONSE_REJECT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

    g_signal_connect_swapped(dialog, "delete_event",
                             G_CALLBACK(points_dialog_closed_cb), graph);
    response_id
        = g_signal_connect_swapped(dialog, "response",
                                   G_CALLBACK(points_dialog_response_cb),
                                   graph);

    g_signal_connect_swapped(graph, "destroy",
                             G_CALLBACK(points_dialog_closed_cb), graph);

    controls.labpoint = g_ptr_array_new();
    controls.pointx = g_ptr_array_new();
    controls.pointy = g_ptr_array_new();
    controls.distx = g_ptr_array_new();
    controls.disty = g_ptr_array_new();
    controls.slope = g_ptr_array_new();
    str = g_string_new("");

    table = gtk_table_new(1, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    /*
    controls.xlabel = gtk_label_new("x");
    gtk_misc_set_alignment(GTK_MISC(controls.xlabel), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.xlabel, 1, 2, 0, 1, 0, 0, 2, 2);

    controls.ylabel = gtk_label_new("y");
    gtk_misc_set_alignment(GTK_MISC(controls.ylabel), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.ylabel, 2, 3, 0, 1, 0, 0, 2, 2);
    
    
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Mouse:</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, 0, 0, 2, 2);
    */
    
    /* big table */
    table = gtk_table_new(6, 11, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), _("<b>Points</b>"));
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                     GTK_FILL | GTK_EXPAND, 0, 2, 2);


    controls.x_mag = gwy_axiser_get_magnification(graph->axis_top);
    controls.y_mag = gwy_axiser_get_magnification(graph->axis_left);
    header_label(table, 1, 1, "X", gwy_axiser_get_magnification_string(graph->axis_top)->str, str);
    header_label(table, 1, 2, "Y", gwy_axiser_get_magnification_string(graph->axis_left)->str, str);
    header_label(table, 1, 3, _("Length"), gwy_axiser_get_magnification_string(graph->axis_top)->str, str);
    header_label(table, 1, 4, _("Height"), gwy_axiser_get_magnification_string(graph->axis_left)->str, str);
    header_label(table, 1, 5, _("Angle"), "deg", str);

    for (i = 0; i < NMAX; i++) {
        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 0, 1, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(controls.labpoint, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 1, 2, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(controls.pointx, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 2, 3, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(controls.pointy, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 3, 4, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(controls.distx, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 4, 5, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(controls.disty, label);

        label = gtk_label_new(NULL);
        gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
        gtk_table_attach(GTK_TABLE(table), label, 5, 6, i+2, i+3,
                         GTK_FILL | GTK_EXPAND, 0, 4, 2);
        g_ptr_array_add(controls.slope, label);
    }
    g_string_free(str, TRUE);

    selection_id = g_signal_connect_swapped(graph, "selected",
                                            G_CALLBACK(selection_updated_cb),
                                            graph);

    gtk_widget_show_all(dialog);

    return TRUE;
}

static void
selection_updated_cb(gpointer data)
{
    GwyGrapher *graph;
    gchar buffer[64];
    GtkWidget *label;
    GString *str;
    gint i, n;
    gdouble *spoints;

    graph = (GwyGrapher *) data;
    g_return_if_fail(GWY_IS_GRAPHER(graph));
    g_return_if_fail(gwy_grapher_get_status(graph) == GWY_GRAPHER_STATUS_POINTS);
    /*FIXME TODO XXX this must be changed XXX TODO FIXME */

    if ((n = gwy_grapher_get_selection_number(graph))>0)
        spoints = (gdouble *) g_malloc(2*gwy_grapher_get_selection_number(graph)*sizeof(gdouble));

    gwy_grapher_get_selection(graph, spoints);
    
    /*update mouse data */
    /*
    if (graph->x_unit != NULL) {
        if ((fabs(cd->actual_data_point.x) <= 1e5
             && fabs(cd->actual_data_point.x) > 1e-2)
            || fabs(cd->actual_data_point.x) == 0)
            g_snprintf(buffer, sizeof(buffer), "x = %.3f %s ",
                       cd->actual_data_point.x, graph->x_unit);
        else
            g_snprintf(buffer, sizeof(buffer), "x = %.3e %s ",
                       cd->actual_data_point.x, graph->x_unit);

    }
    else {
        if ((fabs(cd->actual_data_point.x) <= 1e5
             && fabs(cd->actual_data_point.x) > 1e-2)
            || fabs(cd->actual_data_point.x) == 0)
            g_snprintf(buffer, sizeof(buffer), "x = %.3f ",
                       cd->actual_data_point.x);
        else
            g_snprintf(buffer, sizeof(buffer), "x = %.3e ",
                       cd->actual_data_point.x);

    }

    gtk_label_set_text(GTK_LABEL(controls.xlabel), buffer);

    if (graph->y_unit != NULL) {
        if ((fabs(cd->actual_data_point.y) <= 1e5
             && fabs(cd->actual_data_point.y) > 1e-2)
            || fabs(cd->actual_data_point.y) == 0)
            g_snprintf(buffer, sizeof(buffer), "y = %.3f %s",
                       cd->actual_data_point.y, graph->y_unit);
        else
            g_snprintf(buffer, sizeof(buffer), "y = %.3e %s",
                       cd->actual_data_point.y, graph->y_unit);
    }
    else {
        if ((fabs(cd->actual_data_point.y) <= 1e5
             && fabs(cd->actual_data_point.y) > 1e-2)
            || fabs(cd->actual_data_point.y) == 0)
            g_snprintf(buffer, sizeof(buffer), "y = %.3f ",
                       cd->actual_data_point.y);
        else
            g_snprintf(buffer, sizeof(buffer), "y = %.3e ",
                       cd->actual_data_point.y);
    }

    gtk_label_set_text(GTK_LABEL(controls.ylabel), buffer);
    */
    /*update points data */
    str = g_string_new("");
    for (i = 0; i < NMAX; i++) {
        if (i < n) {
            label = g_ptr_array_index(controls.pointx, i);
            value_label(label, spoints[2*i]/controls.x_mag, str);

            label = g_ptr_array_index(controls.pointy, i);
            value_label(label, spoints[2*i + 1]/controls.y_mag, str);

            if (!i)
                continue;

            label = g_ptr_array_index(controls.distx, i);
            value_label(label, (spoints[2*i] - spoints[2*(i-1)])/controls.x_mag, str);

            label = g_ptr_array_index(controls.disty, i);
            value_label(label, (spoints[2*i + 1] - spoints[2*(i-1) + 1])/controls.y_mag, str);

            label = g_ptr_array_index(controls.slope, i);
            value_label(label,
                        180.0/G_PI*atan2((spoints[2*i + 1] - spoints[2*(i-1) + 1]),
                                         (spoints[2*i] - spoints[2*(i-1)])),
                        str);
        }
        else {
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.pointx, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.pointy, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.distx, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.disty, i)),
                               "");
            gtk_label_set_text(GTK_LABEL(g_ptr_array_index(controls.slope, i)),
                               "");
        }
    }

    if (n) g_free(spoints);
    g_string_free(str, TRUE);
}

static gdouble
get_unit_multiplicator(gchar *unit)
{
    if (g_str_has_prefix(unit, "m"))
        return 1.0e-3;
    if (g_str_has_prefix(unit, "µ"))
        return 1.0e-6;
    if (g_str_has_prefix(unit, "n"))
        return 1.0e-9;
    if (g_str_has_prefix(unit, "p"))
        return 1.0e-12;
    if (g_str_has_prefix(unit, "f"))
        return 1.0e-15;
    if (g_str_has_prefix(unit, "a"))
        return 1.0e-18;
    if (g_str_has_prefix(unit, "z"))
        return 1.0e-21;
    if (g_str_has_prefix(unit, "y"))
        return 1.0e-24;
    if (g_str_has_prefix(unit, "k"))
        return 1.0e3;
    if (g_str_has_prefix(unit, "M"))
        return 1.0e6;
    if (g_str_has_prefix(unit, "G"))
        return 1.0e9;
    if (g_str_has_prefix(unit, "T"))
        return 1.0e12;
    if (g_str_has_prefix(unit, "P"))
        return 1.0e15;
    if (g_str_has_prefix(unit, "E"))
        return 1.0e18;
    if (g_str_has_prefix(unit, "Z"))
        return 1.0e21;
    if (g_str_has_prefix(unit, "Y"))
        return 1.0e24;
    return 1;
}

static void
points_dialog_closed_cb(gpointer data)
{
    GwyGraph *graph;

    graph = (GwyGraph *) data;

    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);
    gtk_widget_queue_draw(GTK_WIDGET(graph));

    if (dialog) {
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
points_dialog_response_cb(gpointer data, gint response)
{
    GwyGraph *graph;

    graph = (GwyGraph *) data;

    if (response == GTK_RESPONSE_REJECT) {
        gwy_grapher_clear_selection(graph);
        gtk_widget_queue_draw(GTK_WIDGET(graph));
        selection_updated_cb(data);
    }
    else
        points_dialog_closed_cb(data);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
