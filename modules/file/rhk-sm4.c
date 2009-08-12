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
    PAGE_INDEX_HEADER_SIZE = 4*4,
    PAGE_INDEX_ARRAY_SIZE = 16 + 4*4,
};

typedef enum {
    RHK_DATA_IMAGE          = 0,
    RHK_DATA_LINE           = 1,
    RHK_DATA_XY_DATA        = 2,
    RHK_DATA_ANNOTATED_LINE = 3,
    RHK_DATA_TEXT           = 4,
    RHK_DATA_ANNOTATED_TEXT = 5,
    RHK_DATA_SEQUENTIAL     = 6,
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
} RHKObjectType;

typedef enum {
    RHK_SOURCE_RAW        = 0,
    RHK_SOURCE_PROCESSED  = 1,
    RHK_SOURCE_CALCULATED = 2,
    RHK_SOURCE_IMPORTED   = 3,
} RHKSourceType;

typedef enum {
    RHK_PAGE_UNDEFINED                = 0,
    RHK_PAGE_TOPOGAPHIC               = 1,
    RHK_PAGE_CURRENT                  = 2,
    RHK_PAGE_AUX                      = 3,
    RHK_PAGE_FORCE                    = 4,
    RHK_PAGE_SIGNAL                   = 5,
    RHK_PAGE_FFT                      = 6,
    RHK_PAGE_NOISE_POWER_SPECTRUM     = 7,
    RHK_PAGE_LINE_TEST                = 8,
    RHK_PAGE_OSCILLOSCOPE             = 9,
    RHK_PAGE_IV_SPECTRA               = 10,
    RHK_PAGE_IV_4x4                   = 11,
    RHK_PAGE_IV_8x8                   = 12,
    RHK_PAGE_IV_16x16                 = 13,
    RHK_PAGE_IV_32x32                 = 14,
    RHK_PAGE_IV_CENTER                = 15,
    RHK_PAGE_INTERACTIVE_SPECTRA      = 16,
    RHK_PAGE_AUTOCORRELATION          = 17,
    RHK_PAGE_IZ_SPECTRA               = 18,
    RHK_PAGE_4_GAIN_TOPOGRAPHY        = 19,
    RHK_PAGE_8_GAIN_TOPOGRAPHY        = 20,
    RHK_PAGE_4_GAIN_CURRENT           = 21,
    RHK_PAGE_8_GAIN_CURRENT           = 22,
    RHK_PAGE_IV_64x64                 = 23,
    RHK_PAGE_AUTOCORRELATION_SPECTRUM = 24,
    RHK_PAGE_COUNTER                  = 25,
    RHK_PAGE_MULTICHANNEL_ANALYSER    = 26,
    RHK_PAGE_AFM_100                  = 27
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
    RHK_LINE_IMAGE_AVERAGE                  = 16
} RHKLineType;

typedef enum {
    RHK_SCAN_RIGHT = 0,
    RHK_SCAN_LEFT  = 1,
    RHK_SCAN_UP    = 2,
    RHK_SCAN_DOWN  = 3
} RHKScanType;

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
    guchar id[16];
    RHKDataType data_type;
    RHKSourceType source;
    guint object_count;
    guint minor_version;
    RHKObject *objects;
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
static RHKObject*    rhk_sm4_read_objects          (const guchar *buffer,
                                                    const guchar *p,
                                                    gsize size,
                                                    guint count,
                                                    GError **error);
static RHKObject*    rhk_sm4_find_object           (RHKObject *objects,
                                                    guint count,
                                                    RHKObjectType type,
                                                    const gchar *name,
                                                    const gchar *parentname,
                                                    GError **error);
static void          rhk_sm4_free                  (RHKFile *rhkfile);

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
    guint i;

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
                                                 rhkfile.object_count, error)))
        goto fail;

    /* Find page index header */
    if (!(obj = rhk_sm4_find_object(rhkfile.objects, rhkfile.object_count,
                                    RHK_OBJECT_PAGE_INDEX_HEADER,
                                    "PageIndexHeader", "FileHeader", error))
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
                                    "PageIndexArray", "PageIndexHeader",
                                    error)))
        goto fail;

    o = *obj;
    for (i = 0; i < rhkfile.page_index_header.page_count; i++) {
        if (!rhk_sm4_read_page_index(rhkfile.page_indices + i, &o,
                                     buffer, size, error))
            goto fail;

        /* Carefully move to the next page index */
        o.offset += o.size + OBJECT_SIZE*rhkfile.page_indices[i].object_count;
    }

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    rhk_sm4_free(&rhkfile);

    return container;
}

static inline void
err_OBJECT_TRUNCATED(GError **error, const gchar *name)
{
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Object %s is truncated."), name);
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
        err_OBJECT_TRUNCATED(error, "PageIndexHeader");
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
                                                 header->object_count, error)))
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
        err_OBJECT_TRUNCATED(error, "PageIndex");
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
                                                 header->object_count, error)))
        return FALSE;

    return TRUE;
}

static RHKObject*
rhk_sm4_read_objects(const guchar *buffer,
                     const guchar *p, gsize size, guint count,
                     GError **error)
{
    RHKObject *objects, *obj;
    guint i;

    if ((p - buffer) + count*OBJECT_SIZE >= size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Object list is truncated."));
        return NULL;
    }

    objects = g_new(RHKObject, count);
    for (i = 0; i < count; i++) {
        obj = objects + i;
        obj->type = gwy_get_guint32_le(&p);
        obj->offset = gwy_get_guint32_le(&p);
        obj->size = gwy_get_guint32_le(&p);
        gwy_debug("object of type %u at %u, size %u",
                  obj->type, obj->offset, obj->size);
        if ((gsize)obj->size + obj->offset > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Object of type %u is truncated."),
                        obj->type);
            g_free(objects);
            return NULL;
        }
    }

    return objects;
}

static RHKObject*
rhk_sm4_find_object(RHKObject *objects, guint count, RHKObjectType type,
                    const gchar *name, const gchar *parentname, GError **error)
{
    guint i;

    for (i = 0; i < count; i++) {
        RHKObject *obj = objects + i;

        if (obj->type == type)
            return obj;
    }

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Cannot find object %s in %s."),
                name, parentname);
    return NULL;
}

static void
rhk_sm4_free(RHKFile *rhkfile)
{
    guint i;

    g_free(rhkfile->objects);
    g_free(rhkfile->page_index_header.objects);
    if (rhkfile->page_indices) {
        for (i = 0; i < rhkfile->page_index_header.page_count; i++)
            g_free(rhkfile->page_indices[i].objects);
        g_free(rhkfile->page_indices);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

