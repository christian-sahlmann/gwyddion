/*
 *  @(#) $Id$
 *  Copyright (C) 2010 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-gdef-spm">
 *   <comment>DME GDEF SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="GDEF\x00"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * DME GDEF
 * .gdf
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <stdio.h>
#include <time.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define MAGIC "GDEF"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define gwy_debug_chars(s, f) gwy_debug(#f ": %.*s", (guint)sizeof(s->f), s->f)
#define gwy_debug_bool(s, f) gwy_debug(#f ": %s", (s->f) ? "TRUE" : "FALSE");

enum {
    HEADER_SIZE = 4 + 2 + 2/*align*/ + 4 + 4,
    CONTROL_BLOCK_SIZE = 2 + 2/*align*/ + 4 + 4 + 1 + 3/*align*/,
    VAR_NAME_SIZE = 50,
    VARIABLE_SIZE = VAR_NAME_SIZE + 4,
};

/* XXX: String type seems unimplemented in the original as well. */
typedef enum {
    VAR_INTEGER   = 0,
    VAR_FLOAT     = 1,
    VAR_DOUBLE    = 2,
    VAR_WORD      = 3,
    VAR_DWORD     = 4,
    VAR_CHAR      = 5,
    VAR_STRING    = 6,
    VAR_DATABLOCK = 7,
    VAR_NVARS,
} GDEFVariableType;

typedef struct {
    gchar magic[4];
    guint version;
    /* alignment 2 bytes here */
    time_t creation_time;
    guint desc_len;
    gchar *description;
} GDEFHeader;

typedef struct {
    /* includes alignment */
    gchar name[VAR_NAME_SIZE+2];
    GDEFVariableType type;
    /* Internal, calculated. */
    gsize size;
    gconstpointer data;
    /*
    union {
        guint u;
        gint i;
        gdouble d;
        guchar c;
    } value;
    */
} GDEFVariable;

typedef struct _GDEFControlBlock GDEFControlBlock;

struct _GDEFControlBlock {
    gchar mark[2];
    /* alignment 2 bytes here */
    guint n_variables;
    guint n_data;
    /* alignment 3 bytes here */

    /* we also put the variable data directly here */
    GDEFVariable *variables;
    GDEFControlBlock *next;
};

static gboolean      module_register(void);
static gint          gdef_detect    (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* gdef_load      (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static const gsize type_sizes[VAR_NVARS] = {
    4, 4, 8, 2, 4, 1, 0, 0
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports DME GDEF data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("gdeffile",
                           N_("DME GDEF files (.gdf)"),
                           (GwyFileDetectFunc)&gdef_detect,
                           (GwyFileLoadFunc)&gdef_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
gdef_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;
    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size > HEADER_SIZE
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 90;

    return score;
}

static gboolean
gdef_read_header(GDEFHeader *header,
                 const guchar **p,
                 gsize size,
                 GError **error)
{
    gwy_clear(header, 1);

    if (size < HEADER_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated."));
        return FALSE;
    }
    get_CHARARRAY(header->magic, p);
    if (memcmp(header->magic, MAGIC, 4) != 0) {
        err_FILE_TYPE(error, "GDEF");
        return FALSE;
    }
    header->version = gwy_get_guint16_le(p);
    gwy_debug("version: %u.%u", header->version/0x100, header->version % 0x100);
    if (header->version != 0x0200) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File version %u.%u is not supported."),
                    header->version/0x100, header->version % 0x100);
        return FALSE;
    }
    *p += 2;
    header->creation_time = gwy_get_guint32_le(p);
    gwy_debug("creation_time: %.24s", ctime(&header->creation_time));
    header->desc_len = gwy_get_guint32_le(p);
    gwy_debug("desc_len: %u", header->desc_len);

    if (size < HEADER_SIZE + header->desc_len) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated."));
        return FALSE;
    }

    header->description = g_strndup(*p, header->desc_len);
    *p += header->desc_len;
    gwy_debug("desc: %s", header->description);

    return TRUE;
}

static gboolean
gdef_read_control_block(GDEFControlBlock *block,
                        const guchar **p,
                        gsize size,
                        GError **error)
{
    if (size < CONTROL_BLOCK_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Control block is truncated."));
        return FALSE;
    }

    get_CHARARRAY(block->mark, p);
    if (block->mark[0] != 'C' || block->mark[1] != 'B') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Control block mark is not CB, file is damaged."));
        return FALSE;
    }

    *p += 2;
    block->n_variables = gwy_get_guint32_le(p);
    block->n_data = gwy_get_guint32_le(p);
    block->next = GUINT_TO_POINTER(!!*((*p)++));
    gwy_debug("n_variables: %u, n_data: %u, next: %u", block->n_variables, block->n_data, !!block->next);
    *p += 3;

    return TRUE;
}

