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

#ifndef __GWY_GRAPH_LABEL_H__
#define __GWY_GRAPH_LABEL_H__

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GWY_TYPE_GRAPH_LABEL            (gwy_graph_label_get_type())
#define GWY_GRAPH_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_GRAPH_LABEL, GwyGraphLabel))
#define GWY_GRAPH_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_GRAPH_LABEL, GwyGraphLabel))
#define GWY_IS_GRAPH_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_GRAPH_LABEL))
#define GWY_IS_GRAPH_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_GRAPH_LABEL))
#define GWY_GRAPH_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_GRAPH_LABEL, GwyGraphLabelClass))

typedef struct _GwyGraphLabel      GwyGraphLabel;
typedef struct _GwyGraphLabelClass GwyGraphLabelClass;


struct _GwyGraphLabel {
    GtkWidget widget;

    PangoFontDescription *label_font;
    gpointer graph_model;

    gint *samplepos;
    gint reqheight;
    gint reqwidth;

    gboolean enable_user_input;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyGraphLabelClass {
    GtkWidgetClass parent_class;

    void (*selected)(GwyGraphLabel *label);
    
    gpointer reserved1;
    gpointer reserved2;
};


GtkWidget* gwy_graph_label_new();

GType gwy_graph_label_get_type(void) G_GNUC_CONST;

void gwy_graph_label_refresh(GwyGraphLabel *label);

void gwy_graph_label_change_model(GwyGraphLabel *label, gpointer gmodel);

void gwy_graph_label_draw_label_on_drawable(GdkDrawable *drawable, GdkGC *gc, PangoLayout *layout,
                                              gint x, gint y, gint width, gint height,
                                              GwyGraphLabel *label);
GString* gwy_graph_label_export_vector(GwyGraphLabel *label,
                                       gint x, gint y,
                                       gint width, gint height,
                                       gint fontsize);


void gwy_graph_label_enable_user_input(GwyGraphLabel *label, gboolean enable);

G_END_DECLS

#endif /*__GWY_AXIS_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
