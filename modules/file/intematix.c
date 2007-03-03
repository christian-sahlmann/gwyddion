/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti), Petr Klapetek.
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
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

/* The value of ISDF_TIFFTAG_FILEID */
#define ISDF_MAGIC_NUMBER 0x00534446

/* TIFF data types */
typedef enum {
    GWY_TIFF_NOTYPE    = 0,
    GWY_TIFF_BYTE      = 1,
    GWY_TIFF_ASCII     = 2,
    GWY_TIFF_SHORT     = 3,
    GWY_TIFF_LONG      = 4,
    GWY_TIFF_RATIONAL  = 5,
    GWY_TIFF_SBYTE     = 6,
    GWY_TIFF_UNDEFINED = 7,
    GWY_TIFF_SSHORT    = 8,
    GWY_TIFF_SLONG     = 9,
    GWY_TIFF_SRATIONAL = 10,
    GWY_TIFF_FLOAT     = 11,
    GWY_TIFF_DOUBLE    = 12,
    GWY_TIFF_IFD       = 13
} GwyTIFFDataType;

/* Standard TIFF tags */
enum {
    GWY_TIFFTAG_IMAGEWIDTH       = 256,
    GWY_TIFFTAG_IMAGELENGTH      = 257,
    GWY_TIFFTAG_IMAGEDESCRIPTION = 270,
    GWY_TIFFTAG_SOFTWARE         = 305,
    GWY_TIFFTAG_DATETIME         = 306,
} GwyTIFFTag;

/* Custom TIFF tags */
enum {
    ISDF_TIFFTAG_FILEID = 65000,
    ISDF_TIFFTAG_FILETYPE,
    ISDF_TIFFTAG_DATATYPE,
    ISDF_TIFFTAG_FILEINFO,
    ISDF_TIFFTAG_USERINFO,
    ISDF_TIFFTAG_DATARANGE,
    ISDF_TIFFTAG_SPMDATA,
    ISDF_TIFFTAG_DATACNVT,
    ISDF_TIFFTAG_DATAUNIT,
    ISDF_TIFFTAG_IMGXDIM,
    ISDF_TIFFTAG_IMGYDIM,
    ISDF_TIFFTAG_IMGZDIM,
    ISDF_TIFFTAG_XDIMUNIT,
    ISDF_TIFFTAG_YDIMUNIT,
    ISDF_TIFFTAG_ZDIMUNIT,
    ISDF_TIFFTAG_SPMDATAPOS,
    ISDF_TIFFTAG_IMAGEDEPTH,
    ISDF_TIFFTAG_SAMPLEINFO,
    ISDF_TIFFTAG_SCANRATE,
    ISDF_TIFFTAG_BIASVOLTS,
    ISDF_TIFFTAG_ZSERVO,
    ISDF_TIFFTAG_ZSERVOREF,
    ISDF_TIFFTAG_ZSERVOBW,
    ISDF_TIFFTAG_ZSERVOSP,
    ISDF_TIFFTAG_ZSERVOPID
};

typedef enum {
    ISDF_FILE_STM = 1,
    ISDF_FILE_AFM = 2,
    ISDF_FILE_EMP = 3
} ISDFFileType;

