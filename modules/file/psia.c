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

/* The value of PSIA_TIFFTAG_MagicNumber */
#define PSIA_MAGIC_NUMBER 0x0E031301

#define Micrometre (1e-6)

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

typedef struct {
    PSIAImageType image_type;
    gchar *source_name;
    gchar *image_mode;
    gdouble lpf_strength;
    gboolean auto_flatten;
    gboolean ac_track;
    guint32 xres;
    guint32 yres;
    gdouble angle;
    gboolean sine_scan;
    gdouble overscan_rate;
    gboolean forward;
    gboolean scan_up;
    gboolean swap_xy;
    gdouble xreal;
    gdouble yreal;
    gdouble xoff;
    gdouble yoff;
    gdouble scan_rate;
    gdouble set_point;
    gchar *set_point_unit;
    gdouble tip_bias;
    gdouble sample_bias;
    gdouble data_gain;
    gdouble z_scale;
    gdouble z_offset;
    gchar *z_unit;
    gint data_min;
    gint data_max;
    gint data_avg;
    gboolean compression;
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
static gchar*        psia_wchar_to_utf8    (const guchar **src,
                                            guint len);
static GwyContainer* psia_get_metadata     (PSIAImageHeader *header);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports PSIA data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
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
    guint magic = 0;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
        && gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        && magic == PSIA_MAGIC_NUMBER)
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
    guint magic, version, i, j;
    const guchar *p;
    const guint16 *data;
    gchar *comment = NULL;
    gint count, data_len, power10;
    gdouble q, z0;
    gdouble *d;

    if (!gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        || magic != PSIA_MAGIC_NUMBER
        || !gwy_tiff_get_uint0(tiff, PSIA_TIFFTAG_Version, &version)
        || version < 0x01000001) {
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
    data = (const guint16*)(tiff->data + tiff->getu32(&p));
    data_len = entry->count;
    gwy_debug("data_len: %d", data_len);

    /* Header */
    entry = gwy_tiff_find_tag(tiff, 0, PSIA_TIFFTAG_Header);
    if (!entry) {
        err_FILE_TYPE(error, "PSIA");
        return NULL;
    }
    p = entry->value;
    i = tiff->getu32(&p);
    p = tiff->data + i;
    count = entry->count;
    gwy_debug("[Header] count: %d", count);

    if (count < 356) {
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
    if (err_SIZE_MISMATCH(error, 2*header.xres*header.yres, data_len, TRUE)) {
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

    meta = psia_get_metadata(&header);
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
psia_get_metadata(PSIAImageHeader *header)
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
