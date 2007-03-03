/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

/* The value of ISDF_TIFFTAG_FILEID */
#define ISDF_MAGIC_NUMBER 0x00534446

/* TIFF data types */
typedef enum {
    GWY_TIFF_NOTYPE    = 0,
    GWY_TIFF_BYTE      = 1,
    GWY_TIFF_ASCII     = 2,
    GWY_TIFF_SHORT     = 3,
    GWY_TIFF_LONG      = 4,
    GWY_TIFF_RATIONAL  = 5,
    GWY_TIFF_SBYTE     = 6,
    GWY_TIFF_UNDEFINED = 7,
    GWY_TIFF_SSHORT    = 8,
    GWY_TIFF_SLONG     = 9,
    GWY_TIFF_SRATIONAL = 10,
    GWY_TIFF_FLOAT     = 11,
    GWY_TIFF_DOUBLE    = 12,
    GWY_TIFF_IFD       = 13
} GwyTIFFDataType;

/* Standard TIFF tags */
enum {
    ISDF_TIFFTAG_IMAGEDESCRIPTION = 270,
    ISDF_TIFFTAG_SOFTWARE         = 305,
    ISDF_TIFFTAG_DATETIME         = 306,
} GwyTIFFTag;

/* Custom TIFF tags */
enum {
    ISDF_TIFFTAG_FILEID = 65000,
    ISDF_TIFFTAG_FILETYPE,
    ISDF_TIFFTAG_DATATYPE,
    ISDF_TIFFTAG_FILEINFO,
    ISDF_TIFFTAG_USERINFO,
    ISDF_TIFFTAG_DATARANGE,
    ISDF_TIFFTAG_SPMDATA,
    ISDF_TIFFTAG_DATACNVT,
    ISDF_TIFFTAG_DATAUNIT,
    ISDF_TIFFTAG_IMGXDIM,
    ISDF_TIFFTAG_IMGYDIM,
    ISDF_TIFFTAG_IMGZDIM,
    ISDF_TIFFTAG_XDIMUNIT,
    ISDF_TIFFTAG_YDIMUNIT,
    ISDF_TIFFTAG_ZDIMUNIT,
    ISDF_TIFFTAG_SPMDATAPOS,
    ISDF_TIFFTAG_IMAGEDEPTH,
    ISDF_TIFFTAG_SAMPLEINFO,
    ISDF_TIFFTAG_SCANRATE,
    ISDF_TIFFTAG_BIASVOLTS,
    ISDF_TIFFTAG_ZSERVO,
    ISDF_TIFFTAG_ZSERVOREF,
    ISDF_TIFFTAG_ZSERVOBW,
    ISDF_TIFFTAG_ZSERVOSP,
    ISDF_TIFFTAG_ZSERVOPID
};

typedef enum {
    ISDF_2D_MAPPED    = 0,
    ISDF_LINE_PROFILE = 1
} ISDFImageType;

typedef struct {
    guint xres;
    guint yres;
} ISDFImageHeader;

typedef struct {
    guint tag;
    GwyTIFFDataType type;
    guint count;
    guchar value[4];
} GwyTIFFEntry;

typedef struct {
    guchar *data;
    gsize size;
    GArray *tags;
    guint16 (*getu16)(const guchar **p);
    gint16 (*gets16)(const guchar **p);
    guint32 (*getu32)(const guchar **p);
    gint32 (*gets32)(const guchar **p);
    gfloat (*getflt)(const guchar **p);
    gdouble (*getdbl)(const guchar **p);
} GwyTIFF;

static gboolean      module_register       (void);
static gint          isdf_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* isdf_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static GwyContainer* isdf_get_metadata     (ISDFImageHeader *header);

/* TIFF reader */
static void                gwy_tiff_free     (GwyTIFF *tiff);
static GwyTIFF*            gwy_tiff_load     (const gchar *filename,
                                              GError **error);
static gboolean            gwy_tiff_load_real(GwyTIFF *tiff,
                                              const gchar *filename,
                                              GError **error);
static const GwyTIFFEntry* gwy_tiff_find_tag (GwyTIFF *tiff,
                                              guint tag);
static gboolean            gwy_tiff_get_sint (GwyTIFF *tiff,
                                              guint tag,
                                              gint *retval);
