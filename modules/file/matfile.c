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
#define DEBUG 1
/* FIXME: Other apps may frown at us for registering this. */
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-matlab-mat5">
 *   <comment>Matlab MAT5 data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="124" value="\x01\x00\x4d\x49"/>
 *     <match type="string" offset="124" value="\x00\x01\x49\x4d"/>
 *   </magic>
 *   <glob pattern="*.mat"/>
 *   <glob pattern="*.MAT"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include <zlib.h>

#include "err.h"
#include "get.h"

#define MAGIC_SWAP "\x01\x00\x4d\x49"
#define MAGIC_NATIVE "\x00\x01\x49\x4d"
#define MAGIC_SIZE (sizeof(MAGIC_NATIVE)-1)
#define MAGIC_OFFSET 124

#define EXTENSION ".mat"

#define MAT5_PAD(x) (((x) + MAT5_TAG_SIZE-1)/MAT5_TAG_SIZE*MAT5_TAG_SIZE)

enum {
    MAXDEPTH = 2,
};

enum {
    MAT5_HEADER_SIZE = 128,
    MAT5_TAG_SIZE = 8,
    MAT5_VAR_SIZE = 56,    /* minimum variable size, if not packed into tag */
};

typedef enum {
    MAT5_INT8       = 1,
    MAT5_UINT8      = 2,
    MAT5_INT16      = 3,
    MAT5_UINT16     = 4,
    MAT5_INT32      = 5,
    MAT5_UINT32     = 6,
    MAT5_SINGLE     = 7,
    MAT5_DOUBLE     = 9,
    MAT5_INT64      = 12,
    MAT5_UINT64     = 13,
    MAT5_MATRIX     = 14,
    MAT5_COMPRESSED = 15,
    MAT5_UTF8       = 16,
    MAT5_UTF16      = 17,    /* endian applies to these too */
    MAT5_UTF32      = 18,
} Mat5DataType;

typedef enum {
    MAT5_CLASS_CELL     = 1,
    MAT5_CLASS_STRUCT   = 2,
    MAT5_CLASS_OBJECT   = 3,
    MAT5_CLASS_CHAR     = 4,
    MAT5_CLASS_SPARSE   = 5,
    MAT5_CLASS_DOUBLE   = 6,
    MAT5_CLASS_SINGLE   = 7,
    MAT5_CLASS_INT8     = 8,
    MAT5_CLASS_UINT8    = 9,
    MAT5_CLASS_INT16    = 10,
    MAT5_CLASS_UINT16   = 11,
    MAT5_CLASS_INT32    = 12,
    MAT5_CLASS_UINT32   = 13,
    MAT5_CLASS_INT64    = 14,
    MAT5_CLASS_UINT64   = 15,
    MAT5_CLASS_FUNCTION = 16,
} Mat5ClassType;

typedef enum {
    MAT5_FLAG_COMPLEX = 0x1000,
    MAT5_FLAG_GLOBAL  = 0x2000,
    MAT5_FLAG_LOGICAL = 0x4000,
    MAT5_FLAG_MASK    = 0x7000,
    MAT5_CLASS_MASK   = 0x00ff,
} Mat5ArrayFlags;

enum {
    MAX_DIMS = 4,
};

struct _Mat5Element;
struct _Mat5Matrix;

typedef struct _Mat5Element {
    Mat5DataType type;
    guint nitems;
    /* single-item value are represented directly, arrays are kept as pointers
     */
    union {
        gint i;
        gint *ia;
        guint u;
        guint *ua;
        gdouble f;
        gdouble *fa;
        gint64 i64;
        gint64 *i64a;
        guint64 u64;
        guint64 *u64a;
        guchar *str;
        struct Mat5Matrix *mat;
    } value;
} Mat5Element;

typedef struct _Mat5Matrix {
    Mat5ClassType klass;
    Mat5ArrayFlags flags;
    guint dims[MAX_DIMS];
    guint nitems;    /* calculated */
    gchar *name;
    /* Regular arrays */
    gpointer real;
    gpointer imag;
    /* Structures */
    gchar **field_names;
    struct Mat5Matrix *fields;
    /* Objects, in addition */
    gchar *class_name;
} Mat5Matrix;

struct _Mat5FileCursor;
struct _Mat5FileContext;

typedef struct _Mat5FileContext {
    guint16 (*get_guint16)(const guchar **p);
    guint32 (*get_guint32)(const guchar **p);
    gdouble (*get_gdouble)(const guchar **p);
    gfloat (*get_gfloat)(const guchar **p);
    GByteArray *zbuffer;
    struct _Mat5FileCursor *zbuffer_owner;
    gboolean msb;
} Mat5FileContext;

typedef struct _Mat5FileCursor {
    Mat5FileContext *context;
    gsize size;   /* This is *remaining* size */
    const guchar *p;    /* Buffer position. */
    const guchar *zp;  /* Data pointer (possibly to deflated data). */
    Mat5DataType data_type;
    guint nbytes;
} Mat5FileCursor;

static gboolean      module_register   (void);
static gint          mat5_detect       (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* mat5_load         (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyDataField* try_read_datafield(Mat5FileCursor *parent,
                                        GString *name);
static gboolean      zinflate_variable (Mat5FileCursor *cursor,
                                        GError **error);

/* Indexed by Mat5DataType */
static const struct {
    Mat5ClassType class_type;
    guint type_size;
}
typeinfo[] = {
    { 0,                     0, },
    { MAT5_CLASS_INT8,       1, },
    { MAT5_CLASS_UINT8,      1, },
    { MAT5_CLASS_INT16,      2, },
    { MAT5_CLASS_UINT16,     2, },
    { MAT5_CLASS_INT32,      4, },
    { MAT5_CLASS_UINT32,     4, },
    { MAT5_CLASS_SINGLE,     4, },
    { 0,                     0, },
    { MAT5_CLASS_DOUBLE,     8, },
    { 0,                     0, },
    { 0,                     0, },
    { 0,                     0, },
    { MAT5_CLASS_INT64,      8, },
    { MAT5_CLASS_UINT64,     8, },
    { 0,                     0, },
    { 0,                     0, },
    { 0,                     1, },
    { 0,                     2, },
    { 0,                     4, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Matlab MAT files v5."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("mat5file",
                           N_("Matlab MAT 5 files (.mat)"),
                           (GwyFileDetectFunc)&mat5_detect,
                           (GwyFileLoadFunc)&mat5_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
mat5_detect(const GwyFileDetectInfo *fileinfo,
            gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_OFFSET + MAGIC_SIZE
        && (memcmp(fileinfo->head + MAGIC_OFFSET, MAGIC_NATIVE, MAGIC_SIZE) == 0
            || memcmp(fileinfo->head + MAGIC_OFFSET, MAGIC_SWAP, MAGIC_SIZE) == 0))

        score = 100;

    return score;
}

static gboolean
mat5_next_tag(Mat5FileCursor *cursor, GError **error)
{
    Mat5FileContext *fc = cursor->context;
    guint padded_size;

    /* If we own the buffer and are called to get the next tag, then the
     * previous tag has been processed and the buffer is free to use again. */
    if (fc->zbuffer_owner == cursor)
        fc->zbuffer_owner = NULL;

    if (cursor->size < MAT5_TAG_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is truncated."));
        return FALSE;
    }

    /* Short tags have nonzero in first two bytes.  So says the Matlab docs,
     * however, this statement seems to be a big-endianism.  Check the two
     * *upper* bytes of data type. */
    cursor->data_type = fc->get_guint32(&cursor->p);
    cursor->nbytes = cursor->data_type >> 16;
    gwy_debug("raw data_type: %08x", cursor->data_type);
    if (cursor->nbytes == 0) {
        /* Normal (long) tag */
        cursor->nbytes = fc->get_guint32(&cursor->p);
        cursor->size -= MAT5_TAG_SIZE;
        /* Elements *start* at multiples of 8, but the last element in the
         * file may lack the padding. */
        padded_size = MIN(MAT5_PAD(cursor->nbytes), cursor->size);
        gwy_debug("Data of type %u, size %u",
                  cursor->data_type, cursor->nbytes);
        if (cursor->nbytes > cursor->size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("File is truncated."));
            return FALSE;
        }
        cursor->zp = cursor->p;
        cursor->p += padded_size;

        if (cursor->data_type == MAT5_COMPRESSED) {
            if (fc->zbuffer_owner) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Compressed data inside compressed data found."));
                return FALSE;
            }

            /* This inflates from zp into zbuffer */
            if (!zinflate_variable(cursor, error))
                return FALSE;

            g_file_set_contents("dump", fc->zbuffer->data, fc->zbuffer->len,
                                NULL);
            fc->zbuffer_owner = cursor;
            cursor->size -= padded_size;

            /* Only after that we make zp the data pointer */
            cursor->zp = fc->zbuffer->data;
            cursor->data_type = fc->get_guint32(&cursor->zp);
            cursor->nbytes = fc->get_guint32(&cursor->zp);
            gwy_debug("+CompressedData of type %u, size %u",
                      cursor->data_type, cursor->nbytes);
        }
        else
            cursor->size -= padded_size;
    }
    else {
        /* Short tag, the length seems to be simply the upper two bytes,
         * whereas the data type the lower two bytes.  Do not trust the nice
         * boxes showing which byte is which in the documentation.  They were
         * made by the evil big-endian people.  */
        cursor->data_type &= 0xffff;
        cursor->zp = cursor->p;
        cursor->p += 4;
        cursor->size -= MAT5_TAG_SIZE;
        if (cursor->data_type >= G_N_ELEMENTS(typeinfo)
            || cursor->nbytes > 4
            || cursor->nbytes < typeinfo[cursor->data_type].type_size
            || cursor->nbytes % typeinfo[cursor->data_type].type_size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Invalid short tag of type %u claims to consists of "
                          "%u bytes."),
                        cursor->data_type, cursor->nbytes);
            return FALSE;
        }
        gwy_debug("ShortData of type %u", cursor->data_type);
    }

    /* Here zp always points to the real data and p after the buffer data */
    return TRUE;
}

static GwyContainer*
mat5_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    gint tagno = 0, id = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GString *name = NULL;
    Mat5FileContext fc;
    Mat5FileCursor cursor;

    gwy_clear(&cursor, 1);
    gwy_clear(&fc, 1);
    cursor.context = &fc;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAT5_HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }

    p = buffer + MAGIC_OFFSET;
    if (memcmp(p, MAGIC_NATIVE, MAGIC_SIZE) == 0)
        fc.msb = FALSE;
    else if (memcmp(p, MAGIC_SWAP, MAGIC_SIZE) == 0)
        fc.msb = TRUE;
    else {
        err_FILE_TYPE(error, "Matlab MAT5");
        goto fail;
    }

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    fc.msb = !fc.msb;
#endif

    fc.get_guint16 = fc.msb ? gwy_get_guint16_be : gwy_get_guint16_le;
    fc.get_guint32 = fc.msb ? gwy_get_guint32_be : gwy_get_guint32_le;
    fc.get_gfloat = fc.msb ? gwy_get_gfloat_be : gwy_get_gfloat_le;
    fc.get_gdouble = fc.msb ? gwy_get_gdouble_be : gwy_get_gdouble_le;
    fc.zbuffer = g_byte_array_new();
    name = g_string_new(NULL);

    cursor.p = buffer + MAT5_HEADER_SIZE;
    cursor.size = size - MAT5_HEADER_SIZE;

    for (tagno = 0; cursor.size; tagno++) {
        if (!mat5_next_tag(&cursor, error))
            break;

        /* The only interesting case is MATRIX */
        if (cursor.data_type == MAT5_MATRIX) {
            if ((dfield = try_read_datafield(&cursor, name))) {
                GQuark quark = gwy_app_get_data_key_for_id(id++);
                gchar *key;

                if (!container)
                    container = gwy_container_new();

                gwy_container_set_object(container, quark, dfield);
                g_object_unref(dfield);
                key = g_strconcat(g_quark_to_string(quark), "/title", NULL);
                gwy_container_set_string_by_name(container, key,
                                                 g_strdup(name->str));
                g_free(key);
            }
        }
    }

    if (!container)
        err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    g_string_free(name, TRUE);
    g_byte_array_free(fc.zbuffer, TRUE);

    return container;
}

