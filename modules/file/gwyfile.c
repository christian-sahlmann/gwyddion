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

#include <string.h>

#include <glib/gstdio.h>

#include <libgwyddion/gwymacros.h>

#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#define EXTENSION ".gwy"
#define MAGIC "GWYO"
#define MAGIC2 "GWYP"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

static gboolean      module_register         (const gchar *name);
static gint          gwyfile_detect          (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* gwyfile_load            (const gchar *filename);
static gboolean      gwyfile_save            (GwyContainer *data,
                                              const gchar *filename);
static GObject*      gwy_container_deserialize_old (const guchar *buffer,
                                                    gsize size,
                                                    gsize *position);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Loads and saves Gwyddion native data files (serialized objects)."),
    "Yeti <yeti@gwyddion.net>",
    "0.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo gwyfile_func_info = {
        "gwyfile",
        N_("Gwyddion native format (.gwy)"),
        (GwyFileDetectFunc)&gwyfile_detect,
        (GwyFileLoadFunc)&gwyfile_load,
        (GwyFileSaveFunc)&gwyfile_save,
    };

    gwy_file_func_register(name, &gwyfile_func_info);

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
gwyfile_load(const gchar *filename)
{
    GObject *object, *dfield;
    GError *err = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    gsize pos = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < MAGIC_SIZE
        || (memcmp(buffer, MAGIC, MAGIC_SIZE)
            && memcmp(buffer, MAGIC2, MAGIC_SIZE))) {
        g_warning("File %s doesn't seem to be a .gwy file", filename);
        if (!gwy_file_abandon_contents(buffer, size, &err)) {
            g_critical("%s", err->message);
            g_clear_error(&err);
        }
        return NULL;
    }

    if (!memcmp(buffer, MAGIC, MAGIC_SIZE))
        object = gwy_container_deserialize_old(buffer + MAGIC_SIZE,
                                               size - MAGIC_SIZE, &pos);
    else
        object = gwy_serializable_deserialize(buffer + MAGIC_SIZE,
                                              size - MAGIC_SIZE, &pos);

    if (!gwy_file_abandon_contents(buffer, size, &err)) {
        g_critical("%s", err->message);
        g_clear_error(&err);
    }
    if (!object) {
        g_warning("File %s deserialization failed", filename);
        return NULL;
    }
    if (!GWY_IS_CONTAINER(object)) {
        g_warning("File %s contains some strange object", filename);
        g_object_unref(object);
        return NULL;
    }
    dfield = gwy_container_get_object_by_name(GWY_CONTAINER(object), "/0/data");
    if (!dfield || !GWY_IS_DATA_FIELD(dfield)) {
        g_warning("File %s contains no data field", filename);
        g_object_unref(object);
        return NULL;
    }

    return (GwyContainer*)object;
}

static gboolean
gwyfile_save(GwyContainer *data,
             const gchar *filename)
{
    GByteArray *buffer;
    FILE *fh;
    gboolean ok = TRUE;

    if (!(fh = g_fopen(filename, "wb")))
        return FALSE;
    buffer = gwy_serializable_serialize(G_OBJECT(data), NULL);
    if (fwrite(MAGIC2, 1, MAGIC_SIZE, fh) != MAGIC_SIZE
        || fwrite(buffer->data, 1, buffer->len, fh) != buffer->len) {
        ok = FALSE;
        g_unlink(filename);
    }
    fclose(fh);
    g_byte_array_free(buffer, TRUE);

    return ok;
}

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
                         sizeof(gdouble), 1, (1 << sizeof(gdouble)) - 1);
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

static inline gsize
gwy_serialize_check_string(const guchar *buffer,
                           gsize size,
                           gsize position,
                           const guchar *compare_to)
{
    const guchar *p;

    gwy_debug("<%s> buf = %p, size = %" G_GSIZE_FORMAT ", pos = %"
              G_GSIZE_FORMAT,
              compare_to ? compare_to : (const guchar*)"(null)",
              buffer, size, position);
    g_assert(buffer);
    g_assert(size > 0);
    g_return_val_if_fail(position < size, 0);
    p = (guchar*)memchr(buffer + position, 0, size - position);
    if (!p || (compare_to && strcmp(buffer + position, compare_to)))
        return 0;

    return (p - buffer) + 1 - position;
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
