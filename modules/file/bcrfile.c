/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klbcrtek.
 *  E-mail: yeti@gwyddion.net, klbcrtek@gwyddion.net.
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

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

#define HEADER_SIZE 2048

#define MAGIC1 "fileformat = bcrstm\n"
#define MAGIC_SIZE1 (sizeof(MAGIC1) - 1)
#define MAGIC2 "fileformat = bcrf\n"
#define MAGIC_SIZE2 (sizeof(MAGIC2) - 1)
#define MAGIC_SIZE (MAX(MAGIC_SIZE1, MAGIC_SIZE2))

/* values are bytes per pixel */
typedef enum {
    BCR_FILE_INT16 = 2,
    BCR_FILE_FLOAT = 4
} BCRFileType;

static gboolean      module_register       (const gchar *name);
static gint          bcrfile_detect        (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* bcrfile_load          (const gchar *filename);
static GwyDataField* file_load_real        (const guchar *buffer,
                                            gsize size,
                                            GHashTable *meta);
static GwyDataField* read_data_field       (const guchar *buffer,
                                            gint xres,
                                            gint yres,
                                            BCRFileType type,
                                            gboolean little_endian);
static void          load_metadata         (gchar *buffer,
                                            GHashTable *meta);
static void          store_metadata        (GHashTable *meta,
                                            GwyContainer *container);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Image Metrology BCR data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo bcrfile_func_info = {
        "bcrfile",
        N_("BCR files (.bcr, .bcrf)"),
        (GwyFileDetectFunc)&bcrfile_detect,
        (GwyFileLoadFunc)&bcrfile_load,
        NULL
    };

    gwy_file_func_register(name, &bcrfile_func_info);

    return TRUE;
}

static gint
bcrfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return (g_str_has_suffix(fileinfo->name_lowercase, ".bcr")
                || g_str_has_suffix(fileinfo->name_lowercase, ".bcrf"))
                ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && (!memcmp(fileinfo->buffer, MAGIC1, MAGIC_SIZE1)
            || !memcmp(fileinfo->buffer, MAGIC2, MAGIC_SIZE2)))
        score = 100;

    return score;
}

static GwyContainer*
bcrfile_load(const gchar *filename)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GHashTable *meta = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        g_warning("File %s is not a BCR file", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    meta = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    dfield = file_load_real(buffer, size, meta);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        store_metadata(meta, container);
    }
    g_hash_table_destroy(meta);

    return container;
}

static GwyDataField*
file_load_real(const guchar *buffer,
               gsize size,
               GHashTable *meta)
{
    GwyDataField *dfield;
    gboolean intelmode = TRUE;
    BCRFileType type;
    gdouble q;
    gint xres, yres;
    guchar *s;

    s = g_memdup(buffer, HEADER_SIZE);
    s[HEADER_SIZE-1] = '\0';
    load_metadata(s, meta);
    g_free(s);

    if (!(s = g_hash_table_lookup(meta, "fileformat"))) {
        g_warning("File is not a BCR file");
        return NULL;
    }

    if (!strcmp(s, "bcrstm"))
        type = BCR_FILE_INT16;
    else if (!strcmp(s, "bcrf"))
        type = BCR_FILE_FLOAT;
    else {
        g_warning("Cannot understand file type header `%s'", s);
        return NULL;
    }
    gwy_debug("File type: %u", type);

    if (!(s = g_hash_table_lookup(meta, "xpixels"))) {
        g_warning("No xpixels (x resolution) info");
        return NULL;
    }
    xres = atol(s);

    if (!(s = g_hash_table_lookup(meta, "ypixels"))) {
        g_warning("No ypixels (y resolution) info");
        return NULL;
    }
    yres = atol(s);

    if ((s = g_hash_table_lookup(meta, "intelmode")))
        intelmode = !!atol(s);

    if (size < HEADER_SIZE + xres*yres*type) {
        g_warning("Expected data size %u, but it's %u",
                  xres*yres*type, (guint)(size - HEADER_SIZE));
        return NULL;
    }

    dfield = read_data_field(buffer + HEADER_SIZE, xres, yres,
                             type, intelmode);

    if ((s = g_hash_table_lookup(meta, "xlength"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_set_xreal(dfield, 1e-9*q);

    if ((s = g_hash_table_lookup(meta, "ylength"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_set_yreal(dfield, 1e-9*q);

    if (type == BCR_FILE_INT16
        && (s = g_hash_table_lookup(meta, "bit2nm"))
        && (q = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_multiply(dfield, 1e-9*q);

    return dfield;
}

static void
store_metadata(GHashTable *meta,
               GwyContainer *container)
{
    const struct {
        const gchar *id;
        const gchar *unit;
        const gchar *key;
    }
    metakeys[] = {
        { "scanspeed",   "nm/s",    "Scan speed"        },
        { "xoffset",     "nm",      "X offset"          },
        { "yoffset",     "nm",      "Y offset"          },
        { "bias",        "V",       "Bias voltage"      },
        { "current",     "nA",      "Tunneling current" },
        { "starttime",   NULL,      "Scan time"         },
        /* FIXME: I've seen other stuff, but don't know interpretation */
    };
    gchar *value;
    GString *key;
    guint i;

    key = g_string_new("");
    for (i = 0; i < G_N_ELEMENTS(metakeys); i++) {
        if (!(value = g_hash_table_lookup(meta, metakeys[i].id)))
            continue;

        g_string_printf(key, "/meta/%s", metakeys[i].key);
        if (metakeys[i].unit)
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup_printf("%s %s",
                                                             value,
                                                             metakeys[i].unit));
        else
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup(value));
    }
    g_string_free(key, TRUE);
}

static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres,
                BCRFileType type,
                gboolean little_endian)
{
    const guint16 *p = (const guint16*)buffer;
    GwyDataField *dfield;
    gdouble *data;
    guint i;

    dfield = gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE);
    data = gwy_data_field_get_data(dfield);
    switch (type) {
        case BCR_FILE_INT16:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_LE(p[i]);
        }
        else {
            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_BE(p[i]);
        }
        break;

        case BCR_FILE_FLOAT:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT(&buffer);
        }
        else {
            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT_BE(&buffer);
        }
        gwy_data_field_multiply(dfield, 1e-9);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return dfield;
}

static void
load_metadata(gchar *buffer,
              GHashTable *meta)
{
    gchar *line, *p;
    gchar *key, *value;

    while ((line = gwy_str_next_line(&buffer))) {
        if (line[0] == '%' || line[0] == '#')
            continue;

        p = strchr(line, '=');
        if (!p || p == line || !p[1])
            continue;

        key = g_strstrip(g_strndup(line, p-line));
        if (!key[0]) {
            g_free(key);
            continue;
        }
        value = g_strstrip(g_strdup(p+1));
        if (!value[0]) {
            g_free(key);
            g_free(value);
            continue;
        }

        g_hash_table_insert(meta, key, value);
        gwy_debug("<%s> = <%s>", key, value);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

