/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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

#include <glib/gutils.h>

/*
 * This is a rudimentary built-in TIFF reader.
 *
 * It is required to read some TIFF-based files because the software that
 * writes them is very creative with regard to the specification.  In other
 * words, we need to read some incorrect TIFFs too.  In particular, we do not
 * expect directories to be sorted and we accept bogus (nul) entries.
 *
 * Names starting GWY_TIFF, GwyTIFF and gwy_tiff are reserved.
 */

/* Search in all directories */
#define GWY_TIFF_ANY_DIR ((guint)-1)

/* Convenience functions for the 0th directory */
#define gwy_tiff_get_sint0(T, t, r) gwy_tiff_get_sint((T), 0, (t), (r))
#define gwy_tiff_get_uint0(T, t, r) gwy_tiff_get_uint((T), 0, (t), (r))
#define gwy_tiff_get_float0(T, t, r) gwy_tiff_get_float((T), 0, (t), (r))
#define gwy_tiff_get_string0(T, t, r) gwy_tiff_get_string((T), 0, (t), (r))

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
typedef enum {
    GWY_TIFFTAG_IMAGE_WIDTH       = 256,
    GWY_TIFFTAG_IMAGE_LENGTH      = 257,
    GWY_TIFFTAG_BITS_PER_SAMPLE   = 258,
    GWY_TIFFTAG_COMPRESSION       = 259,
    GWY_TIFFTAG_PHOTOMETRIC       = 262,
    GWY_TIFFTAG_FILL_ORDER        = 266,
    GWY_TIFFTAG_DOCUMENT_NAME     = 269,
    GWY_TIFFTAG_IMAGE_DESCRIPTION = 270,
    GWY_TIFFTAG_STRIP_OFFSETS     = 273,
    GWY_TIFFTAG_ORIENTATION       = 274,
    GWY_TIFFTAG_SAMPLES_PER_PIXEL = 277,
    GWY_TIFFTAG_ROWS_PER_STRIP    = 278,
    GWY_TIFFTAG_STRIP_BYTE_COUNTS = 279,
    GWY_TIFFTAG_X_RESOLUTION      = 282,
    GWY_TIFFTAG_Y_RESOLUTION      = 283,
    GWY_TIFFTAG_PLANAR_CONFIG     = 284,
    GWY_TIFFTAG_RESOLUTION_UNIT   = 296,
    GWY_TIFFTAG_SOFTWARE          = 305,
    GWY_TIFFTAG_DATE_TIME         = 306,
    GWY_TIFFTAG_ARTIST            = 315,
    GWY_TIFFTAG_SAMPLE_FORMAT     = 339
} GwyTIFFTag;

/* Values of some standard tags.
 * Note only values interesting for us are enumerated.  Add more from the
 * standard if needed.  */
typedef enum {
    GWY_TIFF_COMPRESSION_NONE = 1,
} GwyTIFFCompression;

typedef enum {
    GWY_TIFF_ORIENTATION_TOPLEFT = 1,
    GWY_TIFF_ORIENTATION_TOPRIGHT = 2,
    GWY_TIFF_ORIENTATION_BOTRIGHT = 3,
    GWY_TIFF_ORIENTATION_BOTLEFT = 4,
    GWY_TIFF_ORIENTATION_LEFTTOP = 5,
    GWY_TIFF_ORIENTATION_RIGHTTOP = 6,
    GWY_TIFF_ORIENTATION_RIGHTBOT = 7,
    GWY_TIFF_ORIENTATION_LEFTBOT = 8,
} GwyTIFFOrientation;

typedef enum {
    GWY_TIFF_PHOTOMETRIC_MIN_IS_WHITE = 0,
    GWY_TIFF_PHOTOMETRIC_MIN_IS_BLACK = 1,
    GWY_TIFF_PHOTOMETRIC_RGB          = 2,
} GwyTIFFPhotometric;

typedef enum {
    GWY_TIFF_PLANAR_CONFIG_CONTIGNUOUS = 1,
    GWY_TIFF_PLANAR_CONFIG_SEPARATE = 2,
} GwyTIFFPlanarConfig;

typedef enum {
    GWY_TIFF_RESOLUTION_UNIT_NONE = 0,
    GWY_TIFF_RESOLUTION_UNIT_INCH = 1,
    GWY_TIFF_RESOLUTION_UNIT_CENTIMETER = 2,
} GwyTIFFResolutionUnit;