static GwyDataField*
try_read_datafield(Mat5FileCursor *parent,
                   GString *name)
{
    Mat5FileContext *fc = parent->context;
    Mat5FileCursor cursor;
    Mat5ClassType klass;
    Mat5ArrayFlags array_flags;
    gint i, j, xres, yres;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    guint flags;
    gdouble *data;

    gwy_clear(&cursor, 1);
    cursor.context = parent->context;
    cursor.p = parent->zp;
    cursor.size = parent->nbytes;

    for (i = 0; i < 128; i++)
        printf("%02x%c", cursor.p[i], i % 16 == 15 ? '\n' : ' ');

    /* Array flags */
    if (!mat5_next_tag(&cursor, NULL)
        || cursor.data_type != MAT5_UINT32 || cursor.nbytes != 2*4)
        return NULL;

    flags = fc->get_guint32(&cursor.zp);
    array_flags = flags & MAT5_FLAG_MASK;
    klass = flags & MAT5_CLASS_MASK;
    gwy_debug("array_flags = %02x, class = %02x", array_flags, klass);
    /* reserved: fc->get_guint32(&cursor.zp); */

    if (klass != MAT5_CLASS_DOUBLE
        && klass != MAT5_CLASS_SINGLE
        && klass != MAT5_CLASS_STRUCT)
        return NULL;

    /* Dimensions array */
    /* We can only import two-dimensional arrays */
    if (!mat5_next_tag(&cursor, NULL)
        || cursor.data_type != MAT5_INT32 || cursor.nbytes != 2*4)
        return NULL;

    yres = fc->get_guint32(&cursor.zp);
    xres = fc->get_guint32(&cursor.zp);
    gwy_debug("xres = %d, yres = %d", xres, yres);
    if (err_DIMENSION(NULL, xres) || err_DIMENSION(NULL, yres))
        return NULL;

    /* Array name */
    if (!mat5_next_tag(&cursor, NULL)
        || cursor.data_type != MAT5_INT8)
        return NULL;

    g_string_truncate(name, 0);
    g_string_append_len(name, cursor.zp, cursor.nbytes);
    gwy_debug("name = %s", name->str);

    /* Debug struct contents */
    if (klass == MAT5_CLASS_STRUCT) {
        guint n, field_name_len;

        if (!mat5_next_tag(&cursor, NULL)
            || cursor.data_type != MAT5_INT32)
            return NULL;

        field_name_len = fc->get_guint32(&cursor.zp);
        gwy_debug("field_name_len: %u", field_name_len);

        if (!mat5_next_tag(&cursor, NULL)
            || cursor.data_type != MAT5_INT8)
            return NULL;

        n = cursor.nbytes/field_name_len;
        for (i = 0; i < n; i++) {
            gwy_debug("struct field%d %s", i, cursor.zp);
            cursor.zp += field_name_len;
            cursor.nbytes -= field_name_len;
        }
        return NULL;
    }

    /* Real part */
    if (!mat5_next_tag(&cursor, NULL))
        return NULL;
    gwy_debug("data_type %u, corresponding class is %u, type size %u",
              cursor.data_type,
              typeinfo[cursor.data_type].class_type,
              typeinfo[cursor.data_type].type_size);
    if (cursor.data_type >= G_N_ELEMENTS(typeinfo)
        || typeinfo[cursor.data_type].class_type != klass) {
        g_warning("Variable class %u does not correspond to data type %u",
                  klass, cursor.data_type);
        return NULL;
    }
    if (typeinfo[cursor.data_type].type_size == 0
        || typeinfo[cursor.data_type].type_size * xres*yres > cursor.nbytes)
        return NULL;

    dfield = gwy_data_field_new(xres, yres, 5e-9*xres, 5e-9*yres, FALSE);
    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);
    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
    data = gwy_data_field_get_data(dfield);

    if (klass == MAT5_CLASS_DOUBLE) {
        for (j = 0; j < xres; j++) {
            for (i = 0; i < yres; i++) {
                data[i*xres + j] = fc->get_gdouble(&cursor.zp);
            }
        }
    }
    else if (klass == MAT5_CLASS_SINGLE) {
        for (j = 0; j < xres; j++) {
            for (i = 0; i < yres; i++)
                data[i*xres + j] = fc->get_gfloat(&cursor.zp);
        }
    }

    /* Imaginary part (optional): ignore */

    return dfield;
}

static inline gboolean
zinflate_into(z_stream *zbuf,
              gint flush_mode,
              gsize csize,
              const guchar *compressed,
              GByteArray *output,
              GError **error)
{
    gint status;
    gboolean retval = TRUE;

    memset(zbuf, 0, sizeof(z_stream));

    zbuf->next_in = (char*)compressed;
    zbuf->avail_in = csize;
    zbuf->next_out = output->data;
    zbuf->avail_out = output->len;

    /* XXX: zbuf->msg does not seem to ever contain anything, so just report
     * the error codes. */
    if ((status = inflateInit(zbuf)) != Z_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("zlib initialization failed with error %d, "
                      "cannot decompress data."),
                    status);
        return FALSE;
    }

    if ((status = inflate(zbuf, flush_mode)) != Z_OK
        /* zlib return Z_STREAM_END also when we *exactly* exhaust all input.
         * But this is no error, in fact it should happen every time, so check
         * for it specifically. */
        && !(status == Z_STREAM_END
             && zbuf->total_in == csize
             && zbuf->total_out == output->len)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Decompression of compressed variable failed with "
                      "error %d."),
                    status);
        retval = FALSE;
    }

    status = inflateEnd(zbuf);
    /* This should not really happen whatever data we pass in.  And we have
     * already our output, so just make some noise and get over it.  */
    if (status != Z_OK)
        g_critical("inflateEnd() failed with error %d", status);

    return retval;
}

static gboolean
zinflate_variable(Mat5FileCursor *cursor,
                  GError **error)
{
    Mat5FileContext *fc = cursor->context;
    z_stream zbuf; /* decompression stream */
    const guchar *p;
    guint nbytes;

    g_byte_array_set_size(fc->zbuffer, MAT5_TAG_SIZE);
    if (!zinflate_into(&zbuf, Z_SYNC_FLUSH,
                       cursor->nbytes, cursor->zp, fc->zbuffer, error))
        return FALSE;

    /* FIXME: We may be aborted here if nbytes is utter crap. */
    p = fc->zbuffer->data;
    fc->get_guint32(&p);    /* data_type */
    nbytes = fc->get_guint32(&p);
    g_byte_array_set_size(fc->zbuffer, nbytes + MAT5_TAG_SIZE);

    return zinflate_into(&zbuf, Z_SYNC_FLUSH,
                         cursor->nbytes, cursor->zp, fc->zbuffer, error);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
