/* @(#) $Id$ */

#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwydgets/gwydgets.h>

#include "gwymodule.h"

static void gwy_load_modules_in_dir(GDir *gdir,
                                    const gchar *dirname,
                                    GHashTable *modules);

static GHashTable *modules;
static GHashTable *process_funcs;
static gboolean modules_initialized = FALSE;

void
gwy_modules_init(void)
{
    g_assert(!modules_initialized);

    /* Check whether modules are supported. */
    if (!g_module_supported()) {
        g_error("Cannot initialize modules: not supported on this platform.");
    }

    modules = g_hash_table_new(g_str_hash, g_str_equal);
    process_funcs = g_hash_table_new(g_direct_hash, g_direct_equal);
    modules_initialized = TRUE;
}

/**
 * gwy_module_register_modules:
 * @paths: A %NULL delimited list of directory names.
 *
 * Register all modules in given directories.
 *
 * Can be called several times (on different directories).
 **/
void
gwy_module_register_modules(const gchar **paths)
{
    const gchar *dir;

    if (!modules_initialized)
        gwy_modules_init();
    if (!paths)
        return;

    for (dir = *paths; dir; dir = *(++paths)) {
        GDir *gdir;
        GError *err = NULL;

        gwy_debug("Opening module directory %s", dir);
        gdir = g_dir_open(dir, 0, &err);
        if (err) {
            g_warning("Cannot open module directory %s", dir);
            g_clear_error(&err);
            continue;
        }

        gwy_load_modules_in_dir(gdir, dir, modules);
        g_dir_close(gdir);
    }
}

static void
gwy_load_modules_in_dir(GDir *gdir,
                        const gchar *dirname,
                        GHashTable *modules)
{
    const gchar *filename;
    gchar *modulename;

    modulename = NULL;
    while ((filename = g_dir_read_name(gdir))) {
        GModule *mod;
        gboolean ok;
        GwyModuleInfo *mod_info;
        GwyModuleQueryFunc query;

        if (!g_str_has_suffix(filename, ".so"))
            continue;
        modulename = g_build_filename(dirname, filename, NULL);
        gwy_debug("Trying to load module %s.", modulename);
        mod = g_module_open(modulename, G_MODULE_BIND_LAZY);

        if (!mod) {
            g_warning("Cannot open module %s: %s",
                      modulename, g_module_error());
            continue;
        }
        gwy_debug("Module loaded successfully as %s.", g_module_name(mod));

        /* Do a few sanity checks on the module before registration
         * is performed. */
        ok = TRUE;
        if (!g_module_symbol(mod, GWY_MODULE_QUERY_NAME, (gpointer)&query)
            || !query) {
            g_warning("No query function in module %s", modulename);
            ok = FALSE;
        }

        if (ok) {
            mod_info = query();
            if (!mod_info) {
                g_warning("No module info in module %s", modulename);
                ok = FALSE;
            }
        }

        if (ok) {
            ok = mod_info->abi_version == GWY_MODULE_ABI_VERSION;
            if (!ok)
                g_warning("Module %s ABI version %d is different from %d",
                          modulename, mod_info->abi_version,
                          GWY_MODULE_ABI_VERSION);
        }

        if (ok) {
            ok = mod_info->register_func
                 && mod_info->name && &mod_info->name
                 && mod_info->blurb && &mod_info->blurb
                 && mod_info->author && &mod_info->author
                 && mod_info->version && &mod_info->version
                 && mod_info->copyright && &mod_info->copyright
                 && mod_info->date && &mod_info->date;
            if (!ok)
                g_warning("Module %s info is invalid.",
                          modulename);
        }

        if (ok) {
            ok = !g_hash_table_lookup(modules, mod_info->name);
            if (!ok)
                g_warning("Duplicate module %s, keeping only the first one",
                          mod_info->name);
        }

        if (ok) {
            g_hash_table_insert(modules, (gpointer)mod_info->name, mod_info);
            ok = mod_info->register_func(mod_info->name);
            if (!ok) {
                g_warning("Module %s feature registration failed",
                          mod_info->name);
                /* TODO: clean up all possibly registered features */
                g_hash_table_remove(modules, (gpointer)mod_info->name);
            }
        }

        if (ok) {
            gwy_debug("Making module %s resident.", modulename);
            g_module_make_resident(mod);
        }
        else {
            if (!g_module_close(mod))
                g_critical("Cannot unload module %s: %s",
                           modulename, g_module_error());
        }

    }
}

gboolean
gwy_register_process_func(const gchar *modname,
                          GwyProcessFuncInfo *func_info)
{
    GwyModuleInfo *mod_info;

    gwy_debug("%s", __FUNCTION__);
    gwy_debug("name = %s, menu path = %s, run = %d, func = %p",
              func_info->name, func_info->menu_path, func_info->run,
              func_info->function);

    mod_info = g_hash_table_lookup(modules, modname);
    g_return_val_if_fail(mod_info, FALSE);
    g_return_val_if_fail(func_info->function, FALSE);
    g_return_val_if_fail(func_info->name, FALSE);
    g_return_val_if_fail(func_info->run & GWY_RUN_MASK, FALSE);
    if (g_hash_table_lookup(process_funcs, func_info->name)) {
        g_warning("Duplicate function %s, keeping only first", func_info->name);
        return FALSE;
    }
    g_hash_table_insert(process_funcs, (gpointer)func_info->name, func_info);
    return TRUE;
}

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
     * since we don't have any container loaded yet */
    /* g_return_val_if_fail(GWY_IS_CONTAINER(data), FALSE); */
    return func_info->function(data, run);
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
create_process_menu_entry(const guchar *name,
                          GwyProcessFuncInfo *info,
                          GList **entries)
{
    GtkItemFactoryEntry *menu_item;

    if (!info->menu_path || !*info->menu_path)
        return;

    menu_item = g_new(GtkItemFactoryEntry, 1);
    /* XXX: should be freed somewhere... */
    menu_item->path = g_strconcat("/_Data Process", info->menu_path, NULL);
    menu_item->accelerator = NULL;
    menu_item->callback = gwy_run_process_func_cb;
    menu_item->callback_action = 42;
    menu_item->item_type = "<Item>";
    /* XXX: this is CHEATING!  we set it to NULL later */
    menu_item->extra_data = name;

    *entries = g_list_prepend(*entries, menu_item);
}

static gint
process_menu_entry_compare(GtkItemFactoryEntry *a,
                           GtkItemFactoryEntry *b)
{
    return strcmp(a->path, b->path);
}

GtkItemFactory*
gwy_build_process_menu(void)
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
    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<data>", NULL);

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

    return item_factory;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
