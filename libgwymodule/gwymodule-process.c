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

#include <string.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkmenubar.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwycontainer.h>
#include <libprocess/datafield.h>

#include "gwymoduleinternal.h"
#include "gwymodule-process.h"

/* FIXME:
 * To avoid circular dependency on libgwyapp, we cheat and set object
 * data for sentitivity manually.  Maybe the menu-building doesn't belong
 * here at all.
 **/

typedef struct {
    GwyProcessFuncInfo info;
    const gchar *menu_path_translated;
    gchar *menu_path_factory;
} ProcessFuncInfo;

static void gwy_process_func_info_free (gpointer data);
static gint process_menu_entry_compare (ProcessFuncInfo *a,
                                        ProcessFuncInfo *b);

static GHashTable *process_funcs = NULL;
static void (*func_register_callback)(const gchar *fullname) = NULL;

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
    _GwyModuleInfoInternal *iinfo;
    ProcessFuncInfo *pfinfo;
    gchar *canon_name;

    gwy_debug("");
    gwy_debug("name = %s, menu path = %s, run = %d, func = %p",
              func_info->name, func_info->menu_path, func_info->run,
              func_info->process);

    if (!process_funcs) {
        gwy_debug("Initializing...");
        process_funcs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              NULL, gwy_process_func_info_free);
    }

    iinfo = _gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->process, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->run & GWY_RUN_MASK, FALSE);
    if (g_hash_table_lookup(process_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }

    pfinfo = g_new0(ProcessFuncInfo, 1);
    pfinfo->info = *func_info;
    pfinfo->info.name = g_strdup(func_info->name);
    pfinfo->info.menu_path = g_strdup(func_info->menu_path);
    pfinfo->menu_path_translated = _(func_info->menu_path);
    pfinfo->menu_path_factory
        = gwy_strkill(g_strdup(pfinfo->menu_path_translated), "_");

    g_hash_table_insert(process_funcs, (gpointer)pfinfo->info.name, pfinfo);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_PROC, pfinfo->info.name, NULL);
    iinfo->funcs = g_slist_append(iinfo->funcs, canon_name);
    if (func_register_callback)
        func_register_callback(canon_name);

    return TRUE;
}

void
_gwy_process_func_set_register_callback(void (*callback)(const gchar *fullname))
{
    func_register_callback = callback;
}

static void
gwy_process_func_info_free(gpointer data)
{
    ProcessFuncInfo *pfinfo = (ProcessFuncInfo*)data;

    g_free((gpointer)pfinfo->info.name);
    g_free((gpointer)pfinfo->info.menu_path);
    g_free((gpointer)pfinfo->menu_path_factory);
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
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_process_func_run(const guchar *name,
                     GwyContainer *data,
                     GwyRunType run)
{
    ProcessFuncInfo *func_info;
    GwyDataField *dfield;
    gboolean status;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(run & func_info->info.run, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    /* TODO: Container */
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);
    g_object_ref(data);
    g_object_ref(dfield);
    status = func_info->info.process(data, run, name);
    g_object_unref(dfield);
    g_object_unref(data);

    return status;
}

/**
 * gwy_process_func_build_menu:
 * @item_factory: A #GtkItemFactory to add items to.
 * @prefix: Where to add the menu items to the factory.
 * @item_callback: A #GtkItemFactoryCallback1 called when an item from the
 *                 menu is selected.
 *
 * Creates #GtkItemFactory for a data processing menu with all registered data
 * processing functions.
 *
 * Returns: The menu item factory as a #GtkObject.
 **/
GtkObject*
gwy_process_func_build_menu(GtkObject *item_factory,
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

    if (!process_funcs) {
        g_warning("No process function present to build menu of");
        entries = NULL;
    }
    else
        g_hash_table_foreach(process_funcs, gwy_hash_table_to_slist_cb,
                             &entries);
    entries = g_slist_sort(entries, (GCompareFunc)process_menu_entry_compare);

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
        ProcessFuncInfo *func_info = (ProcessFuncInfo*)l->data;

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
        if (func_info->info.sens_flags) {
            GtkWidget *widget;

            gwy_debug("Setting sens flags for `%s' to %u",
                      item.path, func_info->info.sens_flags);
            widget = gtk_item_factory_get_widget(factory,
                                                 func_info->menu_path_factory);
            g_object_set_data(G_OBJECT(widget), "sensitive",
                              GUINT_TO_POINTER(func_info->info.sens_flags));
        }

        GWY_SWAP(GString*, current, prev);
    }

    g_string_free(prev, TRUE);
    g_string_free(current, TRUE);
    g_slist_free(entries);

    return item_factory;
}

static gint
process_menu_entry_compare(ProcessFuncInfo *a,
                           ProcessFuncInfo *b)
{
    return g_utf8_collate(a->menu_path_factory, b->menu_path_factory);

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
    ProcessFuncInfo *func_info;

    if (!process_funcs)
        return 0;

    func_info = g_hash_table_lookup(process_funcs, name);
    if (!func_info)
        return 0;

    return func_info->info.run;
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
G_CONST_RETURN gchar*
gwy_process_func_get_menu_path(const gchar *name)
{
    ProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->menu_path_translated;
}

/**
 * gwy_process_func_get_sensitivity_flags:
 * @name: Data processing function name.
 *
 * Returns menu sensititivy flags for function @name.
 *
 * Returns: The menu item sensitivity flags, as it was set with
 *          gwy_process_func_set_sensitivity_flags(), i.e., without any
 *          implied flags.
 **/
guint
gwy_process_func_get_sensitivity_flags(const gchar *name)
{
    ProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, 0);

    return func_info->info.sens_flags;
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
 *
 * Returns: Whether it changed @data. (Incidentally, creation of a new data
 *          window without touching @data does not change @data.)
 **/

/**
 * GwyRunType:
 * @GWY_RUN_NONE: None.
 * @GWY_RUN_WITH_DEFAULTS: The function is run non-interactively, and it
 *                         should use default parameter values.
 * @GWY_RUN_NONINTERACTIVE: The function is run non-interactively, and it
 *                          should use parameter values stored in the
 *                          container to reproduce previous runs.
 * @GWY_RUN_MODAL: The function presents a [presumably simple] modal GUI to
 *                 the user, it returns after finishing all operations.
 * @GWY_RUN_MASK: The mask for all the run modes.
 *
 * Data processing function run-modes.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
