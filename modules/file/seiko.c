/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Markus Pristovsek.
 *  E-mail: yeti@gwyddion.net, prissi@gift.physik.tu-berlin.de.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-seiko-spm">
 *   <comment>Seiko SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="SPIZ000AFM"/>
 *     <match type="string" offset="0" value="SPIZ000DFM"/>
 *     <match type="string" offset="0" value="NPXZ000AFM"/>
 *     <match type="string" offset="0" value="NPXZ000DFM"/>
 *   </magic>
 *   <glob pattern="*.xqb"/>
 *   <glob pattern="*.XQB"/>
 *   <glob pattern="*.xqd"/>
 *   <glob pattern="*.XQD"/>
 *   <glob pattern="*.xqt"/>
 *   <glob pattern="*.XQT"/>
 *   <glob pattern="*.xqp"/>
 *   <glob pattern="*.XQP"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Seiko SII
 * .xqb .xqd .xqt .xqp
 * Read
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC1 "SPIZ000AFM"
#define MAGIC2 "SPIZ000DFM"
#define MAGIC3 "NPXZ000AFM"
#define MAGIC4 "NPXZ000DFM"
#define MAGIC_SIZE (sizeof(MAGIC1)-1)

#define EXTENSION1 ".xqb"
#define EXTENSION2 ".xqd"
#define EXTENSION3 ".xqt"
#define EXTENSION4 ".xqp"

#define Nanometer 1e-9

enum { HEADER_SIZE = 2944 };

typedef enum {
    SEIKO_TOPOGRAPHY = 0,
    SEIKO_PHASE      = 1,
} SeikoDataType;

static gboolean      module_register   (void);
static gint          seiko_detect      (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* seiko_load        (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyDataField* read_data_field   (const guchar *buffer,
                                        guint size,
                                        SeikoDataType datatype,
                                        GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Seiko XQB, XQD, XQT and XQP files."),
    "Yeti <yeti@gwyddion.net>",
    "0.9",
    "David Nečas (Yeti) & Markus Pristovsek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("seiko",
                           N_("Seiko files (.xqb, .xqd, .xqt, .xqp)"),
                           (GwyFileDetectFunc)&seiko_detect,
                           (GwyFileLoadFunc)&seiko_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
seiko_detect(const GwyFileDetectInfo *fileinfo,
             gboolean only_name)
{
    gint score = 0;

    if (only_name) {
        if (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION1)
            || g_str_has_suffix(fileinfo->name_lowercase, EXTENSION2)
            || g_str_has_suffix(fileinfo->name_lowercase, EXTENSION3)
            || g_str_has_suffix(fileinfo->name_lowercase, EXTENSION4))
            return 20;
        return 0;
    }

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= HEADER_SIZE + 2
        && (memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE) == 0
            || memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE) == 0
            || memcmp(fileinfo->head, MAGIC3, MAGIC_SIZE) == 0
            || memcmp(fileinfo->head, MAGIC4, MAGIC_SIZE) == 0))
        score = 100;

    return score;
}

