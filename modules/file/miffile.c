/*
 *  @(#) $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

/* TODO:
 * Unknown types:
 * sizeof cardinal (using 8)
 * sizeof integer (using 4)
 * sizeof smallint (using 2)
 *
 * Guessed types:
 * sizeof enum (seems to be 16byte string)
 * TPoint member type (seems to be 32bit int)
 * TFPoint member type (seems to be double)
 * TF3DPoint member type (seems to be double)
 * CMaxCurves (seems to be 8)
 * sizeof boolean (seems to be 1)
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-mif-spm">
 *   <comment>DME MIF SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="MIF\x01\x00"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * DME MIF
 * .mif
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <stdio.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "MIF\x01\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

enum {
    HEADER_SIZE = 491,
    BLOCK_SIZE = 2*4,
    INFO_ITEM_SIZE = BLOCK_SIZE + 1,
    INFO_N_IMAGES = 512,
    MAX_CURVES = 8,
    IMAGE_HEADER_START_SIZE = 19 + 20 + 255,
    SCAN_SETUP_SIZE_1_2 = 2*(4 + 8 + 8 + 8) + 4 + 2 + 4 + 3*16,
    SCAN_SETUP_SIZE_1_3 = 2*(4 + 8 + 8 + 8) + 4 + 2 + 1 + 4 + 1 + 1 + 1,
    IMAGE_CONFIGURATION_SIZE_1_3 = 8 + 3*8 + 3*1 + 1 + 1 + 1 + 1 + 4 + 2*1 + 50,
};

typedef struct {
    gsize offset;
    gsize size;
} MIFBlock;

typedef struct {
    MIFBlock image;
    guint image_type;
} MIFInfoItem;

typedef struct {
    gdouble x, y;
} MIFPoint;

typedef struct {
    guint xres, yres;
    gdouble xreal, yreal;
    gdouble xoff, yoff;
    gdouble xscandir, yscandir;
    gdouble scan_speed;
    gdouble loop_gain;
    guint sample_pause;
    guchar loop_filter[16];
    guchar lead_filter[16];
    guchar lead_lag_mix[16];
} MIFScanSetup;

typedef struct {
    gdouble scan_int_to_meter;
    gdouble xcal, ycal, zcal;
    guchar direction[16];
    guchar signal[16];
    guchar scan_head[16];
    guint scan_head_code;
    guint contact_mode;
    guint detector;
    guchar scan_mode[16];
    gboolean z_linearized;
    gdouble z_correction;
    gboolean is_zcorrected;
    gboolean is_flattened;
    guchar scan_head_name[50];
} MIFImageConfiguration;

typedef struct {
    guint units;
    gchar units_str_1_0[4];
    gchar units_str_1_1[10];
    gboolean show;
} MIFAxisInfo;

typedef struct {
    guint ms, ls;
    guint index;
    guchar workstation[40];
} MIFUniqueID;

typedef struct {
    MIFUniqueID primary;
    MIFUniqueID secondary;
} MIFImageID;

typedef struct {
    gchar time[19];
    gchar title[20];
    gchar comment[255];
    MIFScanSetup setup;
    MIFImageConfiguration configuration;
    gdouble tunnel_current;
    gdouble bias_voltage;
    gdouble force;
    gdouble low_fraction;
    gdouble high_fraction;
    gboolean as_measured;
    gboolean unit_is_valid;
    guint n_curves;
    MIFPoint curve_points[MAX_CURVES];
    MIFAxisInfo z_axis;
    MIFImageID id;
} MIFImageHeader;

typedef struct {
    gchar magic[3];
    guint software_version;
    guint file_version;
    gchar time[19];
    gchar comment[255];
    guint nimages;
    MIFBlock info;
    gchar unused[200];
} MIFHeader;

typedef struct {
    MIFHeader header;
    MIFInfoItem images[INFO_N_IMAGES];
} MIFFile;

static gboolean        module_register       (void);
static gint            mif_detect       (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*   mif_load         (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports DME MIF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("miffile",
                           N_("DME MIF files (.mif)"),
                           (GwyFileDetectFunc)&mif_detect,
                           (GwyFileLoadFunc)&mif_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
mif_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;
    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size > HEADER_SIZE
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static gboolean
mif_read_header(const guchar *buffer,
                gsize size,
                MIFHeader *header,
                GError **error)
{
    const guchar *p = buffer;

    if (size <= HEADER_SIZE) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        err_FILE_TYPE(error, "MIF");
        return FALSE;
    }

    gwy_clear(header, 1);
    get_CHARARRAY(header->magic, &p);
    header->software_version = gwy_get_guint16_be(&p);
    header->file_version = gwy_get_guint16_be(&p);
    gwy_debug("sw version: %u.%u, file version: %u.%u",
              header->software_version/0x100, header->software_version % 0x100,
              header->file_version/0x100, header->file_version % 0x100);
    get_CHARARRAY(header->time, &p);
    get_CHARARRAY(header->comment, &p);
    header->nimages = gwy_get_guint16_le(&p);
    gwy_debug("n images: %u", header->nimages);
    header->info.offset = gwy_get_guint32_le(&p);
    header->info.size = gwy_get_guint32_le(&p);
    gwy_debug("info offset: %zu, info size: %zu", header->info.offset, header->info.size);
    get_CHARARRAY(header->unused, &p);

    if (header->info.offset < HEADER_SIZE
        || header->info.offset > size
        || header->info.size > size
        || header->info.offset + header->info.size > size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File information block is outside the file."));
        return FALSE;
    }

    return TRUE;
}

static gboolean
mif_read_image_items(MIFInfoItem *items,
                     const guchar *buffer,
                     G_GNUC_UNUSED gsize size,
                     const MIFBlock *block,
                     GError **error)
{
    const guchar *p;
    guint i;
    gsize item_size;

    item_size = block->size/INFO_N_IMAGES;
    gwy_debug("item_size: %zu", item_size);
    if (item_size < INFO_ITEM_SIZE || block->size % INFO_N_IMAGES) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File information block size is invalid."));
        return FALSE;
    }

    for (i = 0; i < INFO_N_IMAGES; i++) {
        p = buffer + block->offset + i*item_size;
        items[i].image.offset = gwy_get_guint32_le(&p);
        items[i].image.size = gwy_get_guint32_le(&p);
        items[i].image_type = *(p++);
        if (items[i].image.size || items[i].image_type) {
            gwy_debug("item #%u: type: %u, offset: %zu, size: %zu", i, items[i].image_type, items[i].image.offset, items[i].image.size);
        }
    }

    return TRUE;
}

static void
err_IMAGE_HEADER_TOO_SHORT(GError **error)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Image header record is too short."));
}

static gboolean
mif_read_scan_setup(MIFScanSetup *setup,
                    const guchar **p,
                    gsize size,
                    guint file_version,
                    GError **error)
{
    if (file_version >= 0x103 && file_version <= 0x107) {
        if (size < SCAN_SETUP_SIZE_1_2) {
            err_IMAGE_HEADER_TOO_SHORT(error);
            return FALSE;
        }
        setup->xres = gwy_get_guint32_le(p);
        setup->yres = gwy_get_guint32_le(p);
        gwy_debug("xres: %u, yres: %u", setup->xres, setup->yres);
        setup->xreal = gwy_get_gdouble_le(p);
        setup->yreal = gwy_get_gdouble_le(p);
        gwy_debug("xreal: %g, yreal: %g", setup->xreal, setup->yreal);
        setup->xoff = gwy_get_gdouble_le(p);
        setup->yoff = gwy_get_gdouble_le(p);
        gwy_debug("xoff: %g, yoff: %g", setup->xoff, setup->yoff);
        setup->xscandir = gwy_get_gdouble_le(p);
        setup->yscandir = gwy_get_gdouble_le(p);
        gwy_debug("xscandir: %g, yscandir: %g", setup->xscandir, setup->yscandir);
        setup->scan_speed = gwy_get_gfloat_le(p);
        gwy_debug("scan_speed: %g", setup->scan_speed);
        setup->sample_pause = gwy_get_guint16_le(p);
        gwy_debug("sample_pause: %u", setup->sample_pause);
        setup->loop_gain = gwy_get_gfloat_le(p);
        gwy_debug("loop_gain: %g", setup->loop_gain);
        get_CHARARRAY(setup->loop_filter, p);
        gwy_debug("loop_filter: %.*s", (guint)sizeof(setup->loop_filter), setup->loop_filter);
        get_CHARARRAY(setup->lead_filter, p);
        gwy_debug("lead_filter: %.*s", (guint)sizeof(setup->lead_filter), setup->lead_filter);
        get_CHARARRAY(setup->lead_lag_mix, p);
        gwy_debug("lead_lag_mix: %.*s", (guint)sizeof(setup->lead_lag_mix), setup->lead_lag_mix);
    }
    else {
        g_return_val_if_reached(FALSE);
    }

    return TRUE;
}

static gboolean
mif_read_image_configuration(MIFImageConfiguration *config,
                             const guchar **p,
                             gsize size,
                             guint file_version,
                             GError **error)
{
    if (file_version >= 0x107 && file_version <= 0x109) {
        if (size < IMAGE_CONFIGURATION_SIZE_1_3) {
            err_IMAGE_HEADER_TOO_SHORT(error);
            return FALSE;
        }
        config->scan_int_to_meter = gwy_get_gdouble_le(p);
        gwy_debug("scan_int_to_meter: %g", config->scan_int_to_meter);
        config->xcal = gwy_get_gdouble_le(p);
        config->ycal = gwy_get_gdouble_le(p);
        config->zcal = gwy_get_gdouble_le(p);
        gwy_debug("calibration: %g %g %g", config->xcal, config->ycal, config->zcal);
        get_CHARARRAY(config->direction, p);
        gwy_debug("direction: %.*s", (guint)sizeof(config->direction), config->direction);
        get_CHARARRAY(config->signal, p);
        gwy_debug("signal: %.*s", (guint)sizeof(config->signal), config->signal);
        get_CHARARRAY(config->scan_head, p);
        gwy_debug("scan_head: %.*s", (guint)sizeof(config->scan_head), config->scan_head);
        config->scan_head_code = *((*p)++);
        get_CHARARRAY(config->scan_mode, p);
        gwy_debug("scan_mode: %.*s", (guint)sizeof(config->scan_mode), config->scan_mode);
        config->z_linearized = !!*((*p)++);
        gwy_debug("z_linearized: %s", config->z_linearized ? "TRUE" : "FALSE");
        config->z_correction = gwy_get_gfloat_le(p);
        gwy_debug("z_correction: %g", config->z_correction);
        config->is_zcorrected = !!*((*p)++);
        gwy_debug("is_zcorrected: %s", config->is_zcorrected ? "TRUE" : "FALSE");
        config->is_flattened = !!*((*p)++);
        gwy_debug("is_flattened: %s", config->is_flattened ? "TRUE" : "FALSE");
        get_CHARARRAY(config->scan_head_name, p);
        gwy_debug("scan_head_name: %.*s", (guint)sizeof(config->scan_head_name), config->scan_head_name);
    }
    else {
        g_return_val_if_reached(FALSE);
    }

    return TRUE;
}

static gboolean
mif_read_image_header(MIFImageHeader *header,
                      const guchar *buffer,
                      gsize size,
                      guint file_version,
                      GError **error)
{
    const guchar *p = buffer;
    guint i;

    gwy_clear(header, 1);

    if (size < IMAGE_HEADER_START_SIZE) {
        err_IMAGE_HEADER_TOO_SHORT(error);
        return FALSE;
    }
    get_CHARARRAY(header->time, &p);
    get_CHARARRAY(header->title, &p);
    get_CHARARRAY(header->comment, &p);
    gwy_debug("image <%.*s>", (guint)sizeof(header->title), header->title);

    if (!mif_read_scan_setup(&header->setup,
                             &p, size - (p - buffer), file_version,
                             error))
        return FALSE;
    if (!mif_read_image_configuration(&header->configuration,
                                      &p, size - (p - buffer), file_version,
                                      error))
        return FALSE;

    if (file_version == 0x107) {
        /* TODO: Check size */
        header->tunnel_current = gwy_get_gfloat_le(&p);
        gwy_debug("tunnel_current: %g", header->tunnel_current);
        header->bias_voltage = gwy_get_gfloat_le(&p);
        gwy_debug("bias_voltage: %g", header->bias_voltage);
        header->force = gwy_get_gfloat_le(&p);
        gwy_debug("force: %g", header->force);
        header->as_measured = !!*(p++);
        gwy_debug("as_measured: %s", header->as_measured ? "TRUE" : "FALSE");
        header->n_curves = gwy_get_guint32_le(&p);
        gwy_debug("n_curves: %u", header->n_curves);
        for (i = 0; i < MAX_CURVES; i++) {
            header->curve_points[i].x = gwy_get_guint32_le(&p);
            header->curve_points[i].y = gwy_get_guint32_le(&p);
        }
        header->low_fraction = gwy_get_gfloat_le(&p);
        header->high_fraction = gwy_get_gfloat_le(&p);
        gwy_debug("low_fraction: %g, high_fraction: %g", header->low_fraction, header->high_fraction);
        /* TODO: ZAxis, ImageID */
    }
    else {
        g_return_val_if_reached(FALSE);
    }

    return TRUE;
}

static GwyContainer*
mif_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwySIUnit *units = NULL;
    gint32 power10;
    MIFFile mfile;
    GwyDataField *dfield;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!mif_read_header(buffer, size, &mfile.header, error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    /* TODO: Check file version */
    if (!mif_read_image_items(mfile.images, buffer, size, &mfile.header.info,
                              error)) {
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    for (i = 0; i < INFO_N_IMAGES; i++) {
        MIFInfoItem *item = mfile.images + i;
        MIFImageHeader image_header;

        if (!item->image.size)
            continue;
        if (item->image.offset + item->image.size > size) {
            /* TODO: Error */
            return FALSE;
        }

        if (!mif_read_image_header(&image_header,
                                   buffer + item->image.offset,
                                   item->image.size,
                                   mfile.header.file_version,
                                   error))
            goto fail;
    }

    err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
