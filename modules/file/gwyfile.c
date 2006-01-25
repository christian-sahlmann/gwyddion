/*
 *  @(#) $Id$
 *  Copyright (C) 2003 David Necas (Yeti), Petr Klapetek.
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
#include <errno.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include <libgwyddion/gwymacros.h>

#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "err.h"

#define EXTENSION ".gwy"
#define MAGIC "GWYO"
#define MAGIC2 "GWYP"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

typedef struct {
    GArray *map;
    gint len;
    gint *rmap;
    GwyContainer *target;
    GString *str;
} CompressIdData;

static gboolean      module_register         (const gchar *name);
static gint          gwyfile_detect          (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* gwyfile_load            (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gboolean      gwyfile_save            (GwyContainer *data,
                                              const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static GwyContainer* gwyfile_compress_data_ids(GwyContainer *data);
static GObject*      gwy_container_deserialize_old (const guchar *buffer,
                                                    gsize size,
                                                    gsize *position);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Loads and saves Gwyddion native data files (serialized objects)."),
    "Yeti <yeti@gwyddion.net>",
    "0.9",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_file_func_register("gwyfile",
                           N_("Gwyddion native format (.gwy)"),
                           (GwyFileDetectFunc)&gwyfile_detect,
                           (GwyFileLoadFunc)&gwyfile_load,
                           (GwyFileSaveFunc)&gwyfile_save,
                           NULL);

    return TRUE;
}

static gint
gwyfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && (memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE) == 0
            || memcmp(fileinfo->buffer, MAGIC2, MAGIC_SIZE) == 0))
        score = 100;

    return score;
}

static GwyContainer*
gwyfile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GObject *object;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < MAGIC_SIZE
        || (memcmp(buffer, MAGIC, MAGIC_SIZE)
            && memcmp(buffer, MAGIC2, MAGIC_SIZE))) {
        err_FILE_TYPE(error, "Gwyddion");
        gwy_file_abandon_contents(buffer, size, &err);
        return NULL;
    }

    if (!memcmp(buffer, MAGIC, MAGIC_SIZE))
        object = gwy_container_deserialize_old(buffer + MAGIC_SIZE,
                                               size - MAGIC_SIZE, &pos);
    else
        object = gwy_serializable_deserialize(buffer + MAGIC_SIZE,
                                              size - MAGIC_SIZE, &pos);

    gwy_file_abandon_contents(buffer, size, &err);
    if (!object) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data deserialization failed."));
        return NULL;
    }
    if (!GWY_IS_CONTAINER(object)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data deserialization succeeded, but resulted in "
                      "an unexpected object %s."),
                    g_type_name(G_TYPE_FROM_INSTANCE(object)));
        g_object_unref(object);
        return NULL;
    }

    /* XXX: This should be no longer a hard error, though it's still unexpected
    dfield = gwy_container_get_object_by_name(GWY_CONTAINER(object), "/0/data");
    if (!dfield || !GWY_IS_DATA_FIELD(dfield)) {
        g_warning("File %s contains no data field", filename);
        g_object_unref(object);
        return NULL;
    }
    */

    return (GwyContainer*)object;
}

static gboolean
gwyfile_save(GwyContainer *data,
             const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *compressed;
    GByteArray *buffer;
    FILE *fh;
    gboolean ok = TRUE;

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    compressed = gwyfile_compress_data_ids(data);
    buffer = gwy_serializable_serialize(G_OBJECT(compressed), NULL);
    if (fwrite(MAGIC2, 1, MAGIC_SIZE, fh) != MAGIC_SIZE
        || fwrite(buffer->data, 1, buffer->len, fh) != buffer->len) {
        err_WRITE(error);
        ok = FALSE;
        g_unlink(filename);
    }
    fclose(fh);
    g_byte_array_free(buffer, TRUE);
    g_object_unref(compressed);

    return ok;
}

static gint
key_get_int_prefix(const gchar *strkey,
                   guint *len)
{
    guint i;

    if (strkey[0] != GWY_CONTAINER_PATHSEP)
        return -1;
    for (i = 0; g_ascii_isdigit(strkey[i + 1]); i++)
        ;
    if (!i || strkey[i + 1] != GWY_CONTAINER_PATHSEP)
        return -1;

    *len = i + 2;
    return atoi(strkey + 1);
}

/* Check whether hash key matches "/[0-9]+/data" */
static void
hash_data_find_func(gpointer key,
                    gpointer value,
                    gpointer user_data)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GValue *gvalue = (GValue*)value;
    GArray *array = (GArray*)user_data;
    const gchar *strkey;
    guint len;
    gint i;

    strkey = g_quark_to_string(quark);
    i = key_get_int_prefix(strkey, &len);
    if (i < 0)
        return;
    if (!gwy_strequal(strkey + len, "data"))
        return;

    g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
    g_array_append_val(array, i);
}

/* Remap /N/data, /N/mask, /N/show, /N/select
 * trees elsewhere. copying other values to the same keys */
