/*
 *  @(%) $Id$
 *  Copyright (C) 2003,2004 David Necas (Yeti), Petr Klapetek.
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
 *
 * XXX: it also has to open dump files in binary mode, BUT still assumes
 * normal Unix \n EOLs (one-way now fixed by next_line() accepting both).
 *
 * XXX: the `dump' should probably be `dumb'...
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/file.h>
#include <app/app.h>

#define PLUGIN_PROXY_RUN_MODES \
    (GWY_RUN_NONINTERACTIVE | GWY_RUN_MODAL | GWY_RUN_WITH_DEFAULTS)

typedef struct {
    GwyProcessFuncInfo func;
    gchar *file;
} ProcPluginInfo;

typedef struct {
    GwyFileFuncInfo func;
    GwyFileOperation run;
    gchar *glob;
    GPatternSpec **pattern;
    glong *specificity;
    gchar *file;
} FilePluginInfo;

typedef GList* (*ProxyRegister)(GList *plugins,
                                const gchar *name,
                                const gchar *dir,
                                gchar *buffer);

/* top-level */
static gboolean        module_register           (const gchar *name);
static GList*          register_plugins          (GList *plugins,
                                                  const gchar *dir,
                                                  const gchar *name,
                                                  ProxyRegister register_func);

/* process plug-in proxy */
static GList*          proc_register_plugins     (GList *plugins,
                                                  const gchar *name,
                                                  const gchar *dir,
                                                  gchar *buffer);
static gboolean        proc_plugin_proxy_run     (GwyContainer *data,
                                                  GwyRunType run,
                                                  const gchar *name);
static ProcPluginInfo* proc_find_plugin          (const gchar *name,
                                                  GwyRunType run);

/* file plug-in-proxy */
static GList*          file_register_plugins     (GList *plugins,
                                                  const gchar *name,
                                                  const gchar *dir,
                                                  gchar *buffer);
static GwyContainer*   file_plugin_proxy_load    (const gchar *filename,
                                                  const gchar *name);
static gboolean        file_plugin_proxy_save    (GwyContainer *data,
                                                  const gchar *filename,
                                                  const gchar *name);
static gint            file_plugin_proxy_detect  (const gchar *filename,
                                                  gboolean only_name,
                                                  const gchar *name);
static FilePluginInfo* file_find_plugin          (const gchar *name,
                                                  GwyFileOperation run);
static GPatternSpec**  file_patternize_globs     (const gchar *glob);
static glong*          file_glob_specificities   (const gchar *glob);
static glong           file_pattern_specificity  (const gchar *pattern);

/* common helpers */
static FILE*           text_dump_export          (GwyContainer *data,
                                                  gchar **filename);
static void            dump_export_meta_cb       (gpointer hkey,
                                                  GValue *value,
                                                  FILE *fh);
static void            dump_export_data_field    (GwyDataField *dfield,
                                                  const gchar *name,
                                                  FILE *fh);
static FILE*           open_temporary_file       (gchar **filename);
static GwyContainer*   text_dump_import          (GwyContainer *old_data,
                                                  gchar *buffer,
                                                  gsize size);
static gchar*          next_line                 (gchar **buffer);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "plugin-proxy",
    "Plug-in proxy is a module capable of querying, registering, and running "
        "external programs (plug-ins) on data pretending they are data "
        "processing or file loading/saving modules.",
    "Yeti <yeti@physics.muni.cz>",
    "2.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

/* XXX: static data */
static GList *proc_plugins = NULL;
static GList *file_plugins = NULL;

static const GwyEnum run_mode_names[] = {
    { "interactive",    GWY_RUN_INTERACTIVE },
    { "noninteractive", GWY_RUN_NONINTERACTIVE },
    { "modal",          GWY_RUN_MODAL },
    { "with_defaults",  GWY_RUN_WITH_DEFAULTS },
    { NULL,             -1 }
};

