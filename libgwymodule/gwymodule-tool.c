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

#include "gwymoduleinternal.h"
#include "gwymodule-tool.h"

static void gwy_tool_func_info_free        (gpointer data);

static GHashTable *tool_funcs = NULL;
static void (*func_register_callback)(const gchar *fullname) = NULL;

enum { bufsize = 1024 };

/**
 * gwy_tool_func_register:
 * @modname: Module identifier (name).
 * @func_info: Tool use function info.
 *
 * Registeres a tool use function.
 *
 * To keep compatibility with old versions @func_info should not be an
 * automatic variable.  However, since 1.6 it keeps a copy of @func_info.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_tool_func_register(const gchar *modname,
                       GwyToolFuncInfo *func_info)
{
    _GwyModuleInfoInternal *iinfo;
    GwyToolFuncInfo *tfinfo;
    gchar *canon_name;

    gwy_debug("");
    gwy_debug("name = %s, stock id = %s, func = %p",
              func_info->name, func_info->stock_id, func_info->use);

    if (!tool_funcs) {
        gwy_debug("Initializing...");
        tool_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           NULL, gwy_tool_func_info_free);
    }

    iinfo = _gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->use, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->stock_id, FALSE);
    g_return_val_if_fail(func_info->tooltip, FALSE);
    if (g_hash_table_lookup(tool_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }

    tfinfo = g_memdup(func_info, sizeof(GwyToolFuncInfo));
    tfinfo->name = g_strdup(func_info->name);
    tfinfo->stock_id = g_strdup(func_info->stock_id);
    /* FIXME: This is not very clean. But we need the translated string often,
     * namely in menu building code. */
    tfinfo->tooltip = g_strdup(_(func_info->tooltip));

    g_hash_table_insert(tool_funcs, (gpointer)tfinfo->name, tfinfo);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_TOOL, tfinfo->name, NULL);
    iinfo->funcs = g_slist_append(iinfo->funcs, canon_name);
    if (func_register_callback)
        func_register_callback(canon_name);

    return TRUE;
}

void
_gwy_tool_func_set_register_callback(void (*callback)(const gchar *fullname))
{
    func_register_callback = callback;
}

static void
gwy_tool_func_info_free(gpointer data)
{
    GwyToolFuncInfo *tfinfo = (GwyToolFuncInfo*)data;

    g_free((gpointer)tfinfo->name);
    g_free((gpointer)tfinfo->stock_id);
    g_free((gpointer)tfinfo->tooltip);
    g_free(tfinfo);
}

/**
 * gwy_tool_func_use:
 * @name: Tool use function name.
 * @data_window: A data window the tool should be set for.
 * @event: The tool change event.
 *
 * Sets a tool for a data window.
 *
 * Returns: Whether the tool switch succeeded.  Under normal circumstances
 *          it always return %TRUE.
 **/
gboolean
gwy_tool_func_use(const guchar *name,
                  GwyDataWindow *data_window,
                  GwyToolSwitchEvent event)
{
    GwyToolFuncInfo *func_info;

    func_info = g_hash_table_lookup(tool_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(func_info->use, FALSE);
    g_return_val_if_fail(!data_window || GWY_IS_DATA_WINDOW(data_window),
                         FALSE);

    gwy_debug("toolname = <%s>, data_window = %p, event = %d",
              name, data_window, event);
    return func_info->use(data_window, event);
}

gboolean
_gwy_tool_func_remove(const gchar *name)
{
    gwy_debug("%s", name);
    if (!g_hash_table_remove(tool_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/**
 * gwy_tool_func_exists:
 * @name: Tool function name.
 *
 * Returns whether tool function @name exists.
 *
 * Returns: %TRUE if @name exists, %FALSE otherwise.
 **/
gboolean
gwy_tool_func_exists(const gchar *name)
{
    return tool_funcs && g_hash_table_lookup(tool_funcs, name);
}

/**
 * gwy_tool_func_get_tooltip:
 * @name: Tool function name.
 *
 * Gets tool function tooltip.
 *
 * Returns: The tooltip as a string owned by module loader.
 **/
const gchar*
gwy_tool_func_get_tooltip(const gchar *name)
{
    GwyToolFuncInfo *func_info;

    func_info = g_hash_table_lookup(tool_funcs, name);
    if (!func_info)
        return NULL;

    return func_info->tooltip;
}

/**
 * gwy_tool_func_get_stock_id:
 * @name: Tool function name.
 *
 * Gets tool function stock icon id.
 *
 * Returns: The stock icon id as a string owned by module loader.
 **/
const gchar*
gwy_tool_func_get_stock_id(const gchar *name)
{
    GwyToolFuncInfo *func_info;

    func_info = g_hash_table_lookup(tool_funcs, name);
    if (!func_info)
        return NULL;

    return func_info->stock_id;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodule-tool
 * @title: gwymodule-tool
 * @short_description: Interactive tool modules
 *
 * Tool modules implement interactive tools that work directly on data
 * windows.
 **/

/**
 * GwyToolFuncInfo:
 * @name: An unique tool function name.
 * @stock_id: Icon stock id or button label (FIXME: more to be said).
 * @tooltip: Tooltip for this tool.
 * @use: The tool use function itself.
 *
 * Information about one tool use function.
 **/

/**
 * GwyToolUseFunc:
 * @data_window: A data window the tool should be set for.
 * @event: The tool change event.
 *
 * The type of tool use function.
 *
 * This function is called to set a tool for a data window, either when
 * the user changes the active tool or switches to another window; the
 * detailed event is given in event.
 *
 * Returns: Whether the tool switch succeeded.  Under normal circumstances
 *          it should always return %TRUE.
 **/

/**
 * GwyToolSwitchEvent:
 * @GWY_TOOL_SWITCH_WINDOW: The tool should be set for the data window
 *                          because the user switched windows.
 * @GWY_TOOL_SWITCH_TOOL: The tool should be set for the data window
 *                        because the user switched tools.
 *
 * Tool switch events.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
