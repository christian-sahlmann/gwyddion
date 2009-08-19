/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-rhk-sm4-spm">
 *   <comment>RHK SM4 SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="2" value="S\0T\0i\0M\0a\0g\0e\0 \0\060\0\060\0\065\0.\0"/>
 *   </magic>
 *   <glob pattern="*.sm4"/>
 *   <glob pattern="*.SM4"/>
 * </mime-type>
 **/
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwydgets/gwygraphcurvemodel.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"
#include "get.h"

/* Ugly Microsoft UTF-16...
 * It reads: `STiMage 004.NNN N', but we do not check the NNN N */
static const guchar MAGIC[] = {
  0x53, 0x00, 0x54, 0x00, 0x69, 0x00, 0x4d, 0x00, 0x61, 0x00, 0x67, 0x00,
  0x65, 0x00, 0x20, 0x00, 0x30, 0x00, 0x30, 0x00, 0x35, 0x00, 0x2e, 0x00,
};

#define EXTENSION ".sm4"

enum {
    MAGIC_OFFSET = 2,
    MAGIC_SIZE = G_N_ELEMENTS(MAGIC),
    MAGIC_TOTAL_SIZE = 36,   /* including the version part we do not check */
    HEADER_SIZE = MAGIC_OFFSET + MAGIC_TOTAL_SIZE + 5*4,
    OBJECT_SIZE = 3*4,
    GUID_SIZE = 16,
    PAGE_INDEX_HEADER_SIZE = 4*4,
    PAGE_INDEX_ARRAY_SIZE = GUID_SIZE + 4*4,
    PAGE_HEADER_SIZE = 170,
};

typedef enum {
    RHK_DATA_IMAGE          = 0,
    RHK_DATA_LINE           = 1,
    RHK_DATA_XY_DATA        = 2,
    RHK_DATA_ANNOTATED_LINE = 3,
    RHK_DATA_TEXT           = 4,
    RHK_DATA_ANNOTATED_TEXT = 5,
    RHK_DATA_SEQUENTIAL     = 6,    /* Only in RHKPageIndex */
} RHKDataType;

typedef enum {
    RHK_OBJECT_UNDEFINED          = 0,
    RHK_OBJECT_PAGE_INDEX_HEADER  = 1,
    RHK_OBJECT_PAGE_INDEX_ARRAY   = 2,
    RHK_OBJECT_PAGE_HEADER        = 3,
    RHK_OBJECT_PAGE_DATA          = 4,
    RHK_OBJECT_IMAGE_DRIFT_HEADER = 5,
    RHK_OBJECT_IMAGE_DRIFT        = 6,
    RHK_OBJECT_SPEC_DRIFT_HEADER  = 7,
    RHK_OBJECT_SPEC_DRIFT_DATA    = 8,
    RHK_OBJECT_COLOR_INFO         = 9,
    RHK_OBJECT_STRING_DATA        = 10,
    RHK_OBJECT_TIP_TRACK_HEADER   = 11,
    RHK_OBJECT_TIP_TRACK_DATA     = 12,
    RHK_OBJECT_PRM                = 13,
    RHK_OBJECT_THUMBNAIL          = 14,
    RHK_OBJECT_PRM_HEADER         = 15,
    RHK_OBJECT_THUMBNAIL_HEADER   = 16,
    /* Our types */
    RHK_OBJECT_FILE_HEADER        = -42,
    RHK_OBJECT_PAGE_INDEX         = -43,
} RHKObjectType;

typedef enum {
    RHK_SOURCE_RAW        = 0,
    RHK_SOURCE_PROCESSED  = 1,
    RHK_SOURCE_CALCULATED = 2,
    RHK_SOURCE_IMPORTED   = 3,
} RHKSourceType;

typedef enum {
    RHK_IMAGE_NORMAL         = 0,
    RHK_IMAGE_AUTOCORRELATED = 1,
} RHKImageType;