static const GwyEnum file_op_names[] = {
    { "load", GWY_FILE_LOAD },
    { "save", GWY_FILE_SAVE },
    { NULL,   -1 }
};

static gboolean
module_register(const gchar *name)
{
    gchar *plugin_path;
    gchar *dir;

    plugin_path = gwy_find_self_dir("plugins");
    g_return_val_if_fail(plugin_path, FALSE);
    gwy_debug("plug-in path is: %s", plugin_path);

    dir = g_build_filename(plugin_path, "process", NULL);
    proc_plugins = register_plugins(NULL, dir, name, proc_register_plugins);
    g_free(dir);

    dir = g_build_filename(plugin_path, "file", NULL);
    file_plugins = register_plugins(NULL, dir, name, file_register_plugins);
    g_free(dir);

    g_free(plugin_path);

    return TRUE;
}

/**
 * register_plugins:
 * @plugins: Existing plug-in list.
 * @dir: Plug-in directory to search them in.
 * @name: Plug-in proxy module name (to be used passed to @register_func).
 * @register_func: Particular registration function.
 *
 * Register all plug-ins in a directory @dir with @register_func and add
 * them to @plugins.
 *
 * Returns: The new plug-in list, with all registered plug-in prepended.
 **/
static GList*
register_plugins(GList *plugins,
                 const gchar *dir,
                 const gchar *name,
                 ProxyRegister register_func)
{
    gchar *args[] = { NULL, "register", NULL };
    const gchar *filename;
    gchar *buffer, *pluginname;
    GError *err = NULL;
    gint exit_status;
    gboolean ok;
    GDir *gdir;

    gdir = g_dir_open(dir, 0, &err);
    if (err) {
        g_warning("Cannot open plug-in directory %s: %s", dir, err->message);
        g_clear_error(&err);
        return plugins;
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
        gwy_debug("plug-in %s", filename);
        args[0] = pluginname;
        buffer = NULL;
        ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                          &buffer, NULL, &exit_status, &err);
        ok &= !exit_status;
        if (ok)
            plugins = register_func(plugins, name, pluginname, buffer);
        else {
            g_warning("Cannot register plug-in %s: %s",
                      filename, err ? err->message : "execution failed.");
            g_clear_error(&err);
        }
        g_free(pluginname);
        g_free(buffer);
    }
    g_dir_close(gdir);

    return plugins;
}


/***** Proc ****************************************************************/

/**
 * proc_register_plugins:
 * @plugins: Plug-in list to eventually add the plug-in to.
 * @name: Module name for gwy_process_func_register().
 * @file: Plug-in file (full path).
 * @buffer: The output from "plugin register".
 *
 * Parse output from "plugin register" and eventually add it to the
 * plugin-list.
 *
 * Returns: The new plug-in list, with the plug-in eventually prepended.
 **/
