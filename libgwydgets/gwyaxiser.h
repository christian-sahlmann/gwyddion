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

#ifndef __GWY_AXISER_H__
#define __GWY_AXISER_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include "gwyaxisdialog.h"


G_BEGIN_DECLS

/* FIXME more */
#define GWY_AXISER_NORTH   GTK_POS_TOP
#define GWY_AXISER_SOUTH   GTK_POS_BOTTOM
#define GWY_AXISER_EAST    GTK_POS_LEFT
#define GWY_AXISER_WEST    GTK_POS_RIGHT

#define GWY_TYPE_AXISER            (gwy_axiser_get_type())
#define GWY_AXISER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXISER, GwyAxiser))
#define GWY_AXISER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXISER, GwyAxiser))
#define GWY_IS_AXISER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXISER))
#define GWY_IS_AXISER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXISER))
#define GWY_AXISER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXISER, GwyAxiserClass))

typedef struct _GwyAxiser      GwyAxiser;
typedef struct _GwyAxiserClass GwyAxiserClass;

typedef enum {
    GWY_AXISER_FLOAT = 1,
    GWY_AXISER_EXP,
    GWY_AXISER_INT,
    GWY_AXISER_AUTO
} GwyAxiserScaleFormat;

typedef struct {
    gdouble value;      /*tick value*/
    gint scrpos;        /*precomputed tick screen position*/
} GwyAxiserTick;

typedef struct {
    GwyAxiserTick t;
    GString *ttext;
} GwyAxiserLabeledTick;

typedef struct {
    gint major_length;
    gint major_thickness;
    gint major_maxticks;
    GwyAxiserScaleFormat major_printmode;

    gint minor_length;
    gint minor_thickness;
    gint minor_division;          /*minor division*/

    gint line_thickness;

    PangoFontDescription *major_font;
    PangoFontDescription *label_font;
} GwyAxiserParams;

struct _GwyAxiser {
    GtkWidget widget;

    GwyAxiserParams par;

    gboolean is_visible;
    gboolean is_logarithmic;
    gboolean is_auto;           /*affects: tick numbers and label positions.*/
    gboolean is_standalone;
    gboolean has_unit;
    GtkPositionType orientation;    /*north, south, east, west*/

    gdouble reqmin;
    gdouble reqmax;
    gdouble max;                /*axiser beginning value*/
    gdouble min;                /*axiser end value*/

    GArray *mjticks;            /*array of GwyLabeledTicks*/
    GArray *miticks;            /*array of GwyTicks*/

    gint label_x_pos;           /*label position*/
    gint label_y_pos;
    GString *label_text;

    gchar *unit;                /*axiser unit (if any)*/

    GtkWidget *dialog;      /*axiser label and other properties dialog*/

    gboolean enable_label_edit;
    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyAxiserClass {
    GtkWidgetClass parent_class;

    void (*label_updated)(GwyAxiser *axiser);
    void (*rescaled)(GwyAxiser *axiser);
        
    gpointer reserved1;
    gpointer reserved2;
};


GType       gwy_axiser_get_type           (void) G_GNUC_CONST;
GtkWidget*  gwy_axiser_new                (gint orientation,
                                         gdouble min,
                                         gdouble max,
                                         const gchar *label);
void        gwy_axiser_set_logarithmic    (GwyAxiser *axiser,
                                         gboolean is_logarithmic);
void        gwy_axiser_set_visible        (GwyAxiser *axiser,
                                         gboolean is_visible);
void        gwy_axiser_set_auto           (GwyAxiser *axiser,
                                         gboolean is_auto);
void        gwy_axiser_set_req            (GwyAxiser *axiser,
                                         gdouble min,
                                         gdouble max);
void        gwy_axiser_set_style          (GwyAxiser *axiser,
                                         GwyAxiserParams style);
gdouble     gwy_axiser_get_maximum        (GwyAxiser *axiser);
gdouble     gwy_axiser_get_minimum        (GwyAxiser *axiser);
gdouble     gwy_axiser_get_reqmaximum     (GwyAxiser *axiser);
gdouble     gwy_axiser_get_reqminimum     (GwyAxiser *axiser);
void        gwy_axiser_set_label          (GwyAxiser *axiser,
                                         GString *label_text);
GString*    gwy_axiser_get_label          (GwyAxiser *axiser);
void        gwy_axiser_set_unit           (GwyAxiser *axiser,
                                         char *unit);
void        gwy_axiser_enable_label_edit  (GwyAxiser *axiser,
                                         gboolean enable);
void        gwy_axiser_signal_rescaled   (GwyAxiser *axiser);

G_END_DECLS

#endif /*__GWY_AXISER_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
