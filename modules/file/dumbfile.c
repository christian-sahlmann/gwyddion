/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-gwyddion-dump-spm">
 *   <comment>Gwyddion dumb dump data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="/0/data/"/>
 *   </magic>
 *   <glob pattern="*.dump"/>
 *   <glob pattern="*.DUMP"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Gwyddion dumb dump data
 * .dump
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "/0/data/"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".dump"

static gboolean      module_register (void);
static gint          dumb_detect     (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* dumb_load       (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static GwyContainer* text_dump_import(gchar *buffer,
                                      gsize size,
                                      GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads and exports Gwyddion dumb dump files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("dumbfile",
                           N_("Gwyddion dumb dump files (.dump)"),
                           (GwyFileDetectFunc)&dumb_detect,
                           (GwyFileLoadFunc)&dumb_load,
                           NULL, NULL);

    return TRUE;
}

static gint
dumb_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static GwyContainer*
dumb_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    gchar *buffer = NULL;
    GError *err = NULL;
    gsize size;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Gwyddion dumb dump");
        goto fail;
    }

    container = text_dump_import(buffer, size, error);

fail:
    g_free(buffer);

    return container;
}

static GwyContainer*
text_dump_import(gchar *buffer,
                 gsize size,
                 GError **error)
{
    gchar *val, *key, *pos, *line, *title;
    GwyContainer *data;
    GwyDataField *dfield;
    gdouble xreal, yreal;
    gint xres, yres;
    GwySIUnit *uxy, *uz;
    const guchar *s;
    gdouble *d;
    gsize n;

    data = gwy_container_new();

    pos = buffer;
    while ((line = gwy_str_next_line(&pos)) && *line) {
        val = strchr(line, '=');
        if (!val || *line != '/') {
            g_warning("Garbage key: %s", line);
            continue;
        }
        if ((gsize)(val - buffer) + 1 > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached when value was expected."));
            goto fail;
        }
        *val = '\0';
        val++;
        if (!gwy_strequal(val, "[") || !pos || *pos != '[') {
            gwy_debug("<%s>=<%s>", line, val);
            if (*val)
                gwy_container_set_string_by_name(data, line, g_strdup(val));
            else
                gwy_container_remove_by_name(data, line);
            continue;
        }

        g_assert(pos && *pos == '[');
        pos++;
        dfield = NULL;
        gwy_container_gis_object_by_name(data, line, &dfield);

        /* get datafield parameters from already read values, failing back
         * to values of original data field */
        key = g_strconcat(line, "/xres", NULL);
        if (gwy_container_gis_string_by_name(data, key, &s))
            xres = atoi(s);
        else if (dfield)
            xres = gwy_data_field_get_xres(dfield);
        else {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Missing data field width."));
            goto fail;
        }
        g_free(key);

        key = g_strconcat(line, "/yres", NULL);
        if (gwy_container_gis_string_by_name(data, key, &s))
            yres = atoi(s);
        else if (dfield)
            yres = gwy_data_field_get_yres(dfield);
        else {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Missing data field height."));
            goto fail;
        }
        g_free(key);

        key = g_strconcat(line, "/xreal", NULL);
        if (gwy_container_gis_string_by_name(data, key, &s))
            xreal = g_ascii_strtod(s, NULL);
        else if (dfield)
            xreal = gwy_data_field_get_xreal(dfield);
        else {
            g_warning("Missing real data field width.");
            xreal = 1.0;   /* 0 could cause troubles */
        }
        g_free(key);

        key = g_strconcat(line, "/yreal", NULL);
        if (gwy_container_gis_string_by_name(data, key, &s))
            yreal = g_ascii_strtod(s, NULL);
        else if (dfield)
            yreal = gwy_data_field_get_yreal(dfield);
        else {
            g_warning("Missing real data field height.");
            yreal = 1.0;   /* 0 could cause troubles */
        }
        g_free(key);

        if (!(xres > 0 && yres > 0 && xreal > 0 && yreal > 0)) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Data field dimensions are not positive numbers."));
            goto fail;
        }

        key = g_strconcat(line, "/unit-xy", NULL);
        if (gwy_container_gis_string_by_name(data, key, &s))
            uxy = gwy_si_unit_new((const gchar*)s);
        else if (dfield) {
            uxy = gwy_data_field_get_si_unit_xy(dfield);
            uxy = gwy_si_unit_duplicate(uxy);
        }
        else {
            g_warning("Missing lateral units.");
            uxy = gwy_si_unit_new("m");
        }
        g_free(key);

        key = g_strconcat(line, "/unit-z", NULL);
        if (gwy_container_gis_string_by_name(data, key, &s))
            uz = gwy_si_unit_new((const gchar*)s);
        else if (dfield) {
            uz = gwy_data_field_get_si_unit_z(dfield);
            uz = gwy_si_unit_duplicate(uz);
        }
        else {
            g_warning("Missing value units.");
            uz = gwy_si_unit_new("m");
        }
        g_free(key);

        key = g_strconcat(line, "/title", NULL);
        title = NULL;
        gwy_container_gis_string_by_name(data, key, (const guchar**)&title);
        /* We got the contained string but that would disappear. */
        title = g_strdup(title);
        g_free(key);

        n = xres*yres*sizeof(gdouble);
        if ((gsize)(pos - buffer) + n + 3 > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached inside a data field."));
            goto fail;
        }
        dfield = GWY_DATA_FIELD(gwy_data_field_new(xres, yres, xreal, yreal,
                                                   FALSE));
        gwy_data_field_set_si_unit_xy(dfield, GWY_SI_UNIT(uxy));
        gwy_object_unref(uxy);
        gwy_data_field_set_si_unit_z(dfield, GWY_SI_UNIT(uz));
        gwy_object_unref(uz);
        d = gwy_data_field_get_data(dfield);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        memcpy(d, pos, n);
#else
        gwy_byteswapped_copy(pos, (guint8*)d,
                             sizeof(gdouble), xres*yres, sizeof(gdouble)-1);
#endif
        pos += n;
        val = gwy_str_next_line(&pos);
        if (!gwy_strequal(val, "]]")) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Missing end of data field marker."));
            gwy_object_unref(dfield);
            goto fail;
        }
        gwy_container_remove_by_prefix(data, line);
        gwy_container_set_object_by_name(data, line, dfield);
        g_object_unref(dfield);

        if (title) {
            key = g_strconcat(line, "/title", NULL);
            gwy_container_set_string_by_name(data, key, title);
            g_free(key);
        }
    }
    return data;

fail:
    gwy_container_remove_by_prefix(data, NULL);
    g_object_unref(data);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
