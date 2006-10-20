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
#include <libdraw/gwyselection.h>
#include <libgwymodule/gwymodule-file.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "err.h"

#define EXTENSION ".gwy"
#define MAGIC "GWYO"
#define MAGIC2 "GWYP"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

/* The container prefix all graph reside in.  This is a bit silly but it does
 * not worth to break file compatibility with 1.x. */
#define GRAPH_PREFIX "/0/graph/graph"

/* When highest data id is larger than this, disable repacking to avoid OOM
 * problems.  Either data is corrupted or someone is making fun of us. */
enum { SANITY_LIMIT = 0x1000000 };

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
static GwyContainer* gwyfile_compress_ids    (GwyContainer *data);
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
    "0.14",
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
    GwyContainer *data;
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
    data = gwyfile_compress_ids(GWY_CONTAINER(object));
    g_object_unref(object);
    gwyfile_pack_metadata(data);

    return data;
}

static gboolean
gwyfile_save(GwyContainer *data,
             const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *compressed;
    GByteArray *buffer;
    gchar *filename_orig_utf8, *filename_utf8;
    FILE *fh;
    gboolean restore_filename = FALSE, ok = TRUE;

    if (!(fh = g_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    compressed = gwyfile_compress_ids(data);

    /* Assure the saved file contains its own name under "/filename" */
    filename_orig_utf8 = NULL;
    if (compressed == data) {
        gwy_container_gis_string_by_name(data, "/filename",
                                         (const guchar**)&filename_orig_utf8);
        filename_orig_utf8 = g_strdup(filename_orig_utf8);
        restore_filename = TRUE;
    }

    filename_utf8 = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
    if (!filename_utf8)
        gwy_container_remove_by_name(compressed, "/filename");
    else if (filename_orig_utf8
             && gwy_strequal(filename_orig_utf8, filename_utf8)) {
        restore_filename = FALSE;
    }
    else {
        gwy_container_set_string_by_name(compressed, "/filename",
                                         filename_utf8);
        filename_utf8 = NULL;
    }

    /* Serialize */
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

/** Data id compression {{{ **/
/**
 * key_get_int_prefix:
 * @strkey: String container key.
 * @len: Location to store integer prefix length if the string has an integer
 *       prefix.
 *
 * Checks whether a container key starts with "/[0-9]+/" and gets the prefix
 * number.
 *
 * Returns: The prefix number, or -1 if the key does not match.
 **/
static gint
key_get_int_prefix(const gchar *strkey,
                   guint *len)
{
    guint i;

    if (strkey[0] != GWY_CONTAINER_PATHSEP)
        return -1;
    /* Do not use strtol, it allows queer stuff like spaces */
    for (i = 0; g_ascii_isdigit(strkey[i + 1]); i++)
        ;
    if (!i || strkey[i + 1] != GWY_CONTAINER_PATHSEP)
        return -1;

    *len = i + 2;
    return atoi(strkey + 1);
}

/**
 * hash_data_find_func:
 * @key: Container key (quark).
 * @value: Value at @key.  When it matches, it must be an object.
 * @user_data: #GArray of gint data indices.
 *
 * Checks whether hash key matches "/[0-9]+/data".
 *
 * If it matches, the data number is added to the @user_data array.
 **/
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

/* Remap /N/data, /N/mask, /N/show, /N/select, /N/meta
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
         || g_str_has_prefix(strkey + len, "show")
         || g_str_has_prefix(strkey + len, "meta"))
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

/**
 * hash_graph_find_func:
 * @key: Container key (quark).
 * @value: Value at @key.  When it matches, it must be an object.
 * @user_data: #GArray of gint data indices.
 *
 * Checks whether hash key matches "/0/graph/graph/[0-9]+.
 *
 * If it matches, the data number is added to the @user_data array.
 **/
static void
hash_graph_find_func(gpointer key,
                     gpointer value,
                     gpointer user_data)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GValue *gvalue = (GValue*)value;
    GArray *array = (GArray*)user_data;
    const gchar *strkey;
    gint i;

    strkey = g_quark_to_string(quark);
    if (!g_str_has_prefix(strkey, GRAPH_PREFIX GWY_CONTAINER_PATHSEP_STR))
        return;

    strkey += sizeof(GRAPH_PREFIX);
    /* Do not use strtol, it allows queer stuff like spaces */
    for (i = 0; g_ascii_isdigit(strkey[i]); i++)
        ;
    if (!i || strkey[i])
        return;
    i = atoi(strkey);

    g_return_if_fail(G_VALUE_HOLDS_OBJECT(gvalue));
    g_array_append_val(array, i);
}

/* Remap /0/graph/graph/N
 * trees elsewhere. copying other values to the same keys */
static void
hash_graph_index_map_func(gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    GQuark quark = GPOINTER_TO_UINT(key);
    GValue *copy, *gvalue = (GValue*)value;
    CompressIdData *cidd = (CompressIdData*)user_data;
    const gchar *strkey;
    gint i, j = 0;

    strkey = g_quark_to_string(quark);
    if (!g_str_has_prefix(strkey, GRAPH_PREFIX GWY_CONTAINER_PATHSEP_STR))
        i = -1;
    else {
        /* Do not use strtol, it allows queer stuff like spaces */
        for (j = 0; g_ascii_isdigit(strkey[j + sizeof(GRAPH_PREFIX)]); j++)
            ;
        if (j && (!strkey[j + sizeof(GRAPH_PREFIX)]
                  || strkey[j + sizeof(GRAPH_PREFIX)] == GWY_CONTAINER_PATHSEP))
            i = atoi(strkey + sizeof(GRAPH_PREFIX));
        else
            i = -1;
    }
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

    g_assert(j);
    g_string_printf(cidd->str, "%s/%d%s",
                    GRAPH_PREFIX, cidd->rmap[i],
                    strkey + sizeof(GRAPH_PREFIX) + j);
    gwy_debug("<%s> -> <%s> (REMAP)", strkey, cidd->str->str);
    gwy_container_set_value_by_name(cidd->target, cidd->str->str, copy,
                                    NULL);
}

static gint
compare_integers(gconstpointer a,
                 gconstpointer b)
{
    gint ia = *(const gint*)a;
    gint ib = *(const gint*)b;

    return ia - ib;
}

static gboolean
gwyfile_make_rmap(CompressIdData *cidd,
                  gint base,
                  const gchar *name)
{
    guint i;

    g_array_sort(cidd->map, compare_integers);

#ifdef DEBUG
    for (i = 0; i < cidd->map->len; i++)
        gwy_debug("%s map: %d -> %d",
                  name, i, g_array_index(cidd->map, gint, i));
#endif
    /* When the data indices look like we want them, don't bother with
     * temporary containers. */
    for (i = 0; i < cidd->map->len; i++) {
        if (g_array_index(cidd->map, gint, i) != i + base)
            break;
    }
    if (i == cidd->map->len)
        return FALSE;

    /* Construct the reverse map */
    cidd->len = g_array_index(cidd->map, gint, cidd->map->len - 1) + 1 + base;
    if (cidd->len > SANITY_LIMIT) {
        g_warning("Last %s id %u is larger than %u. "
                  "Container is probably corrupted, disabling id compression.",
                  name, cidd->len, SANITY_LIMIT);
        return FALSE;
    }
    cidd->rmap = g_new(gint, cidd->len);
    for (i = 0; i < cidd->len; i++)
        cidd->rmap[i] = -1;
    for (i = 0; i < cidd->map->len; i++)
        cidd->rmap[g_array_index(cidd->map, gint, i)] = i + base;
#ifdef DEBUG
    for (i = 0; i < cidd->len; i++)
        gwy_debug("%s rmap: %d -> %d", name, i, cidd->rmap[i]);
#endif

    return TRUE;
}

/**
 * gwyfile_compress_data_ids:
 * @data: A data container.
 *
 * Creates a container with compressed data numbers.
 *
 * Returns: A container where data numbers form simple sequence from 0 onward.
 *          It may be @data itself too, in such case a reference is added so
 *          that caller should always use g_object_unref() on the returned
 *          container to release it.
 **/
static GwyContainer*
gwyfile_compress_data_ids(GwyContainer *data)
{
    CompressIdData cidd;

    cidd.map = g_array_new(FALSE, FALSE, sizeof(gint));
    gwy_container_foreach(data, NULL, hash_data_find_func, cidd.map);

    if (gwyfile_make_rmap(&cidd, 0, "data")) {
        cidd.target = gwy_container_new();
        cidd.str = g_string_new("");
        gwy_container_foreach(data, NULL, hash_data_index_map_func, &cidd);
        g_free(cidd.rmap);
        g_string_free(cidd.str, TRUE);
    }
    else
        cidd.target = g_object_ref(data);
    g_array_free(cidd.map, TRUE);

    return cidd.target;
}

/**
 * gwyfile_compress_graph_ids:
 * @data: A data container.
 *
 * Creates a container with compressed graph numbers.
 *
 * Returns: A container where graph numbers form simple sequence from 0 onward.
 *          It may be @data itself too, in such case a reference is added so
 *          that caller should always use g_object_unref() on the returned
 *          container to release it.
 **/
static GwyContainer*
gwyfile_compress_graph_ids(GwyContainer *data)
{
    CompressIdData cidd;

    cidd.map = g_array_new(FALSE, FALSE, sizeof(gint));
    gwy_container_foreach(data, GRAPH_PREFIX, hash_graph_find_func, cidd.map);

    /* For historic reasons graphs start from 1.  Graph with id=0 makes
     * 1.x crash. */
    if (gwyfile_make_rmap(&cidd, 1, "data")) {
        cidd.target = gwy_container_new();
        cidd.str = g_string_new("");
        gwy_container_foreach(data, NULL, hash_graph_index_map_func, &cidd);
        g_free(cidd.rmap);
        g_string_free(cidd.str, TRUE);
    }
    else
        cidd.target = g_object_ref(data);
    /* Keep 1.x compatibility */
    gwy_container_remove_by_name(cidd.target, "/0/graph/lastid");
    gwy_container_set_int32_by_name(cidd.target, "/0/graph/lastid",
                                    cidd.map->len);

    g_array_free(cidd.map, TRUE);

    return cidd.target;
}

static GwyContainer*
gwyfile_compress_ids(GwyContainer *data)
{
    GwyContainer *tmp, *result;

    tmp = gwyfile_compress_data_ids(data);
    result = gwyfile_compress_graph_ids(tmp);
    g_object_unref(tmp);

    return result;
}
/* }}} */

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
