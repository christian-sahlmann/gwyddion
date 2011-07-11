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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*
 *  The list of HDF tags is covered by the following license (it was taken
 *  from libHDF4 header files, mainly due to me being too lazy to retype them),
 *  the rest of the HDF code was implementated from scratch:
 *
 *  Copyright Notice and Statement for NCSA Hierarchical Data Format (HDF)
 *  Software Library and Utilities
 *
 *  Copyright 1988-2005 The Board of Trustees of the University of Illinois
 *
 *  All rights reserved.
 *
 *  Contributors:   National Center for Supercomputing Applications
 *  (NCSA) at the University of Illinois, Fortner Software, Unidata
 *  Program Center (netCDF), The Independent JPEG Group (JPEG),
 *  Jean-loup Gailly and Mark Adler (gzip), and Digital Equipment
 *  Corporation (DEC).
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted for any purpose (including commercial
 *  purposes) provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions, and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions, and the following disclaimer in the
 *  documentation and/or materials provided with the distribution.
 *
 *  3. In addition, redistributions of modified forms of the source or
 *  binary code must carry prominent notices stating that the original
 *  code was changed and the date of the change.
 *
 *  4. All publications or advertising materials mentioning features or use
 *  of this software are asked, but not required, to acknowledge that it was
 *  developed by the National Center for Supercomputing Applications at the
 *  University of Illinois at Urbana-Champaign and credit the contributors.
 *
 *  5. Neither the name of the University nor the names of the Contributors
 *  may be used to endorse or promote products derived from this software
 *  without specific prior written permission from the University or the
 *  Contributors.
 *
 *  DISCLAIMER
 *
 *  THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND THE CONTRIBUTORS "AS IS"
 *  WITH NO WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED.  In no event
 *  shall the University or the Contributors be liable for any damages
 *  suffered by the users arising out of the use of this software, even if
 *  advised of the possibility of such damage.
 */

/**
 * XXX: Disabled, it conflicts with MIME types installed by other packages
 * way too often.  (It is this comment that disables the magic, do not remove
 * it.)
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-hdf">
 *   <comment>Hierarchical Data Format</comment>
 *     <magic priority="10">
 *     <match type="string" offset="0" value="\x0e\x03\x13\x01"/>
 *   </magic>
 *   <glob pattern="*.hdf"/>
 *   <glob pattern="*.HDF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Psi HDF4
 * .hdf
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

#include "get.h"
#include "err.h"

#define MAGIC "\x0e\x03\x13\x01"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".hdf"

#define Micrometer (1e-6)

typedef enum {
    /* Common */
    HDF4_NULL         = 1,
    HDF4_LINKED       = 20,
    HDF4_VERSION      = 30,
    HDF4_COMPRESSED   = 40,
    HDF4_VLINKED      = 50,
    HDF4_VLINKED_DATA = 51,
    HDF4_CHUNKED      = 60,
    HDF4_CHUNK        = 61,

    /* Utility set */
    HDF4_FID  = 100,
    HDF4_FD   = 101,
    HDF4_TID  = 102,
    HDF4_TD   = 103,
    HDF4_DIL  = 104,
    HDF4_DIA  = 105,
    HDF4_NT   = 106,
    HDF4_MT   = 107,
    HDF4_FREE = 108,

    /* Raster-8 set, XXX unused */
    HDF4_ID8 = 200,
    HDF4_IP8 = 201,
    HDF4_RI8 = 202,
    HDF4_CI8 = 203,
    HDF4_II8 = 204,

    /* Raster image set, XXX unused */
    HDF4_ID   = 300,
    HDF4_LUT  = 301,
    HDF4_RI   = 302,
    HDF4_CI   = 303,
    HDF4_NRI  = 304,
    HDF4_RIG  = 306,
    HDF4_LD   = 307,
    HDF4_MD   = 308,
    HDF4_MA   = 309,
    HDF4_CCN  = 310,
    HDF4_CFM  = 311,
    HDF4_AR   = 312,
    HDF4_DRAW = 400,
    HDF4_RUN  = 401,
    HDF4_XYP  = 500,
    HDF4_MTO  = 501,

    /* Tektronix, XXX unused */
    HDF4_T14  = 602,
    HDF4_T105 = 603,

    /* Scientific/Numeric data sets */
    HDF4_SDG   = 700,
    HDF4_SDD   = 701,
    HDF4_SD    = 702,
    HDF4_SDS   = 703,
    HDF4_SDL   = 704,
    HDF4_SDU   = 705,
    HDF4_SDF   = 706,
    HDF4_SDM   = 707,
    HDF4_SDC   = 708,
    HDF4_SDT   = 709,
    HDF4_SDLNK = 710,
    HDF4_NDG   = 720,
    HDF4_CAL   = 731,
    HDF4_FV    = 732,
    HDF4_BREQ  = 799,
    HDF4_SDRAG = 781,
    HDF4_EREQ  = 780,

    /* VGroups */
    HDF4_VG = 1965,
    HDF4_VH = 1962,
    HDF4_VS = 1963,

    /* Compression schemes */
    HDF4_RLE       = 11,
    HDF4_IMC       = 12,
    HDF4_IMCOMP    = 12,
    HDF4_JPEG      = 13,
    HDF4_GREYJPEG  = 14,
    HDF4_JPEG5     = 15,
    HDF4_GREYJPEG5 = 16,

    /* Psi */
    HDF4_PSI     = 0x8001,
    HDF4_PSIHD   = 0x8009,
    HDF4_PSISPEC = 0x800a,
} HDF4Tag;

