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

#ifndef __GWY_GRAPH_CORNER_H__
#define __GWY_GRAPH_CORNER_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GWY_TYPE_GRAPH_CORNER            (gwy_graph_corner_get_type())
#define GWY_GRAPH_CORNER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_CORNER, GwyGraphCorner))
#define GWY_GRAPH_CORNER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_CORNER, GwyGraphCorner))
#define GWY_IS_GRAPH_CORNER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_CORNER))
#define GWY_IS_GRAPH_CORNER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_CORNER))
#define GWY_GRAPH_CORNER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_CORNER, GwyGraphCornerClass))

typedef struct _GwyGraphCorner      GwyGraphCorner;
typedef struct _GwyGraphCornerClass GwyGraphCornerClass;

struct _GwyGraphCorner {
    GtkWidget widget;

};

struct _GwyGraphCornerClass {
     GtkWidgetClass parent_class;
};


GtkWidget* gwy_graph_corner_new();

GType gwy_graph_corner_get_type(void) G_GNUC_CONST;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__GWY_GRAPH_CORNER_H__*/
