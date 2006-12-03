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
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>

#include "err.h"
#include "get.h"

/* Magic includes TIFF directory start address and size because they are fixed
 * numbers for JEOL enabling to quicky weed out most of other TIFF files. */
#define MAGIC      "II\x2a\x00\xd4\x17\x00\x00\x00\x04"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

enum {
    JEOL_DICT_START  = 0x17d4,
    JEOL_DICT_SIZE   = 0x0400,
    JEOL_MIN_SIZE    = JEOL_DICT_START + JEOL_DICT_SIZE + 2
};

typedef enum {
    JEOL_IMAGE     = 1,
    JEOL_SPECTRUM  = 2,
    JEOL_PROFILE   = 3,
    JEOL_HISTOGRAM = 4,
    JEOL_VCO       = 5
} JEOLImageType;

typedef enum {
    JEOL_DATA_SOURCE_Z                 = 1,
    JEOL_DATA_SOURCE_LOG_I             = 2,
    JEOL_DATA_SOURCE_LIN_I             = 3,
    JEOL_DATA_SOURCE_AUX1              = 4,
    JEOL_DATA_SOURCE_AUX2              = 5,
    JEOL_DATA_SOURCE_AUX3              = 6,
    JEOL_DATA_SOURCE_EXT_VOLTAGE       = 7,
    JEOL_DATA_SOURCE_FORCE             = 8,
    JEOL_DATA_SOURCE_AFM               = 9,
    JEOL_DATA_SOURCE_FRICTION          = 10,
    JEOL_DATA_SOURCE_PHASE             = 11,
    JEOL_DATA_SOURCE_MFM               = 12,
    JEOL_DATA_SOURCE_ELASTICITY        = 13,
    JEOL_DATA_SOURCE_VISCOSITY         = 14,
    JEOL_DATA_SOURCE_FFM_FRICTION      = 15,
    JEOL_DATA_SOURCE_SURFACE_V         = 16,
    JEOL_DATA_SOURCE_PRESCAN           = 17,
    JEOL_DATA_SOURCE_RMS               = 18,
    JEOL_DATA_SOURCE_FMD               = 19,
    JEOL_DATA_SOURCE_CAPACITANCE_FORCE = 20
} JEOLDataSourceType;

typedef enum {
    JEOL_DISPLAY_DEFAULT = 1,
    JEOL_DISPLAY_BMP     = 2,
    JEOL_DISPLAY_3D      = 3
} JEOLDisplayModeType;

typedef enum {
    JEOL_MODE_LINE_1024 = 1,
    JEOL_MODE_TOPO_MIRROR = 2,
    JEOL_MODE_TOPO_512 = 3,
    JEOL_MODE_TOPO_256 = 4,
    JEOL_MODE_TOPO_128 = 5,
    JEOL_MODE_LINE_512 = 6,
    JEOL_MODE_LINE_256 = 7,
    JEOL_MODE_LINE_128 = 8,
    JEOL_MODE_TOPO_X2 = 9,
    JEOL_MODE_TOPO_X4 = 10,
    JEOL_MODE_CITS = 11,
    JEOL_MODE_I_V = 12,
    JEOL_MODE_S_V = 13,
    JEOL_MODE_I_S = 14,
    JEOL_MODE_F_C = 15,
    JEOL_MODE_FFC = 16,
    JEOL_MODE_MONTAGE_128 = 17,
    JEOL_MODE_MONTAGE_256 = 18,
    JEOL_MODE_LSTS = 19,
    JEOL_MODE_TOPO_SPS = 20,
    JEOL_MODE_VCO = 21,
    JEOL_MODE_TOPO_IMAGE = 22,
    JEOL_MODE_TOPO3_VE_AFM = 23,
    JEOL_MODE_TOPO4_MFM = 24,
    JEOL_MODE_TOPO3_LM_FFM = 25,
    JEOL_MODE_TOPO2_FKM = 26,
    JEOL_MODE_TOPO2_FFM = 27,
    JEOL_MODE_TOPO_1204 = 28,
    JEOL_MODE_TOPO_2X512 = 29,
    JEOL_MODE_TOPO2_SCFM = 30,
    JEOL_MODE_TOPO2_MFM_1 = 31,
    JEOL_MODE_TOPO64 = 32,
    JEOL_MODE_PHASE_SHIFT = 40,
    JEOL_MODE_MANIPULATION = 40,
    JEOL_MODE_CS3D_SCAN = 50,
    JEOL_MODE_F_V = 60,
    JEOL_MODE_SOFTWARE_GEN = 70
} JEOLModeType;