typedef enum {
    ISDF_DATA_STM_TOPO        = 0x0001,
    ISDF_DATA_STM_TOPOBK      = 0x0002,
    ISDF_DATA_STM_TUNNEL      = 0x0003,
    ISDF_DATA_STM_TUNNELBK    = 0x0004,
    ISDF_DATA_STM_ADAUX       = 0x0005,
    ISDF_DATA_STM_ADAUXBK     = 0x0006,
    ISDF_DATA_STM_LINEIV      = 0x0010,
    ISDF_DATA_STM_LINEDIDV    = 0x0011,
    ISDF_DATA_STM_CITS        = 0x0012,
    ISDF_DATA_STM_IMGDIDV     = 0x0013,
    ISDF_DATA_STM_LNAPPRTNNL  = 0x0020,
    ISDF_DATA_STM_LNAPPRAUX   = 0x0021,
    ISDF_DATA_STM_IMGAPPRTNNL = 0x0022,
    ISDF_DATA_STM_IMGAPPRAUX  = 0x0023,
    ISDF_DATA_AFM_TOPO        = 0x0101,
    ISDF_DATA_AFM_TOPOBK      = 0x0102,
    ISDF_DATA_AFM_ERROR       = 0x0103,
    ISDF_DATA_AFM_ERRORBK     = 0x0104,
    ISDF_DATA_AFM_ADAUX       = 0x0105,
    ISDF_DATA_AFM_ADAUXBK     = 0x0106,
    ISDF_DATA_EMP_TOPO        = 0x0201,
    ISDF_DATA_EMP_TOPOBK      = 0x0202,
    ISDF_DATA_EMP_FREQ        = 0x0203,
    ISDF_DATA_EMP_FREQBK      = 0x0204,
    ISDF_DATA_EMP_QFCT        = 0x0205,
    ISDF_DATA_EMP_QFCTBK      = 0x0206,
    ISDF_DATA_EMP_ADAUX       = 0x0207,
    ISDF_DATA_EMP_ADAUXBK     = 0x0208,
    ISDF_DATA_EMP_LNMIXI      = 0x0210,
    ISDF_DATA_EMP_LNMIXQ      = 0x0211,
    ISDF_DATA_EMP_IMGMIXI     = 0x0212,
    ISDF_DATA_EMP_IMGMIXQ     = 0x0213,
    ISDF_DATA_EMP_LNAPPRFREQ  = 0x0220,
    ISDF_DATA_EMP_LNAPPRQF    = 0x0221,
    ISDF_DATA_EMP_LNAPPRAUX   = 0x0222,
    ISDF_DATA_EMP_IMGAPPRFREQ = 0x0223,
    ISDF_DATA_EMP_IMGAPPRQF   = 0x0224,
    ISDF_DATA_EMP_IMGAPPRAUX  = 0x0225
} ISDFDataType;

typedef struct {
    ISDFFileType file_type;
    ISDFDataType data_type;
    guint xres;
    guint yres;
    guint zres;
    GwyTIFFDataType raw_data_type;
    guint raw_data_len;
    const guchar *raw_data;
    gdouble data_cnvt;
    gdouble xreal;
    gdouble yreal;
    gchar *xunit;
    gchar *yunit;
    gchar *dataunit;
} ISDFImage;

typedef struct {
    guint tag;
    GwyTIFFDataType type;
    guint count;
    guchar value[4];
} GwyTIFFEntry;

typedef struct {
    guchar *data;
    gsize size;
    GArray *tags;
    guint16 (*getu16)(const guchar **p);
    gint16 (*gets16)(const guchar **p);
    guint32 (*getu32)(const guchar **p);
    gint32 (*gets32)(const guchar **p);
    gfloat (*getflt)(const guchar **p);
    gdouble (*getdbl)(const guchar **p);
} GwyTIFF;

static gboolean      module_register       (void);
static gint          isdf_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* isdf_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static gboolean      isdf_image_fill_info  (ISDFImage *image,
                                            const GwyTIFF *tiff,
                                            GError **error);
static void          isdf_image_free       (ISDFImage *image);

/* Rudimentary TIFF reader */
static GwyTIFF*            gwy_tiff_load          (const gchar *filename,
                                                   GError **error);
static gboolean            gwy_tiff_load_real     (GwyTIFF *tiff,
                                                   const gchar *filename,
                                                   GError **error);
static void                gwy_tiff_free          (GwyTIFF *tiff);
static guint               gwy_tiff_data_type_size(GwyTIFFDataType type);
static gboolean            gwy_tiff_tags_valid    (const GwyTIFF *tiff);
static const GwyTIFFEntry* gwy_tiff_find_tag      (const GwyTIFF *tiff,
                                                   guint tag);
static gboolean            gwy_tiff_get_sint      (const GwyTIFF *tiff,
                                                   guint tag,
                                                   gint *retval);
static gboolean            gwy_tiff_get_uint      (const GwyTIFF *tiff,
                                                   guint tag,
                                                   guint *retval);
static gboolean            gwy_tiff_get_float     (const GwyTIFF *tiff,
                                                   guint tag,
                                                   gdouble *retval);
