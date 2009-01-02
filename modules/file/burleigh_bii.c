/*
 *  @(#) $Id: burleigh_bii.c 8625 2007-10-10 20:03:12Z yeti-dn $
 *  Copyright (C) 2008 David Necas (Yeti), Petr Klapetek, Jan Horak.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net, xhorak@gmail.com.
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

/* XXX: Unfortunately, BM6 is Windows 3.x bitmap and we cannot claim
 * ownership of that. */
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-burleigh-bii-spm">
 *   <comment>Burleigh BII SPM data</comment>
 *   <magic priority="30">
 *     <match type="string" offset="0" value="BM6"/>
 *   </magic>
 *   <glob pattern="*.bii"/>
 *   <glob pattern="*.BII"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <libprocess/stats.h>

#include "err.h"

#define EXTENSION "bii"
#define MAGIC "BM6"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define SIGNATURE "Burleigh Instruments"

#define FILE_IMG_SIZE_POS 0x12
#define FILE_IMG_DATA_START_POS 0x36
#define FILE_IMG_XY_SCALE_POS 0x74
#define FILE_IMG_Z_SCALE_POS 0xCC

static gboolean      module_register    (void);
static gint          burleigh_bii_detect(const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer* burleigh_bii_load  (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Burleigh BII binary data files."),
    "Jan Hořák <xhorak@gmail.com>, Yeti <yeti@gwyddion.net>",
    "0.2",
    "Jan Hořák & David Nečas (Yeti) & Petr Klapetek",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("burleigh_bii",
                           N_("Burleigh Image Studio files (.bii)"),
                           (GwyFileDetectFunc)&burleigh_bii_detect,
                           (GwyFileLoadFunc)&burleigh_bii_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
burleigh_bii_detect(const GwyFileDetectInfo *fileinfo,
                    gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < FILE_IMG_DATA_START_POS
        || fileinfo->buffer_len < MAGIC_SIZE
        || strncmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* We are fairly sure if Burleigh claims the file is theirs. */
    if (gwy_memmem(fileinfo->tail, fileinfo->buffer_len,
                   SIGNATURE, sizeof(SIGNATURE)-1))
            return 95;

    /* It might be.  Note 30 is lower than the generic pixmap loader score. */
    return 30;
}

static GwyContainer*
burleigh_bii_load(const gchar *filename,
                  G_GNUC_UNUSED GwyRunType mode,
                  GError **error)
{
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    gint width, height, data_end_offset, i, j, power10;
    gint16 *values;
    gdouble x_scale, y_scale, z_scale, q;
    GwyDataField *dfield;
    GwyContainer *container;
    GwySIUnit *units;
    gdouble *data;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;      
    }
    if (size < FILE_IMG_DATA_START_POS) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    /* get datafield dimensions from file */
    p = buffer + FILE_IMG_SIZE_POS;
    width = gwy_get_gint32_le(&p);
    height = gwy_get_gint32_le(&p);
    // get file offset where datafield ends
    data_end_offset = width * height * 2 + FILE_IMG_DATA_START_POS;      
    // get scaling factors of axes
    if (err_SIZE_MISMATCH(error,
                          data_end_offset + FILE_IMG_Z_SCALE_POS + 8, size,
                          FALSE)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    p = buffer + data_end_offset + FILE_IMG_XY_SCALE_POS;
    x_scale = gwy_get_gdouble_le(&p);
    if (!((x_scale = fabs(x_scale)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        x_scale = 1.0;
    }
    y_scale = gwy_get_gdouble_le(&p);
    if (!((y_scale = fabs(y_scale)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        y_scale = 1.0;
    }
    p = buffer + data_end_offset + FILE_IMG_Z_SCALE_POS;
    z_scale = gwy_get_gdouble_le(&p);

    //FIXME: all axes always in nm?
    units = gwy_si_unit_new_parse("nm", &power10);
    gwy_debug("w:%dfield h:%dfield scale: x%f y%f z%f", 
              width, height, x_scale * pow10(power10), 
              y_scale, z_scale);
    // create new datafield
    dfield = gwy_data_field_new(width, height, 
                                x_scale * pow10(power10), 
                                y_scale * pow10(power10), 
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, units);
    g_object_unref(units);

    units = gwy_si_unit_new_parse("nm", &power10);
    gwy_data_field_set_si_unit_z(dfield, units);
    g_object_unref(units);

    // set pointer to 16bit signed integers to start position 
    // of data definition in file
    values = (gint16*)(buffer + FILE_IMG_DATA_START_POS);
    data = gwy_data_field_get_data(dfield);
    // fill datafield
    q = z_scale * pow10(power10);
    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            data[(height - 1 - j)*width + i] = q*GINT16_FROM_LE(*values);
            values++;
        }
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    // fill container
    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_app_channel_title_fall_back(container, 0);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
