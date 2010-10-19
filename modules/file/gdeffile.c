/*
 *  @(#) $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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
 * <mime-type type="application/x-gdef-spm">
 *   <comment>DME GDEF SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="GDEF\x00"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * DME GDEF
 * .gdf
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <stdio.h>
#include <time.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "GDEF\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

enum {
    HEADER_SIZE = 4 + 2 + 6 + 4,
};

typedef struct {
    gchar magic[4];
    guint version;
    gulong creation_time;
    guint desc_len;
} GDEFHeader;

static gboolean      module_register(void);
static gint          gdef_detect    (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* gdef_load      (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports DME GDEF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("gdeffile",
                           N_("DME GDEF files (.gdf)"),
                           (GwyFileDetectFunc)&gdef_detect,
                           (GwyFileLoadFunc)&gdef_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
gdef_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;
    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size > HEADER_SIZE
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static gboolean
gdef_read_header(GDEFHeader *header,
                 const guchar *buffer,
                 gsize size,
                 GError **error)
{
    const guchar *p = buffer;

    gwy_clear(header, 1);

    if (size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        return FALSE;
    }
    get_CHARARRAY(header->magic, &p);
    if (memcmp(header->magic, MAGIC, 4) != 0) {
        err_FILE_TYPE(error, "GDEF");
        return FALSE;
    }
    header->version = gwy_get_guint16_le(&p);
    gwy_debug("version: %u.%u", header->version/0x100, header->version % 0x100);
    header->creation_time = gwy_get_guint32_le(&p);
    // WTF?
    gwy_get_guint16_le(&p);
    gwy_debug("creation_time: %lu", header->creation_time);
    header->desc_len = gwy_get_guint32_le(&p);
    gwy_debug("desc_len: %u", header->desc_len);

    return TRUE;
}

static GwyContainer*
gdef_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GDEFHeader header;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!gdef_read_header(&header, buffer, size, error)) {
        goto fail;
    }
    /* TODO: Check file version */

    err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
