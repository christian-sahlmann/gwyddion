/* @(#) $Id$ */

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwydgets.h>
#include <app/settings.h>
#include <app/file.h>

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
static FILE*          text_dump_export           (GwyContainer *data,
                                                  gchar **filename);
static void           dump_export_meta_cb        (gpointer hkey,
                                                  GValue *value,
                                                  FILE *fh);
static void           dump_export_data_field     (GwyDataField *dfield,
                                                  const gchar *name,
                                                  FILE *fh);
static GwyContainer*  text_dump_import           (gchar *buffer);
static GwyRunType     str_to_run_modes           (const gchar *str);
static const char*    run_mode_to_str            (GwyRunType run);
static gchar*         next_line                  (gchar **buffer);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "plugin-proxy",
    "Plug-in proxy is a module capable of querying, registering, and running "
        "external programs (plug-ins) on data.",
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
    GwyRunType run;
}
const run_mode_names[] = {
    { "interactive", GWY_RUN_INTERACTIVE },
    { "noninteractive", GWY_RUN_NONINTERACTIVE },
    { "modal", GWY_RUN_MODAL },
    { "with_defaults", GWY_RUN_WITH_DEFAULTS },
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
        ok = g_spawn_sync(dir, args, NULL, 0, NULL, NULL,
                          &buffer, NULL, &exit_status, &err);
        ok &= !exit_status;
        if (ok)
            plugins = register_plugins(plugins, name, pluginname, buffer);
        else {
            g_warning("Cannot register plug-in %s: %s", filename, err->message);
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
            info->file = g_strdup(file);
            if (gwy_process_func_register(name, &info->func))
                plugins = g_list_prepend(plugins, info);
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
    FILE *fh;
    GList *l;
    gchar *args[] = { NULL, "run", NULL, NULL, NULL };
    gboolean ok;

    g_return_val_if_fail(run & PLUGIN_PROXY_RUN_MODES, FALSE);
    gwy_debug("%s: called as %s with run mode %d", __FUNCTION__, name, run);

    for (l = plugins; l; l = g_list_next(l)) {
        info = (PluginInfo*)l->data;
        if (strcmp(info->func.name, name) == 0)
            break;
    }
    if (!l) {
        g_critical("Don't know anything about plug-in `%s'.", name);
        return FALSE;
    }
    g_return_val_if_fail(run & info->func.run, FALSE);
    /* keep the file open
     * FIXME: who knows what it causes on MS Windows */
    fh = text_dump_export(data, &filename);
    g_return_val_if_fail(fh, FALSE);
    args[0] = info->file;
    args[2] = run_mode_to_str(run);
    args[3] = filename;
    gwy_debug("%s: %s %s %s %s", __FUNCTION__,
              args[0], args[1], args[2], args[3]);
    ok = g_spawn_sync(NULL, args, NULL, 0, NULL, NULL,
                      &buffer, NULL, &exit_status, &err);
    /* FIXME: on MS Windows we can't unlink open files */
    unlink(filename);
    fclose(fh);
    gwy_debug("%s: ok = %d, exit_status = %d, err = %p", __FUNCTION__,
              ok, exit_status, err);
    ok &= !exit_status;
    if (ok) {
        data = text_dump_import(buffer);
        if (data) {
            data_window = gwy_app_data_window_create(data);
            gwy_app_data_window_set_untitled(GWY_DATA_WINDOW(data_window));
        }
        else {
            g_warning("Cannot run plug-in %s: %s",
                      info->file,
                      err ? err->message : "it returned garbage.");
            ok = FALSE;
        }
    }
    g_clear_error(&err);
    g_free(buffer);
    g_free(filename);

    return ok;
}

static FILE*
text_dump_export(GwyContainer *data, gchar **filename)
{
    GwyDataField *dfield;
    GError *err = NULL;
    FILE *fh;
    gint fd;

    fd = g_file_open_tmp(NULL, filename, &err);
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
text_dump_import(gchar *buffer)
{
    return NULL;
}

static GwyRunType
str_to_run_modes(const gchar *str)
{
    gchar **modes;
    GwyRunType run = 0;
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
run_mode_to_str(GwyRunType run)
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
        while (p > q) {
            p--;
            if (!g_ascii_isspace(*p)) {
                p++;
                break;
            }
        }
        *p = '\0';
    }
    else
        *buffer = NULL;

    while (g_ascii_isspace(*q))
        q++;

    return q;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

