/*
 *  @(#) $Id$
 *  Copyright (C) 2003-2006 David Necas (Yeti), Petr Klapetek.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-gwyddion-spm">
 *   <comment>Gwyddion SPM data</comment>
 *   <magic priority="100">
 *     <match type="string" offset="0" value="GWYOGwyContainer"/>
 *     <match type="string" offset="0" value="GWYPGwyContainer"/>
 *   </magic>
 *   <glob pattern="*.gwy"/>
 *   <glob pattern="*.GWY"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Gwyddion native data
 * .gwy
 * Read Save SPS
 **/

#include "config.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libdraw/gwyselection.h>
#include <libgwymodule/gwymodule-file.h>

#include "err.h"

#define EXTENSION ".gwy"
#define MAGIC "GWYO"
#define MAGIC2 "GWYP"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

/* The container prefix all graph reside in.  This is a bit silly but it does
 * not worth to break file compatibility with 1.x. */
#define GRAPH_PREFIX "/0/graph/graph"

typedef struct {
    GArray *map;   /* data numbers in container, map plain position -> id */
    gint len;   /* length of reverse map @rmap */
    gint *rmap;   /* inversion of @map, defined only for the image of @map */
    GwyContainer *target;   /* container to remap keys to */
    GString *str;   /* scratch space */
} CompressIdData;

static gboolean      module_register         (void);
static gint          gwyfile_detect          (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* gwyfile_load            (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gboolean      gwyfile_save            (GwyContainer *data,
                                              const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static void          gwyfile_pack_metadata   (GwyContainer *data);
static void          gwyfile_remove_old_data (GObject *object);
static GObject*      gwy_container_deserialize_old (const guchar *buffer,
                                                    gsize size,
                                                    gsize *position);


static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Loads and saves Gwyddion native data files (serialized objects)."),
    "Yeti <yeti@gwyddion.net>",
    "0.15",
    "David Nečas (Yeti) & Petr Klapetek",
    "2003",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
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
        && (memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0
            || memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE) == 0))
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

    if (!memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        object = gwy_container_deserialize_old(buffer + MAGIC_SIZE,
                                               size - MAGIC_SIZE, &pos);
        gwyfile_remove_old_data(object);
    }
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
    gwyfile_pack_metadata(GWY_CONTAINER(object));

    return GWY_CONTAINER(object);
}

static gboolean
gwyfile_save(GwyContainer *data,
             const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GByteArray *buffer;
    gchar *filename_orig_utf8, *filename_utf8;
    FILE *fh;
    gboolean restore_filename, ok = TRUE;

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    /* Assure the saved file contains its own name under "/filename" */
    restore_filename = TRUE;
    filename_orig_utf8 = NULL;
    gwy_container_gis_string_by_name(data, "/filename",
                                     (const guchar**)&filename_orig_utf8);
    filename_orig_utf8 = g_strdup(filename_orig_utf8);

    filename_utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
    if (!filename_utf8)
        gwy_container_remove_by_name(data, "/filename");
    else if (filename_orig_utf8
             && gwy_strequal(filename_orig_utf8, filename_utf8)) {
        restore_filename = FALSE;
    }
    else {
        gwy_container_set_string_by_name(data, "/filename", filename_utf8);
        filename_utf8 = NULL;
    }

    /* Serialize */
    buffer = gwy_serializable_serialize(G_OBJECT(data), NULL);
    if (fwrite(MAGIC2, 1, MAGIC_SIZE, fh) != MAGIC_SIZE
        || fwrite(buffer->data, 1, buffer->len, fh) != buffer->len) {
        err_WRITE(error);
        ok = FALSE;
        g_unlink(filename);
    }
    fclose(fh);
    g_byte_array_free(buffer, TRUE);

    /* Restore filename if save failed */
    if (!ok && restore_filename) {
        if (filename_orig_utf8)
            gwy_container_set_string_by_name(data, "/filename",
                                             filename_orig_utf8);
        else
            gwy_container_remove_by_name(data, "/filename");
        filename_orig_utf8 = NULL;
    }
    g_free(filename_orig_utf8);
    g_free(filename_utf8);

    return ok;
}