typedef enum {
    GWY_TIFF_SAMPLE_FORMAT_UNSIGNED_INTEGER = 1,
    GWY_TIFF_SAMPLE_FORMAT_SIGNED_INTEGER = 2,
    GWY_TIFF_SAMPLE_FORMAT_FLOAT = 3,
    GWY_TIFF_SAMPLE_FORMAT_UNDEFINED = 4
} GwyTIFFSampleFormat;

/* TIFF structure representation */
typedef struct {
    guint tag;
    GwyTIFFDataType type;
    guint count;
    guchar value[4];
} GwyTIFFEntry;

typedef struct {
    guchar *data;
    gsize size;
    GPtrArray *dirs;  // Array of GwyTIFFEntry GArray*
    guint16 (*get_guint16)(const guchar **p);
    gint16 (*get_gint16)(const guchar **p);
    guint32 (*get_guint32)(const guchar **p);
    gint32 (*get_gint32)(const guchar **p);
    gfloat (*get_gfloat)(const guchar **p);
    gdouble (*get_gdouble)(const guchar **p);
} GwyTIFF;

/* State-object for image data reading */
typedef struct {
    /* public for reading */
    guint dirno;
    guint width;
    guint height;
    guint stripe_rows;
    guint bits_per_sample;
    guint samples_per_pixel;
    /* private */
    guint rowstride;
    guint *offsets;
    guint sample_format;
} GwyTIFFImageReader;

/* Does not need to free tags on failure, the caller takes care of it. */
static gboolean
gwy_tiff_load_impl(GwyTIFF *tiff,
                   const gchar *filename,
                   GError **error)
{
    GError *err = NULL;
    const guchar *p;
    guint magic, offset, nentries;

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
        tiff->get_guint16 = gwy_get_guint16_le;
        tiff->get_gint16 = gwy_get_gint16_le;
        tiff->get_guint32 = gwy_get_guint32_le;
        tiff->get_gint32 = gwy_get_gint32_le;
        tiff->get_gfloat = gwy_get_gfloat_le;
        tiff->get_gdouble = gwy_get_gdouble_le;
        break;

        case 0x2a004d4d:
        tiff->get_guint16 = gwy_get_guint16_be;
        tiff->get_gint16 = gwy_get_gint16_be;
        tiff->get_guint32 = gwy_get_guint32_be;
        tiff->get_gint32 = gwy_get_gint32_be;
        tiff->get_gfloat = gwy_get_gfloat_be;
        tiff->get_gdouble = gwy_get_gdouble_be;
        break;

        default:
        err_FILE_TYPE(error, "TIFF");
        return FALSE;
        break;
    }

    tiff->dirs = g_ptr_array_new();
    p = tiff->data + 4;
    offset = tiff->get_guint32(&p);
    do {
        GArray *tags;
        guint i;

        if (offset + 2 + 4 > tiff->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("TIFF directory %u ended unexpectedly."),
                        (guint)tiff->dirs->len);
            return FALSE;
        }

        p = tiff->data + offset;
        nentries = tiff->get_guint16(&p);
        if (offset + 2 + 4 + 12*nentries > tiff->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("TIFF directory %u ended unexpectedly."),
                        (guint)tiff->dirs->len);
            return FALSE;
        }

        tags = g_array_sized_new(FALSE, FALSE, sizeof(GwyTIFFEntry), nentries);
        g_ptr_array_add(tiff->dirs, tags);

        for (i = 0; i < nentries; i++) {
            GwyTIFFEntry entry;

            entry.tag = tiff->get_guint16(&p);
            entry.type = (GwyTIFFDataType)tiff->get_guint16(&p);
            entry.count = tiff->get_guint32(&p);
            memcpy(entry.value, p, 4);
            p += 4;
            g_array_append_val(tags, entry);
        }
        offset = tiff->get_guint32(&p);
    } while (offset);

    return TRUE;
}

