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
#include <libprocess/datafield.h>
#include "gwymoduleinternal.h"
#include "gwymodule-process.h"

/* The process function information */
typedef struct {
    const gchar *name;
    const gchar *menu_path;
    const gchar *stock_id;
    const gchar *tooltip;
    GwyRunType run;
    guint sens_mask;
    GwyProcessFunc func;
} GwyProcessFuncInfo;

/* Auxiliary structure to pass both user callback function and data to
 * g_hash_table_foreach() lambda argument in gwy_process_func_foreach() */
typedef struct {
    GFunc function;
    gpointer user_data;
} ProcFuncForeachData;

static GHashTable *process_funcs = NULL;

/**
 * gwy_process_func_register:
 * @name: Name of function to register.  It should be a valid identifier.
 * @func: The function itself.
 * @menu_path: Menu path under Data Process menu.
 * @stock_id: Stock icon id for toolbar.
 * @run: Supported run modes.
 * @sens_mask: Sensitivity mask (a combination of #GwyMenuSensFlags flags).
 *             Usually it contains #GWY_MENU_FLAG_DATA, possibly other
 *             requirements.
 * @tooltip: Tooltip for this function.
 *
 * Registers a data processing function.
 *
 * Note: the string arguments are not copied as modules are not expected to
 * vanish.  If they are constructed (non-constant) strings, do not free them.
 * Should modules ever become unloadable they will get chance to clean-up.
 *
 * Returns: Normally %TRUE; %FALSE on failure.
 **/
gboolean
gwy_process_func_register(const gchar *name,
                          GwyProcessFunc func,
                          const gchar *menu_path,
                          const gchar *stock_id,
                          GwyRunType run,
                          guint sens_mask,
                          const gchar *tooltip)
{
    GwyProcessFuncInfo *func_info;

    g_return_val_if_fail(name, FALSE);
    g_return_val_if_fail(func, FALSE);
    g_return_val_if_fail(menu_path, FALSE);
    g_return_val_if_fail(run & GWY_RUN_MASK, FALSE);
    gwy_debug("name = %s, menu path = %s, run = %d, func = %p",
              name, menu_path, run, func);

    if (!process_funcs) {
        gwy_debug("Initializing...");
        process_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              NULL, g_free);
    }

    if (!gwy_strisident(name, "_-", NULL))
        g_warning("Function name `%s' is not a valid identifier. "
                  "It may be rejected in future.", name);
    if (g_hash_table_lookup(process_funcs, name)) {
        g_warning("Duplicate function `%s', keeping only first", name);
        return FALSE;
    }

    func_info = g_new0(GwyProcessFuncInfo, 1);
    func_info->name = name;
    func_info->func = func;
    func_info->menu_path = menu_path;
    func_info->stock_id = stock_id;
    func_info->tooltip = tooltip;
    func_info->run = run;
    func_info->sens_mask = sens_mask;

    g_hash_table_insert(process_funcs, (gpointer)func_info->name, func_info);
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_PROC, name)) {
        g_hash_table_remove(process_funcs, func_info->name);
        return FALSE;
    }

    return TRUE;
}

/**
 * gwy_process_func_run:
 * @name: Data processing function name.
 * @data: Data (a #GwyContainer).
 * @run: How the function should be run.
 *
 * Runs a data processing function identified by @name.
 **/
void
gwy_process_func_run(const guchar *name,
                     GwyContainer *data,
                     GwyRunType run)
{
    GwyProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_if_fail(run & func_info->run);
    g_return_if_fail(GWY_IS_CONTAINER(data));
    func_info->func(data, run, name);
}

static void
gwy_process_func_user_cb(gpointer key,
                         G_GNUC_UNUSED gpointer value,
                         gpointer user_data)
{
    ProcFuncForeachData *pffd = (ProcFuncForeachData*)user_data;

    pffd->function(key, pffd->user_data);
}

/**
 * gwy_process_func_foreach:
 * @function: Function to run for each process function.  It will get function
 *            name (constant string owned by module system) as its first
 *            argument, @user_data as the second argument.
 * @user_data: Data to pass to @function.
 *
 * Calls a function for each process function.
 **/
