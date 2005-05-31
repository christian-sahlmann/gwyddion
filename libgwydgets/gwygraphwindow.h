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

#ifndef __GWY_GRAPHWINDOW_H__
#define __GWY_GRAPHWINDOW_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktooltips.h>

#include <libgwydgets/gwygrapher.h>
#include <libgwydgets/gwygraphwindowmeasuredialog.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_WINDOW            (gwy_graph_window_get_type())
#define GWY_GRAPH_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_WINDOW, GwyGraphWindow))
#define GWY_GRAPH_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_WINDOW, GwyGraphWindowClass))
#define GWY_IS_GRAPH_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_WINDOW))
#define GWY_IS_GRAPH_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_WINDOW))
#define GWY_GRAPH_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_WINDOW, GwyGraphWindowClass))

typedef struct _GwyGraphWindow      GwyGraphWindow;
typedef struct _GwyGraphWindowClass GwyGraphWindowClass;

struct _GwyGraphWindow {
    GtkWindow parent_instance;

    GwyZoomMode zoom_mode;  /* reserved for future use */

    GtkWidget *notebook;
    GtkWidget *graph;
    GtkWidget *data;

    GwyGrapherWindowMeasureDialog *measure_dialog;

    GtkWidget *button_measure_points;
    GtkWidget *button_measure_lines;
    
    GtkWidget *button_zoom_in;
    GtkWidget *button_zoom_out;
    
    GtkWidget *button_export_ascii;
    GtkWidget *button_export_bitmap;
    GtkWidget *button_export_vector;

    GtkWidget *label_what;
    GtkWidget *label_x;
    GtkWidget *label_y;
    
    GtkTooltips *tips;

    GtkWidget *widget1;
    GtkWidget *widget2;
    GtkWidget *widget3;
    GtkWidget *widget4;
    GtkWidget *widget5;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

struct _GwyGraphWindowClass {
    GtkWindowClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
};

GtkWidget* gwy_graph_window_new                      (GwyGrapher *graph);
GType      gwy_graph_window_get_type                 (void) G_GNUC_CONST;
GtkWidget* gwy_graph_window_get_graph              (GwyGraphWindow *graphwindow);

G_END_DECLS

#endif /* __GWY_3DWINDOW_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