static inline void
gwy_tiff_free(GwyTIFF *tiff)
{
    if (tiff->dirs) {
        guint i;

        for (i = 0; i < tiff->dirs->len; i++)
            g_array_free((GArray*)g_ptr_array_index(tiff->dirs, i), TRUE);

        g_ptr_array_free(tiff->dirs, TRUE);
    }

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
gwy_tiff_tags_valid(const GwyTIFF *tiff,
                    GError **error)
{
    const guchar *p;
    guint i, j, offset, item_size;

    for (i = 0; i < tiff->dirs->len; i++) {
        const GArray *tags = (const GArray*)g_ptr_array_index(tiff->dirs, i);

        for (j = 0; j < tags->len; j++) {
            const GwyTIFFEntry *entry;

            entry = &g_array_index(tags, GwyTIFFEntry, j);
            p = entry->value;
            offset = tiff->get_guint32(&p);
            item_size = gwy_tiff_data_type_size(entry->type);
            /* Uknown types are implicitly OK.  If we cannot read it we never
             * read it by definition, so let the hell take what it refers to.
             * This also means readers of custom types have to check the size
             * themselves. */
            if (item_size
                && entry->count > 4/item_size
                && !gwy_tiff_data_fits(tiff, offset, item_size, entry->count)) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Invalid tag data positions were found."));
                return FALSE;
            }
        }
    }

    return TRUE;
}

static gint
gwy_tiff_tag_compare(gconstpointer a, gconstpointer b)
{
    const GwyTIFFEntry *ta = (const GwyTIFFEntry*)a;
    const GwyTIFFEntry *tb = (const GwyTIFFEntry*)b;

    if (ta->tag < tb->tag)
        return -1;
    if (ta->tag > tb->tag)
        return 1;
    return 0;
}

static inline void
gwy_tiff_sort_tags(GwyTIFF *tiff)
{
    guint i;

    for (i = 0; i < tiff->dirs->len; i++)
        g_array_sort((GArray*)g_ptr_array_index(tiff->dirs, i),
                     gwy_tiff_tag_compare);
}

static const GwyTIFFEntry*
gwy_tiff_find_tag(const GwyTIFF *tiff,
                  guint dirno,
                  guint tag)
{
    const GwyTIFFEntry *entry;
    const GArray *tags;
    guint lo, hi, m;

    if (!tiff->dirs)
        return NULL;

    /* If dirno is GWY_TIFF_ANY_DIR, search in all directories. */
    if (dirno == GWY_TIFF_ANY_DIR) {
        for (m = 0; m < tiff->dirs->len; m++) {
            if ((entry = gwy_tiff_find_tag(tiff, m, tag)))
                return entry;
        }
        return NULL;
    }

    if (dirno >= tiff->dirs->len)
        return NULL;

    tags = (const GArray*)g_ptr_array_index(tiff->dirs, dirno);
    lo = 0;
    hi = tags->len-1;
    while (hi - lo > 1) {
        m = (lo + hi)/2;
        entry = &g_array_index(tags, GwyTIFFEntry, m);
        if (entry->tag > tag)
            hi = m;
        else
            lo = m;
    }

    entry = &g_array_index(tags, GwyTIFFEntry, lo);
    if (entry->tag == tag)
        return entry;

    entry = &g_array_index(tags, GwyTIFFEntry, hi);
    if (entry->tag == tag)
        return entry;

    return NULL;
}

