/*
 *  @(#) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libgwydgets/gwyvectorlayer.h>

#include "gwymoduleinternal.h"
#include "gwymodule-layer.h"

static GHashTable *layer_funcs = NULL;
static void (*func_register_callback)(const gchar *fullname) = NULL;

enum { bufsize = 1024 };

/**
 * gwy_layer_func_register:
 * @modname: Module identifier (name).
 * @func_info: Layer function info.
 *
 * Registeres a layer function.
 *
 * The passed @func_info must not be an automatic variable.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_layer_func_register(const gchar *modname,
                       GwyLayerFuncInfo *func_info)
{
    _GwyModuleInfoInternal *iinfo;
    gchar *canon_name;

    gwy_debug("name = %s", func_info->name);

    if (!layer_funcs) {
        gwy_debug("Initializing...");
        layer_funcs = g_hash_table_new(g_str_hash, g_str_equal);
    }

    iinfo = gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->type, FALSE);
    if (g_hash_table_lookup(layer_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    g_hash_table_insert(layer_funcs, (gpointer)func_info->name, func_info);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_LAYER, func_info->name, NULL);
    iinfo->funcs = g_slist_append(iinfo->funcs, canon_name);
    if (func_register_callback)
        func_register_callback(canon_name);

    return TRUE;
}

void
_gwy_layer_func_set_register_callback(void (*callback)(const gchar *fullname))
{
    func_register_callback = callback;
}

gboolean
gwy_layer_func_remove(const gchar *name)
{
    gwy_debug("%s", name);
    if (!g_hash_table_remove(layer_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * GwyLayerFuncInfo:
 * @name: An unique data layer type name (GwyLayerSomething is preferred).
 * @type: The type as obtained from gwy_layer_something_get_type().
 *
 * Information about one layer function.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
