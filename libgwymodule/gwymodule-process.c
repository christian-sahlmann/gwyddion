/* @(#) $Id$ */

#include <string.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkmenubar.h>
#include <libgwyddion/gwymacros.h>

#include "gwymodule-process.h"

static GHashTable *process_funcs;

/**
 * gwy_register_process_func:
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
gwy_register_process_func(const gchar *modname,
                          GwyProcessFuncInfo *func_info)
{
    GwyModuleInfoInternal *iinfo;

    gwy_debug("%s", __FUNCTION__);
    gwy_debug("name = %s, menu path = %s, run = %d, func = %p",
              func_info->name, func_info->menu_path, func_info->run,
              func_info->process);

    if (!process_funcs) {
        gwy_debug("%s: Initializing...", __FUNCTION__);
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
    g_hash_table_insert(process_funcs, (gpointer)func_info->name, func_info);
    return TRUE;
}

/**
 * gwy_run_process_func:
 * @name: Data processing function name.
 * @data: Data (a #GwyContainer).
 * @run: How the function should be run.
 *
 * Runs a data processing function identified by @name.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
gwy_run_process_func(const guchar *name,
                     GwyContainer *data,
                     GwyRunType run)
{
    GwyProcessFuncInfo *func_info;

    func_info = g_hash_table_lookup(process_funcs, name);
    g_return_val_if_fail(func_info, FALSE);
    g_return_val_if_fail(run & func_info->run, FALSE);
    g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE);
    return func_info->process(data, run);
}

static void
gwy_hash_table_to_slist_cb(gpointer key,
                           gpointer value,
                           gpointer user_data)
{
    GSList **list = (GSList**)user_data;

    *list = g_slist_prepend(*list, value);
}

static gint
process_menu_entry_compare(GtkItemFactoryEntry *a,
                           GtkItemFactoryEntry *b)
{
    return strcmp(a->path, b->path);
}

/**
 * gwy_build_process_menu:
 * @accel_group: The accelerator group the menu should use (%NULL for a new
 *               one).
 * @item_callback: A #GtkItemFactoryCallback1 called when an item from the
 *                 menu is selected.
 *
 * Creates #GtkItemFactory for a data processing menu with all registered data
 * processing functions.
 *
 * Returns: The menu item factory as a #GtkObject.
 **/
GtkObject*
gwy_build_process_menu(GtkAccelGroup *accel_group,
                       GCallback item_callback)
{
    const gsize bufsize = 4096;
    GtkItemFactory *item_factory;
    GtkItemFactoryEntry branch = { NULL, NULL, NULL, 0, "<Branch>", NULL };
    GtkItemFactoryEntry tearoff = { NULL, NULL, NULL, 0, "<Tearoff>", NULL };
    GtkItemFactoryEntry item = { NULL, NULL, item_callback, 0, "<Item>", NULL };
    gchar *current, *prev, *dp_str;
    GSList *l, *entries = NULL;
    gint i, dp_len;

    g_hash_table_foreach(process_funcs, gwy_hash_table_to_slist_cb, &entries);
    entries = g_slist_sort(entries, (GCompareFunc)process_menu_entry_compare);
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<proc>",
                                        accel_group);

    dp_str = "/_Data Process";
    dp_len = strlen(dp_str);

    /* the root branch */
    current = strncpy(g_new(gchar, bufsize), dp_str, bufsize);
    branch.path = current;
    gtk_item_factory_create_item(item_factory, &branch, NULL, 1);

    /* the root tearoff */
    prev = strncpy(g_new(gchar, bufsize), dp_str, bufsize);
    g_strlcpy(prev + dp_len, "/---", bufsize - dp_len);
    tearoff.path = prev;
    gtk_item_factory_create_item(item_factory, &tearoff, NULL, 1);

    /* create missing branches
     * XXX: Gtk+ essentially can do this itself
     * but this way we can e. g. put a tearoff at the top of each branch... */
    for (l = entries; l; l = g_slist_next(l)) {
        GwyProcessFuncInfo *func_info = (GwyProcessFuncInfo*)l->data;

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
            gtk_item_factory_create_item(item_factory, &branch, NULL, 1);

            strcpy(prev, current);
            g_strlcat(prev, "/---", bufsize);
            tearoff.path = prev;
            gtk_item_factory_create_item(item_factory, &tearoff, NULL, 1);
            current[i] = '/';

            /* find where the next / is  */
            do {
                i++;
            } while (current[i] && current[i] != '/');
        }

        /* XXX: passing directly func_info->name may be a little dangerous,
         * OTOH who would eventually free a newly allocated string? */
        item.path = current;
        gtk_item_factory_create_item(item_factory, &item, func_info->name, 1);

        GWY_SWAP(gchar*, current, prev);
    }

    g_free(prev);
    g_free(current);
    g_slist_free(entries);

    return (GtkObject*)item_factory;
}

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
 *
 * The type of data processing function.
 *
 * Returns: Whether it succeeded (XXX: this means exactly what?).
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
