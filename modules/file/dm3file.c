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

#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>
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
    DM3TagGroup *group;
};

static gboolean      module_register(void);
static gint          dm3_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* dm3_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gboolean      dm3_read_header(DM3File *dm3file,
                                     const guchar **p,
                                     gsize *size,
                                     GError **error);
static DM3TagGroup*  dm3_read_group (DM3TagEntry *parent,
                                     const guchar **p,
                                     gsize *size,
                                     GError **error);
static gboolean      dm3_read_entry (DM3TagEntry *parent,
                                     DM3TagEntry *entry,
                                     const guchar **p,
                                     gsize *size,
                                     GError **error);
static DM3TagType*   dm3_read_type  (DM3TagEntry *parent,
                                     const guchar **p,
                                     gsize *size,
                                     GError **error);
static guint         dm3_type_size  (DM3TagEntry *parent,
                                     const guint *types,
                                     guint *n,
                                     guint level,
                                     GError **error);
static void          dm3_free_group (DM3TagGroup *group);

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
    guchar *buffer = NULL;
    const guchar *p;
    gsize remaining, size = 0;
    GError *err = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    p = buffer;
    remaining = size;

    if (!dm3_read_header(&dm3file, &p, &remaining, error))
        goto fail;

    if (!(dm3file.group = dm3_read_group(NULL, &p, &remaining, error)))
        goto fail;


    err_NO_DATA(error);

fail:
    dm3_free_group(dm3file.group);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static gboolean
dm3_read_header(DM3File *dm3file,
                const guchar **p, gsize *size,
                GError **error)
{
    gwy_clear(dm3file, 1);

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
        if (!dm3_read_entry(parent, group->entries + i, p, size, error)) {
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

    entry->label = g_strndup(*p, lab_len);
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
    type->data = p;
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 */