static void
hash_data_index_map_func(gpointer key,
                         gpointer value,
                         gpointer user_data)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GValue *copy, *gvalue = (GValue*)value;
    CompressIdData *cidd = (CompressIdData*)user_data;
    const gchar *strkey;
    gboolean remap = FALSE;
    guint len;
    gint i;

    strkey = g_quark_to_string(quark);
    i = key_get_int_prefix(strkey, &len);
    copy = g_new0(GValue, 1);
    g_value_init(copy, G_VALUE_TYPE(gvalue));
    g_value_copy(gvalue, copy);
    if (G_VALUE_HOLDS_OBJECT(gvalue))
        g_object_unref(g_value_get_object(gvalue));

    if (i < 0) {
        gwy_debug("<%s> -> <%s> (no number)", strkey, strkey);
        gwy_container_set_value(cidd->target, quark, copy, 0);
        return;
    }

    if (i >= cidd->len || cidd->rmap[i] == -1) {
        gwy_debug("<%s> -> <%s> (eccentric data id)", strkey, strkey);
        gwy_container_set_value(cidd->target, quark, copy, 0);
        return;
    }

    if (cidd->rmap[i] == i) {
        gwy_debug("<%s> -> <%s> (identity)", strkey, strkey);
        gwy_container_set_value(cidd->target, quark, copy, 0);
        return;
    }

    if ((g_str_has_prefix(strkey + len, "data")
         || g_str_has_prefix(strkey + len, "base")
         || g_str_has_prefix(strkey + len, "mask")
         || g_str_has_prefix(strkey + len, "show"))
        && (strkey[len + 4] == '\0'
            || strkey[len + 4] == GWY_CONTAINER_PATHSEP))
        remap = TRUE;
    else if (g_str_has_prefix(strkey + len, "select")
        && (strkey[len + 6] == '\0'
            || strkey[len + 6] == GWY_CONTAINER_PATHSEP))
        remap = TRUE;

    if (remap) {
        g_string_printf(cidd->str, "/%d/%s", cidd->rmap[i], strkey + len);
        gwy_debug("<%s> -> <%s> (REMAP)", strkey, cidd->str->str);
        gwy_container_set_value_by_name(cidd->target, cidd->str->str, copy,
                                        NULL);
    }
    else {
        gwy_debug("<%s> -> <%s> (nothing matched)", strkey, strkey);
        gwy_container_set_value(cidd->target, quark, copy, 0);
    }
}

static gint
compare_integers(gconstpointer a,
                 gconstpointer b)
{
    gint ia = *(const gint*)a;
    gint ib = *(const gint*)b;

    return ia - ib;
}

static GwyContainer*
gwyfile_compress_data_ids(GwyContainer *data)
{
    CompressIdData cidd;
    guint i;

    cidd.map = g_array_new(FALSE, FALSE, sizeof(gint));
    gwy_container_foreach(data, NULL, hash_data_find_func, cidd.map);
    g_array_sort(cidd.map, compare_integers);

    /* When the data indices look like we want them, don't bother with
     * temporary containers. */
#ifdef DEBUG
    for (i = 0; i < cidd.map->len; i++)
        gwy_debug("Map: %d -> %d", i, g_array_index(cidd.map, gint, i));
#endif
    for (i = 0; i < cidd.map->len; i++) {
        if (g_array_index(cidd.map, gint, i) != i)
            break;
    }
    if (i == cidd.map->len) {
        g_array_free(cidd.map, TRUE);
        g_object_ref(data);
        return data;
    }

    cidd.len = g_array_index(cidd.map, gint, cidd.map->len - 1) + 1;
    cidd.rmap = g_new(gint, cidd.len);
    for (i = 0; i < cidd.len; i++)
        cidd.rmap[i] = -1;
    for (i = 0; i < cidd.map->len; i++)
        cidd.rmap[g_array_index(cidd.map, gint, i)] = i;
#ifdef DEBUG
    for (i = 0; i < cidd.len; i++)
        gwy_debug("Rmap: %d -> %d", i, cidd.rmap[i]);
#endif

    cidd.target = gwy_container_new();
    cidd.str = g_string_new("");

    gwy_container_foreach(data, NULL, hash_data_index_map_func, &cidd);

    g_free(cidd.rmap);
    g_string_free(cidd.str, TRUE);
    g_array_free(cidd.map, TRUE);

    return cidd.target;
}

/* Low-level deserialization functions for 1.x file import {{{ */
static inline void
gwy_byteswapped_copy(const guint8 *source,
                     guint8 *dest,
                     gsize size,
                     gsize len,
                     gsize byteswap)
{
    gsize i, k;

    if (!byteswap) {
        memcpy(dest, source, size*len);
        return;
    }

    for (i = 0; i < len; i++) {
        guint8 *b = dest + i*size;

        for (k = 0; k < size; k++)
            b[k ^ byteswap] = *(source++);
    }
}

