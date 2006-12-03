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
#define DEBUG 1
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

#define Nanometer (1e-9)
#define Nanoampere (1e-9)

enum {
    TIFF_HEADER_SIZE = 0x000a,
    JEOL_DICT_START  = 0x17d4,
    JEOL_DICT_SIZE   = 0x0400,
    JEOL_DATA_START  = JEOL_DICT_START + JEOL_DICT_SIZE
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
    JEOL_MODE_LINE_1024    = 1,
    JEOL_MODE_TOPO_MIRROR  = 2,
    JEOL_MODE_TOPO_512     = 3,
    JEOL_MODE_TOPO_256     = 4,
    JEOL_MODE_TOPO_128     = 5,
    JEOL_MODE_LINE_512     = 6,
    JEOL_MODE_LINE_256     = 7,
    JEOL_MODE_LINE_128     = 8,
    JEOL_MODE_TOPO_X2      = 9,
    JEOL_MODE_TOPO_X4      = 10,
    JEOL_MODE_CITS         = 11,
    JEOL_MODE_I_V          = 12,
    JEOL_MODE_S_V          = 13,
    JEOL_MODE_I_S          = 14,
    JEOL_MODE_F_C          = 15,
    JEOL_MODE_FFC          = 16,
    JEOL_MODE_MONTAGE_128  = 17,
    JEOL_MODE_MONTAGE_256  = 18,
    JEOL_MODE_LSTS         = 19,
    JEOL_MODE_TOPO_SPS     = 20,
    JEOL_MODE_VCO          = 21,
    JEOL_MODE_TOPO_IMAGE   = 22,
    JEOL_MODE_TOPO3_VE_AFM = 23,
    JEOL_MODE_TOPO4_MFM    = 24,
    JEOL_MODE_TOPO3_LM_FFM = 25,
    JEOL_MODE_TOPO2_FKM    = 26,
    JEOL_MODE_TOPO2_FFM    = 27,
    JEOL_MODE_TOPO_1204    = 28,
    JEOL_MODE_TOPO_2X512   = 29,
    JEOL_MODE_TOPO2_SCFM   = 30,
    JEOL_MODE_TOPO2_MFM_1  = 31,
    JEOL_MODE_TOPO64       = 32,
    JEOL_MODE_PHASE_SHIFT  = 40,
    JEOL_MODE_MANIPULATION = 40,
    JEOL_MODE_CS3D_SCAN    = 50,
    JEOL_MODE_F_V          = 60,
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
static void          jeol_read_image_header(const guchar *p,
                                            JEOLImageHeader *header);
static GwyDataField* jeol_read_data_field   (const guchar *buffer,
                                             const JEOLImageHeader *header);

static const GwyEnum data_sources[] = {
    { "Topography",        JEOL_DATA_SOURCE_Z,                 },
    { "Log current",       JEOL_DATA_SOURCE_LOG_I,             },
    { "Current",           JEOL_DATA_SOURCE_LIN_I,             },
    { "Aux1",              JEOL_DATA_SOURCE_AUX1,              },
    { "Aux2",              JEOL_DATA_SOURCE_AUX2,              },
    { "Aux3",              JEOL_DATA_SOURCE_AUX3,              },
    { "Ext. voltage",      JEOL_DATA_SOURCE_EXT_VOLTAGE,       },
    { "Force",             JEOL_DATA_SOURCE_FORCE,             },
    { "AFM",               JEOL_DATA_SOURCE_AFM,               },
    { "Friction",          JEOL_DATA_SOURCE_FRICTION,          },
    { "Phase",             JEOL_DATA_SOURCE_PHASE,             },
    { "MFM",               JEOL_DATA_SOURCE_MFM,               },
    { "Elasticity",        JEOL_DATA_SOURCE_ELASTICITY,        },
    { "Viscosity",         JEOL_DATA_SOURCE_VISCOSITY,         },
    { "FFM friction",      JEOL_DATA_SOURCE_FFM_FRICTION,      },
    { "Surface V",         JEOL_DATA_SOURCE_SURFACE_V,         },
    { "Prescan",           JEOL_DATA_SOURCE_PRESCAN,           },
    { "RMS",               JEOL_DATA_SOURCE_RMS,               },
    { "FMD",               JEOL_DATA_SOURCE_FMD,               },
    { "Capacitance force", JEOL_DATA_SOURCE_CAPACITANCE_FORCE, },
};

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
        || fileinfo->file_size < JEOL_DATA_START)
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
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize expected_size, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    const gchar *title;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < JEOL_DATA_START) {
        err_TOO_SHORT(error);
        goto fail;
    }
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "JEOL");
        goto fail;
    }

    jeol_read_image_header(buffer, &image_header);
    /* Elementrary sanity */
    if (image_header.bpp != 16) {
        err_BPP(error, image_header.bpp);
        goto fail;
    }
    expected_size = image_header.bpp/8 * image_header.xres*image_header.yres;
    if (size < JEOL_DATA_START + expected_size) {
        err_SIZE_MISMATCH(error, expected_size, size);
        goto fail;
    }

    /* FIXME: the world is cruel, this is what we know we can read, ditch the
     * rest. */
    if (image_header.image_type != JEOL_IMAGE
        || image_header.data_source != JEOL_DATA_SOURCE_Z
        || image_header.compressed) {
        err_NO_DATA(error);
        goto fail;
    }

    container = gwy_container_new();

    dfield = jeol_read_data_field(buffer + JEOL_DATA_START, &image_header);
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    title = gwy_enum_to_string(image_header.data_source,
                               data_sources, G_N_ELEMENTS(data_sources));
    if (title && *title)
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(title));

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

