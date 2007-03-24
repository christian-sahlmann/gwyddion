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

#include "config.h"
#include <libgwyddion/gwyddion.h>
#include "gwyddioninternal.h"

/**
 * gwy_type_init:
 *
 * Makes libgwyddion types safe for deserialization and performs other
 * initialization.  You have to call this function before using objects
 * from libgwyddion.
 *
 * Calls g_type_init() first to make sure GLib object system is initialized.
 *
 * It is safe to call this function more than once, subsequent calls are no-op.
 **/
void
gwy_type_init(void)
{
    static gboolean types_initialized = FALSE;

    if (types_initialized)
        return;

    g_type_init();

    g_type_class_peek(GWY_TYPE_SI_UNIT);
    g_type_class_peek(GWY_TYPE_CONTAINER);
    g_type_class_peek(GWY_TYPE_INVENTORY);
    g_type_class_peek(GWY_TYPE_RESOURCE);
    g_type_class_peek(GWY_TYPE_NLFIT_PRESET);
    g_type_class_peek(GWY_TYPE_FD_CURVE_PRESET);
    g_type_class_peek(GWY_TYPE_STRING_LIST);
    types_initialized = gwy_enum_get_type();

    _gwy_nlfit_preset_class_setup_presets();
    _gwy_fd_curve_preset_class_setup_presets();
}

/************************** Documentation ****************************/

/**
 * SECTION:gwyddion
 * @title: gwyddion
 * @short_description: Base functions, library initialization
 * @see_also: #GwySerializable
 *
 * Gwyddion classes has to be initialized before they can be safely
 * deserialized. The function gwy_type_init() performs this initialization.
 **/

/**
 * SECTION:gwyddionenums
 * @title: gwyddionenums
 * @short_description: Common libgwyddion enumerations
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
