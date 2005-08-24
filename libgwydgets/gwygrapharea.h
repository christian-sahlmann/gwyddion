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

#ifndef __GWY_GRAPH_AREA_H__
#define __GWY_GRAPH_AREA_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtklayout.h>
#include <libgwydgets/gwygraphareadialog.h>
#include <libgwydgets/gwygraphlabeldialog.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_AREA            (gwy_graph_area_get_type())
#define GWY_GRAPH_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_AREA, GwyGraphArea))
#define GWY_GRAPH_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_AREA, GwyGraphAreaClass))
#define GWY_IS_GRAPH_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_AREA))
#define GWY_IS_GRAPH_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_AREA))
#define GWY_GRAPH_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_AREA, GwyGraphAreaClass))

typedef struct _GwyGraphArea      GwyGraphArea;
typedef struct _GwyGraphAreaClass GwyGraphAreaClass;


typedef struct {
  GwyGraphDataPoint data_point;
} GwyGraphStatus_PointData;

typedef struct {
  GwyGraphDataPoint data_point;
} GwyGraphStatus_CursorData;
    
typedef struct {
  GArray *data_points;
} GwyGraphStatus_PointsData;
                                                                                                                                                             
typedef struct {
  GwyGraphDataArea data_area;
} GwyGraphStatus_AreaData;
                                                                                                                                                             
typedef struct {
  GArray *data_areas;
} GwyGraphStatus_AreasData;

typedef struct {
  GArray *data_lines;
} GwyGraphStatus_LinesData;

typedef struct {
  gdouble xmin;
  gdouble ymin;
  gdouble width;
  gdouble height;
} GwyGraphStatus_ZoomData;


/*single curve*/
typedef struct {
    GdkPoint *points;           /*points to be directly plotted*/
    
    gpointer reserved;
} GwyGraphAreaCurve;


/*graph area structure*/
struct _GwyGraphArea {
    GtkLayout parent_instance;

    GdkGC *gc;
                    /*label*/
    GwyGraphLabel *lab;

    GwyGraphStatusType status;
    GwyGraphStatus_PointData *pointdata;
    GwyGraphStatus_PointsData *pointsdata;
    GwyGraphStatus_AreaData *areadata;
    GwyGraphStatus_AreasData *areasdata;
    GwyGraphStatus_LinesData *linesdata;
    GwyGraphStatus_CursorData *cursordata;
    GwyGraphStatus_ZoomData *zoomdata;

    GwyGraphStatus_CursorData *actual_cursor_data;

    gpointer graph_model;
    GPtrArray *curves;

    /*selection drawing*/
    gboolean selecting;
    gboolean mouse_present;

    /*XXX remove this? It is not set! real boundaries*/
    gint x_max;
    gint x_min;
    gint y_max;
    gint y_min;

    gint old_width;
    gint old_height;
    gboolean newline;
   
    /*linestyle dialog*/
    GwyGraphAreaDialog *area_dialog;
    GwyGraphLabelDialog *label_dialog;
    
    /*label movement*/
    GtkWidget *active;
    gint x0;
    gint y0;
    gint xoff;
    gint yoff;

    GdkColor *colors;

    gboolean enable_user_input;
    gint selection_limit; 
    
    gpointer reserved3;
    gpointer reserved4;
    gpointer reserved5;
};

/*graph area class*/
struct _GwyGraphAreaClass {
    GtkLayoutClass parent_class;

    GdkCursor *cross_cursor;
    GdkCursor *arrow_cursor;
    void (*selected)(GwyGraphArea *area);
    void (*zoomed)(GwyGraphArea *area);
    void (*mouse_moved)(GwyGraphArea *area);


    gpointer reserved1;
    gpointer reserved2;
};


GtkWidget* gwy_graph_area_new(GtkAdjustment *hadjustment, GtkAdjustment *vadjustment);

GType gwy_graph_area_get_type(void) G_GNUC_CONST;

GtkWidget *gwy_graph_area_get_label(GwyGraphArea *area);

void gwy_graph_area_signal_selected(GwyGraphArea *area);

void gwy_graph_area_signal_zoomed(GwyGraphArea *area);

void gwy_graph_area_refresh(GwyGraphArea *area);

void gwy_graph_area_set_selection(GwyGraphArea *area, GwyGraphStatusType status, gdouble* selection, gint n_of_selections);

void gwy_graph_area_set_selection_limit(GwyGraphArea *area, gint limit);

gint gwy_graph_area_get_selection_limit(GwyGraphArea *area);

void gwy_graph_area_set_model(GwyGraphArea *area, gpointer gmodel);

void gwy_graph_area_draw_area_on_drawable(GdkDrawable *drawable, GdkGC *gc,
                                            gint x, gint y, gint width, gint height,
                                            GwyGraphArea *area);
GString* gwy_graph_area_export_vector(GwyGraphArea *area, 
                                      gint x, gint y, 
                                      gint width, gint height);

void gwy_graph_area_clear_selection(GwyGraphArea *area);

void gwy_graph_area_enable_user_input(GwyGraphArea *area, gboolean enable);

void gwy_graph_area_get_cursor(GwyGraphArea *area, gdouble *x_cursor, gdouble *y_cursor);


G_END_DECLS

#endif /*__GWY_AXIS_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
