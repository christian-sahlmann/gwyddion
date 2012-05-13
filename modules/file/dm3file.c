/*
 *  $Id$
 *  Copyright (C) 2012 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-dm3-tem">
 *   <comment>Digital Micrograph DM3 TEM data</comment>
 *   <glob pattern="*.dm3"/>
 *   <glob pattern="*.DM3"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Digital Micrograph DM3 TEM data
 * .dm3
 * Read
 **/


#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define EXTENSION ".dm3"

enum {
    REPORTED_FILE_SIZE_OFF = 16,
    MIN_FILE_SIZE = 3*4 + 1 + 1 + 4,
    TAG_GROUP_MIN_SIZE = 1 + 1 + 4,
    TAG_ENTRY_MIN_SIZE = 1 + 2,
    TAG_TYPE_MIN_SIZE = 4 + 4,
    TAG_TYPE_MARKER = 0x25252525u,
};

typedef enum {
    DM3_IMG_OK = 0,
    DM3_IMG_SKIP = 1,
    DM3_IMG_NOT_FOUND = 2,
    DM3_IMG_ERROR = 3,
} DM3ImgResult;

enum {
    DM3_SHORT   = 2,
    DM3_LONG    = 3,
    DM3_USHORT  = 4,
    DM3_ULONG   = 5,
    DM3_FLOAT   = 6,
    DM3_DOUBLE  = 7,
    DM3_BOOLEAN = 8,
    DM3_CHAR    = 9,
    DM3_OCTET   = 10,
    DM3_STRUCT  = 15,
    DM3_STRING  = 18,
    DM3_ARRAY   = 20
};

typedef enum {
    DM3_DATA_NULL = 0,
    DM3_DATA_SIGNED_INT16 = 1,
    DM3_DATA_REAL4 = 2,
    DM3_DATA_COMPLEX8 = 3,
    DM3_DATA_OBSELETE = 4,
    DM3_DATA_PACKED = 5,
    DM3_DATA_UNSIGNED_INT8 = 6,
    DM3_DATA_SIGNED_INT32 = 7,
    DM3_DATA_RGB = 8,
    DM3_DATA_SIGNED_INT8 = 9,
    DM3_DATA_UNSIGNED_INT16 = 10,
    DM3_DATA_UNSIGNED_INT32 = 11,
    DM3_DATA_REAL8 = 12,
    DM3_DATA_COMPLEX16 = 13,
    DM3_DATA_BINARY = 14,
    DM3_DATA_RGB_UINT8_0 = 15,
    DM3_DATA_RGB_UINT8_1 = 16,
    DM3_DATA_RGB_UINT16 = 17,
    DM3_DATA_RGB_FLOAT32 = 18,
    DM3_DATA_RGB_FLOAT64 = 19,
    DM3_DATA_RGBA_UINT8_0 = 20,
    DM3_DATA_RGBA_UINT8_1 = 21,
    DM3_DATA_RGBA_UINT8_2 = 22,
    DM3_DATA_RGBA_UINT8_3 = 23,
    DM3_DATA_RGBA_UINT16 = 24,
    DM3_DATA_RGBA_FLOAT32 = 25,
    DM3_DATA_RGBA_FLOAT64 = 26,
    DM3_DATA_POINT2_SINT16_0 = 27,
    DM3_DATA_POINT2_SINT16_1 = 28,
    DM3_DATA_POINT2_SINT32_0 = 29,
    DM3_DATA_POINT2_FLOAT32_0 = 30,
    DM3_DATA_RECT_SINT16_1 = 31,
    DM3_DATA_RECT_SINT32_1 = 32,
    DM3_DATA_RECT_FLOAT32_1 = 33,
    DM3_DATA_RECT_FLOAT32_0 = 34,
    DM3_DATA_SIGNED_INT64 = 35,
    DM3_DATA_UNSIGNED_INT64 = 36,
    DM3_DATA_LAST
} DM3DataType;

typedef struct _DM3TagType DM3TagType;
typedef struct _DM3TagEntry DM3TagEntry;
typedef struct _DM3TagGroup DM3TagGroup;
typedef struct _DM3File DM3File;

