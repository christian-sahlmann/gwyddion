/*
 *  @(#) $Id: quesant.c 8632 2007-10-11 07:59:01Z yeti-dn $
 *  Copyright (C) 2008 David Necas (Yeti), Jan Horak.
 *  E-mail: yeti@gwyddion.net, xhorak@gmail.com.
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
 * <mime-type type="application/x-quesant-afm">
 *   <comment>Quesant AFM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\x02\x00\x00\x00\x01\x00\x00\x00"/>
 *   </magic>
 * </mime-type>
 **/
// TODO: not sure about picture orientation

#include "config.h"
#include <stdio.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC "\x02\x00\x00\x00\x01\x00\x00\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define HEADER_SIZE 0x148

typedef char bool;

static gboolean        module_register       (void);
static gint            quesant_detect       (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*   quesant_load         (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Quesant file format."),
    "Jan Hořák <xhorak@gmail.com>, Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Jan Hořák",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("quesant",
                           N_("Quesant files (.afm)"),
                           (GwyFileDetectFunc)&quesant_detect,
                           (GwyFileLoadFunc)&quesant_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
quesant_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;
    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

typedef struct {
    guint32 desc_offset;       // offset of description (unused)
    guint32 date_offset;       // date location (unused)
    guint32 palette_offset;    // unknown 
    guint32 keys_offset;       // some sort of description again
    guint32 image_offset;      // offset of image data (first int16 
                               // is number of rows and cols)
    guint32 img_p_offset;      /* offset of Z axis multiply, this points to a
                                  relatively long block, the Z factor just
                                  happens to be there */
    guint32 hard_offset;       /* offset of X/Y axis width/height, this points
                                  to a relatively long block, the factors
                                  just happen to be there */
    guint32 short_desc_offset; // offset of short desc (unused)

    /* Read data */
    guint32 img_res;           // size of image
    gdouble real_size;         // physical size
    gdouble z_scale;           // z-scale factor
    const guint16 *image_data;
    gchar *title;
} FileInfo;

static const guchar*
get_param_pointer(const guchar *buffer, gsize size,
                  guint32 pos, guint32 len,
                  const guchar *name,
                  GError **error)
{
    if (pos < HEADER_SIZE || pos > size - len) {
        err_INVALID(error, name);
        return NULL;
    }
    return buffer + pos;
}

static gboolean
read_file_info(const guchar *buffer, gsize size,
               FileInfo *info,
               GError **error)
{
    guint expected_size, i;
    const guchar *p;

    gwy_clear(info, 1);
    p = buffer + MAGIC_SIZE;
    /* read structure variables from buffer */
    for (i = 0; i < (HEADER_SIZE - MAGIC_SIZE)/8; i++) {
        gchar key[5];
        guint32 value;

        key[4] = '\0';
        memcpy(key, p, 4);
        p += 4;
        value = gwy_get_guint32_le(&p);
        if (!key[0])
            continue;

        gwy_debug("%s: 0x%04x", key, value);

        /* Do not take values past the end of file into account at all. */
        if (value >= size)
            continue;

        else if (gwy_strequal(key, "DESC"))
            info->desc_offset = value;
        else if (gwy_strequal(key, "DATE"))
            info->date_offset = value;
        else if (gwy_strequal(key, "PLET"))
            info->palette_offset = value;
        else if (gwy_strequal(key, "IMAG"))
            info->image_offset = value;
        else if (gwy_strequal(key, "HARD"))
            info->hard_offset = value;
        else if (gwy_strequal(key, "IMGP"))
            info->img_p_offset = value;
        else if (gwy_strequal(key, "SDES"))
            info->short_desc_offset = value;
        else if (gwy_strequal(key, "KEYS"))
            info->keys_offset = value;
        else {
            gwy_debug("Unknown field %s", key);
        }
    }

    /* Pixel image size */
    if (!(p = get_param_pointer(buffer, size,
                                info->image_offset, sizeof(guint16),
                                "IMAG", error)))
        return FALSE;
    info->img_res = gwy_get_guint16_le(&p);
    if (err_DIMENSION(error, info->img_res))
        return FALSE;

    /* Image data.  It is the *same* pointer, just after the pixel size.  */
    info->image_data = (const guint16*)p;
    expected_size = (p - buffer) + info->img_res*info->img_res*sizeof(guint16);
    if (err_SIZE_MISMATCH(error, expected_size, size, FALSE))
        return FALSE;

    /* Real image size */
    if (!(p = get_param_pointer(buffer, size,
                                info->hard_offset, sizeof(gfloat),
                                "HARD", error)))
        return FALSE;
    info->real_size = gwy_get_gfloat_le(&p);
    if (!((info->real_size = fabs(info->real_size)) > 0)) {
        g_warning("Real size is 0.0, fixing to 1.0");
        info->real_size = 1.0;
    }

    /* Value scale factor */
    if (!(p = get_param_pointer(buffer, size,
                                info->img_p_offset + 8, sizeof(gfloat),
                                "IMGP", error)))
        return FALSE;
    info->z_scale = gwy_get_gfloat_le(&p);

    return TRUE;
}


static GwyContainer*
quesant_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwySIUnit *units = NULL;
    gint32 power10;
    FileInfo info;
    int row, col;
    GwyDataField *dfield;
    gdouble multiplier;
    const guint16 *p;
    gdouble *d;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size <= HEADER_SIZE) {
        gwy_file_abandon_contents(buffer, size, NULL);
        err_TOO_SHORT(error);
        return NULL;
    }

    if (!read_file_info(buffer, size, &info, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    // create units and datafield
    units = gwy_si_unit_new_parse("um", &power10);
    dfield = gwy_data_field_new(info.img_res, info.img_res, 
                                info.real_size * pow10(power10),
                                info.real_size * pow10(power10),
                                FALSE);
    // set units for XY axes
    gwy_data_field_set_si_unit_xy(dfield, units);
    g_object_unref(units);
    // set units for Z axis
    units = gwy_si_unit_new_parse("um", &power10);
    gwy_data_field_set_si_unit_z(dfield, units);
    g_object_unref(units);

    multiplier = info.z_scale * pow10(power10);
    p = info.image_data;
    d = gwy_data_field_get_data(dfield);
    // values are stored in unsigned int16 type
    for (row = 0; row < info.img_res; row++) {
        for (col = info.img_res-1; col >= 0; col--) {
            d[row*info.img_res + col] = GUINT16_FROM_LE(*p) * multiplier;
            p++;
        }
    }

    // create container
    container = gwy_container_new();
    // put datafield into container
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_app_channel_title_fall_back(container, 0);

    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
