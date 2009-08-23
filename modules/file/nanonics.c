/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#define DEBUG 1
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanonics-spm">
 *   <comment>Nanonics SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="NAN File\n-Start Header-"/>
 *   </magic>
 *   <glob pattern="*.nan"/>
 *   <glob pattern="*.NAN"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC_LINE      "NAN File\n"
#define MAGIC_LINE_SIZE (sizeof(MAGIC_LINE) - 1)

#define MAGIC      MAGIC_LINE "-Start Header-"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".nan"

typedef struct {
    guint header_length;
    guint data_length;
    GHashTable *meta;
    GHashTable **chmeta;
} NanonicsFile;

static gboolean      module_register     (void);
static gint          nanonics_detect     (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* nanonics_load       (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static GHashTable*   nanonics_read_header(gchar *text,
                                          const gchar *name,
                                          GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanonics NAN data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanonics",
                           N_("Nanonics files (.nan)"),
                           (GwyFileDetectFunc)&nanonics_detect,
                           (GwyFileLoadFunc)&nanonics_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nanonics_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 80;
}

static GwyContainer*
nanonics_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gchar *s, *header = NULL;
    NanonicsFile nfile;
    gsize size = 0;
    GError *err = NULL;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Nanonics");
        goto fail;
    }

    gwy_clear(&nfile, 1);
    header = g_strndup(buffer + MAGIC_LINE_SIZE,
                       MIN(size - MAGIC_LINE_SIZE, 4906));
    if (!(s = strstr(header, "-End Header-"))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected header end marker ‘%s’ was not found."),
                    "-End Header-");
        goto fail;
    }
    nfile.header_length = (s - header) + strlen("-End Header-");
    header[nfile.header_length] = '\0';

    if (!(nfile.meta = nanonics_read_header(header, "Header", error)))
        goto fail;

    if (!require_keys(nfile.meta, error,
                      "HeaderLength", "DataLength",
                      "ReF", "ReS",
                      NULL))
        goto fail;

    nfile.header_length = strtol(g_hash_table_lookup(nfile.meta,
                                                     "HeaderLength"),
                                 NULL, 10);
    nfile.data_length = strtol(g_hash_table_lookup(nfile.meta, "DataLength"),
                               NULL, 10);

fail:
    g_free(header);
    if (nfile.meta)
        g_hash_table_destroy(nfile.meta);
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

static GHashTable*
nanonics_read_header(gchar *text, const gchar *name, GError **error)
{
    GHashTable *hash;
    gchar *line, *p, *s, *val, *marker;

    p = text;

    line = gwy_str_next_line(&p);
    g_strstrip(line);
    marker = g_strdup_printf("-Start %s-", name);
    if (!gwy_strequal(line, marker)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected header start marker ‘%s’ but found ‘%s’."),
                    marker, line);
        g_free(marker);
        return NULL;
    }
    g_free(marker);

    marker = g_strdup_printf("-End %s-", name);
    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        g_strstrip(line);
        if (gwy_strequal(line, marker))
            break;
        if (!*line)
            continue;

        while (line && (s = strchr(line, '='))) {
            *s = '\0';
            g_strchomp(line);
            for (val = s+1; g_ascii_isspace(*val); val++)
                ;

            s = line;
            if ((line = strchr(val, ','))) {
                *line = '\0';
                line++;
            }
            g_strchomp(val);
            g_hash_table_insert(hash, g_strdup(s), g_strdup(val));
            gwy_debug("<%s>=<%s>", s, val);
        }
    }

    if (!line) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected header end marker ‘%s’ was not found."),
                    marker);
        g_hash_table_destroy(hash);
        g_free(marker);
        return NULL;
    }

    /*
    line = gwy_str_next_line(&p);
    if (line)
        g_warning("Text beyond %s", marker);
        */
    g_free(marker);

    return hash;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

