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
 * <mime-type type="application/x-mif-spm">
 *   <comment>DME MIF SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="MIF\x01\x00"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * DME MIF
 * .mif
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <stdio.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "MIF\x01\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define HEADER_SIZE 491

typedef struct {
    gsize offset;
    gsize size;
} MIFBlock;

typedef struct {
    gchar magic[3];
    guint software_version;
    guint file_version;
    gchar time[19];
    gchar comment[255];
    guint nimages;
    MIFBlock info;
    gchar unused[200];
} MIFHeader;


static gboolean        module_register       (void);
static gint            mif_detect       (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*   mif_load         (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports DME MIF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("miffile",
                           N_("DME MIF files (.mif)"),
                           (GwyFileDetectFunc)&mif_detect,
                           (GwyFileLoadFunc)&mif_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
mif_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
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
mif_read_header(const guchar *buffer,
                gsize size,
                MIFHeader *header,
                GError **error)
{
    const guchar *p = buffer;

    if (size <= HEADER_SIZE) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        err_FILE_TYPE(error, "MIF");
        return FALSE;
    }

    gwy_clear(header, 1);
    get_CHARARRAY(header->magic, &p);
    header->software_version = gwy_get_guint16_be(&p);
    header->file_version = gwy_get_guint16_be(&p);
    gwy_debug("sw version: %u.%u, file version: %u.%u",
              header->software_version/0x100, header->software_version % 0x100,
              header->file_version/0x100, header->file_version % 0x100);
    get_CHARARRAY(header->time, &p);
    get_CHARARRAY(header->comment, &p);
    header->nimages = gwy_get_guint16_le(&p);
    gwy_debug("n images: %u", header->nimages);
    header->info.offset = gwy_get_guint32_le(&p);
    header->info.size = gwy_get_guint32_le(&p);
    gwy_debug("info offset: %zu, info size: %zu", header->info.offset, header->info.size);
    get_CHARARRAY(header->unused, &p);

    if (header->info.offset < HEADER_SIZE
        || header->info.offset > size
        || header->info.size > size
        || header->info.offset + header->info.size > size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File information block is outside the file."));
        return FALSE;
    }

    return TRUE;
}


static GwyContainer*
mif_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwySIUnit *units = NULL;
    gint32 power10;
    MIFHeader header;
    GwyDataField *dfield;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!mif_read_header(buffer, size, &header, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }


    gwy_file_abandon_contents(buffer, size, NULL);
    err_NO_DATA(error);
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
