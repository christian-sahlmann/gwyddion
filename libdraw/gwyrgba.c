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

#include <glib-object.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include "gwyrgba.h"

GType
gwy_rgba_get_type (void)
{
  static GType rgba_type = 0;

  if (rgba_type == 0)
    rgba_type = g_boxed_type_register_static("GwyRGBA",
                                            (GBoxedCopyFunc)gwy_rgba_copy,
                                            (GBoxedFreeFunc)gwy_rgba_free);
  return rgba_type;
}

/**
 * gwy_rgba_copy:
 * @rgba: A #GwyRGBA.
 *
 * Makes a copy of a rgba structure. The result must be freed using
 * gwy_rgba_free().
 *
 * XXX: Just kidding, we curently dont' use memchunks.
 *
 * Returns: A copy of @rgba.
 *
 * Since: 1.3.
 **/
GwyRGBA*
gwy_rgba_copy(const GwyRGBA *rgba)
{
    g_return_val_if_fail(rgba, NULL);
    return g_memdup(rgba, sizeof(GwyRGBA));
}

/**
 * gwy_rgba_free:
 * @rgba: A #GwyRGBA.
 *
 * Frees an rgba structure created with gwy_rgba_copy().
 *
 * Since: 1.3.
 **/
void
gwy_rgba_free(GwyRGBA *rgba)
{
    g_return_if_fail(rgba);
    g_free(rgba);
}

/**
 * gwy_rgba_to_gdk_color:
 * @rgba: A #GwyRGBA.
 * @gdkcolor: A #GdkColor.
 *
 * Converts a rgba to a Gdk color.
 *
 * Note no allocation is performed, just channel value conversion.
 *
 * Since: 1.3.
 **/
void
gwy_rgba_to_gdk_color(GwyRGBA *rgba,
                      GdkColor *gdkcolor)
{
    gdkcolor->red   = (guint16)floor(rgba->r*65535.999999);
    gdkcolor->green = (guint16)floor(rgba->g*65535.999999);
    gdkcolor->blue  = (guint16)floor(rgba->b*65535.999999);
    gdkcolor->pixel = (guint32)-1;
}

/**
 * gwy_rgba_to_gdk_alpha:
 * @rgba: A #GwyRGBA.
 *
 * Converts a rgba to a Gdk opacity value.
 *
 * Returns: The opacity value as a 16bit integer.
 *
 * Since: 1.3.
 **/
guint16
gwy_rgba_to_gdk_alpha(GwyRGBA *rgba)
{
    return (guint16)floor(rgba->a*65535.999999);
}

/**
 * gwy_rgba_from_gdk_color:
 * @rgba: A #GwyRGBA.
 * @gdkcolor: A #GdkColor.
 *
 * Converts a Gdk color to a rgba.
 *
 * The alpha value is unchanged, as #GdkColor has no opacity information.
 *
 * Since: 1.3.
 **/
void
gwy_rgba_from_gdk_color(GwyRGBA *rgba,
                        GdkColor *gdkcolor)
{
    rgba->r = gdkcolor->red/65535.0;
    rgba->g = gdkcolor->green/65535.0;
    rgba->b = gdkcolor->blue/65535.0;
}

/**
 * gwy_rgba_from_gdk_color_and_alpha:
 * @rgba: A #GwyRGBA.
 * @alpha: Gdk 16bit opacity value.
 *
 * Converts a Gdk color plus an opacity value to a rgba.
 **/
void
gwy_rgba_from_gdk_color_and_alpha(GwyRGBA *rgba,
                                  GdkColor *gdkcolor,
                                  guint16 gdkalpha)
{
    rgba->r = gdkcolor->red/65535.0;
    rgba->g = gdkcolor->green/65535.0;
    rgba->b = gdkcolor->blue/65535.0;
    rgba->a = gdkalpha/65535.0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
