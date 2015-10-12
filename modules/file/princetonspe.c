/*
 *  $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-USERGUIDE]
 * Princeton Instruments camera SPE
 * .spe
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".spe"

/* The only fields where anything at all seems to be present in newer files. */
enum {
    XRES_CCD_OFFSET  = 0x06,
    YRES_CCD_OFFSET  = 0x12,
    NOSCAN_OFFSET    = 0x22,
    XRES_OFFSET      = 0x2a,
    DATA_TYPE_OFFSET = 0x6c,
    YRES_OFFSET      = 0x290,
    SCRAMBLE_OFFSET  = 0x292,
    LNOSCAN_OFFSET   = 0x298,
    NUMFRAMES_OFFSET = 0x5a6,
    HEADER_SIZE      = 0x1004,
};

typedef enum {
    PSPE_DATA_FLOAT = 0,
    PSPE_DATA_LONG = 1,
    PSPE_DATA_SHORT = 2,
    PSPE_DATA_USHORT = 3,
    PSPE_DATA_NTYPES
} PSPEDataType;

typedef struct {
    guint xres_ccd;
    guint yres_ccd;
    guint xres;
    guint yres;
    PSPEDataType data_type;
    guint scramble;
    guint num_frames;
    guint noscan;
    guint lnoscan;
} PSPEFile;

static gboolean      module_register (void);
static gint          pspe_detect     (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* pspe_load       (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static gboolean      pspe_read_header(PSPEFile *pspefile,
                                      const guchar *buffer);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Princeton Instruments camera SPE files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("princetonspe",
                           N_("Princeton Instruments SPE files"),
                           (GwyFileDetectFunc)&pspe_detect,
                           (GwyFileLoadFunc)&pspe_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
pspe_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    PSPEFile pspefile;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    /* XXX: We should perform a size check as the dimensions and num_frames
     * must be non-zero and when multiplied give together size smaller than the
     * file. */
    if (fileinfo->file_size > HEADER_SIZE
        && fileinfo->buffer_len >= NUMFRAMES_OFFSET + sizeof(guint32)
        && pspe_read_header(&pspefile, fileinfo->head))
        score = 80;
    /* XXX: New files (3.0) have some XML at the end.  Check that for surer
     * identification. */

    return score;
}

static GwyContainer*
pspe_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    PSPEFile pspefile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gchar *title = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }
    if (!pspe_read_header(&pspefile, buffer)) {
        err_FILE_TYPE(error, "Princeton Instruments SPE");
        goto fail;
    }
    gwy_debug("ccd %u x %u", pspefile.xres_ccd, pspefile.yres_ccd);
    gwy_debug("res %u x %u", pspefile.xres, pspefile.yres);
    gwy_debug("num frames %u", pspefile.num_frames);
    gwy_debug("data type %u", pspefile.data_type);

    err_NO_DATA(error);
    //gwy_file_channel_import_log_add(container, 0, NULL, filename);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static gboolean
pspe_read_header(PSPEFile *pspefile, const guchar *buffer)
{
    const guchar *p;

    p = buffer + XRES_CCD_OFFSET;
    pspefile->xres_ccd = gwy_get_guint16_le(&p);
    p = buffer + YRES_CCD_OFFSET;
    pspefile->yres_ccd = gwy_get_guint16_le(&p);

    p = buffer + XRES_OFFSET;
    pspefile->xres = gwy_get_guint16_le(&p);
    p = buffer + YRES_OFFSET;
    pspefile->yres = gwy_get_guint16_le(&p);

    p = buffer + DATA_TYPE_OFFSET;
    pspefile->data_type = gwy_get_guint16_le(&p);
    p = buffer + SCRAMBLE_OFFSET;
    pspefile->scramble = gwy_get_guint16_le(&p);
    p = buffer + NUMFRAMES_OFFSET;
    pspefile->num_frames = gwy_get_guint32_le(&p);

    p = buffer + NOSCAN_OFFSET;
    pspefile->noscan = gwy_get_guint16_le(&p);
    p = buffer + LNOSCAN_OFFSET;
    pspefile->lnoscan = gwy_get_guint32_le(&p);

    /* The noscan and lnoscan fields must be filled with one-bits, apparently.
     * The rest is more difficult. */
    return (pspefile->noscan == 0xffff
            && pspefile->lnoscan == 0xffffffff
            && pspefile->scramble == 1
            && pspefile->data_type < PSPE_DATA_NTYPES);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
