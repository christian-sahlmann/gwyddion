/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@physics.muni.cz, klapetek@physics.muni.cz.
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

#ifndef __GWY_GRAPH_AREA_H__
#define __GWY_GRAPH_AREA_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtklayout.h>

#include "gwygraphlabel.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_GRAPH_AREA            (gwy_graph_area_get_type())
#define GWY_GRAPH_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_AREA, GwyGraphArea))
#define GWY_GRAPH_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_AREA, GwyGraphArea))
#define GWY_IS_GRAPH_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_AREA))
#define GWY_IS_GRAPH_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_AREA))
#define GWY_GRAPH_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_AREA, GwyGraphAreaClass))


/*single curve data*/
typedef struct {
    gdouble *xvals;     /*screen coordinates of points*/
    gdouble *yvals;     /*screen coordinates of points*/
    gint N;              /*number of points*/
} GwyGraphAreaCurvePoints;

typedef enum {
  GWY_GRAPH_STATUS_PLAIN  = 0,
  GWY_GRAPH_STATUS_CURSOR = 1,
  GWY_GRAPH_STATUS_XSEL   = 2,
  GWY_GRAPH_STATUS_YSEL   = 3,
  GWY_GRAPH_STATUS_POINTS = 4,
  GWY_GRAPH_STATUS_ZOOM   = 5
} GwyGraphStatusType;

typedef struct {
   gdouble x;
   gdouble y;
   char *x_unit;
   char *y_unit;
} GwyGraphDataPoint;

typedef struct {
   gdouble i;
   gdouble j;
} GwyGraphScrPoint;

typedef struct {
   GwyGraphScrPoint scr_point;
   GwyGraphDataPoint data_point;
} GwyGraphStatus_CursorData;

typedef struct {
   gint scr_start;
   gint scr_end;
   gdouble data_start;
   gdouble data_end;
} GwyGraphStatus_SelData;

typedef struct {
   GArray *scr_points;
   GArray *data_points;
   gint n;
} GwyGraphStatus_PointsData;

typedef struct {
  gint x;
  gint y;
  gint width;
  gint height;
  gdouble xmin;
  gdouble xmax;
  gdouble ymin;
  gdouble ymax;
} GwyGraphStatus_ZoomData;

/*NOTE: GwyGraphAreaCurveParams is defined in gwygraphlabel.h*/

/*single curve*/
typedef struct {
    GwyGraphAreaCurvePoints data;       /*original data including its size*/
    GwyGraphAreaCurveParams params;     /*parameters of plot*/
    GdkPoint *points;			/*points to be directly plotted*/
} GwyGraphAreaCurve;

/*overall properties of area*/
typedef struct {
    int ble; 
} GwyGraphAreaParams;

/*graph area structure*/
typedef struct {
    GtkLayout parent_instance;

    GdkGC *gc;
    				/*label*/
    GwyGraphLabel *lab;
    GwyGraphAreaParams par;

    GwyGraphStatusType status;         
    GwyGraphStatus_SelData *seldata;
    GwyGraphStatus_PointsData *pointsdata;
    GwyGraphStatus_CursorData *cursordata;
    GwyGraphStatus_ZoomData *zoomdata;

    /*drawing*/
    GPtrArray *curves;
    gdouble x_shift;
    gdouble y_shift;
    gdouble x_multiply;
    gdouble y_multiply;
    gdouble x_max;
    gdouble x_min;
    gdouble y_max;
    gdouble y_min;
    gint newline;

    gint old_width;
    gint old_height;
   
    /*selection drawing*/
    gboolean selecting; 
    
    /*label movement*/
    GtkWidget *active;
    gint x0;
    gint y0;
    gint xoff;
    gint yoff;   

} GwyGraphArea;

/*graph area class*/
typedef struct {
     GtkLayoutClass parent_class;

     void (* selected) (GwyGraphArea *area);
     void (* zoomed) (GwyGraphArea *area);
} GwyGraphAreaClass;


GtkWidget* gwy_graph_area_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);

GType gwy_graph_area_get_type(void) G_GNUC_CONST;

void gwy_graph_area_set_style(GwyGraphArea *area, GwyGraphAreaParams style);

void gwy_graph_area_set_boundaries(GwyGraphArea *area, gdouble x_min, gdouble x_max, gdouble y_min, gdouble y_max);

void gwy_graph_area_add_curve(GwyGraphArea *area, GwyGraphAreaCurve *curve);

void gwy_graph_area_clear(GwyGraphArea *area);

void gwy_graph_area_signal_selected(GwyGraphArea *area);

void gwy_graph_area_signal_zoomed(GwyGraphArea *area);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_AXIS_H__*/