/* Type info codes */
typedef enum {
    /* types */
    HDF4TI_HDF      = 0x00000000,   /* standard HDF format  */
    HDF4TI_NATIVE   = 0x00001000,   /* native format        */
    HDF4TI_CUSTOM   = 0x00002000,   /* custom format        */
    HDF4TI_LITEND   = 0x00004000,   /* Little Endian format */
    HDF4TI_MASK     = 0x00000fff,   /* format mask */

    HDF4TI_NONE     = 0,   /* indicates that number type not set */
    HDF4TI_UCHAR8   = 3,   /* 3 chosen for backward compatibility */
    HDF4TI_CHAR8    = 4,   /* 4 chosen for backward compatibility */
    HDF4TI_FLOAT32  = 5,
    HDF4TI_FLOAT64  = 6,
    HDF4TI_FLOAT128 = 7,   /* No current plans for support */
    HDF4TI_INT8     = 20,
    HDF4TI_UINT8    = 21,
    HDF4TI_INT16    = 22,
    HDF4TI_UINT16   = 23,
    HDF4TI_INT32    = 24,
    HDF4TI_UINT32   = 25,
    HDF4TI_INT64    = 26,
    HDF4TI_UINT64   = 27,
    HDF4TI_INT128   = 28,  /* No current plans for support */
    HDF4TI_UINT128  = 30,  /* No current plans for support */
    HDF4TI_CHAR16   = 42,  /* No current plans for support */
    HDF4TI_UCHAR16  = 43,  /* No current plans for support */
} HDF4TypeInfo;

/* Class info codes for int */
typedef enum {
    HDF4TC_MBO     = 1,   /* Motorola byte order 2's compl */
    HDF4TC_VBO     = 2,   /* Vax byte order 2's compl */
    HDF4TC_IBO     = 4,   /* Intel byte order 2's compl */
} HDF4IntTypeClass;

/* Class info codes for float */
typedef enum {
    HDF4TC_NONE       = 0,   /* indicates subclass is not set */
    HDF4TC_IEEE       = 1,   /* IEEE format */
    HDF4TC_VAX        = 2,   /* Vax format */
    HDF4TC_CRAY       = 3,   /* Cray format */
    HDF4TC_PC         = 4,   /* PC floats - flipped IEEE */
    HDF4TC_CONVEX     = 5,   /* CONVEX native format */
    HDF4TC_VP         = 6,   /* Fujitsu VP native format */
    HDF4TC_CRAYMPP    = 7,   /* Cray MPP format */
} HDF4FloatTypeClass;