typedef enum {
    RHK_PAGE_UNDEFINED                   = 0,
    RHK_PAGE_TOPOGAPHIC                  = 1,
    RHK_PAGE_CURRENT                     = 2,
    RHK_PAGE_AUX                         = 3,
    RHK_PAGE_FORCE                       = 4,
    RHK_PAGE_SIGNAL                      = 5,
    RHK_PAGE_FFT                         = 6,
    RHK_PAGE_NOISE_POWER_SPECTRUM        = 7,
    RHK_PAGE_LINE_TEST                   = 8,
    RHK_PAGE_OSCILLOSCOPE                = 9,
    RHK_PAGE_IV_SPECTRA                  = 10,
    RHK_PAGE_IV_4x4                      = 11,
    RHK_PAGE_IV_8x8                      = 12,
    RHK_PAGE_IV_16x16                    = 13,
    RHK_PAGE_IV_32x32                    = 14,
    RHK_PAGE_IV_CENTER                   = 15,
    RHK_PAGE_INTERACTIVE_SPECTRA         = 16,
    RHK_PAGE_AUTOCORRELATION             = 17,
    RHK_PAGE_IZ_SPECTRA                  = 18,
    RHK_PAGE_4_GAIN_TOPOGRAPHY           = 19,
    RHK_PAGE_8_GAIN_TOPOGRAPHY           = 20,
    RHK_PAGE_4_GAIN_CURRENT              = 21,
    RHK_PAGE_8_GAIN_CURRENT              = 22,
    RHK_PAGE_IV_64x64                    = 23,
    RHK_PAGE_AUTOCORRELATION_SPECTRUM    = 24,
    RHK_PAGE_COUNTER                     = 25,
    RHK_PAGE_MULTICHANNEL_ANALYSER       = 26,
    RHK_PAGE_AFM_100                     = 27,
    RHK_PAGE_CITS                        = 28,
    RHK_PAGE_GPIB                        = 29,
    RHK_PAGE_VIDEO_CHANNEL               = 30,
    RHK_PAGE_IMAGE_OUT_SPECTRA           = 31,
    RHK_PAGE_I_DATALOG                   = 32,
    RHK_PAGE_I_ECSET                     = 33,
    RHK_PAGE_I_ECDATA                    = 34,
    RHK_PAGE_I_DSP_AD                    = 35,
    RHK_PAGE_DISCRETE_SPECTROSCOPY_PP    = 36,
    RHK_PAGE_IMAGE_DISCRETE_SPECTROSCOPY = 37,
    RHK_PAGE_RAMP_SPECTROSCOPY_RP        = 38,
    RHK_PAGE_DISCRETE_SPECTROSCOPY_RP    = 39,
} RHKPageType;

typedef enum {
    RHK_LINE_NOT_A_LINE                     = 0,
    RHK_LINE_HISTOGRAM                      = 1,
    RHK_LINE_CROSS_SECTION                  = 2,
    RHK_LINE_LINE_TEST                      = 3,
    RHK_LINE_OSCILLOSCOPE                   = 4,
    RHK_LINE_NOISE_POWER_SPECTRUM           = 6,
    RHK_LINE_IV_SPECTRUM                    = 7,
    RHK_LINE_IZ_SPECTRUM                    = 8,
    RHK_LINE_IMAGE_X_AVERAGE                = 9,
    RHK_LINE_IMAGE_Y_AVERAGE                = 10,
    RHK_LINE_NOISE_AUTOCORRELATION_SPECTRUM = 11,
    RHK_LINE_MULTICHANNEL_ANALYSER_DATA     = 12,
    RHK_LINE_RENORMALIZED_IV                = 13,
    RHK_LINE_IMAGE_HISTOGRAM_SPECTRA        = 14,
    RHK_LINE_IMAGE_CROSS_SECTION            = 15,
    RHK_LINE_IMAGE_AVERAGE                  = 16,
    RHK_LINE_IMAGE_CROSS_SECTION_G          = 17,
    RHK_LINE_IMAGE_OUT_SPECTRA              = 18,
    RHK_LINE_DATALOG_SPECTRUM               = 19,
    RHK_LINE_GXY                            = 20,
    RHK_LINE_ELECTROCHEMISTRY               = 21,
    RHK_LINE_DISCRETE_SPECTROSCOPY          = 22,
} RHKLineType;

