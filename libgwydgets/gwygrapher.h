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

#ifndef __GTK_GGPLOT_H__
#define __GTK_GGPLOT_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktable.h>

#include <libprocess/dataline.h>

#include "gwyaxiser.h"
#include "gwygrapherbasics.h"
#include "gwygrapherlabel.h"
#include "gwygraphercorner.h"
#include "gwygrapherarea.h"

G_BEGIN_DECLS

#define GWY_TYPE_GRAPHER            (gwy_grapher_get_type())
#define GWY_GRAPHER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPHER, GwyGrapher))
#define GWY_GRAPHER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPHER, GwyGrapher))
#define GWY_IS_GRAPHER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPHER))
#define GWY_IS_GRAPHER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPHER))
#define GWY_GRAPHER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPHER, GwyGrapherClass))

typedef struct _GwyGrapher      GwyGrapher;
typedef struct _GwyGrapherClass GwyGrapherClass;

typedef struct {
    gboolean is_line;
    gboolean is_point;
    gint line_size;
    gint point_size;
    GdkColor color;
} GwyGrapherAutoProperties;


struct _GwyGrapher {
    GtkTable table;

    GwyAxiser *axis_top;
    GwyAxiser *axis_left;
    GwyAxiser *axis_right;
    GwyAxiser *axis_bottom;

    GwyGrapherCorner *corner_tl;
    GwyGrapherCorner *corner_bl;
    GwyGrapherCorner *corner_tr;
    GwyGrapherCorner *corner_br;

    GwyGrapherArea *area;

    gpointer grapher_model;

    gint n_of_autocurves;
    GwyGrapherAutoProperties autoproperties;


    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyGrapherClass {
    GtkTableClass parent_class;

    void (*gwygrapher)(GwyGrapher *grapher);

    gpointer reserved1;
    gpointer reserved2;
};

GtkWidget *gwy_grapher_new();
GType      gwy_grapher_get_type(void) G_GNUC_CONST;

void       gwy_grapher_refresh(GwyGrapher *grapher);

void       gwy_grapher_change_model(GwyGrapher *grapher, 
                                    gpointer *gmodel);

G_END_DECLS

#endif /* __GWY_GRADSPHERE_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