struct _DM3TagType {
    guint ntypes;
    guint typesize;
    guint *types;
    gconstpointer data;
};

struct _DM3TagEntry {
    gboolean is_group;
    gchar *label;
    DM3TagGroup *group;
    DM3TagType *type;
    DM3TagEntry *parent;
};

struct _DM3TagGroup {
    gboolean is_sorted;
    gboolean is_open;
    guint ntags;
    DM3TagEntry *entries;
};

struct _DM3File {
    guint version;
    guint size;
    gboolean little_endian;
    DM3TagEntry root_entry;
    GHashTable *hash;
};

static gboolean      module_register       (void);
static gint          dm3_detect            (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* dm3_load              (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static DM3ImgResult  dm3_read_image        (DM3File *dm3file,
                                            GwyContainer *container,
                                            guint i,
                                            guint *id,
                                            GError **error);
static gboolean      dm3_get_uint          (DM3File *dm3file,
                                            guint *value,
                                            const gchar *keyfmt,
                                            ...);
static gboolean      dm3_get_int           (DM3File *dm3file,
                                            gint *value,
                                            const gchar *keyfmt,
                                            ...);
static gboolean      dm3_get_float         (DM3File *dm3file,
                                            gdouble *value,
                                            const gchar *keyfmt,
                                            ...);
static gboolean      dm3_get_string        (DM3File *dm3file,
                                            gchar **value,
                                            const gchar *keyfmt,
                                            ...);
static DM3TagType*   dm3_get_leaf_entry    (DM3File *dm3file,
                                            const guint *typespec,
                                            guint typespeclen,
                                            const gchar *keyfmt,
                                            ...);
static DM3TagType*   dm3_get_leaf_entry_lit(DM3File *dm3file,
                                            const guint *typespec,
                                            guint typespeclen,
                                            const gchar *key);
static gboolean      dm3_read_header       (DM3File *dm3file,
                                            const guchar **p,
                                            gsize *size,
                                            GError **error);
static void          dm3_build_hash        (GHashTable *hash,
                                            const DM3TagEntry *entry);
static DM3TagGroup*  dm3_read_group        (DM3TagEntry *parent,
                                            const guchar **p,
                                            gsize *size,
                                            GError **error);
static gboolean      dm3_read_entry        (DM3TagEntry *parent,
                                            DM3TagEntry *entry,
                                            guint idx,
                                            const guchar **p,
                                            gsize *size,
                                            GError **error);
static DM3TagType*   dm3_read_type         (DM3TagEntry *parent,
                                            const guchar **p,
                                            gsize *size,
                                            GError **error);
static guint         dm3_type_size         (DM3TagEntry *parent,
                                            const guint *types,
                                            guint *n,
                                            guint level,
                                            GError **error);
static void          dm3_free_group        (DM3TagGroup *group);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads Digital Micrograph DM3 files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2012",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("dm3file",
                           N_("Digital Micrograph DM3 TEM data (.dm3)"),
                           (GwyFileDetectFunc)&dm3_detect,
                           (GwyFileLoadFunc)&dm3_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
dm3_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    const guchar *p = fileinfo->head;
    guint version, size, ordering, is_sorted, is_open;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->file_size < MIN_FILE_SIZE)
        return 0;

    version = gwy_get_guint32_be(&p);
    size = gwy_get_guint32_be(&p);
    ordering = gwy_get_guint32_be(&p);
    is_sorted = *(p++);
    is_open = *(p++);
    gwy_debug("%u %u %u %u %u", version, size, ordering, is_sorted, is_open);
    if (version != 3
        || size + REPORTED_FILE_SIZE_OFF != fileinfo->file_size
        || ordering > 1
        || is_sorted > 1
        || is_open > 1)
        return 0;

    return 100;
}

