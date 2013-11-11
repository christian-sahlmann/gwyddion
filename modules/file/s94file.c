/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
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
 * S94 STM files
 * .s94
 * Read
 **/

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".s94"

enum {
    HEADER_LEN = 60
};

static gboolean      module_register(void);
static gint          s94_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* s94_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports S94 STM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("s94file",
                           N_("S94 STM files (.s94)"),
                           (GwyFileDetectFunc)&s94_detect,
                           (GwyFileLoadFunc)&s94_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
s94_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    guint xres, yres;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len <= HEADER_LEN)
        return 0;

    xres = ((guint)fileinfo->head[1] << 8 | fileinfo->head[0]);
    yres = ((guint)fileinfo->head[3] << 8 | fileinfo->head[2]);
    if (2*xres*yres + HEADER_LEN == fileinfo->file_size)
        return 80;

    return 0;
}

static GwyContainer*
s94_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    enum {
        RES_OFFSET = 0x00,
        REAL_OFFSET = 0x0c,
        ZSCALE_OFFSET = 0x30,
    };

    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    guint xres, yres;
    gdouble xreal, yreal, zscale, min, max;
    GwyDataField *dfield;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size <= HEADER_LEN) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer + RES_OFFSET;
    xres = gwy_get_guint16_le(&p);
    yres = gwy_get_guint16_le(&p);
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    if (err_SIZE_MISMATCH(error, 2*xres*yres + HEADER_LEN, size, TRUE)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer + REAL_OFFSET;
    xreal = 1e-9*gwy_get_gfloat_le(&p);
    yreal = 1e-9*gwy_get_gfloat_le(&p);
    if (!(xreal = fabs(xreal))) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!(yreal = fabs(yreal))) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    p = buffer + ZSCALE_OFFSET;
    // FIXME: Just a wild guess based on a single file.
    zscale = 1e-9/(50.0*gwy_get_gdouble_le(&p));
    if (!(zscale >= 0.0)) {
        // Just a random guess.
        g_warning("Bogus z scale, fixing to 1 AA");
        zscale = 1e-9/50.0;
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_convert_raw_data(buffer + HEADER_LEN, xres*yres,
                         1, GWY_RAW_DATA_SINT16, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         dfield->data, 1.0, 0.0);
    gwy_data_field_get_min_max(dfield, &min, &max);
    if (max > min)
        gwy_data_field_multiply(dfield, zscale/(max - min));

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "m");

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));
    g_object_unref(dfield);

    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
