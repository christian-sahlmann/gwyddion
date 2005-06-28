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

#ifndef __GTK_GRAPH_H__
#define __GTK_GRAPH_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktable.h>

#include <libprocess/dataline.h>

#include <libgwydgets/gwyaxis.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphbasics.h>
#include <libgwydgets/gwygraphlabel.h>
#include <libgwydgets/gwygraphcorner.h>
#include <libgwydgets/gwygrapharea.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH            (gwy_graph_get_type())
#define GWY_GRAPH(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_GRAPH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_IS_GRAPH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH))
#define GWY_IS_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH))
#define GWY_GRAPH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH, GwyGraphClass))

typedef struct _GwyGraph      GwyGraph;
typedef struct _GwyGraphClass GwyGraphClass;


struct _GwyGraph {
    GtkTable table;

    GwyAxis *axis_top;
    GwyAxis *axis_left;
    GwyAxis *axis_right;
    GwyAxis *axis_bottom;

    GwyGraphCorner *corner_tl;
    GwyGraphCorner *corner_bl;
    GwyGraphCorner *corner_tr;
    GwyGraphCorner *corner_br;

    GwyGraphArea *area;

    GwyGraphModel *graph_model;

    gboolean enable_user_input;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyGraphClass {
    GtkTableClass parent_class;

    void (*gwygraph)(GwyGraph *grapher);
    void (*selected)(GwyGraph *grapher);
    void (*mousemoved)(GwyGraph *grapher);    
    void (*zoomed)(GwyGraph *grapher);
    
    gpointer reserved1;
    gpointer reserved2;
};

GtkWidget *gwy_graph_new(GwyGraphModel *gmodel);
GType      gwy_graph_get_type(void) G_GNUC_CONST;

void       gwy_graph_refresh(GwyGraph *grapher);
void       gwy_graph_refresh_and_reset(GwyGraph *grapher);

void       gwy_graph_change_model(GwyGraph *grapher, 
                                    GwyGraphModel *gmodel);
void       gwy_graph_set_status(GwyGraph *grapher,
                                  GwyGraphStatusType status);
GwyGraphStatusType  gwy_graph_get_status(GwyGraph *grapher);

GwyGraphModel *gwy_graph_get_model(GwyGraph *grapher);

void       gwy_graph_signal_selected(GwyGraph *grapher);
void       gwy_graph_signal_mousemoved(GwyGraph *grapher);
void       gwy_graph_signal_zoomed(GwyGraph *grapher);

gint       gwy_graph_get_selection_number(GwyGraph *grapher);
void       gwy_graph_get_selection(GwyGraph *grapher,
                                     gdouble *selection);

void       gwy_graph_clear_selection(GwyGraph *grapher);

void       gwy_graph_get_cursor(GwyGraph *grapher,
                                  gdouble *x_cursor, gdouble *y_cursor);

void       gwy_graph_request_x_range(GwyGraph *grapher, gdouble x_min_req, gdouble x_max_req);
void       gwy_graph_request_y_range(GwyGraph *grapher, gdouble y_min_req, gdouble y_max_req);
void       gwy_graph_get_x_range(GwyGraph *grapher, gdouble *x_min, gdouble *x_max);
void       gwy_graph_get_y_range(GwyGraph *grapher, gdouble *y_min, gdouble *y_max);

void       gwy_graph_enable_user_input(GwyGraph *grapher, gboolean enable);


void       gwy_graph_export_pixmap(GwyGraph *grapher, const gchar *filename, 
                                     gboolean export_title, gboolean export_axis,
                                     gboolean export_labels);
void       gwy_graph_export_postscript(GwyGraph *grapher, const gchar *filename,
                                         gboolean export_title, gboolean export_axis,
                                         gboolean export_labels);

void       gwy_graph_zoom_in(GwyGraph *grapher);
void       gwy_graph_zoom_out(GwyGraph *grapher);


G_END_DECLS

#endif /* __GWY_GRADSPHERE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
