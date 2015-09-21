/*
 *  $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-keyence-vk4">
 *   <comment>Keyence VK4 profilometry data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="VK4_"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # Keyence VK4.
 * 0 string VK4_ Keyence profilometry VK4 data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Keyence profilometry VK4
 * *.vk4
 * Read
 **/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <libprocess/dataline.h>
#include <libprocess/spectra.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define MAGIC "VK4_"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define MAGIC0 "\x00\x00\x00\x00"
#define MAGIC0_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".vk4"

#define Picometre (1e-12)

enum {
    KEYENCE_HEADER_SIZE = 12,
    KEYENCE_OFFSET_TABLE_SIZE = 72,
    KEYENCE_MEASUREMENT_CONDITIONS_MIN_SIZE = 304,
    KEYENCE_ASSEMBLY_INFO_SIZE = 16,
    KEYENCE_ASSEMBLY_CONDITIONS_SIZE = 8,
    KEYENCE_ASSEMBLY_HEADERS_SIZE = (KEYENCE_ASSEMBLY_INFO_SIZE
                                     + KEYENCE_ASSEMBLY_CONDITIONS_SIZE),
    KEYENCE_ASSEMBLY_FILE_SIZE = 532,
    KEYENCE_TRUE_COLOR_IMAGE_MIN_SIZE = 20,
    KEYENCE_FALSE_COLOR_IMAGE_MIN_SIZE = 796,
    KEYENCE_LINE_MEASUREMENT_LEN = 1024,
    KEYENCE_LINE_MEASUREMENT_SIZE = 18440,
};

typedef enum {
    KEYENCE_NORMAL_FILE = 0,
    KEYENCE_ASSEMBLY_FILE = 1,
    KEYENCE_ASSEMBLY_FILE_UNICODE = 2,
} KeyenceFileType;

typedef struct {
    guchar magic[4];
    guchar dll_version[4];
    guchar file_type[4];
} KeyenceHeader;

typedef struct {
    guint setting;
    guint color_peak;
    guint color_light;
    guint light[3];
    guint height[3];
    guint color_peak_thumbnail;
    guint color_thumbnail;
    guint light_thumbnail;
    guint height_thumbnail;
    guint assemble;
    guint line_measure;
    guint line_thickness;
    guint string_data;
    guint reserved;
} KeyenceOffsetTable;

typedef struct {
    guint size;
    guint year;
    guint month;
    guint day;
    guint hour;
    guint minute;
    guint second;
    gint diff_utc_by_minutes;
    guint image_attributes;
    guint user_interface_mode;
    guint color_composite_mode;
    guint num_layer;
    guint run_mode;
    guint peak_mode;
    guint sharpening_level;
    guint speed;
    guint distance;
    guint pitch;
    guint optical_zoom;
    guint num_line;
    guint line0_pos;
    guint reserved1[3];
    guint lens_mag;
    guint pmt_gain_mode;
    guint pmt_gain;
    guint pmt_offset;
    guint nd_filter;
    guint reserved2;
    guint persist_count;
    guint shutter_speed_mode;
    guint shutter_speed;
    guint white_balance_mode;
    guint white_balance_red;
    guint white_balance_blue;
    guint camera_gain;
    guint plane_compensation;
    guint xy_length_unit;
    guint z_length_unit;
    guint xy_decimal_place;
    guint z_decimal_place;
    guint x_length_per_pixel;
    guint y_length_per_pixel;
    guint z_length_per_digit;
    guint reserved3[5];
    guint light_filter_type;
    guint reserved4;
    guint gamma_reverse;
    guint gamma;
    guint gamma_offset;
    guint ccd_bw_offset;
    guint numerical_aperture;
    guint head_type;
    guint pmt_gain2;
    guint omit_color_image;
    guint lens_id;
    guint light_lut_mode;
    guint light_lut_in0;
    guint light_lut_out0;
    guint light_lut_in1;
    guint light_lut_out1;
    guint light_lut_in2;
    guint light_lut_out2;
    guint light_lut_in3;
    guint light_lut_out3;
    guint light_lut_in4;
    guint light_lut_out4;
    guint upper_position;
    guint lower_position;
    guint light_effective_bit_depth;
    guint height_effective_bit_depth;
    /* XXX: There is much more... */
} KeyenceMeasurementConditions;

typedef struct {
    guint size;   /* The size of *all* assembly-related blocks. */
    KeyenceFileType file_type;
    guint stage_type;
    guint x_position;
    guint y_position;
} KeyenceAssemblyInformation;

typedef struct {
    guint auto_adjustment;
    guint source;
    guint thin_out;
    guint count_x;
    guint count_y;
} KeyenceAssemblyConditions;