/** Convert and/or remove various old-style data structures {{{ **/
static void
gwyfile_gather_one_meta(GQuark quark,
                        GValue *value,
                        GwyContainer *meta,
                        const gchar *prefix,
                        guint prefix_len)
{
    const gchar *strkey;

    strkey = g_quark_to_string(quark);
    g_return_if_fail(g_str_has_prefix(strkey, prefix));
    strkey += prefix_len;

    if (strkey[0] != '/' || !strkey[1] || !G_VALUE_HOLDS_STRING(value))
        return;

    gwy_container_set_string_by_name(meta, strkey+1, g_value_dup_string(value));
}

static void
gwyfile_gather_meta(gpointer key, gpointer value, gpointer user_data)
{
    gwyfile_gather_one_meta(GPOINTER_TO_UINT(key),
                            (GValue*)value,
                            (GwyContainer*)user_data,
                            "/meta", sizeof("/meta")-1);
}

static void
gwyfile_gather_0_data_meta(gpointer key, gpointer value, gpointer user_data)
{
    gwyfile_gather_one_meta(GPOINTER_TO_UINT(key),
                            (GValue*)value,
                            (GwyContainer*)user_data,
                            "/0/data/meta", sizeof("/0/data/meta")-1);
}

/**
 * gwyfile_pack_metadata:
 * @data: A data container.
 *
 * Pack scattered metadata under "/meta" to a container at "/0/meta" and
 * metadata scattered under "/0/data/meta" to a container at "/0/meta".
 **/
static void
gwyfile_pack_metadata(GwyContainer *data)
{
    GwyContainer *meta;

    meta = gwy_container_new();

    /* Mindlessly run one packing after another.  Losing some metadata is not
     * a tragedy and the file has to be borken in the first place to have
     * metadata conflicts. */
    gwy_container_foreach(data, "/meta", &gwyfile_gather_meta, meta);
    gwy_container_foreach(data, "/0/data/meta", &gwyfile_gather_0_data_meta,
                          meta);
    if (gwy_container_get_n_items(meta)) {
        gwy_container_remove_by_prefix(data, "/meta");
        gwy_container_remove_by_prefix(data, "/0/data/meta");
        gwy_container_set_object_by_name(data, "/0/meta", meta);
    }

    g_object_unref(meta);
}

static GwySelection*
gwyfile_gather_old_rect_selection(GwyContainer *data)
{
    GwySelection *sel;
    GType type;
    gboolean selected;
    gdouble xy[4];

    type = g_type_from_name("GwySelectionRectangle");
    if (!type
        || !gwy_container_gis_boolean_by_name(data, "/0/select/rect/selected",
                                              &selected)
        || !selected)
        return NULL;

    if (!gwy_container_gis_double_by_name(data, "/0/select/rect/x0", &xy[0])
        || !gwy_container_gis_double_by_name(data, "/0/select/rect/y0", &xy[1])
        || !gwy_container_gis_double_by_name(data, "/0/select/rect/x1", &xy[2])
        || !gwy_container_gis_double_by_name(data, "/0/select/rect/y1", &xy[3]))
        return NULL;

    sel = GWY_SELECTION(g_object_new(type, "max-objects", 1, NULL));
    gwy_selection_set_object(sel, 0, xy);

    return sel;
}

