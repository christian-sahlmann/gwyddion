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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgwyddion/gwymacros.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define MAGIC_BIN "\\*File list\r\n"
#define MAGIC_TXT "?*File list\r\n"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

typedef enum {
    NANOSCOPE_FILE_TYPE_NONE = 0,
    NANOSCOPE_FILE_TYPE_BIN,
    NANOSCOPE_FILE_TYPE_TXT
} NanoscopeFileType;

static gboolean      module_register     (const gchar *name);
static gint          nanoscope_detect    (const gchar *filename,
                                          gboolean only_name);
static GwyContainer* nanoscope_load      (const gchar *filename);
static GHashTable*   read_hash           (gchar **buffer);
static gchar*        next_line           (gchar **buffer);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    "nanoscope",
    "Load Nanoscope data.",
    "Yeti <yeti@gwyddion.net>",
    "0.0",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo nanoscope_func_info = {
        "nanoscope",
        "Nanoscope files",
        (GwyFileDetectFunc)&nanoscope_detect,
        (GwyFileLoadFunc)&nanoscope_load,
        NULL,
    };

    gwy_file_func_register(name, &nanoscope_func_info);

    return TRUE;
}

static gint
nanoscope_detect(const gchar *filename,
                 gboolean only_name)
{
    FILE *fh;
    gint score;
    gchar magic[MAGIC_SIZE];

    if (only_name)
        return 0;

    if (!(fh = fopen(filename, "rb")))
        return 0;
    score = 0;
    if (fread(magic, 1, MAGIC_SIZE, fh) == MAGIC_SIZE
        && (memcmp(magic, MAGIC_TXT, MAGIC_SIZE) == 0
            || memcmp(magic, MAGIC_BIN, MAGIC_SIZE) == 0))
        score = 100;
    fclose(fh);

    return score;
}

static GwyContainer*
nanoscope_load(const gchar *filename)
{
    GObject *object;
    GError *err = NULL;
    gchar *buffer = NULL, *p;
    gsize size = 0;
    gsize pos = 0;
    NanoscopeFileType file_type = NANOSCOPE_FILE_TYPE_NONE;
    GHashTable *hash;
    GList *hashes = NULL;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    p = buffer;
    if (size > MAGIC_SIZE) {
        if (!memcmp(buffer, MAGIC_TXT, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_TXT;
        else if (!memcmp(buffer, MAGIC_BIN, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_TXT;
    }
    if (!file_type) {
        g_warning("File %s doesn't seem to be a nanoscope file", filename);
        g_free(buffer);
        return NULL;
    }
    /* fix the first char for hash reading */
    *buffer = '\\';

    while ((hash = read_hash(&buffer))) {
        hashes = g_list_append(hashes, hash);
    }
    g_free(buffer);

    return NULL;
    return (GwyContainer*)object;
}

static GHashTable*
read_hash(gchar **buffer)
{
    GHashTable *hash;
    gchar *line, *colon;

    line = next_line(buffer);
    if (line[0] != '\\' || line[1] != '*')
        return NULL;
    if (!strcmp(line, "\\*File list end")) {
        gwy_debug("FILE LIST END");
        return NULL;
    }

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hash, "#self", line + 2);    /* self */
    gwy_debug("hash table <%s>", line + 2);
    while ((*buffer)[0] == '\\' && (*buffer)[1] && (*buffer)[1] != '*') {
        line = next_line(buffer) + 1;
        if (!line || !line[0] || !line[1] || !line[2])
            goto fail;
        colon = strchr(line + 3, ':');
        if (!colon || !isspace(colon[1]))
            goto fail;
        *colon = '\0';
        g_hash_table_insert(hash, line, colon + 2);
        gwy_debug("<%s>: <%s>", line, colon + 2);
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}

/**
 * next_line:
 * @buffer: A character buffer containing some text.
 *
 * Extracts a next line from @buffer.
 *
 * @buffer is updated to point after the end of the line and the "\n" 
 * (or "\r\n") is replaced with "\0", if present.
 *
 * Returns: The start of the line.  %NULL if the buffer is empty or %NULL.
 *          The line is not duplicated, the returned pointer points somewhere
 *          to @buffer.
 **/
static gchar*
next_line(gchar **buffer)
{
    gchar *p, *q;

    if (!buffer || !*buffer)
        return NULL;

    q = *buffer;
    p = strchr(*buffer, '\n');
    if (p) {
        if (p > *buffer && *(p-1) == '\r')
            *(p-1) = '\0';
        *buffer = p+1;
        *p = '\0';
    }
    else
        *buffer = NULL;

    return q;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

