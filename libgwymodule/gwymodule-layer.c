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

#include "gwymoduleinternal.h"
#include "gwymodule-layer.h"

static GHashTable *layer_funcs = NULL;

static void gwy_layer_func_info_free(gpointer data);

/**
 * gwy_layer_func_register:
 * @modname: Module identifier (name).
 * @func_info: Layer function info.
 *
 * Registeres a layer function.
 *
 * To keep compatibility with old versions @func_info should not be an
 * automatic variable.  However, since 1.6 it keeps a copy of @func_info.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_layer_func_register(const gchar *modname,
                        GwyLayerFuncInfo *func_info)
{
    GwyLayerFuncInfo *lfinfo;

    gwy_debug("name = %s", func_info->name);

    if (!layer_funcs) {
        gwy_debug("Initializing...");
        layer_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            NULL, gwy_layer_func_info_free);
    }

    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->type, FALSE);
    if (g_hash_table_lookup(layer_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    g_type_class_ref(func_info->type);

    lfinfo = g_memdup(func_info, sizeof(GwyLayerFuncInfo));
    lfinfo->name = g_strdup(func_info->name);

    g_hash_table_insert(layer_funcs, (gpointer)lfinfo->name, lfinfo);
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_LAYER, lfinfo->name)) {
        g_hash_table_remove(layer_funcs, (gpointer)lfinfo->name);
        return FALSE;
    }

    return TRUE;
}

static void
gwy_layer_func_info_free(gpointer data)
{
    GwyLayerFuncInfo *lfinfo = (GwyLayerFuncInfo*)data;

    g_free((gpointer)lfinfo->name);
    g_free(lfinfo);
}

gboolean
_gwy_layer_func_remove(const gchar *name)
{
    GwyLayerFuncInfo *lfinfo;

    gwy_debug("%s", name);
    lfinfo = g_hash_table_lookup(layer_funcs, name);
    if (!lfinfo) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }

    g_type_class_unref(g_type_class_peek(lfinfo->type));
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

/**
 * GwyLayerFuncInfo:
 * @name: An unique data layer type name (GwyLayerSomething is preferred).
 * @type: The type as obtained from gwy_layer_something_get_type().
 *
 * Information about one layer function.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
