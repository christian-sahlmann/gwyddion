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
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>

#include "gwymoduleinternal.h"
#include "gwymodule-tool.h"

static void gwy_hash_table_to_slist_cb (gpointer key,
                                        gpointer value,
                                        gpointer user_data);
static gint tool_toolbar_item_compare  (GwyToolFuncInfo *a,
                                        GwyToolFuncInfo *b);

static GHashTable *tool_funcs = NULL;

static const gsize bufsize = 1024;

/**
 * gwy_tool_func_register:
 * @modname: Module identifier (name).
 * @func_info: Tool use function info.
 *
 * Registeres a tool use function.
 *
 * The passed @func_info must not be an automatic variable.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_tool_func_register(const gchar *modname,
                       GwyToolFuncInfo *func_info)
{
    _GwyModuleInfoInternal *iinfo;

    gwy_debug("%s", __FUNCTION__);
    gwy_debug("name = %s, stock id = %s, func = %p",
              func_info->name, func_info->stock_id, func_info->use);

    if (!tool_funcs) {
        gwy_debug("%s: Initializing...", __FUNCTION__);
        tool_funcs = g_hash_table_new(g_str_hash, g_str_equal);
    }

    iinfo = gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->use, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->stock_id, FALSE);
    if (g_hash_table_lookup(tool_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    g_hash_table_insert(tool_funcs, (gpointer)func_info->name, func_info);
    iinfo->funcs = g_slist_append(iinfo->funcs, (gpointer)func_info->name);

    return TRUE;
}

/**
 * gwy_tool_func_run:
 * @name: Tool use function name.
 * @data_window: A data window the tool should be set for.
 * @event: The tool change event.
 *
 * Sets a tool for a data window.
 **/
void
gwy_tool_func_use(const guchar *name,
                  GwyDataWindow *data_window,
                  GwyToolSwitchEvent event)
{
    GwyToolFuncInfo *func_info;

    func_info = g_hash_table_lookup(tool_funcs, name);
    g_return_if_fail(func_info);
    g_return_if_fail(func_info->use);
    g_return_if_fail(!data_window || GWY_IS_DATA_WINDOW(data_window));
    func_info->use(data_window, event);
}

/**
 * gwy_build_tool_menu:
 * @item_callback: A #GtkItemFactoryCallback1 called when a tool from the
 *                 toolbar is selected.
 *
 * Creates a toolbar with the tools.
 *
 * Returns: The toolbar as a #GtkWidget.
 **/
/* XXX: This is broken, because the toolbar may have more than one row.
 * But... */
GtkWidget*
gwy_build_tool_toolbar(GCallback item_callback)
{
    GSList *l, *entries = NULL;
    gint i, dp_len;

    g_hash_table_foreach(tool_funcs, gwy_hash_table_to_slist_cb, &entries);
    entries = g_slist_sort(entries, (GCompareFunc)tool_toolbar_item_compare);

#if 0
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
        GwyToolFuncInfo *func_info = (GwyToolFuncInfo*)l->data;

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
        gtk_item_factory_create_item(factory, &item, func_info->name, 1);

        GWY_SWAP(gchar*, current, prev);
    }

    g_free(prev);
    g_free(current);
    g_slist_free(entries);

#endif
    return NULL;
}

static gint
tool_toolbar_item_compare(GwyToolFuncInfo *a,
                          GwyToolFuncInfo *b)
{
    return strcmp(a->name, b->name);
}

static void
gwy_hash_table_to_slist_cb(gpointer key,
                           gpointer value,
                           gpointer user_data)
{
    GSList **list = (GSList**)user_data;

    *list = g_slist_prepend(*list, value);
}

gboolean
gwy_tool_func_try_remove(const gchar *name)
{
    return g_hash_table_remove(tool_funcs, name);
}

/************************** Documentation ****************************/

/**
 * GwyToolFuncInfo:
 * @name: An unique data processing function name.
 * @stock_id: Icon stock id or button label (FIXME: more to be said).
 * @use: The tool use function itself.
 *
 * Information about one tool use function.
 **/

/**
 * GwyToolFunc:
 * @data_window: A data window the tool should be set for.
 * @event: The tool change event.
 *
 * The type of tool use function.
 *
 * This function is called to set a tool for a data window, either when
 * the user changes the active tool or switches to another window; the
 * detailed event is given in event.
 **/

/**
 * GwyRunType:
 * @GWY_TOOL_SWITCH_WINDOW: The tool should be set for the data window
 *                          because the user switched windows.
 * @GWY_TOOL_SWITCH_TOOL: The tool should be set for the data window
 *                        because the user switched tools.
 *
 * Tool switch events.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
