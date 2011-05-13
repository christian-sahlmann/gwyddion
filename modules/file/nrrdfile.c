/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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
 * <mime-type type="application/x-nearly-raw-raster-data">
 *   <comment>Nearly raw raster data (NRRD)</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="NRRD000"/>
 *   </magic>
 *   <glob pattern="*.nrrd"/>
 *   <glob pattern="*.NRRD"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nearly raw raster data (NRRD)
 * .nrrd
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "NRRD000"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".nrrd"

typedef enum {
    NRRD_TYPE_UNKNOWN = -1,
    NRRD_TYPE_SINT8,
    NRRD_TYPE_UINT8,
    NRRD_TYPE_SINT16,
    NRRD_TYPE_UINT16,
    NRRD_TYPE_SINT32,
    NRRD_TYPE_UINT32,
    NRRD_TYPE_SINT64,
    NRRD_TYPE_UINT64,
    NRRD_TYPE_FLOAT,
    NRRD_TYPE_DOUBLE,
    NRRD_TYPE_BLOCK,
} NRRDDataType;

typedef enum {
    NRRD_ENCODING_UNKNOWN = -1,
    NRRD_ENCODING_RAW,
    NRRD_ENCODING_TEXT,
    NRRD_ENCODING_HEX,
    NRRD_ENCODING_GZIP,
    NRRD_ENCODING_BZIP2,
} NRRDEncoding;

static gboolean      module_register(void);
static gint          nrrdfile_detect(const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* nrrdfile_load  (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static NRRDDataType  parse_data_type(const gchar *value);
static NRRDEncoding  parse_encoding (const gchar *value);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports and exports nearly raw raster data (NRRD) files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nrrdfile",
                           N_("Nearly raw raster data (NRRD) files (.nrrd)"),
                           (GwyFileDetectFunc)&nrrdfile_detect,
                           (GwyFileLoadFunc)&nrrdfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nrrdfile_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    // Weed out files that do not pretend to be Windows bitmaps quickly.
    if (fileinfo->buffer_len >= MAGIC_SIZE + 3
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0
        && g_ascii_isdigit(fileinfo->head[MAGIC_SIZE])
        && (fileinfo->head[MAGIC_SIZE+1] == '\n'
            || (fileinfo->head[MAGIC_SIZE+1] == '\r'
                && fileinfo->head[MAGIC_SIZE+2] == '\n')))
        return 100;

    return 0;
}

static void
ascii_tolower_inplace(gchar *s)
{
    while (*s) {
        *s = g_ascii_tolower(*s);
        s++;
    }
}

static GwyContainer*
nrrdfile_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *meta, *container = NULL;
    GHashTable *hash, *fields = NULL, *keyvalue = NULL;
    guchar *buffer = NULL, *header_end;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint xres, yres, header_size, i, j;
    GwySIUnit *unit = NULL;
    gchar *value, *vkeyvalue, *vfield, *p, *line, *header = NULL;
    gdouble *data;
    guint version;
    gboolean unix_eol;
    NRRDDataType data_type;
    NRRDEncoding encoding;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAGIC_SIZE + 3) {
        err_TOO_SHORT(error);
        goto fail;
    }
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0
        || !g_ascii_isdigit(buffer[MAGIC_SIZE])
        || (buffer[MAGIC_SIZE+1] != '\n'
            && (buffer[MAGIC_SIZE+1] != '\r'
                || buffer[MAGIC_SIZE+2] != '\n'))) {
        err_FILE_TYPE(error, "NRRD");
        goto fail;
    }

    version = buffer[MAGIC_SIZE] - '0';
    unix_eol = (buffer[MAGIC_SIZE+1] == '\n');
    header_end = gwy_memmem(buffer, size,
                     unix_eol ? "\n\n" : "\r\n\r\n",
                     unix_eol ? 2 : 4);
    if (!header_end) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated"));
        goto fail;
    }

    header_size = header_end - buffer - (MAGIC_SIZE + 2 + !unix_eol);
    header = g_new(gchar, header_size + 1);
    memcpy(header, buffer + MAGIC_SIZE + 2 + !unix_eol, header_size);
    header[header_size] = '\0';

    fields = g_hash_table_new(g_str_hash, g_str_equal);
    keyvalue = g_hash_table_new(g_str_hash, g_str_equal);
    p = header;
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        g_strstrip(line);
        if (line[0] == '#')
            continue;

        /* Fields and key-values are almost the same for us.  But do not put
         * them to one hash table as we do not want key-values to override
         * fields. */
        vfield = strstr(line, ": ");
        vkeyvalue = strstr(line, ":=");
        if (vfield) {
            hash = (vkeyvalue && vkeyvalue < vfield) ? keyvalue : fields;
            value = (vkeyvalue && vkeyvalue < vfield) ? vkeyvalue : vfield;
        }
        else {
            hash = vkeyvalue ? keyvalue : NULL;
            value = vkeyvalue ? vkeyvalue : NULL;
        }
        if (!hash) {
            g_warning("Neither field nor key-value separator found on line: %s",
                      line);
            continue;
        }

        *value = '\0';
        value += 2;
        g_strchomp(line);
        g_strchug(value);
        if (hash == fields)
            ascii_tolower_inplace(line);
        gwy_debug("<%s> = <%s> (%s)", line, value, hash == fields ? "F" : "KV");
        g_hash_table_insert(hash, line, value);
    }

    if (!require_keys(fields, error,
                      "dimension", "encoding", "sizes", "type", NULL))
        goto fail;

    data_type = parse_data_type(g_hash_table_lookup(fields, "type"));
    encoding = parse_encoding(g_hash_table_lookup(fields, "encoding"));
    gwy_debug("data_type: %u, encoding: %u", data_type, encoding);

    err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    gwy_object_unref(unit);
    g_free(header);
    if (fields)
        g_hash_table_destroy(fields);
    if (keyvalue)
        g_hash_table_destroy(keyvalue);

    return container;
}