typedef enum {
    RHK_SCAN_RIGHT = 0,
    RHK_SCAN_LEFT  = 1,
    RHK_SCAN_UP    = 2,
    RHK_SCAN_DOWN  = 3
} RHKScanType;

typedef enum {
    RHK_STRING_LABEL,
    RHK_STRING_SYSTEM_TEXT,
    RHK_STRING_SESSION_TEXT,
    RHK_STRING_USER_TEXT,
    RHK_STRING_PATH,
    RHK_STRING_DATE,
    RHK_STRING_TIME,
    RHK_STRING_X_UNITS,
    RHK_STRING_Y_UNITS,
    RHK_STRING_Z_UNITS,
    RHK_STRING_X_LABEL,
    RHK_STRING_Y_LABEL,
    RHK_STRING_STATUS_CHANNEL_TEXT,
    RHK_STRING_COMPLETED_LINE_COUNT,
    RHK_STRING_OVERSAMPLING_COUNT,
    RHK_STRING_SLICED_VOLTAGE,
    RHK_STRING_PLL_PRO_STATUS,
    RHK_STRING_NSTRINGS
} RHKStringType;

typedef struct {
    RHKObjectType type;
    guint offset;
    guint size;
} RHKObject;

typedef struct {
    guint page_count;
    guint object_count;
    guint reserved1;
    guint reserved2;
    RHKObject *objects;
} RHKPageIndexHeader;

typedef struct {
    guint field_size;
    guint string_count;
    RHKPageType page_type;
    guint data_sub_source;
    RHKLineType line_type;
    gint x_coord;
    gint y_coord;
    guint x_size;
    guint y_size;
    RHKImageType image_type;
    RHKScanType scan_dir;
    guint group_id;
    guint data_size;
    guint min_z_value;
    guint max_z_value;
    gdouble x_scale;
    gdouble y_scale;
    gdouble z_scale;
    gdouble xy_scale;
    gdouble x_offset;
    gdouble y_offset;
    gdouble z_offset;
    gdouble period;
    gdouble bias;
    gdouble current;
    gdouble angle;
    guint color_info_count;
    guint grid_x_size;
    guint grid_y_size;
    guint object_count;
    guint reserved[16];
    const guchar *data;
    gchar *strings[RHK_STRING_NSTRINGS];
    RHKObject *objects;
} RHKPage;

typedef struct {
    guchar id[GUID_SIZE];
    RHKDataType data_type;
    RHKSourceType source;
    guint object_count;
    guint minor_version;
    RHKObject *objects;
    RHKPage page;
} RHKPageIndex;

typedef struct {
    guint page_count;
    guint object_count;
    guint object_field_size;
    guint reserved1;
    guint reserved2;
    RHKObject *objects;
    RHKPageIndexHeader page_index_header;
    RHKPageIndex *page_indices;
} RHKFile;

static gboolean      module_register               (void);
static gint          rhk_sm4_detect                (const GwyFileDetectInfo *fileinfo,
                                                    gboolean only_name);
static GwyContainer* rhk_sm4_load                  (const gchar *filename,
                                                    GwyRunType mode,
                                                    GError **error);
static gboolean      rhk_sm4_read_page_index_header(RHKPageIndexHeader *header,
                                                    const RHKObject *obj,
                                                    const guchar *buffer,
                                                    gsize size,
                                                    GError **error);
static gboolean      rhk_sm4_read_page_index       (RHKPageIndex *header,
                                                    const RHKObject *obj,
                                                    const guchar *buffer,
                                                    gsize size,
                                                    GError **error);
static gboolean      rhk_sm4_read_page_header      (RHKPage *page,
                                                    const RHKObject *obj,
                                                    const guchar *buffer,
                                                    gsize size,
                                                    GError **error);
static gboolean      rhk_sm4_read_page_data        (RHKPage *page,
                                                    const RHKObject *obj,
                                                    const guchar *buffer,
                                                    GError **error);
static gboolean      rhk_sm4_read_string_data      (RHKPage *page,
                                                    const RHKObject *obj,
                                                    guint count,
                                                    const guchar *buffer);