static GwyContainer*
dm3_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    DM3File dm3file;
    DM3ImgResult status;
    guchar *buffer = NULL;
    const guchar *p;
    gsize remaining, size = 0;
    GError *err = NULL;
    guint i = 0, id = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    gwy_clear(&dm3file, 1);
    p = buffer;
    remaining = size;

    if (!dm3_read_header(&dm3file, &p, &remaining, error))
        goto fail;

    dm3file.root_entry.is_group = TRUE;
    dm3file.root_entry.label = (gchar*)"";
    if (!(dm3file.root_entry.group
          = dm3_read_group(&dm3file.root_entry, &p, &remaining, error)))
        goto fail;

    dm3file.hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    dm3_build_hash(dm3file.hash, &dm3file.root_entry);

    container = gwy_container_new();
    do {
        status = dm3_read_image(&dm3file, container, i, &id, error);
        i++;
    } while (status == DM3_IMG_OK || status == DM3_IMG_SKIP);

    if (status == DM3_IMG_NOT_FOUND && !id) {
        gwy_object_unref(container);
        err_NO_DATA(error);
    }
    else if (status == DM3_IMG_ERROR) {
        gwy_object_unref(container);
    }

fail:
    dm3_free_group(dm3file.root_entry.group);
    if (dm3file.hash) {
        g_hash_table_destroy(dm3file.hash);
        dm3file.hash = NULL;
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static DM3ImgResult
dm3_read_image(DM3File *dm3file,
               GwyContainer *container,
               guint i,
               guint *id,
               GError **error)
{
    static const gchar res_fmt[]
        = "/ImageList/#%u/ImageData/Dimensions/#%u";
    static const gchar name_fmt[]
        = "/ImageList/#%u/Name";
    static const gchar img_fmt[]
        = "/ImageList/#%u/ImageData/%s";
    static const gchar calib_fmt[]
        = "/ImageList/#%u/ImageData/Calibrations/Dimension/#%u/%s";

    DM3DataType datatype;
    DM3TagType *type;
    DM3ImgResult retval = DM3_IMG_NOT_FOUND;
    GwyByteOrder byteorder = (dm3file->little_endian
                              ? GWY_BYTE_ORDER_LITTLE_ENDIAN
                              : GWY_BYTE_ORDER_BIG_ENDIAN);
    GwyRawDataType rawdatatype;
    gdouble xreal, yreal, xoff, yoff;
    guint xres, yres, pixeldepth, itemsize;
    gchar *xunit = NULL, *yunit = NULL, *title = NULL, *key;
    gdouble scale = 1.0;
    GQuark quark;
    GwySIUnit *unit;
    gint power10;
    GwyDataField *field = NULL;

    if (!dm3_get_uint(dm3file, &xres, res_fmt, i, 0)
        || !dm3_get_uint(dm3file, &yres, res_fmt, i, 1)
        || !dm3_get_float(dm3file, &xreal, calib_fmt, i, 0, "Scale")
        || !dm3_get_float(dm3file, &yreal, calib_fmt, i, 1, "Scale")
        || !dm3_get_float(dm3file, &xoff, calib_fmt, i, 0, "Origin")
        || !dm3_get_float(dm3file, &yoff, calib_fmt, i, 1, "Origin")
        || !dm3_get_string(dm3file, &xunit, calib_fmt, i, 0, "Units")
        || !dm3_get_string(dm3file, &yunit, calib_fmt, i, 1, "Units")
        || !dm3_get_uint(dm3file, &datatype, img_fmt, i, "DataType")
        || !dm3_get_uint(dm3file, &pixeldepth, img_fmt, i, "PixelDepth"))
        goto fail;

    gwy_debug("image #%u: xres %u, yres %u xreal %g [%s] yreal %g [%s], "
              "datatype %u",
              i, xres, yres, xreal, xunit, yreal, yunit, datatype);

    if (!(type = dm3_get_leaf_entry(dm3file, NULL, 0, img_fmt, i, "Data")))
        goto fail;

    retval = DM3_IMG_SKIP;
    if (type->ntypes != 3 || type->types[0] != DM3_ARRAY) {
        gwy_debug("type is not a simple array");
        goto fail;
    }

    if (datatype == DM3_DATA_UNSIGNED_INT8) {
        rawdatatype = GWY_RAW_DATA_UINT8;
        scale = 1.0/255.0;
    }
    else if (datatype == DM3_DATA_SIGNED_INT8) {
        rawdatatype = GWY_RAW_DATA_SINT8;
        scale = 1.0/128.0;
    }
    else if (datatype == DM3_DATA_UNSIGNED_INT16) {
        rawdatatype = GWY_RAW_DATA_UINT16;
        scale = 1.0/65535.0;
    }
    else if (datatype == DM3_DATA_SIGNED_INT16) {
        rawdatatype = GWY_RAW_DATA_SINT16;
        scale = 1.0/32768.0;
    }
    else if (datatype == DM3_DATA_UNSIGNED_INT32) {
        rawdatatype = GWY_RAW_DATA_UINT32;
        scale = 1.0/4294967295.0;
    }
    else if (datatype == DM3_DATA_SIGNED_INT32) {
        rawdatatype = GWY_RAW_DATA_SINT32;
        scale = 1.0/2147483648.0;
    }
    else if (datatype == DM3_DATA_UNSIGNED_INT64) {
        rawdatatype = GWY_RAW_DATA_UINT64;
        scale = 1.0/18446744073709551615.0;
    }
    else if (datatype == DM3_DATA_SIGNED_INT64) {
        rawdatatype = GWY_RAW_DATA_SINT64;
        scale = 1.0/9223372036854775808.0;
    }
    else if (datatype == DM3_DATA_REAL4)
        rawdatatype = GWY_RAW_DATA_FLOAT;
    else if (datatype == DM3_DATA_REAL8)
        rawdatatype = GWY_RAW_DATA_DOUBLE;
    else {
        gwy_debug("type is not implemented");
        goto fail;
    }

    itemsize = gwy_raw_data_size(rawdatatype);
    gwy_debug("gwy raw data type %u (item size %u)", rawdatatype, itemsize);
    if (err_SIZE_MISMATCH(error, itemsize*xres*yres, type->typesize, TRUE)) {
        retval = DM3_IMG_ERROR;
        goto fail;
    }

    unit = gwy_si_unit_new_parse(yunit, &power10);
    yreal *= pow10(power10);
    yoff *= pow10(power10);
    g_object_unref(unit);

    unit = gwy_si_unit_new_parse(xunit, &power10);
    xreal *= pow10(power10);
    xoff *= pow10(power10);

    field = gwy_data_field_new(xres, yres, xreal*xres, yreal*yres, FALSE);
    gwy_data_field_set_si_unit_xy(field, unit);
    g_object_unref(unit);

    gwy_convert_raw_data(type->data, xres*yres, 1, rawdatatype, byteorder,
                         gwy_data_field_get_data(field), scale, 0.0);

    quark = gwy_app_get_data_key_for_id(*id);
    gwy_container_set_object(container, quark, field);
    g_object_unref(field);

    if (dm3_get_string(dm3file, &title, name_fmt, i)) {
        key = g_strdup_printf("/%u/data/title", *id);
        gwy_container_set_string_by_name(container, key, title);
        g_free(key);
        title = NULL;
    }

    retval = DM3_IMG_OK;
    (*id)++;

fail:
    g_free(xunit);
    g_free(yunit);

    return retval;
}

G_GNUC_UNUSED
static gboolean
dm3_get_uint(DM3File *dm3file,
             guint *value,
             const gchar *keyfmt,
             ...)
{
    DM3TagType *type;
    gchar *key;
    va_list ap;
    const guchar *p;
    gboolean ok = FALSE;

    va_start(ap, keyfmt);
    key = g_strdup_vprintf(keyfmt, ap);
    va_end(ap);
    type = dm3_get_leaf_entry_lit(dm3file, NULL, 0, key);
    g_free(key);

    if (type && type->ntypes == 1) {
        p = (const guchar*)type->data;
        if (type->types[0] == DM3_USHORT) {
            if (dm3file->little_endian)
                *value = gwy_get_guint16_le(&p);
            else
                *value = gwy_get_guint16_be(&p);
            ok = TRUE;
        }
        else if (type->types[0] == DM3_ULONG) {
            if (dm3file->little_endian)
                *value = gwy_get_guint32_le(&p);
            else
                *value = gwy_get_guint32_be(&p);
            ok = TRUE;
        }
        else if (type->types[0] == DM3_OCTET) {
            *value = *p;
            ok = TRUE;
        }
    }

    return ok;
}

G_GNUC_UNUSED
static gboolean
dm3_get_int(DM3File *dm3file,
            gint *value,
            const gchar *keyfmt,
            ...)
{
    DM3TagType *type;
    gchar *key;
    va_list ap;
    const guchar *p;
    gboolean ok = FALSE;

    va_start(ap, keyfmt);
    key = g_strdup_vprintf(keyfmt, ap);
    va_end(ap);
    type = dm3_get_leaf_entry_lit(dm3file, NULL, 0, key);
    g_free(key);

    if (type && type->ntypes == 1) {
        p = (const guchar*)type->data;
        if (type->types[0] == DM3_SHORT) {
            if (dm3file->little_endian)
                *value = gwy_get_gint16_le(&p);
            else
                *value = gwy_get_gint16_be(&p);
            ok = TRUE;
        }
        else if (type->types[0] == DM3_LONG) {
            if (dm3file->little_endian)
                *value = gwy_get_gint32_le(&p);
            else
                *value = gwy_get_gint32_be(&p);
            ok = TRUE;
        }
        else if (type->types[0] == DM3_CHAR) {
            *value = *p;
            ok = TRUE;
        }
    }

    return ok;
}

G_GNUC_UNUSED
static gboolean
dm3_get_float(DM3File *dm3file,
              gdouble *value,
              const gchar *keyfmt,
              ...)
{
    DM3TagType *type;
    gchar *key;
    va_list ap;
    const guchar *p;
    gboolean ok = FALSE;

    va_start(ap, keyfmt);
    key = g_strdup_vprintf(keyfmt, ap);
    va_end(ap);
    type = dm3_get_leaf_entry_lit(dm3file, NULL, 0, key);
    g_free(key);

    if (type && type->ntypes == 1) {
        p = (const guchar*)type->data;
        if (type->types[0] == DM3_FLOAT) {
            if (dm3file->little_endian)
                *value = gwy_get_gfloat_le(&p);
            else
                *value = gwy_get_gfloat_be(&p);
            ok = TRUE;
        }
        else if (type->types[0] == DM3_DOUBLE) {
            if (dm3file->little_endian)
                *value = gwy_get_gdouble_le(&p);
            else
                *value = gwy_get_gdouble_be(&p);
            ok = TRUE;
        }
    }

    return ok;
}

G_GNUC_UNUSED
static gboolean
dm3_get_string(DM3File *dm3file,
               gchar **value,
               const gchar *keyfmt,
               ...)
{
    const gchar *encoding = dm3file->little_endian ? "UTF-16LE" : "UTF16-BE";
    DM3TagType *type;
    gchar *key;
    va_list ap;

    *value = NULL;

    va_start(ap, keyfmt);
    key = g_strdup_vprintf(keyfmt, ap);
    va_end(ap);
    type = dm3_get_leaf_entry_lit(dm3file, NULL, 0, key);
    g_free(key);

    if (!type)
        return FALSE;

    if (type->ntypes == 2 && type->types[0] == DM3_STRING)
        *value = g_convert(type->data, 2*type->types[1], "UTF-8", encoding,
                           NULL, NULL, NULL);
    else if (type->ntypes == 3 && type->types[0] == DM3_ARRAY
             && type->types[1] == DM3_USHORT)
        *value = g_convert(type->data, 2*type->types[2], "UTF-8", encoding,
                           NULL, NULL, NULL);

    return !!*value;
}

static DM3TagType*
dm3_get_leaf_entry(DM3File *dm3file,
                   const guint *typespec,
                   guint typespeclen,
                   const gchar *keyfmt,
                   ...)
{
    DM3TagType *type;
    va_list ap;
    gchar *key;

    va_start(ap, keyfmt);
    key = g_strdup_vprintf(keyfmt, ap);
    va_end(ap);

    type = dm3_get_leaf_entry_lit(dm3file, typespec, typespeclen, key);
    g_free(key);

    return type;
}

static DM3TagType*
dm3_get_leaf_entry_lit(DM3File *dm3file,
                       const guint *typespec,
                       guint typespeclen,
                       const gchar *key)
{
    DM3TagType *type;
    guint i;

    gwy_debug("looking for <%s>", key);
    type = g_hash_table_lookup(dm3file->hash, key);

    if (!type) {
        gwy_debug("not found");
        return NULL;
    }

    if (!typespec) {
        gwy_debug("found, not specific type requested, considering OK");
        return type;
    }

    if (type->ntypes != typespeclen) {
        gwy_debug("found, wrong typespec length (%u instead of %u)",
                  type->ntypes, typespeclen);
        return NULL;
    }

    for (i = 0; i < typespeclen; i++) {
        if (typespec[i] != G_MAXUINT && type->types[i] != typespec[i]) {
            gwy_debug("found, wrong typespec mismatch at pos #%u", i);
            return NULL;
        }
    }

    gwy_debug("found, seems OK");
    return type;
}

static gboolean
dm3_read_header(DM3File *dm3file,
                const guchar **p, gsize *size,
                GError **error)
{
    if (*size < MIN_FILE_SIZE) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    dm3file->version = gwy_get_guint32_be(p);
    dm3file->size = gwy_get_guint32_be(p);
    dm3file->little_endian = gwy_get_guint32_be(p);

    if (dm3file->version != 3 || dm3file->little_endian > 1) {
        err_FILE_TYPE(error, "DM3");
        return FALSE;
    }

    if (err_SIZE_MISMATCH(error, dm3file->size + REPORTED_FILE_SIZE_OFF, *size,
                          TRUE))
        return FALSE;

    *size -= 3*4;
    return TRUE;
}

#ifdef DEBUG
static guint
dm3_entry_depth(const DM3TagEntry *entry)
{
    guint depth = 0;

    while (entry) {
        entry = entry->parent;
        depth++;
    }
    return depth;
}
#endif

static gchar*
format_path(const DM3TagEntry *entry)
{
    GPtrArray *path = g_ptr_array_new();
    gchar *retval;
    guint i;

    while (entry) {
        g_ptr_array_add(path, entry->label);
        entry = entry->parent;
    }

    for (i = 0; i < path->len/2; i++)
        GWY_SWAP(gpointer,
                 g_ptr_array_index(path, i),
                 g_ptr_array_index(path, path->len-1 - i));
    g_ptr_array_add(path, NULL);

    retval = g_strjoinv("/", (gchar**)path->pdata);
    g_ptr_array_free(path, TRUE);

    return retval;
}

static void
dm3_build_hash(GHashTable *hash,
               const DM3TagEntry *entry)
{
    if (entry->is_group) {
        const DM3TagGroup *group;
        guint i;

        g_assert(entry->group);
        group = entry->group;
        for (i = 0; i < group->ntags; i++)
            dm3_build_hash(hash, group->entries + i);
    }
    else {
        gchar *path = format_path(entry);
        g_assert(entry->type);
        g_hash_table_replace(hash, path, entry->type);
    }
}

static guint
err_INVALID_TAG(const DM3TagEntry *entry, GError **error)
{
    if (error) {
        gchar *path = format_path(entry);
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid tag type definition in entry ‘%s’."), path);
        g_free(path);
    }
    return G_MAXUINT;
}

static gpointer
err_TRUNCATED(const DM3TagEntry *entry, GError **error)
{
    if (error) {
        gchar *path = format_path(entry);
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is truncated in entry ‘%s’."), path);
        g_free(path);
    }
    return NULL;
}

static DM3TagGroup*
dm3_read_group(DM3TagEntry *parent,
               const guchar **p, gsize *size,
               GError **error)
{
    DM3TagGroup *group = NULL;
    guint i;

    if (*size < TAG_GROUP_MIN_SIZE)
        return err_TRUNCATED(parent, error);

    group = g_new(DM3TagGroup, 1);
    group->is_sorted = *((*p)++);
    group->is_open = *((*p)++);
    group->ntags = gwy_get_guint32_be(p);
    *size -= TAG_GROUP_MIN_SIZE;
    gwy_debug("[%u] Entering a group of %u tags (%u, %u)",
              dm3_entry_depth(parent),
              group->ntags, group->is_sorted, group->is_open);

    group->entries = g_new0(DM3TagEntry, group->ntags);
    for (i = 0; i < group->ntags; i++) {
        gwy_debug("[%u] Reading entry #%u", dm3_entry_depth(parent), i);
        if (!dm3_read_entry(parent, group->entries + i, i, p, size, error)) {
            dm3_free_group(group);
            return NULL;
        }
    }

    gwy_debug("[%u] Leaving group of %u tags read",
              dm3_entry_depth(parent), group->ntags);

    return group;
}

static gboolean
dm3_read_entry(DM3TagEntry *parent,
               DM3TagEntry *entry,
               guint idx,
               const guchar **p, gsize *size,
               GError **error)
{
    guint kind, lab_len;

    if (*size < TAG_ENTRY_MIN_SIZE) {
        err_TRUNCATED(entry, error);
        return FALSE;
    }

    kind = *((*p)++);
    if (kind != 20 && kind != 21) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Tag entry type is neither group nor data."));
        return FALSE;
    }

    entry->parent = parent;
    entry->is_group = (kind == 20);
    lab_len = gwy_get_guint16_be(p);
    *size -= TAG_ENTRY_MIN_SIZE;
    gwy_debug("[%u] Entry is %s",
              dm3_entry_depth(entry), entry->is_group ? "group" : "type");

    if (*size < lab_len) {
        err_TRUNCATED(entry, error);
        return FALSE;
    }

    if (lab_len)
        entry->label = g_strndup(*p, lab_len);
    else
        entry->label = g_strdup_printf("#%u", idx);
    gwy_debug("[%u] Entry label <%s>", dm3_entry_depth(entry), entry->label);
#ifdef DEBUG
    {
        gchar *path = format_path(entry);
        gwy_debug("[%u] Full path <%s>", dm3_entry_depth(entry), path);
        g_free(path);
    }
#endif
    *p += lab_len;
    *size -= lab_len;

    if (entry->is_group) {
        if (!(entry->group = dm3_read_group(entry, p, size, error)))
            return FALSE;
    }
    else {
        if (!(entry->type = dm3_read_type(entry, p, size, error)))
            return FALSE;
    }

    return TRUE;
}

