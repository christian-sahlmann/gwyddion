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

#include "config.h"
#include <string.h>
#include <errno.h>

#include <glib/gstdio.h>

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwyserializable.h>
#include <libdraw/gwyrgba.h>
#include "settings.h"

static gboolean create_config_dir_real         (const gchar *cfgdir);
static void     gwy_app_settings_set_defaults  (GwyContainer *settings);

static const gchar *magic_header = "Gwyddion Settings 1.0\n";

static GwyContainer *gwy_settings = NULL;

/**
 * gwy_app_settings_get:
 *
 * Gets the Gwyddion settings.
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

/**
 * gwy_app_settings_free:
 *
 * Frees Gwyddion settings.
 *
 * Should not be called only by main application.
 **/
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
 * Use gwy_app_settings_get_settings_filename() to obtain a suitable default
 * filename.
 *
 * Returns: Whether it succeeded.
 **/
gboolean
gwy_app_settings_save(const gchar *filename)
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
    fh = g_fopen(filename, "w");
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
 * gwy_app_settings_load_text:
 * @filename: A filename to read settings from.
 *
 * Loads settings file.
 *
 * Returns: Whether it succeeded.  In either case you can call
 * gwy_app_settings_get() then to obtain either the loaded settings or the
 * old ones (if failed), or an empty #GwyContainer.
 **/
gboolean
gwy_app_settings_load(const gchar *filename)
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
#ifdef G_OS_WIN32
    gwy_strkill(buffer, "\r");
#endif
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
 * gwy_app_settings_create_config_dir:
 *
 * Create gwyddion config directory.
 *
 * Returns: Whether it succeeded (also returns %TRUE if the directory already
 * exists).
 **/
gboolean
gwy_app_settings_create_config_dir(void)
{
    return create_config_dir_real(gwy_get_user_dir());
}

static gboolean
create_config_dir_real(const gchar *cfgdir)
{
    gboolean ok;
    gchar **moddirs;
    gint i, n;

    ok = g_file_test(cfgdir, G_FILE_TEST_IS_DIR);
    moddirs = gwy_app_settings_get_module_dirs();
    for (n = 0; moddirs[n]; n++)
        ;
    n /= 2;
    g_assert(n > 0);
    /* put the toplevel module dir before particula module dirs */
    g_free(moddirs[n-1]);
    moddirs[n-1] = g_path_get_dirname(moddirs[n]);

    if (!ok) {
        gwy_debug("Trying to create user config directory %s", cfgdir);
        ok = !mkdir(cfgdir, 0700);
        if (!ok) {
            g_warning("Cannot create user config directory %s: %s",
                      cfgdir, g_strerror(errno));
        }
    }

    if (ok) {
        for (i = n-1; i < 2*n; i++) {
            if (g_file_test(moddirs[i], G_FILE_TEST_IS_DIR))
                continue;
            gwy_debug("Trying to create user module directory %s", moddirs[i]);
            if (mkdir(moddirs[i], 0700))
                g_warning("Cannot create user module directory %s: %s",
                          moddirs[i], g_strerror(errno));
        }
    }
    g_strfreev(moddirs);

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
    const gchar *q;
    gsize n, i;

    n = G_N_ELEMENTS(module_types);
    module_dirs = g_new(gchar*, 2*(n+1) + 1);

    p = gwy_find_self_dir("modules");
    for (i = 0; i < n; i++)
        module_dirs[i] = g_build_filename(p, module_types[i], NULL);
    module_dirs[i++] = p;

    q = gwy_get_user_dir();
    for (i = 0; i < n; i++)
        module_dirs[n+1 + i] = g_build_filename(q, "modules", module_types[i],
                                                NULL);
    module_dirs[2*n + 1] = g_build_filename(q, "modules", NULL);;

    module_dirs[2*n + 2] = NULL;

    return module_dirs;
}

/**
 * gwy_app_settings_get_settings_filename:
 *
 * Returns a suitable human-readable settings file name.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_settings_filename(void)
{
    return g_build_filename(gwy_get_user_dir(), "settings", NULL);
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
    return g_build_filename(gwy_get_user_dir(), "gwyddion.log", NULL);
}

/**
 * gwy_app_settings_get_recent_file_list_filename:
 *
 * Returns a suitable recent file list file name.
 *
 * Returns: The file name as a newly allocated string.
 **/
gchar*
gwy_app_settings_get_recent_file_list_filename(void)
{
    return g_build_filename(gwy_get_user_dir(), "recent-files", NULL);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