static RHKObject*    rhk_sm4_read_objects          (const guchar *buffer,
                                                    const guchar *p,
                                                    gsize size,
                                                    guint count,
                                                    RHKObjectType intype,
                                                    GError **error);
static RHKObject*    rhk_sm4_find_object           (RHKObject *objects,
                                                    guint count,
                                                    RHKObjectType type,
                                                    RHKObjectType parenttype,
                                                    GError **error);
static const gchar*  rhk_sm4_describe_object       (RHKObjectType type);
static void          rhk_sm4_free                  (RHKFile *rhkfile);
static GwyDataField* rhk_sm4_page_to_data_field    (const RHKPage *page);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports RHK Technology SM4 data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

static const GwyEnum scan_directions[] = {
    { "Right", RHK_SCAN_RIGHT, },
    { "Left",  RHK_SCAN_LEFT,  },
    { "Up",    RHK_SCAN_UP,    },
    { "Down",  RHK_SCAN_DOWN,  },
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("rhk-sm4",
                           N_("RHK SM4 files (.sm4)"),
                           (GwyFileDetectFunc)&rhk_sm4_detect,
                           (GwyFileLoadFunc)&rhk_sm4_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
rhk_sm4_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_TOTAL_SIZE
        && memcmp(fileinfo->head + MAGIC_OFFSET, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
rhk_sm4_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    RHKFile rhkfile;
    RHKObject *obj, o;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    const guchar *p;
    GString *key = NULL;
    guint i, imageid = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    gwy_clear(&rhkfile, 1);
    if (size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }

    /* File header */
    p = buffer + MAGIC_OFFSET + MAGIC_TOTAL_SIZE;
    rhkfile.page_count = gwy_get_guint32_le(&p);
    rhkfile.object_count = gwy_get_guint32_le(&p);
    rhkfile.object_field_size = gwy_get_guint32_le(&p);
    gwy_debug("page_count: %u, object_count: %u, object_field_size: %u",
              rhkfile.page_count, rhkfile.object_count,
              rhkfile.object_field_size);
    if (rhkfile.object_field_size != OBJECT_SIZE)
        g_warning("Object field size %u differs from %u",
                  rhkfile.object_field_size, OBJECT_SIZE);
    rhkfile.reserved1 = gwy_get_guint32_le(&p);
    rhkfile.reserved2 = gwy_get_guint32_le(&p);

    /* Header objects */
    if (!(rhkfile.objects = rhk_sm4_read_objects(buffer, p, size,
                                                 rhkfile.object_count,
                                                 RHK_OBJECT_FILE_HEADER,
                                                 error)))
        goto fail;

    /* Find page index header */
    if (!(obj = rhk_sm4_find_object(rhkfile.objects, rhkfile.object_count,
                                    RHK_OBJECT_PAGE_INDEX_HEADER,
                                    RHK_OBJECT_FILE_HEADER, error))
        || !rhk_sm4_read_page_index_header(&rhkfile.page_index_header,
                                           obj, buffer, size, error))
        goto fail;

    /* There, find the page index array.  That's a single object in the object
     * list but it contains a page_count-long sequence of page indices. */
    rhkfile.page_indices = g_new0(RHKPageIndex,
                                  rhkfile.page_index_header.page_count);
    if (!(obj = rhk_sm4_find_object(rhkfile.page_index_header.objects,
                                    rhkfile.page_index_header.object_count,
                                    RHK_OBJECT_PAGE_INDEX_ARRAY,
                                    RHK_OBJECT_PAGE_INDEX_HEADER, error)))
        goto fail;

    o = *obj;
    for (i = 0; i < rhkfile.page_index_header.page_count; i++) {
        if (!rhk_sm4_read_page_index(rhkfile.page_indices + i, &o,
                                     buffer, size, error))
            goto fail;

        /* Carefully move to the next page index */
        o.offset += o.size + OBJECT_SIZE*rhkfile.page_indices[i].object_count;
    }

    container = gwy_container_new();
    key = g_string_new(NULL);

    /* Read pages */
    for (i = 0; i < rhkfile.page_index_header.page_count; i++) {
        RHKPageIndex *pi = rhkfile.page_indices + i;
        RHKPage *page = &pi->page;

        /* Page must contain header */
        if (!(obj = rhk_sm4_find_object(pi->objects, pi->object_count,
                                        RHK_OBJECT_PAGE_HEADER,
                                        RHK_OBJECT_PAGE_INDEX, error))
            || !rhk_sm4_read_page_header(page, obj, buffer, size, error))
            goto fail;

        /* Page must contain data */
        if (!(obj = rhk_sm4_find_object(pi->objects, pi->object_count,
                                        RHK_OBJECT_PAGE_DATA,
                                        RHK_OBJECT_PAGE_INDEX, error))
            || !rhk_sm4_read_page_data(page, obj, buffer, error))
            goto fail;

        /* Page may contain strings */
        if (!(obj = rhk_sm4_find_object(page->objects, page->object_count,
                                        RHK_OBJECT_STRING_DATA,
                                        RHK_OBJECT_PAGE_HEADER, NULL))
            || !rhk_sm4_read_string_data(page, obj, pi->page.string_count,
                                         buffer)) {
            g_warning("Failed to read string data in page %u", i);
        }

        /* Read the data */
        if (pi->data_type == RHK_DATA_IMAGE) {
            GwyDataField *dfield = rhk_sm4_page_to_data_field(page);
            GQuark quark = gwy_app_get_data_key_for_id(imageid);
            const gchar *scandir, *name;
            gchar *title;

            gwy_container_set_object(container, quark, dfield);
            g_object_unref(dfield);

            if ((name = page->strings[RHK_STRING_LABEL])) {
                scandir = gwy_enum_to_string(page->scan_dir, scan_directions,
                                             G_N_ELEMENTS(scan_directions));
                g_string_assign(key, g_quark_to_string(quark));
                g_string_append(key, "/title");
                if (scandir && *scandir)
                    title = g_strdup_printf("%s [%s]", name, scandir);
                else
                    title = g_strdup(name);
                gwy_container_set_string_by_name(container, key->str, title);
            }

            imageid++;
        }
    }

    if (!imageid)
        err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    rhk_sm4_free(&rhkfile);
    if (!imageid) {
        gwy_object_unref(container);
    }
    if (key)
        g_string_free(key, TRUE);

    return container;
}

static inline void
err_OBJECT_TRUNCATED(GError **error, RHKObjectType type)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Object %s is truncated."),
                rhk_sm4_describe_object(type));
}

