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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyserializable.h>
#include "settings.h"

static void gwy_app_set_defaults (GwyContainer *settings);

static GwyContainer *settings = NULL;

GwyContainer*
gwy_app_settings_get(void)
{
    if (!settings) {
        g_warning("No settings loaded, creating empty");
        settings = GWY_CONTAINER(gwy_container_new());
    }
    gwy_app_set_defaults(settings);

    return settings;
}

void
gwy_app_settings_free(void)
{
    gwy_object_unref(settings);
}

gboolean
gwy_app_settings_save(const gchar *filename)
{
    GwyContainer *settings;
    gchar *buffer = NULL;
    gsize size = 0;
    FILE *fh;

    gwy_debug("%s: Saving settings to `%s'", __FUNCTION__, filename);
    settings = gwy_app_settings_get();
    g_return_val_if_fail(GWY_IS_CONTAINER(settings), FALSE);
    fh = fopen(filename, "wb");
    if (!fh)
        return FALSE;
    buffer = gwy_serializable_serialize(G_OBJECT(settings), buffer, &size);
    if (!buffer)
        return FALSE;
    fwrite(buffer, 1, size, fh);
    g_free(buffer);
    fclose(fh);

    return TRUE;
}

gboolean
gwy_app_settings_load(const gchar *filename)
{
    GwyContainer *new_settings;
    GError *err = NULL;
    gchar *buffer = NULL;
    gsize size = 0, position = 0;

    gwy_debug("%s: Loading settings from `%s'", __FUNCTION__, filename);
    if (!g_file_get_contents(filename, &buffer, &size, &err)
        || !size || !buffer) {
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
    settings = new_settings;
    gwy_app_set_defaults(settings);

    return TRUE;
}

static void
gwy_app_set_defaults(GwyContainer *settings)
{
    g_return_if_fail(GWY_IS_CONTAINER(settings));

    if (!gwy_container_contains_by_name(settings, "/app/plugindir"))
        gwy_container_set_string_by_name(settings, "/app/plugindir",
                                         g_strdup(GWY_PLUGIN_DIR));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
