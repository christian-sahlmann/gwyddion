/*
 *  @(#) $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Loosely based on jpkscan.c:
 *  Loader for JPK Image Scans.
 *  Copyright (C) 2005  JPK Instruments AG.
 *  Written by Sven Neumann <neumann@jpk.com>.
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
#include <tiffio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>

#include "err.h"
#include "get.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

/* Custom TIFF tags */
#define PSIA_TIFFTAG_MagicNumber       50432
#define PSIA_TIFFTAG_Version           50433
#define PSIA_TIFFTAG_Data              50434
#define PSIA_TIFFTAG_Header            50435
#define PSIA_TIFFTAG_Comments          50436
#define PSIA_TIFFTAG_LineProfileHeader 50437
/* PSIA claims tag numbers 50432 to 50441, but nothing is known about the
 * remaining tags. */
#define PSIA_MAGIC_NUMBER              0x0E031301

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
static GwyContainer* psia_load_tiff        (TIFF *tiff,
                                            GError **error);
static void          psia_free_image_header(PSIAImageHeader *header);
static gchar*        psia_wchar_to_utf8    (const guchar **src,
                                            guint len);
static gboolean      tiff_check_version    (gint macro,
                                            gint micro,
                                            GError **error);
static gboolean      tiff_get_custom_uint  (TIFF *tiff,
                                            ttag_t tag,
                                            guint *value);
static gboolean      tiff_get_custom_string(TIFF *tiff,
                                            ttag_t tag,
                                            const gchar **value);
static void          tiff_ignore           (const gchar *module,
                                            const gchar *format,
                                            va_list args);
static void          tiff_error            (const gchar *module,
                                            const gchar *format,
                                            va_list args);
static GwyContainer* psia_get_metadata     (PSIAImageHeader *header);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports PSIA data files."),
    "Sven Neumann <neumann@jpk.com>, Yeti <yeti@gwyddion.net>",
    "0.1",
    "JPK Instruments AG, David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    GError *err = NULL;

    /* Handling of custom tags was introduced with LibTIFF version 3.6.0 */
    /* FIXME: Can we do better?  Module registration should be able to return
     * GErrors too... */
    if (!tiff_check_version(3, 6, &err)) {
        g_warning("%s", err->message);
        g_clear_error(&err);
        return FALSE;
    }

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
    TIFFErrorHandler old_error, old_warning;
    TIFF *tiff;
    gint score = 0;
    guint magic = 0;

    if (only_name)
        return score;

    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    old_warning = TIFFSetWarningHandler(tiff_ignore);
    old_error = TIFFSetErrorHandler(tiff_ignore);

    if ((tiff = TIFFOpen(fileinfo->name, "r"))
        && tiff_get_custom_uint(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        && magic == PSIA_MAGIC_NUMBER)
        score = 100;

    if (tiff)
        TIFFClose(tiff);

    TIFFSetErrorHandler(old_error);
    TIFFSetErrorHandler(old_warning);

    return score;
}

static GwyContainer*
psia_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    TIFFErrorHandler old_error, old_warning;
    TIFF *tiff;
    GwyContainer *container = NULL;

    gwy_debug("Loading <%s>", filename);

    old_warning = TIFFSetWarningHandler(tiff_ignore);
    old_error = TIFFSetErrorHandler(tiff_error);

    tiff = TIFFOpen(filename, "r");
    if (!tiff)
        /* This can be I/O too, but it's hard to tell the difference. */
        err_FILE_TYPE(error, _("PSIA"));
    else {
        container = psia_load_tiff(tiff, error);
        TIFFClose(tiff);
    }

    TIFFSetErrorHandler(old_error);
    TIFFSetErrorHandler(old_warning);

    return container;
}

