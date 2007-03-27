/*
 *  $Id: microprof.c 6038 2006-05-23 09:16:26Z yeti-dn $
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>

#include "err.h"

#define MAGIC      "HeaderLines="
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".txt"

enum {
    MICROPROF_MIN_TEXT_SIZE = 80
};

static gboolean      module_register      (void);
static gint          microprof_detect     (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* microprof_load       (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static GHashTable*   microprof_read_header(gchar *buffer,
                                           GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports MicroProf FRT profilometer data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("microprof",
                           N_("MicroProf FRT files (.txt)"),
                           (GwyFileDetectFunc)&microprof_detect,
                           (GwyFileLoadFunc)&microprof_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
microprof_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    GHashTable *meta;
    gchar *buffer;
    gsize size;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    size = fileinfo->buffer_len;
    if (size < MICROPROF_MIN_TEXT_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    buffer = g_memdup(fileinfo->head, size);
    if ((meta = microprof_read_header(buffer, NULL))
        && g_hash_table_lookup(meta, "XSize")
        && g_hash_table_lookup(meta, "YSize")
        && g_hash_table_lookup(meta, "XRange")
        && g_hash_table_lookup(meta, "YRange")
        && g_hash_table_lookup(meta, "ZScale"))
        score = 90;

    g_free(buffer);
    if (meta)
        g_hash_table_destroy(meta);

    return score;
}

static GwyContainer*
microprof_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    GwyContainer *container = NULL;
    guchar *p, *buffer = NULL;
    GHashTable *meta;
    GwySIUnit *siunit;
    gchar *header, *s, *prev;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gdouble xreal, yreal, zscale, v;
    gint hlines, xres, yres, i, j;
    gdouble *d;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MICROPROF_MIN_TEXT_SIZE
        || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "MicroProf");
        gwy_file_abandon_contents(buffer, size, NULL);
        return 0;
    }

    hlines = atoi(buffer + MAGIC_SIZE);
    if (hlines < 7) {
        err_FILE_TYPE(error, "MicroProf");
        gwy_file_abandon_contents(buffer, size, NULL);
    }

    /* Skip specified number of lines */
    for (p = buffer, i = 0; i < hlines; i++) {
        while (*p != '\n' && (gsize)(p - buffer) < size)
            p++;
        if ((gsize)(p - buffer) == size) {
            err_FILE_TYPE(error, "MicroProf");
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }
        /* Now skip the \n */
        p++;
    }

    header = g_memdup(buffer, p - buffer + 1);
    header[p - buffer] = '\0';

    if (!(meta = microprof_read_header(header, error))) {
        g_free(header);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    g_free(header);

    if (!(s = g_hash_table_lookup(meta, "XSize"))
        || !((xres = atoi(s)) > 0)) {
        err_INVALID(error, "XSize");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (!(s = g_hash_table_lookup(meta, "YSize"))
        || !((yres = atoi(s)) > 0)) {
        err_INVALID(error, "YSize");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (!(s = g_hash_table_lookup(meta, "XRange"))
        || !((xreal = g_ascii_strtod(s, NULL)) > 0.0)) {
        err_INVALID(error, "YRange");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (!(s = g_hash_table_lookup(meta, "YRange"))
        || !((yreal = g_ascii_strtod(s, NULL)) > 0.0)) {
        err_INVALID(error, "YRange");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (!(s = g_hash_table_lookup(meta, "ZScale"))
        || !((zscale = g_ascii_strtod(s, NULL)) > 0.0)) {
        err_INVALID(error, "ZScale");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    d = gwy_data_field_get_data(dfield);
    s = (gchar*)p;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            prev = s;
            /* Skip x */
            v = strtol(s, &s, 10);
            if (v != j)
                g_warning("Column number mismatch");
            /* Skip y */
            v = strtol(s, &s, 10);
            if (v != i)
                g_warning("Row number mismatch");
            /* Read value */
            d[(yres-1 - i)*xres + j] = strtol(s, &s, 10)*zscale;

            /* Check whether we moved in the file */
            if (s == prev) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("File contains less than XSize*YSize data "
                              "points."));
                gwy_file_abandon_contents(buffer, size, NULL);
                g_hash_table_destroy(meta);
                return NULL;
            }
        }
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));

    g_hash_table_destroy(meta);

    return container;
}

/* NB: Buffer must be writable and nul-terminated */
static GHashTable*
microprof_read_header(gchar *buffer,
                      GError **error)
{
    GHashTable *hash;
    gchar *line, *p;

    line = buffer;
    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    while ((line = gwy_str_next_line(&buffer))) {
        g_strstrip(line);
        /* Stop scanning at the first empty line */
        if (!*line)
            return hash;
        gwy_debug("<%s>", line);
        p = strchr(line, '=');
        if (!p) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Malformed header line (missing =)."));
            goto fail;
        }
        *p = '\0';
        p++;
        g_strstrip(line);
        g_strstrip(p);
        gwy_debug("<%s> <%s>", line, p);
        g_hash_table_insert(hash, g_strdup(line), g_strdup(p));
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

