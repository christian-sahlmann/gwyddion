/*
 *  $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#define DEBUG 1
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-intelliwave-esd">
 *   <comment>IntelliWave interferometric ESD data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="ESD IntelliWave"/>
 *   </magic>
 *   <glob pattern="*.esd"/>
 *   <glob pattern="*.ESD"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # IntelliWave interferometric ESD data
 * 0 string ESD\ IntelliWave IntelliWave interferometric ESD data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * IntelliWave ESD
 * .esd
 * Read
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define EXTENSION ".esd"

#define MAGIC "ESD IntelliWave"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

enum {
    HEADER_SIZE = 1024,
};

/* TNameType seems to be char[50] */
typedef struct {
    gchar name[50];
    gchar version[50];
    guint32 reserved1;
    guint32 file_offsets[100];
    guint n_data_sets;
    gchar reserved2[516];
} IntWaveFileHeader;

typedef struct {
    IntWaveFileHeader header;
} IntWaveFile;

static gboolean      module_register (void);
static gint          intw_detect     (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* intw_load       (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static void          intw_read_header(const guchar *p,
                                      IntWaveFile *intwfile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports IntelliWave inteferometric ESD data."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("intelliwave",
                           N_("IntelliWave inteferometric data (.esd)"),
                           (GwyFileDetectFunc)&intw_detect,
                           (GwyFileLoadFunc)&intw_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
intw_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->file_size > HEADER_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
intw_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    IntWaveFile intwfile;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    intw_read_header(buffer, &intwfile);


    gwy_file_abandon_contents(buffer, size, NULL);
    err_NO_DATA(error);

    //gwy_file_channel_import_log_add(container, 0, NULL, filename);

    return container;
}

static void
intw_read_header(const guchar *p,
                 IntWaveFile *intwfile)
{
    IntWaveFileHeader *header = &intwfile->header;
    guint i;

    get_CHARARRAY0(header->name, &p);
    gwy_debug("name %s", header->name);
    get_CHARARRAY0(header->version, &p);
    gwy_debug("version %s", header->version);
    header->reserved1 = gwy_get_guint32_le(&p);
    for (i = 0; i < G_N_ELEMENTS(header->file_offsets); i++) {
        header->file_offsets[i] = gwy_get_guint32_le(&p);
        if (header->file_offsets[i]) {
            gwy_debug("offset[%u] 0x%08x", i, header->file_offsets[i]);
        }
    }
    header->n_data_sets = gwy_get_guint32_le(&p);
    gwy_debug("n_data_sets %u", header->n_data_sets);
    get_CHARARRAY(header->reserved2, &p);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
