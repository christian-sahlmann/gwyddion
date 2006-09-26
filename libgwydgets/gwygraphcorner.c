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
#include <gtk/gtkwidget.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwygraphcorner.h>

static void     gwy_graph_corner_realize       (GtkWidget *widget);
static void     gwy_graph_corner_size_request  (GtkWidget *widget,
                                                GtkRequisition *requisition);
static void     gwy_graph_corner_size_allocate (GtkWidget *widget,
                                                GtkAllocation *allocation);
static gboolean gwy_graph_corner_expose        (GtkWidget *widget,
                                                GdkEventExpose *event);

G_DEFINE_TYPE(GwyGraphCorner, gwy_graph_corner, GTK_TYPE_WIDGET)

static void
gwy_graph_corner_class_init(GwyGraphCornerClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    widget_class->realize = gwy_graph_corner_realize;
    widget_class->expose_event = gwy_graph_corner_expose;
    widget_class->size_request = gwy_graph_corner_size_request;
    widget_class->size_allocate = gwy_graph_corner_size_allocate;
}

static void
gwy_graph_corner_init(G_GNUC_UNUSED GwyGraphCorner *graph_corner)
{
}

/**
 * gwy_graph_corner_new:
 *
 * Creates a new graph corner.
 *
 * Returns: A new graph corner as a #GtkWidget.
 **/
GtkWidget*
gwy_graph_corner_new()
{
    GwyGraphCorner *graph_corner;

    graph_corner = g_object_new(GWY_TYPE_GRAPH_CORNER, NULL);

    return GTK_WIDGET(graph_corner);
}

static void
gwy_graph_corner_realize(GtkWidget *widget)
{
    GwyGraphCorner *graph_corner;
    GdkWindowAttr attributes;
    gint i, attributes_mask;
    GtkStyle *style;

    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
    graph_corner = GWY_GRAPH_CORNER(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(widget->parent->window,
                                    &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);

    /* set background to white forever */
    style = gtk_style_copy(widget->style);
    for (i = 0; i < 5; i++) {
        style->bg_gc[i] = widget->style->white_gc;
        style->bg[i] = widget->style->white;
    }
    gtk_style_set_background(style, widget->window, GTK_STATE_NORMAL);
    g_object_unref(style);
}

static void
gwy_graph_corner_size_request(G_GNUC_UNUSED GtkWidget *widget,
                              GtkRequisition *requisition)
{
    requisition->width = 0;
    requisition->height = 0;
}

static void
gwy_graph_corner_size_allocate(GtkWidget *widget,
                               GtkAllocation *allocation)
{
    GwyGraphCorner *graph_corner;

    widget->allocation = *allocation;

    if (GTK_WIDGET_REALIZED(widget)) {
        graph_corner = GWY_GRAPH_CORNER(widget);

        gdk_window_move_resize(widget->window,
                               allocation->x, allocation->y,
                               allocation->width, allocation->height);
    }
}

static gboolean
gwy_graph_corner_expose(GtkWidget *widget,
                       GdkEventExpose *event)
{
    GwyGraphCorner *graph_corner;

    if (event->count > 0)
        return FALSE;

    graph_corner = GWY_GRAPH_CORNER(widget);

    gdk_window_clear_area(widget->window,
                          0, 0,
                          widget->allocation.width,
                          widget->allocation.height);

    return FALSE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwygraphcorner
 * @title: GwyGraphCorner
 * @short_description: Graph corners
 *
 * #GwyGraphCorner is a part of #GwyGraph and it currently has no
 * functionality.  It is reserved for future.  It will be probably used for
 * some graph options quick accesibility.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