/* Class info codes for char */
typedef enum {
    HDF4TC_BYTE    = 0,   /* bitwise/numeric field */
    HDF4TC_ASCII   = 1,   /* ASCII */
    HDF4TC_EBCDIC  = 5,   /* EBCDIC */
} HDF4CharTypeClass;

/* Array order */
typedef enum {
    HDF4_ORDER_FORTRAN = 1,   /* column major order */
    HDF4_ORDER_C       = 2,   /* row major order */
} HDF4ArrayOrder;

/* Miscellaneous sizes */
enum {
    HDF4_DDH_SIZE     = 2 + 4,
    HDF4_DD_SIZE      = 2 + 2 + 4 + 4,
    HDF4_TAGREF_SIZE  = 2 + 2,
    HDF4_VERSION_SIZE = 3*4 + 1,
    PSI_ID_LEN        = 28,
    PSI_HD_LEN        = 32,
    PSI_HD_SIZE       = 202,
};

typedef struct {
    HDF4Tag tag;
    guint32 ref;  /* in fact 16bit */
    guint32 offset;
    guint32 length;
    const guchar *data;
} HDF4DataDescriptor;

typedef struct {
    guint32 unknown1;
    gchar title[32];
    gchar instrument[8];
    gboolean x_dir;
    gboolean y_dir;
    gboolean show_offset;
    gboolean no_units;
    guint32 xres;
    guint32 yres;
    gchar unknown2[12];
    gdouble xscale;
    gdouble yscale;
    gdouble xoff;
    gdouble yoff;
    gdouble rotation;
    gdouble unknown3;
    gdouble lines_per_sec;
    gdouble set_point;
    gchar set_point_unit[8];
    gdouble sample_bias;
    gdouble tip_bias;
    gdouble zgain;
    gchar zgain_unit[8];
    gint32 min;
    gint32 max;
} PsiHeader;

static gboolean      module_register   (void);
static gint          psi_detect        (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* psi_load          (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static gboolean      psi_read_header   (const guchar *buffer,
                                        gsize size,
                                        PsiHeader *header,
                                        GError **error);
static GArray*       hdf4_read_tags    (const guchar *buffer,
                                        gsize size,
                                        GError **error);
static GwyContainer* psi_get_metadadata(const PsiHeader *header);

#ifdef DEBUG
static guint        get_data_type_size     (HDF4TypeInfo id,
                                            GError **error);
static HDF4TypeInfo map_number_type        (guint32 number_type);
static gchar*       hdf4_describe_tag      (const HDF4DataDescriptor *desc);
static gchar*       hdf4_describe_data_type(HDF4TypeInfo id);
#endif

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Hierarchical Data Format (HDF) files, version 4."),
    "Yeti <yeti@gwyddion.net>",
    "0.6",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    /*
    gwy_file_func_register("hdf4file",
                           N_("Hierarchical Data Format v4 (.hdf)"),
                           (GwyFileDetectFunc)&hdf_detect,
                           (GwyFileLoadFunc)&hdf_load,
                           NULL,
                           NULL);
                           */
    gwy_file_func_register("psifile",
                           N_("Psi HDF4 files (.hdf)"),
                           (GwyFileDetectFunc)&psi_detect,
                           (GwyFileLoadFunc)&psi_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
psi_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    gint score = 0;

    /* Only when explicitly asked for (we cannot save anyway yet) */
    if (only_name)
        return 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0) {
        guchar *buffer = NULL;
        gsize size;
        GArray *tags;
        guint i;

        if (!gwy_file_get_contents(fileinfo->name, &buffer, &size, NULL))
            return 0;

        tags = hdf4_read_tags(buffer, size, NULL);
        if (tags) {
            for (i = 0; i < tags->len; i++) {
                HDF4DataDescriptor *desc;

                desc = &g_array_index(tags, HDF4DataDescriptor, i);
                if (desc->tag == HDF4_PSIHD) {
                    score = 100;
                    break;
                }
            }
            g_array_free(tags, TRUE);
        }
        gwy_file_abandon_contents(buffer, size, NULL);

    }

    return score;
}

static GwyContainer*
psi_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    GError *err = NULL;
    PsiHeader *header;
    const gint16 *data;
    gsize size;
    GArray *tags;
    const guchar *p;
    guint i, data_len;
    gboolean fail;
    gdouble *d;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    tags = hdf4_read_tags(buffer, size, error);
    if (!tags) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    fail = FALSE;
    header = NULL;
    data = NULL;
    for (i = 0; i < tags->len; i++) {
        const HDF4DataDescriptor *desc;

        desc = &g_array_index(tags, HDF4DataDescriptor, i);
#ifdef DEBUG
        {
            gchar *s;

            s = hdf4_describe_tag(desc);
            gwy_debug("%s", s);
            g_free(s);
        }
#endif
        if (desc->tag == HDF4_MT)
            continue;
        if (desc->offset == 0xFFFFFFFFUL || desc->length == 0xFFFFFFFFUL)
            continue;

        p = desc->data;
        switch (desc->tag) {
            case HDF4_PSIHD:
            header = g_new0(PsiHeader, 1);
            fail = !psi_read_header(p, desc->length, header, error);
            break;

            case HDF4_SD:
            /* TODO: check size */
            data = (const guint16*)desc->data;
            data_len = desc->length;
            break;

            default:
            break;
        }

        if (fail)
            break;
    }

    if (header && data && !fail) {
        GwyContainer *meta;
        GwyDataField *dfield;
        GwySIUnit *siunit;
        gint power10;

        dfield = gwy_data_field_new(header->xres, header->yres,
                                    header->xscale * Micrometer,
                                    header->yscale * Micrometer,
                                    FALSE);

        d = gwy_data_field_get_data(dfield);
        for (i = 0; i < header->xres*header->yres; i++)
            d[i] = GINT16_FROM_LE(data[i]);

        siunit = gwy_si_unit_new("m");
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);

        siunit = gwy_si_unit_new_parse(header->zgain_unit, &power10);
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);

        gwy_data_field_multiply(dfield, header->zgain*pow10(power10));
        gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);

        if (header->title[0])
            gwy_container_set_string_by_name(container, "/0/data/title",
                                             g_strdup(header->title));

        meta = psi_get_metadadata(header);
        gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    g_array_free(tags, TRUE);
    g_free(header);

    if (!container && !fail)
        err_NO_DATA(error);

    return container;
}