typedef struct {
    guint16 source_file[260];   /* This is Microsoft's wchar_t. */
    guint pos_x;
    guint pos_y;
    guint datums_pos;
    guint fix_distance;
    guint distance_x;
    guint distance_y;
} KeyenceAssemblyFile;

typedef struct {
    guint width;
    guint height;
    guint bit_depth;
    guint compression;
    guint byte_size;
    const guchar *data;
} KeyenceTrueColorImage;

typedef struct {
    guint width;
    guint height;
    guint bit_depth;
    guint compression;
    guint byte_size;
    guint palette_range_min;
    guint palette_range_max;
    guchar palette[0x300];
    const guchar *data;
} KeyenceFalseColorImage;

typedef struct {
    guint size;
    guint line_width;
    const guchar *light[3];
    const guchar *height[3];
} KeyenceLineMeasurement;

typedef struct {
    gchar *title;
    gchar *lens_name;
} KeyenceCharacterStrings;

typedef struct {
    KeyenceHeader header;
    KeyenceOffsetTable offset_table;
    KeyenceMeasurementConditions meas_conds;
    /* The rest is optional. */
    KeyenceAssemblyInformation assembly_info;
    KeyenceAssemblyConditions assembly_conds;
    guint assembly_nfiles;
    guint nimages;
    KeyenceAssemblyFile *assembly_files;
    KeyenceTrueColorImage color_peak;
    KeyenceTrueColorImage color_light;
    KeyenceFalseColorImage light[3];
    KeyenceFalseColorImage height[3];
    KeyenceLineMeasurement line_measure;
    KeyenceCharacterStrings char_strs;
    /* Raw file contents. */
    guchar *buffer;
    gsize size;
} KeyenceFile;

