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

#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwyserializable.h>
#include "settings.h"

static GwyContainer *gwy_settings = NULL;

/**
 * gwy_app_settings_get:
 *
 * Gets the gwyddion settings.
 *
 * The settings are a #GwyContainer automatically loaded at program startup
 * and saved ad its exit.  For storing persistent module data you should
 * use "/mod/YOUR_MODULE_NAME/" prefix.
 *
 * Returns: The settings as a #GwyContainer.
 **/
GwyContainer*
gwy_app_settings_get(void)
{
    if (!gwy_settings) {
        g_warning("No settings loaded, creating empty");
        gwy_settings = GWY_CONTAINER(gwy_container_new());
    }

    return gwy_settings;
}

void
gwy_app_settings_free(void)
{
    gwy_object_unref(gwy_settings);
}

/**
 * gwy_app_settings_save:
 * @filename: A filename to save the settings to.
 *
 * Saves the settings.
 *
 * Probably useful only in the application.
 *
 * Returns: Whether it succeeded.
 **/
gboolean
gwy_app_settings_save(const gchar *filename)
{
    GwyContainer *settings;
    gchar *buffer = NULL;
    gsize size = 0;
    FILE *fh;

    gwy_debug("Saving settings to `%s'", filename);
    settings = gwy_app_settings_get();
    g_return_val_if_fail(GWY_IS_CONTAINER(settings), FALSE);
    fh = fopen(filename, "wb");
    if (!fh) {
        g_warning("Cannot save settings to `%s': %s",
                  filename, g_strerror(errno));
        return FALSE;
    }
    buffer = gwy_serializable_serialize(G_OBJECT(settings), buffer, &size);
    if (!buffer)
        return FALSE;
    fwrite(buffer, 1, size, fh);
    g_free(buffer);
    fclose(fh);

    return TRUE;
}

/**
 * gwy_app_settings_load:
 * @filename: A filename to read the settings from.
 *
 * Loads the settings.
 *
 * Probably useful only in the application.
 *
 * Returns: Whether it succeeded.  In any case you can call
 * gwy_app_settings_get() then to obtain either the loaded settings or the
 * old ones (if failed), or an empty #GwyContainer.
 **/
gboolean
gwy_app_settings_load(const gchar *filename)
{
    GwyContainer *new_settings;
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0, position = 0;
    gchar *cfgdir;
    gint ok;

    cfgdir = g_path_get_dirname(filename);
    if (!g_file_test(cfgdir, G_FILE_TEST_IS_DIR)) {
        gwy_debug("Trying to create config directory %s", cfgdir);
        ok = !mkdir(cfgdir, 0700);
        if (!ok) {
            g_warning("Cannot create config directory %s: %s",
                      cfgdir, g_strerror(errno));
        }
        g_free(cfgdir);
        return FALSE;
    }
    g_free(cfgdir);

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

    return TRUE;
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
    const gchar *module_types[] = { "file", "process", "graph", "tool" };
    gchar **module_dirs;
    gchar *p;
    gsize i;

    module_dirs = g_new(gchar*, G_N_ELEMENTS(module_types)+2);
#ifdef G_OS_WIN32
    p = gwy_find_self_dir("modules");
#else
    p = g_strdup(GWY_MODULE_DIR);
#endif
    for (i = 0; i < G_N_ELEMENTS(module_types); i++)
        module_dirs[i] = g_build_filename(p, module_types[i], NULL);
    module_dirs[i++] = p;
    module_dirs[i] = NULL;

    return module_dirs;
}

/**
 * gwy_app_settings_get_config_filename:
 *
 * Returns a suitable configuration file name.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_config_filename(void)
{
    const gchar *gwydir =
#ifdef G_OS_WIN32
        "gwyddion";
#else
        ".gwyddion";
#endif
    const gchar *homedir;
    gchar *config_file;

    homedir = g_get_home_dir();
#ifdef G_OS_WIN32
    if (!homedir)
        homedir = g_get_tmp_dir();
    if (!homedir)
        homedir = "C:\\Windows";  /* XXX :-))) */
#endif
    config_file = g_build_filename(homedir, gwydir, "gwydrc", NULL);

    return config_file;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