static gboolean            gwy_tiff_get_uint (GwyTIFF *tiff,
                                              guint tag,
                                              guint *retval);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Intematix SDF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("intematix",
                           N_("Intematix SDF data files (.sdf)"),
                           (GwyFileDetectFunc)&isdf_detect,
                           (GwyFileLoadFunc)&isdf_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
isdf_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))) {
        gint magic;

        if (gwy_tiff_get_sint(tiff, ISDF_TIFFTAG_FILEID, &magic)
            && magic == ISDF_MAGIC_NUMBER) {
            score = 100;
        }

        gwy_tiff_free(tiff);
    }

    return score;
}

static GwyContainer*
isdf_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    GwyTIFF *tiff;
    guint magic;

    if (!(tiff = gwy_tiff_load(filename, error)))
        return NULL;

    if (!gwy_tiff_get_sint(tiff, ISDF_TIFFTAG_FILEID, &magic)
        || magic != ISDF_MAGIC_NUMBER) {
        err_FILE_TYPE(error, "Intematix SDF");
        gwy_tiff_free(tiff);
        return NULL;
    }

    gwy_tiff_free(tiff);
    err_NO_DATA(error);

    return container;
}

#if 0
static GwyContainer*
isdf_load_tiff(TIFF *tiff, GError **error)
{
    ISDFImageHeader header;
    GwyContainer *container = NULL;
    GwyContainer *meta = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    guint magic, version, i, j;
    const guchar *p;
    const guint16 *data;
    const gchar *comment = NULL;
    gint count, data_len, power10;
    gdouble q, z0;
    gdouble *d;

    if (!tiff_get_custom_uint(tiff, ISDF_TIFFTAG_FILEID, &magic)
        || magic != ISDF_MAGIC_NUMBER) {
        err_FILE_TYPE(error, "Intematix SDF");
        return NULL;
    }

    err_NO_DATA(error);
    return NULL;
}


    if (!TIFFGetField(tiff, ISDF_TIFFTAG_Header, &count, &p)) {
        err_FILE_TYPE(error, "Intematix SDF");
        return NULL;
    }
    gwy_debug("[Header] count: %d", count);

    if (count < 356) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header is too short (only %d bytes)."),
                    count);
        return NULL;
    }

    memset(&header, 0, sizeof(ISDFImageHeader));
    header.image_type = gwy_get_guint32_le(&p);
    gwy_debug("image_type: %d", header.image_type);
    if (header.image_type != ISDF_2D_MAPPED) {
        err_NO_DATA(error);
        return NULL;
    }
    header.source_name = isdf_wchar_to_utf8(&p, 32);
    header.image_mode = isdf_wchar_to_utf8(&p, 8);
    gwy_debug("source_name: <%s>, image_mode: <%s>",
              header.source_name, header.image_mode);
    header.lpf_strength = gwy_get_gdouble_le(&p);
    header.auto_flatten = gwy_get_guint32_le(&p);
    header.ac_track = gwy_get_guint32_le(&p);
    header.xres = gwy_get_guint32_le(&p);
    header.yres = gwy_get_guint32_le(&p);
    gwy_debug("xres: %d, yres: %d", header.xres, header.yres);
    header.angle = gwy_get_gdouble_le(&p);
    header.sine_scan = gwy_get_guint32_le(&p);
    header.overscan_rate = gwy_get_gdouble_le(&p);
    header.forward = gwy_get_guint32_le(&p);
    header.scan_up = gwy_get_guint32_le(&p);
    header.swap_xy = gwy_get_guint32_le(&p);
    header.xreal = gwy_get_gdouble_le(&p) * 1e-6;
    header.yreal = gwy_get_gdouble_le(&p) * 1e-6;
    gwy_debug("xreal: %g, yreal: %g", header.xreal, header.yreal);
    header.xoff = gwy_get_gdouble_le(&p) * 1e-6;
    header.yoff = gwy_get_gdouble_le(&p) * 1e-6;
    gwy_debug("xoff: %g, yoff: %g", header.xoff, header.yoff);
    header.scan_rate = gwy_get_gdouble_le(&p);
    header.set_point = gwy_get_gdouble_le(&p);
    header.set_point_unit = isdf_wchar_to_utf8(&p, 8);
    if (!header.set_point_unit)
        header.set_point_unit = g_strdup("V");
    header.tip_bias = gwy_get_gdouble_le(&p);
    header.sample_bias = gwy_get_gdouble_le(&p);
    header.data_gain = gwy_get_gdouble_le(&p);
    header.z_scale = gwy_get_gdouble_le(&p);
    header.z_offset = gwy_get_gdouble_le(&p);
    gwy_debug("data_gain: %g, z_scale: %g", header.data_gain, header.z_scale);
    header.z_unit = isdf_wchar_to_utf8(&p, 8);
    gwy_debug("z_unit: <%s>", header.z_unit);
    header.data_min = gwy_get_gint32_le(&p);
    header.data_max = gwy_get_gint32_le(&p);
    header.data_avg = gwy_get_gint32_le(&p);
    header.compression = gwy_get_guint32_le(&p);

    tiff_get_custom_string(tiff, ISDF_TIFFTAG_Comments, &comment);
    if (comment) {
        gwy_debug("comment: <%s>", comment);
    }

    if (!TIFFGetField(tiff, ISDF_TIFFTAG_Data, &data_len, &data)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data tag is missing."));
        isdf_free_image_header(&header);
        return NULL;
    }
    /* FIXME: This is always a totally bogus value, although tiffdump(1) can
     * print the right size. Why? */
    gwy_debug("data_len: %d", data_len);

    dfield = gwy_data_field_new(header.xres, header.yres,
                                header.xreal, header.yreal,
                                FALSE);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    if (header.z_unit)
        siunit = gwy_si_unit_new_parse(header.z_unit, &power10);
    else {
        g_warning("Z units are missing");
        siunit = gwy_si_unit_new_parse("um", &power10);
    }
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    if (header.z_scale == 0.0)
        header.z_scale = 1.0;
    z0 = header.z_offset;
    q = pow10(power10)*header.data_gain;
    for (i = 0; i < header.yres; i++) {
        d = gwy_data_field_get_data(dfield) + (header.yres-1 - i)*header.xres;
        for (j = 0; j < header.xres; j++)
            d[j] = q*(GINT16_FROM_LE(data[i*header.xres + j])*header.z_scale
                      + z0);
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    if (header.source_name && *header.source_name)
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(header.source_name));

    meta = isdf_get_metadata(&header);
    if (comment && *comment) {
        /* FIXME: Charset conversion. But from what? */
        gwy_container_set_string_by_name(meta, "Comment", g_strdup(comment));
        comment = NULL;
    }
    gwy_container_set_string_by_name(meta, "Version",
                                     g_strdup_printf("%08x", version));

    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    isdf_free_image_header(&header);

    return container;
}