static NRRDDataType
parse_data_type(const gchar *value)
{
    GwyEnum data_types[] = {
        { "signed char",            NRRD_TYPE_SINT8,  },
        { "int8",                   NRRD_TYPE_SINT8,  },
        { "int8_t",                 NRRD_TYPE_SINT8,  },
        { "unsigned char",          NRRD_TYPE_UINT8,  },
        { "uchar",                  NRRD_TYPE_UINT8,  },
        { "uint8",                  NRRD_TYPE_UINT8,  },
        { "uint8_t",                NRRD_TYPE_UINT8,  },
        { "short",                  NRRD_TYPE_SINT16, },
        { "short int",              NRRD_TYPE_SINT16, },
        { "signed short",           NRRD_TYPE_SINT16, },
        { "signed short int",       NRRD_TYPE_SINT16, },
        { "int16",                  NRRD_TYPE_SINT16, },
        { "int16_t",                NRRD_TYPE_SINT16, },
        { "ushort",                 NRRD_TYPE_UINT16, },
        { "unsigned short",         NRRD_TYPE_UINT16, },
        { "unsigned short int",     NRRD_TYPE_UINT16, },
        { "uint16",                 NRRD_TYPE_UINT16, },
        { "uint16_t",               NRRD_TYPE_UINT16, },
        { "int",                    NRRD_TYPE_SINT32, },
        { "signed int",             NRRD_TYPE_SINT32, },
        { "int32",                  NRRD_TYPE_SINT32, },
        { "int32_t",                NRRD_TYPE_SINT32, },
        { "uint",                   NRRD_TYPE_UINT32, },
        { "unsigned int",           NRRD_TYPE_UINT32, },
        { "uint32",                 NRRD_TYPE_UINT32, },
        { "uint32_t",               NRRD_TYPE_UINT32, },
        { "longlong",               NRRD_TYPE_SINT64, },
        { "long long",              NRRD_TYPE_SINT64, },
        { "long long int",          NRRD_TYPE_SINT64, },
        { "signed long long",       NRRD_TYPE_SINT64, },
        { "signed long long int",   NRRD_TYPE_SINT64, },
        { "int64",                  NRRD_TYPE_SINT64, },
        { "int64_t",                NRRD_TYPE_SINT64, },
        { "ulonglong",              NRRD_TYPE_UINT64, },
        { "unsigned long long",     NRRD_TYPE_UINT64, },
        { "unsigned long long int", NRRD_TYPE_UINT64, },
        { "uint64",                 NRRD_TYPE_UINT64, },
        { "uint64_t",               NRRD_TYPE_UINT64, },
        { "float",                  NRRD_TYPE_FLOAT,  },
        { "double",                 NRRD_TYPE_DOUBLE, },
        { "block",                  NRRD_TYPE_BLOCK,  },
    };

    gchar *s;
    NRRDDataType data_type;

    if (!value)
        return NRRD_TYPE_UNKNOWN;

    s = g_ascii_strdown(value, -1);
    data_type = gwy_string_to_enum(s, data_types, G_N_ELEMENTS(data_types));
    g_free(s);

    return data_type;
}

static NRRDEncoding
parse_encoding(const gchar *value)
{
    GwyEnum encodings[] = {
        { "raw",   NRRD_ENCODING_RAW,   },
        { "text",  NRRD_ENCODING_TEXT,  },
        { "txt",   NRRD_ENCODING_TEXT,  },
        { "ascii", NRRD_ENCODING_TEXT,  },
        { "hex",   NRRD_ENCODING_HEX,   },
        { "gzip",  NRRD_ENCODING_GZIP,  },
        { "gz",    NRRD_ENCODING_GZIP,  },
        { "bzip2", NRRD_ENCODING_BZIP2, },
        { "bz2",   NRRD_ENCODING_BZIP2, },
    };

    NRRDEncoding encoding;
    gchar *s;

    if (!value)
        return NRRD_ENCODING_UNKNOWN;

    s = g_ascii_strdown(value, -1);
    encoding = gwy_string_to_enum(s, encodings, G_N_ELEMENTS(encodings));
    g_free(s);

    return encoding;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
