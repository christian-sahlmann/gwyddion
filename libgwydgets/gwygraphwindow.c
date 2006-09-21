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
#include <libgwydgets/gwygraphwindow.h>
#include <libgwydgets/gwygraphdata.h>
#include <libgwydgets/gwystock.h>
#include <libgwydgets/gwystatusbar.h>
#include "gwygraphwindowmeasuredialog.h"

#define DEFAULT_SIZE 360

/* Forward declarations */

static void gwy_graph_window_destroy            (GtkObject *object);
static void gwy_graph_window_finalize           (GObject *object);
static gboolean gwy_graph_window_key_pressed    (GtkWidget *widget,
                                                 GdkEventKey *event);
static gboolean gwy_graph_cursor_motion_cb      (GwyGraphWindow *graphwindow);
static void gwy_graph_window_measure_cb         (GwyGraphWindow *graphwindow);
static void gwy_graph_window_zoom_in_cb         (GwyGraphWindow *graphwindow);
static void gwy_graph_window_zoom_out_cb        (GwyGraphWindow *graphwindow);
static void gwy_graph_window_x_log_cb           (GwyGraphWindow *graphwindow);
static void gwy_graph_window_y_log_cb           (GwyGraphWindow *graphwindow);
static void gwy_graph_window_zoom_finished_cb   (GwyGraphWindow *graphwindow);
static void gwy_graph_window_measure_finished_cb(GwyGraphWindow *graphwindow,
                                                 gint response);
static void gwy_graph_window_set_tooltip        (GtkWidget *widget,
                                                 const gchar *tip_text);
static void graph_title_changed                 (GwyGraphWindow *graphwindow);


/* Local data */

/* These are actually class data.  To put them to Class struct someone would
 * have to do class_ref() and live with this reference to the end of time. */
static GtkTooltips *tooltips = NULL;
static gboolean tooltips_set = FALSE;

G_DEFINE_TYPE(GwyGraphWindow, gwy_graph_window, GTK_TYPE_WINDOW)

static void
gwy_graph_window_class_init(GwyGraphWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
    GtkObjectClass *object_class = GTK_OBJECT_CLASS(klass);

    gobject_class->finalize = gwy_graph_window_finalize;

    object_class->destroy = gwy_graph_window_destroy;

    widget_class->key_press_event = gwy_graph_window_key_pressed;
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
 * Creates a new window showing @graph.
 *
 * Returns: A newly created graph window as #GtkWidget.
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
    graphwindow->last_status = gwy_graph_get_status(graph);

    /*add notebook with graph and text matrix*/
    graphwindow->notebook = gtk_notebook_new();

    graph_title_changed(graphwindow);

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

    graphwindow->button_measure_points = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_measure_points),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_MEASURE,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_measure_points,
                       FALSE, FALSE, 0);
    gwy_graph_window_set_tooltip(graphwindow->button_measure_points,
                                 _("Measure distances in graph"));
    g_signal_connect_swapped(graphwindow->button_measure_points, "clicked",
                           G_CALLBACK(gwy_graph_window_measure_cb),
                           graphwindow);


    graphwindow->button_zoom_in = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_zoom_in),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_ZOOM_IN,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_zoom_in,
                       FALSE, FALSE, 0);
    gwy_graph_window_set_tooltip(graphwindow->button_zoom_in,
                                 _("Zoom in by mouse selection"));
    g_signal_connect_swapped(graphwindow->button_zoom_in, "toggled",
                           G_CALLBACK(gwy_graph_window_zoom_in_cb),
                           graphwindow);

    graphwindow->button_zoom_out = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_zoom_out),
                      gtk_image_new_from_stock(GWY_STOCK_GRAPH_ZOOM_FIT,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_zoom_out,
                       FALSE, FALSE, 0);
    gwy_graph_window_set_tooltip(graphwindow->button_zoom_out,
                                 _("Zoom out to full curve"));
    g_signal_connect_swapped(graphwindow->button_zoom_out, "clicked",
                           G_CALLBACK(gwy_graph_window_zoom_out_cb),
                           graphwindow);


    graphwindow->button_x_log = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_x_log),
                      gtk_image_new_from_stock(GWY_STOCK_LOGSCALE_HORIZONTAL,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_x_log,
                       FALSE, FALSE, 0);
    g_signal_connect_swapped(graphwindow->button_x_log, "clicked",
                           G_CALLBACK(gwy_graph_window_x_log_cb),
                           graphwindow);

    gtk_widget_set_sensitive(graphwindow->button_x_log,
                gwy_graph_model_x_data_can_be_logarithmed(graph->graph_model));

    graphwindow->button_y_log = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(graphwindow->button_y_log),
                      gtk_image_new_from_stock(GWY_STOCK_LOGSCALE_VERTICAL,
                                               GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->button_y_log,
                       FALSE, FALSE, 0);
    g_signal_connect_swapped(graphwindow->button_y_log, "clicked",
                           G_CALLBACK(gwy_graph_window_y_log_cb),
                           graphwindow);

    gtk_widget_set_sensitive(graphwindow->button_y_log,
                gwy_graph_model_y_data_can_be_logarithmed(graph->graph_model));


    graphwindow->statusbar = gwy_statusbar_new();
    gtk_widget_set_name(graphwindow->statusbar, "gwyflatstatusbar");
    gtk_box_pack_start(GTK_BOX(hbox), graphwindow->statusbar, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    graphwindow->measure_dialog
        = _gwy_graph_window_measure_dialog_new(graph);
    g_signal_connect_swapped(graphwindow->measure_dialog, "response",
                           G_CALLBACK(gwy_graph_window_measure_finished_cb),
                           graphwindow);

    g_signal_connect_swapped(gwy_graph_get_area(graph),
                             "motion-notify-event",
                             G_CALLBACK(gwy_graph_cursor_motion_cb),
                             graphwindow);

    g_signal_connect_swapped(gwy_graph_area_get_selection(
                                       GWY_GRAPH_AREA(gwy_graph_get_area(graph)),
                                       GWY_GRAPH_STATUS_ZOOM), "finished",
                             G_CALLBACK(gwy_graph_window_zoom_finished_cb),
                             graphwindow);


    if (gwy_graph_get_model(GWY_GRAPH(graphwindow->graph)))
        g_signal_connect_swapped(gwy_graph_get_model(graph), "notify::title",
                                 G_CALLBACK(graph_title_changed), graphwindow);

    return GTK_WIDGET(graphwindow);
}

