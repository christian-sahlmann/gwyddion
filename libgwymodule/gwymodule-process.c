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
    /* XXX the test is commented out only for testing,
     * since we don't have any container loaded yet
     * TODO uncomment it ASAP */
    /* g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE); */
    return func_info->process(data, run);
}

static void
gwy_run_process_func_cb(gchar *name,
                        guint cb_action,
                        GtkWidget *who_knows)
{
    gwy_debug("first argument = %s", name);
    gwy_debug("second argument = %u", cb_action);
    gwy_debug("third argument = %p (%s)",
              who_knows, g_type_name(G_TYPE_FROM_INSTANCE(who_knows)));
    gwy_run_process_func(name, NULL, GWY_RUN_NONINTERACTIVE);
}

static void
create_process_menu_entry(const guchar *key,
                          GwyProcessFuncInfo *func_info,
                          GList **entries)
{
    GtkItemFactoryEntry *menu_item;

    g_assert(strcmp(key, func_info->name) == 0);
    if (!func_info->menu_path || !*func_info->menu_path)
        return;

    menu_item = g_new(GtkItemFactoryEntry, 1);
    menu_item->path = g_strconcat("/_Data Process", func_info->menu_path, NULL);
    menu_item->accelerator = NULL;
    menu_item->callback = gwy_run_process_func_cb;
    menu_item->callback_action = 42;
    menu_item->item_type = "<Item>";
    /* XXX: this is CHEATING!  we set it to NULL later */
    menu_item->extra_data = key;

    *entries = g_list_prepend(*entries, menu_item);
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
 *
 * Creates #GtkItemFactory for a data processing menu with all registered data
 * processing functions.
 *
 * Returns: The menu as a #GtkObject.
 **/
GtkObject*
gwy_build_process_menu(GtkAccelGroup *accel_group)
{
    GtkItemFactory *item_factory;
    GtkItemFactoryEntry *menu_item;
    GtkItemFactoryEntry branch, tearoff;
    GList *entries = NULL;
    GList *l, *p;
    gpointer cbdata;
    gint i;

    g_hash_table_foreach(process_funcs, (GHFunc)create_process_menu_entry,
                         &entries);
    entries = g_list_sort(entries, (GCompareFunc)process_menu_entry_compare);
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<data>",
                                        accel_group);

    /* the root item */
    menu_item = g_new(GtkItemFactoryEntry, 1);
    menu_item->path = g_strdup("/_Data Process");
    menu_item->accelerator = "<control>D";
    menu_item->callback = NULL;
    menu_item->callback_action = 0;
    menu_item->item_type = "<Branch>";
    menu_item->extra_data = NULL;
    entries = g_list_prepend(entries, menu_item);
    gwy_debug("inserting %s `%s'", menu_item->item_type, menu_item->path);
    gtk_item_factory_create_item(item_factory, menu_item, NULL, 1);

    /* the root tearoff */
    tearoff.path = "/_Data Process/---";
    tearoff.accelerator = NULL;
    tearoff.callback = NULL;
    tearoff.callback_action = 0;
    tearoff.item_type = "<Tearoff>";
    tearoff.extra_data = NULL;
    gwy_debug("inserting %s `%s'", tearoff.item_type, tearoff.path);
    gtk_item_factory_create_item(item_factory, &tearoff, NULL, 1);

    branch.item_type = "<Branch>";
    branch.accelerator = NULL;
    branch.callback = NULL;
    branch.callback_action = 0;
    branch.extra_data = NULL;

    /* create missing branches
     * XXX: Gtk+ can do this itself
     * but this way we can e. g. put a tearoff at the top of each branch... */
    p = entries;
    for (l = p->next; l; l = l->next) {
        GtkItemFactoryEntry *le = (GtkItemFactoryEntry*)l->data;
        GtkItemFactoryEntry *pe = (GtkItemFactoryEntry*)p->data;
        guchar *lp = le->path;
        guchar *pp = pe->path;

        gwy_debug("<Item> %s", lp);
        /* find where the paths differ */
        for (i = 0; lp[i] && pp[i] && lp[i] == pp[i]; i++)
            ;
        if (!lp[i])
            break;

        /* find where the next / is  */
        do {
            i++;
        } while (lp[i] && lp[i] != '/');
        while (lp[i]) {
            /* create a branch with a tearoff */
            branch.path = g_strndup(lp, i);
            gwy_debug("inserting %s `%s'", branch.item_type, branch.path);
            gtk_item_factory_create_item(item_factory, &branch, NULL, 1);
            tearoff.path = g_strconcat(branch.path, "/---", NULL);
            gwy_debug("inserting %s `%s'", tearoff.item_type, tearoff.path);
            gtk_item_factory_create_item(item_factory, &tearoff, NULL, 1);
            g_free(tearoff.path);
            g_free(branch.path);

            /* find where the next / is  */
            do {
                i++;
            } while (lp[i] && lp[i] != '/');
        }

        /* the ugly `cbdata in extra_data' trick */
        menu_item = (GtkItemFactoryEntry*)l->data;
        cbdata = (gpointer)menu_item->extra_data;
        menu_item->extra_data = NULL;
        gtk_item_factory_create_item(item_factory, menu_item, cbdata, 1);
    }

    for (l = entries; l; l = l->next) {
        menu_item = (GtkItemFactoryEntry*)l->data;
        g_free(menu_item->path);
        g_free(menu_item);
    }
    g_list_free(entries);

    return (GtkObject*)item_factory;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