static gboolean
rhk_sm4_read_page_index_header(RHKPageIndexHeader *header,
                               const RHKObject *obj,
                               const guchar *buffer,
                               gsize size,
                               GError **error)
{
    const guchar *p;

    if (obj->size < PAGE_INDEX_HEADER_SIZE) {
        err_OBJECT_TRUNCATED(error, RHK_OBJECT_PAGE_INDEX_HEADER);
        return FALSE;
    }

    p = buffer + obj->offset;
    header->page_count = gwy_get_guint32_le(&p);
    header->object_count = gwy_get_guint32_le(&p);
    gwy_debug("page_count: %u, object_count: %u",
              header->page_count, header->object_count);
    header->reserved1 = gwy_get_guint32_le(&p);
    header->reserved2 = gwy_get_guint32_le(&p);

    if (!(header->objects = rhk_sm4_read_objects(buffer, p, size,
                                                 header->object_count,
                                                 RHK_OBJECT_PAGE_INDEX_HEADER,
                                                 error)))
        return FALSE;

    return TRUE;
}

static gboolean
rhk_sm4_read_page_index(RHKPageIndex *header,
                        const RHKObject *obj,
                        const guchar *buffer,
                        gsize size,
                        GError **error)
{
    const guchar *p;

    if (obj->size < PAGE_INDEX_ARRAY_SIZE) {
        err_OBJECT_TRUNCATED(error, RHK_OBJECT_PAGE_INDEX_ARRAY);
        return FALSE;
    }

    p = buffer + obj->offset;
    memcpy(header->id, p, sizeof(header->id));
    p += sizeof(header->id);
    header->data_type = gwy_get_guint32_le(&p);
    header->source = gwy_get_guint32_le(&p);
    header->object_count = gwy_get_guint32_le(&p);
    header->minor_version = gwy_get_guint32_le(&p);
    gwy_debug("data_type: %u, source: %u, object_count: %u, minorv: %u",
              header->data_type, header->source,
              header->object_count, header->minor_version);

    if (!(header->objects = rhk_sm4_read_objects(buffer, p, size,
                                                 header->object_count,
                                                 RHK_OBJECT_PAGE_INDEX_ARRAY,
                                                 error)))
        return FALSE;

    return TRUE;
}

