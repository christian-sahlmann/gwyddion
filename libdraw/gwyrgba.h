/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004 David Necas (Yeti), Petr Klapetek.
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

#ifndef __GWY_RGBA_H__
#define __GWY_RGBA_H__

#include <gdk/gdkcolor.h>

G_BEGIN_DECLS

#define GWY_TYPE_RGBA                         (gwy_rgba_get_type())

typedef struct _GwyRGBA GwyRGBA;

struct _GwyRGBA {
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
};

GType         gwy_rgba_get_type                 (void) G_GNUC_CONST;
GwyRGBA*      gwy_rgba_copy                     (const GwyRGBA *color);
void          gwy_rgba_free                     (GwyRGBA *color);
void          gwy_rgba_to_gdk_color             (GwyRGBA *rgba,
                                                 GdkColor *gdkcolor);
guint16       gwy_rgba_to_gdk_alpha             (GwyRGBA *rgba);
void          gwy_rgba_from_gdk_color           (GwyRGBA *rgba,
                                                 GdkColor *gdkcolor);
void          gwy_rgba_from_gdk_color_and_alpha (GwyRGBA *rgba,
                                                 GdkColor *gdkcolor,
                                                 guint16 gdkalpha);

G_END_DECLS


#endif /*__GWY_PALETTE_H__*/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