static GList*
proc_register_plugins(GList *plugins,
                      const gchar *name,
                      const gchar *file,
                      gchar *buffer)
{
    ProcPluginInfo *info;
    gchar *pname = NULL, *menu_path = NULL, *run_modes = NULL;
    GwyRunType run;

    gwy_debug("buffer: <<<%s>>>", buffer);
    while (buffer) {
        if ((pname = next_line(&buffer))
            && *pname
            && (menu_path = next_line(&buffer))
            && menu_path[0] == '/'
            && (run_modes = next_line(&buffer))
            && (run = gwy_string_to_flags(run_modes,
                                          run_mode_names, -1, NULL))) {
            info = g_new(ProcPluginInfo, 1);
            info->func.name = g_strdup(pname);
            info->func.menu_path = g_strconcat("/_Plug-Ins", menu_path, NULL);
            info->func.process = proc_plugin_proxy_run;
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
        else if (pname && *pname) {
            g_warning("%s failed; "
                      "pname = %s, menu_path = %s, run_modes = %s",
                      name, pname, menu_path, run_modes);
        }
        while (buffer && *buffer)
            next_line(&buffer);
    }

    return plugins;
}

/**
 * proc_plugin_proxy_run:
 * @data: A data container.
 * @run: Run mode.
 * @name: Plug-in name (i.e. data processing function) to run.
 *
 * The plug-in proxy itself, runs plug-in @name on @data.
 *
 * Returns: Whether it succeeded running the plug-in.
 **/
static gboolean
proc_plugin_proxy_run(GwyContainer *data,
                      GwyRunType run,
                      const gchar *name)
{
    GtkWidget *data_window;
    ProcPluginInfo *info;
    gchar *filename, *buffer = NULL;
    GError *err = NULL;
    gint exit_status;
    gsize size = 0;
    FILE *fh;
    gchar *args[] = { NULL, "run", NULL, NULL, NULL };
    gboolean ok;

    gwy_debug("called as %s with run mode %d", name, run);
    if (!(info = proc_find_plugin(name, run)))
        return FALSE;

    fh = text_dump_export(data, &filename);
    g_return_val_if_fail(fh, FALSE);
    args[0] = info->file;
    args[2] = g_strdup(gwy_enum_to_string(run, run_mode_names, -1));
    args[3] = filename;
    gwy_debug("%s %s %s %s", args[0], args[1], args[2], args[3]);
    ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                      NULL, NULL, &exit_status, &err);
    if (!err)
        ok &= g_file_get_contents(filename, &buffer, &size, &err);
    unlink(filename);
    fclose(fh);
    gwy_debug("ok = %d, exit_status = %d, err = %p", ok, exit_status, err);
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

/**
 * proc_find_plugin:
 * @name: Plug-in name.
 * @run: Run modes it is supposed to support.
 *
 * Finds a data processing plugin of name @name supporting at least one of the
 * modes in @run.
 *
 * Returns: The plug-in info.
 **/
static ProcPluginInfo*
proc_find_plugin(const gchar *name,
                 GwyRunType run)
{
    ProcPluginInfo *info;
    GList *l;

    for (l = proc_plugins; l; l = g_list_next(l)) {
        info = (ProcPluginInfo*)l->data;
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

/***** File ****************************************************************/

/**
 * file_register_plugins:
 * @plugins: Plug-in list to eventually add the plug-in to.
 * @name: Module name for gwy_file_func_register().
 * @file: Plug-in file (full path).
 * @buffer: The output from "plugin register".
 *
 * Parse output from "plugin register" and eventually add it to the
 * plugin-list.
 *
 * Returns: The new plug-in list, with the plug-in eventually prepended.
 **/
static GList*
file_register_plugins(GList *plugins,
                      const gchar *name,
                      const gchar *file,
                      gchar *buffer)
{
    FilePluginInfo *info;
    gchar *pname = NULL, *file_desc = NULL, *run_modes = NULL, *glob = NULL;
    GwyFileOperation run;

    gwy_debug("buffer: <<<%s>>>", buffer);
    while (buffer) {
        if ((pname = next_line(&buffer))
            && *pname
            && (file_desc = next_line(&buffer))
            && *file_desc
            && (glob = next_line(&buffer))
            && *glob
            && (run_modes = next_line(&buffer))
            && (run = gwy_string_to_flags(run_modes,
                                          file_op_names, -1, NULL))) {
            info = g_new(FilePluginInfo, 1);
            info->func.name = g_strdup(pname);
            info->func.file_desc = g_strdup(file_desc);
            info->func.detect = file_plugin_proxy_detect;
            info->func.load = (run & GWY_FILE_LOAD) ? file_plugin_proxy_load
                                                    : NULL;
            info->func.save = (run & GWY_FILE_SAVE) ? file_plugin_proxy_save
                                                    : NULL;
            if (gwy_file_func_register(name, &info->func)) {
                info->file = g_strdup(file);
                info->run = run;
                info->glob = g_strdup(glob);
                info->pattern = file_patternize_globs(glob);
                info->specificity = file_glob_specificities(glob);
                plugins = g_list_prepend(plugins, info);
            }
            else {
                g_free((gpointer)info->func.name);
                g_free((gpointer)info->func.file_desc);
                g_free(info);
            }
        }
        else if (pname && *pname) {
            g_warning("%s failed; "
                      "pname = %s, file_desc = %s, run_modes = %s, glob = %s",
                      name, pname, file_desc, run_modes, glob);
        }
        while (buffer && *buffer)
            next_line(&buffer);
    }

    return plugins;
}

/**
 * file_plugin_proxy_load:
 * @filename. A file name to load.
 * @name: Plug-in name (i.e. file-loading function) to run.
 *
 * The plug-in proxy itself, runs file-loading plug-in @name to load @filename.
 *
 * Returns: A newly created data container with the contents of @filename,
 *          or %NULL if it fails.
 **/
static GwyContainer*
file_plugin_proxy_load(const gchar *filename,
                       const gchar *name)
{
    FilePluginInfo *info;
    GwyContainer *data = NULL;
    gchar *tmpname = NULL, *buffer = NULL;
    GError *err = NULL;
    gint exit_status;
    gsize size = 0;
    FILE *fh;
    gchar *args[] = { NULL, NULL, NULL, NULL, NULL };
    gboolean ok;

    gwy_debug("called as %s with file `%s'", name, filename);
    if (!(info = file_find_plugin(name, GWY_FILE_LOAD)))
        return FALSE;

    if (!(fh = open_temporary_file(&tmpname)))
        return FALSE;
    args[0] = info->file;
    args[1] = g_strdup(gwy_enum_to_string(GWY_FILE_LOAD, file_op_names, -1));
    args[2] = tmpname;
    args[3] = g_strdup(filename);
    gwy_debug("%s %s %s %s", args[0], args[1], args[2], args[3]);
    ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                      NULL, NULL, &exit_status, &err);
    if (!err)
        ok &= g_file_get_contents(tmpname, &buffer, &size, &err);
    unlink(tmpname);
    fclose(fh);
    gwy_debug("ok = %d, exit_status = %d, err = %p", ok, exit_status, err);
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

/**
 * file_plugin_proxy_save:
 * @data: A data container to save.
 * @filename: A file name to save @data to.
 * @name: Plug-in name (i.e. file-saving function) to run.
 *
 * The plug-in proxy itself, runs file-saving plug-in @name to save @filename.
 *
 * Returns: Whether it succeeded saving the data.
 **/
static gboolean
file_plugin_proxy_save(GwyContainer *data,
                       const gchar *filename,
                       const gchar *name)
{
    FilePluginInfo *info;
    gchar *tmpname = NULL;
    GError *err = NULL;
    gint exit_status;
    FILE *fh;
    gchar *args[] = { NULL, NULL, NULL, NULL, NULL };
    gboolean ok;

    gwy_debug("called as %s with file `%s'", name, filename);
    if (!(info = file_find_plugin(name, GWY_FILE_SAVE)))
        return FALSE;

    fh = text_dump_export(data, &tmpname);
    g_return_val_if_fail(fh, FALSE);
    args[0] = info->file;
    args[1] = g_strdup(gwy_enum_to_string(GWY_FILE_SAVE, file_op_names, -1));
    args[2] = tmpname;
    args[3] = g_strdup(filename);
    gwy_debug("%s %s %s %s", args[0], args[1], args[2], args[3]);
    ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                      NULL, NULL, &exit_status, &err);
    unlink(tmpname);
    fclose(fh);
    gwy_debug("ok = %d, exit_status = %d, err = %p", ok, exit_status, err);
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

/**
 * file_plugin_proxy_detect:
 * @filename: A file name to detect type of..
 * @only_name: Whether only name should be used for detection (otherwise
 *             trying to open the file is allowed).  Note this parameter is
 *             formal, as the proxy always decides only on filename basis.
 * @name: Plug-in name (i.e. file-detection function) to run.
 *
 * The plug-in proxy itself.  Emulates filetype detection based on file
 * name glob given by the plug-in during registration.
 *
 * Returns: The score (as defined in gwyddion filetype module interface).
 **/
static gint
file_plugin_proxy_detect(const gchar *filename,
                         G_GNUC_UNUSED gboolean only_name,
                         const gchar *name)
{
    FilePluginInfo *info;
    gint i, max;

    gwy_debug("called as %s with file `%s'", name, filename);
    if (!(info = file_find_plugin(name, GWY_FILE_MASK)))
        return 0;

    max = G_MININT;
    for (i = 0; info->pattern[i]; i++) {
        if (info->specificity[i] > max
            && g_pattern_match_string(info->pattern[i], filename))
            max = info->specificity[i];
    }
    if (max == G_MININT)
        return 0;

    return CLAMP(max, 1, 20);
}

/**
 * file_find_plugin:
 * @name: Plug-in name.
 * @run: File operations it is supposed to support.
 *
 * Finds a filetype plugin of name @name supporting at least one of the
 * file operations in @run.
 *
 * Returns: The plug-in info.
 **/
static FilePluginInfo*
file_find_plugin(const gchar *name,
                 GwyFileOperation run)
{
    FilePluginInfo *info;
    GList *l;

    for (l = file_plugins; l; l = g_list_next(l)) {
        info = (FilePluginInfo*)l->data;
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

static GPatternSpec**
file_patternize_globs(const gchar *glob)
{
    GPatternSpec **specs;
    gchar **globs;
    gchar *s;
    gint i, n;

    globs = g_strsplit(glob, " ", 0);
    if (!globs) {
        specs = g_new(GPatternSpec*, 1);
        *specs = NULL;
        return specs;
    }

    for (n = 0; globs[n]; n++)
        ;
    specs = g_new(GPatternSpec*, n+1);
    for (i = 0; i < n; i++) {
        s = g_strstrip(globs[i]);
        specs[i] = g_pattern_spec_new(s);
    }
    specs[n] = NULL;
    g_strfreev(globs);

    return specs;
}

static glong*
file_glob_specificities(const gchar *glob)
{
    glong *specs;
    gchar **globs;
    gchar *s;
    gint i, n;

    globs = g_strsplit(glob, " ", 0);
    if (!globs) {
        specs = g_new(glong, 1);
        *specs = 0;
        return specs;
    }

    for (n = 0; globs[n]; n++)
        ;
    specs = g_new(glong, n+1);
    for (i = 0; i < n; i++) {
        s = g_strstrip(globs[i]);
        specs[i] = file_pattern_specificity(s);
    }
    specs[n] = 0;
    g_strfreev(globs);

    return specs;
}

/**
 * file_pattern_specificity:
 * @pattern: A fileglob-like pattern, as supported by #GPatternSpec.
 *
 * Computes a number approximately representing pattern specificity.
 *
 * The pattern specificity increases with the number of non-wildcards in
 * the pattern and decreases with the number of wildcards (*) in the pattern.
 *
 * Returns: The pattern specificity. Normally a small integer, may be even
 *          negative (e.g. for "*").
 **/
static glong
file_pattern_specificity(const gchar *pattern)
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

/***** Sub *****************************************************************/

/**
 * text_dump_export:
 * @data: A %GwyContainer to dump.
 * @filename: File name to dump the container to.
 *
 * Dumps data container to a file @filename.
 *
 * In fact, it only dumps data and mask %DataField's and everything under
 * "/meta" as strings.
 *
 * Returns: A filehandle of the dump file open in "wb" mode.
 **/
static FILE*
text_dump_export(GwyContainer *data, gchar **filename)
{
    GwyDataField *dfield;
    FILE *fh;

    if (!(fh = open_temporary_file(filename)))
        return NULL;
    gwy_container_foreach(data, "/meta", (GHFunc)dump_export_meta_cb, fh);
    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, "/0/data"));
    dump_export_data_field(dfield, "/0/data", fh);
    if (gwy_container_contains_by_name(data, "/0/mask")) {
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                 "/0/mask"));
        dump_export_data_field(dfield, "/0/mask", fh);
    }
    fflush(fh);

    return fh;
}

/**
 * dump_export_meta_cb:
 * @hkey: A disguised #GQuark key.
 * @value: A string value.
 * @fh: A filehandle open for writing.
 *
 * Dumps a one string value to @fh.
 **/
static void
dump_export_meta_cb(gpointer hkey, GValue *value, FILE *fh)
{
    GQuark quark = GPOINTER_TO_UINT(hkey);
    const gchar *key;

    key = g_quark_to_string(quark);
    g_return_if_fail(key);
    g_return_if_fail(G_VALUE_TYPE(value) == G_TYPE_STRING);
    fprintf(fh, "%s=%s\n", key, g_value_get_string(value));
}

/**
 * dump_export_data_field:
 * @dfield: A #GwyDataField.
 * @name: The name of @dfield.
 * @fh: A filehandle open for writing.
 *
 * Dumps a one #GwyDataField to @fh.
 **/
static void
dump_export_data_field(GwyDataField *dfield, const gchar *name, FILE *fh)
{
    gint xres, yres;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    fprintf(fh, "%s/xres=%d\n", name, xres);
    fprintf(fh, "%s/yres=%d\n", name, yres);
    fprintf(fh, "%s/xreal=%.16g\n", name, gwy_data_field_get_xreal(dfield));
    fprintf(fh, "%s/yreal=%.16g\n", name, gwy_data_field_get_yreal(dfield));
    fprintf(fh, "%s=[\n[", name);
    fflush(fh);
    fwrite(dfield->data, sizeof(gdouble), xres*yres, fh);
    fwrite("]]\n", 1, 3, fh);
    fflush(fh);
}

/**
 * open_temporary_file:
 * @filename: Where the filename is to be stored.
 *
 * Open a temporary file in "wb" mode, return the stream handle.
 *
 * On *nix, it tries to open the file in a safe manner.  On MS systems,
 * it just opens a file.  Who cares...
 *
 * Returns: The filehandle of the open file.
 **/
static FILE*
open_temporary_file(gchar **filename)
{
    FILE *fh;
#ifdef G_OS_WIN32
    gchar buf[9];
    gsize i;

    /* FIXME: this is bogus. like the OS it's needed for. */
    for (i = 0; i < sizeof(buf)-1; i++)
        buf[i] = 'a' + (rand()/283)%26;
    buf[sizeof(buf)-1] = '\0';
    *filename = g_build_filename(g_get_tmp_dir(), buf, NULL);

    fh = fopen(*filename, "wb");
    if (!fh)
        g_warning("Cannot create a temporary file: %s", g_strerror(errno));
#else
    GError *err = NULL;
    int fd;

    fd = g_file_open_tmp("gwydXXXXXXXX", filename, &err);
    if (fd < 0) {
        g_warning("Cannot create a temporary file: %s", err->message);
        g_clear_error(&err);
        return NULL;
    }
    fh = fdopen(fd, "wb");
    g_assert(fh != NULL);
#endif

    return fh;
}

static GwyContainer*
text_dump_import(GwyContainer *old_data, gchar *buffer, gsize size)
{
    gchar *val, *key, *pos, *line;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xreal, yreal;
    gint xres, yres;
    gsize n;

    if (old_data) {
        data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(old_data)));
        gwy_app_clean_up_data(data);
    }
    else
        data = GWY_CONTAINER(gwy_container_new());

    pos = buffer;
    while ((line = next_line(&pos)) && *line) {
        val = strchr(line, '=');
        if (!val || *line != '/') {
            g_warning("Garbage key: %s", line);
            continue;
        }
        if ((gsize)(val - buffer) + 1 > size) {
            g_warning("Unexpected end of file (value expected).");
            goto fail;
        }
        *val = '\0';
        val++;
        if (strcmp(val, "[") != 0) {
            gwy_debug("<%s>=<%s>", line, val);
            if (*val)
                gwy_container_set_string_by_name(data, line, g_strdup(val));
            else
                gwy_container_remove_by_name(data, line);
            continue;
        }

        if (!pos || *pos != '[') {
            g_warning("Unexpected end of file (datafield expected).");
            goto fail;
        }
        pos++;
        if (gwy_container_contains_by_name(data, line))
            dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data,
                                                                     line));
        else
            dfield = NULL;

        /* get datafield parameters from already read values, failing back
         * to values of original data field */
        key = g_strconcat(line, "/xres", NULL);
        if (gwy_container_contains_by_name(data, key))
            xres = atoi(gwy_container_get_string_by_name(data, key));
        else if (dfield)
            xres = gwy_data_field_get_xres(dfield);
        else {
            g_warning("Broken dump doesn't specify data field width.");
            goto fail;
        }
        g_free(key);

        key = g_strconcat(line, "/yres", NULL);
        if (gwy_container_contains_by_name(data, key))
            yres = atoi(gwy_container_get_string_by_name(data, key));
        else if (dfield)
            yres = gwy_data_field_get_yres(dfield);
        else {
            g_warning("Broken dump doesn't specify data field height.");
            goto fail;
        }
        g_free(key);

        key = g_strconcat(line, "/xreal", NULL);
        if (gwy_container_contains_by_name(data, key))
            xreal = g_ascii_strtod(gwy_container_get_string_by_name(data, key),
                                   NULL);
        else if (dfield)
            xreal = gwy_data_field_get_xreal(dfield);
        else {
            g_warning("Broken dump doesn't specify real data field width.");
            xreal = 1;   /* 0 could cause troubles */
        }
        g_free(key);

        key = g_strconcat(line, "/yreal", NULL);
        if (gwy_container_contains_by_name(data, key))
            yreal = g_ascii_strtod(gwy_container_get_string_by_name(data, key),
                                   NULL);
        else if (dfield)
            yreal = gwy_data_field_get_yreal(dfield);
        else {
            g_warning("Broken dump doesn't specify real data field height.");
            yreal = 1;   /* 0 could cause troubles */
        }
        g_free(key);

        if (!(xres > 0 && yres > 0 && xreal > 0 && yreal > 0)) {
            g_warning("Broken dump has nonpositive data field dimensions");
            goto fail;
        }

        n = xres*yres*sizeof(gdouble);
        if ((gsize)(pos - buffer) + n + 3 > size) {
            g_warning("Unexpected end of file (truncated datafield).");
            goto fail;
        }
        dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                                   FALSE));
        memcpy(dfield->data, pos, n);
        pos += n;
        val = next_line(&pos);
        if (strcmp(val, "]]") != 0) {
            g_warning("Missed end of data field.");
            gwy_object_unref(dfield);
            goto fail;
        }
        gwy_container_remove_by_prefix(data, line);
        gwy_container_set_object_by_name(data, line, G_OBJECT(dfield));
    }
    return data;

fail:
    gwy_container_remove_by_prefix(data, NULL);
    return NULL;
}

/**
 * next_line:
 * @buffer: A character buffer containing some text.
 *
 * Extracts a next line from @buffer.
 *
 * @buffer is updated to point after the end of the line and the "\n" 
 * (or "\r\n") is replaced with "\0", if present.
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          The line is not duplicated, the returned pointer points somewhere
 *          to @buffer.
 **/
static gchar*
next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        if (p > *buffer && *(p-1) == '\r')
            *(p-1) = '\0';
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