static gboolean
psi_read_header(const guchar *buffer,
                gsize size,
                PsiHeader *header,
                GError **error)
{
    const guchar *p;

    if (size < PSI_HD_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("PSI binary header is too short."));
        return FALSE;
    }
    p = buffer;

    header->unknown1 = gwy_get_guint32_le(&p);
    get_CHARARRAY0(header->title, &p);
    gwy_debug("     title: <%s>", header->title);
    get_CHARARRAY0(header->instrument, &p);
    gwy_debug("     instrument: <%s>", header->instrument);
    header->x_dir = gwy_get_guint16_le(&p);
    header->y_dir = gwy_get_guint16_le(&p);
    gwy_debug("     x_dir: %d, y_dir: %d", header->x_dir, header->y_dir);
    header->show_offset = gwy_get_gboolean8(&p);
    header->no_units = gwy_get_gboolean8(&p);
    gwy_debug("     show_offset: %d, no_units: %d",
            header->show_offset, header->no_units);
    header->xres = gwy_get_guint16_le(&p);
    header->yres = gwy_get_guint16_le(&p);
    gwy_debug("     xres: %u, yres: %u", header->xres, header->yres);
    if (err_DIMENSION(error, header->xres)
        || err_DIMENSION(error, header->yres))
        return FALSE;

    get_CHARARRAY(header->unknown2, &p);
    header->xscale = gwy_get_gfloat_le(&p);
    header->yscale = gwy_get_gfloat_le(&p);
    gwy_debug("     xscale: %g, yscale: %g", header->xscale, header->yscale);
    /* Use negated positive conditions to catch NaNs */
    if (!((header->xscale = fabs(header->xscale)) > 0)) {
        g_warning("Real x scale is 0.0, fixing to 1.0");
        header->xscale = 1.0;
    }
    if (!((header->yscale = fabs(header->yscale)) > 0)) {
        g_warning("Real y scale is 0.0, fixing to 1.0");
        header->yscale = 1.0;
    }

    header->xoff = gwy_get_gfloat_le(&p);
    header->yoff = gwy_get_gfloat_le(&p);
    gwy_debug("     xoff: %g, yoff: %g", header->xoff, header->yoff);
    header->rotation = gwy_get_gfloat_le(&p);
    header->unknown3 = gwy_get_gfloat_le(&p);
    header->lines_per_sec = gwy_get_gfloat_le(&p);
    gwy_debug("     rotation: %g, lines/s: %g",
            header->rotation, header->lines_per_sec);
    header->set_point = gwy_get_gfloat_le(&p);
    get_CHARARRAY0(header->set_point_unit, &p);
    gwy_debug("     set_point: %g [%s]",
            header->set_point, header->set_point_unit);
    header->sample_bias = gwy_get_gfloat_le(&p);
    header->tip_bias = gwy_get_gfloat_le(&p);
    gwy_debug("     sample_bias: %g [V], tip_bias %g [V]",
            header->sample_bias, header->tip_bias);
    header->zgain = gwy_get_gfloat_le(&p);
    get_CHARARRAY0(header->zgain_unit, &p);
    gwy_debug("     zgain: %g [%s]",
            header->zgain, header->zgain_unit);
    header->min = gwy_get_gint16_le(&p);
    header->max = gwy_get_gint16_le(&p);

    return TRUE;
}