static DM3TagType*
dm3_read_type(DM3TagEntry *parent,
              const guchar **p, gsize *size,
              GError **error)
{
    DM3TagType *type = NULL;
    guint i, marker, consumed_ntypes;

    if (*size < TAG_TYPE_MIN_SIZE)
        return err_TRUNCATED(parent, error);

    marker = gwy_get_guint32_be(p);
    if (marker != TAG_TYPE_MARKER) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Tag type does not start with marker ‘%s’."), "%%%%");
        return NULL;
    }

    type = g_new(DM3TagType, 1);
    type->ntypes = gwy_get_guint32_be(p);
    *size -= TAG_TYPE_MIN_SIZE;
    gwy_debug("[%u] Entering a typespec of %u items",
              dm3_entry_depth(parent), type->ntypes);

    if (*size < sizeof(guint32)*type->ntypes) {
        g_free(type);
        return err_TRUNCATED(parent, error);
    }

    type->types = g_new0(guint, type->ntypes);
    for (i = 0; i < type->ntypes; i++) {
        type->types[i] = gwy_get_guint32_be(p);
        *size -= sizeof(guint32);
        gwy_debug("[%u] Typespec #%u is %u",
                  dm3_entry_depth(parent), i, type->types[i]);
    }
    gwy_debug("[%u] Leaving a typespec of %u items",
              dm3_entry_depth(parent), type->ntypes);

    consumed_ntypes = type->ntypes;
    if ((type->typesize = dm3_type_size(parent, type->types, &consumed_ntypes,
                                        0, error)) == G_MAXUINT)
        goto fail;
    if (consumed_ntypes != 0) {
        err_INVALID_TAG(parent, error);
        goto fail;
    }

    gwy_debug("[%u] Type size: %u", dm3_entry_depth(parent), type->typesize);
    if (*size < type->typesize) {
        err_TRUNCATED(parent, error);
        goto fail;
    }
    type->data = *p;
    *p += type->typesize;

    return type;

