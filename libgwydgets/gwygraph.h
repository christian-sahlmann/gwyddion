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

#ifndef __GTK_PLOT_H__
#define __GTK_PLOT_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktable.h>

#include <libprocess/dataline.h>

#include "gwyaxis.h"
#include "gwygrapharea.h"
#include "gwygraphcorner.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_GRAPH            (gwy_graph_get_type())
#define GWY_GRAPH(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_GRAPH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_IS_GRAPH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH))
#define GWY_IS_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH))
#define GWY_GRAPH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH, GwyGraphClass))

typedef struct _GwyGraph      GwyGraph;
typedef struct _GwyGraphClass GwyGraphClass;

typedef struct {
   gboolean is_line;
   gboolean is_point;
   gint line_size;
   gint point_size;
   GdkColor color;
} GwyGraphAutoProperties;


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

   gint n_of_curves;
   gint n_of_autocurves;

   GwyGraphAutoProperties autoproperties;

   gdouble x_max, x_reqmax;
   gdouble x_min, x_reqmin;
   gdouble y_max, y_reqmax;
   gdouble y_min, y_reqmin;

   gboolean has_x_unit;
   gboolean has_y_unit;
   gchar *x_unit;
   gchar *y_unit;

   GwyGraphStatus_SelData seldata;
   GwyGraphStatus_PointsData pointsdata;

};

struct _GwyGraphClass {
   GtkTableClass parent_class;

   void (*gwygraph)(GwyGraph *graph);
};

GtkWidget *gwy_graph_new();
GType      gwy_graph_get_type(void) G_GNUC_CONST;


/*basic interfaces*/

void gwy_graph_add_dataline_with_units(GwyGraph *graph, GwyDataLine *dataline,
                              gdouble shift, GString *label, GwyGraphAreaCurveParams *params,
            gdouble x_order, gdouble y_order, char *x_unit, char *y_unit);

void gwy_graph_add_dataline(GwyGraph *graph, GwyDataLine *dataline,
                              gdouble shift, GString *label, GwyGraphAreaCurveParams *params);

void gwy_graph_add_datavalues(GwyGraph *graph, gdouble *xvals, gdouble *yvals,
                              gint n, GString *label, GwyGraphAreaCurveParams *params);

void gwy_graph_clear(GwyGraph *graph);
void gwy_graph_set_autoproperties(GwyGraph *graph, GwyGraphAutoProperties *autoproperties);
void gwy_graph_get_autoproperties(GwyGraph *graph, GwyGraphAutoProperties *autoproperties);

void gwy_graph_export_ascii(GwyGraph *graph, char *filename);

/*graph status (selections enabled) handling*/

void               gwy_graph_set_status(GwyGraph *graph,
          GwyGraphStatusType status);

GwyGraphStatusType gwy_graph_get_status(GwyGraph *graph);

gpointer           gwy_graph_get_status_data(GwyGraph *graph);


void gwy_graph_get_boundaries(GwyGraph *graph, gdouble *x_min, gdouble *x_max, gdouble *y_min, gdouble *y_max);
void gwy_graph_set_boundaries(GwyGraph *graph, gdouble x_min, gdouble x_max, gdouble y_min, gdouble y_max);

void gwy_graph_unzoom(GwyGraph *graph);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GRADSPHERE_H__ */

