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

#ifndef __GWY_PIXMAPLAYER_H__
#define __GWY_PIXMAPLAYER_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include "libgwydgets/gwydataviewlayer.h"
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_PIXMAP_LAYER            (gwy_pixmap_layer_get_type())
#define GWY_PIXMAP_LAYER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_PIXMAP_LAYER, GwyPixmapLayer))
#define GWY_PIXMAP_LAYER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_PIXMAP_LAYER, GwyPixmapLayerClass))
#define GWY_IS_PIXMAP_LAYER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_PIXMAP_LAYER))
#define GWY_IS_PIXMAP_LAYER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_PIXMAP_LAYER))
#define GWY_PIXMAP_LAYER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_PIXMAP_LAYER, GwyPixmapLayerClass))

typedef struct _GwyPixmapLayer      GwyPixmapLayer;
typedef struct _GwyPixmapLayerClass GwyPixmapLayerClass;

struct _GwyPixmapLayer {
    GwyDataViewLayer parent_instance;

    GdkPixbuf *pixbuf;
};

struct _GwyPixmapLayerClass {
    GwyDataViewLayerClass parent_class;

    /* renderers */
    GdkPixbuf* (*paint)(GwyPixmapLayer *layer);
};

GType            gwy_pixmap_layer_get_type        (void) G_GNUC_CONST;
GdkPixbuf*       gwy_pixmap_layer_paint           (GwyPixmapLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_PIXMAPLAYER_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

