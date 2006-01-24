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
#include <libgwydgets/gwygraph.h>
#include <libgwymodule/gwymodule-graph.h>
#include "gwymoduleinternal.h"

typedef struct {
    const gchar *name;
    const gchar *menu_path;
    const gchar *stock_id;
    const gchar *tooltip;
    guint sens_mask;
    GwyGraphFunc func;
} GwyGraphFuncInfo;

typedef struct {
    GFunc function;
    gpointer user_data;
} GraphFuncForeachData;

static GHashTable *graph_funcs = NULL;

/**
 * gwy_graph_func_register:
 * @name: Name of function to register.  It should be a valid identifier.
 * @func: The function itself.
 * @menu_path: Menu path under Graph menu.
 * @stock_id: Stock icon id for toolbar.
 * @sens_mask: Sensitivity mask (a combination of #GwyMenuSensFlags
 *             flags).
 * @tooltip: Tooltip for this function.
 *
 * Registers a graph function.
 *
 * Note: the string arguments are not copied as modules are not expected to
 * vanish.  If they are constructed (non-constant) strings, do not free them.
 * Should modules ever become unloadable they will get chance to clean-up.
 *
 * Returns: Normally %TRUE; %FALSE on failure.
 **/
gboolean
gwy_graph_func_register(const gchar *name,
                        GwyGraphFunc func,
                        const gchar *menu_path,
                        const gchar *stock_id,
                        guint sens_mask,
                        const gchar *tooltip)
{
    GwyGraphFuncInfo *gfinfo;

    g_return_val_if_fail(name, FALSE);
    g_return_val_if_fail(func, FALSE);
    g_return_val_if_fail(menu_path, FALSE);
    gwy_debug("name = %s, menu path = %s, func = %p", name, menu_path, func);

    if (!graph_funcs) {
        gwy_debug("initializing...");
        graph_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            NULL, g_free);
    }

    if (!gwy_strisident(name, "_-", NULL))
        g_warning("Function name `%s' is not a valid identifier. "
                  "It may be rejected in future.", name);
    if (g_hash_table_lookup(graph_funcs, name)) {
        g_warning("Duplicate function %s, keeping only first", name);
        return FALSE;
    }

    gfinfo = g_new0(GwyGraphFuncInfo, 1);
    gfinfo->name = name;
    gfinfo->func = func;
    gfinfo->menu_path = menu_path;
    gfinfo->stock_id = stock_id;
    gfinfo->tooltip = tooltip;
    gfinfo->sens_mask = sens_mask;

    g_hash_table_insert(graph_funcs, (gpointer)gfinfo->name, gfinfo);
    if (!_gwy_module_add_registered_function(GWY_MODULE_PREFIX_GRAPH, name)) {
        g_hash_table_remove(graph_funcs, (gpointer)gfinfo->name);
        return FALSE;
    }

    return TRUE;
}

/**
 * gwy_graph_func_run:
 * @name: Graph function name.
 * @graph: Graph (a #GwyGraph).
 *
 * Runs a graph function identified by @name.
 *
 * Returns: %TRUE on success, %FALSE on failure. XXX: whatever it means.
 **/
gboolean
gwy_graph_func_run(const guchar *name,
                   GwyGraph *graph)
{
    GwyGraphFuncInfo *func_info;
    gboolean status;

    func_info = g_hash_table_lookup(graph_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH(graph), FALSE);
    g_object_ref(graph);
    _gwy_module_watch_settings(GWY_MODULE_PREFIX_GRAPH, name);
    status = func_info->func(graph, name);
    _gwy_module_unwatch_settings();
    g_object_unref(graph);

    return status;
}

static void
gwy_graph_func_user_cb(gpointer key,
                       G_GNUC_UNUSED gpointer value,
                       gpointer user_data)
{
    GraphFuncForeachData *gffd = (GraphFuncForeachData*)user_data;

    gffd->function(key, gffd->user_data);
}