static gboolean
gdef_read_variable(GDEFVariable *variable,
                   const guchar **p,
                   gsize size,
                   GError **error)
{
    if (size < VARIABLE_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Variable record is truncated."));
        return FALSE;
    }

    get_CHARARRAY(variable->name, p);
    gwy_debug_chars(variable, name);
    variable->type = gwy_get_guint32_le(p);
    gwy_debug("type: %u", variable->type);
    if (variable->type >= VAR_NVARS || variable->type == VAR_STRING) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unknown variable type %u."), variable->type);
        return FALSE;
    }

    return TRUE;
}

static GDEFControlBlock*
gdef_read_variable_lists(const guchar **p,
                         gsize size,
                         guint depth,
                         GError **error)
{
    GDEFControlBlock *block, *first_block = NULL, *last_block = NULL;
    const guchar *buffer = *p;
    guint i;

    do {
        block = g_new0(GDEFControlBlock, 1);
        if (!first_block) {
            first_block = last_block = block;
        }
        else {
            last_block->next = block;
            last_block = block;
        }

        if (!gdef_read_control_block(block, p, size - (*p - buffer), error))
            goto fail;

        block->variables = g_new0(GDEFVariable, block->n_variables);
        for (i = 0; i < block->n_variables; i++) {
            GDEFVariable *var = block->variables + i;

            if (!gdef_read_variable(block->variables + i,
                                    p, size - (*p - buffer), error))
                goto fail;

            if (var->type == VAR_DATABLOCK) {
                gwy_debug("ENTER nested datablock (%u)", depth+1);
                var->data = gdef_read_variable_lists(p, size - (*p - buffer),
                                                     depth + 1,
                                                     error);
                gwy_debug("EXIT nested datablock (%u)", depth+1);
            }
        }
        /* FIXME: Apparently, unlike in .gwy, the data are always stored at the
         * top level, i.e. the data of all nested blocks follow the
         * corresponding top-level block. */
        if (depth > 0)
            continue;

        for (i = 0; i < block->n_variables; i++) {
            GDEFVariable *var = block->variables + i;

            var->data = *p;
            var->size = type_sizes[var->type] * block->n_data;
            *p += var->size;

#if 0
            if (var->type == VAR_INTEGER)
                var->value.i = gwy_get_gint32_le(p);
            else if (var->type == VAR_FLOAT)
                var->value.d = gwy_get_gfloat_le(p);
            else if (var->type == VAR_DOUBLE)
                var->value.d = gwy_get_gdouble_le(p);
            else if (var->type == VAR_WORD)
                var->value.u = gwy_get_guint16_le(p);
            else if (var->type == VAR_DWORD)
                var->value.u = gwy_get_guint32_le(p);
            else if (var->type == VAR_CHAR)
                var->value.c = *((*p)++);
            else if (var->type == VAR_DATABLOCK) {
                /* g_printerr("Cannot handle DATABLOCK yet!\n"); */
            }
            else {
                g_assert_not_reached();
            }

            if (var->type == VAR_INTEGER || var->type == VAR_WORD
                || var->type == VAR_DWORD) {
                gwy_debug("%.*s = %u", VAR_NAME_SIZE, var->name, var->value.u);
            }
            else if (var->type == VAR_FLOAT || var->type == VAR_DOUBLE) {
                gwy_debug("%.*s = %g", VAR_NAME_SIZE, var->name, var->value.d);
            }
            else if (var->type == VAR_CHAR) {
                gwy_debug("%.*s = %c", VAR_NAME_SIZE, var->name, var->value.c);
            }
#endif
        }
    } while (block->next);

    return first_block;

fail:
    /* TODO: free the blocks */
    return NULL;
}

static GwyContainer*
gdef_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    GDEFHeader header;
    GDEFControlBlock *first_block = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer;
    gwy_debug("buffer: %p", buffer);
    if (!gdef_read_header(&header, &p, size, error))
        goto fail;

    first_block = gdef_read_variable_lists(&p, size - (p - buffer), 0, error);

    err_NO_DATA(error);

fail:
    g_free(header.description);
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
