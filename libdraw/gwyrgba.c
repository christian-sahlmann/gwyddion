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
 **/
void
gwy_rgba_free(GwyRGBA *rgba)
{
    g_return_if_fail(rgba);
    g_free(rgba);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
