/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"
#include "gwygraphwindow.h"
#include "gwygraphdata.h"

#define GWY_GRAPH_WINDOW_TYPE_NAME "GwyGraphWindow"

#define DEFAULT_SIZE 360


/* Forward declarations */

static void     gwy_graph_window_class_init          (GwyGraphWindowClass *klass);
static void     gwy_graph_window_init                (GwyGraphWindow *graphwindow);
static void     gwy_graph_window_destroy             (GtkObject *object);
static void     gwy_graph_window_finalize            (GObject *object);
static void     gwy_graph_cursor_motion_cb           (GwyGraphWindow *graphwindow);
static void     gwy_graph_window_measure_cb          (GwyGraphWindow *graphwindow);
static void     gwy_graph_window_zoom_in_cb          (GwyGraphWindow *graphwindow);
static void     gwy_graph_window_zoom_out_cb         (GwyGraphWindow *graphwindow);
static void     gwy_graph_window_zoom_finished_cb    (GwyGraphWindow *graphwindow);
static void     gwy_graph_window_measure_finished_cb (GwyGraphWindow *graphwindow, gint response);

/* Local data */

static GtkWindowClass *parent_class = NULL;

/*static guint gwy3dwindow_signals[LAST_SIGNAL] = { 0 };*/


GType
gwy_graph_window_get_type(void)
{
    static GType gwy_graph_window_type = 0;

    if (!gwy_graph_window_type) {
        static const GTypeInfo gwy_graph_window_info = {
            sizeof(GwyGraphWindowClass),
            NULL,
            NULL,
            (GClassInitFunc)gwy_graph_window_class_init,
            NULL,
            NULL,
            sizeof(GwyGraphWindow),
            0,
            (GInstanceInitFunc)gwy_graph_window_init,
            NULL,
        };
        gwy_debug("");
        gwy_graph_window_type = g_type_register_static(GTK_TYPE_WINDOW,
                                                    GWY_GRAPH_WINDOW_TYPE_NAME,
                                                    &gwy_graph_window_info,
                                                    0);
    }

    return gwy_graph_window_type;
}

static void
gwy_graph_window_class_init(GwyGraphWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;

    gwy_debug("");

    object_class = (GtkObjectClass*)klass;
    parent_class = g_type_class_peek_parent(klass);

    gobject_class->finalize = gwy_graph_window_finalize;
    object_class->destroy = gwy_graph_window_destroy;

}

static void
gwy_graph_window_init(GwyGraphWindow *graphwindow)
{
    gwy_debug("");

}

static void
gwy_graph_window_finalize(GObject *object)
{
    gwy_debug("finalizing a GwyGraphWindow %p (refcount = %u)",
              object, object->ref_count);

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
gwy_graph_window_destroy(GtkObject *object)
{
    GTK_OBJECT_CLASS(parent_class)->destroy(object);
}


/**
 * gwy_graph_window_new:
 * @gwy3dview: A #Gwy3DView containing the data-displaying widget to show.
 *
 * Creates a new OpenGL 3D data displaying window.
 *
 * Returns: A newly created widget, as #GtkWidget.
 **/
GtkWidget*
gwy_graph_window_new(GwyGrapher *graph)
{
    GwyGraphWindow *graphwindow;
    GtkScrolledWindow *swindow;
    GtkWidget *vbox, *hbox;
    GtkWidget *label;
    
    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPHER(graph), NULL);

    graphwindow = (GwyGraphWindow*)g_object_new(GWY_TYPE_GRAPH_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(graphwindow), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(graphwindow), TRUE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_WINDOW(graphwindow), vbox);

    graphwindow->graph = graph;
    
    /*add notebook with graph and text matrix*/
    graphwindow->notebook = gtk_notebook_new();

    
    label = gtk_label_new("Graph");
    gtk_notebook_append_page(GTK_NOTEBOOK(graphwindow->notebook),
                             GTK_WIDGET(graph),
                             label);
                             
    
    swindow = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
    graphwindow->data = gwy_graph_data_new(gwy_grapher_get_model(graph));
    gtk_container_add(GTK_CONTAINER(swindow), graphwindow->data);

    label = gtk_label_new("Data");
    gtk_notebook_append_page(GTK_NOTEBOOK(graphwindow->notebook),
                             GTK_WIDGET(swindow),
                             label);
                             

    gtk_container_add(GTK_CONTAINER(vbox), graphwindow->notebook);
    /*add buttons*/

    hbox = gtk_hbox_new(FALSE, 0);

    graphwindow->button_measure_points = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_measure_points),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_MEASURE, GTK_ICON_SIZE_BUTTON)); 
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_measure_points, FALSE, FALSE, 4);
    g_signal_connect_swapped(graphwindow->button_measure_points, "clicked",
                           G_CALLBACK(gwy_graph_window_measure_cb),
                           graphwindow);
    
    
    graphwindow->button_zoom_in = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_zoom_in),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_ZOOM_IN, GTK_ICON_SIZE_BUTTON)); 
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_zoom_in, FALSE, FALSE, 0);
    g_signal_connect_swapped(graphwindow->button_zoom_in, "clicked",
                           G_CALLBACK(gwy_graph_window_zoom_in_cb),
                           graphwindow);
  
    graphwindow->button_zoom_out = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_zoom_out),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_ZOOM_OUT, GTK_ICON_SIZE_BUTTON)); 
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_zoom_out, FALSE, FALSE, 4);
    g_signal_connect_swapped(graphwindow->button_zoom_out, "clicked",
                           G_CALLBACK(gwy_graph_window_zoom_out_cb),
                           graphwindow);
 
    graphwindow->button_export_ascii = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_export_ascii),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH, GTK_ICON_SIZE_BUTTON)); 
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_export_ascii, FALSE, FALSE, 0);

    graphwindow->button_export_vector = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_export_vector),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH, GTK_ICON_SIZE_BUTTON)); 
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_export_vector, FALSE, FALSE, 0);

    graphwindow->button_export_bitmap = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_export_bitmap),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH, GTK_ICON_SIZE_BUTTON)); 
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_export_bitmap, FALSE, FALSE, 0);

    graphwindow->label_what = gtk_label_new("Cursor values:");
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->label_what, TRUE, FALSE, 4);

    graphwindow->label_x = gtk_label_new("       ");
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->label_x, TRUE, FALSE, 4);

    graphwindow->label_y = gtk_label_new("       ");
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->label_y, TRUE, FALSE, 4);

    

    gtk_container_add(GTK_CONTAINER(vbox), hbox);
   
    graphwindow->measure_dialog = GWY_GRAPHER_WINDOW_MEASURE_DIALOG(gwy_grapher_window_measure_dialog_new(graphwindow->graph));
    g_signal_connect_swapped(graphwindow->measure_dialog, "response",
                           G_CALLBACK(gwy_graph_window_measure_finished_cb),
                           graphwindow);
     
    /*
    gtk_tooltips_set_tip(gwy3dwindow->tips, button,
                         _("Show full controls"), NULL);
    
    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_graph_window_select_controls),
                             GINT_TO_POINTER(FALSE));
     */

    g_signal_connect_swapped(graphwindow->graph, "mousemoved", 
                             G_CALLBACK(gwy_graph_cursor_motion_cb), graphwindow);

    g_signal_connect_swapped(graphwindow->graph, "zoomed", 
                             G_CALLBACK(gwy_graph_window_zoom_finished_cb), graphwindow);


    
    return GTK_WIDGET(graphwindow);
}

