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

#include "config.h"
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libprocess/datafield.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwydgets.h"
#include "gwygraphwindow.h"
#include "gwygraphdata.h"
#include <libgwydgets/gwygraphwindowasciidialog.h>

#define DEFAULT_SIZE 360

/* Forward declarations */

static void gwy_graph_window_destroy            (GtkObject *object);
static void gwy_graph_window_finalize           (GObject *object);
static void gwy_graph_cursor_motion_cb          (GwyGraphWindow *graphwindow);
static void gwy_graph_window_measure_cb         (GwyGraphWindow *graphwindow);
static void gwy_graph_window_zoom_in_cb         (GwyGraphWindow *graphwindow);
static void gwy_graph_window_zoom_out_cb        (GwyGraphWindow *graphwindow);
static void gwy_graph_window_x_log_cb           (GwyGraphWindow *graphwindow);
static void gwy_graph_window_y_log_cb           (GwyGraphWindow *graphwindow);
static void gwy_graph_window_zoom_finished_cb   (GwyGraphWindow *graphwindow);
static void gwy_graph_window_measure_finished_cb(GwyGraphWindow *graphwindow,
                                                 gint response);
static void gwy_graph_window_export_vector_cb   (GwyGraphWindow *graphwindow);
static void gwy_graph_window_export_ascii_cb    (GwyGraphWindow *graphwindow);
static void gwy_graph_window_export_bitmap_cb   (GwyGraphWindow *graphwindow);
static void gwy_graph_window_set_tooltip        (GtkWidget *widget,
                                                 const gchar *tip_text);

/* Local data */

/* These are actually class data.  To put them to Class struct someone would
 * have to do class_ref() and live with this reference to the end of time. */
static GtkTooltips *tooltips = NULL;
static gboolean tooltips_set = FALSE;

/* XXX XXX XXX extreme brain damage XXX XXX XXX */
static GwyGraphStatusType last_status = 0;

/*static guint gwy3dwindow_signals[LAST_SIGNAL] = { 0 };*/

G_DEFINE_TYPE(GwyGraphWindow, gwy_graph_window, GTK_TYPE_WINDOW)

static void
gwy_graph_window_class_init(GwyGraphWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkObjectClass *object_class;

    object_class = (GtkObjectClass*)klass;

    gobject_class->finalize = gwy_graph_window_finalize;
    object_class->destroy = gwy_graph_window_destroy;
}

static void
gwy_graph_window_init(G_GNUC_UNUSED GwyGraphWindow *graphwindow)
{
    if (!tooltips_set && !tooltips) {
        tooltips = gtk_tooltips_new();
        g_object_ref(tooltips);
        gtk_object_sink(GTK_OBJECT(tooltips));
    }
}

static void
gwy_graph_window_finalize(GObject *object)
{
    gtk_widget_destroy(GTK_WIDGET(GWY_GRAPH_WINDOW(object)->measure_dialog));
    G_OBJECT_CLASS(gwy_graph_window_parent_class)->finalize(object);
}

static void
gwy_graph_window_destroy(GtkObject *object)
{
    GTK_OBJECT_CLASS(gwy_graph_window_parent_class)->destroy(object);
}


/**
 * gwy_graph_window_new:
 * @graph: A GwyGraph object containing the graph.
 *
 * Creates a new window showind graph.
 *
 * Returns: A newly created widget, as #GtkWidget.
 **/