static gboolean
rhk_sm4_read_page_header(RHKPage *page,
                         const RHKObject *obj,
                         const guchar *buffer,
                         gsize size,
                         GError **error)
{
    const guchar *p;
    guint i;

    if (obj->size < PAGE_HEADER_SIZE) {
        err_OBJECT_TRUNCATED(error, RHK_OBJECT_PAGE_HEADER);
        return FALSE;
    }

    p = buffer + obj->offset;
    page->field_size = gwy_get_guint16_le(&p);
    if (obj->size < page->field_size) {
        err_OBJECT_TRUNCATED(error, RHK_OBJECT_PAGE_HEADER);
        return FALSE;
    }

    page->string_count = gwy_get_guint16_le(&p);
    gwy_debug("string_count = %u", page->string_count);
    page->page_type = gwy_get_guint32_le(&p);
    gwy_debug("page_type = %u", page->page_type);
    page->data_sub_source = gwy_get_guint32_le(&p);
    page->line_type = gwy_get_guint32_le(&p);
    page->x_coord = gwy_get_gint32_le(&p);
    page->y_coord = gwy_get_gint32_le(&p);
    page->x_size = gwy_get_guint32_le(&p);
    page->y_size = gwy_get_guint32_le(&p);
    gwy_debug("x_size = %u, y_size = %u", page->x_size, page->y_size);
    if (err_DIMENSION(error, page->x_size)
        || err_DIMENSION(error, page->y_size))
        return FALSE;

    page->image_type = gwy_get_guint32_le(&p);
    gwy_debug("image_type = %u", page->image_type);
    page->scan_dir = gwy_get_guint32_le(&p);
    gwy_debug("scan_dir = %u", page->scan_dir);
    page->group_id = gwy_get_guint32_le(&p);
    gwy_debug("group_id = 0x%08x", page->group_id);
    page->data_size = gwy_get_guint32_le(&p);
    gwy_debug("data_size = %u", page->data_size);
    page->min_z_value = gwy_get_gint32_le(&p);
    page->max_z_value = gwy_get_gint32_le(&p);
    gwy_debug("min,max_z_value = %d %d", page->min_z_value, page->max_z_value);
    page->x_scale = gwy_get_gfloat_le(&p);
    page->y_scale = gwy_get_gfloat_le(&p);
    page->z_scale = gwy_get_gfloat_le(&p);
    gwy_debug("x,y,z_scale = %g %g %g",
              page->x_scale, page->y_scale, page->z_scale);
    /* Use negated positive conditions to catch NaNs */
    if (!((page->x_scale = fabs(page->x_scale)) > 0)) {
        g_warning("Real x scale is 0.0, fixing to 1.0");
        page->x_scale = 1.0;
    }
    if (!((page->y_scale = fabs(page->y_scale)) > 0)) {
        g_warning("Real y scale is 0.0, fixing to 1.0");
        page->y_scale = 1.0;
    }
    page->xy_scale = gwy_get_gfloat_le(&p);
    page->x_offset = gwy_get_gfloat_le(&p);
    page->y_offset = gwy_get_gfloat_le(&p);
    page->z_offset = gwy_get_gfloat_le(&p);
    gwy_debug("x,y,z_offset = %g %g %g",
              page->x_offset, page->y_offset, page->z_offset);
    page->period = gwy_get_gfloat_le(&p);
    page->bias = gwy_get_gfloat_le(&p);
    page->current = gwy_get_gfloat_le(&p);
    page->angle = gwy_get_gfloat_le(&p);
    gwy_debug("period = %g, bias = %g, current = %g, angle = %g",
              page->period, page->bias, page->current, page->angle);
    page->color_info_count = gwy_get_guint32_le(&p);
    gwy_debug("color_info_count = %u", page->color_info_count);
    page->grid_x_size = gwy_get_guint32_le(&p);
    page->grid_y_size = gwy_get_guint32_le(&p);
    gwy_debug("gird_x,y = %u %u", page->grid_x_size, page->grid_y_size);
    page->object_count = gwy_get_guint32_le(&p);
    for (i = 0; i < G_N_ELEMENTS(page->reserved); i++)
        page->reserved[i] = gwy_get_guint32_le(&p);

    if (!(page->objects = rhk_sm4_read_objects(buffer, p, size,
                                               page->object_count,
                                               RHK_OBJECT_PAGE_HEADER,
                                               error)))
        return FALSE;

    return TRUE;
}

