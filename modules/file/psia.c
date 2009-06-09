/*
 *  @(#) $Id$
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
#include "gwytiff.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define Micrometre (1e-6)

enum {
    /* The value of PSIA_TIFFTAG_MagicNumber */
    PSIA_MAGIC_NUMBER = 0x0E031301u,
    /* Version values */
    PSIA_VERSION1 = 0x1000001u,
    PSIA_VERSION2 = 0x1000002u,
};

/* Custom TIFF tags */
enum {
    PSIA_TIFFTAG_MagicNumber       = 50432,
    PSIA_TIFFTAG_Version           = 50433,
    PSIA_TIFFTAG_Data              = 50434,
    PSIA_TIFFTAG_Header            = 50435,
    PSIA_TIFFTAG_Comments          = 50436,
    PSIA_TIFFTAG_LineProfileHeader = 50437
};
/* PSIA claims tag numbers 50432 to 50441, but nothing is known about the
 * remaining tags. */

typedef enum {
    PSIA_2D_MAPPED    = 0,
    PSIA_LINE_PROFILE = 1
} PSIAImageType;

/* Version 2+ only */
typedef enum {
    PSIA_DATA_INT16 = 0,
    PSIA_DATA_INT32 = 1,
    PSIA_DATA_FLOAT = 2,
} PSIADataType;

typedef struct {
    PSIAImageType image_type;
    gchar *source_name;     /* [32] Topography, ... */
    gchar *image_mode;      /* [8] AFM, NCM, ... */
    gdouble lpf_strength;   /* Low-pass filter strength */
    gboolean auto_flatten;  /* Automatic flatten after scan */
    gboolean ac_track;    /* AC track, order of flattening + 1 (WTF?) */
    guint32 xres;
    guint32 yres;
    gdouble angle;          /* Of fast axis wrt positive x-axis */
    gboolean sine_scan;
    gdouble overscan_rate;  /* In % */
    gboolean forward;       /* Otherwise backward */
    gboolean scan_up;       /* Otherwise scan down */
    gboolean swap_xy;       /* Swap slow/fast, actually */
    gdouble xreal;          /* In micrometers */
    gdouble yreal;
    gdouble xoff;
    gdouble yoff;
    gdouble scan_rate;      /* In rows per second */
    gdouble set_point;      /* Error signal set point */
    gchar *set_point_unit;  /* [8] */
    gdouble tip_bias;       /* In volts */
    gdouble sample_bias;
    gdouble data_gain;
    gdouble z_scale;        /* Scale multiplier, they say it is always 1 */
    gdouble z_offset;
    gchar *z_unit;          /* [8] */
    gint data_min;          /* Statistics, we do not trust these anyway */
    gint data_max;
    gint data_avg;
    gboolean compression;
    gboolean logscale;
    gboolean square;
    /* Only in version 2+
     * NB: This must be interpreted as the new version can have different data
     * types. */
    gdouble z_servo_gain;
    gdouble z_scanner_range;
    gchar *xy_voltage_mode;  /* [8] */
    gchar *z_voltage_mode;   /* [8] */
    gchar *xy_servo_mode;    /* [8] */
    PSIADataType data_type;
    gint reserved1;
    gint reserved2;
    gdouble ncm_amplitude;
    gdouble ncm_frequency;
    gdouble head_rotation_angle;
    gchar *cantilever_name;  /* [16] */
} PSIAImageHeader;

static gboolean      module_register       (void);
static gint          psia_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* psia_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static GwyContainer* psia_load_tiff        (GwyTIFF *tiff,
                                            GError **error);
static void          psia_free_image_header(PSIAImageHeader *header);
static void          psia_read_data_field  (GwyDataField *dfield,
                                            const guchar *p,
                                            PSIADataType data_type,
                                            gdouble q,
                                            gdouble z_scale,
                                            gdouble z0);
static gchar*        psia_wchar_to_utf8    (const guchar **src,
                                            guint len);
static GwyContainer* psia_get_metadata     (PSIAImageHeader *header,
                                            guint version);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports PSIA data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.5",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("psia",
                           N_("PSIA data files (.tiff)"),
                           (GwyFileDetectFunc)&psia_detect,
                           (GwyFileLoadFunc)&psia_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
psia_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;
    guint magic, version;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
        && gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        && magic == PSIA_MAGIC_NUMBER
        && gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_Version, &version)
        && (version == PSIA_VERSION1 || version == PSIA_VERSION2))
        score = 100;

    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
psia_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyTIFF *tiff;
    GwyContainer *container = NULL;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    container = psia_load_tiff(tiff, error);
    gwy_tiff_free(tiff);

    return container;
}

