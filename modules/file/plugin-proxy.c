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
static FILE*          text_dump_export           (GwyContainer *data,
                                                  gchar **filename);
static void           dump_export_meta_cb        (gpointer hkey,
                                                  GValue *value,
                                                  FILE *fh);
static void           dump_export_data_field     (GwyDataField *dfield,
                                                  const gchar *name,
                                                  FILE *fh);
static GwyContainer*  text_dump_import           (GwyContainer *old_data,
                                                  gchar *buffer,
                                                  gsize size);
static PluginInfo*    find_plugin                (const gchar *name,
                                                  GwyLoadSave run);
static glong          pattern_specificity        (const gchar *pattern);
static GwyLoadSave    str_to_run_modes           (const gchar *str);
static const char*    run_mode_to_str            (GwyLoadSave run);
static gchar*         next_line                  (gchar **buffer);

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
            info->func.load = plugin_proxy_load;
            info->func.save = plugin_proxy_save;
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
    unlink(filename);
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
    gwy_debug("%s: called as %s with file `%s'", __FUNCTION__, name, filename);
    return 0;
}

static FILE*
text_dump_export(GwyContainer *data, gchar **filename)
{
    GwyDataField *dfield;
    GError *err = NULL;
    FILE *fh;
    gint fd;

    fd = g_file_open_tmp("gwydXXXXXXXX", filename, &err);
    if (fd < 0) {
        g_warning("Cannot create a temporary file: %s", err->message);
        return NULL;
    }
    fh = fdopen(fd, "wb");
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

static GwyContainer*
text_dump_import(GwyContainer *old_data, gchar *buffer, gsize size)
{
    gchar *val, *key, *pos, *line;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xreal, yreal;
    gint xres, yres;
    gsize n;

    data = GWY_CONTAINER(gwy_serializable_duplicate(G_OBJECT(old_data)));
    gwy_app_clean_up_data(data);
    pos = buffer;
    while ((line = next_line(&pos)) && *line) {
        val = strchr(line, '=');
        if (!val || *line != '/') {
            g_warning("Garbage key: %s", line);
            continue;
        }
        if ((gsize)(val - buffer) + 1 > size) {
            g_critical("Unexpected end of file.");
            goto fail;
        }
        *val = '\0';
        val++;
        if (strcmp(val, "[") != 0) {
            gwy_debug("%s: <%s>=<%s>", __FUNCTION__,
                      line, val);
            if (*val)
                gwy_container_set_string_by_name(data, line, g_strdup(val));
            else
                gwy_container_remove_by_name(data, line);
            continue;
        }

        if (!pos || *pos != '[') {
            g_critical("Unexpected end of file.");
            goto fail;
        }
        pos++;
        dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, line));

        /* get datafield parameters from already read values, failing back
         * to values of original data field */
        key = g_strconcat(line, "/xres", NULL);
        if (gwy_container_contains_by_name(data, key))
            xres = atoi(gwy_container_get_string_by_name(data, key));
        else
            xres = gwy_data_field_get_xres(dfield);
        g_free(key);

        key = g_strconcat(line, "/yres", NULL);
        if (gwy_container_contains_by_name(data, key))
            yres = atoi(gwy_container_get_string_by_name(data, key));
        else
            yres = gwy_data_field_get_yres(dfield);
        g_free(key);

        key = g_strconcat(line, "/xreal", NULL);
        if (gwy_container_contains_by_name(data, key))
            xreal = g_ascii_strtod(gwy_container_get_string_by_name(data, key),
                                   NULL);
        else
            xreal = gwy_data_field_get_xreal(dfield);
        g_free(key);

        key = g_strconcat(line, "/yreal", NULL);
        if (gwy_container_contains_by_name(data, key))
            yreal = g_ascii_strtod(gwy_container_get_string_by_name(data, key),
                                   NULL);
        else
            yreal = gwy_data_field_get_yreal(dfield);
        g_free(key);

        dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                                   FALSE));

        n = xres*yres*sizeof(gdouble);
        if ((gsize)(pos - buffer) + n + 3 > size) {
            g_critical("Unexpected end of file.");
            goto fail;
        }
        memcpy(dfield->data, pos, n);
        pos += n;
        val = next_line(&pos);
        if (strcmp(val, "]]") != 0) {
            g_critical("Missed end of data field.");
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
    return strlen(pattern);
}

static GwyLoadSave
str_to_run_modes(const gchar *str)
{
    gchar **modes;
    GwyLoadSave run = 0;
    gsize i, j;

    modes = g_strsplit(str, " ", 0);
    for (i = 0; modes[i]; i++) {
        for (j = 0; j < G_N_ELEMENTS(run_mode_names); j++) {
            if (strcmp(modes[i], run_mode_names[j].str) == 0) {
                run |= run_mode_names[j].run;
                break;
            }
        }
    }
    g_strfreev(modes);

    return run;
}

static const char*
run_mode_to_str(GwyLoadSave run)
{
    gsize j;

    for (j = 0; j < G_N_ELEMENTS(run_mode_names); j++) {
        if (run & run_mode_names[j].run)
            return run_mode_names[j].str;
    }

    g_assert_not_reached();
    return "";
}

static gchar*
next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