static gboolean            gwy_tiff_get_string    (const GwyTIFF *tiff,
                                                   guint tag,
                                                   gchar **retval);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Intematix SDF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("intematix",
                           N_("Intematix SDF data files (.sdf)"),
                           (GwyFileDetectFunc)&isdf_detect,
                           (GwyFileLoadFunc)&isdf_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
isdf_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))) {
        const GwyTIFFEntry *entry;

        if ((entry = gwy_tiff_find_tag(tiff, ISDF_TIFFTAG_FILEID))) {
            const guchar *p = entry->value;

            if (tiff->gets32(&p) == ISDF_MAGIC_NUMBER)
                score = 100;
        }
        gwy_tiff_free(tiff);
    }

    return score;
}

static GwyContainer*
isdf_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunitx, *siunity, *siunitz;
    GwyTIFF *tiff;
    ISDFImage image;
    guint magic, t, i, j;
    gint power10x, power10y, power10z;
    const guchar *p;
    gdouble *data;
    gdouble q;

    if (!(tiff = gwy_tiff_load(filename, error)))
        return NULL;

    memset(&image, 0, sizeof(ISDFImage));
    if (!gwy_tiff_tags_valid(tiff)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid tag data positions were found."));
        goto fail;
    }

    if (!gwy_tiff_get_sint(tiff, ISDF_TIFFTAG_FILEID, &magic)
        || magic != ISDF_MAGIC_NUMBER) {
        err_FILE_TYPE(error, "Intematix SDF");
        goto fail;
    }

    if (!isdf_image_fill_info(&image, tiff, error))
        goto fail;

    t = gwy_tiff_data_type_size(image.raw_data_type);
    if (image.xres*image.yres != image.raw_data_len) {
        err_SIZE_MISMATCH(error, t*image.xres*image.yres, t*image.raw_data_len);
        goto fail;
    }

    siunitx = gwy_si_unit_new_parse(image.xunit, &power10x);
    siunity = gwy_si_unit_new_parse(image.yunit, &power10y);
    siunitz = gwy_si_unit_new_parse(image.dataunit, &power10z);
    if (!gwy_si_unit_equal(siunitx, siunity))
        g_warning("Different x and y units are not representable, ignoring y.");

    dfield = gwy_data_field_new(image.xres, image.yres,
                                image.xreal*pow10(power10x),
                                image.yreal*pow10(power10y),
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, siunitx);
    gwy_data_field_set_si_unit_z(dfield, siunitz);
    g_object_unref(siunitx);
    g_object_unref(siunity);
    g_object_unref(siunitz);
    q = pow10(power10z)/image.data_cnvt;

    data = gwy_data_field_get_data(dfield);
    p = image.raw_data;
    switch (image.raw_data_type) {
        case GWY_TIFF_SLONG:
        for (i = 0; i < image.yres; i++) {
            for (j = 0; j < image.xres; j++)
                data[i*image.xres + j] = q*tiff->gets32(&p);
        }
        break;

        case GWY_TIFF_DOUBLE:
        for (i = 0; i < image.yres; i++) {
            for (j = 0; j < image.xres; j++)
                data[i*image.xres + j] = q*tiff->getdbl(&p);
        }
        break;

        default:
        g_critical("Should not be reached.");
        break;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

fail:
    isdf_image_free(&image);
    gwy_tiff_free(tiff);

    return container;
}

static gboolean
isdf_image_fill_info(ISDFImage *image,
                     const GwyTIFF *tiff,
                     GError **error)
{
    const GwyTIFFEntry *entry;
    const guchar *p;

    /* Required parameters */
    if (!(gwy_tiff_get_uint(tiff, GWY_TIFFTAG_IMAGEWIDTH, &image->xres)
          && gwy_tiff_get_uint(tiff, GWY_TIFFTAG_IMAGELENGTH, &image->yres)
          && gwy_tiff_get_uint(tiff, ISDF_TIFFTAG_IMAGEDEPTH, &image->zres)
          && gwy_tiff_get_uint(tiff, ISDF_TIFFTAG_FILETYPE, &image->file_type)
          && gwy_tiff_get_uint(tiff, ISDF_TIFFTAG_DATATYPE, &image->data_type)
          && gwy_tiff_get_float(tiff, ISDF_TIFFTAG_DATACNVT, &image->data_cnvt)
          && gwy_tiff_get_string(tiff, ISDF_TIFFTAG_XDIMUNIT, &image->xunit)
          && gwy_tiff_get_string(tiff, ISDF_TIFFTAG_YDIMUNIT, &image->yunit)
          && gwy_tiff_get_string(tiff, ISDF_TIFFTAG_DATAUNIT, &image->dataunit)
          && gwy_tiff_get_float(tiff, ISDF_TIFFTAG_IMGXDIM, &image->xreal)
          && gwy_tiff_get_float(tiff, ISDF_TIFFTAG_IMGYDIM, &image->yreal))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Parameter tag set is incomplete."));
        return FALSE;
    }

    if (image->zres != 1) {
        err_UNSUPPORTED(error, _("image depth"));
        return FALSE;
    }

    if (!(entry = gwy_tiff_find_tag(tiff, ISDF_TIFFTAG_SPMDATA))) {
        err_NO_DATA(error);
        return FALSE;
    }
    p = entry->value;
    image->raw_data = tiff->data + tiff->getu32(&p);
    image->raw_data_type = entry->type;
    image->raw_data_len = entry->count;
    if (image->raw_data_type != GWY_TIFF_SLONG
        && image->raw_data_type != GWY_TIFF_DOUBLE) {
        err_UNSUPPORTED(error, _("data type"));
        return FALSE;
    }

    return TRUE;
}