static inline gboolean
gwy_serialize_unpack_boolean(const guchar *buffer,
                             gsize size,
                             gsize *position)
{
    gboolean value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(guchar) <= size, FALSE);
    value = !!buffer[*position];
    *position += sizeof(guchar);

    gwy_debug("value = <%s>", value ? "TRUE" : "FALSE");
    return value;
}

static inline guchar
gwy_serialize_unpack_char(const guchar *buffer,
                          gsize size,
                          gsize *position)
{
    guchar value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(guchar) <= size, '\0');
    value = buffer[*position];
    *position += sizeof(guchar);

    gwy_debug("value = <%c>", value);
    return value;
}

static inline gint32
gwy_serialize_unpack_int32(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gint32 value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(gint32) <= size, 0);
    memcpy(&value, buffer + *position, sizeof(gint32));
    value = GINT32_FROM_LE(value);
    *position += sizeof(gint32);

    gwy_debug("value = <%d>", value);
    return value;
}

static inline gint64
gwy_serialize_unpack_int64(const guchar *buffer,
                           gsize size,
                           gsize *position)
{
    gint64 value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(gint64) <= size, 0);
    memcpy(&value, buffer + *position, sizeof(gint64));
    value = GINT64_FROM_LE(value);
    *position += sizeof(gint64);

    gwy_debug("value = <%lld>", value);
    return value;
}

static inline gdouble
gwy_serialize_unpack_double(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    gdouble value;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position + sizeof(gdouble) <= size, 0.0);
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    memcpy(&value, buffer + *position, sizeof(gdouble));
#else
    gwy_byteswapped_copy(buffer + *position, (guint8*)&value,
                         sizeof(gdouble), 1, sizeof(gdouble) - 1);
#endif
    *position += sizeof(gdouble);

    gwy_debug("value = <%g>", value);
    return value;
}

static inline guchar*
gwy_serialize_unpack_string(const guchar *buffer,
                            gsize size,
                            gsize *position)
{
    guchar *value;
    const guchar *p;

    gwy_debug("buf = %p, size = %" G_GSIZE_FORMAT ", pos = %" G_GSIZE_FORMAT,
              buffer, size, *position);
    g_assert(buffer);
    g_assert(position);
    g_return_val_if_fail(*position < size, NULL);
    p = memchr(buffer + *position, 0, size - *position);
    g_return_val_if_fail(p, NULL);
    value = g_strdup(buffer + *position);
    *position += (p - buffer) - *position + 1;

    gwy_debug("value = <%s>", value);
    return value;
}

static GObject*
gwy_container_deserialize_old(const guchar *buffer,
                              gsize size,
                              gsize *position)
{
    GwyContainer *container;
    gsize mysize, pos;
    const guchar *buf;

    gwy_debug("");
    g_return_val_if_fail(buffer, NULL);

    pos = gwy_serialize_check_string(buffer, size, *position,
                                     g_type_name(GWY_TYPE_CONTAINER));
    g_return_val_if_fail(pos, NULL);
    *position += pos;
    mysize = gwy_serialize_unpack_int32(buffer, size, position);
    buf = buffer + *position;
    pos = 0;

    container = (GwyContainer*)gwy_container_new();
    container->in_construction = TRUE;
    while (pos < mysize) {
        GType type;
        guchar *name;
        GQuark key;
        GObject *object;

        type = gwy_serialize_unpack_int32(buf, mysize, &pos);
        name = gwy_serialize_unpack_string(buf, mysize, &pos);
            gwy_debug("deserializing %s => %s", name, g_type_name(type));
            key = g_quark_from_string(name);
        g_free(name);

        switch (type) {
            case G_TYPE_OBJECT:
            if ((object = gwy_serializable_deserialize(buf, mysize, &pos))) {
                gwy_container_set_object(container, key, object);
                g_object_unref(object);
            }
            break;

            case G_TYPE_BOOLEAN:
            gwy_container_set_boolean(container, key,
                                      gwy_serialize_unpack_boolean(buf, mysize,
                                                                   &pos));
            break;

            case G_TYPE_UCHAR:
            gwy_container_set_uchar(container, key,
                                    gwy_serialize_unpack_char(buf, mysize,
                                                              &pos));
            break;

            case G_TYPE_INT:
            gwy_container_set_int32(container, key,
                                    gwy_serialize_unpack_int32(buf, mysize,
                                                               &pos));
            break;

            case G_TYPE_INT64:
            gwy_container_set_int64(container, key,
                                    gwy_serialize_unpack_int64(buf, mysize,
                                                               &pos));
            break;

            case G_TYPE_DOUBLE:
            gwy_container_set_double(container, key,
                                     gwy_serialize_unpack_double(buf, mysize,
                                                                 &pos));
            break;

            case G_TYPE_STRING:
            gwy_container_set_string(container, key,
                                     gwy_serialize_unpack_string(buf, mysize,
                                                                 &pos));
            break;

            default:
            g_warning("Cannot unpack GValue holding type #%d", (gint)type);
            break;
        }
    }
    *position += mysize;
    container->in_construction = FALSE;

    return (GObject*)container;
}
/* }}} */

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