static gboolean
rhk_sm4_read_page_data(RHKPage *page,
                       const RHKObject *obj,
                       const guchar *buffer,
                       GError **error)
{
    gsize expected_size;

    expected_size = 4 * page->x_size * page->y_size;
    if (err_SIZE_MISMATCH(error, expected_size, obj->size, TRUE))
        return FALSE;

    page->data = buffer + obj->offset;

    return TRUE;
}

static gboolean
rhk_sm4_read_string_data(RHKPage *page,
                         const RHKObject *obj,
                         guint count,
                         const guchar *buffer)
{
    const guchar *p;
    gchar *s;
    guint i, len;

    gwy_debug("count: %u, known strings: %u", count, RHK_STRING_NSTRINGS);
    count = MIN(count, RHK_STRING_NSTRINGS);
    p = buffer + obj->offset;
    for (i = 0; i < count; i++) {
        /* Not enough strings */
        if (p - buffer + 2 > obj->offset + obj->size)
            return FALSE;

        len = gwy_get_guint16_le(&p);
        if (!len)
            continue;

        /* String does not fit */
        if (p - buffer + 2*len > obj->offset + obj->size)
            return FALSE;

        s = page->strings[i] = g_new0(gchar, 6*len + 1);
        while (len--)
            s += g_unichar_to_utf8(gwy_get_guint16_le(&p), s);

        if (s != page->strings[i]) {
            gwy_debug("string[%u]: <%s>", i, page->strings[i]);
        }
    }

    return TRUE;
}

/* FIXME: Some of the objects read are of type 0 and size 0, but maybe
 * that's right and they allow empty object slots */
static RHKObject*
rhk_sm4_read_objects(const guchar *buffer,
                     const guchar *p, gsize size, guint count,
                     RHKObjectType intype, GError **error)
{
    RHKObject *objects, *obj;
    guint i;

    if ((p - buffer) + count*OBJECT_SIZE >= size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Object list in %s is truncated."),
                    rhk_sm4_describe_object(intype));
        return NULL;
    }

    objects = g_new(RHKObject, count);
    for (i = 0; i < count; i++) {
        obj = objects + i;
        obj->type = gwy_get_guint32_le(&p);
        obj->offset = gwy_get_guint32_le(&p);
        obj->size = gwy_get_guint32_le(&p);
        gwy_debug("object of type %u (%s) at %u, size %u",
                  obj->type, rhk_sm4_describe_object(obj->type),
                  obj->offset, obj->size);
        if ((gsize)obj->size + obj->offset > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Object of type %s is truncated."),
                        rhk_sm4_describe_object(obj->type));
            g_free(objects);
            return NULL;
        }
    }

    return objects;
}

static RHKObject*
rhk_sm4_find_object(RHKObject *objects, guint count,
                    RHKObjectType type, RHKObjectType parenttype,
                    GError **error)
{
    guint i;

    for (i = 0; i < count; i++) {
        RHKObject *obj = objects + i;

        if (obj->type == type)
            return obj;
    }

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Cannot find object %s in %s."),
                rhk_sm4_describe_object(type),
                rhk_sm4_describe_object(parenttype));
    return NULL;
}