static void
isdf_image_free(ISDFImage *image)
{
    g_free(image->xunit);
    g_free(image->yunit);
    g_free(image->dataunit);
}

/***************************************************************************
 *
 * Rudimentary TIFF reader
 *
 ***************************************************************************/

static GwyTIFF*
gwy_tiff_load(const gchar *filename,
              GError **error)
{
    GwyTIFF *tiff;

    tiff = g_new0(GwyTIFF, 1);
    if (gwy_tiff_load_real(tiff, filename, error))
        return tiff;

    gwy_tiff_free(tiff);
    return NULL;
}

static gboolean
gwy_tiff_load_real(GwyTIFF *tiff,
                   const gchar *filename,
                   GError **error)
{
    GError *err = NULL;
    const guchar *p;
    guint magic, offset, ifdno;

    if (!gwy_file_get_contents(filename, &tiff->data, &tiff->size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return FALSE;
    }

    if (tiff->size < 8) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    p = tiff->data;
    magic = gwy_get_guint32_le(&p);
    switch (magic) {
        case 0x002a4949:
        tiff->getu16 = gwy_get_guint16_le;
        tiff->gets16 = gwy_get_gint16_le;
        tiff->getu32 = gwy_get_guint32_le;
        tiff->gets32 = gwy_get_gint32_le;
        tiff->getflt = gwy_get_gfloat_le;
        tiff->getdbl = gwy_get_gdouble_le;
        break;

        case 0x2a004d4d:
        tiff->getu16 = gwy_get_guint16_be;
        tiff->gets16 = gwy_get_gint16_be;
        tiff->getu32 = gwy_get_guint32_be;
        tiff->gets32 = gwy_get_gint32_be;
        tiff->getflt = gwy_get_gfloat_be;
        tiff->getdbl = gwy_get_gdouble_be;
        break;

        default:
        err_FILE_TYPE(error, "TIFF");
        return FALSE;
        break;
    }

    offset = tiff->getu32(&p);
    tiff->tags = g_array_new(FALSE, FALSE, sizeof(GwyTIFFEntry));
    ifdno = 0;
    do {
        GwyTIFFEntry entry;
        guint nentries, i;

        if (offset + 2 + 4 > tiff->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        "TIFF directory %u ended unexpectedly.", ifdno);
            return FALSE;
        }

        p = tiff->data + offset;
        nentries = tiff->getu16(&p);
        if (offset + 2 + 4 + 12*nentries > tiff->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        "TIFF directory %u ended unexpectedly.", ifdno);
            return FALSE;
        }
        for (i = 0; i < nentries; i++) {
            entry.tag = tiff->getu16(&p);
            entry.type = tiff->getu16(&p);
            entry.count = tiff->getu32(&p);
            memcpy(entry.value, p, 4);
            p += 4;
            g_array_append_val(tiff->tags, entry);
        }
        offset = tiff->getu32(&p);
        ifdno++;
    } while (offset);

    return TRUE;
}

static void
gwy_tiff_free(GwyTIFF *tiff)
{
    if (tiff->tags)
        g_array_free(tiff->tags, TRUE);
    if (tiff->data)
        gwy_file_abandon_contents(tiff->data, tiff->size, NULL);

    g_free(tiff);
}

