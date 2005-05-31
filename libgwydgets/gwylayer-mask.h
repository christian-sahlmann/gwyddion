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

#ifndef __GWY_LAYER_MASK_H__
#define __GWY_LAYER_MASK_H__

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

#include <libgwydgets/gwypixmaplayer.h>

G_BEGIN_DECLS

#define GWY_TYPE_LAYER_MASK            (gwy_layer_mask_get_type())
#define GWY_LAYER_MASK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GWY_TYPE_LAYER_MASK, GwyLayerMask))
#define GWY_LAYER_MASK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GWY_TYPE_LAYER_MASK, GwyLayerMaskClass))
#define GWY_IS_LAYER_MASK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GWY_TYPE_LAYER_MASK))
#define GWY_IS_LAYER_MASK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GWY_TYPE_LAYER_MASK))
#define GWY_LAYER_MASK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GWY_TYPE_LAYER_MASK, GwyLayerMaskClass))

typedef struct _GwyLayerMask      GwyLayerMask;
typedef struct _GwyLayerMaskClass GwyLayerMaskClass;

struct _GwyLayerMask {
    GwyPixmapLayer parent_instance;

    GQuark color_key;
    gulong red_id;
    gulong green_id;
    gulong blue_id;
    gulong alpha_id;

    gpointer reserved1;
    gpointer reserved2;
};

struct _GwyLayerMaskClass {
    GwyPixmapLayerClass parent_class;

    gpointer reserved1;
    gpointer reserved2;
};

GType            gwy_layer_mask_get_type        (void) G_GNUC_CONST;

GwyPixmapLayer*  gwy_layer_mask_new             (void);
void             gwy_layer_mask_set_color_key   (GwyLayerMask *mask_layer,
                                                 const gchar *prefix);
const gchar*     gwy_layer_mask_get_color_key   (GwyLayerMask *mask_layer);
GwyRGBA          gwy_layer_mask_get_color       (GwyLayerMask *mask_layer);

G_END_DECLS

#endif /* __GWY_LAYER_MASK_H__ */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

