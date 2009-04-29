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

static gboolean      module_register   (void);
static gint          mat5_detect       (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* mat5_load         (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyDataField* try_read_datafield(gsize size,
                                        const guchar *buffer,
                                        gboolean msb,
                                        GString *name);
static gboolean      zinflate_variable (gsize csize,
                                        const guchar *compressed,
                                        gboolean msb,
                                        GByteArray *output,
                                        GError **error);

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

static GwyContainer*
mat5_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    guint16 (*get_guint16)(const guchar **p);
    guint32 (*get_guint32)(const guchar **p);

    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p, *zp;
    gsize size = 0;
    gint id = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GString *name = NULL;
    gboolean msb;
    GByteArray *zbuffer = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < 128) {
        err_TOO_SHORT(error);
        goto fail;
    }

    p = buffer + MAGIC_OFFSET;
    if (memcmp(p, MAGIC_NATIVE, MAGIC_SIZE) == 0)
        msb = FALSE;
    else if (memcmp(p, MAGIC_SWAP, MAGIC_SIZE) == 0)
        msb = TRUE;
    else {
        err_FILE_TYPE(error, "Matlab MAT5");
        goto fail;
    }

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    msb != msb;
#endif

    get_guint16 = msb ? gwy_get_guint16_be : gwy_get_guint16_le;
    get_guint32 = msb ? gwy_get_guint32_be : gwy_get_guint32_le;

    p = buffer + MAT5_HEADER_SIZE;
    while (p - buffer < size) {
        guint nbytes, data_type;
        if (p - buffer >= size - MAT5_VAR_SIZE) {
            g_warning("Truncated data");
            break;
        }

        nbytes = get_guint16(&p);
        if (nbytes) {
            p -= 2;
            data_type = get_guint32(&p);
            nbytes = get_guint32(&p);
            gwy_debug("Data of type %d, size %d", data_type, nbytes);
            zp = p;
            p += nbytes;

            if (data_type == MAT5_COMPRESSED) {
                if (!zbuffer)
                    zbuffer = g_byte_array_new();
                if (!zinflate_variable(nbytes, p - nbytes, msb, zbuffer, error))
                    goto fail;

                zp = zbuffer->data;
                data_type = get_guint32(&zp);
                nbytes = get_guint32(&zp);
                gwy_debug("+CompressedData of type %d, size %d",
                          data_type, nbytes);
            }
            /* Here zp always points to the real data and p after the buffer
             * data */

            if (data_type == MAT5_MATRIX) {
                if (!name)
                    name = g_string_new(NULL);
                dfield = try_read_datafield(nbytes, zp, msb, name);
                if (dfield) {
                    GQuark quark = gwy_app_get_data_key_for_id(id++);
                    gchar *key;

                    if (!container)
                        container = gwy_container_new();
                    gwy_container_set_object(container, quark, dfield);
                    g_object_unref(dfield);
                    key = g_strdup_printf("%s/%d",
                                          g_quark_to_string(quark), id);
                    gwy_container_set_string_by_name(container, key,
                                                     g_strdup(name->str));
                    g_free(key);
                }
            }
        }
        else {
            data_type = get_guint16(&p);
            zp = p;
            p += 4;
            gwy_debug("ShortData of type %d", data_type);
            /* There is not much we can do with such data because there is
             * no identification available.  */
        }
    }

    if (!container)
        err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    if (name)
        g_string_free(name, TRUE);
    if (zbuffer)
        g_byte_array_free(zbuffer, TRUE);

    return container;
}