GtkWidget*
gwy_graph_window_new(GwyGraph *graph)
{
    GwyGraphWindow *graphwindow;
    GtkScrolledWindow *swindow;
    GtkWidget *vbox, *hbox;
    GtkWidget *label;

    gwy_debug("");
    g_return_val_if_fail(GWY_IS_GRAPH(graph), NULL);

    graphwindow = (GwyGraphWindow*)g_object_new(GWY_TYPE_GRAPH_WINDOW, NULL);
    gtk_window_set_wmclass(GTK_WINDOW(graphwindow), "data",
                           g_get_application_name());
    gtk_window_set_resizable(GTK_WINDOW(graphwindow), TRUE);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_WINDOW(graphwindow)), vbox);

    graphwindow->graph = GTK_WIDGET(graph);

    /*add notebook with graph and text matrix*/
    graphwindow->notebook = gtk_notebook_new();
    if (graph->graph_model->title->str)
        gtk_window_set_title(GTK_WINDOW(graphwindow), graph->graph_model->title->str);        
    else
        gtk_window_set_title(GTK_WINDOW(graphwindow), "Untitled");

    label = gtk_label_new("Graph");
    gtk_notebook_append_page(GTK_NOTEBOOK(graphwindow->notebook),
                             GTK_WIDGET(graph),
                             label);


    swindow = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
    graphwindow->data = gwy_graph_data_new(gwy_graph_get_model(graph));
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
    gwy_graph_window_set_tooltip(graphwindow->button_measure_points, "Measure distances in graph");
    g_signal_connect_swapped(graphwindow->button_measure_points, "clicked",
                           G_CALLBACK(gwy_graph_window_measure_cb),
                           graphwindow);


    graphwindow->button_zoom_in = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_zoom_in),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_ZOOM_IN, GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_zoom_in, FALSE, FALSE, 0);
    gwy_graph_window_set_tooltip(graphwindow->button_zoom_in, "Zoom in by mouse selection.");
    g_signal_connect_swapped(graphwindow->button_zoom_in, "toggled",
                           G_CALLBACK(gwy_graph_window_zoom_in_cb),
                           graphwindow);

    graphwindow->button_zoom_out = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_zoom_out),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_ZOOM_OUT, GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_zoom_out, FALSE, FALSE, 4);
    gwy_graph_window_set_tooltip(graphwindow->button_zoom_out, "Zoom out to full curve.");
    g_signal_connect_swapped(graphwindow->button_zoom_out, "clicked",
                           G_CALLBACK(gwy_graph_window_zoom_out_cb),
                           graphwindow);


    graphwindow->button_x_log = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_x_log),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_X_LOGARITHMIC, GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_x_log, FALSE, FALSE, 0);
    g_signal_connect_swapped(graphwindow->button_x_log, "clicked",
                           G_CALLBACK(gwy_graph_window_x_log_cb),
                           graphwindow);
    
    gtk_widget_set_sensitive(graphwindow->button_x_log,
                gwy_graph_model_x_data_can_be_logarithmed(GWY_GRAPH(graphwindow->graph)->graph_model));
     
    graphwindow->button_y_log = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_y_log),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_Y_LOGARITHMIC, GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_y_log, FALSE, FALSE, 0);
    g_signal_connect_swapped(graphwindow->button_y_log, "clicked",
                           G_CALLBACK(gwy_graph_window_y_log_cb),
                           graphwindow);
    
    gtk_widget_set_sensitive(graphwindow->button_y_log,
                gwy_graph_model_y_data_can_be_logarithmed(GWY_GRAPH(graphwindow->graph)->graph_model));
    
     /*
    graphwindow->button_export_ascii = gtk_button_new();
    gtk_container_add(gtk_container(graphwindow->button_export_ascii),
                      gtk_image_new_from_stock(gwy_stock_graph_ascii, gtk_icon_size_button));
    gtk_box_pack_start(gtk_box(hbox), graphwindow->button_export_ascii, false, false, 0);
    g_signal_connect_swapped(graphwindow->button_export_ascii, "clicked",
                           g_callback(gwy_graph_window_export_ascii_cb),
                           graphwindow);

    graphwindow->button_export_vector = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_export_vector),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_VECTOR, GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_export_vector, FALSE, FALSE, 0);
    g_signal_connect_swapped(graphwindow->button_export_vector, "clicked",
                           G_CALLBACK(gwy_graph_window_export_vector_cb),
                           graphwindow);

    graphwindow->button_export_bitmap = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_export_bitmap),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_BITMAP, GTK_ICON_SIZE_BUTTON));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_export_bitmap, FALSE, FALSE, 0);
    g_signal_connect_swapped(graphwindow->button_export_bitmap, "clicked",
                           G_CALLBACK(gwy_graph_window_export_bitmap_cb),
                           graphwindow);
*/

    graphwindow->label_what = gtk_label_new("Cursor values:");
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->label_what, FALSE, FALSE, 4);

    graphwindow->label_x = gtk_label_new("          ");
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->label_x, FALSE, FALSE, 4);

    graphwindow->label_y = gtk_label_new("          ");
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->label_y, FALSE, FALSE, 4);


    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    graphwindow->measure_dialog = GWY_GRAPH_WINDOW_MEASURE_DIALOG(
                                                gwy_graph_window_measure_dialog_new(GWY_GRAPH(graphwindow->graph)));
    g_signal_connect_swapped(graphwindow->measure_dialog, "response",
                           G_CALLBACK(gwy_graph_window_measure_finished_cb),
                           graphwindow);

    /*
    gwy_graph_window_set_tooltip(button, _("Show full controls"));

    g_object_set_data(G_OBJECT(button), "gwy3dwindow", gwy3dwindow);
    g_signal_connect_swapped(button, "clicked",
                             G_CALLBACK(gwy_graph_window_select_controls),
                             GINT_TO_POINTER(FALSE));
     */

    g_signal_connect_swapped(graphwindow->graph, "mouse-moved",
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

/**
 * gwy_graph_window_class_set_tooltips:
 * @tips: Tooltips object #GwyGraphWindow's should use for setting tooltips.
 *        A %NULL value disables tooltips altogether.
 *
 * Sets the tooltips object to use for adding tooltips to graph window parts.
 *
 * This is a class method.  It affects only newly cerated graph windows,
 * existing graph windows will continue to use the tooltips they were
 * constructed with.
 *
 * If no class tooltips object is set before first #GwyGraphWindow is created,
 * the class instantiates one on its own.  You can normally obtain it with
 * gwy_graph_window_class_get_tooltips() then.  The class takes a reference on
 * the tooltips in either case.
 **/
void
gwy_graph_window_class_set_tooltips(GtkTooltips *tips)
{
    g_return_if_fail(!tips || GTK_IS_TOOLTIPS(tips));

    if (tips) {
        g_object_ref(tips);
        gtk_object_sink(GTK_OBJECT(tips));
    }
    gwy_object_unref(tooltips);
    tooltips = tips;
    tooltips_set = TRUE;
}

/**
 * gwy_graph_window_class_get_tooltips:
 *
 * Gets the tooltips object used for adding tooltips to Graph window parts.
 *
 * Returns: The #GtkTooltips object.
 **/
GtkTooltips*
gwy_graph_window_class_get_tooltips(void)
{
    return tooltips;
}

static void
gwy_graph_cursor_motion_cb(GwyGraphWindow *graphwindow)
{
    gdouble x, y;
    gchar buffer[100];
    gdouble xmag, ymag;
    GString *xstring, *ystring;

    gwy_graph_get_cursor(GWY_GRAPH(graphwindow->graph), &x, &y);

    xmag = gwy_axis_get_magnification(GWY_GRAPH(graphwindow->graph)->axis_top);
    xstring = gwy_axis_get_magnification_string(GWY_GRAPH(graphwindow->graph)->axis_top);

    ymag = gwy_axis_get_magnification(GWY_GRAPH(graphwindow->graph)->axis_left);
    ystring = gwy_axis_get_magnification_string(GWY_GRAPH(graphwindow->graph)->axis_left);


    g_snprintf(buffer, sizeof(buffer), "%.4f", x/xmag);
    xstring = g_string_prepend(xstring, buffer);
    gtk_label_set_markup(GTK_LABEL(graphwindow->label_x), xstring->str);

    g_snprintf(buffer, sizeof(buffer), "%.4f", y/ymag);
    ystring = g_string_prepend(ystring, buffer);
    gtk_label_set_markup(GTK_LABEL(graphwindow->label_y), ystring->str);

    g_string_free(xstring, TRUE);
    g_string_free(ystring, TRUE);
}

static void
gwy_graph_window_measure_cb(GwyGraphWindow *graphwindow)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(graphwindow->button_zoom_in), FALSE);
    gwy_graph_set_status(GWY_GRAPH(graphwindow->graph), GWY_GRAPH_STATUS_XLINES);
    gtk_widget_queue_draw(GTK_WIDGET(graphwindow->graph));
    gwy_graph_signal_selected(GWY_GRAPH(graphwindow->graph));
    gtk_widget_show_all(GTK_WIDGET(graphwindow->measure_dialog));
}