static GwySelection*
gwyfile_gather_old_point_selection(GwyContainer *data)
{
    GwySelection *sel;
    GType type;
    gint i, nselected;
    gdouble xy[2];
    gchar key[40];

    type = g_type_from_name("GwySelectionPoint");
    if (!type
        || !gwy_container_gis_int32_by_name(data,
                                            "/0/select/points/nselected",
                                            &nselected))
        return NULL;

    nselected = CLAMP(nselected, 0, 16);
    if (!nselected)
        return NULL;

    sel = GWY_SELECTION(g_object_new(type, "max-objects", nselected, NULL));
    for (i = 0; i < nselected; i++) {
        g_snprintf(key, sizeof(key), "/0/select/points/%d/x", i);
        if (!gwy_container_gis_double_by_name(data, key, &xy[0]))
            break;
        g_snprintf(key, sizeof(key), "/0/select/points/%d/y", i);
        if (!gwy_container_gis_double_by_name(data, key, &xy[1]))
            break;

        gwy_selection_set_object(sel, i, xy);
    }

    if (!i)
        gwy_object_unref(sel);

    return sel;
}

static GwySelection*
gwyfile_gather_old_line_selection(GwyContainer *data)
{
    GwySelection *sel;
    GType type;
    gint i, nselected;
    gdouble xy[4];
    gchar key[40];

    type = g_type_from_name("GwySelectionLine");
    if (!type
        || !gwy_container_gis_int32_by_name(data,
                                            "/0/select/lines/nselected",
                                            &nselected))
        return NULL;

    nselected = CLAMP(nselected, 0, 16);
    if (!nselected)
        return NULL;

    sel = GWY_SELECTION(g_object_new(type, "max-objects", nselected, NULL));
    for (i = 0; i < nselected; i++) {
        g_snprintf(key, sizeof(key), "/0/select/lines/%d/x0", i);
        if (!gwy_container_gis_double_by_name(data, key, &xy[0]))
            break;
        g_snprintf(key, sizeof(key), "/0/select/lines/%d/y0", i);
        if (!gwy_container_gis_double_by_name(data, key, &xy[1]))
            break;
        g_snprintf(key, sizeof(key), "/0/select/lines/%d/x1", i);
        if (!gwy_container_gis_double_by_name(data, key, &xy[2]))
            break;
        g_snprintf(key, sizeof(key), "/0/select/lines/%d/y1", i);
        if (!gwy_container_gis_double_by_name(data, key, &xy[3]))
            break;

        gwy_selection_set_object(sel, i, xy);
    }

    if (!i)
        gwy_object_unref(sel);

    return sel;
}

static void
gwyfile_remove_old_data(GObject *object)
{
    GwyContainer *data;
    GwySelection *rect, *point, *line;

    if (!object || !GWY_IS_CONTAINER(object))
        return;

    data = GWY_CONTAINER(object);

    /* Selections */
    rect = gwyfile_gather_old_rect_selection(data);
    point = gwyfile_gather_old_point_selection(data);
    line = gwyfile_gather_old_line_selection(data);
    gwy_container_remove_by_prefix(data, "/0/select");
    if (rect) {
        gwy_container_set_object_by_name(data, "/0/select/rectangle", rect);
        g_object_unref(rect);
    }
    if (point) {
        gwy_container_set_object_by_name(data, "/0/select/point", point);
        g_object_unref(point);
    }
    if (line) {
        gwy_container_set_object_by_name(data, "/0/select/line", line);
        g_object_unref(line);
    }

    /* 3D */
    gwy_container_remove_by_prefix(data, "/0/3d/labels");
    gwy_container_remove_by_name(data, "/0/3d/rot_x");
    gwy_container_remove_by_name(data, "/0/3d/rot_y");
    gwy_container_remove_by_name(data, "/0/3d/view_scale");
    gwy_container_remove_by_name(data, "/0/3d/deformation_z");
    gwy_container_remove_by_name(data, "/0/3d/light_z");
    gwy_container_remove_by_name(data, "/0/3d/light_y");
}
/* }}} */

/** Low-level deserialization functions for 1.x file import {{{ **/
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
    gwy_memcpy_byte_swap(buffer + *position, (guint8*)&value,
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
