/* @(#) $Id$ */

#include <string.h>
#include <libgwyddion/gwymacros.h>

#include "gwymoduleloader.h"

#define GWY_MODULE_QUERY_NAME G_STRINGIFY(_GWY_MODULE_QUERY)

static void gwy_load_modules_in_dir(GDir *gdir,
                                    const gchar *dirname,
                                    GHashTable *modules);

static GHashTable *modules;
static gboolean modules_initialized = FALSE;


/**
 * gwy_modules_init:
 *
 * Initializes the loadable module system, aborting if there's no support
 * for modules on the platform.
 *
 * Must be called at most once.  It's automatically called on first
 * gwy_module_register_modules() call.
 **/
void
gwy_modules_init(void)
{
    g_assert(!modules_initialized);

    /* Check whether modules are supported. */
    if (!g_module_supported()) {
        g_error("Cannot initialize modules: not supported on this platform.");
    }

    modules = g_hash_table_new(g_str_hash, g_str_equal);
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

/**
 * gwy_module_get_module_info:
 * @name: A module name.
 *
 * Gets internal module info for module identified by @name.
 *
 * This function exposes internal module info and is intended to be used only
 * by the rest of Gwyddion module system.
 *
 * Returns: The internal module info.
 **/
GwyModuleInfoInternal*
gwy_module_get_module_info(const gchar *name)
{
    g_assert(modules_initialized);

    return g_hash_table_lookup(modules, name);
}

/**
 * gwy_module_foreach:
 * @function: A #GHFunc run for each module.
 * @data: User data.
 *
 * Runs @function on each registered module, passing module name as the key
 * and module internal info as the value.
 *
 * This function exposes internal module info and is intended to be used only
 * by the rest of Gwyddion module system.
 **/
void
gwy_module_foreach(GHFunc function,
                   gpointer data)
{
    g_assert(modules_initialized);

    g_hash_table_foreach(modules, function, data);
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
        GwyModuleInfoInternal *iinfo;
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
            iinfo = g_new(GwyModuleInfoInternal, 1);
            iinfo->mod_info = mod_info;
            iinfo->file = g_strdup(filename);
            iinfo->loaded = TRUE;
            g_hash_table_insert(modules, (gpointer)mod_info->name, iinfo);
            ok = mod_info->register_func(mod_info->name);
            if (!ok) {
                g_warning("Module %s feature registration failed",
                          mod_info->name);
                /* TODO: clean up all possibly registered features */
                g_hash_table_remove(modules, (gpointer)mod_info->name);
                g_free(iinfo);
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

/************************** Documentation ****************************/

/**
 * GWY_MODULE_ABI_VERSION:
 *
 * Gwyddion module ABI version.
 *
 * To be filled as @abi_version in #GwyModuleInfo.
 **/

/**
 * GWY_MODULE_QUERY(mod_info):
 * @mod_info: The %GwyModuleInfo structure to return as module info.
 *
 * The declaration of module info query (the ONLY exported symbol from
 * a module).
 **/

/**
 * GwyRunType:
 * @GWY_RUN_INTERACTIVE: The function can present a GUI to the user, if it
 *                       wishes so.
 * @GWY_RUN_NONINTERACTIVE: The function is run non-interactively, and it
 *                          should use parameter values stored in the
 *                          container to reproduce previous runs.
 * @GWY_RUN_WITH_DEFAULTS: The function is run non-interactively, and it
 *                         should use default parameter values.
 * @GWY_RUN_MASK: The mask.
 *
 * Data processing function run-modes.
 **/

/**
 * GwyModuleRegisterFunc:
 * @name: An unique module name.
 *
 * Module registration function type.
 *
 * It actually runs particular featrue registration functions, like
 * gwy_module_register_file_func() and gwy_module_register_process_func().
 *
 * Returns: Whether the registration succeeded.  When it returns %FALSE, the
 *          module and its features are unloaded (FIXME: maybe. Currenly only
 *          module is unloaded, features are NOT unregistered, this can lead
 *          to all kinds of disasters).
 **/

/**
 * GwyModuleQueryFunc:
 *
 * Module query function type.
 *
 * The module query function should be simply declared as
 * GWY_MODULE_QUERY(mod_info), where mod_info is module info struct for
 * the module.
 *
 * Returns: The module info struct.
 **/

/**
 * GwyModuleInfo:
 * @abi_version: Gwyddion module ABI version, should be always
 *               #GWY_MODULE_ABI_VERSION.
 * @register_func: Module registration function (the function run by Gwyddion
 *                 module system, actually registering particular module
 *                 features).
 * @name: An unique module name.
 * @blurb: Some module description.
 * @author: Module author(s).
 * @version: Module version.
 * @copyright: Who has copyright on this module.
 * @date: Date (year).
 *
 * Module information returned by GWY_MODULE_QUERY().
 **/

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