static gboolean      module_register    (void);
static gint          keyence_detect     (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer* keyence_load       (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);
static void          free_file          (KeyenceFile *kfile);
static gboolean      read_header        (const guchar **p,
                                         gsize *size,
                                         KeyenceHeader *header,
                                         GError **error);
static gboolean      read_offset_table  (const guchar **p,
                                         gsize *size,
                                         KeyenceOffsetTable *offsettable,
                                         GError **error);
static gboolean      read_meas_conds    (const guchar **p,
                                         gsize *size,
                                         KeyenceMeasurementConditions *measconds,
                                         GError **error);
static gboolean      read_assembly_info (KeyenceFile *kfile,
                                         GError **error);
static gboolean      read_data_images   (KeyenceFile *kfile,
                                         GError **error);
static gboolean      read_color_images  (KeyenceFile *kfile,
                                         GError **error);
static gboolean      read_line_meas     (KeyenceFile *kfile,
                                         GError **error);
static gboolean      read_character_strs(KeyenceFile *kfile,
                                         GError **error);
static GwyDataField* create_data_field  (const KeyenceFalseColorImage *image,
                                         const KeyenceMeasurementConditions *measconds,
                                         gboolean is_height);
static GwyDataField* create_color_field (const KeyenceTrueColorImage *image,
                                         const KeyenceMeasurementConditions *measconds,
                                         gint channelid);
static GwyContainer* create_meta        (const KeyenceFile *kfile);
static void          add_data_field     (GwyContainer *data,
                                         gint *id,
                                         GwyDataField *dfield,
                                         GwyContainer *meta,
                                         const gchar *title,
                                         gint i,
                                         const gchar *gradient);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Keyence VK4 files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("keyence",
                           N_("Keyence VK4 data files (.vk4)"),
                           (GwyFileDetectFunc)&keyence_detect,
                           (GwyFileLoadFunc)&keyence_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
keyence_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE + KEYENCE_HEADER_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0
        && memcmp(fileinfo->head + 8, MAGIC0, MAGIC0_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
keyence_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    KeyenceFile *kfile;
    GwyDataField *dfield;
    GwyContainer *data = NULL, *meta = NULL;
    const guchar *p;
    gsize remsize;
    GError *err = NULL;
    guint i, id;

    kfile = g_new0(KeyenceFile, 1);
    if (!gwy_file_get_contents(filename, &kfile->buffer, &kfile->size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_free(kfile);
        return NULL;
    }

    remsize = kfile->size;
    p = kfile->buffer;

    if (!read_header(&p, &remsize, &kfile->header, error)
        || !read_offset_table(&p, &remsize, &kfile->offset_table, error)
        || !read_meas_conds(&p, &remsize, &kfile->meas_conds, error)
        || !read_assembly_info(kfile, error)
        || !read_data_images(kfile, error)
        || !read_color_images(kfile, error)
        || !read_line_meas(kfile, error)
        || !read_character_strs(kfile, error))
        goto fail;

    if (!kfile->nimages) {
        err_NO_DATA(error);
        goto fail;
    }

    data = gwy_container_new();
    meta = create_meta(kfile);
    id = 0;

    for (i = 0; i < G_N_ELEMENTS(kfile->height); i++) {
        if (kfile->height[i].data) {
            dfield = create_data_field(&kfile->height[i], &kfile->meas_conds,
                                       TRUE);
            add_data_field(data, &id, dfield, meta, "Height", i, NULL);
        }
    }

    for (i = 0; i < G_N_ELEMENTS(kfile->light); i++) {
        if (kfile->light[i].data) {
            dfield = create_data_field(&kfile->light[i], &kfile->meas_conds,
                                       FALSE);
            add_data_field(data, &id, dfield, meta, "Light", i, NULL);
        }
    }

    if (kfile->color_peak.data) {
        dfield = create_color_field(&kfile->color_peak, &kfile->meas_conds, 0);
        add_data_field(data, &id, dfield, meta, "Peak Red", -1, "RGB-Red");
        dfield = create_color_field(&kfile->color_peak, &kfile->meas_conds, 1);
        add_data_field(data, &id, dfield, meta, "Peak Green", -1, "RGB-Green");
        dfield = create_color_field(&kfile->color_peak, &kfile->meas_conds, 2);
        add_data_field(data, &id, dfield, meta, "Peak Blue", -1, "RGB-Blue");
    }

    if (kfile->color_light.data) {
        dfield = create_color_field(&kfile->color_light, &kfile->meas_conds, 0);
        add_data_field(data, &id, dfield, meta, "Light Red", -1, "RGB-Red");
        dfield = create_color_field(&kfile->color_light, &kfile->meas_conds, 1);
        add_data_field(data, &id, dfield, meta, "Light Green", -1, "RGB-Green");
        dfield = create_color_field(&kfile->color_light, &kfile->meas_conds, 2);
        add_data_field(data, &id, dfield, meta, "Light Blue", -1, "RGB-Blue");
    }

    g_object_unref(meta);

fail:
    free_file(kfile);
    return data;
}

static void
free_file(KeyenceFile *kfile)
{
    g_free(kfile->assembly_files);
    gwy_file_abandon_contents(kfile->buffer, kfile->size, NULL);
    g_free(kfile->char_strs.title);
    g_free(kfile->char_strs.lens_name);
    g_free(kfile);
}

static void
err_TRUNCATED(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("File is truncated."));
}

static gboolean
read_header(const guchar **p,
            gsize *size,
            KeyenceHeader *header,
            GError **error)
{
    gwy_debug("remaining size 0x%08lx", (gulong)*size);
    if (*size < KEYENCE_HEADER_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    get_CHARARRAY(header->magic, p);
    get_CHARARRAY(header->dll_version, p);
    get_CHARARRAY(header->file_type, p);
    if (memcmp(header->magic, MAGIC, MAGIC_SIZE) != 0
        || memcmp(header->file_type, MAGIC0, MAGIC0_SIZE) != 0) {
        err_FILE_TYPE(error, "Keyence VK4");
        return FALSE;
    }

    *size -= KEYENCE_HEADER_SIZE;
    return TRUE;
}

static gboolean
read_offset_table(const guchar **p,
                  gsize *size,
                  KeyenceOffsetTable *offsettable,
                  GError **error)
{
    guint i;

    gwy_debug("remaining size 0x%08lx", (gulong)*size);
    if (*size < KEYENCE_OFFSET_TABLE_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    offsettable->setting = gwy_get_guint32_le(p);
    offsettable->color_peak = gwy_get_guint32_le(p);
    offsettable->color_light = gwy_get_guint32_le(p);
    for (i = 0; i < G_N_ELEMENTS(offsettable->light); i++)
        offsettable->light[i] = gwy_get_guint32_le(p);
    for (i = 0; i < G_N_ELEMENTS(offsettable->height); i++)
        offsettable->height[i] = gwy_get_guint32_le(p);
    offsettable->color_peak_thumbnail = gwy_get_guint32_le(p);
    offsettable->color_thumbnail = gwy_get_guint32_le(p);
    offsettable->light_thumbnail = gwy_get_guint32_le(p);
    offsettable->height_thumbnail = gwy_get_guint32_le(p);
    offsettable->assemble = gwy_get_guint32_le(p);
    offsettable->line_measure = gwy_get_guint32_le(p);
    offsettable->line_thickness = gwy_get_guint32_le(p);
    offsettable->string_data = gwy_get_guint32_le(p);
    offsettable->reserved = gwy_get_guint32_le(p);

    *size -= KEYENCE_OFFSET_TABLE_SIZE;
    return TRUE;
}

static gboolean
read_meas_conds(const guchar **p,
                gsize *size,
                KeyenceMeasurementConditions *measconds,
                GError **error)
{
    guint i;

    gwy_debug("remaining size 0x%08lx", (gulong)*size);
    if (*size < KEYENCE_MEASUREMENT_CONDITIONS_MIN_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    measconds->size = gwy_get_guint32_le(p);
    if (*size < measconds->size) {
        err_TRUNCATED(error);
        return FALSE;
    }
    if (measconds->size < KEYENCE_MEASUREMENT_CONDITIONS_MIN_SIZE) {
        err_INVALID(error, "MeasurementConditions::Size");
        return FALSE;
    }

    measconds->year = gwy_get_guint32_le(p);
    measconds->month = gwy_get_guint32_le(p);
    measconds->day = gwy_get_guint32_le(p);
    measconds->hour = gwy_get_guint32_le(p);
    measconds->minute = gwy_get_guint32_le(p);
    measconds->second = gwy_get_guint32_le(p);
    measconds->diff_utc_by_minutes = gwy_get_gint32_le(p);
    measconds->image_attributes = gwy_get_guint32_le(p);
    measconds->user_interface_mode = gwy_get_guint32_le(p);
    measconds->color_composite_mode = gwy_get_guint32_le(p);
    measconds->num_layer = gwy_get_guint32_le(p);
    measconds->run_mode = gwy_get_guint32_le(p);
    measconds->peak_mode = gwy_get_guint32_le(p);
    measconds->sharpening_level = gwy_get_guint32_le(p);
    measconds->speed = gwy_get_guint32_le(p);
    measconds->distance = gwy_get_guint32_le(p);
    measconds->pitch = gwy_get_guint32_le(p);
    measconds->optical_zoom = gwy_get_guint32_le(p);
    measconds->num_line = gwy_get_guint32_le(p);
    measconds->line0_pos = gwy_get_guint32_le(p);
    for (i = 0; i < G_N_ELEMENTS(measconds->reserved1); i++)
        measconds->reserved1[i] = gwy_get_guint32_le(p);
    measconds->lens_mag = gwy_get_guint32_le(p);
    measconds->pmt_gain_mode = gwy_get_guint32_le(p);
    measconds->pmt_gain = gwy_get_guint32_le(p);
    measconds->pmt_offset = gwy_get_guint32_le(p);
    measconds->nd_filter = gwy_get_guint32_le(p);
    measconds->reserved2 = gwy_get_guint32_le(p);
    measconds->persist_count = gwy_get_guint32_le(p);
    measconds->shutter_speed_mode = gwy_get_guint32_le(p);
    measconds->shutter_speed = gwy_get_guint32_le(p);
    measconds->white_balance_mode = gwy_get_guint32_le(p);
    measconds->white_balance_red = gwy_get_guint32_le(p);
    measconds->white_balance_blue = gwy_get_guint32_le(p);
    measconds->camera_gain = gwy_get_guint32_le(p);
    measconds->plane_compensation = gwy_get_guint32_le(p);
    measconds->xy_length_unit = gwy_get_guint32_le(p);
    measconds->z_length_unit = gwy_get_guint32_le(p);
    measconds->xy_decimal_place = gwy_get_guint32_le(p);
    measconds->z_decimal_place = gwy_get_guint32_le(p);
    measconds->x_length_per_pixel = gwy_get_guint32_le(p);
    measconds->y_length_per_pixel = gwy_get_guint32_le(p);
    measconds->z_length_per_digit = gwy_get_guint32_le(p);
    for (i = 0; i < G_N_ELEMENTS(measconds->reserved3); i++)
        measconds->reserved3[i] = gwy_get_guint32_le(p);
    measconds->light_filter_type = gwy_get_guint32_le(p);
    measconds->reserved4 = gwy_get_guint32_le(p);
    measconds->gamma_reverse = gwy_get_guint32_le(p);
    measconds->gamma = gwy_get_guint32_le(p);
    measconds->gamma_offset = gwy_get_guint32_le(p);
    measconds->ccd_bw_offset = gwy_get_guint32_le(p);
    measconds->numerical_aperture = gwy_get_guint32_le(p);
    measconds->head_type = gwy_get_guint32_le(p);
    measconds->pmt_gain2 = gwy_get_guint32_le(p);
    measconds->omit_color_image = gwy_get_guint32_le(p);
    measconds->lens_id = gwy_get_guint32_le(p);
    measconds->light_lut_mode = gwy_get_guint32_le(p);
    measconds->light_lut_in0 = gwy_get_guint32_le(p);
    measconds->light_lut_out0 = gwy_get_guint32_le(p);
    measconds->light_lut_in1 = gwy_get_guint32_le(p);
    measconds->light_lut_out1 = gwy_get_guint32_le(p);
    measconds->light_lut_in2 = gwy_get_guint32_le(p);
    measconds->light_lut_out2 = gwy_get_guint32_le(p);
    measconds->light_lut_in3 = gwy_get_guint32_le(p);
    measconds->light_lut_out3 = gwy_get_guint32_le(p);
    measconds->light_lut_in4 = gwy_get_guint32_le(p);
    measconds->light_lut_out4 = gwy_get_guint32_le(p);
    measconds->upper_position = gwy_get_guint32_le(p);
    measconds->lower_position = gwy_get_guint32_le(p);
    measconds->light_effective_bit_depth = gwy_get_guint32_le(p);
    measconds->height_effective_bit_depth = gwy_get_guint32_le(p);

    *size -= measconds->size;
    return TRUE;
}

static gboolean
read_assembly_info(KeyenceFile *kfile,
                   GError **error)
{
    const guchar *p = kfile->buffer;
    gsize size = kfile->size;
    guint off = kfile->offset_table.assemble;
    guint nfiles, i, j;

    gwy_debug("0x%08x", off);
    if (!off)
        return TRUE;

    if (size <= KEYENCE_ASSEMBLY_HEADERS_SIZE
        || off > size - KEYENCE_ASSEMBLY_HEADERS_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    p += off;

    kfile->assembly_info.size = gwy_get_guint32_le(&p);
    kfile->assembly_info.file_type = gwy_get_guint16_le(&p);
    kfile->assembly_info.stage_type = gwy_get_guint16_le(&p);
    kfile->assembly_info.x_position = gwy_get_guint32_le(&p);
    kfile->assembly_info.y_position = gwy_get_guint32_le(&p);

    kfile->assembly_conds.auto_adjustment = *(p++);
    kfile->assembly_conds.source = *(p++);
    kfile->assembly_conds.thin_out = gwy_get_guint16_le(&p);
    kfile->assembly_conds.count_x = gwy_get_guint16_le(&p);
    kfile->assembly_conds.count_y = gwy_get_guint16_le(&p);

    nfiles = kfile->assembly_conds.count_x * kfile->assembly_conds.count_y;
    if (!nfiles)
        return TRUE;

    if ((size - KEYENCE_ASSEMBLY_HEADERS_SIZE - off)/nfiles
        < KEYENCE_ASSEMBLY_FILE_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    kfile->assembly_nfiles = nfiles;
    kfile->assembly_files = g_new(KeyenceAssemblyFile, nfiles);
    for (i = 0; i < nfiles; i++) {
        KeyenceAssemblyFile *kafile = kfile->assembly_files + i;

        for (j = 0; j < G_N_ELEMENTS(kafile->source_file); j++)
            kafile->source_file[j] = gwy_get_guint16_le(&p);
        kafile->pos_x = *(p++);
        kafile->pos_y = *(p++);
        kafile->datums_pos = *(p++);
        kafile->fix_distance = *(p++);
        kafile->distance_x = gwy_get_guint32_le(&p);
        kafile->distance_y = gwy_get_guint32_le(&p);
    }

    return TRUE;
}

static gboolean
read_data_image(KeyenceFile *kfile,
                KeyenceFalseColorImage *image,
                guint offset,
                GError **error)
{
    const guchar *p = kfile->buffer;
    gsize size = kfile->size;
    guint bps;

    gwy_debug("0x%08x", offset);
    if (!offset)
        return TRUE;

    if (size <= KEYENCE_FALSE_COLOR_IMAGE_MIN_SIZE
        || offset > size - KEYENCE_FALSE_COLOR_IMAGE_MIN_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    p += offset;
    image->width = gwy_get_guint32_le(&p);
    if (err_DIMENSION(error, image->width))
        return FALSE;
    image->height = gwy_get_guint32_le(&p);
    if (err_DIMENSION(error, image->height))
        return FALSE;

    image->bit_depth = gwy_get_guint32_le(&p);
    if (image->bit_depth != 8
        && image->bit_depth != 16
        && image->bit_depth != 32) {
        err_BPP(error, image->bit_depth);
        return FALSE;
    }
    bps = image->bit_depth/8;

    image->compression = gwy_get_guint32_le(&p);
    image->byte_size = gwy_get_guint32_le(&p);
    if (err_SIZE_MISMATCH(error,
                          image->width*image->height*bps,
                          image->byte_size,
                          TRUE))
        return FALSE;

    image->palette_range_min = gwy_get_guint32_le(&p);
    image->palette_range_max = gwy_get_guint32_le(&p);
    memcpy(image->palette, p, sizeof(image->palette));
    p += sizeof(image->palette);

    if (size - offset - KEYENCE_FALSE_COLOR_IMAGE_MIN_SIZE < image->byte_size) {
        err_TRUNCATED(error);
        return FALSE;
    }
    image->data = p;
    kfile->nimages++;

    return TRUE;
}

static gboolean
read_data_images(KeyenceFile *kfile,
                 GError **error)
{
    const KeyenceOffsetTable *offtable = &kfile->offset_table;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(kfile->light); i++) {
        if (!read_data_image(kfile, &kfile->light[i], offtable->light[i],
                             error))
            return FALSE;
    }
    for (i = 0; i < G_N_ELEMENTS(kfile->height); i++) {
        if (!read_data_image(kfile, &kfile->height[i], offtable->height[i],
                             error))
            return FALSE;
    }
    return TRUE;
}

static gboolean
read_color_image(KeyenceFile *kfile,
                 KeyenceTrueColorImage *image,
                 guint offset,
                 GError **error)
{
    const guchar *p = kfile->buffer;
    gsize size = kfile->size;
    guint bps;

    gwy_debug("0x%08x", offset);
    if (!offset)
        return TRUE;

    if (size <= KEYENCE_TRUE_COLOR_IMAGE_MIN_SIZE
        || offset > size - KEYENCE_TRUE_COLOR_IMAGE_MIN_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    p += offset;
    image->width = gwy_get_guint32_le(&p);
    if (err_DIMENSION(error, image->width))
        return FALSE;
    image->height = gwy_get_guint32_le(&p);
    if (err_DIMENSION(error, image->height))
        return FALSE;

    image->bit_depth = gwy_get_guint32_le(&p);
    if (image->bit_depth != 24) {
        err_BPP(error, image->bit_depth);
        return FALSE;
    }
    bps = image->bit_depth/8;

    image->compression = gwy_get_guint32_le(&p);
    image->byte_size = gwy_get_guint32_le(&p);
    if (err_SIZE_MISMATCH(error,
                          image->width*image->height*bps,
                          image->byte_size,
                          TRUE))
        return FALSE;

    if (size - offset - KEYENCE_TRUE_COLOR_IMAGE_MIN_SIZE < image->byte_size) {
        err_TRUNCATED(error);
        return FALSE;
    }
    image->data = p;

    return TRUE;
}

static gboolean
read_line_meas(KeyenceFile *kfile,
               GError **error)
{
    KeyenceLineMeasurement *linemeas;
    const guchar *p = kfile->buffer;
    gsize size = kfile->size;
    guint off = kfile->offset_table.line_measure;
    guint i;

    gwy_debug("0x%08x", off);
    if (!off)
        return TRUE;

    if (size <= KEYENCE_LINE_MEASUREMENT_SIZE
        || off > size - KEYENCE_LINE_MEASUREMENT_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }

    p += off;
    linemeas = &kfile->line_measure;

    linemeas->size = gwy_get_guint32_le(&p);
    if (size < KEYENCE_LINE_MEASUREMENT_SIZE) {
        err_TRUNCATED(error);
        return FALSE;
    }
    linemeas->line_width = gwy_get_guint32_le(&p);
    /* XXX: We should use the real length even though the format description
     * seems to specify a fixed length.  Also note that only the first data
     * block is supposed to be used; the rest it reserved. */
    for (i = 0; i < G_N_ELEMENTS(linemeas->light); i++) {
        linemeas->light[i] = p;
        p += KEYENCE_LINE_MEASUREMENT_LEN*sizeof(guint16);
    }
    for (i = 0; i < G_N_ELEMENTS(linemeas->height); i++) {
        linemeas->height[i] = p;
        p += KEYENCE_LINE_MEASUREMENT_LEN*sizeof(guint32);
    }

    return TRUE;
}

static gboolean
read_color_images(KeyenceFile *kfile,
                  GError **error)
{
    const KeyenceOffsetTable *offtable = &kfile->offset_table;

    if (!read_color_image(kfile, &kfile->color_peak, offtable->color_peak,
                          error))
        return FALSE;
    if (!read_color_image(kfile, &kfile->color_light, offtable->color_light,
                          error))
        return FALSE;

    return TRUE;
}

static gchar*
read_character_str(const guchar **p,
                   gsize *remsize,
                   GError **error)
{
    GError *err = NULL;
    gchar *s;
    guint len;

    if (*remsize < sizeof(guint32)) {
        err_TRUNCATED(error);
        return NULL;
    }

    len = gwy_get_guint32_le(p);
    gwy_debug("%u", len);
    *remsize -= sizeof(guint32);

    if (!len)
        return g_strdup("");

    if (*remsize/2 < len) {
        err_TRUNCATED(error);
        return NULL;
    }

    s = g_utf16_to_utf8((const gunichar2*)*p, len, NULL, NULL, &err);
    if (!s) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot convert string from UTF-16: %s"),
                    err->message);
        g_clear_error(&err);
        return FALSE;
    }
    gwy_debug("%s", s);

    *remsize -= 2*len;
    *p += 2*len;
    return s;
}

static gboolean
read_character_strs(KeyenceFile *kfile,
                    GError **error)
{
    KeyenceCharacterStrings *charstrs;
    const guchar *p = kfile->buffer;
    gsize remsize = kfile->size;
    guint off = kfile->offset_table.string_data;

    gwy_debug("0x%08x", off);
    if (!off)
        return TRUE;

    if (remsize < off) {
        err_TRUNCATED(error);
        return FALSE;
    }

    p += off;
    remsize -= off;
    charstrs = &kfile->char_strs;
    if (!(charstrs->title = read_character_str(&p, &remsize, error))
        || !(charstrs->lens_name = read_character_str(&p, &remsize, error)))
        return FALSE;

    return TRUE;
}

static GwyDataField*
create_data_field(const KeyenceFalseColorImage *image,
                  const KeyenceMeasurementConditions *measconds,
                  gboolean is_height)
{
    guint w = image->width, h = image->height;
    gdouble dx = measconds->x_length_per_pixel * Picometre;
    gdouble dy = measconds->y_length_per_pixel * Picometre;
    GwyRawDataType datatype = GWY_RAW_DATA_UINT8;
    GwyDataField *dfield;
    gdouble *data;
    gdouble q;

    /* The -1 is from comparison with original software. */
    dfield = gwy_data_field_new(w, h, dx*(w - 1.0), dy*(h - 1.0), FALSE);
    if (image->bit_depth == 16)
        datatype = GWY_RAW_DATA_UINT16;
    else if (image->bit_depth == 32)
        datatype = GWY_RAW_DATA_UINT32;

    q = (is_height
         ? measconds->z_length_per_digit * Picometre
         : pow(0.5, image->bit_depth));

    data = gwy_data_field_get_data(dfield);
    gwy_convert_raw_data(image->data, w*h, 1,
                         datatype, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         data, q, 0.0);

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    if (is_height)
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "m");

    return dfield;
}

static GwyDataField*
create_color_field(const KeyenceTrueColorImage *image,
                   const KeyenceMeasurementConditions *measconds,
                   gint channelid)
{
    guint w = image->width, h = image->height;
    gdouble dx = measconds->x_length_per_pixel * Picometre;
    gdouble dy = measconds->y_length_per_pixel * Picometre;
    GwyDataField *dfield;
    gdouble *data;

    /* The -1 is from comparison with original software. */
    dfield = gwy_data_field_new(w, h, dx*(w - 1.0), dy*(h - 1.0), FALSE);
    data = gwy_data_field_get_data(dfield);
    gwy_convert_raw_data(image->data + channelid, w*h, 3,
                         GWY_RAW_DATA_UINT8, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         data, 1.0/255.0, 0.0);

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    return dfield;
}

#define store_int(c,n,i) \
    g_snprintf(buf, sizeof(buf), "%d", (i)); \
    gwy_container_set_const_string_by_name((c), (n), buf);

#define store_uint(c,n,i) \
    g_snprintf(buf, sizeof(buf), "%u", (i)); \
    gwy_container_set_const_string_by_name((c), (n), buf);

#define store_int2(c,n,i,u) \
    g_snprintf(buf, sizeof(buf), "%d %s", (i), (u)); \
    gwy_container_set_const_string_by_name((c), (n), buf);

#define store_uint2(c,n,i,u) \
    g_snprintf(buf, sizeof(buf), "%u %s", (i), (u)); \
    gwy_container_set_const_string_by_name((c), (n), buf);

#define store_float(c,n,v) \
    g_snprintf(buf, sizeof(buf), "%g", (v)); \
    gwy_container_set_const_string_by_name((c), (n), buf);

static GwyContainer*
create_meta(const KeyenceFile *kfile)
{
    const KeyenceMeasurementConditions *measconds = &kfile->meas_conds;
    const KeyenceCharacterStrings *charstrs = &kfile->char_strs;
    GwyContainer *meta = gwy_container_new();
    gchar buf[48];

    g_snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
               kfile->header.dll_version[3], kfile->header.dll_version[2],
               kfile->header.dll_version[1], kfile->header.dll_version[0]);
    gwy_container_set_const_string_by_name(meta, "DLL version", buf);

    g_snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
               measconds->year, measconds->month, measconds->day,
               measconds->hour, measconds->minute, measconds->second);
    gwy_container_set_const_string_by_name(meta, "Date", buf);

    store_int2(meta, "Time difference to UTC", measconds->diff_utc_by_minutes,
               "min");
    store_uint(meta, "Image attributes", measconds->image_attributes);
    store_uint(meta, "User interface mode", measconds->user_interface_mode);
    store_uint(meta, "Color composition mode", measconds->color_composite_mode);
    store_uint(meta, "Image layer number", measconds->num_layer);
    store_uint(meta, "Run mode", measconds->run_mode);
    store_uint(meta, "Peak mode", measconds->peak_mode);
    store_uint(meta, "Sharpening level", measconds->sharpening_level);
    store_uint(meta, "Speed", measconds->speed);
    store_uint2(meta, "Distance", measconds->distance, "nm");
    store_uint2(meta, "Pitch", measconds->pitch, "nm");
    store_float(meta, "Optical zoom", measconds->optical_zoom/10.0);
    store_uint(meta, "Number of lines", measconds->num_line);
    store_uint(meta, "First line position", measconds->line0_pos);
    store_float(meta, "Lens magnification", measconds->lens_mag/10.0);
    store_uint(meta, "PMT gain mode", measconds->pmt_gain_mode);
    store_uint(meta, "PMT gain", measconds->pmt_gain);
    store_uint(meta, "PMT offset", measconds->pmt_offset);
    store_uint(meta, "ND filter", measconds->nd_filter);
    store_uint(meta, "Image average frequency", measconds->persist_count);
    store_uint(meta, "Shutter speed mode", measconds->shutter_speed_mode);
    store_uint(meta, "Shutter speed", measconds->shutter_speed);
    store_uint(meta, "White balance mode", measconds->white_balance_mode);
    store_uint(meta, "White balance red", measconds->white_balance_red);
    store_uint(meta, "White balance blue", measconds->white_balance_blue);
    store_uint2(meta, "Camera gain", 6*measconds->camera_gain, "dB");
    store_uint(meta, "Plane compensation", measconds->plane_compensation);
    store_uint(meta, "Light filter type", measconds->light_filter_type);
    store_uint(meta, "Gamma reverse", measconds->gamma_reverse);
    store_float(meta, "Gamma", measconds->gamma/100.0);
    store_float(meta, "Gamma correction offset",
                measconds->gamma_offset/65536.0);
    store_float(meta, "CCD BW offset", measconds->ccd_bw_offset/100.0);
    store_float(meta, "Numerical aperture",
                measconds->numerical_aperture/1000.0);
    store_uint(meta, "Head type", measconds->head_type);
    store_uint(meta, "PMT gain 2", measconds->pmt_gain2);
    store_uint(meta, "Omit color image", measconds->omit_color_image);
    store_uint(meta, "Lens ID", measconds->lens_id);
    store_uint(meta, "Light LUT mode", measconds->light_lut_mode);
    store_uint(meta, "Light LUT input 0", measconds->light_lut_in0);
    store_uint(meta, "Light LUT output 0", measconds->light_lut_out0);
    store_uint(meta, "Light LUT input 1", measconds->light_lut_in1);
    store_uint(meta, "Light LUT output 1", measconds->light_lut_out1);
    store_uint(meta, "Light LUT input 2", measconds->light_lut_in2);
    store_uint(meta, "Light LUT output 2", measconds->light_lut_out2);
    store_uint(meta, "Light LUT input 3", measconds->light_lut_in3);
    store_uint(meta, "Light LUT output 3", measconds->light_lut_out3);
    store_uint(meta, "Light LUT input 4", measconds->light_lut_in4);
    store_uint(meta, "Light LUT output 4", measconds->light_lut_out4);
    store_uint2(meta, "Upper position", measconds->upper_position, "nm");
    store_uint2(meta, "Lower position", measconds->lower_position, "nm");
    store_uint(meta, "Light effective bit depth",
               measconds->light_effective_bit_depth);
    store_uint(meta, "Height effective bit depth",
               measconds->height_effective_bit_depth);

    if (charstrs->title && strlen(charstrs->title))
        gwy_container_set_const_string_by_name(meta, "Title", charstrs->title);
    if (charstrs->lens_name && strlen(charstrs->lens_name))
        gwy_container_set_const_string_by_name(meta, "Lens name",
                                               charstrs->lens_name);

    return meta;
}

static void
add_data_field(GwyContainer *data, gint *id,
               GwyDataField *dfield, GwyContainer *meta,
               const gchar *title, gint i, const gchar *gradient)
{
    GwyContainer *tmpmeta;
    GQuark quark;
    gchar key[48];

    quark = gwy_app_get_data_key_for_id(*id);
    gwy_container_set_object(data, quark, dfield);
    g_object_unref(dfield);

    g_snprintf(key, sizeof(key), "/%u/data/title", *id);
    if (i >= 0) {
        gchar *t = g_strdup_printf("%s %u", title, i);
        gwy_container_set_string_by_name(data, key, t);
    }
    else
        gwy_container_set_const_string_by_name(data, key, title);

    g_snprintf(key, sizeof(key), "/%u/meta", *id);
    tmpmeta = gwy_container_duplicate(meta);
    gwy_container_set_object_by_name(data, key, tmpmeta);
    g_object_unref(tmpmeta);

    if (gradient) {
        g_snprintf(key, sizeof(key), "/%u/base/palette", *id);
        gwy_container_set_const_string_by_name(data, key, gradient);
    }

    (*id)++;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
