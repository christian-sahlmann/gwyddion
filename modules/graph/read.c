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
    GtkWidget *xlabel;
    GtkWidget *ylabel;
} ReadControls;


static gboolean    module_register            (const gchar *name);
static gboolean    read                       (GwyGraph *graph);
static gboolean    read_dialog                (GwyGraph *graph);
static void        selection_updated_cb       (gpointer data);
static void        read_dialog_closed_cb      (gpointer data);
static void        read_dialog_response_cb    (gpointer data);

static ReadControls controls;
static GtkWidget *dialog = NULL;
static gulong response_id = 0;
static gulong selection_id = 0;

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Read graph values."),
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
    static GwyGraphFuncInfo read_func_info = {
        "read",
        N_("/_Read Values"),
        (GwyGraphFunc)&read,
    };

    gwy_graph_func_register(name, &read_func_info);

    return TRUE;
}

static gboolean
read(GwyGraph *graph)
{

    if (!graph) {
        if (dialog)
            gtk_widget_destroy(dialog);
        dialog = NULL;
        return TRUE;
    }

    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_CURSOR);
    if (!dialog)
        read_dialog(graph);

    return TRUE;
}


static gboolean
read_dialog(GwyGraph *graph)
{
    GtkWidget *table, *label;

    dialog = gtk_dialog_new_with_buttons(_("Read Graph Values"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

    g_signal_connect_swapped(dialog, "delete_event",
                             G_CALLBACK(read_dialog_closed_cb), graph);
    response_id = g_signal_connect_swapped(dialog, "response",
                                           G_CALLBACK(read_dialog_response_cb),
                                           graph);

    g_signal_connect_swapped(graph, "destroy",
                             G_CALLBACK(read_dialog_closed_cb), graph);

    table = gtk_table_new(2, 3, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_container_set_border_width(GTK_CONTAINER(table), 4);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

    label = gtk_label_new("x");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, 0, 0, 2, 2);

    label = gtk_label_new("y");
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2, 0, 0, 2, 2);

    label = gtk_label_new("=");
    gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1, 0, 0, 2, 2);

    controls.xlabel = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.xlabel), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.xlabel, 2, 3, 0, 1,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    controls.ylabel = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(controls.ylabel), 1.0, 0.5);
    gtk_table_attach(GTK_TABLE(table), controls.ylabel, 2, 3, 1, 2,
                     GTK_EXPAND | GTK_FILL, 0, 2, 2);

    selection_id = g_signal_connect_swapped(graph->area, "selected",
                                            G_CALLBACK(selection_updated_cb),
                                            graph);

    gtk_widget_show_all(dialog);

    return TRUE;
}

static void
selection_updated_cb(gpointer data)
{
    GwyGraph *graph;
    GwyGraphStatus_CursorData *cd;
    gchar buffer[40];

    graph = (GwyGraph *)data;
    g_return_if_fail(GWY_IS_GRAPH(graph));
    g_return_if_fail(gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_CURSOR);
    /* FIXME TODO XXX this must be changed XXX TODO FIXME */

    cd = (GwyGraphStatus_CursorData *) gwy_graph_get_status_data(graph);


    if (cd->data_point.x_unit != NULL) {
        if ((fabs(cd->data_point.x) <= 1e5 && fabs(cd->data_point.x) > 1e-2)
            || fabs(cd->data_point.x) == 0)
            g_snprintf(buffer, sizeof(buffer), "%.3f %s", cd->data_point.x,
                       cd->data_point.x_unit);
        else
            g_snprintf(buffer, sizeof(buffer), "%.3e %s", cd->data_point.x,
                       cd->data_point.x_unit);
    }
    else {
        if ((fabs(cd->data_point.x) <= 1e5 && fabs(cd->data_point.x) > 1e-2)
            || fabs(cd->data_point.x) == 0)
            g_snprintf(buffer, sizeof(buffer), "%.3f", cd->data_point.x);
        else
            g_snprintf(buffer, sizeof(buffer), "%.3e", cd->data_point.x);
    }

    gtk_label_set_text(GTK_LABEL(controls.xlabel), buffer);

    if (cd->data_point.y_unit != NULL) {
        if ((fabs(cd->data_point.y) <= 1e5 && fabs(cd->data_point.y) > 1e-2)
            || fabs(cd->data_point.y) == 0)
            g_snprintf(buffer, sizeof(buffer), "%.3f %s", cd->data_point.y,
                       cd->data_point.y_unit);
        else
            g_snprintf(buffer, sizeof(buffer), "%.3e %s", cd->data_point.y,
                       cd->data_point.y_unit);
    }
    else {
        if ((fabs(cd->data_point.y) <= 1e5 && fabs(cd->data_point.y) > 1e-2)
            || fabs(cd->data_point.y) == 0)
            g_snprintf(buffer, sizeof(buffer), "%.3f", cd->data_point.y);
        else
            g_snprintf(buffer, sizeof(buffer), "%.3e", cd->data_point.y);
    }

    gtk_label_set_text(GTK_LABEL(controls.ylabel), buffer);

}

static void
read_dialog_closed_cb(gpointer data)
{
    GwyGraph *graph;
    graph = (GwyGraph *)data;

    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);

    if (dialog) {
        g_signal_handler_disconnect(dialog, response_id);
        g_signal_handler_disconnect(graph->area, selection_id);
        response_id = 0;
        selection_id = 0;
        gtk_widget_destroy(dialog);
        dialog = NULL;
    }
}


static void
read_dialog_response_cb(gpointer data)
{
    read_dialog_closed_cb(data);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
