/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef _MSC_VER
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwyserializable.h>
#include <libdraw/gwypalettedef.h>
#include "settings.h"

static gboolean create_config_dir_real         (const gchar *cfgdir);
static void     gwy_app_settings_set_defaults  (GwyContainer *settings);
static G_CONST_RETURN
gchar*   gwy_app_settings_get_user_dir  (void);

static const gchar *magic_header = "Gwyddion Settings 1.0\n";

static GwyContainer *gwy_settings = NULL;

/**
 * gwy_app_settings_get:
 *
 * Gets the gwyddion settings.
 *
 * The settings are a #GwyContainer automatically loaded at program startup
 * and saved ad its exit.  For storing persistent module data you should
 * use "/module/YOUR_MODULE_NAME/" prefix.
 *
 * Returns: The settings as a #GwyContainer.
 **/
GwyContainer*
gwy_app_settings_get(void)
{
    if (!gwy_settings) {
        g_warning("No settings loaded, creating empty");
        gwy_settings = GWY_CONTAINER(gwy_container_new());
        gwy_app_settings_set_defaults(gwy_settings);
    }

    return gwy_settings;
}

void
gwy_app_settings_free(void)
{
    gwy_object_unref(gwy_settings);
}

/**
 * gwy_app_settings_save_bin:
 * @filename: A filename to save the settings to.
 *
 * Saves the settings in old binary dump format.
 *
 * Do NOT use any more.
 *
 * Returns: Whether it succeeded.
 **/
gboolean
gwy_app_settings_save_bin(const gchar *filename)
{
    GwyContainer *settings;
    GByteArray *buffer = NULL;
    FILE *fh;
    gchar *cfgdir;

    cfgdir = g_path_get_dirname(filename);
    if (!create_config_dir_real(cfgdir)) {
        g_free(cfgdir);
        return FALSE;
    }
    g_free(cfgdir);

    gwy_debug("Saving binary settings to `%s'", filename);
    settings = gwy_app_settings_get();
    g_return_val_if_fail(GWY_IS_CONTAINER(settings), FALSE);
    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("Cannot save binary settings to `%s': %s",
                  filename, g_strerror(errno));
        return FALSE;
    }
    buffer = gwy_serializable_serialize(G_OBJECT(settings), NULL);
    if (!buffer)
        return FALSE;
    fwrite(buffer->data, 1, buffer->len, fh);
    g_byte_array_free(buffer, TRUE);
    fclose(fh);

    return TRUE;
}

/**
 * gwy_app_settings_save_text:
 * @filename: A filename to save the settings to.
 *
 * Saves the settings in human-readable text format.
 *
 * Probably useful only in the application.  Use
 * gwy_app_settings_get_settings_filename() to obtain a suitable default
 * filename.
 *
 * Returns: Whether it succeeded.
 *
 * Since: 1.2.
 **/
gboolean
gwy_app_settings_save_text(const gchar *filename)
{
    GwyContainer *settings;
    GPtrArray *pa;
    guint i;
    gboolean ok;
    FILE *fh;
    gchar *cfgdir;

    cfgdir = g_path_get_dirname(filename);
    if (!create_config_dir_real(cfgdir)) {
        g_free(cfgdir);
        return FALSE;
    }
    g_free(cfgdir);

    gwy_debug("Saving text settings to `%s'", filename);
    settings = gwy_app_settings_get();
    g_return_val_if_fail(GWY_IS_CONTAINER(settings), FALSE);
    fh = fopen(filename, "w");
    if (!fh) {
        g_warning("Cannot save text settings to `%s': %s",
                  filename, g_strerror(errno));
        return FALSE;
    }
    if (fputs(magic_header, fh) == EOF) {
        g_warning("Cannot save text settings to `%s': %s",
                  filename, g_strerror(errno));
        fclose(fh);
        return FALSE;
    }

    ok = TRUE;
    pa = gwy_container_serialize_to_text(settings);
    for (i = 0; i < pa->len; i++) {
        if (fputs((gchar*)pa->pdata[i], fh) == EOF) {
            g_warning("Cannot save text settings to `%s': %s",
                      filename, g_strerror(errno));
            while (i < pa->len)
                g_free(pa->pdata[i]);
            ok = FALSE;
            break;
        }
        fputc('\n', fh);
        g_free(pa->pdata[i]);
    }
    g_ptr_array_free(pa, TRUE);
    fclose(fh);

    return ok;
}