fail:
    g_free(type->types);
    g_free(type);
    return NULL;
}

static guint
dm3_type_size(DM3TagEntry *parent,
              const guint *types, guint *n,
              guint level,
              GError **error)
{
    static const guint atomic_type_sizes[] = {
        0, 0, 2, 4, 2, 4, 4, 8, 1, 1, 1,
    };
    guint primary_type;

    if (!*n)
        return err_INVALID_TAG(parent, error);

    primary_type = types[0];
    if (primary_type < G_N_ELEMENTS(atomic_type_sizes)
        && atomic_type_sizes[primary_type]) {
        gwy_debug("<%u> Known atomic type %u", level, primary_type);
        if (*n < 1)
            return err_INVALID_TAG(parent, error);
        *n -= 1;
        if (!atomic_type_sizes[primary_type])
            return err_INVALID_TAG(parent, error);
        return atomic_type_sizes[primary_type];
    }

    if (primary_type == DM3_STRING) {
        gwy_debug("<%u> string type", level);
        if (*n < 2)
            return err_INVALID_TAG(parent, error);
        *n -= 2;
        /* The second item is the length, presumably in characters (2byte) */
        gwy_debug("<%u> string length %u",
                  level, types[1]);
        return 2*types[1];
    }

    if (primary_type == DM3_ARRAY) {
        guint item_size, oldn;

        gwy_debug("<%u> array type", level);
        if (*n < 3)
            return err_INVALID_TAG(parent, error);
        *n -= 1;
        types += 1;
        oldn = *n;
        gwy_debug("<%u> recusring to figure out item type",
                  level);
        if ((item_size = dm3_type_size(parent, types, n,
                                       level+1, error)) == G_MAXUINT)
            return G_MAXUINT;
        gwy_debug("<%u> item type found", level);
        types += oldn - *n;
        if (*n < 1)
            return err_INVALID_TAG(parent, error);
        gwy_debug("<%u> array length %u",
                  level, types[0]);
        *n -= 1;
        return types[0]*item_size;
    }

    if (primary_type == DM3_STRUCT) {
        guint structsize = 0, namelength, fieldnamelength, nfields, i;

        gwy_debug("<%u> struct type", level);
        if (*n < 3)
            return err_INVALID_TAG(parent, error);

        namelength = types[1];
        nfields = types[2];
        types += 3;
        *n -= 3;
        gwy_debug("<%u> namelength: %u, nfields: %u",
                  level, namelength, nfields);
        structsize = namelength;

        for (i = 0; i < nfields; i++) {
            guint oldn, field_size;

            gwy_debug("<%u> struct field #%u", level, i);
            if (*n < 2)
                return err_INVALID_TAG(parent, error);
            fieldnamelength = types[0];
            types += 1;
            *n -= 1;
            oldn = *n;
            structsize += fieldnamelength;
            gwy_debug("<%u> recusring to figure out field type",
                      level);
            if ((field_size = dm3_type_size(parent, types, n,
                                            level+1, error)) == G_MAXUINT)
                return G_MAXUINT;
            gwy_debug("<%u> field type found", level);
            types += oldn - *n;
            structsize += field_size;
        }
        gwy_debug("<%u> all struct fields read", level);
        return structsize;
    }

    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                _("Invalid or unsupported tag type %u."), primary_type);
    return G_MAXUINT;
}

static void
dm3_free_group(DM3TagGroup *group)
{
    guint i;

    if (!group)
        return;

    for (i = 0; i < group->ntags; i++) {
        DM3TagEntry *entry = group->entries + i;
        if (entry->group) {
            dm3_free_group(entry->group);
            entry->group = NULL;
        }
        else if (entry->type) {
            g_free(entry->type->types);
            g_free(entry->type);
            entry->type = NULL;
        }
        g_free(entry->label);
    }
    g_free(group);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
