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

#include "config.h"

#include <libgwyddion/gwyddion.h>
#include <libgwymodule/gwymodule.h>
#include <libgwydgets/gwydgets.h>
#include <libprocess/libgwyprocess.h>

static GType optimization_fooler = 0;
static GSList *palettes = NULL;

static void ref_palette      (const gchar *name,
                              GwyPaletteDef *pdef);
static void unref_palettes   (void);
/**
 * gwy_type_init:
 *
 * Initializes all Gwyddion data types, i.e. types that may appear in
 * serialized data. GObject has to know about them when g_type_from_name()
 * is called.
 *
 * XXX: This function does much more. It registeres stock items, setups
 * palette presets, and similar things.
 **/
void
gwy_type_init(void)
{
    g_assert(palettes == NULL);

    optimization_fooler += gwy_sphere_coords_get_type();
    optimization_fooler += gwy_data_field_get_type();
    optimization_fooler += gwy_data_line_get_type();
    optimization_fooler += gwy_palette_get_type();
    optimization_fooler += gwy_palette_def_get_type();
    optimization_fooler += gwy_container_get_type();

    g_set_application_name(_(PACKAGE_NAME));
    gwy_palette_def_setup_presets();
    gwy_palette_def_foreach((GwyPaletteDefFunc)ref_palette, NULL);
    g_atexit(unref_palettes);
    gwy_stock_register_stock_items();
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL);
}

static void
ref_palette(const gchar *name,
            GwyPaletteDef *pdef)
{
    GwyPalette *palette;

    palette = gwy_palette_new(pdef);
    palettes = g_slist_prepend(palettes, palette);
}

static void
unref_palettes(void)
{
    GSList *l;

    for (l = palettes; l; l = g_slist_next(l))
        gwy_object_unref(l->data);
    g_slist_free(palettes);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
