/* @(#) $Id$ */

#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyserializable.h>
#include "settings.h"

static GwyContainer *settings = NULL;

GwyContainer*
gwy_app_settings_get(void)
{
    if (!settings) {
        g_warning("No settings loaded, creating empty");
        settings = GWY_CONTAINER(gwy_container_new());
    }
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
    g_return_val_if_fail(settings, FALSE);
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
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