static guint
gwy_tiff_data_type_size(GwyTIFFDataType type)
{
    switch (type) {
        case GWY_TIFF_BYTE:
        case GWY_TIFF_SBYTE:
        case GWY_TIFF_ASCII:
        return 1;
        break;

        case GWY_TIFF_SHORT:
        case GWY_TIFF_SSHORT:
        return 2;
        break;

        case GWY_TIFF_LONG:
        case GWY_TIFF_SLONG:
        case GWY_TIFF_FLOAT:
        return 4;
        break;

        case GWY_TIFF_RATIONAL:
        case GWY_TIFF_SRATIONAL:
        case GWY_TIFF_DOUBLE:
        return 8;
        break;

        default:
        return 0;
        break;
    }
}

static inline gboolean
gwy_tiff_data_fits(const GwyTIFF *tiff,
                   guint offset,
                   guint item_size,
                   guint nitems)
{
    guint bytesize;

    /* Overflow in total size */
    if (nitems > 0xffffffffU/item_size)
        return FALSE;

    bytesize = nitems*item_size;
    /* Overflow in addition */
    if (offset + bytesize < offset)
        return FALSE;

    return offset + bytesize <= tiff->size;
}

static gboolean
gwy_tiff_tags_valid(const GwyTIFF *tiff)
{
    const guchar *p;
    guint i, offset, item_size;

    for (i = 0; i < tiff->tags->len; i++) {
        const GwyTIFFEntry *entry;

        entry = &g_array_index(tiff->tags, GwyTIFFEntry, i);
        p = entry->value;
        offset = tiff->getu32(&p);
        item_size = gwy_tiff_data_type_size(entry->type);
        /* Uknown types are implicitly OK.  If we cannot read it we never
         * read it by definition, so let the hell take what it refers to. */
        if (item_size
            && entry->count > 4/item_size
            && !gwy_tiff_data_fits(tiff, offset, item_size, entry->count))
            return FALSE;
    }

    return TRUE;
}

static const GwyTIFFEntry*
gwy_tiff_find_tag(const GwyTIFF *tiff,
                  guint tag)
{
    guint i;

    if (!tiff->tags)
        return NULL;

    for (i = 0; i < tiff->tags->len; i++) {
        const GwyTIFFEntry *entry = &g_array_index(tiff->tags, GwyTIFFEntry, i);

        if (entry->tag == tag)
            return entry;
    }

    return NULL;
}

static gboolean
gwy_tiff_get_sint(const GwyTIFF *tiff,
                  guint tag,
                  gint *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    const gchar *q;

    entry = gwy_tiff_find_tag(tiff, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_BYTE:
        q = (const gchar*)p;
        *retval = q[0];
        break;

        case GWY_TIFF_SHORT:
        *retval = tiff->gets16(&p);
        break;

        case GWY_TIFF_LONG:
        *retval = tiff->gets32(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

static gboolean
gwy_tiff_get_uint(const GwyTIFF *tiff,
                  guint tag,
                  guint *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;

    entry = gwy_tiff_find_tag(tiff, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_BYTE:
        *retval = p[0];
        break;

        case GWY_TIFF_SHORT:
        *retval = tiff->gets16(&p);
        break;

        case GWY_TIFF_LONG:
        *retval = tiff->gets32(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

static gboolean
gwy_tiff_get_float(const GwyTIFF *tiff,
                   guint tag,
                   gdouble *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    guint offset;

    entry = gwy_tiff_find_tag(tiff, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_FLOAT:
        *retval = tiff->getflt(&p);
        break;

        case GWY_TIFF_DOUBLE:
        offset = tiff->getu32(&p);
        p = tiff->data + offset;
        *retval = tiff->getdbl(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

static gboolean
gwy_tiff_get_string(const GwyTIFF *tiff,
                    guint tag,
                    gchar **retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    guint offset;

    entry = gwy_tiff_find_tag(tiff, tag);
    if (!entry || entry->type != GWY_TIFF_ASCII)
        return FALSE;

    p = entry->value;
    if (entry->count <= 4) {
        *retval = g_new0(gchar, MAX(entry->count, 1));
        memcpy(*retval, entry->value, entry->count);
    }
    else {
        offset = tiff->getu32(&p);
        p = tiff->data + offset;
        *retval = g_new(gchar, entry->count);
        memcpy(*retval, p, entry->count);
        (*retval)[entry->count-1] = '\0';
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