static void
isdf_free_image_header(ISDFImageHeader *header)
{
    g_free(header->source_name);
    g_free(header->image_mode);
    g_free(header->set_point_unit);
    g_free(header->z_unit);
}

static gchar*
isdf_wchar_to_utf8(const guchar **src,
                   guint len)
{
    gchar *s;
    gunichar2 *wstr;
    guint i;

    wstr = g_memdup(*src, 2*len);
    for (i = 0; i < len; i++)
        wstr[i] = GUINT16_FROM_LE(wstr[i]);
    s = g_utf16_to_utf8(wstr, len, NULL, NULL, NULL);
    g_free(wstr);
    *src += 2*len;

    return s;
}
#endif

#if 0
static GwyContainer*
isdf_get_metadata(ISDFImageHeader *header)
{
    GwyContainer *meta;

    meta = gwy_container_new();

    if (header->source_name && *header->source_name) {
        gwy_container_set_string_by_name(meta, "Source name",
                                         header->source_name);
        header->source_name = NULL;
    }
    if (header->image_mode && *header->image_mode) {
        gwy_container_set_string_by_name(meta, "Image mode",
                                         header->image_mode);
        header->image_mode = NULL;
    }

    gwy_container_set_string_by_name(meta, "Fast direction",
                                     g_strdup(header->swap_xy ? "Y" : "X"));
    gwy_container_set_string_by_name(meta, "Angle",
                                     g_strdup_printf("%g°", header->angle));
    gwy_container_set_string_by_name(meta, "Scanning direction",
                                     g_strdup(header->scan_up
                                              ? "Bottom to top"
                                              : "Top to bottom"));
    gwy_container_set_string_by_name(meta, "Line direction",
                                     g_strdup(header->forward
                                              ? "Forward"
                                              : "Backward"));
    gwy_container_set_string_by_name(meta, "Sine scan",
                                     g_strdup(header->sine_scan
                                              ? "Yes"
                                              : "No"));
    gwy_container_set_string_by_name(meta, "Scan rate",
                                     g_strdup_printf("%g s<sup>-1</sup>",
                                                     header->scan_rate));
    gwy_container_set_string_by_name(meta, "Set point",
                                     g_strdup_printf("%g %s",
                                                     header->set_point,
                                                     header->set_point_unit));
    gwy_container_set_string_by_name(meta, "Tip bias",
                                     g_strdup_printf("%g V", header->tip_bias));
    gwy_container_set_string_by_name(meta, "Sample bias",
                                     g_strdup_printf("%g V",
                                                     header->sample_bias));

    return meta;
}
#endif

