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

#ifndef __GWY_AXIS_H__
#define __GWY_AXIS_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include "gwyaxisdialog.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_AXIS_NORTH   0
#define GWY_AXIS_SOUTH   1
#define GWY_AXIS_EAST    2
#define GWY_AXIS_WEST    3

#define GWY_AXIS_FLOAT   0
#define GWY_AXIS_EXP     1
#define GWY_AXIS_INT     2
#define GWY_AXIS_AUTO    3
    
    
#define GWY_TYPE_AXIS            (gwy_axis_get_type())
#define GWY_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS, GwyAxis))
#define GWY_AXIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS, GwyAxis))
#define GWY_IS_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS))
#define GWY_IS_AXIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS))
#define GWY_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS, GwyAxisClass))
    

typedef struct {
    gdouble value;	/*tick value*/
    gint scrpos;	/*precomputed tick screen position*/
} GwyTick;

typedef struct {
    GwyTick t;
    GString *ttext;
} GwyLabeledTick;

typedef struct {
    gint major_length;
    gint major_thickness;
    gint major_maxticks;
    gint major_printmode;
    
    gint minor_length;
    gint minor_thickness;
    gint minor_division;          /*minor division*/
    
    gint line_thickness;

    PangoFontDescription *major_font;
    PangoFontDescription *label_font;
} GwyAxisParams;

typedef struct {
    GtkWidget widget;

    GwyAxisParams par;
    
    gboolean is_visible;
    gboolean is_logarithmic;
    gboolean is_auto;           /*affects: tick numbers and label positions.*/
    gboolean is_standalone;
    gboolean has_unit;
    gint orientation;		/*north, south, east, west*/
   
    gdouble reqmin;
    gdouble reqmax;
    gdouble max;		/*axis beginning value*/
    gdouble min;		/*axis end value*/

    GArray *mjticks;		/*array of GwyLabeledTicks*/
    GArray *miticks;		/*array of GwyTicks*/
    
    gint label_x_pos;		/*label position*/
    gint label_y_pos;
    GString *label_text;

    gchar *unit;		/*axis unit (if any)*/

    GwyAxisDialog *dialog;      /*axis label and other properties dialog*/
    
} GwyAxis;

typedef struct {
     GtkWidgetClass parent_class;
} GwyAxisClass;


GtkWidget* gwy_axis_new(gint orientation, gdouble min, gdouble max, const gchar *label);

GType gwy_axis_get_type(void) G_GNUC_CONST;

void gwy_axis_set_logarithmic(GwyAxis *axis, gboolean is_logarithmic);

void gwy_axis_set_visible(GwyAxis *axis, gboolean is_visible);

void gwy_axis_set_auto(GwyAxis *axis, gboolean is_auto);

void gwy_axis_set_req(GwyAxis *axis, gdouble min, gdouble max);

void gwy_axis_set_style(GwyAxis *axis, GwyAxisParams style);

gdouble gwy_axis_get_maximum(GwyAxis *axis);

gdouble gwy_axis_get_minimum(GwyAxis *axis);

gdouble gwy_axis_get_reqmaximum(GwyAxis *axis);

gdouble gwy_axis_get_reqminimum(GwyAxis *axis);

void gwy_axis_set_label(GwyAxis *axis, GString *label_text);

GString* gwy_axis_get_label(GwyAxis *axis);

void gwy_axis_set_unit(GwyAxis *axis, char *unit);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_AXIS_H__*/
