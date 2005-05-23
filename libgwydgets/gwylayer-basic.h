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

#ifndef __GWY_LAYER_BASIC_H__
#define __GWY_LAYER_BASIC_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#include <libgwydgets/gwypixmaplayer.h>
#include <libdraw/gwygradient.h>

G_BEGIN_DECLS

#define GWY_TYPE_LAYER_BASIC            (gwy_layer_basic_get_type())
#define GWY_LAYER_BASIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasic))
#define GWY_LAYER_BASIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))
#define GWY_IS_LAYER_BASIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_BASIC))
#define GWY_IS_LAYER_BASIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_BASIC))
#define GWY_LAYER_BASIC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))

typedef struct _GwyLayerBasic      GwyLayerBasic;
typedef struct _GwyLayerBasicClass GwyLayerBasicClass;

struct _GwyLayerBasic {
    GwyPixmapLayer parent_instance;

    GwyGradient *gradient;
    gulong gradient_id;

    gpointer reserved2;
};

struct _GwyLayerBasicClass {
    GwyPixmapLayerClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType            gwy_layer_basic_get_type        (void) G_GNUC_CONST;

GwyPixmapLayer*  gwy_layer_basic_new             (void);
void             gwy_layer_basic_set_gradient    (GwyLayerBasic *layer,
                                                  const gchar *gradient);
const gchar*     gwy_layer_basic_get_gradient    (GwyLayerBasic *layer);

G_END_DECLS

#endif /* __GWY_LAYER_BASIC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