static void
jeol_read_image_header(const guchar *p,
                       JEOLImageHeader *header)
{
    const guchar *q = p;

    p += TIFF_HEADER_SIZE;

    header->winspm_version = get_WORD_LE(&p);
    p += 80;  /* there isn't anything interesting in internal_filename_old */
    header->xres = get_WORD_LE(&p);
    header->yres = get_WORD_LE(&p);
    header->xreal = get_FLOAT_LE(&p);
    header->yreal = get_FLOAT_LE(&p);
    gwy_debug("res: (%d,%d), real: (%g,%g)",
              header->xres, header->yres, header->xreal, header->yreal);
    header->z0 = get_FLOAT_LE(&p);
    header->z255 = get_FLOAT_LE(&p);
    header->adc_min = get_WORD_LE(&p);
    header->adc_max = get_WORD_LE(&p);
    header->initial_scan_scale = get_FLOAT_LE(&p);
    get_CHARARRAY0(header->internal_filename, &p);
    get_CHARARRAY0(header->info[0], &p);
    get_CHARARRAY0(header->info[1], &p);
    get_CHARARRAY0(header->info[2], &p);
    get_CHARARRAY0(header->info[3], &p);
    get_CHARARRAY0(header->info[4], &p);
    get_CHARARRAY(header->history, &p);
    header->has_current_info = get_WORD_LE(&p);
    header->bias = get_FLOAT_LE(&p);
    header->reference_value = get_FLOAT_LE(&p);
    p += 2;  /* reserved */
    header->measurement_date.day = *(p++);
    header->measurement_date.month = *(p++);
    header->measurement_date.year_1980 = get_WORD_LE(&p);
    header->measurement_date.weekday_sun = *(p++);
    header->save_date.day = *(p++);
    header->save_date.month = *(p++);
    header->save_date.year_1980 = get_WORD_LE(&p);
    header->save_date.weekday_sun = *(p++);
    header->measurement_time.hour = *(p++);
    header->measurement_time.minute = *(p++);
    header->measurement_time.second= *(p++);
    header->measurement_time.second_100 = *(p++);
    header->save_time.hour = *(p++);
    header->save_time.minute = *(p++);
    header->save_time.second= *(p++);
    header->save_time.second_100 = *(p++);
    p += 0x100;  /* lookup table */
    header->fft_offset = get_DWORD_LE(&p);
    header->transform_off = get_WORD_LE(&p);
    p += 8;  /* extra */
    header->compressed = get_BBOOLEAN(&p);
    header->bpp = *(p++);
    gwy_debug("bpp: %d", header->bpp);
    header->cits_offset = get_DWORD_LE(&p);
    header->backscan_tip_voltage = get_FLOAT_LE(&p);
    header->sts_point_x = get_WORD_LE(&p);
    header->sts_point_y = get_WORD_LE(&p);
    p += 20;  /* reserved */
    header->tip_speed_x = get_FLOAT_LE(&p);
    header->tip_speed_y = get_FLOAT_LE(&p);
    p += 42;  /* piezo_sensitivity */
    header->spm_param.clock = get_FLOAT_LE(&p);
    header->spm_param.rotation = get_FLOAT_LE(&p);
    header->spm_param.feedback_filter = get_FLOAT_LE(&p);
    header->spm_param.present_filter = get_FLOAT_LE(&p);
    header->spm_param.head_amp_gain = get_FLOAT_LE(&p);
    header->spm_param.loop_gain = get_WORD_LE(&p);
    header->spm_param.x_off = get_FLOAT_LE(&p);
    header->spm_param.y_off = get_FLOAT_LE(&p);
    header->spm_param.z_gain = get_FLOAT_LE(&p);
    header->spm_param.z_off = get_FLOAT_LE(&p);
    header->spm_param.o_gain = get_FLOAT_LE(&p);
    header->spm_param.o_off = get_FLOAT_LE(&p);
    header->spm_param.back_scan_bias = get_FLOAT_LE(&p);
    header->spm_param.mode = get_DWORD_LE(&p);
    p += 2;  /* reserved */
    p += 4;  /* reserved, XXX: size of JEOLSPMParam1 does not match the space
                alloted for it in the header, it's 4 bytes shorter and this
                is the compensation of the 4 bytes */
    header->montage_offset = get_DWORD_LE(&p);
    get_CHARARRAY0(header->image_location, &p);
    p += 118;  /* spm_misc_param */
    p += 0x300;  /* RGB lookup table */
    header->sub_revision_no = get_DWORD_LE(&p);
    header->image_type = get_DWORD_LE(&p);
    header->data_source = get_DWORD_LE(&p);
    header->display_mode = get_DWORD_LE(&p);
    gwy_debug("image_type: %d, data_source %d",
              header->image_type, header->data_source);
    p += 2*8;  /* measurement_start_time, measurement_end_time */
    p += 254;  /* profile_roughness */
    p += 446;  /* settings_3d */
    header->lut_brightness = get_FLOAT_LE(&p);
    header->lut_contrast = get_FLOAT_LE(&p);
    header->software_version = get_WORD_LE(&p);
    header->software_subversion = get_DWORD_LE(&p);
    p += 20;  /* extracted_region */
    header->lut_gamma = get_FLOAT_LE(&p);
    header->n_of_sps = get_WORD_LE(&p);
    header->sps_offset = get_DWORD_LE(&p);
    header->line_trace_end_x = get_WORD_LE(&p);
    header->line_trace_end_y = get_WORD_LE(&p);
    gwy_debug("0x%x", (guint)(p - q));
    header->forward = get_WORD_LE(&p);
    gwy_debug("forward: %d", header->forward);
    header->lift_signal = get_WORD_LE(&p);
    /* stuff... */
}

static GwyDataField*
jeol_read_data_field(const guchar *buffer,
                     const JEOLImageHeader *header)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    const guint16 *d16;
    gdouble *data;
    gdouble q, z0;
    gint i;

    dfield = gwy_data_field_new(header->xres, header->yres,
                                Nanometer*header->xreal,
                                Nanometer*header->yreal,
                                FALSE);
    z0 = Nanometer*header->z0;
    q = (header->z255 - header->z0)/65535.0*Nanometer;  /* FIXME */

    data = gwy_data_field_get_data(dfield);
    d16 = (const guint16*)buffer;
    for (i = 0; i < header->xres*header->yres; i++)
        data[i] = q*GUINT16_FROM_LE(d16[i]) + z0;

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
