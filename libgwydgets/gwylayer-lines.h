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

#ifndef __GWY_LAYER_LINES_H__
#define __GWY_LAYER_LINES_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include <libgwydgets/gwydataviewlayer.h>
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_LAYER_LINES            (gwy_layer_lines_get_type())
#define GWY_LAYER_LINES(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_LINES, GwyLayerLines))
#define GWY_LAYER_LINES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_LINES, GwyLayerLinesClass))
#define GWY_IS_LAYER_LINES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_LINES))
#define GWY_IS_LAYER_LINES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_LINES))
#define GWY_LAYER_LINES_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_LINES, GwyLayerLinesClass))

typedef struct _GwyLayerLines      GwyLayerLines;
typedef struct _GwyLayerLinesClass GwyLayerLinesClass;

struct _GwyLayerLines {
    GwyDataViewLayer parent_instance;

    gint nlines;
    gint nselected;
    gint inear;
    gboolean moving_line;
    gdouble lmove_x;
    gdouble lmove_y;
    guint button;
    gdouble *lines;
};

struct _GwyLayerLinesClass {
    GwyDataViewLayerClass parent_class;

    GdkCursor *near_cursor;
    GdkCursor *nearline_cursor;
    GdkCursor *move_cursor;
};

GType            gwy_layer_lines_get_type       (void) G_GNUC_CONST;

GtkObject*       gwy_layer_lines_new            (void);
void             gwy_layer_lines_set_max_lines  (GwyDataViewLayer *layer,
                                                 gint nlines);
gint             gwy_layer_lines_get_max_lines  (GwyDataViewLayer *layer);
gint             gwy_layer_lines_get_lines      (GwyDataViewLayer *layer,
                                                 gdouble *lines);
gint             gwy_layer_lines_get_nselected  (GwyDataViewLayer *layer);
void             gwy_layer_lines_unselect       (GwyDataViewLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_LAYER_LINES_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