static void
gwy_graph_window_measure_finished_cb(GwyGraphWindow *graphwindow, gint response)
{

    gwy_graph_clear_selection(GWY_GRAPH(graphwindow->graph));
    if (response == GWY_GRAPH_WINDOW_MEASURE_RESPONSE_CLEAR)
        return;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(graphwindow->button_zoom_in), FALSE);
    gwy_graph_set_status(GWY_GRAPH(graphwindow->graph), GWY_GRAPH_STATUS_PLAIN);

    gtk_widget_queue_draw(GTK_WIDGET(graphwindow->graph));
    gtk_widget_hide(GTK_WIDGET(graphwindow->measure_dialog));

}

static void
gwy_graph_window_zoom_in_cb(GwyGraphWindow *graphwindow)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(graphwindow->button_zoom_in)))
    {
        last_status = gwy_graph_get_status(GWY_GRAPH(graphwindow->graph));
        gwy_graph_zoom_in(GWY_GRAPH(graphwindow->graph));
    }
    else
        gwy_graph_set_status(GWY_GRAPH(graphwindow->graph), last_status);


}

static void
gwy_graph_window_zoom_out_cb(GwyGraphWindow *graphwindow)
{
    gwy_graph_zoom_out(GWY_GRAPH(graphwindow->graph));
}

