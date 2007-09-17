/*
 *  @(#) $Id: shimadzu.c 8177 2007-06-20 08:40:20Z yeti-dn $
 *  Copyright (C) 2007 David Necas (Yeti).
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
 * <mime-type type="application/x-shimadzu-spm">
 *   <comment>Shimadzu SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="Shimadzu SPM File Format"/>
 *   </magic>
 * </mime-type>
 **/
#define DEBUG 1
#include "config.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

enum {
    HEADER_SIZE = 32768
};

#define MAGIC "Shimadzu SPM File Format Version 2."
#define MAGIC_SIZE (sizeof(MAGIC)-1)

static gboolean      module_register      (void);
static gint          shimadzu_detect      (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* shimadzu_load        (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static gboolean      read_ascii_data      (gint n,
                                           gdouble *data,
                                           gchar **buffer,
                                           gint bpp,
                                           GError **error);
static gboolean      read_binary_data     (gint n,
                                           gdouble *data,
                                           gchar *buffer,
                                           gint bpp,
                                           GError **error);
static GHashTable*   read_hash            (gchar *buffer,
                                           gint *text_data_start,
                                           GError **error);
static void          get_scan_list_res    (GHashTable *hash,
                                           gint *xres,
                                           gint *yres);
static GwySIUnit*    get_physical_scale   (GHashTable *hash,
                                           GHashTable *scannerlist,
                                           GHashTable *scanlist,
                                           gboolean has_version,
                                           gdouble *scale,
                                           GError **error);
static GwyContainer* shimadzu_get_metadata(GHashTable *hash,
                                           GList *list);
static gboolean      require_keys         (GHashTable *hash,
                                           GError **error,
                                           ...);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Shimadzu SPM data files, version 2."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("shimadzu",
                           N_("Shimadzu files"),
                           (GwyFileDetectFunc)&shimadzu_detect,
                           (GwyFileLoadFunc)&shimadzu_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
shimadzu_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= HEADER_SIZE + 2
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
shimadzu_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *meta, *container = NULL;
    GError *err = NULL;
    guchar *buffer = NULL;
    GHashTable *hash;
    gchar *head;
    gsize size = 0;
    gboolean ok;
    gint text_data_start;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        return NULL;
    }
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Shimadzu");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    head = g_memdup(buffer, HEADER_SIZE+1);
    head[HEADER_SIZE] = '\0';

    hash = read_hash(head, &text_data_start, error);
    ok = require_keys(hash, error, "SizeX", "SizeY", NULL);

    g_free(head);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (!container && ok)
        err_NO_DATA(error);

    g_hash_table_destroy(hash);

    return container;
}

static GHashTable*
read_hash(gchar *buffer,
          gint *text_data_start,
          GError **error)
{
    GHashTable *hash;
    gchar *line, *value;
    gboolean next_is_process_profile = FALSE;

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    line = gwy_str_next_line(&buffer);

    g_hash_table_insert(hash, "Version", line + MAGIC_SIZE-2);
    while ((line = gwy_str_next_line(&buffer))) {
        gint llen;

        if (gwy_strequal(line, "hap 2"))
            break;

        if (line[0] == '/')
            line++;

        g_strstrip(line);
        llen = strlen(line);
        /* sections */
        if (line[0] == '[' && line[llen-1] == ']') {
            line[llen-1] = '\0';
            line++;
            g_strstrip(line);
            gwy_debug("section %s", line);
            if (gwy_strequal(line, "PROCESS PROFILE")) {
                next_is_process_profile = TRUE;
                continue;
            }
            next_is_process_profile = FALSE;
            /* Other sectioning seems too be uninteresting. */
            continue;
        }

        if (next_is_process_profile) {
            g_hash_table_insert(hash, "Process Profile", line);
            next_is_process_profile = FALSE;
            continue;
        }

        next_is_process_profile = FALSE;
        value = strchr(line, ':');
        if (!value) {
            g_printerr("Cannot parse: %s\n", line);
            continue;
        }
        *value = '\0';
        value++;
        g_strstrip(line);
        g_strstrip(value);
        gwy_debug("%s = %s", line, value);
        g_hash_table_insert(hash, line, value);
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}

#if 0
/* General parameter line parser */
static ShimadzuValue*
parse_value(const gchar *key, gchar *line)
{
    ShimadzuValue *val;
    gchar *p, *q;

    val = g_new0(ShimadzuValue, 1);

    /* old-style values */
    if (key[0] != '@') {
        val->hard_value = g_ascii_strtod(line, &p);
        if (p-line > 0 && *p == ' ') {
            do {
                p++;
            } while (g_ascii_isspace(*p));
            if ((q = strchr(p, '('))) {
                *q = '\0';
                q++;
                val->hard_scale = g_ascii_strtod(q, &q);
                if (*q != ')')
                    val->hard_scale = 0.0;
            }
            val->hard_value_units = p;
        }
        val->hard_value_str = line;
        return val;
    }

    /* type */
    switch (line[0]) {
        case 'V':
        val->type = SHIMADZU_VALUE_VALUE;
        break;

        case 'S':
        val->type = SHIMADZU_VALUE_SELECT;
        break;

        case 'C':
        val->type = SHIMADZU_VALUE_SCALE;
        break;

        default:
        g_warning("Cannot parse value type <%s> for key <%s>", line, key);
        g_free(val);
        return NULL;
        break;
    }

    line++;
    if (line[0] != ' ') {
        g_warning("Cannot parse value type <%s> for key <%s>", line, key);
        g_free(val);
        return NULL;
    }
    do {
        line++;
    } while (g_ascii_isspace(*line));

    /* softscale */
    if (line[0] == '[') {
        if (!(p = strchr(line, ']'))) {
            g_warning("Cannot parse soft scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        if (p-line-1 > 0) {
            *p = '\0';
            val->soft_scale = line+1;
        }
        line = p+1;
        if (line[0] != ' ') {
            g_warning("Cannot parse soft scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        do {
            line++;
        } while (g_ascii_isspace(*line));
    }

    /* hardscale (probably useless) */
    if (line[0] == '(') {
        do {
            line++;
        } while (g_ascii_isspace(*line));
        if (!(p = strchr(line, ')'))) {
            g_warning("Cannot parse hard scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        val->hard_scale = g_ascii_strtod(line, &q);
        while (g_ascii_isspace(*q))
            q++;
        if (p-q > 0) {
            *p = '\0';
            val->hard_scale_units = q;
        }
        line = p+1;
        if (line[0] != ' ') {
            g_warning("Cannot parse hard scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        do {
            line++;
        } while (g_ascii_isspace(*line));
    }

    /* hard value (everything else) */
    switch (val->type) {
        case SHIMADZU_VALUE_SELECT:
        val->hard_value_str = line;
        break;

        case SHIMADZU_VALUE_SCALE:
        val->hard_value = g_ascii_strtod(line, &p);
        break;

        case SHIMADZU_VALUE_VALUE:
        val->hard_value = g_ascii_strtod(line, &p);
        if (p-line > 0 && *p == ' ' && !strchr(p+1, ' ')) {
            do {
                p++;
            } while (g_ascii_isspace(*p));
            val->hard_value_units = p;
        }
        val->hard_value_str = line;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return val;
}
#endif

static gboolean
require_keys(GHashTable *hash,
             GError **error,
             ...)
{
    va_list ap;
    const gchar *key;

    va_start(ap, error);
    while ((key = va_arg(ap, const gchar *))) {
        if (!g_hash_table_lookup(hash, key)) {
            err_MISSING_FIELD(error, key);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