static GwyContainer*
psia_load_tiff(TIFF *tiff, GError **error)
{
    PSIAImageHeader header;
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

    if (!tiff_get_custom_uint(tiff, PSIA_TIFFTAG_MagicNumber, &magic)
        || magic != PSIA_MAGIC_NUMBER
        || !tiff_get_custom_uint(tiff, PSIA_TIFFTAG_Version, &version)
        || version < 0x01000001) {
        err_FILE_TYPE(error, _("PSIA"));
        return NULL;
    }

    if (!TIFFGetField(tiff, PSIA_TIFFTAG_Header, &count, &p)) {
        err_FILE_TYPE(error, _("PSIA"));
        return NULL;
    }
    gwy_debug("[Header] count: %d", count);

    if (count < 356) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header is too short (only %d bytes)."),
                    count);
        return NULL;
    }

    memset(&header, 0, sizeof(PSIAImageHeader));
    header.image_type = get_DWORD_LE(&p);
    gwy_debug("image_type: %d", header.image_type);
    if (header.image_type != PSIA_2D_MAPPED) {
        err_NO_DATA(error);
        return NULL;
    }
    header.source_name = psia_wchar_to_utf8(&p, 32);
    header.image_mode = psia_wchar_to_utf8(&p, 8);
    gwy_debug("source_name: <%s>, image_mode: <%s>",
              header.source_name, header.image_mode);
    header.lpf_strength = get_DOUBLE_LE(&p);
    header.auto_flatten = get_DWORD_LE(&p);
    header.ac_track = get_DWORD_LE(&p);
    header.xres = get_DWORD_LE(&p);
    header.yres = get_DWORD_LE(&p);
    gwy_debug("xres: %d, yres: %d", header.xres, header.yres);
    header.angle = get_DOUBLE_LE(&p);
    header.sine_scan = get_DWORD_LE(&p);
    header.overscan_rate = get_DOUBLE_LE(&p);
    header.forward = get_DWORD_LE(&p);
    header.scan_up = get_DWORD_LE(&p);
    header.swap_xy = get_DWORD_LE(&p);
    header.xreal = get_DOUBLE_LE(&p) * 1e-6;
    header.yreal = get_DOUBLE_LE(&p) * 1e-6;
    gwy_debug("xreal: %g, yreal: %g", header.xreal, header.yreal);
    header.xoff = get_DOUBLE_LE(&p) * 1e-6;
    header.yoff = get_DOUBLE_LE(&p) * 1e-6;
    gwy_debug("xoff: %g, yoff: %g", header.xoff, header.yoff);
    header.scan_rate = get_DOUBLE_LE(&p);
    header.set_point = get_DOUBLE_LE(&p);
    header.set_point_unit = psia_wchar_to_utf8(&p, 8);
    if (!header.set_point_unit)
        header.set_point_unit = g_strdup("V");
    header.tip_bias = get_DOUBLE_LE(&p);
    header.sample_bias = get_DOUBLE_LE(&p);
    header.data_gain = get_DOUBLE_LE(&p);
    header.z_scale = get_DOUBLE_LE(&p);
    header.z_offset = get_DOUBLE_LE(&p);
    gwy_debug("data_gain: %g, z_scale: %g", header.data_gain, header.z_scale);
    header.z_unit = psia_wchar_to_utf8(&p, 8);
    gwy_debug("z_unit: <%s>", header.z_unit);
    header.data_min = get_DWORD_LE(&p);
    header.data_max = get_DWORD_LE(&p);
    header.data_avg = get_DWORD_LE(&p);
    header.compression = get_DWORD_LE(&p);

    tiff_get_custom_string(tiff, PSIA_TIFFTAG_Comments, &comment);
    if (comment) {
        gwy_debug("comment: <%s>", comment);
    }

    if (!TIFFGetField(tiff, PSIA_TIFFTAG_Data, &data_len, &data)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data tag is missing."));
        psia_free_image_header(&header);
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

    meta = psia_get_metadata(&header);
    if (comment && *comment) {
        /* FIXME: Charset conversion. But from what? */
        gwy_container_set_string_by_name(meta, "Comment", g_strdup(comment));
        comment = NULL;
    }
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

static gboolean
tiff_check_version(gint required_macro, gint required_micro, GError **error)
{
    gchar *version = g_strdup(TIFFGetVersion());
    gchar *ptr;
    gboolean result = TRUE;
    gint major;
    gint minor;
    gint micro;

    ptr = strchr(version, '\n');
    if (ptr)
        *ptr = '\0';

    ptr = version;
    while (*ptr && !g_ascii_isdigit(*ptr))
        ptr++;

    if (sscanf(ptr, "%d.%d.%d", &major, &minor, &micro) != 3) {
        g_warning("Cannot parse TIFF version, proceed with fingers crossed");
    }
    else if ((major < required_macro)
             || (major == required_macro && minor < required_micro)) {
        result = FALSE;

        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("LibTIFF too old!\n\n"
                      "You are using %s. Please update to "
                      "libtiff version %d.%d or newer."), version,
                    required_macro, required_micro);
    }

    g_free(version);

    return result;
}

/*  reads what the TIFF spec calls LONG  */
static gboolean
tiff_get_custom_uint(TIFF *tiff, ttag_t tag, guint *value)
{
    guint32 *l;
    gint count;

    if (TIFFGetField(tiff, tag, &count, &l)) {
        *value = *l;
        return TRUE;
    }
    else {
        *value = 0;
        return FALSE;
    }
}

static gboolean
tiff_get_custom_string(TIFF *tiff, ttag_t tag, const gchar **value)
{
    const gchar *s;
    gint count;

    if (TIFFGetField(tiff, tag, &count, &s)) {
        *value = s;
        return TRUE;
    }
    else {
        *value = NULL;
        return FALSE;
    }
}

static void
tiff_ignore(const gchar *module G_GNUC_UNUSED,
            const gchar *format G_GNUC_UNUSED,
            va_list args G_GNUC_UNUSED)
{
    /*  ignore  */
}

/* TODO: pass the error message upstream, somehow */
static void
tiff_error(const gchar *module G_GNUC_UNUSED, const gchar *format, va_list args)
{
    g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, format, args);
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
