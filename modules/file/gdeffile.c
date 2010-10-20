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

typedef enum {
    VAR_INTEGER   = 0,
    VAR_FLOAT     = 1,
    VAR_DOUBLE    = 2,
    VAR_WORD      = 3,
    VAR_DWORD     = 4,
    VAR_CHAR      = 5,
    VAR_STRING    = 6,
    VAR_DATABLOCK = 7,
    VAR_LAST = VAR_DATABLOCK
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
    gchar mark[2];
    /* alignment 2 bytes here */
    guint n_variables;
    guint n_data;
    gboolean has_next_block;
    /* alignment 3 bytes here */
} GDEFControlBlock;

typedef struct {
    gchar name[VAR_NAME_SIZE];
    GDEFVariableType type;
    /* Internal, calculated. */
    gpointer data;
} GDEFVariable;

typedef struct {
    GDEFControlBlock block;
    GDEFVariable *vars;
} GDEFVariableTable;

static gboolean      module_register(void);
static gint          gdef_detect    (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* gdef_load      (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

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
    gwy_clear(block, 1);

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
    block->has_next_block = !!*((*p)++);
    gwy_debug("n_variables: %u, n_data: %u", block->n_variables, block->n_data);
    gwy_debug_bool(block, has_next_block);
    *p += 3;

    return TRUE;
}

static gboolean
gdef_read_variable(GDEFVariable *variable,
                   const guchar **p,
                   gsize size,
                   GError **error)
{
    gwy_clear(variable, 1);

    if (size < VARIABLE_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Variable record is truncated."));
        return FALSE;
    }

    get_CHARARRAY(variable->name, p);
    gwy_debug_chars(variable, name);
    variable->type = gwy_get_guint32_le(p);
    gwy_debug("type: %u", variable->type);
    if (variable->type > VAR_LAST) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unknown variable type %u."), variable->type);
        return FALSE;
    }

    return TRUE;
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
    GDEFControlBlock block;
    GList *variable_tables = NULL;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer;
    if (!gdef_read_header(&header, &p, size, error))
        goto fail;

    while (TRUE) {
        GDEFVariableTable *table = NULL;

        if (!gdef_read_control_block(&block, &p, size - (p - buffer), error))
            goto fail;

        table = g_new0(GDEFVariableTable, 1);
        for (i = 0; i < block.n_variables; i++) {
            GDEFVariable var;

            if (!gdef_read_variable(&var, &p, size - (p - buffer), error))
                goto fail;
        }
        for (i = 0; i < block.n_variables; i++) {
        }

        if (!block.has_next_block)
            break;

    }

    err_NO_DATA(error);

fail:
    g_free(header.description);
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
