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
    "read",
    "Read graph value module",
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
    static GwyGraphFuncInfo read_func_info = {
        "read",
        "/_Read Values",
        (GwyGraphFunc)&read,
    };

    gwy_graph_func_register(name, &read_func_info);

    return TRUE;
}

static gboolean
read(GwyGraph *graph)
{

    if (!graph) {
        if (dialog) gtk_widget_destroy(dialog);
        dialog = NULL;
        return 1;
    }

    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_CURSOR);
    if (!dialog) read_dialog(graph);

    return 1;
}


static gboolean
read_dialog(GwyGraph *graph)
{

    dialog = gtk_dialog_new_with_buttons(_("Read graph values"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                         NULL);

    g_signal_connect_swapped(dialog, "delete_event",
                             G_CALLBACK(read_dialog_closed_cb), graph);
    response_id = g_signal_connect_swapped(dialog, "response",
                                           G_CALLBACK(read_dialog_response_cb),
                                           graph);

    g_signal_connect_swapped(graph, "destroy",
                             G_CALLBACK(read_dialog_closed_cb), graph);

    controls.xlabel = gtk_label_new("x");
    controls.ylabel = gtk_label_new("y");

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.xlabel,
                                                FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), controls.ylabel,
                                                FALSE, FALSE, 4);
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
    gchar buffer[50];

    graph = (GwyGraph *) data;
    g_return_if_fail(GWY_IS_GRAPH(graph));

    if (gwy_graph_get_status(graph) != GWY_GRAPH_STATUS_CURSOR) return;
        /* FIXME TODO XXX this must be changed XXX TODO FIXME*/

    g_assert(gwy_graph_get_status(graph) == GWY_GRAPH_STATUS_CURSOR);

    cd = (GwyGraphStatus_CursorData*)gwy_graph_get_status_data(graph);


    if (cd->data_point.x_unit != NULL)
    {
        if ((fabs(cd->data_point.x)<=1e5 && fabs(cd->data_point.x)>1e-2) || fabs(cd->data_point.x)==0)
            g_snprintf(buffer, sizeof(buffer), "x = %.3f %s", cd->data_point.x, cd->data_point.x_unit);
        else
            g_snprintf(buffer, sizeof(buffer), "x = %.3e %s", cd->data_point.x, cd->data_point.x_unit);
    }
    else
    {
        if ((fabs(cd->data_point.x)<=1e5 && fabs(cd->data_point.x)>1e-2) || fabs(cd->data_point.x)==0)
            g_snprintf(buffer, sizeof(buffer), "x = %.3f", cd->data_point.x);
        else
            g_snprintf(buffer, sizeof(buffer), "x = %.3e", cd->data_point.x);
    }

    gtk_label_set_text(GTK_LABEL(controls.xlabel), buffer);

    if (cd->data_point.y_unit != NULL)
    {
        if ((fabs(cd->data_point.y)<=1e5 && fabs(cd->data_point.y)>1e-2) || fabs(cd->data_point.y)==0)
            g_snprintf(buffer, sizeof(buffer), "y = %.3f %s", cd->data_point.y, cd->data_point.y_unit);
        else
            g_snprintf(buffer, sizeof(buffer), "y = %.3e %s", cd->data_point.y, cd->data_point.y_unit);
    }
    else
    {
        if ((fabs(cd->data_point.y)<=1e5 && fabs(cd->data_point.y)>1e-2) || fabs(cd->data_point.y)==0)
            g_snprintf(buffer, sizeof(buffer), "y = %.3f", cd->data_point.y);
        else
            g_snprintf(buffer, sizeof(buffer), "y = %.3e", cd->data_point.y);
    }

    gtk_label_set_text(GTK_LABEL(controls.ylabel), buffer);

}

static void
read_dialog_closed_cb(gpointer data)
{
    GwyGraph *graph;
    graph = (GwyGraph *) data;

    gwy_graph_set_status(graph, GWY_GRAPH_STATUS_PLAIN);

    if (dialog)
    {
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
