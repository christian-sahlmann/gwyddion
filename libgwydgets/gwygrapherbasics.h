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

#ifndef __GWY_GRAPHER_BASIC__
#define __GWY_GRAPHER_BASIC__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <libdraw/gwyrgba.h>
#include "gwygrapher.h"

G_BEGIN_DECLS

typedef enum {
    GWY_GRAPHER_POINT_SQUARE        = 0,
    GWY_GRAPHER_POINT_CROSS         = 1,
    GWY_GRAPHER_POINT_CIRCLE        = 2,
    GWY_GRAPHER_POINT_STAR          = 3,
    GWY_GRAPHER_POINT_TIMES         = 4,
    GWY_GRAPHER_POINT_TRIANGLE_UP   = 5,
    GWY_GRAPHER_POINT_TRIANGLE_DOWN = 6,
    GWY_GRAPHER_POINT_DIAMOND       = 7
} GwyGrapherPointType;

typedef enum {
    GWY_GRAPHER_CURVE_HIDDEN      = 0,
    GWY_GRAPHER_CURVE_POINTS      = 1,
    GWY_GRAPHER_CURVE_LINE        = 2,
    GWY_GRAPHER_CURVE_LINE_POINTS = 3
} GwyGrapherCurveType;


typedef enum {
    GWY_GRAPHER_LABEL_NORTHEAST = 0,
    GWY_GRAPHER_LABEL_NORTHWEST = 1,
    GWY_GRAPHER_LABEL_SOUTHEAST = 2,
    GWY_GRAPHER_LABEL_SOUTHWEST = 3,
    GWY_GRAPHER_LABEL_USER      = 4
} GwyGrapherLabelPosition;


typedef struct {
    gdouble x;
    gdouble y;
} GwyGrapherDataPoint;

typedef struct {
    gdouble xmin;
    gdouble xmax;
    gdouble ymin;
    gdouble ymax;
} GwyGrapherDataArea;

typedef struct {
    gint xmin;         /*x offset of the active area with respect to drawable left border*/
    gint ymin;         /*y offset of the active area with respect to drawable top border*/
    gint height;       /*active area height*/
    gint width;        /*active area width*/
    gdouble real_xmin; /*real units values*/
    gdouble real_ymin; /*real units values*/
    gdouble real_height; /*real units values*/
    gdouble real_width; /*real units values*/
    gboolean log_x;     /*x axis is logarithmic*/
    gboolean log_y;     /*y axis is logarithmic*/
} GwyGrapherActiveAreaSpecs;


void  gwy_grapher_draw_point (GdkDrawable *drawable,
                            GdkGC *gc, 
                            gint x,
                            gint y,
                            GwyGrapherPointType type, 
                            gint size, 
                            GwyRGBA *color, 
                            gboolean clear);

void gwy_grapher_draw_line   (GdkDrawable *drawable,
                            GdkGC *gc,
                            gint x_from,
                            gint y_from,
                            gint x_to,
                            gint y_to,
                            GdkLineStyle line_style,
                            gint size,
                            GwyRGBA *color);

void gwy_grapher_draw_curve  (GdkDrawable *drawable,
                            GdkGC *gc,
                            GwyGrapherActiveAreaSpecs *specs,
                            GObject *curvemodel);

void gwy_grapher_draw_selection_points(GdkDrawable *drawable,
                            GdkGC *gc,
                            GwyGrapherActiveAreaSpecs *specs,
                            GwyGrapherDataPoint *data_points,
                            gint n_of_points);

void gwy_grapher_draw_selection_areas(GdkDrawable *drawable,
                                    GdkGC *gc,
                                    GwyGrapherActiveAreaSpecs *specs,
                                    GwyGrapherDataArea *data_areas,
                                    gint n_of_areas);


#endif /*__GWY_GRAPHER_BASIC_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
