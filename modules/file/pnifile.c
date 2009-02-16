/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-pni-spm">
 *   <comment>PNI SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\0\0\0\0001.0"/>
 *     <match type="string" offset="0" value="\315\315\315\3151.0"/>
 *   </magic>
 *   <glob pattern="*.pni"/>
 *   <glob pattern="*.PNI"/>
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

#define EXTENSION ".pni"

#define MAGIC1 "\0\0\0\0" "1.0"
#define MAGIC2 "\315\315\315\315" "1.0"
#define MAGIC_SIZE (sizeof(MAGIC1)-1)

#define Nanometer (1e-9)
#define Micrometer (1e-6)
#define Milivolt (1e-3)

enum {
    /* Relative to DATA_HEADER_START */
    DATA_NUMBER_OFFSET = 0x0006,
    DATA_TYPE_OFFSET   = 0x000a,
    DIRECTION_OFFSET   = 0x000e,
    RESOLUTION_OFFSET  = 0x001e,
    VALUE_TYPE_OFFSET  = 0x0046,
    VALUE_SCALE_OFFSET = 0x004a,
    REAL_SIZE_OFFSET   = 0x0052,

    /* Absolute in file */
    HEADER_START       = 0x0090,
    /* Palette is 3x256 8bit r,g,b components. */
    PALETTE_START      = 0x00ca,
    /* Thumbnail is 64x64, 8 bits per sample */
    THUMB_START        = 0x03ca,
    DATA_HEADER_START  = 0x13ca,
    /* Data is 16 bits per sample */
    DATA_START         = 0x1c90
};

typedef enum {
    DIRECTION_FORWARD = 0,
    DIRECTION_REVERSE = 1
} PNIDirection;

typedef enum {
    DATA_TYPE_HGT = 1,
    DATA_TYPE_L_R = 2,
    DATA_TYPE_SEN = 3,
    DATA_TYPE_DEM = 6,
    DATA_TYPE_ERR = 8
} PNIDataType;

typedef enum {
    VALUE_TYPE_NM = 1,
    VALUE_TYPE_MV = 4
} PNIValueType;

static gboolean      module_register(void);
static gint          pni_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* pni_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Pacific Nanotechnology PNI data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("pnifile",
                           N_("PNI files (.pni)"),
                           (GwyFileDetectFunc)&pni_detect,
                           (GwyFileLoadFunc)&pni_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
pni_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    gint score = 0;
    guint32 xres, yres;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len >= 0xa0
        && (memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE) == 0
            || memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE) == 0)) {
        const guchar *p = fileinfo->head + 0x90;

        xres = gwy_get_guint32_le(&p);
        yres = gwy_get_guint32_le(&p);
        gwy_debug("%u %u", xres, yres);
        if (fileinfo->file_size == DATA_START + 2*xres*yres)
            score = 95;
    }

    return score;
}

static GwyContainer*
pni_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    static const GwyEnum titles[] = {
        { N_("Height"), DATA_TYPE_HGT, },
        { N_("Sens"),   DATA_TYPE_SEN, },
        { N_("Dem"),    DATA_TYPE_DEM, },
        { N_("Error"),  DATA_TYPE_ERR, },
        { N_("L-R"),    DATA_TYPE_L_R, },
    };

    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    const guchar *p;
    gint i, xres, yres;
    PNIDataType data_type;
    PNIValueType value_type;
    PNIDirection direction;
    gdouble xreal, yreal, zscale;
    gdouble *data;
    const gint16 *d16;
    GwySIUnit *siunit;
    const gchar *title;
    gchar *s;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < DATA_START + 2) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer + DATA_HEADER_START + RESOLUTION_OFFSET;
    xres = gwy_get_guint32_le(&p);
    yres = gwy_get_guint32_le(&p);
    gwy_debug("%d %d", xres, yres);
    if (err_DIMENSION(error, xres)
        || err_DIMENSION(error, yres)
        || err_SIZE_MISMATCH(error, DATA_START + 2*xres*yres, size, TRUE)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    p = buffer + DATA_HEADER_START;
    data_type = p[DATA_TYPE_OFFSET];
    value_type = p[VALUE_TYPE_OFFSET];
    direction = p[DIRECTION_OFFSET];

    p = buffer + DATA_HEADER_START + REAL_SIZE_OFFSET;
    xreal = gwy_get_gfloat_le(&p);
    yreal = gwy_get_gfloat_le(&p);
    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }
    xreal *= Micrometer;
    yreal *= Micrometer;

    p = buffer + DATA_HEADER_START + VALUE_SCALE_OFFSET;
    zscale = gwy_get_gfloat_le(&p);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    d16 = (const gint16*)(buffer + DATA_START);
    for (i = 0; i < xres*yres; i++)
        data[i] = zscale*GINT16_FROM_LE(d16[i])/65536.0;

    gwy_file_abandon_contents(buffer, size, NULL);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    switch (value_type) {
        case VALUE_TYPE_NM:
        siunit = gwy_si_unit_new("m");
        gwy_data_field_multiply(dfield, Nanometer);
        break;

        case VALUE_TYPE_MV:
        siunit = gwy_si_unit_new("V");
        gwy_data_field_multiply(dfield, Milivolt);
        break;

        default:
        g_warning("Value type %d is unknown", value_type);
        siunit = gwy_si_unit_new(NULL);
        break;
    }
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    title = gwy_enum_to_string(data_type, titles, G_N_ELEMENTS(titles));
    if (title) {
        s = g_strdup_printf("%s (%s)",
                            title,
                            direction ? "Forward" : "Backward");
        gwy_container_set_string_by_name(container, "/0/data/title", s);
    }
    else
        g_warning("Data type %d is unknown", data_type);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

