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
#include <libgwyddion/gwymacros.h>

#include "gwymoduleinternal.h"

#define GWY_MODULE_QUERY_NAME G_STRINGIFY(_GWY_MODULE_QUERY)

static void gwy_load_modules_in_dir (GDir *gdir,
                                     const gchar *dirname,
                                     GHashTable *mods);
static void gwy_module_get_rid_of   (const gchar *modname);
static void gwy_module_init         (void);
static G_CONST_RETURN
GwyModuleInfo* gwy_module_do_register_module(const gchar *modulename,
                                             GHashTable *mods);

static GHashTable *modules;
static gboolean modules_initialized = FALSE;


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
        gwy_module_init();
    if (!paths)
        return;

    for (dir = *paths; dir; dir = *(++paths)) {
        GDir *gdir;
        GError *err = NULL;

        gwy_debug("Opening module directory %s", dir);
        gdir = g_dir_open(dir, 0, &err);
        if (err) {
            gwy_debug("Cannot open module directory %s: %s", dir, err->message);
            g_clear_error(&err);
            continue;
        }

        gwy_load_modules_in_dir(gdir, dir, modules);
        g_dir_close(gdir);
    }
}

/**
 * gwy_module_set_register_callback:
 * @callback: A callback function called when a function is registered with
 *            full (prefixed) function name.
 *
 * Sets function registration callback.
 *
 * Note this is very rudimentary and only one callback can exist at a time.
 **/
void
gwy_module_set_register_callback(void (*callback)(const gchar *fullname))
{
    _gwy_file_func_set_register_callback(callback);
    _gwy_graph_func_set_register_callback(callback);
    _gwy_layer_func_set_register_callback(callback);
    _gwy_process_func_set_register_callback(callback);
    _gwy_tool_func_set_register_callback(callback);
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
_GwyModuleInfoInternal*
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

/**
 * gwy_module_register_module:
 * @modulename: Module file name to load, including full path and extension.
 *
 * Loads a single module.
 *
 * Returns: Module info on success, %NULL on failure.
 *
 * Since: 1.4.
 **/
G_CONST_RETURN GwyModuleInfo*
gwy_module_register_module(const gchar *modulename)
{
    if (!modules_initialized)
        gwy_module_init();

    return gwy_module_do_register_module(modulename, modules);
}

static G_CONST_RETURN GwyModuleInfo*
gwy_module_do_register_module(const gchar *modulename,
                              GHashTable *mods)
{
    GModule *mod;
    gboolean ok;
    _GwyModuleInfoInternal *iinfo;
    GwyModuleInfo *mod_info = NULL;
    GwyModuleQueryFunc query;

    gwy_debug("Trying to load module %s.", modulename);
    mod = g_module_open(modulename, G_MODULE_BIND_LAZY);

    if (!mod) {
        g_warning("Cannot open module %s: %s",
                  modulename, g_module_error());
        return NULL;
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
        ok = !g_hash_table_lookup(mods, mod_info->name);
        if (!ok)
            g_warning("Duplicate module %s, keeping only the first one",
                        mod_info->name);
    }

    if (ok) {
        iinfo = g_new(_GwyModuleInfoInternal, 1);
        iinfo->mod_info = mod_info;
        iinfo->file = g_strdup(modulename);
        iinfo->loaded = TRUE;
        iinfo->funcs = NULL;
        g_hash_table_insert(mods, (gpointer)mod_info->name, iinfo);
        ok = mod_info->register_func(mod_info->name);
        if (!ok) {
            g_warning("Module %s feature registration failed",
                        mod_info->name);
            gwy_module_get_rid_of(mod_info->name);
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

    return ok ? mod_info : NULL;
}

static void
gwy_load_modules_in_dir(GDir *gdir,
                        const gchar *dirname,
                        GHashTable *mods)
{
    const gchar *filename;
    gchar *modulename;

    modulename = NULL;
    while ((filename = g_dir_read_name(gdir))) {
        if (g_str_has_prefix(filename, "."))
            continue;
        if (!g_str_has_suffix(filename, ".so")
             && !g_str_has_suffix(filename, ".dll")
             && !g_str_has_suffix(filename, ".DLL"))
            continue;
        modulename = g_build_filename(dirname, filename, NULL);
        gwy_module_do_register_module(modulename, mods);
        g_free(modulename);
    }
}

static void
gwy_module_get_rid_of(const gchar *modname)
{
    static const struct {
        const gchar *prefix;
        gboolean (*func)(const gchar*);
    }
    gro_funcs[] = {
        { GWY_MODULE_PREFIX_PROC,  gwy_process_func_remove },
        { GWY_MODULE_PREFIX_FILE,  gwy_file_func_remove },
        { GWY_MODULE_PREFIX_GRAPH, gwy_graph_func_remove },
        { GWY_MODULE_PREFIX_TOOL,  gwy_tool_func_remove },
    };

    _GwyModuleInfoInternal *iinfo;
    GSList *l;
    gsize i;

    iinfo = g_hash_table_lookup(modules, modname);
    g_return_if_fail(iinfo);
    /* FIXME: this is quite crude, it can remove functions of the same name
     * in different module type */
    for (l = iinfo->funcs; l; l = g_slist_next(l)) {
        gchar *canon_name = (gchar*)iinfo->funcs->data;

        for (i = 0; i < G_N_ELEMENTS(gro_funcs); i++) {
            if (g_str_has_prefix(canon_name, gro_funcs[i].prefix)
                && gro_funcs[i].func(canon_name + strlen(gro_funcs[i].prefix)))
                break;
        }
        if (i == G_N_ELEMENTS(gro_funcs)) {
            g_critical("Unable to find out %s function type", canon_name);
        }
        g_free(canon_name);
    }
    g_slist_free(iinfo->funcs);
    iinfo->funcs = NULL;
    g_hash_table_remove(modules, (gpointer)iinfo->mod_info->name);
    g_free(iinfo);
}

/**
 * gwy_module_init:
 *
 * Initializes the loadable module system, aborting if there's no support
 * for modules on the platform.
 *
 * Must be called at most once.  It's automatically called on first
 * gwy_module_register_modules() call.
 **/
static void
gwy_module_init(void)
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
 * gwy_module_lookup:
 * @name: A module name.
 *
 * Returns information about one module.
 *
 * Returns: The module info, of %NULL if not found.  It must be considered
 *          constant and never modified or freed.
 **/
G_CONST_RETURN GwyModuleInfo*
gwy_module_lookup(const gchar *name)
{
    _GwyModuleInfoInternal *iinfo;

    iinfo = gwy_module_get_module_info(name);
    return iinfo ? iinfo->mod_info : NULL;
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
 * @mod_info: The #GwyModuleInfo structure to return as module info.
 *
 * The declaration of module info query (the ONLY exported symbol from
 * a module).
 *
 * This macro does The Right Thing necessary to export module info in a way
 * Gwyddion understands it. Put #GWY_MODULE_QUERY with the module info
 * (#GwyModuleInfo) of your module as its argument on a line (with NO
 * semicolon after).
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