/**
 * gwy_app_settings_save:
 * @filename: A filename to save the settings to.
 *
 * Saves the settings.
 *
 * Probably useful only in the application.
 *
 * Since 1.2 it saves settings in human-readable format.  Use
 * gwy_app_settings_save_bin() if you specifically require the old binary
 * dump format.
 *
 * Returns: Whether it succeeded.
 **/
gboolean
gwy_app_settings_save(const gchar *filename)
{
    return gwy_app_settings_save_text(filename);
}

/**
 * gwy_app_settings_load_bin:
 * @filename: A filename to read the config from.
 *
 * Loads the old binary-dump config.
 *
 * Probably useful only in the application, and even there only for
 * compatibility. Do NOT use binary configs.
 *
 * Returns: Whether it succeeded.  In either case you can call
 * gwy_app_settings_get() then to obtain either the loaded settings or the
 * old ones (if failed), or an empty #GwyContainer.
 *
 * Since: 1.2.
 **/
gboolean
gwy_app_settings_load_bin(const gchar *filename)
{
    GwyContainer *new_settings;
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0, position = 0;

    gwy_debug("Loading settings from `%s'", filename);
    if (!g_file_get_contents(filename, &buffer, &size, &err)
        || !size || !buffer) {
        g_warning("Cannot load settings from `%s': %s",
                  filename, err ? err->message : "unknown error");
        g_clear_error(&err);
        return FALSE;
    }
    new_settings = GWY_CONTAINER(gwy_serializable_deserialize(buffer, size,
                                                              &position));
    g_free(buffer);
    if (!GWY_IS_CONTAINER(new_settings)) {
        g_object_unref(new_settings);
        return FALSE;
    }
    gwy_app_settings_free();
    gwy_settings = new_settings;
    gwy_app_settings_set_defaults(gwy_settings);

    return TRUE;
}

/**
 * gwy_app_settings_load_text:
 * @filename: A filename to read the config from.
 *
 * Loads the human-readable settings file.
 *
 * Probably useful only in the application.
 *
 * Returns: Whether it succeeded.  In either case you can call
 * gwy_app_settings_get() then to obtain either the loaded settings or the
 * old ones (if failed), or an empty #GwyContainer.
 *
 * Since: 1.2.
 **/
gboolean
gwy_app_settings_load_text(const gchar *filename)
{
    GwyContainer *new_settings;
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0;

    gwy_debug("Loading settings from `%s'", filename);
    if (!g_file_get_contents(filename, &buffer, &size, &err)
        || !size || !buffer) {
        g_warning("Cannot load settings from `%s': %s",
                  filename, err ? err->message : "unknown error");
        g_clear_error(&err);
        g_free(buffer);
        return FALSE;
    }
    if (!g_str_has_prefix(buffer, magic_header)) {
        g_warning("Bad magic header of settings file");
        g_free(buffer);
        return FALSE;
    }
    new_settings = gwy_container_deserialize_from_text(buffer
                                                       + strlen(magic_header));
    g_free(buffer);
    if (!GWY_IS_CONTAINER(new_settings)) {
        g_object_unref(new_settings);
        return FALSE;
    }
    gwy_app_settings_free();
    gwy_settings = new_settings;
    gwy_app_settings_set_defaults(gwy_settings);

    return TRUE;
}

/**
 * gwy_app_settings_load:
 * @filename: A filename to read the config from.
 *
 * Loads the old binary-dump config.
 *
 * Probably useful only in the application.
 *
 * Since 1.2 no longer tries creates to create the parent directory
 * when it doesn't exist, and loads the human-readable (text) format.
 *
 * Returns: Whether it succeeded.  In either case you can call
 * gwy_app_settings_get() then to obtain either the loaded settings or the
 * old ones (if failed), or an empty #GwyContainer.
 **/
gboolean
gwy_app_settings_load(const gchar *filename)
{
    return gwy_app_settings_load_text(filename);
}

/**
 * gwy_app_settings_create_config_dir:
 *
 * Create gwyddion config directory.
 *
 * Returns: Whether it succeeded (also returns %TRUE if the directory already
 * exists).
 *
 * Since: 1.2.
 **/
