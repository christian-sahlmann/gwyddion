/* @(#) $Id$ */

#ifndef __GTK_PLOT_H__
#define __GTK_PLOT_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktable.h>

#include "gwyaxis.h"
#include "gwygrapharea.h"
#include "gwygraphcorner.h"
#include "../libprocess/dataline.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_GRAPH            (gwy_graph_get_type())
#define GWY_GRAPH(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_GRAPH_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH, GwyGraph))
#define GWY_IS_GRAPH(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH))
#define GWY_IS_GRAPH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH))
#define GWY_GRAPH_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH, GwyGraph))

typedef struct {
   gboolean is_line;
   gboolean is_point;
   gint line_size;
   gint point_size;
   GdkColor color;
} GwyGraphAutoProperties;

typedef struct {
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

   gdouble x_max;
   gdouble x_min;
   gdouble y_max;
   gdouble y_min;
    
} GwyGraph;

typedef struct {
   GtkTableClass parent_class;
   
   void (* gwygraph) (GwyGraph *graph);
} GwyGraphClass;

GtkWidget *gwy_graph_new();
  
void gwy_graph_add_dataline(GwyGraph *graph, GwyDataLine *dataline, gdouble shift);
void gwy_graph_add_datavalues(GwyGraph *graph, gdouble *xvals, gdouble *yvals, gint n);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_GRADSPHERE_H__ */

