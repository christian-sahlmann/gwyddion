/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkmenubar.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwydgets/gwygraph.h>

#include "gwymoduleinternal.h"
#include "gwymodule-graph.h"

static gint graph_menu_entry_compare   (GwyGraphFuncInfo *a,
                                        GwyGraphFuncInfo *b);

static GHashTable *graph_funcs = NULL;

static const gsize bufsize = 1024;

/**
 * gwy_graph_func_register:
 * @modname: Module identifier (name).
 * @func_info: Data graphing function info.
 *
 * Registeres a data graphing function.
 *
 * The passed @func_info must not be an automatic variable.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_graph_func_register(const gchar *modname,
                        GwyGraphFuncInfo *func_info)
{
    _GwyModuleInfoInternal *iinfo;
    gchar *canon_name;

    gwy_debug("%s", __FUNCTION__);
    gwy_debug("name = %s, menu path = %s, func = %p",
              func_info->name, func_info->menu_path, func_info->graph);

    if (!graph_funcs) {
        gwy_debug("%s: Initializing...", __FUNCTION__);
        graph_funcs = g_hash_table_new(g_str_hash, g_str_equal);
    }

    iinfo = gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->graph, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    if (g_hash_table_lookup(graph_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    g_hash_table_insert(graph_funcs, (gpointer)func_info->name, func_info);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_GRAPH, func_info->name, NULL);
    iinfo->funcs = g_slist_append(iinfo->funcs, canon_name);

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
    status = func_info->graph(graph, name);
    g_object_unref(graph);

    return status;
}

/**
 * gwy_build_graph_menu:
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
gwy_build_graph_menu(GtkObject *item_factory,
                     const gchar *prefix,
                     GCallback item_callback)
{
    GtkItemFactoryEntry branch = { NULL, NULL, NULL, 0, "<Branch>", NULL };
    GtkItemFactoryEntry tearoff = { NULL, NULL, NULL, 0, "<Tearoff>", NULL };
    GtkItemFactoryEntry item = { NULL, NULL, item_callback, 0, "<Item>", NULL };
    GtkItemFactory *factory;
    gchar *current, *prev;
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
    current = strncpy(g_new(gchar, bufsize), prefix, bufsize);
    branch.path = current;
    gtk_item_factory_create_item(factory, &branch, NULL, 1);

    /* the root tearoff */
    prev = strncpy(g_new(gchar, bufsize), prefix, bufsize);
    g_strlcpy(prev + dp_len, "/---", bufsize - dp_len);
    tearoff.path = prev;
    gtk_item_factory_create_item(factory, &tearoff, NULL, 1);

    /* create missing branches
     * XXX: Gtk+ essentially can do this itself
     * but this way we can e. g. put a tearoff at the top of each branch... */
    for (l = entries; l; l = g_slist_next(l)) {
        GwyGraphFuncInfo *func_info = (GwyGraphFuncInfo*)l->data;

        if (!func_info->menu_path || !*func_info->menu_path)
            continue;
        if (*func_info->menu_path != '/') {
            g_warning("Menu path `%s' doesn't start with a slash",
                      func_info->menu_path);
            continue;
        }

        if (g_strlcpy(current + dp_len, func_info->menu_path, bufsize - dp_len)
            > bufsize-2)
            g_warning("Too long path `%s' will be truncated",
                      func_info->menu_path);
        /* find where the paths differ */
        for (i = dp_len; current[i] && prev[i] && current[i] == prev[i]; i++)
            ;
        if (!current[i])
            g_warning("Duplicate menu entry `%s'", func_info->menu_path);
        else {
            /* find where the next / is  */
            do {
                i++;
            } while (current[i] && current[i] != '/');
        }

        while (current[i]) {
            /* create a branch with a tearoff */
            current[i] = '\0';
            branch.path = current;
            gtk_item_factory_create_item(factory, &branch, NULL, 1);

            strcpy(prev, current);
            g_strlcat(prev, "/---", bufsize);
            tearoff.path = prev;
            gtk_item_factory_create_item(factory, &tearoff, NULL, 1);
            current[i] = '/';

            /* find where the next / is  */
            do {
                i++;
            } while (current[i] && current[i] != '/');
        }

        /* XXX: passing directly func_info->name may be a little dangerous,
         * OTOH who would eventually free a newly allocated string? */
        item.path = current;
        gtk_item_factory_create_item(factory, &item,
                                     (gpointer)func_info->name, 1);

        GWY_SWAP(gchar*, current, prev);
    }

    g_free(prev);
    g_free(current);
    g_slist_free(entries);

    return item_factory;
}

static gint
graph_menu_entry_compare(GwyGraphFuncInfo *a,
                           GwyGraphFuncInfo *b)
{
    gchar p[bufsize], q[bufsize];
    gsize i, j;

    g_assert(a->menu_path && b->menu_path);
    for (i = j = 0; a->menu_path[i] && j < bufsize-1; i++) {
        if (a->menu_path[i] != '_')
            p[j++] = a->menu_path[i];
    }
    p[j] = '\0';
    for (i = j = 0; b->menu_path[i] && j < bufsize-1; i++) {
        if (b->menu_path[i] != '_')
            q[j++] = b->menu_path[i];
    }
    q[j] = '\0';
    return strcmp(p, q);
}

gboolean
gwy_graph_func_remove(const gchar *name)
{
    if (!g_hash_table_remove(graph_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/************************** Documentation ****************************/

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
