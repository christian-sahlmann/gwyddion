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

#ifndef __GWY_COLOR_AXIS_H__
#define __GWY_COLOR_AXIS_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>
#include <libdraw/gwypalette.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GWY_TYPE_COLOR_AXIS            (gwy_color_axis_get_type())
#define GWY_COLOR_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COLOR_AXIS, GwyColorAxis))
#define GWY_COLOR_AXIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COLOR_AXIS, GwyColorAxis))
#define GWY_IS_COLOR_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COLOR_AXIS))
#define GWY_IS_COLOR_AXIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COLOR_AXIS))
#define GWY_COLOR_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COLOR_AXIS, GwyColorAxisClass))

typedef struct _GwyColorAxis      GwyColorAxis;
typedef struct _GwyColorAxisClass GwyColorAxisClass;

typedef struct {
    gint tick_length;
    gint textarea;    /*text area width*/

    PangoFontDescription *font;
} GwyColorAxisParams;

struct _GwyColorAxis {
    GtkWidget widget;

    GwyColorAxisParams par;
    GwyPalette *palette;

    GdkPixbuf *pixbuf;

    GtkOrientation orientation;   /*north, south, east, west*/
    gdouble min;
    gdouble max;

    GString *label_text;

};

struct _GwyColorAxisClass {
     GtkWidgetClass parent_class;
};


GtkWidget* gwy_color_axis_new(GtkOrientation orientation, gdouble min, gdouble max, GwyPalette *pal);

GType gwy_color_axis_get_type(void) G_GNUC_CONST;

void gwy_color_axis_get_range(GwyColorAxis *axis, gdouble *min, gdouble *max);

void gwy_color_axis_set_range(GwyColorAxis *axis, gdouble min, gdouble max);

void gwy_color_axis_set_palette(GwyColorAxis *axis, GwyPalette *pal);

GwyPalette * gwy_color_axis_get_palette(GwyColorAxis *axis);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_AXIS_H__*/