typedef struct {
    guint day;
    guint month;
    guint year_1980;
    guint weekday_sun;
} JEOLDate;

typedef struct {
    guint hour;
    guint minute;
    guint second;
    guint second_100;
} JEOLTime;

typedef struct {
    guint x1;
    guint x2;
    guint y1;
    guint y2;
} JEOLSelectionRange;

typedef struct {
    guint adc_source;
    guint adc_offset;
    guint adc_gain;
    guint head_amp_gain;
} JEOLCITSParam;

typedef struct {
    gdouble clock;
    gdouble rotation;
    gdouble feedback_filter;
    gdouble present_filter;
    gdouble head_amp_gain;
    guint loop_gain;
    gdouble x_off;
    gdouble y_off;
    gdouble z_gain;
    gdouble z_off;
    gdouble o_gain;
    gdouble o_off;
    gdouble back_scan_bias;
    JEOLModeType mode;
} JEOLSPMParam;

typedef struct {
    guint32 winspm_version;
    gchar internal_filename_old[80];
    guint32 xres;
    guint32 yres;
    gdouble xreal;
    gdouble yreal;
    gdouble z0;
    gdouble z255;
    gint32 adc_min;
    gint32 adc_max;
    gdouble initial_scan_scale;
    gchar internal_filename[40];
    gchar info[5][40];
    guchar history[50];
    gboolean has_current_info;
    gdouble bias;
    gdouble reference_value;
    JEOLDate measurement_date;
    JEOLDate save_date;
    JEOLTime measurement_time;
    JEOLTime save_time;
    guchar lookup_table[0x00];
    guint32 fft_offset;
    guint32 transform_off;
    /* JEOLExtraType extra; union of JEOLSelectionRange and JEOLCITSParam */
    gboolean compressed;
    guint32 bpp;
    guint32 cits_offset;
    gdouble backscan_tip_voltage;
    guint32 sts_point_x;
    guint32 sts_point_y;
    gdouble tip_speed_x;
    gdouble tip_speed_y;
    /* JEOLCalib piezo_sensitivity; */
    JEOLSPMParam spm_param;
    guint32 montage_offset;
    gchar image_location[260];
    /* JEOLSPMParam1 spm_misc_param; */
    guint32 sub_revision_no;
    JEOLImageType image_type;
    JEOLDataSourceType data_source;
    JEOLDisplayModeType display_mode;
    /* JEOLFileTime measurement_start_time; */
    /* JEOLFileTime measurement_end_time; */
    /* JEOLProfileDef profile_roughness; */
    /* JEOL3DSettings settings_3d; */
    gdouble lut_brightness;
    gdouble lut_contrast;
    guint32 software_version;
    guint32 software_subversion;
    /* JEOLExtract extracted_region; */
    gdouble lut_gamma;
    guint32 n_of_sps;
    guint32 sps_offset;
    guint32 line_trace_end_x;
    guint32 line_trace_end_y;
    gboolean forward;
    gboolean lift_signal;
    /* JEOLLIAParam lia_settings; */
    /* JEOLTempParam temp_param; */
    /* JEOLAFMFixedRef converted_afm; */
    /* JEOLLMCantileverParam special_measurement_param; */
} JEOLImageHeader;