static const gchar*
rhk_sm4_describe_object(RHKObjectType type)
{
    static const GwyEnum types[] = {
        { "Undefined",        RHK_OBJECT_UNDEFINED,          },
        { "PageIndexHeader",  RHK_OBJECT_PAGE_INDEX_HEADER,  },
        { "PageIndexArray",   RHK_OBJECT_PAGE_INDEX_ARRAY,   },
        { "PageHeader",       RHK_OBJECT_PAGE_HEADER,        },
        { "PageData",         RHK_OBJECT_PAGE_DATA,          },
        { "ImageDriftHeader", RHK_OBJECT_IMAGE_DRIFT_HEADER, },
        { "ImageDrift",       RHK_OBJECT_IMAGE_DRIFT,        },
        { "SpecDriftHeader",  RHK_OBJECT_SPEC_DRIFT_HEADER,  },
        { "SpecDriftData",    RHK_OBJECT_SPEC_DRIFT_DATA,    },
        { "ColorInfo",        RHK_OBJECT_COLOR_INFO,         },
        { "StringData",       RHK_OBJECT_STRING_DATA,        },
        { "TipTrackHeader",   RHK_OBJECT_TIP_TRACK_HEADER,   },
        { "TipTrackData",     RHK_OBJECT_TIP_TRACK_DATA,     },
        { "PRM",              RHK_OBJECT_PRM,                },
        { "Thumbnail",        RHK_OBJECT_THUMBNAIL,          },
        { "PRMHeader",        RHK_OBJECT_PRM_HEADER,         },
        { "ThumbnailHeader",  RHK_OBJECT_THUMBNAIL_HEADER,   },
        /* Our types */
        { "FileHeader",       RHK_OBJECT_FILE_HEADER,        },
        { "PageIndex",        RHK_OBJECT_PAGE_INDEX,         },
    };

    const gchar *retval;

    retval = gwy_enum_to_string(type, types, G_N_ELEMENTS(types));
    if (!retval || !*retval)
        return "Unknown";

    return retval;
};

static void
rhk_sm4_free(RHKFile *rhkfile)
{
    RHKPage *page;
    guint i, j;

    g_free(rhkfile->objects);
    g_free(rhkfile->page_index_header.objects);
    if (rhkfile->page_indices) {
        for (i = 0; i < rhkfile->page_index_header.page_count; i++) {
            g_free(rhkfile->page_indices[i].objects);
            page = &rhkfile->page_indices[i].page;
            for (j = 0; j < RHK_STRING_NSTRINGS; j++)
                g_free(page->strings[j]);
        }
        g_free(rhkfile->page_indices);
    }
}

static GwyDataField*
rhk_sm4_page_to_data_field(const RHKPage *page)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    const gchar *unit;
    const gint32 *pdata;
    gint xres, yres, i, j;
    gdouble *data;

    xres = page->x_size;
    yres = page->y_size;
    dfield = gwy_data_field_new(xres, yres,
                                xres*page->x_scale, yres*page->y_scale,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint32*)page->data;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            data[i*xres + xres-1 - j] = GINT32_FROM_LE(pdata[i*xres + j])
                                        *page->z_scale + page->z_offset;
        }
    }

    /* XY units */
    if (page->strings[RHK_STRING_X_UNITS]
        && page->strings[RHK_STRING_Y_UNITS]) {
        if (!gwy_strequal(page->strings[RHK_STRING_X_UNITS],
                          page->strings[RHK_STRING_Y_UNITS]))
            g_warning("X and Y units differ, using X");
        unit = page->strings[RHK_STRING_X_UNITS];
    }
    else if (page->strings[RHK_STRING_X_UNITS])
        unit = page->strings[RHK_STRING_X_UNITS];
    else if (page->strings[RHK_STRING_Y_UNITS])
        unit = page->strings[RHK_STRING_Y_UNITS];
    else
        unit = NULL;

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string(siunit, unit);

    /* Z units */
    if (page->strings[RHK_STRING_Z_UNITS])
        unit = page->strings[RHK_STRING_Z_UNITS];
    else
        unit = NULL;
    /* Fix some silly units */
    if (unit && gwy_strequal(unit, "N/sec"))
        unit = "s^-1";

    siunit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_set_from_string(siunit, unit);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
