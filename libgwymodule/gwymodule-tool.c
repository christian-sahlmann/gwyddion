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
#include <gtk/gtktoolbar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>

#include "gwymoduleinternal.h"
#include "gwymodule-tool.h"

static GtkWidget* tool_toolbar_append      (GtkWidget *toolbar,
                                            GtkWidget *radio,
                                            GwyToolFuncInfo *func_info,
                                            GtkSignalFunc callback);
static gint tool_toolbar_item_compare      (GwyToolFuncInfo *a,
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
 * gwy_tool_func_build_toolbar:
 * @item_callback: A callback called when a tool from the toolbar is selected
 *                 with tool name as the user data.
 * @first_tool: Where name of the first tool in the toolbar should be stored.
 *
 * Creates a toolbar with the tools.
 *
 * Returns: The toolbar as a #GtkWidget.
 **/
/* XXX: This is broken, because the toolbar may have more than one row.
 * But... */
GtkWidget*
gwy_tool_func_build_toolbar(GtkSignalFunc item_callback,
                            const gchar **first_tool)
{
    GtkWidget *toolbar, *group;
    GSList *l, *entries = NULL;

    if (!tool_funcs) {
        g_warning("No tool function present to build menu of");
        entries = NULL;
    }
    else
       g_hash_table_foreach(tool_funcs, gwy_hash_table_to_slist_cb,
                            &entries);
    entries = g_slist_sort(entries, (GCompareFunc)tool_toolbar_item_compare);

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar),
                                GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar),
                              GTK_ICON_SIZE_BUTTON);
    if (!entries)
        return toolbar;

    group = tool_toolbar_append(toolbar, NULL,
                                (GwyToolFuncInfo*)entries->data, item_callback);
    *first_tool = ((GwyToolFuncInfo*)entries->data)->name;
    for (l = entries->next; l; l = g_slist_next(l))
        tool_toolbar_append(toolbar, group,
                            (GwyToolFuncInfo*)l->data, item_callback);
    g_slist_free(entries);

    return toolbar;
}

static GtkWidget*
tool_toolbar_append(GtkWidget *toolbar,
                    GtkWidget *radio,
                    GwyToolFuncInfo *func_info,
                    GtkSignalFunc callback)
{
    GtkWidget *icon;
    const gchar *name, *stock_id, *label, *tooltip;

    /* ,none` tool now unused */
    if (!func_info) {
        name = NULL;
        stock_id = "gwy_none";
        label = stock_id;
        tooltip = _("No tool");
    }
    else {
        name = func_info->name;
        stock_id = func_info->stock_id;
        label = stock_id;
        tooltip = func_info->tooltip;
    }
    icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
    return gtk_toolbar_append_element(GTK_TOOLBAR(toolbar),
                                      GTK_TOOLBAR_CHILD_RADIOBUTTON, radio,
                                      label, tooltip, NULL, icon,
                                      callback, (gpointer)name);
}

static gint
tool_toolbar_item_compare(GwyToolFuncInfo *a,
                          GwyToolFuncInfo *b)
{
    if (a->toolbar_position < b->toolbar_position)
        return -1;
    else if (a->toolbar_position > b->toolbar_position)
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
 * @toolbar_position: Position in the toolbar, the tools are sorted by this
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