static GwyContainer*
psi_get_metadadata(const PsiHeader *header)
{
    GwyContainer *meta;

    meta = gwy_container_new();

    gwy_container_set_string_by_name(meta, "Set point",
                                     g_strdup_printf("%g %s",
                                                     header->set_point,
                                                     header->set_point_unit));
    gwy_container_set_string_by_name(meta, "Sample bias",
                                     g_strdup_printf("%g V",
                                                     header->sample_bias));
    gwy_container_set_string_by_name(meta, "Tip bias",
                                     g_strdup_printf("%g V",
                                                     header->tip_bias));
    gwy_container_set_string_by_name(meta, "Instrument",
                                     g_strdup_printf("%s",
                                                     header->instrument));
    gwy_container_set_string_by_name(meta, "Rotation",
                                     g_strdup_printf("%g deg",
                                                     header->rotation));
    gwy_container_set_string_by_name(meta, "Scan speed",
                                     g_strdup_printf("%g lines/s",
                                                     header->lines_per_sec));

    return meta;
}

static GArray*
hdf4_read_tags(const guchar *buffer,
               gsize size,
               GError **error)
{
    GArray *tags;
    const guchar *p;
    guint ddb_size, ddb_offset, i;

    g_return_val_if_fail(size >= MAGIC_SIZE, NULL);

    tags = g_array_new(FALSE, FALSE, sizeof(HDF4DataDescriptor));
    ddb_offset = MAGIC_SIZE;
    do {
        gwy_debug("Reading DDB at 0x%x", ddb_offset);
        p = buffer + ddb_offset;
        if ((gulong)(p - buffer) + HDF4_DDH_SIZE > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Data descriptor header is truncated."));
            g_array_free(tags, TRUE);
            return NULL;
        }
        ddb_size = gwy_get_guint16_be(&p);
        ddb_offset = gwy_get_guint32_be(&p);
        gwy_debug("DDB size: %u, next offset: 0x%x", ddb_size, ddb_offset);
        if ((gulong)(p - buffer) + ddb_size * HDF4_DD_SIZE > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Data descriptor block is truncated."));
            g_array_free(tags, TRUE);
            return NULL;
        }

        for (i = 0; i < ddb_size; i++) {
            HDF4DataDescriptor desc;

            desc.tag = gwy_get_guint16_be(&p);
            desc.ref = gwy_get_guint16_be(&p);
            desc.offset = gwy_get_guint32_be(&p);
            desc.length = gwy_get_guint32_be(&p);
            desc.data = buffer + desc.offset;
            /* Ignore NULL and invalid tags */
            if (desc.tag == HDF4_NULL
                || desc.offset == 0xFFFFFFFFUL
                || desc.length == 0xFFFFFFFFUL)
                continue;

            g_array_append_val(tags, desc);
        }
    } while (ddb_offset);

    return tags;
}

