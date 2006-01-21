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
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkmenubar.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwydgets/gwygraph.h>

#include "gwymoduleinternal.h"
#include "gwymodule-graph.h"

typedef struct {
    GwyGraphFuncInfo info;
    const gchar *menu_path_translated;
    gchar *menu_path_factory;
} GraphFuncInfo;

typedef struct {
    GFunc function;
    gpointer user_data;
} GraphFuncForeachData;

static void gwy_graph_func_info_free   (gpointer data);
static gint graph_menu_entry_compare   (GraphFuncInfo *a,
                                        GraphFuncInfo *b);

static GHashTable *graph_funcs = NULL;
static void (*func_register_callback)(const gchar *fullname) = NULL;

/**
 * gwy_graph_func_register:
 * @modname: Module identifier (name).
 * @func_info: Data graphing function info.
 *
 * Registeres a data graphing function.
 *
 * To keep compatibility with old versions @func_info should not be an
 * automatic variable.  However, since 1.6 it keeps a copy of @func_info.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_graph_func_register(const gchar *modname,
                        GwyGraphFuncInfo *func_info)
{
    _GwyModuleInfoInternal *iinfo;
    GraphFuncInfo *gfinfo;
    gchar *canon_name;

    gwy_debug("");
    gwy_debug("name = %s, menu path = %s, func = %p",
              func_info->name, func_info->menu_path, func_info->graph);

    if (!graph_funcs) {
        gwy_debug("Initializing...");
        graph_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                            NULL, gwy_graph_func_info_free);
    }

    iinfo = _gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->graph, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    if (g_hash_table_lookup(graph_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }

    gfinfo = g_new0(GraphFuncInfo, 1);
    gfinfo->info = *func_info;
    gfinfo->info.name = g_strdup(func_info->name);
    gfinfo->info.menu_path = g_strdup(func_info->menu_path);
    gfinfo->menu_path_translated = _(func_info->menu_path);
    gfinfo->menu_path_factory
        = gwy_strkill(g_strdup(gfinfo->menu_path_translated), "_");

    g_hash_table_insert(graph_funcs, (gpointer)gfinfo->info.name, gfinfo);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_GRAPH, gfinfo->info.name, NULL);
    iinfo->funcs = g_slist_append(iinfo->funcs, canon_name);
    if (func_register_callback)
        func_register_callback(canon_name);

    return TRUE;
}

void
_gwy_graph_func_set_register_callback(void (*callback)(const gchar *fullname))
{
    func_register_callback = callback;
}

static void
gwy_graph_func_info_free(gpointer data)
{
    GraphFuncInfo *gfinfo = (GraphFuncInfo*)data;

    g_free((gpointer)gfinfo->info.name);
    g_free((gpointer)gfinfo->info.menu_path);
    g_free((gpointer)gfinfo->menu_path_factory);
    g_free(gfinfo);
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
    GraphFuncInfo *func_info;
    gboolean status;

    func_info = g_hash_table_lookup(graph_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(GWY_IS_GRAPH(graph), FALSE);
    g_object_ref(graph);
    _gwy_module_watch_settings(GWY_MODULE_PREFIX_GRAPH, name);
    status = func_info->info.graph(graph, name);
    _gwy_module_unwatch_settings();
    g_object_unref(graph);

    return status;
}

/**
 * gwy_graph_func_build_menu:
 * @item_factory: A #GtkItemFactory to add items to.
 * @prefix: Where to add the menu items to the factory.
 * @item_callback: A #GtkItemFactoryCallback1 called when an item from the
 *                 menu is selected.
 *
 * Creates #GtkItemFactory for a graph menu with all registered graph
 * functions.
 *
 * Returns: The menu item factory as a #GtkObject.
 **/
