/*
 *  @(#) $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-ambios-amb">
 *   <comment>Ambios AMB data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="Binary TrueMap Data File \\ Ambios File Format\r\n"/>
 *   </magic>
 *   <glob pattern="*.amb"/>
 *   <glob pattern="*.AMB"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Ambios AMB
 * .amb
 * Read
 **/

#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "Binary TrueMap Data File \\ Ambios File Format\r\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".amb"

enum {
    HEADER_SIZE = 65,
    XRES_OFFSET = 0x31,
    YRES_OFFSET = 0x35,
};

static gboolean      module_register(void);
static gint          amb_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* amb_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Ambios AMB data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("ambfile",
                           N_("Ambios amb files (.amb)"),
                           (GwyFileDetectFunc)&amb_detect,
                           (GwyFileLoadFunc)&amb_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
amb_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->file_size >= HEADER_SIZE
        && fileinfo->buffer_len >= MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
amb_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gdouble *data;
    guint i, j;
    gsize size = 0;
    GError *err = NULL;
    guint xres, yres;
    GwyDataField *dfield;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size <= HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }

    /* The two bytes before are usually zeroes */
    p = buffer + XRES_OFFSET;
    xres = gwy_get_guint32_le(&p);
    p = buffer + YRES_OFFSET;
    yres = gwy_get_guint32_le(&p);
    /* The four bytes after might be a float, then there are four more bytes. */
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;
    if (err_SIZE_MISMATCH(error, 4*xres*yres + HEADER_SIZE, size, TRUE))
        goto fail;

    dfield = gwy_data_field_new(xres, yres, 1.0, 1.0*yres/xres,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    p = buffer + HEADER_SIZE;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++)
            data[i*xres + j] = gwy_get_gfloat_le(&p);
    }

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "m");

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));
    g_object_unref(dfield);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
