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

#ifndef __GWY_LAYER_BASIC_H__
#define __GWY_LAYER_BASIC_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#ifndef GWY_TYPE_DATA_VIEW_LAYER
#  include <libgwydgets/gwydataviewlayer.h>
#endif /* no GWY_TYPE_DATA_VIEW_LAYER */

#ifndef GWY_TYPE_PALETTE
#  include <libdraw/gwypalette.h>
#endif /* no GWY_TYPE_PALETTE */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GWY_TYPE_LAYER_BASIC            (gwy_layer_basic_get_type())
#define GWY_LAYER_BASIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasic))
#define GWY_LAYER_BASIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))
#define GWY_IS_LAYER_BASIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_BASIC))
#define GWY_IS_LAYER_BASIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_BASIC))
#define GWY_LAYER_BASIC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_BASIC, GwyLayerBasicClass))

typedef struct _GwyLayerBasic      GwyLayerBasic;
typedef struct _GwyLayerBasicClass GwyLayerBasicClass;

struct _GwyLayerBasic {
    GwyDataViewLayer parent_instance;

    gboolean changed;
};

struct _GwyLayerBasicClass {
    GwyDataViewLayerClass parent_class;
};

GType            gwy_layer_basic_get_type        (void) G_GNUC_CONST;

GtkObject*       gwy_layer_basic_new             (void);
void             gwy_layer_basic_set_palette     (GwyDataViewLayer *layer,
                                                  GwyPalette *palette);
GwyPalette*      gwy_layer_basic_get_palette     (GwyDataViewLayer *layer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GWY_LAYER_BASIC_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

