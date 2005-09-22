/*
 *  @(#) $Id$
 *  Copyright (C) 2005 Chris Anderson, Molecular Imaging, Corp.
 *  E-mail: sidewinder.asu@gmail.com.
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

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * Modified by Chris Anderson 2005.
 * GwyCurve is based on GtkCurve (instead of subclassing) since GtkCurve
 * can be subject to removal from Gtk+ at some unspecified point in the
 * future.
 */

#ifndef __GWY_CURVE_H__
#define __GWY_CURVE_H__

#include <gdk/gdk.h>
#include <gtk/gtkdrawingarea.h>

#include <libgwydgets/gwydgetenums.h>

G_BEGIN_DECLS

#define GWY_TYPE_CURVE                 (gwy_curve_get_type ())
#define GWY_CURVE(obj) \
            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GWY_TYPE_CURVE, GwyCurve))
#define GWY_CURVE_CLASS(klass) \
            (G_TYPE_CHECK_CLASS_CAST ((klass), GWY_TYPE_CURVE, GwyCurveClass))
#define GWY_IS_CURVE(obj) \
            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GWY_TYPE_CURVE))
#define GWY_IS_CURVE_CLASS(klass) \
            (G_TYPE_CHECK_CLASS_TYPE ((klass), GWY_TYPE_CURVE))
#define GWY_CURVE_GET_CLASS(obj) \
            (G_TYPE_INSTANCE_GET_CLASS ((obj), GWY_TYPE_CURVE, GwyCurveClass))

typedef struct _GwyCurve        GwyCurve;
typedef struct _GwyCurveClass   GwyCurveClass;

typedef struct _GwyPoint        GwyPoint;
typedef struct _GwyChannelData  GwyChannelData;

struct _GwyPoint
{
    gfloat x;
    gfloat y;
};

struct _GwyChannelData
{
    /* curve points: */
    gint num_points;
    GwyPoint *points;

    /* control points: */
    gint num_ctlpoints;
    GwyPoint *ctlpoints;
};

struct _GwyCurve
{
    GtkDrawingArea graph;

    gint cursor_type;
    gfloat min_x;
    gfloat max_x;
    gfloat min_y;
    gfloat max_y;
    GdkPixmap *pixmap;
    GwyCurveType curve_type;
    gint height;                  /* (cached) graph height in pixels */
    gint grab_point;              /* point currently grabbed */
    gint grab_channel;            /* channel of grabbed point */
    gint last;

    /* curve point and control point data
       (3 color channels: red, green, blue) */
    GwyChannelData channel_data[3];
};

struct _GwyCurveClass
{
  GtkDrawingAreaClass parent_class;

  void (* curve_type_changed) (GwyCurve *curve);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType       gwy_curve_get_type  (void) G_GNUC_CONST;
GtkWidget*  gwy_curve_new       (void);
void        gwy_curve_reset     (GwyCurve *curve);
//void        gwy_curve_set_gamma (GwyCurve *curve, gfloat gamma_);
void        gwy_curve_set_range (GwyCurve *curve,
                      gfloat min_x, gfloat max_x,
                      gfloat min_y, gfloat max_y);

void    gwy_curve_get_vector    (GwyCurve *c, gint channel,
                                 gint veclen, gfloat vector[]);

//void        gwy_curve_set_vector    (GwyCurve *curve,
//                     int veclen, gfloat vector[]);
void        gwy_curve_set_curve_type (GwyCurve *curve, GwyCurveType type);


G_END_DECLS

#endif /* __GWY_CURVE_H__ */
