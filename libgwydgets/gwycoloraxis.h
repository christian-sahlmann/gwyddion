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
#include <libgwyddion/gwysiunit.h>
#include <libdraw/gwypalette.h>
#include <libdraw/gwygradient.h>

G_BEGIN_DECLS

#define GWY_TYPE_COLOR_AXIS            (gwy_color_axis_get_type())
#define GWY_COLOR_AXIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_COLOR_AXIS, GwyColorAxis))
#define GWY_COLOR_AXIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_COLOR_AXIS, GwyColorAxis))
#define GWY_IS_COLOR_AXIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_COLOR_AXIS))
#define GWY_IS_COLOR_AXIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_COLOR_AXIS))
#define GWY_COLOR_AXIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_COLOR_AXIS, GwyColorAxisClass))

typedef struct _GwyColorAxis      GwyColorAxis;
typedef struct _GwyColorAxisClass GwyColorAxisClass;

/* XXX: merge or something */
typedef struct {
    gint tick_length;
    gint textarea;    /*text area width*/

    PangoFontDescription *font;
} GwyColorAxisParams;

struct _GwyColorAxis {
    GtkWidget widget;

    GwyColorAxisParams par;
    GwyPalette *palette;   /* XXX: Remove */

    GdkPixbuf *pixbuf;

    GtkOrientation orientation;
    gdouble min;
    gdouble max;

    GString *label_text;
    GwySIUnit *siunit;

    GwyGradient *gradient;
    gpointer reserved2;
};

struct _GwyColorAxisClass {
    GtkWidgetClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};


GType        gwy_color_axis_get_type  (void) G_GNUC_CONST;
/* XXX: change arguments, ideally to orientation only */
GtkWidget*   gwy_color_axis_new       (GtkOrientation orientation,
                                       gdouble min,
                                       gdouble max,
                                       GwyPalette *pal);
void         gwy_color_axis_get_range (GwyColorAxis *axis,
                                       gdouble *min,
                                       gdouble *max);
void         gwy_color_axis_set_range (GwyColorAxis *axis,
                                       gdouble min,
                                       gdouble max);
GwySIUnit*   gwy_color_axis_get_si_unit(GwyColorAxis *axis);
void         gwy_color_axis_set_si_unit(GwyColorAxis *axis,
                                        GwySIUnit *unit);

void         gwy_color_axis_set_gradient(GwyColorAxis *axis,
                                         const gchar *gradient);
const gchar* gwy_color_axis_get_gradient(GwyColorAxis *axis);

#ifndef GWY_DISABLE_DEPRECATED
void         gwy_color_axis_set_unit(GwyColorAxis *axis,
                                     GwySIUnit *unit);
void gwy_color_axis_set_palette(GwyColorAxis *axis, GwyPalette *pal);
GwyPalette* gwy_color_axis_get_palette(GwyColorAxis *axis);
#endif

G_END_DECLS

#endif /*__GWY_COLOR_AXIS_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