/**
 * gwy_graph_window_get_graph:
 * @graphwindow: a graph window.
 *
 * Returns the #GwyGraph widget this graph window currently shows.
 *
 * Returns: The currently shown #GwyGraph widget. (Do not free).
 **/
GtkWidget*
gwy_graph_window_get_graph(GwyGraphWindow *graphwindow)
{
    g_return_val_if_fail(GWY_IS_GRAPH_WINDOW(graphwindow), NULL);

    return graphwindow->graph;
}

/*XXX: the "tips" line below is hard to understand */
/**
 * gwy_graph_window_class_set_tooltips:
 * @tips: #GtkTooltips object #GwyGraphWindow should use for setting tooltips.
 *        A %NULL value disables tooltips altogether.
 *
 * Sets the tooltips object to use for adding tooltips to graph window parts.
 *
 * This is a class method.  It affects only newly created graph windows,
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
 * Returns: The #GtkTooltips object. (Do not free).
 **/
GtkTooltips*
gwy_graph_window_class_get_tooltips(void)
{
    return tooltips;
}

static void
gwy_graph_window_copy_to_clipboard(GwyGraphWindow *graph_window)
{
    GtkClipboard *clipboard;
    GdkDisplay *display;
    GdkPixbuf *pixbuf;
    GdkAtom atom;

    display = gtk_widget_get_display(GTK_WIDGET(graph_window));
    atom = gdk_atom_intern("CLIPBOARD", FALSE);
    clipboard = gtk_clipboard_get_for_display(display, atom);
    pixbuf = gwy_graph_export_pixmap(GWY_GRAPH(graph_window->graph),
                                     FALSE, TRUE, TRUE);
    gtk_clipboard_set_image(clipboard, pixbuf);
    g_object_unref(pixbuf);
}

static gboolean
gwy_graph_window_key_pressed(GtkWidget *widget,
                             GdkEventKey *event)
{
    enum {
        important_mods = GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_RELEASE_MASK
    };
    GwyGraphWindow *graph_window;
    gboolean (*method)(GtkWidget*, GdkEventKey*);
    guint state, key;

    gwy_debug("state = %u, keyval = %u", event->state, event->keyval);
    graph_window = GWY_GRAPH_WINDOW(widget);
    state = event->state & important_mods;
    key = event->keyval;
    /* TODO: it would be nice to have these working too
    if (!state && (key == GDK_minus || key == GDK_KP_Subtract))
        gwy_graph_window_set_zoom(graph_window, -1);
    else if (!state && (key == GDK_equal || key == GDK_KP_Equal
                        || key == GDK_plus || key == GDK_KP_Add))
        gwy_graph_window_set_zoom(graph_window, 1);
    else if (!state && (key == GDK_Z || key == GDK_z || key == GDK_KP_Divide))
        gwy_graph_window_set_zoom(graph_window, 10000);
    else */
    if (state == GDK_CONTROL_MASK && (key == GDK_C || key == GDK_c)) {
        gwy_graph_window_copy_to_clipboard(graph_window);
        return TRUE;
    }

    method = GTK_WIDGET_CLASS(gwy_graph_window_parent_class)->key_press_event;
    return method ? method(widget, event) : FALSE;
}