static GwyContainer*
seiko_load(const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    enum {
        COMMENT_OFFSET = 0x480,
        COMMENT_SIZE = 0x80,
    };

    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    SeikoDataType datatype = SEIKO_TOPOGRAPHY;
    gchar *comment;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (memcmp(buffer, MAGIC1, MAGIC_SIZE) != 0
        && memcmp(buffer, MAGIC2, MAGIC_SIZE) != 0
        && memcmp(buffer, MAGIC3, MAGIC_SIZE) != 0
        && memcmp(buffer, MAGIC4, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Seiko");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* FIXME: I was not able to reverse-engineer what identifies the type.
     * May need a large set of files.  */
    if (g_str_has_suffix(filename, ".xqp")
        || g_str_has_suffix(filename, ".XQP"))
        datatype = SEIKO_PHASE;

    dfield = read_data_field(buffer, size, datatype, error);
    if (!dfield) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    comment = g_strndup(buffer + COMMENT_OFFSET, COMMENT_SIZE);
    g_strstrip(comment);
    if (strlen(comment))
        gwy_container_set_string_by_name(container, "/0/data/title", comment);
    else {
        g_free(comment);
        gwy_app_channel_title_fall_back(container, 0);
    }

    gwy_app_channel_check_nonsquare(container, 0);

    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static GwyDataField*
read_data_field(const guchar *buffer,
                guint size,
                SeikoDataType datatype,
                GError **error)
{
    enum {
        VERSION_OFFSET  = 0x10,
        ENDFILE_OFFSET  = 0x14,
        DATASTART_OFFSET  = 0x18,
        XSCALE_OFFSET = 0x98,
        YSCALE_OFFSET = 0xa0,
        ZSCALE_OFFSET = 0xa8,
    };
    gint xres, yres;
    G_GNUC_UNUSED guint version;
    guint n, endfile, datastart;
    gdouble xreal, yreal, q, alpha;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    const guchar *p;

    p = buffer + VERSION_OFFSET;
    version = gwy_get_guint32_le(&p);
    p = buffer + ENDFILE_OFFSET;
    endfile = gwy_get_guint32_le(&p);
    p = buffer + DATASTART_OFFSET;
    datastart = gwy_get_guint32_le(&p);
    gwy_debug("version: %u, endfile: %u, datastart: %u",
              version, endfile, datastart);

    if (err_SIZE_MISMATCH(error, endfile, size, TRUE))
        return NULL;

    p = buffer + XSCALE_OFFSET;
    xreal = gwy_get_gdouble_le(&p) * Nanometer;
    p = buffer + YSCALE_OFFSET;
    yreal = gwy_get_gdouble_le(&p) * Nanometer;
    p = buffer + ZSCALE_OFFSET;
    q = gwy_get_gdouble_le(&p);
    if (datatype == SEIKO_TOPOGRAPHY)
        q *= Nanometer;
    gwy_debug("xscale: %g, yscale: %g, zreal: %g",
              xreal/Nanometer, yreal/Nanometer, q);

    alpha = xreal/yreal;
    n = (endfile - datastart)/2;
    xres = (int)sqrt(n/alpha + 0.1);
    yres = n/xres;
    gwy_debug("1st try: xres: %d, yres: %d, size: %u vs. %u",
              xres, yres, 2*xres*yres, endfile - datastart);
    if (2*xres*yres != endfile - datastart) {
        xres += 1;
        yres = n/xres;
        gwy_debug("2nd try: xres: %d, yres: %d, size: %u vs. %u",
                  xres, yres, 2*xres*yres, endfile - datastart);
    }
    if (2*xres*yres != endfile - datastart) {
        xres += 2;
        yres = n/xres;
        gwy_debug("3rd try: xres: %d, yres: %d, size: %u vs. %u",
                  xres, yres, 2*xres*yres, endfile - datastart);
    }
    if (2*xres*yres != endfile - datastart) {
        /* Square */
        if (fabs(alpha - 1.0) > 1e-3)
            xres = yres = (int)sqrt(n + 0.1);
        gwy_debug("4th try: xres: %d, yres: %d, size: %u vs. %u",
                  xres, yres, 2*xres*yres, endfile - datastart);
    }
    if (2*xres*yres != endfile - datastart) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot determine scan dimensions; it seems "
                      "non-square with an unknown side ratio."));
        return NULL;
    }

    xreal *= xres;
    yreal *= yres;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_convert_raw_data(buffer + HEADER_SIZE, xres*yres, 1,
                         GWY_RAW_DATA_UINT16, G_LITTLE_ENDIAN,
                         gwy_data_field_get_data(dfield), q, 0.0);
    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    if (datatype == SEIKO_PHASE)
        siunit = gwy_si_unit_new("deg");
    else
        siunit = gwy_si_unit_new("m");

    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