gboolean
gwy_app_settings_create_config_dir(void)
{
    return create_config_dir_real(gwy_app_settings_get_user_dir());
}

static gboolean
create_config_dir_real(const gchar *cfgdir)
{
    gboolean ok;

    if (g_file_test(cfgdir, G_FILE_TEST_IS_DIR))
        return TRUE;

    gwy_debug("Trying to create config directory %s", cfgdir);
    ok = !mkdir(cfgdir, 0700);
    if (!ok) {
        g_warning("Cannot create config directory %s: %s",
                  cfgdir, g_strerror(errno));
    }

    return ok;
}

static void
gwy_app_settings_set_defaults(GwyContainer *settings)
{
    static const GwyRGBA default_mask_color = { 1.0, 0.0, 0.0, 0.5 };

    if (!gwy_container_contains_by_name(settings, "/mask/alpha")) {
        gwy_container_set_double_by_name(settings, "/mask/red",
                                         default_mask_color.r);
        gwy_container_set_double_by_name(settings, "/mask/green",
                                         default_mask_color.g);
        gwy_container_set_double_by_name(settings, "/mask/blue",
                                         default_mask_color.b);
        gwy_container_set_double_by_name(settings, "/mask/alpha",
                                         default_mask_color.a);
    }
}

/**
 * gwy_app_settings_get_module_dirs:
 *
 * Returns a list of directories to search modules in.
 *
 * Returns: The list of module directories as a newly allocated array of
 *          newly allocated strings, to be freed with g_str_freev() when
 *          not longer needed.
 **/
gchar**
gwy_app_settings_get_module_dirs(void)
{
    const gchar *module_types[] = {
        "layer", "file", "process", "graph", "tool"
    };
    gchar **module_dirs;
    gchar *p;
    gsize n, i;

    n = G_N_ELEMENTS(module_types);
    module_dirs = g_new(gchar*, 2*(n+1) + 1);

    p = gwy_find_self_dir("modules");
    for (i = 0; i < n; i++)
        module_dirs[i] = g_build_filename(p, module_types[i], NULL);
    module_dirs[i++] = p;

    p = gwy_app_settings_get_user_dir();
    for (i = 0; i < n; i++)
        module_dirs[n+1 + i] = g_build_filename(p, "modules", module_types[i],
                                                NULL);
    module_dirs[2*n + 1] = g_build_filename(p, "modules", NULL);;

    module_dirs[2*n + 2] = NULL;

    return module_dirs;
}

/**
 * gwy_app_settings_get_config_filename:
 *
 * Returns a suitable (binary) configuration file name.
 *
 * Note binary config is deprecated now.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_config_filename(void)
{
    return g_build_filename(gwy_app_settings_get_user_dir(), "gwydrc", NULL);
}

/**
 * gwy_app_settings_get_settings_filename:
 *
 * Returns a suitable human-readable settings file name.
 *
 * Returns: The file name as a newly allocated string.
 *
 * Since: 1.2.
 **/
gchar*
gwy_app_settings_get_settings_filename(void)
{
    return g_build_filename(gwy_app_settings_get_user_dir(), "settings", NULL);
}

/**
 * gwy_app_settings_get_log_filename:
 *
 * Returns a suitable log file name.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_log_filename(void)
{
    return g_build_filename(gwy_app_settings_get_user_dir(), "gwyddion.log",
                            NULL);
}

/**
 * gwy_app_settings_get_user_dir:
 *
 * Return directory where Gwyddion user settings and data should be stored.
 *
 * On silly platforms or silly occasions, silly locations can be returned
 * as fallback.
 *
 * Returns: The directory as a string that should not be freed.
 **/
static G_CONST_RETURN gchar*
gwy_app_settings_get_user_dir(void)
{
    const gchar *gwydir =
#ifdef G_OS_WIN32
        "gwyddion";
#else
        ".gwyddion";
#endif
    const gchar *homedir;
    static gchar *gwyhomedir = NULL;

    if (gwyhomedir)
        return gwyhomedir;

    homedir = g_get_home_dir();
    if (!homedir)
        homedir = g_get_tmp_dir();
#ifdef G_OS_WIN32
    if (!homedir)
        homedir = "C:\\Windows";  /* XXX :-))) */
#endif

    gwyhomedir = g_build_filename(homedir, gwydir, NULL);
    return gwyhomedir;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
