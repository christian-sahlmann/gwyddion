/* @(#) $Id$ */

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
#include <app/file.h>

typedef enum {
    PLUGIN_LOAD = 1 << 0,
    PLUGIN_SAVE = 1 << 1,
    PLUGIN_ANY  = 0x03
} GwyLoadSave;

typedef struct {
    GwyFileFuncInfo func;
    GwyLoadSave run;
    gchar *glob;
    GPatternSpec *pattern;
    glong specificity;
    gchar *file;
} PluginInfo;

static gboolean       module_register            (const gchar *name);
static GList*         register_plugins           (GList *plugins,
                                                  const gchar *name,
                                                  const gchar *dir,
                                                  gchar *buffer);
static GwyContainer*  plugin_proxy_load          (const gchar *filename,
                                                  const gchar *name);
static gboolean       plugin_proxy_save          (GwyContainer *data,
                                                  const gchar *filename,
                                                  const gchar *name);
static gint           plugin_proxy_detect        (const gchar *filename,
                                                  gboolean only_name,
                                                  const gchar *name);
static PluginInfo*    find_plugin                (const gchar *name,
                                                  GwyLoadSave run);
static glong          pattern_specificity        (const gchar *pattern);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "plugin-proxy-file",
    "Plug-in proxy is a module capable of querying, registering, and running "
        "external programs (plug-ins) on data pretending they are file "
        "loading and saving modules.",
    "Yeti",
    "1.0",
    "Yeti",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

/* XXX: static data */
static GList *plugins = NULL;

static struct {
    const gchar *str;
    gint run;
}
const run_mode_names[] = {
    { "load", PLUGIN_LOAD },
    { "save", PLUGIN_SAVE },
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

    dir = g_build_filename(plugin_path, "file", NULL);
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
    gchar *pname, *file_desc, *run_modes, *glob;
    GwyLoadSave run;

    while (buffer) {
        if ((pname = next_line(&buffer))
            && *pname
            && (file_desc = next_line(&buffer))
            && *file_desc
            && (glob = next_line(&buffer))
            && *glob
            && (run_modes = next_line(&buffer))
            && (run = str_to_run_modes(run_modes))) {
            info = g_new(PluginInfo, 1);
            info->func.name = g_strdup(pname);
            info->func.file_desc = g_strdup(file_desc);
            info->func.detect = plugin_proxy_detect;
            info->func.load = (run && PLUGIN_LOAD) ? plugin_proxy_load : NULL;
            info->func.save = (run && PLUGIN_SAVE) ? plugin_proxy_save : NULL;
            if (gwy_file_func_register(name, &info->func)) {
                info->file = g_strdup(file);
                info->run = run;
                info->glob = g_strdup(glob);
                info->pattern = g_pattern_spec_new(glob);
                info->specificity = pattern_specificity(glob);
                plugins = g_list_prepend(plugins, info);
            }
            else {
                g_free((gpointer)info->func.name);
                g_free((gpointer)info->func.file_desc);
                g_free(info);
            }
        }
        while (buffer && *buffer)
            next_line(&buffer);
    }

    return plugins;
}

static GwyContainer*
plugin_proxy_load(const gchar *filename,
                  const gchar *name)
{
    PluginInfo *info;
    GwyContainer *data = NULL;
    gchar *tmpname = NULL, *buffer = NULL;
    GError *err = NULL;
    gint exit_status;
    gsize size = 0;
    FILE *fh;
    gchar *args[] = { NULL, NULL, NULL, NULL, NULL };
    gboolean ok;
    gint fd;

    gwy_debug("%s: called as %s with file `%s'", __FUNCTION__, name, filename);
    if (!(info = find_plugin(name, PLUGIN_LOAD)))
        return FALSE;

    fd = g_file_open_tmp("gwydXXXXXXXX", &tmpname, &err);
    if (fd < 0) {
        g_warning("Cannot create a temporary file: %s", err->message);
        return FALSE;
    }
    fh = fdopen(fd, "wb");
    g_return_val_if_fail(fh, FALSE);
    args[0] = info->file;
    args[1] = g_strdup(run_mode_to_str(PLUGIN_LOAD));
    args[2] = tmpname;
    args[3] = g_strdup(filename);
    gwy_debug("%s: %s %s %s %s", __FUNCTION__,
              args[0], args[1], args[2], args[3]);
    ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                      NULL, NULL, &exit_status, &err);
    if (!err)
        ok &= g_file_get_contents(tmpname, &buffer, &size, &err);
    unlink(tmpname);
    fclose(fh);
    gwy_debug("%s: ok = %d, exit_status = %d, err = %p", __FUNCTION__,
              ok, exit_status, err);
    ok &= !exit_status;
    if (!ok || !(data = text_dump_import(data, buffer, size))) {
        g_warning("Cannot run plug-in %s: %s",
                    info->file,
                    err ? err->message : "it returned garbage.");
    }
    g_free(args[1]);
    g_free(args[3]);
    g_clear_error(&err);
    g_free(buffer);
    g_free(tmpname);

    return data;
}