#ifdef DEBUG
static guint
get_data_type_size(HDF4TypeInfo id,
                   GError **error)
{
    /* Native data format is Evil. */
    if (id & HDF4TI_NATIVE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    N_("HDF data with `native' type is not supported."));
        return 0;
    }

    switch (id & ~(HDF4TI_NATIVE | HDF4TI_LITEND)) {
        case HDF4TI_CHAR8:
        case HDF4TI_UCHAR8:
        case HDF4TI_INT8:
        case HDF4TI_UINT8:
        return 1;
        break;

        case HDF4TI_INT16:
        case HDF4TI_UINT16:
        return 2;
        break;

        case HDF4TI_INT32:
        case HDF4TI_UINT32:
        case HDF4TI_FLOAT32:
        return 4;
        break;

        case HDF4TI_INT64:
        case HDF4TI_UINT64:
        case HDF4TI_FLOAT64:
        return 8;
        break;

        default:
        err_DATA_TYPE(error, id);
        return 0;
        break;
    }
}

static HDF4TypeInfo
map_number_type(guint32 number_type)
{
    HDF4TypeInfo typeinfo;
    guint version, bits, expected_bits, flags;

    version = number_type >> 24;
    typeinfo = (number_type >> 16) & 0xff;
    bits = (number_type >> 8) & 0xff;
    flags = number_type & 0xff;

    if (version != 1)
        g_warning("Type information version %u is not 1", version);

    expected_bits = 8*get_data_type_size(typeinfo, NULL);
    if (!expected_bits)
        return 0;
    if (bits != expected_bits) {
        g_warning("Number of bits in type %u is %u instead of %u",
                  typeinfo, bits, expected_bits);
        return 0;
    }

    switch (typeinfo) {
        case HDF4TI_UCHAR8:
        case HDF4TI_CHAR8:
        if (flags != HDF4TC_ASCII && flags != HDF4TC_BYTE)
            g_warning("Unimplemented char class %u", flags);
        break;

        case HDF4TI_FLOAT32:
        case HDF4TI_FLOAT64:
        switch (flags) {
            case HDF4TC_IEEE:
            break;

            case HDF4TC_PC:
            typeinfo |= HDF4TI_LITEND;
            break;

            default:
            g_warning("Unimplemented float class %u", flags);
            break;
        }
        break;

        case HDF4TI_INT8:
        case HDF4TI_UINT8:
        case HDF4TI_INT16:
        case HDF4TI_UINT16:
        case HDF4TI_INT32:
        case HDF4TI_UINT32:
        case HDF4TI_INT64:
        case HDF4TI_UINT64:
        case HDF4TI_INT128:
        case HDF4TI_UINT128:
        switch (flags) {
           case HDF4TC_MBO:
           break;

           case HDF4TC_VBO:
           case HDF4TC_IBO:
           typeinfo |= HDF4TI_LITEND;
           break;

           default:
           g_warning("Unimplemented int class %u", flags);
           break;
        }
        break;

        default:
        g_warning("Unimplemented type %u", typeinfo);
        return 0;
        break;
    }

    return typeinfo;
}