/***************************************************************************
 *
 * Rudimentary TIFF reader
 *
 ***************************************************************************/

static GwyTIFF*
gwy_tiff_load(const gchar *filename,
              GError **error)
{
    GwyTIFF *tiff;

    tiff = g_new0(GwyTIFF, 1);
    if (gwy_tiff_load_real(tiff, filename, error))
        return tiff;

    gwy_tiff_free(tiff);
    return NULL;
}

static gboolean
gwy_tiff_load_real(GwyTIFF *tiff,
                   const gchar *filename,
                   GError **error)
{
    GError *err = NULL;
    const guchar *p;
    guint magic, offset, ifdno;

    if (!gwy_file_get_contents(filename, &tiff->data, &tiff->size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return FALSE;
    }

    if (tiff->size < 8) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    p = tiff->data;
    magic = gwy_get_guint32_le(&p);
    switch (magic) {
        case 0x002a4949:
        tiff->getu16 = gwy_get_guint16_le;
        tiff->gets16 = gwy_get_gint16_le;
        tiff->getu32 = gwy_get_guint32_le;
        tiff->gets32 = gwy_get_gint32_le;
        tiff->getflt = gwy_get_gfloat_le;
        tiff->getdbl = gwy_get_gdouble_le;
        break;

        case 0x2a004d4d:
        tiff->getu16 = gwy_get_guint16_be;
        tiff->gets16 = gwy_get_gint16_be;
        tiff->getu32 = gwy_get_guint32_be;
        tiff->gets32 = gwy_get_gint32_be;
        tiff->getflt = gwy_get_gfloat_be;
        tiff->getdbl = gwy_get_gdouble_be;
        break;

        default:
        err_FILE_TYPE(error, "TIFF");
        return FALSE;
        break;
    }

    offset = tiff->getu32(&p);
    tiff->tags = g_array_new(FALSE, FALSE, sizeof(GwyTIFFEntry));
    ifdno = 0;
    do {
        GwyTIFFEntry entry;
        guint nentries, i;

        if (offset + 6 > tiff->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        "TIFF directory %u ended unexpectedly.", ifdno);
            return FALSE;
        }

        p = tiff->data + offset;
        nentries = tiff->getu16(&p);
        if (offset + 6 + 12*nentries > tiff->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        "TIFF directory %u ended unexpectedly.", ifdno);
            return FALSE;
        }
        for (i = 0; i < nentries; i++) {
            entry.tag = tiff->getu16(&p);
            entry.type = tiff->getu16(&p);
            entry.count = tiff->getu32(&p);
            memcpy(entry.value, p, 4);
            p += 4;
            g_array_append_val(tiff->tags, entry);
        }
        offset = tiff->getu32(&p);
        ifdno++;
    } while (offset);

    return TRUE;
}

static void
gwy_tiff_free(GwyTIFF *tiff)
{
    if (tiff->tags)
        g_array_free(tiff->tags, TRUE);
    if (tiff->data)
        gwy_file_abandon_contents(tiff->data, tiff->size, NULL);

    g_free(tiff);
}

static const GwyTIFFEntry*
gwy_tiff_find_tag(GwyTIFF *tiff,
                  guint tag)
{
    guint i;

    if (!tiff->tags)
        return NULL;

    for (i = 0; i < tiff->tags->len; i++) {
        const GwyTIFFEntry *entry = &g_array_index(tiff->tags, GwyTIFFEntry, i);

        if (entry->tag == tag)
            return entry;
    }

    return NULL;
}

static gboolean
gwy_tiff_get_sint(GwyTIFF *tiff,
                  guint tag,
                  gint *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    const gchar *q;

    entry = gwy_tiff_find_tag(tiff, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_BYTE:
        q = (const gchar*)p;
        *retval = q[0];
        break;

        case GWY_TIFF_SHORT:
        *retval = tiff->gets16(&p);
        break;

        case GWY_TIFF_LONG:
        *retval = tiff->gets32(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

static gboolean
gwy_tiff_get_uint(GwyTIFF *tiff,
                  guint tag,
                  guint *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;

    entry = gwy_tiff_find_tag(tiff, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_BYTE:
        *retval = p[0];
        break;

        case GWY_TIFF_SHORT:
        *retval = tiff->gets16(&p);
        break;

        case GWY_TIFF_LONG:
        *retval = tiff->gets32(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