static gboolean      module_register       (void);
static gint          jeol_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* jeol_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static void          jeol_free_image_header(JEOLImageHeader *header);
static GwyContainer* jeol_get_metadata     (JEOLImageHeader *header);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports JEOL data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("jeol",
                           N_("JEOL data files (.tif)"),
                           (GwyFileDetectFunc)&jeol_detect,
                           (GwyFileLoadFunc)&jeol_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
jeol_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    if (only_name)
        return 0;

    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0
        || fileinfo->file_size < JEOL_MIN_SIZE)
        return 0;

    /* TODO: check it better, some TIFFs can have the same dictionary start
     * and size by chance. */
    return 90;
}

static GwyContainer*
jeol_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    JEOLImageHeader image_header;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    p = buffer;
    if (size < JEOL_MIN_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    err_NO_DATA(error);

    return container;
}

#if 0
static GwyContainer*
jeol_load_tiff(TIFF *tiff, GError **error)
{
    JEOLImageHeader header;
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

    if (!tiff_get_custom_uint(tiff, JEOL_TIFFTAG_MagicNumber, &magic)
        || magic != JEOL_MAGIC_NUMBER
        || !tiff_get_custom_uint(tiff, JEOL_TIFFTAG_Version, &version)
        || version < 0x01000001) {
        err_FILE_TYPE(error, _("JEOL"));
        return NULL;
    }

    if (!TIFFGetField(tiff, JEOL_TIFFTAG_Header, &count, &p)) {
        err_FILE_TYPE(error, _("JEOL"));
        return NULL;
    }
    gwy_debug("[Header] count: %d", count);

    if (count < 356) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header is too short (only %d bytes)."),
                    count);
        return NULL;
    }

    memset(&header, 0, sizeof(JEOLImageHeader));
    header.image_type = get_DWORD_LE(&p);
    gwy_debug("image_type: %d", header.image_type);
    if (header.image_type != JEOL_2D_MAPPED) {
        err_NO_DATA(error);
        return NULL;
    }
    header.source_name = jeol_wchar_to_utf8(&p, 32);
    header.image_mode = jeol_wchar_to_utf8(&p, 8);
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
    header.set_point_unit = jeol_wchar_to_utf8(&p, 8);
    if (!header.set_point_unit)
        header.set_point_unit = g_strdup("V");
    header.tip_bias = get_DOUBLE_LE(&p);
    header.sample_bias = get_DOUBLE_LE(&p);
    header.data_gain = get_DOUBLE_LE(&p);
    header.z_scale = get_DOUBLE_LE(&p);
    header.z_offset = get_DOUBLE_LE(&p);
    gwy_debug("data_gain: %g, z_scale: %g", header.data_gain, header.z_scale);
    header.z_unit = jeol_wchar_to_utf8(&p, 8);
    gwy_debug("z_unit: <%s>", header.z_unit);
    header.data_min = get_DWORD_LE(&p);
    header.data_max = get_DWORD_LE(&p);
    header.data_avg = get_DWORD_LE(&p);
    header.compression = get_DWORD_LE(&p);

    tiff_get_custom_string(tiff, JEOL_TIFFTAG_Comments, &comment);
    if (comment) {
        gwy_debug("comment: <%s>", comment);
    }

    if (!TIFFGetField(tiff, JEOL_TIFFTAG_Data, &data_len, &data)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data tag is missing."));
        jeol_free_image_header(&header);
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

    meta = jeol_get_metadata(&header);
    if (comment && *comment) {
        /* FIXME: Charset conversion. But from what? */
        gwy_container_set_string_by_name(meta, "Comment", g_strdup(comment));
        comment = NULL;
    }
    gwy_container_set_string_by_name(meta, "Version",
                                     g_strdup_printf("%08x", version));

    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    jeol_free_image_header(&header);

    return container;
}

static void
jeol_free_image_header(JEOLImageHeader *header)
{
    g_free(header->source_name);
    g_free(header->image_mode);
    g_free(header->set_point_unit);
    g_free(header->z_unit);
}

static GwyContainer*
jeol_get_metadata(JEOLImageHeader *header)
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