G_GNUC_UNUSED static gboolean
gwy_tiff_get_uint(const GwyTIFF *tiff,
                  guint dirno,
                  guint tag,
                  guint *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;

    entry = gwy_tiff_find_tag(tiff, dirno, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_BYTE:
        *retval = p[0];
        break;

        case GWY_TIFF_SHORT:
        *retval = tiff->get_guint16(&p);
        break;

        case GWY_TIFF_LONG:
        *retval = tiff->get_guint32(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

G_GNUC_UNUSED static gboolean
gwy_tiff_get_uints(const GwyTIFF *tiff,
                   guint dirno,
                   guint tag,
                   guint expected_count,
                   guint *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    guint i, offset, size = 0;

    entry = gwy_tiff_find_tag(tiff, dirno, tag);
    if (!entry || entry->count != expected_count)
        return FALSE;

    p = entry->value;
    if (entry->type == GWY_TIFF_BYTE)
        size = expected_count;
    else if (entry->type == GWY_TIFF_SHORT)
        size = 2*expected_count;
    else if (entry->type == GWY_TIFF_LONG)
        size = 4*expected_count;
    else
        return FALSE;

    if (size > 4) {
        offset = tiff->get_guint32(&p);
        p = tiff->data + offset;
    }

    for (i = 0; i < expected_count; i++) {
        switch (entry->type) {
            case GWY_TIFF_BYTE:
            *(retval++) = *(p++);
            break;

            case GWY_TIFF_SHORT:
            *(retval++) = tiff->get_guint16(&p);
            break;

            case GWY_TIFF_LONG:
            *(retval++) = tiff->get_guint32(&p);
            break;

            default:
            return FALSE;
            break;
        }
    }

    return TRUE;
}

G_GNUC_UNUSED static gboolean
gwy_tiff_get_sint(const GwyTIFF *tiff,
                  guint dirno,
                  guint tag,
                  gint *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    const gchar *q;

    entry = gwy_tiff_find_tag(tiff, dirno, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_SBYTE:
        q = (const gchar*)p;
        *retval = q[0];
        break;

        case GWY_TIFF_BYTE:
        *retval = p[0];
        break;

        case GWY_TIFF_SHORT:
        *retval = tiff->get_guint16(&p);
        break;

        case GWY_TIFF_SSHORT:
        *retval = tiff->get_gint16(&p);
        break;

        /* XXX: If the value does not fit, this is wrong no matter what. */
        case GWY_TIFF_LONG:
        *retval = tiff->get_guint32(&p);
        break;

        case GWY_TIFF_SLONG:
        *retval = tiff->get_gint32(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

G_GNUC_UNUSED static gboolean
gwy_tiff_get_bool(const GwyTIFF *tiff,
                  guint dirno,
                  guint tag,
                  gboolean *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    const gchar *q;

    entry = gwy_tiff_find_tag(tiff, dirno, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_BYTE:
        case GWY_TIFF_SBYTE:
        q = (const gchar*)p;
        *retval = !!q[0];
        break;

        case GWY_TIFF_SHORT:
        case GWY_TIFF_SSHORT:
        *retval = !!tiff->get_gint16(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

G_GNUC_UNUSED static gboolean
gwy_tiff_get_float(const GwyTIFF *tiff,
                   guint dirno,
                   guint tag,
                   gdouble *retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    guint offset;

    entry = gwy_tiff_find_tag(tiff, dirno, tag);
    if (!entry || entry->count != 1)
        return FALSE;

    p = entry->value;
    switch (entry->type) {
        case GWY_TIFF_FLOAT:
        *retval = tiff->get_gfloat(&p);
        break;

        case GWY_TIFF_DOUBLE:
        offset = tiff->get_guint32(&p);
        p = tiff->data + offset;
        *retval = tiff->get_gdouble(&p);
        break;

        default:
        return FALSE;
        break;
    }

    return TRUE;
}

G_GNUC_UNUSED static gboolean
gwy_tiff_get_string(const GwyTIFF *tiff,
                    guint dirno,
                    guint tag,
                    gchar **retval)
{
    const GwyTIFFEntry *entry;
    const guchar *p;
    guint offset;

    entry = gwy_tiff_find_tag(tiff, dirno, tag);
    if (!entry || entry->type != GWY_TIFF_ASCII)
        return FALSE;

    p = entry->value;
    if (entry->count <= 4) {
        *retval = g_new0(gchar, MAX(entry->count, 1) + 1);
        memcpy(*retval, entry->value, entry->count);
    }
    else {
        offset = tiff->get_guint32(&p);
        p = tiff->data + offset;
        *retval = g_new(gchar, entry->count);
        memcpy(*retval, p, entry->count);
        (*retval)[entry->count-1] = '\0';
    }

    return TRUE;
}

G_GNUC_UNUSED static GwyTIFFImageReader*
gwy_tiff_get_image_reader(const GwyTIFF *tiff,
                          guint dirno,
                          guint max_samples,
                          GError **error)
{
    GwyTIFFImageReader reader;
    guint nstripes, i, ssize;
    guint *bps;

    reader.dirno = dirno;

    /* Required integer fields */
    if (!gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_IMAGE_WIDTH,
                           &reader.width)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Required tag %u was not found."),
                    GWY_TIFFTAG_IMAGE_WIDTH);
        return NULL;
    }
    if (!gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_IMAGE_LENGTH,
                           &reader.height)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Required tag %u was not found."),
                    GWY_TIFFTAG_IMAGE_LENGTH);
        return NULL;
    }

    /* The TIFF specs say this is required, but it seems to default to 1. */
    if (!gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_SAMPLES_PER_PIXEL,
                           &reader.samples_per_pixel))
        reader.samples_per_pixel = 1;
    if (reader.samples_per_pixel == 0
        || reader.samples_per_pixel > max_samples) {
        err_UNSUPPORTED(error, "SamplesPerPixel");
        return NULL;
    }

    /* The TIFF specs say this is required, but it seems to default to 1. */
    bps = g_new(guint, reader.samples_per_pixel);
    if (!gwy_tiff_get_uints(tiff, dirno, GWY_TIFFTAG_BITS_PER_SAMPLE,
                            reader.samples_per_pixel, bps))
        reader.bits_per_sample = 1;
    else {
        for (i = 1; i < reader.samples_per_pixel; i++) {
            if (bps[i] != bps[i-1]) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Non-uniform bits per sample are unsupported."));
                g_free(bps);
                return NULL;
            }
        }
        reader.bits_per_sample = bps[0];
    }
    g_free(bps);

    /* The TIFF specs say this is required, but it seems to default to
     * MAXINT.  Setting more reasonably to RowsPerStrip = ImageLength achieves
     * the same ends. */
    if (!gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_ROWS_PER_STRIP,
                           &reader.stripe_rows))
        reader.stripe_rows = reader.height;

    /*
     * The data sample type (default is unsigned integer)
     */
    if (!gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_SAMPLE_FORMAT,
                           &reader.sample_format))
        reader.sample_format = GWY_TIFF_SAMPLE_FORMAT_UNSIGNED_INTEGER;

    /* Integer fields specifying data in a format we do not support */
    if (gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_COMPRESSION, &i)
        && i != GWY_TIFF_COMPRESSION_NONE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Compression type %u is not supported."), i);
        return NULL;
    }
    if (gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_PLANAR_CONFIG, &i)
        && i != GWY_TIFF_PLANAR_CONFIG_CONTIGNUOUS) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Planar configuration %u is not supported."), i);
        return NULL;
    }
    if (reader.bits_per_sample != 8
        && reader.bits_per_sample != 16
        && reader.bits_per_sample != 32) {
        err_BPP(error, reader.bits_per_sample);
        return NULL;
    }
    if (reader.sample_format != GWY_TIFF_SAMPLE_FORMAT_UNSIGNED_INTEGER
        && reader.sample_format != GWY_TIFF_SAMPLE_FORMAT_SIGNED_INTEGER) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unsupported sample format"));
        return NULL;
    }

    if (reader.stripe_rows == 0 || reader.stripe_rows > reader.width) {
        err_INVALID(error, "RowsPerStripe");
        return NULL;
    }
    if (err_DIMENSION(error, reader.width)
        || err_DIMENSION(error, reader.height))
        return NULL;

    nstripes = (reader.height + reader.stripe_rows-1)/reader.stripe_rows;
    reader.offsets = g_new(guint, nstripes);

    /* Stripe offsets and byte counts.
     * Actually, we ignore the latter as compression is not permitted. */
    if (nstripes == 1) {
        if (!gwy_tiff_get_uint(tiff, dirno, GWY_TIFFTAG_STRIP_OFFSETS,
                               reader.offsets)) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Required tag %u was not found."),
                        GWY_TIFFTAG_STRIP_OFFSETS);
            g_free(reader.offsets);
            return NULL;
        }
    }
    else {
        const GwyTIFFEntry *entry;
        const guchar *p;

        if (!(entry = gwy_tiff_find_tag(tiff, dirno,
                                         GWY_TIFFTAG_STRIP_OFFSETS))
            || entry->type != GWY_TIFF_LONG
            || entry->count != nstripes) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Required tag %u was not found."),
                        GWY_TIFFTAG_STRIP_OFFSETS);
            g_free(reader.offsets);
            return NULL;
        }

        /* Matching type ensured the tag data is at a valid position in the
         * file. */
        p = entry->value;
        i = tiff->get_guint32(&p);
        p = tiff->data + i;
        for (i = 0; i < nstripes; i++)
            reader.offsets[i] = tiff->get_guint32(&p);
    }

    /* Validate stripe offsets and sizes */
    reader.rowstride = (reader.bits_per_sample/8 * reader.samples_per_pixel
                        * reader.width);
    ssize = reader.rowstride * reader.stripe_rows;
    for (i = 0; i < nstripes; i++) {
        if (i == nstripes-1 && reader.height % reader.stripe_rows)
            ssize = reader.rowstride * (reader.height % reader.stripe_rows);

        if (reader.offsets[i] + ssize > tiff->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("File is truncated."));
            g_free(reader.offsets);
            return NULL;
        }
    }

    /* If we got here we are convinced we can read the image data. */
    return (GwyTIFFImageReader*)g_memdup(&reader, sizeof(GwyTIFFImageReader));
}