/**
 * gwy_graph_func_foreach:
 * @function: Function to run for each graph function.  It will get function
 *            name (constant string owned by module system) as its first
 *            argument, @user_data as the second argument.
 * @user_data: Data to pass to @function.
 *
 * Calls a function for each graph function.
 **/
void
gwy_graph_func_foreach(GFunc function,
                       gpointer user_data)
{
    GraphFuncForeachData gffd;

    if (!graph_funcs)
        return;

    gffd.user_data = user_data;
    gffd.function = function;
    g_hash_table_foreach(graph_funcs, gwy_graph_func_user_cb, &gffd);
}

/**
 * gwy_graph_func_exists:
 * @name: Graph function name.
 *
 * Checks whether a graph function exists.
 *
 * Returns: %TRUE if @name exists, %FALSE otherwise.
 **/
gboolean
gwy_graph_func_exists(const gchar *name)
{
    return graph_funcs && g_hash_table_lookup(graph_funcs, name);
}

/**
 * gwy_graph_func_get_menu_path:
 * @name: Graph function name.
 *
 * Gets menu path of a graph function.
 *
 * The returned menu path is only the tail part registered by the function,
 * i.e., without any leading "/Graph".
 *
 * Returns: The menu path.  The returned string is owned by the module.
 **/
const gchar*
gwy_graph_func_get_menu_path(const gchar *name)
{
    GwyGraphFuncInfo *func_info;

    g_return_val_if_fail(graph_funcs, NULL);
    func_info = g_hash_table_lookup(graph_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->menu_path;
}

/**
 * gwy_graph_func_get_stock_id:
 * @name: Graph function name.
 *
 * Gets stock icon id of a graph function.
 *
 * Returns: The stock icon id.  The returned string is owned by the module.
 **/
const gchar*
gwy_graph_func_get_stock_id(const gchar *name)
{
    GwyGraphFuncInfo *func_info;

    g_return_val_if_fail(graph_funcs, NULL);
    func_info = g_hash_table_lookup(graph_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->stock_id;
}

/**
 * gwy_graph_func_get_tooltip:
 * @name: Graph function name.
 *
 * Gets tooltip for a graph function.
 *
 * Returns: The tooltip.  The returned string is owned by the module.
 **/
const gchar*
gwy_graph_func_get_tooltip(const gchar *name)
{
    GwyGraphFuncInfo *func_info;

    g_return_val_if_fail(graph_funcs, NULL);
    func_info = g_hash_table_lookup(graph_funcs, name);
    g_return_val_if_fail(func_info, NULL);

    return func_info->tooltip;
}

/**
 * gwy_graph_func_get_sensitivity_mask:
 * @name: Graph function name.
 *
 * Gets menu sensititivy mask for a graph function.
 *
 * Returns: The menu item sensitivity mask (a combination of #GwyMenuSensFlags
 *          flags).
 **/
guint
gwy_graph_func_get_sensitivity_mask(const gchar *name)
{
    GwyGraphFuncInfo *func_info;

    func_info = g_hash_table_lookup(graph_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->sens_mask;
}

gboolean
_gwy_graph_func_remove(const gchar *name)
{
    gwy_debug("%s", name);
    if (!g_hash_table_remove(graph_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * SECTION:gwymodule-graph
 * @title: gwymodule-graph
 * @short_description: Graph modules
 *
 * Graph modules implement operations on graphs, e.g., curve fitting.
 **/

/**
 * GwyGraphFuncInfo:
 * @name: An unique data graphing function name.
 * @menu_path: A path under "/Data Graph" where the function should appear.
 *             It must start with "/".
 * @graph: The function itself.
 *
 * Information about one graph function.
 **/

/**
 * GwyGraphFunc:
 * @graph: Graph (a #GwyGraph) to operate on.
 * @name: Function name from #GwyGraphFuncInfo (most modules can safely
 *        ignore this argument)
 *
 * The type of graph function.
 *
 * Returns: Whether it succeeded (XXX: this means exactly what?).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