void
gwy_process_func_foreach(GFunc function,
                         gpointer user_data)
{
    ProcFuncForeachData pffd;

    if (!process_funcs)
        return;

    pffd.user_data = user_data;
    pffd.function = function;
    g_hash_table_foreach(process_funcs, gwy_process_func_user_cb, &pffd);
}

/**
 * gwy_process_func_exists:
 * @name: Data processing function name.
 *
 * Checks whether a data processing function exists.
 *
 * Returns: %TRUE if function @name exists, %FALSE otherwise.
 **/
gboolean
gwy_process_func_exists(const gchar *name)
{
    return process_funcs && g_hash_table_lookup(process_funcs, name);
}

/**
 * gwy_process_func_get_run_types:
 * @name: Data processing function name.
 *
 * Returns run modes supported by a data processing function.
 *
 * Returns: The run mode bit mask.
 **/
GwyRunType
gwy_process_func_get_run_types(const gchar *name)
{
    GwyProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->run;
}

/**
 * gwy_process_func_get_menu_path:
 * @name: Data processing function name.
 *
 * Returns the menu path of a data processing function.
 *
 * The returned menu path is only the tail part registered by the function,
 * i.e., without any leading "/Data Process".
 *
 * Returns: The menu path.  The returned string is owned by the module.
 **/
const gchar*
gwy_process_func_get_menu_path(const gchar *name)
{
    GwyProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->menu_path;
}

/**
 * gwy_process_func_get_stock_id:
 * @name: Data processing function name.
 *
 * Gets stock icon id of a data processing  function.
 *
 * Returns: The stock icon id.  The returned string is owned by the module.
 **/
const gchar*
gwy_process_func_get_stock_id(const gchar *name)
{
    GwyProcessFuncInfo *func_info;

    g_return_val_if_fail(process_funcs, NULL);
    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->stock_id;
}

/**
 * gwy_process_func_get_tooltip:
 * @name: Data processing function name.
 *
 * Gets tooltip for a data processing function.
 *
 * Returns: The tooltip.  The returned string is owned by the module.
 **/
const gchar*
gwy_process_func_get_tooltip(const gchar *name)
{
    GwyProcessFuncInfo *func_info;

    g_return_val_if_fail(process_funcs, NULL);
    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->tooltip;
}

/**
 * gwy_process_func_get_sensitivity_mask:
 * @name: Data processing function name.
 *
 * Gets menu sensititivy mask for a data processing function.
 *
 * Returns: The menu item sensitivity mask (a combination of #GwyMenuSensFlags
 *          flags).
 **/
guint
gwy_process_func_get_sensitivity_mask(const gchar *name)
{
    GwyProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->sens_mask;
}

gboolean
_gwy_process_func_remove(const gchar *name)
{
    gwy_debug("%s", name);
    if (!g_hash_table_remove(process_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodule-process
 * @title: gwymodule-process
 * @short_description: Data processing modules
 *
 * Data processing modules implement the actual ability to do something useful
 * with data. They reigster functions that get a #GwyContainer with data
 * and either modify it or create a new data from it.
 **/

/**
 * GwyProcessFuncInfo:
 * @name: An unique data processing function name.
 * @menu_path: A path under "/Data Process" where the function should appear.
 *             It must start with "/".
 * @process: The function itself.
 * @run: Possible run-modes for this function.
 * @sens_flags: Sensitivity flags.  All data processing function have implied
 *       %GWY_MENU_FLAG_DATA flag which cannot be removed.  You can specify
 *       additional flags here, the most common (and most useful) probably
 *       is %GWY_MENU_FLAG_DATA_MASK meaning the function requires a mask.
 *
 * Information about one data processing function.
 **/

/**
 * GwyProcessFunc:
 * @data: The data container to operate on.
 * @run: Run mode.
 * @name: Function name from as registered with gwy_process_func_register()
 *        (single-function modules can safely ignore this argument).
 *
 * The type of data processing function.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