static const GwyEnum tag_names[] = {
    /* Common */
    { "No data", HDF4_NULL, },
    { "Linked blocks indicator", HDF4_LINKED, },
    { "Version descriptor", HDF4_VERSION, },
    { "Compressed data indicator", HDF4_COMPRESSED, },
    { "Data chunk", HDF4_CHUNK, },

    /* Utility set */
    { "File identifier", HDF4_FID, },
    { "File description", HDF4_FD, },
    { "Tag identifier", HDF4_TID, },
    { "Tag description", HDF4_TD, },
    { "Data id label", HDF4_DIL, },
    { "Data id annotation", HDF4_DIA, },
    { "Number type", HDF4_NT, },
    { "Machine type", HDF4_MT, },
    { "Free space", HDF4_FREE, },

    /* Raster-8 set, XXX unused */
    { "Image dimensions-8", HDF4_ID8, },
    { "Image palette-8", HDF4_IP8, },
    { "Raster image-8", HDF4_RI8, },
    { "RLE compressed image-8", HDF4_CI8, },
    { "Imcomp image-8", HDF4_II8, },

    /* Raster image set, XXX unused */
    { "Image dimensions", HDF4_ID, },
    { "Image palette", HDF4_LUT, },
    { "Raster image data", HDF4_RI, },
    { "Compressed image", HDF4_CI, },
    { "Raster image group", HDF4_RIG, },
    { "Palette dimension", HDF4_LD, },
    { "Matte dimension", HDF4_MD, },
    { "Matte data", HDF4_MA, },
    { "Color correction", HDF4_CCN, },
    { "Color format", HDF4_CFM, },
    { "Aspect ratio", HDF4_AR, },
    { "Sequenced images", HDF4_DRAW, },
    { "Runable program/script", HDF4_RUN, },
    { "X-Y position", HDF4_XYP, },
    { "M/c-type override", HDF4_MTO, },

    /* Tektronix, XXX unused */
    { "TEK 4014 data", HDF4_T14, },
    { "TEK 4105 data", HDF4_T105, },

    /* Scientific/Numeric data sets */
    { "Scientific data group", HDF4_SDG, },
    { "Scientific data dimension record", HDF4_SDD, },
    { "Scientific data", HDF4_SD, },
    { "Scientific data scales", HDF4_SDS, },
    { "Scientific data labels", HDF4_SDL, },
    { "Scientific data units", HDF4_SDU, },
    { "Scientific data formats", HDF4_SDF, },
    { "Scientific data max/min", HDF4_SDM, },
    { "Scientific data coordinate system", HDF4_SDC, },
    { "Transpose", HDF4_SDT, },
    { "Links related to the dataset", HDF4_SDLNK, },
    { "Numeric data group", HDF4_NDG, },
    { "Calibration information", HDF4_CAL, },
    { "Fill value information", HDF4_FV, },

    /* VGroups */
    { "Vgroup", HDF4_VG, },
    { "Vdata", HDF4_VH, },
    { "Vdata storage", HDF4_VS, },

    /* Compression Schemes */
    { "Run length encoding", HDF4_RLE, },
    { "IMCOMP encoding", HDF4_IMCOMP, },
    { "24-bit JPEG encoding", HDF4_JPEG, },
    { "8-bit JPEG encoding", HDF4_GREYJPEG, },
    { "24-bit JPEG encoding", HDF4_JPEG5, },
    { "8-bit JPEG encoding", HDF4_GREYJPEG5, },

    /* Psi */
    { "PSI ProScan version", HDF4_PSI, },
    { "PSI binary header", HDF4_PSIHD, },
    { "PSI spectroscopy", HDF4_PSISPEC, },
};

