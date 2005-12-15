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
#include <app/app.h>

/* Data for this function.*/
typedef struct {
    GtkWidget *graph;

    GtkWidget *dialog;
    gulong response_id;
    gulong selection_id;

    GtkWidget *selection_start_label;
    GtkWidget *selection_end_label;
    GwyGraphStatusType last_status;
} StatsControls;

static gboolean    module_register             (const gchar *name);
static gboolean    stats                       (GwyGraph *graph);
static gboolean    stats_dialog                (StatsControls *data);
static void        selection_updated_cb        (StatsControls *data);
static void        stats_dialog_closed_cb      (StatsControls *data);
static void        stats_dialog_response_cb    (StatsControls *data);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Graph statistics."),
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
    static GwyGraphFuncInfo stats_func_info = {
        "stats",
        N_("/_Graph statistics"),
        (GwyGraphFunc)&stats,
    };

    gwy_graph_func_register(name, &stats_func_info);

    return TRUE;
}

static gboolean
stats(GwyGraph *graph)
{
    StatsControls *data;
    GwyGraphArea *area;

    data = g_new(StatsControls, 1);
    data->dialog = NULL;

    /*
    if (!graph) {
        if (dialog)
            gtk_widget_destroy(dialog);
        dialog = NULL;
        return TRUE;
    } */

    data->graph = GTK_WIDGET(graph);


    data->last_status = gwy_graph_get_status(graph);
    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_XSEL);

    area = GWY_GRAPH_AREA(gwy_graph_get_area(graph));
    gwy_graph_area_set_selection_limit(area, 1);

    if (!data->dialog)
        stats_dialog(data);

    return TRUE;
}

static gboolean
stats_dialog(StatsControls *data)
{
    GwyGraph *graph;
    GtkDialog *dialog;
    GtkWidget *table, *label;

    graph = GWY_GRAPH(data->graph);
    data->dialog = gtk_dialog_new_with_buttons(_("Graph statistics"),
                                               NULL,
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_STOCK_CLOSE,
                                               GTK_RESPONSE_CLOSE,
                                               NULL);
    dialog = GTK_DIALOG(data->dialog);
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_dialog_set_default_response(dialog, GTK_RESPONSE_CLOSE);
    g_signal_connect_swapped(dialog, "delete_event",
                             G_CALLBACK(stats_dialog_closed_cb), data);
    data->response_id =
        g_signal_connect_swapped(dialog, "response",
                                 G_CALLBACK(stats_dialog_response_cb),
                                 data);

    g_signal_connect_swapped(graph, "destroy",
                             G_CALLBACK(stats_dialog_closed_cb), data);

    table = gtk_table_new(2, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new("from");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, 0, 0, 2, 2);

    label = gtk_label_new("to");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, 0, 0, 2, 2);

    data->selection_start_label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(data->selection_start_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), data->selection_start_label, 2, 3, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    data->selection_end_label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(data->selection_end_label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), data->selection_end_label, 2, 3, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    data->selection_id =
        g_signal_connect_swapped(graph, "selected",
                                 G_CALLBACK(selection_updated_cb),
                                 data);

    gtk_widget_show_all(GTK_WIDGET(dialog));

    selection_updated_cb(data);

    return TRUE;
}

static void
selection_updated_cb(StatsControls *data)
{
    GwyGraph *graph;
    GwyGraphArea *area;
    gdouble selection[2];
    gdouble from, to;
    gchar buffer[100];
    GwySIValueFormat *format;

    graph = GWY_GRAPH(data->graph);
    g_return_if_fail(GWY_IS_GRAPH(graph));
    area = GWY_GRAPH_AREA(gwy_graph_get_area(graph));

    if (gwy_graph_area_get_selection_number(area))
    {
        gwy_graph_area_get_selection(area, selection);
        from = selection[0];
        to = selection[1];
    }
    else
    {
        from = gwy_graph_get_model(graph)->x_min;
        to = gwy_graph_get_model(graph)->x_max;
    }

    format = gwy_si_unit_get_format((gwy_graph_get_model(graph))->x_unit,
                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    from, NULL);


    g_snprintf(buffer, sizeof(buffer), "%.3f %s", from/format->magnitude,
format->units);

    gtk_label_set_markup(GTK_LABEL(data->selection_start_label), buffer);

    format = gwy_si_unit_get_format((gwy_graph_get_model(graph))->x_unit,
                                    GWY_SI_UNIT_FORMAT_VFMARKUP,
                                    to, format);

    g_snprintf(buffer, sizeof(buffer), "%.3f %s", to/format->magnitude,
format->units);

    gtk_label_set_markup(GTK_LABEL(data->selection_end_label), buffer);

    g_free(format->units);


}

static void
stats_dialog_closed_cb(StatsControls *data)
{
    GwyGraph *graph;
    graph = GWY_GRAPH(data->graph);

    gwy_graph_set_status(graph, data->last_status);

    if (data->dialog) {
        g_signal_handler_disconnect(data->dialog, data->response_id);
        g_signal_handler_disconnect(data->graph, data->selection_id);
        data->response_id = 0;
        data->selection_id = 0;
        gtk_widget_destroy(data->dialog);
        data->dialog = NULL;
    }

    g_free(data);
}


static void
stats_dialog_response_cb(StatsControls *data)
{
    stats_dialog_closed_cb(data);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