GtkObject*
gwy_graph_func_build_menu(GtkObject *item_factory,
                          const gchar *prefix,
                          GCallback item_callback)
{
    GtkItemFactoryEntry branch = { NULL, NULL, NULL, 0, "<Branch>", NULL };
    GtkItemFactoryEntry tearoff = { NULL, NULL, NULL, 0, "<Tearoff>", NULL };
    GtkItemFactoryEntry item = { NULL, NULL, item_callback, 0, "<Item>", NULL };
    GtkItemFactory *factory;
    GString *current, *prev;
    const gchar *mpath;
    GSList *l, *entries = NULL;
    gint i, dp_len;

    g_return_val_if_fail(GTK_IS_ITEM_FACTORY(item_factory), NULL);
    factory = GTK_ITEM_FACTORY(item_factory);

    if (!graph_funcs) {
        g_warning("No graph function present to build menu of");
        entries = NULL;
    }
    else
        g_hash_table_foreach(graph_funcs, gwy_hash_table_to_slist_cb,
                             &entries);
    entries = g_slist_sort(entries, (GCompareFunc)graph_menu_entry_compare);

    dp_len = strlen(prefix);

    /* the root branch */
    current = g_string_new(prefix);

    /* the root tearoff */
    prev = g_string_new(prefix);
    g_string_append(prev, "/---");
    tearoff.path = prev->str;
    gtk_item_factory_create_item(factory, &tearoff, NULL, 1);

    /* create missing branches
     * XXX: Gtk+ essentially can do this itself
     * but this way we can e. g. put a tearoff at the top of each branch... */
    for (l = entries; l; l = g_slist_next(l)) {
        GraphFuncInfo *func_info = (GraphFuncInfo*)l->data;

        mpath = func_info->menu_path_translated;
        if (!mpath || !*mpath)
            continue;
        if (mpath[0] != '/') {
            g_warning("Menu path `%s' doesn't start with a slash", mpath);
            continue;
        }

        g_string_truncate(current, dp_len);
        g_string_append(current, mpath);
        /* find where the paths differ */
        i = gwy_strdiffpos(current->str + dp_len, prev->str + dp_len);
        if (!current->str[i] && !prev->str[i])
            g_warning("Duplicate menu entry `%s'", mpath);
        else {
            /* find where the next / is  */
            do {
                i++;
            } while (current->str[i] && current->str[i] != '/');
        }

        while (current->str[i]) {
            /* create a branch with a tearoff */
            current->str[i] = '\0';
            branch.path = current->str;
            gtk_item_factory_create_item(factory, &branch, NULL, 1);

            g_string_assign(prev, current->str);
            g_string_append(prev, "/---");
            tearoff.path = prev->str;
            gtk_item_factory_create_item(factory, &tearoff, NULL, 1);
            current->str[i] = '/';

            /* find where the next / is  */
            do {
                i++;
            } while (current->str[i] && current->str[i] != '/');
        }

        /* XXX: passing directly func_info->name may be a little dangerous,
         * OTOH who would eventually free a newly allocated string? */
        item.path = current->str;
        gtk_item_factory_create_item(factory, &item,
                                     (gpointer)func_info->info.name, 1);

        GWY_SWAP(GString*, current, prev);
    }

    g_string_free(prev, TRUE);
    g_string_free(current, TRUE);
    g_slist_free(entries);

    return item_factory;
}

static gint
graph_menu_entry_compare(GraphFuncInfo *a,
                         GraphFuncInfo *b)
{
    return g_utf8_collate(a->menu_path_factory, b->menu_path_factory);
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
 * Returns whether graph function @name exists.
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
 * Returns the menu path of a data graph identified by @name.
 *
 * The returned menu path is only the tail part registered by the function,
 * i.e., without any leading "/Graph".
 *
 * Returns: The menu path.  The returned string must be treated as constant
 *          and never modified or freed.
 **/
const gchar*
gwy_graph_func_get_menu_path(const gchar *name)
{
    GraphFuncInfo *func_info;

    func_info = g_hash_table_lookup(graph_funcs, name);
    g_return_val_if_fail(func_info, 0);
    return func_info->menu_path_translated;
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
