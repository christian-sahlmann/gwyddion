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

#include <gdk/gdkdrawable.h>
#include <gtk/gtkwidget.h>
#include <libgwydgets/gwydgetenums.h>
#include <libgwyddion/gwysiunit.h>

G_BEGIN_DECLS

#define GWY_TYPE_AXIS            (gwy_axis_get_type())
#define GWY_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_AXIS, GwyAxis))
#define GWY_AXIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_AXIS, GwyAxisClass))
#define GWY_IS_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_AXIS))
#define GWY_IS_AXIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_AXIS))
#define GWY_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_AXIS, GwyAxisClass))

typedef struct _GwyAxis      GwyAxis;
typedef struct _GwyAxisClass GwyAxisClass;

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

    GdkGC *gc;
    GwyAxisParams par;

    gboolean is_visible;
    gboolean is_logarithmic;
    gboolean is_auto;           /*affects: tick numbers and label positions.*/
    gboolean is_standalone;
    GtkPositionType orientation;

    gint outer_border_width;

    gdouble reqmin;
    gdouble reqmax;
    gdouble max;                /*axis beginning value*/
    gdouble min;                /*axis end value*/

    GArray *mjticks;            /*array of GwyLabeledTicks*/
    GArray *miticks;            /*array of GwyTicks*/

    GString *label_text;

    GwySIUnit *unit;                /*axis unit (if any)*/
    GString *magnification_string;
    gdouble magnification;

    GtkWidget *dialog;      /*axis label and other properties dialog*/

    gboolean enable_label_edit;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyAxisClass {
    GtkWidgetClass parent_class;

    void (*label_updated)(GwyAxis *axis);
    void (*rescaled)(GwyAxis *axis);

    gpointer reserved1;
    gpointer reserved2;
};


GType           gwy_axis_get_type                (void) G_GNUC_CONST;
GtkWidget*      gwy_axis_new                     (gint orientation);
void            gwy_axis_set_logarithmic         (GwyAxis *axis,
                                                  gboolean is_logarithmic);
void            gwy_axis_set_visible             (GwyAxis *axis,
                                                  gboolean is_visible);
gboolean        gwy_axis_is_visible              (GwyAxis *axis);
gboolean        gwy_axis_is_logarithmic          (GwyAxis *axis);
GtkPositionType gwy_axis_get_orientation         (GwyAxis *axis);
void            gwy_axis_set_auto                (GwyAxis *axis,
                                                  gboolean is_auto);
void            gwy_axis_request_range           (GwyAxis *axis,
                                                  gdouble min,
                                                  gdouble max);
void            gwy_axis_get_range               (GwyAxis *axis,
                                                  gdouble *min,
                                                  gdouble *max);
void            gwy_axis_get_requested_range     (GwyAxis *axis,
                                                  gdouble *min,
                                                  gdouble *max);
void            gwy_axis_set_style               (GwyAxis *axis,
                                                  GwyAxisParams style);
gdouble         gwy_axis_get_magnification       (GwyAxis *axis);
const gchar*    gwy_axis_get_magnification_string(GwyAxis *axis);
void            gwy_axis_set_label               (GwyAxis *axis,
                                                  const gchar *label);
const gchar*    gwy_axis_get_label               (GwyAxis *axis);
void            gwy_axis_set_unit                (GwyAxis *axis,
                                                  GwySIUnit *unit);
void            gwy_axis_enable_label_edit       (GwyAxis *axis,
                                                  gboolean enable);
void            gwy_axis_draw_on_drawable        (GwyAxis *axis,
                                                  GdkDrawable *drawable,
                                                  GdkGC *gc,
                                                  gint xmin,
                                                  gint ymin,
                                                  gint width,
                                                  gint height);
GString*        gwy_axis_export_vector           (GwyAxis *axis,
                                                  gint xmin,
                                                  gint ymin,
                                                  gint width,
                                                  gint height,
                                                  gint fontsize);
void            gwy_axis_get_major_ticks         (GwyAxis *axis,
                                                  GArray *array);

G_END_DECLS

#endif /* __GWY_AXIS_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
