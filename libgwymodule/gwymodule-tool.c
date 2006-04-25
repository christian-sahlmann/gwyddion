/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
#include <libgwymodule/gwymodule-tool.h>
#include "gwymoduleinternal.h"

/* Auxiliary structure to pass both user callback function and data to
 * g_hash_table_foreach() lambda argument in gwy_tool_func_foreach() */
typedef struct {
    GFunc function;
    gpointer user_data;
} ToolFuncForeachData;

static GHashTable *tool_funcs = NULL;

/**
 * gwy_tool_func_register:
 * @type: Layer type in GObject type system.  That is the return value of
 *        gwy_tool_foo_get_type().
 *
 * Registeres a tool function (tool type).
 *
 * Returns: Normally %TRUE; %FALSE on failure.
 **/
gboolean
gwy_tool_func_register(GType type)
{
    const gchar *name;
    gpointer klass;

    g_return_val_if_fail(type, FALSE);
    klass = g_type_class_ref(type);
    name = g_type_name(type);
    gwy_debug("tool type = %s", name);

    if (!tool_funcs) {
        gwy_debug("Initializing...");
        tool_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            NULL, NULL);
    }

    if (g_hash_table_lookup(tool_funcs, name)) {
        g_warning("Duplicate type %s, keeping only first", name);
        g_type_class_unref(klass);
        return FALSE;
    }
    g_hash_table_insert(tool_funcs, (gpointer)name, GUINT_TO_POINTER(type));
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_TOOL, name)) {
        g_hash_table_remove(tool_funcs, name);
        g_type_class_unref(klass);
        return FALSE;
    }

    return TRUE;
}

static void
gwy_tool_func_user_cb(gpointer key,
                      G_GNUC_UNUSED gpointer value,
                      gpointer user_data)
{
    ToolFuncForeachData *tffd = (ToolFuncForeachData*)user_data;

    tffd->function(key, tffd->user_data);
}

/**
 * gwy_tool_func_foreach:
 * @function: Function to run for each tool function.  It will get function
 *            name (constant string owned by module system) as its first
 *            argument, @user_data as the second argument.
 * @user_data: Data to pass to @function.
 *
 * Calls a function for each tool function.
 **/
void
gwy_tool_func_foreach(GFunc function,
                      gpointer user_data)
{
    ToolFuncForeachData tffd;

    if (!tool_funcs)
        return;

    tffd.user_data = user_data;
    tffd.function = function;
    g_hash_table_foreach(tool_funcs, gwy_tool_func_user_cb, &tffd);
}

gboolean
_gwy_tool_func_remove(const gchar *name)
{
    GType type;

    gwy_debug("%s", name);
    type = GPOINTER_TO_UINT(g_hash_table_lookup(tool_funcs, name));
    if (!type) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }

    g_type_class_unref(g_type_class_peek(type));
    g_hash_table_remove(tool_funcs, name);
    return TRUE;
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
