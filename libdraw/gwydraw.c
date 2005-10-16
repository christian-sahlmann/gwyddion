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
#include <libprocess/gwyprocess.h>
#include "gwydraw.h"

void _gwy_gradient_class_setup_presets   (void);
void _gwy_gl_material_class_setup_presets(void);

static guint types_initialized = 0;

/**
 * gwy_draw_type_init:
 *
 * Makes libgwydraw types safe for deserialization and performs other
 * initialization.  You have to call this function before using objects
 * from libgwydraw.
 *
 * Calls gwy_process_type_init() first to make sure libgwyprocess is
 * initialized.
 *
 * It is safe to call this function more than once, subsequent calls are no-op.
 **/
void
gwy_draw_type_init(void)
{
    if (types_initialized)
        return;

    gwy_process_type_init();

    types_initialized += gwy_rgba_get_type();
    types_initialized += gwy_gradient_get_type();
    types_initialized += gwy_selection_get_type();
    types_initialized |= 1;

    _gwy_gradient_class_setup_presets();
    _gwy_gl_material_class_setup_presets();
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */


