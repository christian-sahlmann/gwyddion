/*
 *  @(#) $Id: intematix.c 7756 2007-03-25 08:10:20Z yeti-dn $
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/*
 * This is a rudimentary built-in TIFF reader.
 *
 * It is required to read some TIFF-based files because the software that
 * writes them is very creative with regard to the specification.  In other
 * words, we need to read some incorrect TIFFs too.
 *
 * Names starting GWY_TIFF, GwyTIFF and gwy_tiff are reserved.
 */

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
gwy_tiff_tags_valid(const GwyTIFF *tiff,
                    GError **error)
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
            && !gwy_tiff_data_fits(tiff, offset, item_size, entry->count)) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Invalid tag data positions were found."));
            return FALSE;
        }
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

static GwyTIFF*
gwy_tiff_load(const gchar *filename,
              GError **error)
{
    GwyTIFF *tiff;

    tiff = g_new0(GwyTIFF, 1);
    if (gwy_tiff_load_real(tiff, filename, error)
        && gwy_tiff_tags_valid(tiff, error))
        return tiff;

    gwy_tiff_free(tiff);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