static void
gwy_graph_window_zoom_finished_cb(GwyGraphWindow *graphwindow)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(graphwindow->button_zoom_in), FALSE);
    gwy_graph_set_status(GWY_GRAPH(graphwindow->graph), last_status);
}


static void 
gwy_graph_window_x_log_cb(GwyGraphWindow *graphwindow)
{
    gwy_graph_model_set_direction_logarithmic(GWY_GRAPH(graphwindow->graph)->graph_model, 
                                              GTK_ORIENTATION_HORIZONTAL,
                   !gwy_graph_model_get_direction_logarithmic(GWY_GRAPH(graphwindow->graph)->graph_model,
                                                                           GTK_ORIENTATION_HORIZONTAL));
}

static void 
gwy_graph_window_y_log_cb(GwyGraphWindow *graphwindow)
{
    gwy_graph_model_set_direction_logarithmic(GWY_GRAPH(graphwindow->graph)->graph_model, 
                                              GTK_ORIENTATION_VERTICAL,
                   !gwy_graph_model_get_direction_logarithmic(GWY_GRAPH(graphwindow->graph)->graph_model,
                                                                           GTK_ORIENTATION_VERTICAL));
}


static void
gwy_graph_window_export_vector_cb(GwyGraphWindow *graphwindow)
{
    GtkDialog *filedialog;
    gchar *filename;

    filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Export to postscript",
                                              GTK_WINDOW(graphwindow),
                                              GTK_FILE_CHOOSER_ACTION_SAVE,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                              NULL));
    if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
    {
       filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
       gwy_graph_export_postscript(GWY_GRAPH(graphwindow->graph), filename,
                                   TRUE, TRUE, TRUE);
    }
    gtk_widget_destroy(GTK_WIDGET(filedialog));

}

static void
gwy_graph_window_export_ascii_cb(GwyGraphWindow *graphwindow)
{
    GwyGraphWindowAsciiDialog *dialog;
    GtkDialog *filedialog;
    gchar *filename;
    gboolean units, labels, metadata;
    GwyGraphModelExportStyle style;
    gint result;

    dialog = GWY_GRAPH_WINDOW_ASCII_DIALOG(gwy_graph_window_ascii_dialog_new());
    result = gtk_dialog_run (GTK_DIALOG (dialog));
    switch (result)
    {
       case GTK_RESPONSE_OK:
       gwy_graph_window_ascii_dialog_get_data(dialog,
                                              &style,
                                              &units,
                                              &labels,
                                              &metadata);
       filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Export to ASCII File",
                                                 GTK_WINDOW(graphwindow),
                                                 GTK_FILE_CHOOSER_ACTION_SAVE,
                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                 NULL));
       if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
       {
           filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
           gwy_graph_model_export_ascii(GWY_GRAPH(graphwindow->graph)->graph_model, filename,
                                        units, labels, metadata,
                                        style);

       }
       gtk_widget_destroy(GTK_WIDGET(filedialog));


       break;

       default:
       break;
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));

}

static void
gwy_graph_window_export_bitmap_cb(GwyGraphWindow *graphwindow)
{
    GtkDialog *filedialog;
    gchar *filename;

    filedialog = GTK_DIALOG(gtk_file_chooser_dialog_new ("Export to bitmap",
                                              GTK_WINDOW(graphwindow),
                                              GTK_FILE_CHOOSER_ACTION_SAVE,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                              NULL));
    if (gtk_dialog_run (GTK_DIALOG (filedialog)) == GTK_RESPONSE_ACCEPT)
    {
       filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filedialog));
       gwy_graph_export_pixmap(GWY_GRAPH(graphwindow->graph), filename,
                               TRUE, TRUE, TRUE);
    }
    gtk_widget_destroy(GTK_WIDGET(filedialog));
}

static void
gwy_graph_window_set_tooltip(GtkWidget *widget,
                             const gchar *tip_text)
{
    if (tooltips)
        gtk_tooltips_set_tip(tooltips, widget, tip_text, NULL);
}


/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
