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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwytoolbox.h>

#include "gwymoduleinternal.h"
#include "gwymodule-tool.h"

static gint tool_toolbox_item_compare      (GwyToolFuncInfo *a,
                                            GwyToolFuncInfo *b);

static GHashTable *tool_funcs = NULL;

enum { bufsize = 1024 };

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
    gchar *canon_name;

    gwy_debug("");
    gwy_debug("name = %s, stock id = %s, func = %p",
              func_info->name, func_info->stock_id, func_info->use);

    if (!tool_funcs) {
        gwy_debug("Initializing...");
        tool_funcs = g_hash_table_new(g_str_hash, g_str_equal);
    }

    iinfo = gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->use, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->stock_id, FALSE);
    g_return_val_if_fail(func_info->tooltip, FALSE);
    if (g_hash_table_lookup(tool_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    g_hash_table_insert(tool_funcs, (gpointer)func_info->name, func_info);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_TOOL, func_info->name, NULL);
    iinfo->funcs = g_slist_append(iinfo->funcs, canon_name);

    return TRUE;
}

/**
 * gwy_tool_func_use:
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
 * gwy_tool_func_build_toolbox:
 * @item_callback: A callback called when a tool from the toolbox is selected
 *                 with tool name as the user data.
 * @max_width: The number of columns.
 * @first_tool: Where name of the first tool in the toolbox should be stored.
 *
 * Creates a toolbox with the tools.
 *
 * Returns: The toolbox as a #GtkWidget.
 **/
GtkWidget*
gwy_tool_func_build_toolbox(GtkSignalFunc item_callback,
                            gint max_width,
                            const gchar **first_tool)
{
    GtkWidget *toolbox, *widget, *group = NULL;
    GSList *l, *entries = NULL;

    if (!tool_funcs) {
        g_warning("No tool function present to build menu of");
        entries = NULL;
    }
    else
       g_hash_table_foreach(tool_funcs, gwy_hash_table_to_slist_cb,
                            &entries);
    entries = g_slist_sort(entries, (GCompareFunc)tool_toolbox_item_compare);

    *first_tool = NULL;
    toolbox = gwy_toolbox_new(max_width);
    if (!entries)
        return toolbox;

    for (l = entries->next; l; l = g_slist_next(l)) {
        GwyToolFuncInfo *func_info = (GwyToolFuncInfo*)l->data;

        widget = gwy_toolbox_append(GWY_TOOLBOX(toolbox),
                                    GTK_TYPE_RADIO_BUTTON, group,
                                    func_info->tooltip, NULL,
                                    func_info->stock_id,
                                    item_callback, (gpointer)func_info->name);
        if (!group) {
            group = widget;
            *first_tool = func_info->name;
        }
    }

    g_slist_free(entries);

    return toolbox;
}

static gint
tool_toolbox_item_compare(GwyToolFuncInfo *a,
                          GwyToolFuncInfo *b)
{
    if (a->toolbox_position < b->toolbox_position)
        return -1;
    else if (a->toolbox_position > b->toolbox_position)
        return 1;

    return strcmp(a->name, b->name);
}

gboolean
gwy_tool_func_remove(const gchar *name)
{
    if (!g_hash_table_remove(tool_funcs, name)) {
        g_warning("Cannot remove function %s", name);
        return FALSE;
    }
    return TRUE;
}

/************************** Documentation ****************************/

/**
 * GwyToolFuncInfo:
 * @name: An unique data processing function name.
 * @stock_id: Icon stock id or button label (FIXME: more to be said).
 * @tooltip: Tooltip for this tool.
 * @toolbox_position: Position in the toolbox, the tools are sorted by this
 *                    value (and then alphabetically if they are equal).
 *                    Standard tools are in the range [0,100].
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