static gboolean
plugin_proxy_save(GwyContainer *data,
                  const gchar *filename,
                  const gchar *name)
{
    PluginInfo *info;
    gchar *tmpname = NULL;
    GError *err = NULL;
    gint exit_status;
    FILE *fh;
    gchar *args[] = { NULL, NULL, NULL, NULL, NULL };
    gboolean ok;

    gwy_debug("%s: called as %s with file `%s'", __FUNCTION__, name, filename);
    if (!(info = find_plugin(name, PLUGIN_SAVE)))
        return FALSE;

    fh = text_dump_export(data, &tmpname);
    g_return_val_if_fail(fh, FALSE);
    args[0] = info->file;
    args[1] = g_strdup(run_mode_to_str(PLUGIN_SAVE));
    args[2] = tmpname;
    args[3] = g_strdup(filename);
    gwy_debug("%s: %s %s %s %s", __FUNCTION__,
              args[0], args[1], args[2], args[3]);
    ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                      NULL, NULL, &exit_status, &err);
    unlink(tmpname);
    fclose(fh);
    gwy_debug("%s: ok = %d, exit_status = %d, err = %p", __FUNCTION__,
              ok, exit_status, err);
    ok &= !exit_status;
    if (!ok) {
        g_warning("Cannot run plug-in %s: %s",
                    info->file,
                    err ? err->message : "it returned garbage.");
        ok = FALSE;
    }
    g_free(args[1]);
    g_free(args[3]);
    g_clear_error(&err);
    g_free(tmpname);

    return ok;
}

static gint
plugin_proxy_detect(const gchar *filename,
                    gboolean only_name,
                    const gchar *name)
{
    PluginInfo *info;

    gwy_debug("%s: called as %s with file `%s'", __FUNCTION__, name, filename);
    if (!(info = find_plugin(name, PLUGIN_ANY)))
        return 0;
    if (!g_pattern_match_string(info->pattern, filename))
        return 0;

    return CLAMP(info->specificity, 1, 15);
}

static PluginInfo*
find_plugin(const gchar *name,
            GwyLoadSave run)
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
    if (!(info->run & run)) {
        g_critical("Plug-in `%s' doesn't suport this operation.", name);
        return NULL;
    }

    return info;
}

static glong
pattern_specificity(const gchar *pattern)
{
    glong psp = 0;
    gboolean changed;
    gchar *pat, *end, *p;

    g_return_val_if_fail(pattern && *pattern, 0);

    pat = g_strdup(pattern);
    end = pat + strlen(pat) - 1;
    /* change all '?' next to a '*' to '*' */
    do {
        changed = FALSE;
        for (p = pat; p < end; p++) {
            if (*p == '*' && *(p+1) == '?') {
                *(p+1) = '*';
                changed = TRUE;
            }
        }
        for (p = end; p > pat; p--) {
            if (*p == '*' && *(p-1) == '?') {
                *(p-1) = '*';
                changed = TRUE;
            }
        }
    } while (changed);

    end = p = pat;
    while (*p) {
        *end = *p;
        if (*p == '*') {
            while (*p == '*')
                p++;
        }
        else
            p++;
        end++;
    }
    *end = '\0';

    for (p = pat; *p; p++) {
        switch (*p) {
            case '*':
            psp -= 2;
            break;

            case '?':
            psp += 1;
            break;

            default:
            psp += 3;
            break;
        }
    }
    g_free(pat);

    return psp;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

