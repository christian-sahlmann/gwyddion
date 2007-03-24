/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek, Markus Pristovsek
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net,
 *  prissi@gift.physik.tu-berlin.de.
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
 * <mime-type type="application/x-seiko-spm">
 *   <comment>Seiko SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="SPIZ000DFM"/>
 *   </magic>
 *   <glob pattern="*.xqd"/>
 *   <glob pattern="*.XQD"/>
 * </mime-type>
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

#define MAGIC "SPIZ000DFM"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".xqd"

#define Nanometer 1e-9

enum { HEADER_SIZE = 2944 };

static gboolean      module_register   (void);
static gint          seiko_detect      (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* seiko_load        (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyDataField* read_data_field   (const guchar *buffer,
                                        guint size,
                                        GError **error);
static void          seiko_process_meta(GwyContainer *container,
                                        const guchar *buffer,
                                        guint size);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Seiko XQD files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek & Markus Pristovsek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("seiko",
                           N_("Seiko files (.xqd)"),
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

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= HEADER_SIZE + 2
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
seiko_load(const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = read_data_field(buffer, size, error);
    if (!dfield) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));

    seiko_process_meta(container, buffer, size);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static GwyDataField*
read_data_field(const guchar *buffer,
                guint size,
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
    gint xres, yres, i, j;
    guint version, endfile, datastart;
    gdouble xreal, yreal, q;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data, *row;
    const gint16 *pdata;
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

    xres = (int)sqrt((endfile - datastart)/2 + 0.1);
    yres = xres;
    gwy_debug("xres = yres: %d (%s)",
              xres, 2*xres*xres == endfile - datastart ? "OK" : "Not square!");

    p = buffer + XSCALE_OFFSET;
    xreal = gwy_get_gdouble_le(&p) * xres * Nanometer;
    p = buffer + YSCALE_OFFSET;
    yreal = gwy_get_gdouble_le(&p) * yres * Nanometer;
    p = buffer + ZSCALE_OFFSET;
    q = gwy_get_gdouble_le(&p) * Nanometer;
    gwy_debug("xreal: %g, yreal: %g, zreal: %g",
              xreal/Nanometer, yreal/Nanometer, q/Nanometer);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint16*)(buffer + HEADER_SIZE);
    for (i = 0; i < yres; i++) {
        row = data + i*xres;
        for (j = 0; j < xres; j++)
            row[j] = GUINT16_TO_LE(pdata[i*xres + j])*q;
    }

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

static void
seiko_process_meta(GwyContainer *container,
                   const guchar *buffer,
                   G_GNUC_UNUSED guint size)
{
    GwyContainer *meta;

    enum {
        COMMENT_OFFSET  = 0x28,
        COMMENT_SIZE  = 0x70,
    };
    gchar comment[0x70];
    const guchar *p;

    p = buffer + COMMENT_OFFSET;
    get_CHARARRAY0(comment, &p);

    meta = gwy_container_new();

    if (comment[0])
        gwy_container_set_string_by_name(meta, "Comment", g_strdup(comment));

    if (gwy_container_get_n_items(meta))
        gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

