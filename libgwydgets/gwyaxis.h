/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_AXIS_H__
#define __GWY_AXIS_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include "gwyaxisdialog.h"


G_BEGIN_DECLS

/* FIXME more */
#define GWY_AXIS_NORTH   GTK_POS_TOP
#define GWY_AXIS_SOUTH   GTK_POS_BOTTOM
#define GWY_AXIS_EAST    GTK_POS_LEFT
#define GWY_AXIS_WEST    GTK_POS_RIGHT

#define GWY_TYPE_AXIS            (gwy_axis_get_type())
#define GWY_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS, GwyAxis))
#define GWY_AXIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS, GwyAxis))
#define GWY_IS_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS))
#define GWY_IS_AXIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS))
#define GWY_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS, GwyAxisClass))

typedef struct _GwyAxis      GwyAxis;
typedef struct _GwyAxisClass GwyAxisClass;

typedef enum {
    GWY_AXIS_FLOAT = 1,
    GWY_AXIS_EXP,
    GWY_AXIS_INT,
    GWY_AXIS_AUTO
} GwyAxisScaleFormat;

typedef struct {
    gdouble value;      /*tick value*/
    gint scrpos;        /*precomputed tick screen position*/
} GwyTick;

typedef struct {
    GwyTick t;
    GString *ttext;
} GwyLabeledTick;

typedef struct {
    gint major_length;
    gint major_thickness;
    gint major_maxticks;
    GwyAxisScaleFormat major_printmode;

    gint minor_length;
    gint minor_thickness;
    gint minor_division;          /*minor division*/

    gint line_thickness;

    PangoFontDescription *major_font;
    PangoFontDescription *label_font;
} GwyAxisParams;

struct _GwyAxis {
    GtkWidget widget;

    GwyAxisParams par;

    gboolean is_visible;
    gboolean is_logarithmic;
    gboolean is_auto;           /*affects: tick numbers and label positions.*/
    gboolean is_standalone;
    gboolean has_unit;
    GtkPositionType orientation;    /*north, south, east, west*/

    gdouble reqmin;
    gdouble reqmax;
    gdouble max;                /*axis beginning value*/
    gdouble min;                /*axis end value*/

    GArray *mjticks;            /*array of GwyLabeledTicks*/
    GArray *miticks;            /*array of GwyTicks*/

    gint label_x_pos;           /*label position*/
    gint label_y_pos;
    GString *label_text;

    gchar *unit;                /*axis unit (if any)*/

    GtkWidget *dialog;      /*axis label and other properties dialog*/

    gboolean enable_set_label;
    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyAxisClass {
    GtkWidgetClass parent_class;

    gpointer reserved1;
    gpointer reserved2;

    void (*selected)(GwyAxis *axis);
};


GType       gwy_axis_get_type           (void) G_GNUC_CONST;
GtkWidget*  gwy_axis_new                (gint orientation,
                                         gdouble min,
                                         gdouble max,
                                         const gchar *label);
void        gwy_axis_set_logarithmic    (GwyAxis *axis,
                                         gboolean is_logarithmic);
void        gwy_axis_set_visible        (GwyAxis *axis,
                                         gboolean is_visible);
void        gwy_axis_set_auto           (GwyAxis *axis,
                                         gboolean is_auto);
void        gwy_axis_set_req            (GwyAxis *axis,
                                         gdouble min,
                                         gdouble max);
void        gwy_axis_set_style          (GwyAxis *axis,
                                         GwyAxisParams style);
gdouble     gwy_axis_get_maximum        (GwyAxis *axis);
gdouble     gwy_axis_get_minimum        (GwyAxis *axis);
gdouble     gwy_axis_get_reqmaximum     (GwyAxis *axis);
gdouble     gwy_axis_get_reqminimum     (GwyAxis *axis);
void        gwy_axis_set_label          (GwyAxis *axis,
                                         GString *label_text);
GString*    gwy_axis_get_label          (GwyAxis *axis);
void        gwy_axis_set_unit           (GwyAxis *axis,
                                         char *unit);

void        gwy_axis_enable_set_label   (GwyAxis *axis,
                                         gboolean enable);
G_END_DECLS

#endif /*__GWY_AXIS_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
