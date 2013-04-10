/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2004,2013 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include "gwymoduleinternal.h"
#include "gwymodule-volume.h"

/* The volume function information */
typedef struct {
    const gchar *name;
    const gchar *menu_path;
    const gchar *stock_id;
    const gchar *tooltip;
    GwyRunType run;
    guint sens_mask;
    GwyVolumeFunc func;
} GwyVolumeFuncInfo;

/* Auxiliary structure to pass both user callback function and data to
 * g_hash_table_foreach() lambda argument in gwy_volume_func_foreach() */
typedef struct {
    GFunc function;
    gpointer user_data;
} ProcFuncForeachData;

static GHashTable *volume_funcs = NULL;

/**
 * gwy_volume_func_register:
 * @name: Name of function to register.  It should be a valid identifier and
 *        if a module registers only one function, module and function names
 *        should be the same.
 * @func: The function itself.
 * @menu_path: Menu path under Volume Data menu.  The menu path should be
 *             marked translatabe, but passed untranslated (to allow merging
 *             of translated and untranslated submenus).
 * @stock_id: Stock icon id for toolbar.
 * @run: Supported run modes.  Volume data processing functions can have two run
 *       modes: %GWY_RUN_IMMEDIATE (no questions asked) and
 *       %GWY_RUN_INTERACTIVE (a modal dialog with parameters).
 * @sens_mask: Sensitivity mask (a combination of #GwyMenuSensFlags flags).
 *             Usually it contains #GWY_MENU_FLAG_DATA, possibly other
 *             requirements.
 * @tooltip: Tooltip for this function.
 *
 * Registers a volume data processing function.
 *
 * Note: the string arguments are not copied as modules are not expected to
 * vanish.  If they are constructed (non-constant) strings, do not free them.
 * Should modules ever become unloadable they will get a chance to clean-up.
 *
 * Returns: Normally %TRUE; %FALSE on failure.
 **/
gboolean
gwy_volume_func_register(const gchar *name,
                         GwyVolumeFunc func,
                         const gchar *menu_path,
                         const gchar *stock_id,
                         GwyRunType run,
                         guint sens_mask,
                         const gchar *tooltip)
{
    GwyVolumeFuncInfo *func_info;

    g_return_val_if_fail(name, FALSE);
    g_return_val_if_fail(func, FALSE);
    g_return_val_if_fail(menu_path, FALSE);
    g_return_val_if_fail(run & GWY_RUN_MASK, FALSE);
    gwy_debug("name = %s, menu path = %s, run = %d, func = %p",
              name, menu_path, run, func);

    if (!volume_funcs) {
        gwy_debug("Initializing...");
        volume_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              NULL, g_free);
    }

    if (!gwy_strisident(name, "_-", NULL))
        g_warning("Function name `%s' is not a valid identifier. "
                  "It may be rejected in future.", name);
    if (g_hash_table_lookup(volume_funcs, name)) {
        g_warning("Duplicate function `%s', keeping only first", name);
        return FALSE;
    }

    func_info = g_new0(GwyVolumeFuncInfo, 1);
    func_info->name = name;
    func_info->func = func;
    func_info->menu_path = menu_path;
    func_info->stock_id = stock_id;
    func_info->tooltip = tooltip;
    func_info->run = run;
    func_info->sens_mask = sens_mask;

    g_hash_table_insert(volume_funcs, (gpointer)func_info->name, func_info);
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_VOLUME, name)) {
        g_hash_table_remove(volume_funcs, func_info->name);
        return FALSE;
    }

    return TRUE;
}

/**
 * gwy_volume_func_run:
 * @name: Volume data processing function name.
 * @data: Data (a #GwyContainer).
 * @run: How the function should be run.
 *
 * Runs a volume data processing function identified by @name.
 **/
void
gwy_volume_func_run(const gchar *name,
                    GwyContainer *data,
                    GwyRunType run)
{
    GwyVolumeFuncInfo *func_info;

    func_info = g_hash_table_lookup(volume_funcs, name);
    g_return_if_fail(run & func_info->run);
    func_info->func(data, run, name);
}

static void
gwy_volume_func_user_cb(gpointer key,
                        G_GNUC_UNUSED gpointer value,
                        gpointer user_data)
{
    ProcFuncForeachData *pffd = (ProcFuncForeachData*)user_data;

    pffd->function(key, pffd->user_data);
}

/**
 * gwy_volume_func_foreach:
 * @function: Function to run for each volume function.  It will get function
 *            name (constant string owned by module system) as its first
 *            argument, @user_data as the second argument.
 * @user_data: Data to pass to @function.
 *
 * Calls a function for each volume function.
 **/
