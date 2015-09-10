/*
 *  $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * Keyence microscope VK
 * *.vk4
 * Read
 **/
#define DEBUG 1
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libprocess/dataline.h>
#include <libprocess/spectra.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "VK4_"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define MAGIC0 "\x00\x00\x00\x00"
#define MAGIC0_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".vk4"

enum {
    KEYENCE_HEADER_SIZE = 12,
    KEYENCE_OFFSET_TABLE_SIZE = 72,
};

typedef struct {
    guchar magic[4];
    guchar dll_version[4];
    guchar file_type[4];
} KeyenceHeader;

typedef struct {
    guint setting;
    guint color_peak;
    guint color_light;
    guint light0;
    guint light1;
    guint light2;
    guint height0;
    guint height1;
    guint height2;
    guint color_peak_thumbnail;
    guint color_thumbnail;
    guint light_thumbnail;
    guint height_thumbnail;
    guint assemble;
    guint line_measure;
    guint line_thickness;
    guint string_data;
    guint reserved;
} KeyenceOffsetTable;

typedef struct {
    KeyenceHeader header;
    KeyenceOffsetTable offset_table;
    /* Raw file contents. */
    guchar *buffer;
    gsize size;
} KeyenceFile;

static gboolean      module_register  (void);
static gint          keyence_detect   (const GwyFileDetectInfo *fileinfo,
                                       gboolean only_name);
static GwyContainer* keyence_load     (const gchar *filename,
                                       GwyRunType mode,
                                       GError **error);
static gboolean      read_header      (const guchar **p,
                                       gsize *size,
                                       KeyenceHeader *header,
                                       GError **error);
static gboolean      read_offset_table(const guchar **p,
                                       gsize *size,
                                       KeyenceOffsetTable *offsettable,
                                       GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Keyence VK4 files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("keyence",
                           N_("Omicron flat files "),
                           (GwyFileDetectFunc)&keyence_detect,
                           (GwyFileLoadFunc)&keyence_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
keyence_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE + KEYENCE_HEADER_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0
        && memcmp(fileinfo->head + 8, MAGIC0, MAGIC0_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
keyence_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    KeyenceFile kfile;
    GwyContainer *data = NULL;
    guchar* buffer = NULL;
    const guchar *p;
    gsize size = 0, remsize;
    GError *err = NULL;

    gwy_clear(&kfile, 1);
    if (!gwy_file_get_contents(filename, &kfile.buffer, &kfile.size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    remsize = size;
    p = buffer;

    if (!read_header(&p, &remsize, &kfile.header, error)
        || !read_offset_table(&p, &remsize, &kfile.offset_table, error))
        goto fail;

    err_NO_DATA(error);

fail:
    //free_file(fff);
    gwy_file_abandon_contents(kfile.buffer, kfile.size, NULL);
    return data;
}

static void
err_TRUNCATED(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File is truncated."));
}

static gboolean
read_header(const guchar **p,
            gsize *size,
            KeyenceHeader *header,
            GError **error)
{
    if (*size < KEYENCE_HEADER_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    get_CHARARRAY(header->magic, p);
    get_CHARARRAY(header->dll_version, p);
    get_CHARARRAY(header->file_type, p);
    if (memcmp(header->magic, MAGIC, MAGIC_SIZE) != 0
        || memcmp(header->file_type, MAGIC0, MAGIC0_SIZE) != 0) {
        err_FILE_TYPE(error, "Keyence VK4");
        return FALSE;
    }

    *size -= KEYENCE_HEADER_SIZE;
    return TRUE;
}

static gboolean
read_offset_table(const guchar **p,
                  gsize *size,
                  KeyenceOffsetTable *offsettable,
                  GError **error)
{
    if (*size < KEYENCE_OFFSET_TABLE_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    offsettable->setting = gwy_get_guint32_le(p);
    offsettable->color_peak = gwy_get_guint32_le(p);
    offsettable->color_light = gwy_get_guint32_le(p);
    offsettable->light0 = gwy_get_guint32_le(p);
    offsettable->light1 = gwy_get_guint32_le(p);
    offsettable->light2 = gwy_get_guint32_le(p);
    offsettable->height0 = gwy_get_guint32_le(p);
    offsettable->height1 = gwy_get_guint32_le(p);
    offsettable->height2 = gwy_get_guint32_le(p);
    offsettable->color_peak_thumbnail = gwy_get_guint32_le(p);
    offsettable->color_thumbnail = gwy_get_guint32_le(p);
    offsettable->light_thumbnail = gwy_get_guint32_le(p);
    offsettable->height_thumbnail = gwy_get_guint32_le(p);
    offsettable->assemble = gwy_get_guint32_le(p);
    offsettable->line_measure = gwy_get_guint32_le(p);
    offsettable->line_thickness = gwy_get_guint32_le(p);
    offsettable->string_data = gwy_get_guint32_le(p);
    offsettable->reserved = gwy_get_guint32_le(p);

    *size -= KEYENCE_OFFSET_TABLE_SIZE;
    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