static gboolean
gwy_graph_cursor_motion_cb(GwyGraphWindow *graphwindow)
{
    const gchar* xstring, *ystring;
    GwyGraph *graph;
    GwyAxis *axis;
    gdouble x, y;
    gchar buffer[200];
    gdouble xmag, ymag;

    graph = GWY_GRAPH(graphwindow->graph);
    gwy_graph_area_get_cursor(GWY_GRAPH_AREA(gwy_graph_get_area(graph)),
                              &x, &y);

    axis = gwy_graph_get_axis(graph, GTK_POS_TOP);
    xmag = gwy_axis_get_magnification(axis);
    xstring = gwy_axis_get_magnification_string(axis);

    axis = gwy_graph_get_axis(graph, GTK_POS_LEFT);
    ymag = gwy_axis_get_magnification(axis);
    ystring = gwy_axis_get_magnification_string(axis);

    g_snprintf(buffer, sizeof(buffer), "%.4f %s, %.4f %s",
               x/xmag, xstring, y/ymag, ystring);
    gwy_statusbar_set_markup(GWY_STATUSBAR(graphwindow->statusbar), buffer);

    return FALSE;
}

static void
gwy_graph_window_measure_cb(GwyGraphWindow *graphwindow)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(graphwindow->button_measure_points)))
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(graphwindow->button_zoom_in), FALSE);
        gwy_graph_set_status(GWY_GRAPH(graphwindow->graph), GWY_GRAPH_STATUS_XLINES);
        gtk_widget_queue_draw(GTK_WIDGET(graphwindow->graph));
        gtk_widget_show_all(GTK_WIDGET(graphwindow->measure_dialog));
    }
    else
    {
        gwy_graph_window_measure_finished_cb(graphwindow, 0);
    }
}


static void
gwy_graph_window_measure_finished_cb(GwyGraphWindow *graphwindow, gint response)
{

    gwy_selection_clear(gwy_graph_area_get_selection
                        (GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(graphwindow->graph))),
                         GWY_GRAPH_STATUS_POINTS));
    gwy_selection_clear(gwy_graph_area_get_selection
                        (GWY_GRAPH_AREA(gwy_graph_get_area(GWY_GRAPH(graphwindow->graph))),
                         GWY_GRAPH_STATUS_XLINES));

    if (response == GWY_GRAPH_WINDOW_MEASURE_RESPONSE_CLEAR)
        return;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(graphwindow->button_measure_points), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(graphwindow->button_zoom_in), FALSE);
    gwy_graph_set_status(GWY_GRAPH(graphwindow->graph), GWY_GRAPH_STATUS_PLAIN);

    gtk_widget_queue_draw(GTK_WIDGET(graphwindow->graph));
    gtk_widget_hide(GTK_WIDGET(graphwindow->measure_dialog));

}

static void
gwy_graph_window_zoom_in_cb(GwyGraphWindow *graphwindow)
{
    GwyGraph *graph;

    graph = GWY_GRAPH(graphwindow->graph);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(graphwindow->button_zoom_in))) {
        graphwindow->last_status = gwy_graph_get_status(graph);
        gwy_graph_zoom_in(graph);
    }
    else
        gwy_graph_set_status(graph, graphwindow->last_status);
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
    gwy_graph_set_status(GWY_GRAPH(graphwindow->graph), graphwindow->last_status);
}


static void
gwy_graph_window_x_log_cb(GwyGraphWindow *graphwindow)
{
    GwyGraphModel *model;
    gboolean state;

    model = gwy_graph_get_model(GWY_GRAPH(graphwindow->graph));
    g_object_get(model, "x-logarithmic", &state, NULL);
    g_object_set(model, "x-logarithmic", !state, NULL);
}

static void
gwy_graph_window_y_log_cb(GwyGraphWindow *graphwindow)
{
    GwyGraphModel *model;
    gboolean state;

    model = gwy_graph_get_model(GWY_GRAPH(graphwindow->graph));
    g_object_get(model, "y-logarithmic", &state, NULL);
    g_object_set(model, "y-logarithmic", !state, NULL);
}

static void
gwy_graph_window_set_tooltip(GtkWidget *widget,
                             const gchar *tip_text)
{
    if (tooltips)
        gtk_tooltips_set_tip(tooltips, widget, tip_text, NULL);
}

static void
graph_title_changed(GwyGraphWindow *graphwindow)
{
    GwyGraphModel *gmodel;
    gchar *title;

    gmodel = gwy_graph_get_model(GWY_GRAPH(graphwindow->graph));
    g_object_get(gmodel, "title", &title, NULL);

    /* FIXME: Can it be NULL? */
    if (title)
        gtk_window_set_title(GTK_WINDOW(graphwindow), title);
    else
        gtk_window_set_title(GTK_WINDOW(graphwindow), _("Untitled"));

    g_free(title);
}


/************************** Documentation ****************************/

/**
 * SECTION:gwygraphwindow
 * @title: GwyGraphWindow
 * @short_description: Graph display window
 *
 * #GwyGraphWindow encapsulates a #GwyGraph together with other controls and
 * graph data view.
 * You can create a graph window for a graph with gwy_graph_window_new().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
