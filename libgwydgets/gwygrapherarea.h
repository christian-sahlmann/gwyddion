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

#ifndef __GWY_GRAPHER_AREA_H__
#define __GWY_GRAPHER_AREA_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtklayout.h>
#include "gwygrapher.h"
#include "gwygrapherareadialog.h"
#include "gwygrapherlabeldialog.h"

G_BEGIN_DECLS

#define GWY_TYPE_GRAPHER_AREA            (gwy_grapher_area_get_type())
#define GWY_GRAPHER_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPHER_AREA, GwyGrapherArea))
#define GWY_GRAPHER_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPHER_AREA, GwyGrapherArea))
#define GWY_IS_GRAPHER_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPHER_AREA))
#define GWY_IS_GRAPHER_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPHER_AREA))
#define GWY_GRAPHER_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPHER_AREA, GwyGrapherAreaClass))

typedef struct _GwyGrapherArea      GwyGrapherArea;
typedef struct _GwyGrapherAreaClass GwyGrapherAreaClass;


typedef enum {
    GWY_GRAPHER_STATUS_PLAIN  = 0,
    GWY_GRAPHER_STATUS_CURSOR = 1,
    GWY_GRAPHER_STATUS_XSEL   = 2,
    GWY_GRAPHER_STATUS_YSEL   = 3,
    GWY_GRAPHER_STATUS_POINTS = 4,
    GWY_GRAPHER_STATUS_ZOOM   = 5
} GwyGrapherStatusType;


typedef struct {
  GwyGrapherDataPoint data_point;
} GwyGrapherStatus_PointData;

typedef struct {
  GwyGrapherDataPoint data_point;
} GwyGrapherStatus_CursorData;
                                                                                                                                                               typedef struct {
  GArray *data_points;
} GwyGrapherStatus_PointsData;
                                                                                                                                                             
typedef struct {
  GwyGrapherDataArea data_area;
} GwyGrapherStatus_AreaData;
                                                                                                                                                             
typedef struct {
  GArray *data_areas;
} GwyGrapherStatus_AreasData;

typedef struct {
  gdouble xmin;
  gdouble ymin;
  gdouble width;
  gdouble height;
} GwyGrapherStatus_ZoomData;


/*single curve*/
typedef struct {
    GdkPoint *points;           /*points to be directly plotted*/
    
    gpointer reserved;
} GwyGrapherAreaCurve;


/*grapher area structure*/
struct _GwyGrapherArea {
    GtkLayout parent_instance;

    GdkGC *gc;
                    /*label*/
    GwyGrapherLabel *lab;

    GwyGrapherStatusType status;
    GwyGrapherStatus_PointData *pointdata;
    GwyGrapherStatus_PointsData *pointsdata;
    GwyGrapherStatus_AreaData *areadata;
    GwyGrapherStatus_AreasData *areasdata;
    GwyGrapherStatus_CursorData *cursordata;
    GwyGrapherStatus_ZoomData *zoomdata;

    gpointer grapher_model;
    GPtrArray *curves;

    /*selection drawing*/
    gboolean selecting;

    /*real boundaries*/
    gint x_max;
    gint x_min;
    gint y_max;
    gint y_min;

    gint old_width;
    gint old_height;
    gboolean newline;
   
    /*linestyle dialog*/
    GwyGrapherAreaDialog *area_dialog;
    GwyGrapherLabelDialog *label_dialog;
    
    /*label movement*/
    GtkWidget *active;
    gint x0;
    gint y0;
    gint xoff;
    gint yoff;

    GdkColor *colors;

    gpointer reserved2;
    gpointer reserved3;
    gpointer reserved4;
    gpointer reserved5;
};

/*grapher area class*/
struct _GwyGrapherAreaClass {
    GtkLayoutClass parent_class;

    GdkCursor *cross_cursor;
    GdkCursor *arrow_cursor;
    void (*selected)(GwyGrapherArea *area);
    void (*zoomed)(GwyGrapherArea *area);


    gpointer reserved1;
    gpointer reserved2;
};


GtkWidget* gwy_grapher_area_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);

GType gwy_grapher_area_get_type(void) G_GNUC_CONST;

void gwy_grapher_area_signal_selected(GwyGrapherArea *area);

void gwy_grapher_area_signal_zoomed(GwyGrapherArea *area);

void gwy_grapher_area_refresh(GwyGrapherArea *area);

void gwy_grapher_area_set_selection(GwyGrapherArea *area, GwyGrapherStatusType *status, gpointer statusdata);

void gwy_grapher_area_change_model(GwyGrapherArea *area, gpointer gmodel);

G_END_DECLS

#endif /*__GWY_AXIS_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
