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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#define DEBUG 1
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

/**
 * [FILE-MAGIC-USERGUIDE]
 * RHK Instruments SM4
 * .sm4
 * Read
 **/

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
    RHK_OBJECT_API_INFO           = 17,
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

typedef enum {
    RHK_DRIFT_DISABLED = 0,
    RHK_DRIFT_EACH_SPECTRA = 1,
    RHK_DRIFT_EACH_LOCATION = 2
} RHKDriftOptionType;

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
    guint64 start_time;
    RHKDriftOptionType drift_opt;
    guint nstrings;
    /* and then @nstrings rhk_strings follow but we do not know how to read
     * that. */
} RHKSpecDriftHeader;

typedef struct {
    gdouble ftime;
    gdouble xcoord;
    gdouble ycoord;
    gdouble dx;
    gdouble dy;
    gdouble cumulative_dx;
    gdouble cumulative_dy;
} RHKSpecInfo;

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
static gboolean      rhk_sm4_read_drift_header     (RHKSpecDriftHeader *drift_header,
                                                    const RHKObject *obj,
                                                    const guchar *buffer);
static gboolean      rhk_sm4_read_spec_info        (RHKSpecInfo *spec_info,
                                                    const RHKObject *obj,
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
static GwyGraphModel* rhk_sm4_page_to_graph_model  (const RHKPage *page);
static GwyContainer* rhk_sm4_get_metadata          (const RHKPageIndex *pi,
                                                    const RHKPage *page);

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
    guint i, imageid = 0, graphid = 0;

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

            meta = rhk_sm4_get_metadata(pi, page);
            g_string_printf(key, "/%u/meta", imageid);
            gwy_container_set_object_by_name(container, key->str, meta);
            g_object_unref(meta);

            imageid++;
        }
        else if (pi->data_type == RHK_DATA_LINE) {
            GwyGraphModel *gmodel;
            RHKSpecDriftHeader drift_header;
            RHKSpecInfo spec_info;
            gboolean have_header = FALSE, have_info = FALSE;

            gwy_debug("page_type %u", page->page_type);
            gwy_debug("line_type %u", page->line_type);
            gwy_debug("page_sizes %u %u", page->x_size, page->y_size);
            /* Page may contain drift header */
            if ((obj = rhk_sm4_find_object(page->objects, page->object_count,
                                           RHK_OBJECT_SPEC_DRIFT_HEADER,
                                           RHK_OBJECT_PAGE_HEADER, NULL))
                && rhk_sm4_read_drift_header(&drift_header, obj, buffer)) {
                gwy_debug("drift_header OK");
                have_header = TRUE;
            }
            if ((obj = rhk_sm4_find_object(page->objects, page->object_count,
                                           RHK_OBJECT_SPEC_DRIFT_DATA,
                                           RHK_OBJECT_PAGE_HEADER, NULL))
                && rhk_sm4_read_spec_info(&spec_info, obj, buffer)) {
                gwy_debug("spec_info OK");
                have_info = TRUE;
            }
            /* FIXME: RHK_STRING_PLL_PRO_STATUS may contain interesting
             * metadata.  But we have not place where to put it. */

            if ((gmodel = rhk_sm4_page_to_graph_model(page))) {
                graphid++;
                gwy_container_set_object(container,
                                         gwy_app_get_graph_key_for_id(graphid),
                                         gmodel);
                g_object_unref(gmodel);
            }
        }
    }

    if (!imageid && !graphid)
        err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    rhk_sm4_free(&rhkfile);
    if (!imageid && !graphid) {
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

static gboolean
rhk_sm4_read_drift_header(RHKSpecDriftHeader *drift_header,
                          const RHKObject *obj,
                          const guchar *buffer)
{
    if (obj->size < 16)
        return FALSE;

    drift_header->start_time = gwy_get_guint64_le(&buffer);
    drift_header->drift_opt = gwy_get_gint16_le(&buffer);
    drift_header->nstrings = gwy_get_guint16_le(&buffer);
    /* TODO: Read the strings. */
    return TRUE;
}

static gboolean
rhk_sm4_read_spec_info(RHKSpecInfo *spec_info,
                       const RHKObject *obj,
                       const guchar *buffer)
{
    if (obj->size < 28)
        return FALSE;

    spec_info->ftime = gwy_get_gfloat_le(&buffer);
    spec_info->xcoord = gwy_get_gfloat_le(&buffer);
    spec_info->ycoord = gwy_get_gfloat_le(&buffer);
    spec_info->dx = gwy_get_gfloat_le(&buffer);
    spec_info->dy = gwy_get_gfloat_le(&buffer);
    spec_info->cumulative_dx = gwy_get_gfloat_le(&buffer);
    spec_info->cumulative_dy = gwy_get_gfloat_le(&buffer);
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
        { "APIInfo",          RHK_OBJECT_API_INFO,           },
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

static GwyGraphModel*
rhk_sm4_page_to_graph_model(const RHKPage *page)
{
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *siunit;
    const gint32 *pdata;
    const gchar *name;
    gint res, ncurves, i, j;
    gdouble *xdata, *ydata;

    res = page->x_size;
    ncurves = page->y_size;

    gmodel = gwy_graph_model_new();
    pdata = (const gint32*)page->data;
    xdata = g_new(gdouble, res);
    ydata = g_new(gdouble, res);
    name = page->strings[RHK_STRING_LABEL];
    for (i = 0; i < ncurves; i++) {
        gcmodel = gwy_graph_curve_model_new();
        for (j = 0; j < res; j++) {
            xdata[j] = j*page->x_scale + page->x_offset;
            ydata[j] = (GINT32_FROM_LE(pdata[i*res + j])*page->z_scale
                        + page->z_offset);
        }
        gwy_graph_curve_model_set_data(gcmodel, xdata, ydata, res);
        g_object_set(gcmodel,
                     "mode", GWY_GRAPH_CURVE_LINE,
                     "color", gwy_graph_get_preset_color(i),
                     NULL);
        if (name)
            g_object_set(gcmodel, "description", name, NULL);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
    }
    g_free(ydata);
    g_free(xdata);

    /* Units */
    siunit = gwy_si_unit_new(page->strings[RHK_STRING_X_UNITS]);
    g_object_set(gmodel, "si-unit-x", siunit, NULL);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new(page->strings[RHK_STRING_Z_UNITS]);
    g_object_set(gmodel, "si-unit-y", siunit, NULL);
    g_object_unref(siunit);

    if (name)
        g_object_set(gmodel, "title", name, NULL);

    return gmodel;
}

static void
rhk_sm4_meta_string(const RHKPage *page,
                    RHKStringType stringid,
                    const gchar *name,
                    GwyContainer *meta)
{
    const gchar *s;

    g_return_if_fail(stringid < RHK_STRING_NSTRINGS);
    if ((s = page->strings[stringid]))
        gwy_container_set_string_by_name(meta, name, g_strdup(s));
}

static GwyContainer*
rhk_sm4_get_metadata(const RHKPageIndex *pi,
                     const RHKPage *page)
{
    static const gchar hex[] = "0123456789abcdef";

    GwyContainer *meta;
    const gchar *s;
    gchar *str;
    guint i, w;

    meta = gwy_container_new();

    s = gwy_enuml_to_string(page->page_type,
                            "Topographic", RHK_PAGE_TOPOGAPHIC,
                            "Current", RHK_PAGE_CURRENT,
                            "Aux", RHK_PAGE_AUX,
                            "Force", RHK_PAGE_FORCE,
                            "Signal", RHK_PAGE_SIGNAL,
                            "FFT transform", RHK_PAGE_FFT,
                            "Noise power spectrum",
                            RHK_PAGE_NOISE_POWER_SPECTRUM,
                            "Line test", RHK_PAGE_LINE_TEST,
                            "Oscilloscope", RHK_PAGE_OSCILLOSCOPE,
                            "IV spectra", RHK_PAGE_IV_SPECTRA,
                            "Image IV 4x4", RHK_PAGE_IV_4x4,
                            "Image IV 8x8", RHK_PAGE_IV_8x8,
                            "Image IV 16x16", RHK_PAGE_IV_16x16,
                            "Image IV 32x32", RHK_PAGE_IV_32x32,
                            "Image IV Center", RHK_PAGE_IV_CENTER,
                            "Interactive spectra", RHK_PAGE_INTERACTIVE_SPECTRA,
                            "Autocorrelation", RHK_PAGE_AUTOCORRELATION,
                            "IZ spectra", RHK_PAGE_IZ_SPECTRA,
                            "4 gain topography", RHK_PAGE_4_GAIN_TOPOGRAPHY,
                            "8 gain topography", RHK_PAGE_8_GAIN_TOPOGRAPHY,
                            "4 gain current", RHK_PAGE_4_GAIN_CURRENT,
                            "8 gain current", RHK_PAGE_8_GAIN_CURRENT,
                            "Image IV 64x64", RHK_PAGE_IV_64x64,
                            "Autocorrelation spectrum",
                            RHK_PAGE_AUTOCORRELATION_SPECTRUM,
                            "Counter data", RHK_PAGE_COUNTER,
                            "Multichannel analyser",
                            RHK_PAGE_MULTICHANNEL_ANALYSER,
                            "AFM using AFM-100", RHK_PAGE_AFM_100,
                            "CITS", RHK_PAGE_CITS,
                            "GBIB", RHK_PAGE_GPIB,
                            "Video channel", RHK_PAGE_VIDEO_CHANNEL,
                            "Image OUT spectra", RHK_PAGE_IMAGE_OUT_SPECTRA,
                            "I_Datalog", RHK_PAGE_I_DATALOG,
                            "I_Ecset", RHK_PAGE_I_ECSET,
                            "I_Ecdata", RHK_PAGE_I_ECDATA,
                            "DSP channel", RHK_PAGE_I_DSP_AD,
                            "Discrete spectroscopy (present pos)",
                            RHK_PAGE_DISCRETE_SPECTROSCOPY_PP,
                            "Image discrete spectroscopy",
                            RHK_PAGE_IMAGE_DISCRETE_SPECTROSCOPY,
                            "Ramp spectroscopy (relative points)",
                            RHK_PAGE_RAMP_SPECTROSCOPY_RP,
                            "Discrete spectroscopy (relative points)",
                            RHK_PAGE_DISCRETE_SPECTROSCOPY_RP,
                            NULL);
    if (s && *s)
        gwy_container_set_string_by_name(meta, "Type", g_strdup(s));

    s = gwy_enum_to_string(page->scan_dir,
                           scan_directions, G_N_ELEMENTS(scan_directions));
    if (s && *s)
        gwy_container_set_string_by_name(meta, "Scan Direction", g_strdup(s));

    s = gwy_enuml_to_string(pi->source,
                            "Raw", RHK_SOURCE_RAW,
                            "Processed", RHK_SOURCE_PROCESSED,
                            "Calculated", RHK_SOURCE_CALCULATED,
                            "Imported", RHK_SOURCE_IMPORTED,
                            NULL);
    if (s && *s)
        gwy_container_set_string_by_name(meta, "Source", g_strdup(s));

    gwy_container_set_string_by_name(meta, "Bias",
                                     g_strdup_printf("%g V", page->bias));
    gwy_container_set_string_by_name(meta, "Rotation angle",
                                     g_strdup_printf("%f", page->angle));
    gwy_container_set_string_by_name(meta, "Period",
                                     g_strdup_printf("%f s", page->period));

    s = page->strings[RHK_STRING_DATE];
    if (s && *s) {
        str = g_strconcat(s, " ", page->strings[RHK_STRING_TIME], NULL);
        gwy_container_set_string_by_name(meta, "Date", str);
    }

    rhk_sm4_meta_string(page, RHK_STRING_LABEL, "Label", meta);
    rhk_sm4_meta_string(page, RHK_STRING_PATH, "Path", meta);
    rhk_sm4_meta_string(page, RHK_STRING_SYSTEM_TEXT, "System comment", meta);
    rhk_sm4_meta_string(page, RHK_STRING_SESSION_TEXT, "Session comment", meta);
    rhk_sm4_meta_string(page, RHK_STRING_USER_TEXT, "User comment", meta);

    str = g_new(gchar, 33);
    for (i = 0; i < 16; i++) {
        str[2*i] = hex[pi->id[i]/16];
        str[2*i + 1] = hex[pi->id[i] % 16];
    }
    str[32] = '\0';
    gwy_container_set_string_by_name(meta, "Page ID", str);

    str = g_new(gchar, 9);
    w = page->group_id;
    for (i = 0; i < 8; i++) {
        str[7 - i] = hex[w & 0xf];
        w = w >> 4;
    }
    str[8] = '\0';
    gwy_container_set_string_by_name(meta, "Group ID", str);

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