static GwyDataField*
try_read_datafield(gsize size,
                   const guchar *p,
                   gboolean msb,
                   GString *name)
{
    /* Indexed by Mat5DataType */
    static const struct {
        Mat5ClassType class_type;
        guint type_size;
    }
    type_correspondence[] = {
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
    guint32 (*get_guint32)(const guchar **p);
    gint32 (*get_gint32)(const guchar **p);

    guint data_type, nbytes, flags;
    Mat5ClassType klass;
    Mat5ArrayFlags array_flags;
    gint i, xres, yres;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data;

    for (i = 0; i < (gint)size; i++)
        g_printerr("%02x%c", p[i], i % 8 == 7 ? '\n' : ' ');
    g_printerr("\n");

    get_guint32 = msb ? gwy_get_guint32_be : gwy_get_guint32_le;
    get_gint32 = msb ? gwy_get_gint32_be : gwy_get_gint32_le;

    /* Array flags */
    if (size < MAT5_TAG_SIZE)
        return NULL;
    data_type = get_guint32(&p);
    nbytes = get_guint32(&p);
    size -= MAT5_TAG_SIZE;
    if (data_type != MAT5_UINT32 || nbytes != 2*4 || size < nbytes)
        return NULL;

    flags = get_guint32(&p);
    array_flags = flags & MAT5_FLAG_MASK;
    klass = flags & MAT5_CLASS_MASK;
    gwy_debug("array_flags = %02x, class = %02x", array_flags, klass);
    get_guint32(&p);    /* reserved */

    if (klass != MAT5_CLASS_DOUBLE
        && klass != MAT5_CLASS_SINGLE)
        return NULL;

    /* Dimensions array */
    if (size < MAT5_TAG_SIZE)
        return NULL;
    data_type = get_guint32(&p);
    nbytes = get_guint32(&p);
    size -= MAT5_TAG_SIZE;
    /* We can only import two-dimensional arrays */
    if (data_type != MAT5_INT32 || nbytes != 2*4 || size < nbytes)
        return NULL;

    yres = get_gint32(&p);
    xres = get_gint32(&p);
    size -= nbytes;
    gwy_debug("xres = %d, yres = %d", xres, yres);
    if (err_DIMENSION(NULL, xres) || err_DIMENSION(NULL, yres))
        return NULL;

    /* FIXME: Must read short tags too! */
    /* Array name */
    if (size < MAT5_TAG_SIZE)
        return NULL;
    data_type = get_guint32(&p);
    nbytes = get_guint32(&p);
    gwy_debug("data_type %u, nbytes %u", data_type, nbytes);
    size -= MAT5_TAG_SIZE;
    if (data_type != MAT5_INT8 || size < nbytes)
        return NULL;

    g_string_truncate(name, 0);
    g_string_append_len(name, p, nbytes);
    p += (nbytes + 7)/8*8;
    size -= (nbytes + 7)/8*8;

    /* Real part */
    if (size < MAT5_TAG_SIZE)
        return NULL;
    data_type = get_guint32(&p);
    nbytes = get_guint32(&p);
    gwy_debug("data_type %u, nbytes %u", data_type, nbytes);
    size -= MAT5_TAG_SIZE;
    if (size < nbytes)
        return NULL;
    gwy_debug("data_type %u, corresponding class is %u, type size %u",
              data_type,
              type_correspondence[data_type].class_type,
              type_correspondence[data_type].type_size);
    if (data_type > G_N_ELEMENTS(type_correspondence)
        || type_correspondence[data_type].class_type != klass) {
        g_warning("Variable class %u does not correspond to data type %u",
                  klass, data_type);
        return NULL;
    }
    if (type_correspondence[data_type].type_size == 0
        || type_correspondence[data_type].type_size * xres*yres > nbytes)
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
        gdouble (*get)(const guchar **p)
            = msb ? gwy_get_gdouble_be : gwy_get_gdouble_le;
        for (i = 0; i < xres*yres; i++)
            data[i] = get(&p);
    }
    else if (klass == MAT5_CLASS_SINGLE) {
        gfloat (*get)(const guchar **p)
            = msb ? gwy_get_gfloat_be : gwy_get_gfloat_le;
        for (i = 0; i < xres*yres; i++)
            data[i] = get(&p);
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

    if ((status = inflateInit(zbuf)) != Z_OK
        || (status = inflate(zbuf, flush_mode)) != Z_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Decompression of compressed variable failed with "
                      "error %d (%s)."),
                    status, zbuf->msg);
        retval = FALSE;
    }
    status = inflateEnd(zbuf);
    /* This should not really happen whatever data we pass in. */
    if (status != Z_OK) {
        g_critical("inflateEnd() failed with error %d (%s)", status, zbuf->msg);
    }

    return retval;
}


static gboolean
zinflate_variable(gsize csize, const guchar *compressed,
                  gboolean msb,
                  GByteArray *output,
                  GError **error)
{
    z_stream zbuf; /* decompression stream */
    guint data_type, nbytes;
    const guchar *p;

    g_byte_array_set_size(output, MAT5_TAG_SIZE);
    if (!zinflate_into(&zbuf, Z_SYNC_FLUSH, csize, compressed, output, error))
        return FALSE;

    p = output->data;
    data_type = (msb ? gwy_get_guint32_be : gwy_get_guint32_le)(&p);
    nbytes = (msb ? gwy_get_guint32_be : gwy_get_guint32_le)(&p);

    g_byte_array_set_size(output, nbytes);

    /* FIXME: Why Z_FINISH does not work? */
    return zinflate_into(&zbuf, Z_SYNC_FLUSH, csize, compressed, output, error);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