G_GNUC_UNUSED static inline void
gwy_tiff_read_image_row(const GwyTIFF *tiff,
                        const GwyTIFFImageReader *reader,
                        guint channelno,
                        guint rowno,
                        gdouble q,
                        gdouble z0,
                        gdouble *dest)
{
    GwyTIFFSampleFormat sformat = (GwyTIFFSampleFormat)reader->sample_format;
    const guchar *p;
    guint stripeno, stripeindex, i, skip;

    g_return_if_fail(reader->dirno < tiff->dirs->len);
    g_return_if_fail(rowno < reader->height);
    g_return_if_fail(channelno < reader->samples_per_pixel);

    stripeno = rowno/reader->stripe_rows;
    stripeindex = rowno % reader->stripe_rows;
    p = tiff->data + (reader->offsets[stripeno] + stripeindex*reader->rowstride
                      + (reader->bits_per_sample/8)*channelno);

    switch (reader->bits_per_sample) {
        case 8:
        skip = reader->samples_per_pixel;
        if (sformat == GWY_TIFF_SAMPLE_FORMAT_UNSIGNED_INTEGER) {
            for (i = 0; i < reader->width; i++, p += skip)
                dest[i] = z0 + q*(*p);
        }
        else if (sformat == GWY_TIFF_SAMPLE_FORMAT_SIGNED_INTEGER) {
            const gchar *s = (const gchar*)p;
            for (i = 0; i < reader->width; i++, s += skip)
                dest[i] = z0 + q*(*s);
        }
        break;

        case 16:
        skip = (reader->samples_per_pixel - 1)*reader->bits_per_sample;
        if (sformat == GWY_TIFF_SAMPLE_FORMAT_UNSIGNED_INTEGER) {
            for (i = 0; i < reader->width; i++, p += skip)
                dest[i] = z0 + q*tiff->get_guint16(&p);
        }
        else if (sformat == GWY_TIFF_SAMPLE_FORMAT_SIGNED_INTEGER) {
            for (i = 0; i < reader->width; i++, p += skip)
                dest[i] = z0 + q*tiff->get_gint16(&p);
        }
        break;

        case 32:
        skip = (reader->samples_per_pixel - 1)*reader->bits_per_sample;
        if (sformat == GWY_TIFF_SAMPLE_FORMAT_UNSIGNED_INTEGER) {
            for (i = 0; i < reader->width; i++, p += skip)
                dest[i] = z0 + q*tiff->get_guint32(&p);
        }
        else if (sformat == GWY_TIFF_SAMPLE_FORMAT_SIGNED_INTEGER) {
            for (i = 0; i < reader->width; i++, p += skip)
                dest[i] = z0 + q*tiff->get_gint32(&p);
        }
        break;

        default:
        g_return_if_reached();
        break;
    }
}

/* Idempotent, use: reader = gwy_tiff_image_reader_free(reader); */
G_GNUC_UNUSED static inline GwyTIFFImageReader*
gwy_tiff_image_reader_free(GwyTIFFImageReader *reader)
{
    if (reader) {
        g_free(reader->offsets);
        g_free(reader);
    }
    return NULL;
}

G_GNUC_UNUSED static inline guint
gwy_tiff_get_n_dirs(const GwyTIFF *tiff)
{
    if (!tiff->dirs)
        return 0;

    return tiff->dirs->len;
}

G_GNUC_UNUSED static GwyTIFF*
gwy_tiff_load(const gchar *filename,
              GError **error)
{
    GwyTIFF *tiff;

    tiff = g_new0(GwyTIFF, 1);
    if (gwy_tiff_load_impl(tiff, filename, error)
        && gwy_tiff_tags_valid(tiff, error)) {
        gwy_tiff_sort_tags(tiff);
        return tiff;
    }

    gwy_tiff_free(tiff);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