/**
 * gwy_graph_window_get_3d_view:
 * @gwy3dwindow: A 3D data view window.
 *
 * Returns the #Gwy3DView widget this 3D window currently shows.
 *
 * Returns: The currently shown #GwyDataView.
 **/
GtkWidget*
gwy_graph_window_get_graph(GwyGraphWindow *graphwindow)
{
    g_return_val_if_fail(GWY_IS_GRAPH_WINDOW(graphwindow), NULL);

    return graphwindow->graph;
}


static void     
gwy_graph_cursor_motion_cb(GwyGraphWindow *graphwindow)
{
    gdouble x, y;
    gchar buffer[100];
    gdouble xmag, ymag;
    GString *xstring, *ystring;
    
    gwy_grapher_get_cursor(graphwindow->graph, &x, &y);

    xmag = gwy_axiser_get_magnification(GWY_GRAPHER(graphwindow->graph)->axis_top);
    xstring = gwy_axiser_get_magnification_string(GWY_GRAPHER(graphwindow->graph)->axis_top);

    ymag = gwy_axiser_get_magnification(GWY_GRAPHER(graphwindow->graph)->axis_left);
    ystring = gwy_axiser_get_magnification_string(GWY_GRAPHER(graphwindow->graph)->axis_left);
    
    
    g_snprintf(buffer, sizeof(buffer), "%.4f", x/xmag);
    xstring = g_string_prepend(xstring, buffer);
    gtk_label_set_markup(graphwindow->label_x, xstring->str);

    g_snprintf(buffer, sizeof(buffer), "%.4f", y/ymag);
    ystring = g_string_prepend(ystring, buffer);
    gtk_label_set_markup(graphwindow->label_y, ystring->str);
    
    g_string_free(xstring, TRUE);
    g_string_free(ystring, TRUE);
}

static void     
gwy_graph_window_measure_cb(GwyGraphWindow *graphwindow)
{
    gwy_grapher_set_status(graphwindow->graph, GWY_GRAPHER_STATUS_POINTS);
    gwy_grapher_signal_selected(graphwindow->graph);

    gtk_widget_show_all(GTK_WIDGET(graphwindow->measure_dialog));
}


static void     
gwy_graph_window_measure_finished_cb(GwyGraphWindow *graphwindow, gint response)
{
    
    gwy_grapher_clear_selection(graphwindow->graph);
    if (response == GWY_GRAPH_WINDOW_MEASURE_RESPONSE_CLEAR) return;
    
    gwy_grapher_set_status(graphwindow->graph, GWY_GRAPHER_STATUS_PLAIN);
    gtk_widget_hide(GTK_WIDGET(graphwindow->measure_dialog));
}

static void     
gwy_graph_window_zoom_in_cb(GwyGraphWindow *graphwindow)
{
    gwy_grapher_zoom_in(graphwindow->graph);
}

static void     
gwy_graph_window_zoom_out_cb(GwyGraphWindow *graphwindow)
{
    gwy_grapher_zoom_out(graphwindow->graph);
}

static void     
gwy_graph_window_zoom_finished_cb(GwyGraphWindow *graphwindow)
{
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
