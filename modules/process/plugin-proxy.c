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

/* XXX: ,safe` for Unix, but probably broken for Win32
 * It always creates the temporary file, keeps it open all the time during
 * plug-in runs, then unlinks it and closes at last.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/app.h>

#define PLUGIN_PROXY_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_MODAL | GWY_RUN_WITH_DEFAULTS)

typedef struct {
    GwyProcessFuncInfo func;
    gchar *file;
} PluginInfo;

static gboolean       module_register            (const gchar *name);
static GList*         register_plugins           (GList *plugins,
                                                  const gchar *name,
                                                  const gchar *dir,
                                                  gchar *buffer);
static gboolean       plugin_proxy               (GwyContainer *data,
                                                  GwyRunType run,
                                                  const gchar *name);
static PluginInfo*    find_plugin                (const gchar *name,
                                                  GwyRunType run);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "plugin-proxy-process",
    "Plug-in proxy is a module capable of querying, registering, and running "
        "external programs (plug-ins) on data pretending they are data "
        "processing modules.",
    "Yeti <yeti@physics.muni.cz>",
    "1.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

/* XXX: static data */
static GList *plugins = NULL;

static struct {
    const gchar *str;
    GwyRunType run;
}
const run_mode_names[] = {
    { "interactive", GWY_RUN_INTERACTIVE },
    { "noninteractive", GWY_RUN_NONINTERACTIVE },
    { "modal", GWY_RUN_MODAL },
    { "with_defaults", GWY_RUN_WITH_DEFAULTS },
};

/* XXX: extreme brain damage */
#include "../plugin-proxy-common.c"

static gboolean
module_register(const gchar *name)
{
    GwyContainer *settings;
    const gchar *plugin_path, *filename;
    gchar *buffer, *pluginname, *dir;
    gint exit_status;
    GDir *gdir;
    GError *err = NULL;
    gchar *args[] = { NULL, "register", NULL };
    gboolean ok;

    settings = gwy_app_settings_get();
    plugin_path = gwy_container_get_string_by_name(settings, "/app/plugindir");
    g_return_val_if_fail(plugin_path, FALSE);
    gwy_debug("%s: plug-in path is: %s", __FUNCTION__, plugin_path);

    dir = g_build_filename(plugin_path, "process", NULL);
    gdir = g_dir_open(dir, 0, &err);
    if (err) {
        g_warning("Cannot open plug-in directory %s: %s", dir, err->message);
        g_clear_error(&err);
        return FALSE;
    }
    while ((filename = g_dir_read_name(gdir))) {
        if (g_str_has_prefix(filename, ".")
            || g_str_has_suffix(filename, "~")
            || g_str_has_suffix(filename, ".BAK")
            || g_str_has_suffix(filename, ".bak"))
            continue;
        pluginname = g_build_filename(dir, filename, NULL);
        if (!g_file_test(pluginname, G_FILE_TEST_IS_EXECUTABLE)) {
            g_free(pluginname);
            continue;
        }
        gwy_debug("%s: plug-in %s", __FUNCTION__, filename);
        args[0] = pluginname;
        buffer = NULL;
        ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                          &buffer, NULL, &exit_status, &err);
        ok &= !exit_status;
        if (ok)
            plugins = register_plugins(plugins, name, pluginname, buffer);
        else {
            g_warning("Cannot register plug-in %s: %s",
                      filename, err ? err->message : "execution failed.");
            g_clear_error(&err);
        }
        g_free(pluginname);
        g_free(buffer);
    }
    g_dir_close(gdir);
    g_free(dir);

    return TRUE;
}

static GList*
register_plugins(GList *plugins,
                 const gchar *name,
                 const gchar *file,
                 gchar *buffer)
{
    PluginInfo *info;
    gchar *pname, *menu_path, *run_modes;
    GwyRunType run;

    while (buffer) {
        if ((pname = next_line(&buffer))
            && *pname
            && (menu_path = next_line(&buffer))
            && menu_path[0] == '/'
            && (run_modes = next_line(&buffer))
            && (run = str_to_run_modes(run_modes))) {
            info = g_new(PluginInfo, 1);
            info->func.name = g_strdup(pname);
            info->func.menu_path = g_strconcat("/_Plug-Ins", menu_path, NULL);
            info->func.process = plugin_proxy;
            info->func.run = run;
            if (gwy_process_func_register(name, &info->func)) {
                info->file = g_strdup(file);
                plugins = g_list_prepend(plugins, info);
            }
            else {
                g_free((gpointer)info->func.name);
                g_free((gpointer)info->func.menu_path);
                g_free(info);
            }
        }
        while (buffer && *buffer)
            next_line(&buffer);
    }

    return plugins;
}

static gboolean
plugin_proxy(GwyContainer *data, GwyRunType run, const gchar *name)
{
    GtkWidget *data_window;
    PluginInfo *info;
    gchar *filename, *buffer = NULL;
    GError *err = NULL;
    gint exit_status;
    gsize size = 0;
    FILE *fh;
    gchar *args[] = { NULL, "run", NULL, NULL, NULL };
    gboolean ok;

    gwy_debug("%s: called as %s with run mode %d", __FUNCTION__, name, run);
    if (!(info = find_plugin(name, run)))
        return FALSE;

    fh = text_dump_export(data, &filename);
    g_return_val_if_fail(fh, FALSE);
    args[0] = info->file;
    args[2] = g_strdup(run_mode_to_str(run));
    args[3] = filename;
    gwy_debug("%s: %s %s %s %s", __FUNCTION__,
              args[0], args[1], args[2], args[3]);
    ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                      NULL, NULL, &exit_status, &err);
    if (!err)
        ok &= g_file_get_contents(filename, &buffer, &size, &err);
    unlink(filename);
    fclose(fh);
    gwy_debug("%s: ok = %d, exit_status = %d, err = %p", __FUNCTION__,
              ok, exit_status, err);
    ok &= !exit_status;
    if (ok && (data = text_dump_import(data, buffer, size))) {
        data_window = gwy_app_data_window_create(data);
        gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window), NULL);
    }
    else {
        g_warning("Cannot run plug-in %s: %s",
                    info->file,
                    err ? err->message : "it returned garbage.");
        ok = FALSE;
    }
    g_free(args[2]);
    g_clear_error(&err);
    g_free(buffer);
    g_free(filename);

    return ok;
}

static PluginInfo*
find_plugin(const gchar *name,
            GwyRunType run)
{
    PluginInfo *info;
    GList *l;

    for (l = plugins; l; l = g_list_next(l)) {
        info = (PluginInfo*)l->data;
        if (strcmp(info->func.name, name) == 0)
            break;
    }
    if (!l) {
        g_critical("Don't know anything about plug-in `%s'.", name);
        return NULL;
    }
    if (!(info->func.run & run)) {
        g_critical("Plug-in `%s' doesn't suport this run mode.", name);
        return NULL;
    }

    return info;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