void
gwy_volume_func_foreach(GFunc function,
                        gpointer user_data)
{
    ProcFuncForeachData pffd;

    if (!volume_funcs)
        return;

    pffd.user_data = user_data;
    pffd.function = function;
    g_hash_table_foreach(volume_funcs, gwy_volume_func_user_cb, &pffd);
}

/**
 * gwy_volume_func_exists:
 * @name: Volume data processing function name.
 *
 * Checks whether a volume data processing function exists.
 *
 * Returns: %TRUE if function @name exists, %FALSE otherwise.
 **/
gboolean
gwy_volume_func_exists(const gchar *name)
{
    return volume_funcs && g_hash_table_lookup(volume_funcs, name);
}

/**
 * gwy_volume_func_get_run_types:
 * @name: Volume data processing function name.
 *
 * Returns run modes supported by a volume data processing function.
 *
 * Returns: The run mode bit mask.
 **/
GwyRunType
gwy_volume_func_get_run_types(const gchar *name)
{
    GwyVolumeFuncInfo *func_info;

    func_info = g_hash_table_lookup(volume_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->run;
}

/**
 * gwy_volume_func_get_menu_path:
 * @name: Volume data processing function name.
 *
 * Returns the menu path of a volume data processing function.
 *
 * The returned menu path is only the tail part registered by the function,
 * i.e., without any leading "/Volume Data".
 *
 * Returns: The menu path.  The returned string is owned by the module.
 **/
const gchar*
gwy_volume_func_get_menu_path(const gchar *name)
{
    GwyVolumeFuncInfo *func_info;

    func_info = g_hash_table_lookup(volume_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->menu_path;
}

/**
 * gwy_volume_func_get_stock_id:
 * @name: Volume data processing function name.
 *
 * Gets stock icon id of a volume data processing  function.
 *
 * Returns: The stock icon id.  The returned string is owned by the module.
 **/
const gchar*
gwy_volume_func_get_stock_id(const gchar *name)
{
    GwyVolumeFuncInfo *func_info;

    g_return_val_if_fail(volume_funcs, NULL);
    func_info = g_hash_table_lookup(volume_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->stock_id;
}

/**
 * gwy_volume_func_get_tooltip:
 * @name: Volume data processing function name.
 *
 * Gets tooltip for a volume data processing function.
 *
 * Returns: The tooltip.  The returned string is owned by the module.
 **/
const gchar*
gwy_volume_func_get_tooltip(const gchar *name)
{
    GwyVolumeFuncInfo *func_info;

    g_return_val_if_fail(volume_funcs, NULL);
    func_info = g_hash_table_lookup(volume_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->tooltip;
}

/**
 * gwy_volume_func_get_sensitivity_mask:
 * @name: Volume data processing function name.
 *
 * Gets menu sensititivy mask for a volume data processing function.
 *
 * Returns: The menu item sensitivity mask (a combination of #GwyMenuSensFlags
 *          flags).
 **/
guint
gwy_volume_func_get_sensitivity_mask(const gchar *name)
{
    GwyVolumeFuncInfo *func_info;

    func_info = g_hash_table_lookup(volume_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->sens_mask;
}

gboolean
_gwy_volume_func_remove(const gchar *name)
{
    gwy_debug("%s", name);
    if (!g_hash_table_remove(volume_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodule-volume
 * @title: gwymodule-volume
 * @short_description: Volume data processing modules
 *
 * Volume data processing modules implement function processing volume data
 * represented with #GwyBrick.  They reigster functions that get a
 * #GwyContainer with data and either modify it or create a new data from it.
 * In this regard, they are quite similar to regular (two-dimensional) data
 * processing functions but they live in separate menus, toolbars, etc.
 **/

/**
 * GwyVolumeFuncInfo:
 * @name: An unique volume data processing function name.
 * @menu_path: A path under "/Volume Data" where the function should appear.
 *             It must start with "/".
 * @volume: The function itself.
 * @run: Possible run-modes for this function.
 * @sens_flags: Sensitivity flags.  Volume data processing function should
 *              include, in general, %GWY_MENU_FLAG_VOLUME.  Functions
 *              constructing synthetic data from nothing do not have to specify
 *              even %GWY_MENU_FLAG_VOLUME.
 *
 * Information about one volume data processing function.
 **/

/**
 * GwyVolumeFunc:
 * @data: The data container to operate on.
 * @run: Run mode.
 * @name: Function name from as registered with gwy_volume_func_register()
 *        (single-function modules can safely ignore this argument).
 *
 * The type of volume data processing function.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
