/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwydgets/gwygraphselections.h>
#include <libgwydgets/gwygraphmodel.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_AREA            (gwy_graph_area_get_type())
#define GWY_GRAPH_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_AREA, GwyGraphArea))
#define GWY_GRAPH_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_AREA, GwyGraphAreaClass))
#define GWY_IS_GRAPH_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_AREA))
#define GWY_IS_GRAPH_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_AREA))
#define GWY_GRAPH_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_AREA, GwyGraphAreaClass))

typedef struct _GwyGraphArea      GwyGraphArea;
typedef struct _GwyGraphAreaClass GwyGraphAreaClass;

struct _GwyGraphArea {
    GtkLayout parent_instance;

    GwyGraphModel *graph_model;

    GdkGC *gc;
    GdkCursor *cross_cursor;
    GdkCursor *fleur_cursor;
    GdkCursor *harrow_cursor;
    GdkCursor *varrow_cursor;

    gulong curve_notify_id;
    gulong curve_data_changed_id;
    gulong model_notify_id;
    gulong handler1_id;
    gulong handler2_id;

    /* label */
    GwyGraphLabel *lab;

    GwyGraphStatusType status;
    GwySelection *pointsdata;
    GwySelection *xseldata;
    GwySelection *yseldata;
    GwySelection *xlinesdata;
    GwySelection *ylinesdata;
    GwySelection *zoomdata;

    struct {
        gdouble x;
        gdouble y;
    } actual_cursor;

    /* selection drawing */
    gboolean selecting;
    gboolean mouse_present;
    gint selected_object_index;
    gint selected_border;

    /* grid lines */
    GArray *x_grid_data;
    GArray *y_grid_data;

    /* area boundaries, the real ones */
    gdouble x_max;
    gdouble x_min;
    gdouble y_max;
    gdouble y_min;

    gint old_width;
    gint old_height;
    gint label_old_width;
    gint label_old_height;

    /* dialogs */
    GtkWidget *area_dialog;
    GtkWidget *label_dialog;

    /* label movement */
    GtkWidget *active;
    gint x0;
    gint y0;
    gint xoff;
    gint yoff;
    gdouble rx0;
    gdouble ry0;
    gint rxoff;
    gint ryoff;

    gboolean enable_user_input;
    gint selection_limit;

    gboolean bool1;
    gboolean bool2;
    gint int1;
    gint int2;
    GwyGraphStatusType enum1;
    GwyGraphStatusType enum2;

    gpointer reserved1;
    gpointer reserved3;
    gpointer reserved4;
    gpointer reserved5;
};

struct _GwyGraphAreaClass {
    GtkLayoutClass parent_class;

    void (*status_changed)(GwyGraphArea *area);

    gpointer reserved1;
    gpointer reserved2;
};

GType              gwy_graph_area_get_type         (void) G_GNUC_CONST;
GtkWidget*         gwy_graph_area_new              (void);
GtkWidget*         gwy_graph_area_get_label        (GwyGraphArea *area);
void               gwy_graph_area_set_model        (GwyGraphArea *area,
                                                    GwyGraphModel *gmodel);
GwyGraphModel*     gwy_graph_area_get_model        (GwyGraphArea *area);
void               gwy_graph_area_get_cursor       (GwyGraphArea *area,
                                                    gdouble *x_cursor,
                                                    gdouble *y_cursor);
void               gwy_graph_area_set_x_range      (GwyGraphArea *area,
                                                    gdouble x_min,
                                                    gdouble x_max);
void               gwy_graph_area_set_y_range      (GwyGraphArea *area,
                                                    gdouble y_min,
                                                    gdouble y_max);
void               gwy_graph_area_set_x_grid_data  (GwyGraphArea *area,
                                                    guint ndata,
                                                    const gdouble *grid_data);
void               gwy_graph_area_set_y_grid_data  (GwyGraphArea *area,
                                                    guint ndata,
                                                    const gdouble *grid_data);
const gdouble*     gwy_graph_area_get_x_grid_data  (GwyGraphArea *area,
                                                    guint *ndata);
const gdouble*     gwy_graph_area_get_y_grid_data  (GwyGraphArea *area,
                                                    guint *ndata);
GwySelection*      gwy_graph_area_get_selection    (GwyGraphArea *area,
                                                    GwyGraphStatusType status_type);
void               gwy_graph_area_set_status       (GwyGraphArea *area,
                                                    GwyGraphStatusType status_type);
GwyGraphStatusType gwy_graph_area_get_status       (GwyGraphArea *area);
void               gwy_graph_area_draw_on_drawable (GwyGraphArea *area,
                                                    GdkDrawable *drawable,
                                                    GdkGC *gc,
                                                    gint x,
                                                    gint y,
                                                    gint width,
                                                    gint height);
GString*           gwy_graph_area_export_vector    (GwyGraphArea *area,
                                                    gint x,
                                                    gint y,
                                                    gint width,
                                                    gint height);
void               gwy_graph_area_enable_user_input(GwyGraphArea *area,
                                                    gboolean enable);

G_END_DECLS

#endif  /* __GWY_GRAPH_AREA_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
