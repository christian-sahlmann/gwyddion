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

#ifndef __GWY_DATAVIEWLAYER_H__
#define __GWY_DATAVIEWLAYER_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_CONTAINER
#  include <libgwyddion/gwycontainer.h>
#endif /* no GWY_TYPE_CONTAINER */

G_BEGIN_DECLS

#define GWY_TYPE_DATA_VIEW_LAYER            (gwy_data_view_layer_get_type())
#define GWY_DATA_VIEW_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_DATA_VIEW_LAYER, GwyDataViewLayer))
#define GWY_DATA_VIEW_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_DATA_VIEW_LAYER, GwyDataViewLayerClass))
#define GWY_IS_DATA_VIEW_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_DATA_VIEW_LAYER))
#define GWY_IS_DATA_VIEW_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_DATA_VIEW_LAYER))
#define GWY_DATA_VIEW_LAYER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_DATA_VIEW_LAYER, GwyDataViewLayerClass))

typedef struct _GwyDataViewLayer      GwyDataViewLayer;
typedef struct _GwyDataViewLayerClass GwyDataViewLayerClass;

struct _GwyDataViewLayer {
    GtkObject parent_instance;

    GtkWidget *parent;
    GwyContainer *data;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyDataViewLayerClass {
    GtkObjectClass parent_class;

    /* XXX: Move to pixmap layer class, vector layers don't implement it */
    gboolean (*wants_repaint)(GwyDataViewLayer *layer);
    /* signal functions */
    void (*plugged)(GwyDataViewLayer *layer);
    void (*unplugged)(GwyDataViewLayer *layer);
    void (*updated)(GwyDataViewLayer *layer);

    gpointer reserved1;
    gpointer reserved2;
};

GType            gwy_data_view_layer_get_type        (void) G_GNUC_CONST;
gboolean         gwy_data_view_layer_wants_repaint   (GwyDataViewLayer *layer);
void             gwy_data_view_layer_plugged         (GwyDataViewLayer *layer);
void             gwy_data_view_layer_unplugged       (GwyDataViewLayer *layer);
void             gwy_data_view_layer_updated         (GwyDataViewLayer *layer);

G_END_DECLS

#endif /* __GWY_DATAVIEWLAYER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

