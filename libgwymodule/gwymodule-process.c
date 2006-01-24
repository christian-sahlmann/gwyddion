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

typedef struct {
    GFunc function;
    gpointer user_data;
} ProcFuncForeachData;

static void gwy_process_func_info_free (gpointer data);

static GHashTable *process_funcs = NULL;

/**
 * gwy_process_func_register:
 * @modname: Module identifier (name).
 * @func_info: Data processing function info.
 *
 * Registeres a data processing function.
 *
 * To keep compatibility with old versions @func_info should not be an
 * automatic variable.  However, since 1.6 it keeps a copy of @func_info.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_process_func_register(const gchar *modname,
                          GwyProcessFuncInfo *func_info)
{
    GwyProcessFuncInfo *pfinfo;

    gwy_debug("");
    gwy_debug("name = %s, menu path = %s, run = %d, func = %p",
              func_info->name, func_info->menu_path, func_info->run,
              func_info->process);

    if (!process_funcs) {
        gwy_debug("Initializing...");
        process_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              NULL, gwy_process_func_info_free);
    }

    g_return_val_if_fail(func_info->process, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->run & GWY_RUN_MASK, FALSE);
    if (!gwy_strisident(func_info->name, "_-", NULL))
        g_warning("Function name `%s' is not a valid identifier. "
                  "It may be rejected in future.", func_info->name);
    if (g_hash_table_lookup(process_funcs, func_info->name)) {
        g_warning("Duplicate function `%s', keeping only first",
                  func_info->name);
        return FALSE;
    }

    pfinfo = g_memdup(func_info, sizeof(GwyProcessFuncInfo));
    pfinfo->name = g_strdup(func_info->name);
    pfinfo->menu_path = g_strdup(func_info->menu_path);

    g_hash_table_insert(process_funcs, (gpointer)pfinfo->name, pfinfo);
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_PROC, pfinfo->name)) {
        g_hash_table_remove(process_funcs, (gpointer)pfinfo->name);
        return FALSE;
    }

    return TRUE;
}

static void
gwy_process_func_info_free(gpointer data)
{
    GwyProcessFuncInfo *pfinfo = (GwyProcessFuncInfo*)data;

    g_free((gpointer)pfinfo->name);
    g_free((gpointer)pfinfo->menu_path);
    g_free(pfinfo);
}

/**
 * gwy_process_func_run:
 * @name: Data processing function name.
 * @data: Data (a #GwyContainer).
 * @run: How the function should be run.
 *
 * Runs a data processing function identified by @name.
 *
 * It guarantees the container lifetime spans through the actual processing,
 * so the module function doesn't have to care about it.
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
    g_object_ref(data);
    _gwy_module_watch_settings(GWY_MODULE_PREFIX_PROC, name);
    func_info->process(data, run, name);
    _gwy_module_unwatch_settings();
    g_object_unref(data);
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
 * gwy_process_func_get_run_types:
 * @name: Data processing function name.
 *
 * Returns possible run modes for a data processing function identified by
 * @name.
 *
 * This function is the prefered one for testing whether a data processing
 * function exists, as function with no run modes cannot be registered.
 *
 * Returns: The run mode bit mask, zero if the function does not exist.
 **/
GwyRunType
gwy_process_func_get_run_types(const gchar *name)
{
    GwyProcessFuncInfo *func_info;

    if (!process_funcs)
        return 0;

    func_info = g_hash_table_lookup(process_funcs, name);
    if (!func_info)
        return 0;

    return func_info->run;
}

/**
 * gwy_process_func_get_menu_path:
 * @name: Data processing function name.
 *
 * Returns the menu path of a data processing function identified by
 * @name.
 *
 * The returned menu path is only the tail part registered by the function,
 * i.e., without any leading "/Data Process".
 *
 * Returns: The menu path.  The returned string must be treated as constant
 *          and never modified or freed.
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
 * gwy_process_func_get_sensitivity_flags:
 * @name: Data processing function name.
 *
 * Returns menu sensititivy flags for function @name.
 *
 * Returns: The menu item sensitivity flags (a #GwyMenuSensFlags mask).
 **/
guint
gwy_process_func_get_sensitivity_flags(const gchar *name)
{
    GwyProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->sens_flags;
}

/**
 * gwy_process_func_remove:
 * @name: Data processing function name.
 *
 * Removes a data processing function from.
 *
 * Returns: %TRUE if there was such a function and was removed, %FALSE
 *          otherwise.
 **/
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
 * @name: Function name from #GwyProcessFuncInfo (most modules can safely
 *        ignore this argument)
 *
 * The type of data processing function.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
