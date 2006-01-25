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

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwyvectorlayer.h>
#include <libgwymodule/gwymodule-layer.h>
#include "gwymoduleinternal.h"

static GHashTable *layer_funcs = NULL;

/**
 * gwy_layer_func_register:
 * @type: Layer type in GObject type system.  That is the return value of
 *        gwy_layer_foo_get_type().
 *
 * Registeres a layer function (layer type).
 *
 * Returns: Normally %TRUE; %FALSE on failure.
 **/
gboolean
gwy_layer_func_register(GType type)
{
    const gchar *name;

    g_return_val_if_fail(type, FALSE);
    name = g_type_name(type);
    gwy_debug("layer type = %s", name);

    if (!layer_funcs) {
        gwy_debug("Initializing...");
        layer_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            NULL, NULL);
    }

    if (g_hash_table_lookup(layer_funcs, name)) {
        g_warning("Duplicate type %s, keeping only first", name);
        return FALSE;
    }
    g_type_class_ref(type);
    g_hash_table_insert(layer_funcs, (gpointer)name, GUINT_TO_POINTER(type));
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_LAYER, name)) {
        g_hash_table_remove(layer_funcs, name);
        return FALSE;
    }

    return TRUE;
}

gboolean
_gwy_layer_func_remove(const gchar *name)
{
    GType type;

    gwy_debug("%s", name);
    type = GPOINTER_TO_UINT(g_hash_table_lookup(layer_funcs, name));
    if (!type) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }

    g_type_class_unref(g_type_class_peek(type));
    g_hash_table_remove(layer_funcs, name);
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodule-layer
 * @title: gwymodule-layer
 * @short_description: #GwyDataView layer modules
 *
 * Layer modules implement #GwyDataView layers, corresponding to different
 * kinds of selections.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
