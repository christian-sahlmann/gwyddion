/* @(#) $Id$ */

#include <string.h>
#include <gmodule.h>
#include <libgwyddion/gwymacros.h>

#include "gwymodule.h"

static void gwy_load_modules_in_dir(GDir *gdir, GHashTable *modules);

static GHashTable *modules = NULL;

void
gwy_modules_init(void)
{
    /* Check whether modules are supported. */
    if (!g_module_supported()) {
        g_error("Cannot initialize modules: not supported on this platform.\n");
    }
}

void
gwy_register_modules(const gchar **paths)
{
    const gchar *dir;

    if (!paths)
        return;

    if (!modules)
        modules = g_hash_table_new(g_str_hash, g_str_equal);

    for (dir = *paths; dir; dir = *(++paths)) {
        GDir *gdir;
        GError *err = NULL;

        gdir = g_dir_open(dir, 0, &err);
        if (err) {
            g_warning("Cannot open module directory %s", dir);
            g_clear_error(&err);
            continue;
        }

        gwy_load_modules_in_dir(gdir, modules);
        g_dir_close(gdir);
    }
}

static void
gwy_load_modules_in_dir(GDir *gdir, GHashTable *modules)
{
    const gchar *modulename;

    while ((modulename = g_dir_read_name(gdir))) {
        GModule *mod;
        GwyModuleRegisterFunc register_func;
        gboolean ok;
        GwyModuleInfo *mod_info;

        gwy_debug("Trying to load module %s.\n", modulename);
        mod = g_module_open(modulename, G_MODULE_BIND_LAZY);

        if (!mod) {
            g_warning("Cannot open module %s: %s\n",
                      modulename, g_module_error());
            continue;
        }
        gwy_debug("Module loaded successfully as %s.\n", g_module_name(mod));

        ok = TRUE;
        if (!g_module_symbol(mod, GWY_MODULE_REGISTER_FUNC_NAME,
                             (gpointer)&register_func)) {
            g_warning("No registration function in module %s", modulename);
            ok = FALSE;
        }

        if (ok) {
            mod_info = register_func(mod);
            ok = mod_info != NULL;
            if (!ok)
                g_warning("Module %s returned empty info", modulename);
        }

        if (ok) {
            ok = mod_info->abi_version != GWY_MODULE_ABI_VERSION;
            if (!ok)
                g_warning("Module %s ABI version %d is different from %d",
                          modulename, mod_info->abi_version,
                          GWY_MODULE_ABI_VERSION);
        }

        if (ok) {
            ok = !g_hash_table_lookup(modules, mod_info->name);
            if (!ok)
                g_warning("Duplicate module %s, keeping only the first one",
                          mod_info->name);
        }

        if (ok) {
            g_hash_table_insert(modules, (gpointer)mod_info->name, mod_info);
            gwy_debug("Making module %s resident.\n", modulename);
            g_module_make_resident(mod);
        }
        else {
            if (!g_module_close(mod))
                g_critical("Cannot unload module %s: %s\n",
                           modulename, g_module_error());
        }

    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
