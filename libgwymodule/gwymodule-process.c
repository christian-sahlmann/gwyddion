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
 * 1. This should be in GwyProcessFuncInfo, but adding it there would break
 *    binary compatibility, so it has to wait till 2.0, which will use
 *    GtkActions (Gtk+-2.4) anyway
 * 2. To avoid circular dependency on libgwyapp, we cheat and set object
 *    data for sentitivity manually.  Maybe the menu-building doesn't belong
 *    here at all.
 **/
typedef struct {
    GwyProcessFuncInfo *func_info;
    gint sens_flags;
} GwyProcessFuncInfoInternal;

static gint process_menu_entry_compare (GwyProcessFuncInfoInternal *ipfa,
                                        GwyProcessFuncInfoInternal *ipfb);

static GHashTable *process_funcs = NULL;
static void (*func_register_callback)(const gchar *fullname) = NULL;

enum { bufsize = 1024 };

/**
 * gwy_process_func_register:
 * @modname: Module identifier (name).
 * @func_info: Data processing function info.
 *
 * Registeres a data processing function.
 *
 * The passed @func_info must not be an automatic variable.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_process_func_register(const gchar *modname,
                          GwyProcessFuncInfo *func_info)
{
    GwyProcessFuncInfoInternal *ipfinfo;
    _GwyModuleInfoInternal *iinfo;
    gchar *canon_name;

    gwy_debug("");
    gwy_debug("name = %s, menu path = %s, run = %d, func = %p",
              func_info->name, func_info->menu_path, func_info->run,
              func_info->process);

    if (!process_funcs) {
        gwy_debug("Initializing...");
        process_funcs = g_hash_table_new(g_str_hash, g_str_equal);
    }

    iinfo = gwy_module_get_module_info(modname);
    g_return_val_if_fail(iinfo, FALSE);
    g_return_val_if_fail(func_info->process, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->run & GWY_RUN_MASK, FALSE);
    if (g_hash_table_lookup(process_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    ipfinfo = g_new0(GwyProcessFuncInfoInternal, 1);
    ipfinfo->func_info = func_info;
    g_hash_table_insert(process_funcs, (gpointer)func_info->name, ipfinfo);
    canon_name = g_strconcat(GWY_MODULE_PREFIX_PROC, func_info->name, NULL);
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
    GwyProcessFuncInfoInternal *ipfinfo;
    GwyDataField *dfield;
    gboolean status;

    ipfinfo = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(ipfinfo, FALSE);
    g_assert(ipfinfo->func_info);
    g_return_val_if_fail(run & ipfinfo->func_info->run, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    /* TODO: Container */
    dfield = (GwyDataField*)gwy_container_get_object_by_name(data, "/0/data");
    g_return_val_if_fail(GWY_IS_DATA_FIELD(dfield), FALSE);
    g_object_ref(data);
    g_object_ref(dfield);
    status = ipfinfo->func_info->process(data, run, name);
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
    gchar *current, *prev, *s;
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
        GwyProcessFuncInfoInternal *ipfinfo;
        GwyProcessFuncInfo *func_info;

        ipfinfo = (GwyProcessFuncInfoInternal*)l->data;
        func_info = ipfinfo->func_info;

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
        if (ipfinfo->sens_flags) {
            GtkWidget *widget;

            gwy_debug("Setting sens flags for `%s' to %u",
                      item.path, ipfinfo->sens_flags);
            s = gwy_strkill(g_strdup(item.path), "_");
            widget = gtk_item_factory_get_widget(factory, s);
            g_free(s);
            g_object_set_data(G_OBJECT(widget), "sensitive",
                              GUINT_TO_POINTER(ipfinfo->sens_flags));
        }

        GWY_SWAP(gchar*, current, prev);
    }

    g_free(prev);
    g_free(current);
    g_slist_free(entries);

    return item_factory;
}

static gint
process_menu_entry_compare(GwyProcessFuncInfoInternal *ipfa,
                           GwyProcessFuncInfoInternal *ipfb)
{
    GwyProcessFuncInfo *a, *b;
    gchar p[bufsize], q[bufsize];
    gsize i, j;

    a = ipfa->func_info;
    b = ipfb->func_info;
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

/**
 * gwy_process_func_get_run_types:
 * @name: Data processing function name.
 *
 * Returns possible run modes for a data processing function identified by
 * @name.
 *
 * Returns: The run mode bit mask.
 **/
GwyRunType
gwy_process_func_get_run_types(const gchar *name)
{
    GwyProcessFuncInfoInternal *ipfinfo;

    ipfinfo = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(ipfinfo, 0);
    g_assert(ipfinfo->func_info);

    return ipfinfo->func_info->run;
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
    GwyProcessFuncInfoInternal *ipfinfo;

    ipfinfo = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(ipfinfo, 0);
    g_assert(ipfinfo->func_info);

    return ipfinfo->func_info->menu_path;
}

/**
 * gwy_process_func_set_sensitivity_flags:
 * @name: Data processing function name.
 * @flags: Menu sensitivity flags from #GwyMenuSensFlags.
 *
 * Sets menu sensititivy flags for function @name.
 *
 * All data processing function have implied %GWY_MENU_FLAG_DATA flag which
 * cannot be removed.  This function can be used to set other requirements;
 * the most common (and most useful) probably is %GWY_MENU_FLAG_DATA_MASK
 * meaning the function requires a mask.
 *
 * Since: 1.1.
 **/
void
gwy_process_func_set_sensitivity_flags(const gchar *name,
                                       guint flags)
{
    GwyProcessFuncInfoInternal *ipfinfo;

    ipfinfo = g_hash_table_lookup(process_funcs, name);
    g_return_if_fail(ipfinfo);

    ipfinfo->sens_flags = flags;
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
 *
 * Since: 1.2.
 **/
guint
gwy_process_func_get_sensitivity_flags(const gchar *name)
{
    GwyProcessFuncInfoInternal *ipfinfo;

    ipfinfo = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(ipfinfo, 0);

    return ipfinfo->sens_flags;
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
gwy_process_func_remove(const gchar *name)
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
 * @GWY_RUN_INTERACTIVE: The function presents a non-modal GUI to the user,
 *                       it returns while after setting up the GUI, not after
 *                       finishing its work.
 * @GWY_RUN_MASK: The mask for all the run modes.
 *
 * Data processing function run-modes.
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
