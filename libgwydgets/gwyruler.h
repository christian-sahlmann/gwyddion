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
 * GwyRuler is based on GtkRuler (instead of subclassing) since GtkRuler
 * can be subject of removal from Gtk+ in some unspecified point in the future.
 */

#ifndef __GWY_RULER_H__
#define __GWY_RULER_H__


#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GWY_TYPE_RULER            (gwy_ruler_get_type())
#define GWY_RULER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_RULER, GwyRuler))
#define GWY_RULER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_RULER, GwyRulerClass))
#define GWY_IS_RULER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_RULER))
#define GWY_IS_RULER_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_RULER))
#define GWY_RULER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_RULER, GwyRulerClass))


typedef struct _GwyRuler        GwyRuler;
typedef struct _GwyRulerClass   GwyRulerClass;

typedef enum {
    GWY_UNITS_PLACEMENT_NONE,
    GWY_UNITS_PLACEMENT_AT_ZERO
} GwyUnitsPlacement;

struct _GwyRuler
{
    GtkWidget widget;

    GdkPixmap *backing_store;
    GdkGC *non_gr_exp_gc;
    gint xsrc, ysrc;
    gint slider_size;
    gchar *units;
    GwyUnitsPlacement units_placement;

    gdouble lower;    /* The upper limit of the ruler (in physical units) */
    gdouble upper;    /* The lower limit of the ruler */
    gdouble position;    /* The position of the mark on the ruler */
    gdouble max_size;    /* The maximum size of the ruler */
};

struct _GwyRulerClass
{
    GtkWidgetClass parent_class;

    void (*draw_ticks) (GwyRuler *ruler);
    void (*draw_pos)   (GwyRuler *ruler);
};


GType             gwy_ruler_get_type            (void) G_GNUC_CONST;
void              gwy_ruler_set_range           (GwyRuler      *ruler,
                                                 gdouble        lower,
                                                 gdouble        upper,
                                                 gdouble        position,
                                                 gdouble        max_size);
void              gwy_ruler_draw_ticks          (GwyRuler      *ruler);
void              gwy_ruler_draw_pos            (GwyRuler      *ruler);

void              gwy_ruler_get_range           (GwyRuler *ruler,
                                                 gdouble  *lower,
                                                 gdouble  *upper,
                                                 gdouble  *position,
                                                 gdouble  *max_size);
void              gwy_ruler_set_units           (GwyRuler *ruler,
                                                 const gchar *units);
G_CONST_RETURN
gchar*            gwy_ruler_get_units           (GwyRuler *ruler);
GwyUnitsPlacement gwy_ruler_get_units_placement (GwyRuler *ruler);
void              gwy_ruler_set_units_placement (GwyRuler *ruler,
                                                 GwyUnitsPlacement placement);

/* internal */
void _gwy_ruler_real_draw_ticks(GwyRuler *ruler,
                                gint pixelsize,
                                gint min_label_spacing,
                                gint min_tick_spacing,
                                void (*label_callback)(GwyRuler *ruler,
                                                       gint position,
                                                       const gchar *label,
                                                       PangoLayout *layout,
                                                       gint digit_height,
                                                       gint digit_offset),
                                void (*tick_callback)(GwyRuler *ruler,
                                                      gint position,
                                                      gint depth));

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GWY_RULER_H__ */
/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