static gchar*
hdf4_describe_tag(const HDF4DataDescriptor *desc)
{
    GString *str;
    const guchar *p, *q;
    const gchar *name;
    gchar *s;
    guint i;

    str = g_string_new(NULL);
    if (desc->tag == HDF4_NULL)
        goto finish;

    name = gwy_enum_to_string(desc->tag, tag_names, G_N_ELEMENTS(tag_names));
    g_string_append_printf(str,
                           "Tag: %u (%s), Ref: %u, Offset: 0x%x, Length: 0x%x;",
                           desc->tag, name ? name : "UNKNOWN", desc->ref,
                           desc->offset, desc->length);

    if (desc->tag == HDF4_MT) {
        /* FIXME */
        goto finish;
    }

    if (desc->offset == 0xFFFFFFFFUL || desc->length == 0xFFFFFFFFUL) {
        g_string_append(str, " INVALID");
        goto finish;
    }

    p = desc->data;
    switch (desc->tag) {
        case HDF4_TID:
        case HDF4_TD:
        case HDF4_FID:
        case HDF4_FD:
        case HDF4_SDF:
        case HDF4_SDC:
        g_string_append_printf(str, " \"%.*s\"", desc->length, p);
        break;

        /* FIXME: Psi simply stores three strings in a row.  It does not
         * seem to be accessible via libHDF SD API though, so I wonder how
         * big hack it is. */
        case HDF4_SDL:
        case HDF4_SDU:
        {
            i = desc->length;
            do {
                g_string_append_printf(str, " \"%.*s\"", i, p);
                q = memchr(p, 0, i);
                i -= (q - p + 1);
                p = q+1;
            } while (q && i);
        }

        case HDF4_SDG:
        case HDF4_NDG:
        case HDF4_RIG:
        case HDF4_VG:
        for (i = 0; i < desc->length/HDF4_TAGREF_SIZE; i++) {
            guint tag, ref;

            tag = gwy_get_guint16_be(&p);
            ref = gwy_get_guint16_be(&p);
            g_string_append_printf(str, " (%u, %u)", tag, ref);
        }
        break;

        case HDF4_VERSION:
        if (desc->length >= 3*4 + 1) {
            guint major, minor, micro;

            major = gwy_get_guint32_be(&p);
            minor = gwy_get_guint32_be(&p);
            micro = gwy_get_guint32_be(&p);
            g_string_append_printf(str, " %u.%u.%u \"%s\"",
                                   major, minor, micro, p);
        }
        break;

        case HDF4_NT:
        if (desc->length == 4) {
            HDF4TypeInfo id;

            id = gwy_get_guint32_be(&p);
            id = map_number_type(id);
            s = hdf4_describe_data_type(id);
            g_string_append_c(str, ' ');
            g_string_append(str, s);
            g_free(s);
        }
        break;

        case HDF4_SDM:
        if (desc->length == 8) {
            /* XXX: depends on data type */
            guint min, max;

            min = gwy_get_guint16_be(&p);
            max = gwy_get_guint16_be(&p);
            g_string_append_printf(str, " %u %u", min, max);
        }
        break;

        case HDF4_SD:
        /* Nothing interesting to print */
        break;

        case HDF4_PSI:
        if (desc->length == 4) {
            guint32 version;

            version = gwy_get_guint32_be(&p);
            g_string_append_printf(str, " %x", version);
        }
        break;

        case HDF4_PSIHD:
        {
            PsiHeader psi_header;

            psi_read_header(p, desc->length, &psi_header, NULL);
        }
        break;

        default:
        g_string_append(str, " (UNHANDLED)");
        break;
    }

finish:
    s = str->str;
    g_string_free(str, FALSE);

    return s;
}

static struct {
    HDF4TypeInfo id;
    const gchar *desc;
}
const data_types[] = {
    { HDF4TI_CHAR8,   "signed 8-bit character"   },
    { HDF4TI_UCHAR8,  "unsigned 8-bit character" },
    { HDF4TI_INT8,    "signed 8-bit integer"     },
    { HDF4TI_UINT8,   "unsigned 8-bit integer"   },
    { HDF4TI_INT16,   "signed 16-bit integer"    },
    { HDF4TI_UINT16,  "unsigned 16-bit integer"  },
    { HDF4TI_INT32,   "signed 32-bit integer"    },
    { HDF4TI_UINT32,  "unsigned 32-bit integer"  },
    { HDF4TI_INT64,   "signed 64-bit integer"    },
    { HDF4TI_UINT64,  "unsigned 64-bit integer"  },
    { HDF4TI_FLOAT32, "single precision float"   },
    { HDF4TI_FLOAT64, "double precision float"   },
};

static gchar*
hdf4_describe_data_type(HDF4TypeInfo id)
{
    guint i;
    HDF4TypeInfo baseid;

    baseid = id & HDF4TI_MASK;
    for (i = 0; i < sizeof(data_types)/sizeof(data_types[0]); i++) {
        if (baseid == data_types[i].id)
            return g_strdup_printf("%s%s%s",
                                   (id & HDF4TI_NATIVE) ? "native " : "",
                                   (id & HDF4TI_LITEND) ? "little endian " : "",
                                   data_types[i].desc);
    }

    return g_strdup("UNKNOWN");
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