static GwyContainer*
psia_load_tiff(GwyTIFF *tiff, GError **error)
{
    const GwyTIFFEntry *entry;
    PSIAImageHeader header;
    GwyContainer *container = NULL;
    GwyContainer *meta = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    guint magic, version, bps, i;
    const guchar *p, *data;
    gchar *comment = NULL;
    gint count, data_len, power10;
    gdouble q, z0;

    if (!gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        || magic != PSIA_MAGIC_NUMBER
        || !gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_Version, &version)
        || !(version == PSIA_VERSION1 || version == PSIA_VERSION2)) {
        err_FILE_TYPE(error, "PSIA");
        return NULL;
    }

    /* Data */
    entry = gwy_tiff_find_tag(tiff, 0, PSIA_TIFFTAG_Data);
    if (!entry) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data tag is missing."));
        return NULL;
    }
    p = entry->value;
    count = tiff->get_guint32(&p);
    data = tiff->data + count;
    data_len = entry->count;
    gwy_debug("data_len: %d", data_len);
    if (data_len + count > tiff->size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is truncated."));
        return NULL;
    }

    /* Header */
    entry = gwy_tiff_find_tag(tiff, 0, PSIA_TIFFTAG_Header);
    if (!entry) {
        err_FILE_TYPE(error, "PSIA");
        return NULL;
    }
    p = entry->value;
    i = tiff->get_guint32(&p);
    p = tiff->data + i;
    count = entry->count;
    gwy_debug("[Header] count: %d", count);

    if ((version == PSIA_VERSION1 && count < 356)
        || (version == PSIA_VERSION2 && count < 580)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header is too short (only %d bytes)."),
                    count);
        return NULL;
    }

    /* Parse header */
    memset(&header, 0, sizeof(PSIAImageHeader));
    header.image_type = gwy_get_guint32_le(&p);
    gwy_debug("image_type: %d", header.image_type);
    if (header.image_type != PSIA_2D_MAPPED) {
        err_NO_DATA(error);
        return NULL;
    }
    header.source_name = psia_wchar_to_utf8(&p, 32);
    header.image_mode = psia_wchar_to_utf8(&p, 8);
    gwy_debug("source_name: <%s>, image_mode: <%s>",
              header.source_name, header.image_mode);
    header.lpf_strength = gwy_get_gdouble_le(&p);
    header.auto_flatten = gwy_get_guint32_le(&p);
    header.ac_track = gwy_get_guint32_le(&p);
    header.xres = gwy_get_guint32_le(&p);
    header.yres = gwy_get_guint32_le(&p);
    gwy_debug("xres: %d, yres: %d", header.xres, header.yres);
    if (err_DIMENSION(error, header.xres)
        || err_DIMENSION(error, header.yres)) {
        psia_free_image_header(&header);
        return NULL;
    }

    header.angle = gwy_get_gdouble_le(&p);
    header.sine_scan = gwy_get_guint32_le(&p);
    header.overscan_rate = gwy_get_gdouble_le(&p);
    header.forward = gwy_get_guint32_le(&p);
    header.scan_up = gwy_get_guint32_le(&p);
    header.swap_xy = gwy_get_guint32_le(&p);
    header.xreal = gwy_get_gdouble_le(&p);
    header.yreal = gwy_get_gdouble_le(&p);
    gwy_debug("xreal: %g, yreal: %g", header.xreal, header.yreal);
    /* Use negated positive conditions to catch NaNs */
    if (!((header.xreal = fabs(header.xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        header.xreal = 1.0;
    }
    if (!((header.yreal = fabs(header.yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        header.yreal = 1.0;
    }
    header.xreal *= Micrometre;
    header.yreal *= Micrometre;

    header.xoff = gwy_get_gdouble_le(&p) * Micrometre;
    header.yoff = gwy_get_gdouble_le(&p) * Micrometre;
    gwy_debug("xoff: %g, yoff: %g", header.xoff, header.yoff);
    header.scan_rate = gwy_get_gdouble_le(&p);
    header.set_point = gwy_get_gdouble_le(&p);
    header.set_point_unit = psia_wchar_to_utf8(&p, 8);
    if (!header.set_point_unit)
        header.set_point_unit = g_strdup("V");
    header.tip_bias = gwy_get_gdouble_le(&p);
    header.sample_bias = gwy_get_gdouble_le(&p);
    header.data_gain = gwy_get_gdouble_le(&p);
    header.z_scale = gwy_get_gdouble_le(&p);
    header.z_offset = gwy_get_gdouble_le(&p);
    gwy_debug("data_gain: %g, z_scale: %g", header.data_gain, header.z_scale);
    header.z_unit = psia_wchar_to_utf8(&p, 8);
    gwy_debug("z_unit: <%s>", header.z_unit);
    header.data_min = gwy_get_gint32_le(&p);
    header.data_max = gwy_get_gint32_le(&p);
    header.data_avg = gwy_get_gint32_le(&p);
    header.compression = gwy_get_guint32_le(&p);
    header.logscale = gwy_get_guint32_le(&p);
    header.square = gwy_get_guint32_le(&p);

    if (version == PSIA_VERSION2) {
        header.z_servo_gain = gwy_get_gdouble_le(&p);
        header.z_scanner_range = gwy_get_gdouble_le(&p);
        header.xy_voltage_mode = psia_wchar_to_utf8(&p, 8);
        header.z_voltage_mode = psia_wchar_to_utf8(&p, 8);
        header.xy_servo_mode = psia_wchar_to_utf8(&p, 8);
        header.data_type = gwy_get_guint32_le(&p);
        header.reserved1 = gwy_get_guint32_le(&p);
        header.reserved2 = gwy_get_guint32_le(&p);
        header.ncm_amplitude = gwy_get_gdouble_le(&p);
        header.ncm_frequency = gwy_get_gdouble_le(&p);
        header.cantilever_name = psia_wchar_to_utf8(&p, 16);
    }
    else
        header.data_type = PSIA_DATA_INT16;

    gwy_debug("data_type: %d", header.data_type);
    if (header.data_type == PSIA_DATA_INT16)
        bps = 2;
    else if (header.data_type == PSIA_DATA_INT32
             || header.data_type == PSIA_DATA_FLOAT)
        bps = 4;
    else {
        err_DATA_TYPE(error, header.data_type);
        psia_free_image_header(&header);
        return NULL;
    }

    if (err_SIZE_MISMATCH(error, bps*header.xres*header.yres, data_len, TRUE)) {
        psia_free_image_header(&header);
        return NULL;
    }

    gwy_tiff_get_string0(tiff, PSIA_TIFFTAG_Comments, &comment);

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
    psia_read_data_field(dfield, data, header.data_type, q, header.z_scale, z0);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    if (header.source_name && *header.source_name)
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(header.source_name));

    meta = psia_get_metadata(&header, version);
    if (comment && *comment) {
        /* FIXME: Charset conversion. But from what? */
        gwy_container_set_string_by_name(meta, "Comment", comment);
        comment = NULL;
    }
    g_free(comment);
    gwy_container_set_string_by_name(meta, "Version",
                                     g_strdup_printf("%08x", version));

    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    psia_free_image_header(&header);

    return container;
}

static void
psia_free_image_header(PSIAImageHeader *header)
{
    g_free(header->source_name);
    g_free(header->image_mode);
    g_free(header->set_point_unit);
    g_free(header->z_unit);
}

static void
psia_read_data_field(GwyDataField *dfield,
                     const guchar *p,
                     PSIADataType data_type,
                     gdouble q,
                     gdouble z_scale,
                     gdouble z0)
{
    gint i, j, xres, yres;
    gdouble *data, *d;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    data = gwy_data_field_get_data(dfield);

    if (data_type == PSIA_DATA_INT16) {
        const gint16 *p16 = (const gint16*)p;

        for (i = 0; i < yres; i++) {
            d = data + (yres-1 - i)*xres;
            for (j = 0; j < xres; j++)
                d[j] = q*(GINT16_FROM_LE(p16[i*xres + j])*z_scale + z0);
        }
    }
    else if (data_type == PSIA_DATA_INT32) {
        const gint32 *p32 = (const gint32*)p;

        for (i = 0; i < yres; i++) {
            d = data + (yres-1 - i)*xres;
            for (j = 0; j < xres; j++)
                d[j] = q*(GINT32_FROM_LE(p32[i*xres + j])*z_scale + z0);
        }
    }
    else if (data_type == PSIA_DATA_FLOAT) {
        for (i = 0; i < yres; i++) {
            d = data + (yres-1 - i)*xres;
            for (j = 0; j < xres; j++)
                d[j] = q*(gwy_get_gfloat_le(&p)*z_scale + z0);
        }
    }
    else {
        g_return_if_reached();
    }
}

static gchar*
psia_wchar_to_utf8(const guchar **src,
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

static GwyContainer*
psia_get_metadata(PSIAImageHeader *header,
                  guint version)
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

    gwy_container_set_string_by_name(meta, "Version",
                                     g_strdup_printf("%u.%u.%u",
                                                     version >> 24,
                                                     (version >> 12) & 0xfff,
                                                     version & 0xfff));
    gwy_container_set_string_by_name(meta, "Overscan",
                                     g_strdup_printf("%g %%",
                                                     100*header->overscan_rate));
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

    if (version == PSIA_VERSION1)
        return meta;

    if (header->xy_voltage_mode && *header->xy_voltage_mode) {
        gwy_container_set_string_by_name(meta, "XY voltage mode",
                                         header->xy_voltage_mode);
        header->xy_voltage_mode = NULL;
    }

    if (header->z_voltage_mode && *header->z_voltage_mode) {
        gwy_container_set_string_by_name(meta, "Z voltage mode",
                                         header->z_voltage_mode);
        header->z_voltage_mode = NULL;
    }

    if (header->xy_servo_mode && *header->xy_servo_mode) {
        gwy_container_set_string_by_name(meta, "XY servo mode",
                                         header->xy_servo_mode);
        header->xy_servo_mode = NULL;
    }

    if (header->cantilever_name && *header->cantilever_name) {
        gwy_container_set_string_by_name(meta, "Cantilever",
                                         header->cantilever_name);
        header->cantilever_name = NULL;
    }

    gwy_container_set_string_by_name(meta, "Z scanner range",
                                     g_strdup_printf("%g",
                                                     header->z_scanner_range));
    gwy_container_set_string_by_name(meta, "Z servo gain",
                                     g_strdup_printf("%g",
                                                     header->z_servo_gain));
    gwy_container_set_string_by_name(meta, "Head tilt angle",
                                     g_strdup_printf("%g°", header->head_rotation_angle));

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
