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
/* TODO: metadata */
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

#include "config.h"
#include <stdio.h>
#include <time.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"
#include "get.h"

#define MAGIC "GDEF"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

enum {
    HEADER_SIZE = 4 + 2 + 2/*align*/ + 4 + 4,
    CONTROL_BLOCK_SIZE = 2 + 2/*align*/ + 4 + 4 + 1 + 3/*align*/,
    VAR_NAME_SIZE = 50,
    VARIABLE_SIZE = VAR_NAME_SIZE + 4,
};

/* XXX: String type seems unimplemented in the original as well. */
typedef enum {
    /* our markers, not actually present in GDEF */
    VAR_REAL        = -3,
    VAR_INTEGRAL    = -2,
    VAR_UNSPECIFIED = -1,
    /* GDEF types */
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
    /* includes 2 bytes of alignment which we use for nul-termination */
    gchar name[VAR_NAME_SIZE+2];
    GDEFVariableType type;
    /* Internal, calculated. */
    gsize size;
    gconstpointer data;  /* for nested blocks, it points to GDEFControlBlock! */
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
    /* creatively use the padding to terminate the string */
    variable->name[VAR_NAME_SIZE] = '\0';
    variable->type = gwy_get_guint32_le(p);
    gwy_debug("name: %s, type: %u", variable->name, variable->type);
    if (variable->type >= VAR_NVARS || variable->type == VAR_STRING) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unknown variable type %u."), variable->type);
        return FALSE;
    }

    return TRUE;
}

static gboolean
gdef_read_variable_data(GDEFControlBlock *block,
                        const guchar **p,
                        gsize size,
                        guint depth,
                        GError **error)
{
    const guchar *buffer = *p;
    guint i, ib = 0;

    do {
        gwy_debug("block #%u", ib);
        for (i = 0; i < block->n_variables; i++) {
            GDEFVariable *var = block->variables + i;

            if (var->type == VAR_DATABLOCK) {
                GDEFControlBlock *nestedblock = (GDEFControlBlock*)var->data;
                const guchar *q = *p;
                gboolean ok;

                gwy_debug("ENTER nested data (%u)", depth+1);
                ok = gdef_read_variable_data(nestedblock,
                                             p, size - (*p - buffer),
                                             depth+1, error);
                gwy_debug("EXIT nested data (%u)", depth+1);
                if (!ok)
                    return FALSE;

                var->size = q - *p;
            }
            else {
                var->data = *p;
                var->size = type_sizes[var->type] * block->n_data;
                gwy_debug("%s has size %zu", var->name, var->size);
                *p += var->size;
                if (*p > buffer + size) {
                    g_set_error(error, GWY_MODULE_FILE_ERROR,
                                GWY_MODULE_FILE_ERROR_DATA,
                                _("Data of variable %s is truncated."),
                                var->name);
                    return FALSE;
                }
            }
        }
        ib++;
        /* recurse only in the nested levels */
    } while ((block = block->next) && depth);

    return TRUE;
}

static void
gdef_free_control_block_list(GDEFControlBlock *block)
{
    guint i;

    while (block) {
        GDEFControlBlock *next = block->next;

        for (i = 0; i < block->n_variables; i++) {
            GDEFVariable *var = block->variables + i;

            if (var->type == VAR_DATABLOCK)
                gdef_free_control_block_list((GDEFControlBlock*)var->data);
        }
        g_free(block->variables);
        g_free(block);
        block = next;
    }
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
                gwy_debug("ENTER nested variables (%u)", depth+1);
                var->data = gdef_read_variable_lists(p, size - (*p - buffer),
                                                     depth + 1,
                                                     error);
                gwy_debug("EXIT nested variables (%u)", depth+1);
                if (!var->data)
                    goto fail;
            }
        }
        /* Unlike in .gwy, the data are always stored at the top level, i.e.
         * the data of all nested blocks follow the corresponding top-level
         * block.  So we have to postpone data reading until we get to the
         * top level again. */
        if (depth == 0) {
            if (!gdef_read_variable_data(block, p, size - (*p - buffer), depth,
                                         error))
                goto fail;
        }
    } while (block->next);

    return first_block;

fail:
    block->next = NULL;
    gdef_free_control_block_list(first_block);
    return NULL;
}

static GDEFVariable*
gdef_find_variable(GDEFControlBlock *block,
                   const gchar *name,
                   GDEFVariableType type,
                   gboolean iterate)
{
    guint i;

    while (block) {
        for (i = 0; i < block->n_variables; i++) {
            GDEFVariable *var = block->variables + i;

            if (gwy_strequal(var->name, name)) {
                if (type == VAR_UNSPECIFIED)
                    return var;
                if (type == VAR_INTEGRAL && (var->type == VAR_WORD
                                             || var->type == VAR_INTEGER
                                             || var->type == VAR_DWORD))
                    return var;
                if (type == VAR_REAL && (var->type == VAR_FLOAT
                                         || var->type == VAR_DOUBLE))
                    return var;
                if (var->type == type)
                    return var;
            }
        }
        block = iterate ? block->next : NULL;
    }
    return NULL;
}

static gint
gdef_get_int_var(GDEFVariable *var)
{
    const guchar *p = var->data;

    if (var->type == VAR_INTEGER)
        return gwy_get_gint32_le(&p);
    if (var->type == VAR_WORD)
        return gwy_get_guint16_le(&p);
    if (var->type == VAR_DWORD)
        return gwy_get_guint16_le(&p);
    if (var->type == VAR_CHAR)
        return *p;

    g_return_val_if_reached(0);
}

static gdouble
gdef_get_real_var(GDEFVariable *var)
{
    const guchar *p = var->data;

    if (var->type == VAR_FLOAT)
        return gwy_get_gfloat_le(&p);
    if (var->type == VAR_DOUBLE)
        return gwy_get_gdouble_le(&p);

    g_return_val_if_reached(0);
}

static gchar*
gdef_get_string_var(GDEFVariable *var)
{
    if (var->type == VAR_CHAR)
        return g_strndup(var->data, var->size);

    g_return_val_if_reached(NULL);
}

static GwyDataField*
gdef_read_channel(GDEFControlBlock *block)
{
    GDEFVariable *xres_var, *yres_var, *ymis_var, *xreal_var, *yreal_var,
                 *zunit_var, *data_var, *value_var;
    GDEFControlBlock *datablock;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data;
    const guchar *rawdata;
    const gchar *unit;
    gdouble xreal, yreal;
    gint xres, yres, ymis, i;

    if (block->n_data != 1)
        return FALSE;

    if (!(xres_var = gdef_find_variable(block, "Columns", VAR_INTEGRAL, FALSE))
        || !(yres_var = gdef_find_variable(block, "Lines", VAR_INTEGRAL, FALSE))
        || !(ymis_var = gdef_find_variable(block, "MissingLines", VAR_INTEGRAL, FALSE))
        || !(xreal_var = gdef_find_variable(block, "MaxWidth", VAR_REAL, FALSE))
        || !(yreal_var = gdef_find_variable(block, "MaxHeight", VAR_REAL, FALSE))
        || !(zunit_var = gdef_find_variable(block, "ZUnit", VAR_CHAR, FALSE))
        || !(data_var = gdef_find_variable(block, "Data", VAR_DATABLOCK, FALSE)))
        return FALSE;

    datablock = (GDEFControlBlock*)data_var->data;
    if (!(value_var = gdef_find_variable(datablock, "Value", VAR_FLOAT, TRUE)))
        return FALSE;

    xres = gdef_get_int_var(xres_var);
    yres = gdef_get_int_var(yres_var);
    ymis = gdef_get_int_var(ymis_var);
    if (err_DIMENSION(NULL, xres)
        || err_DIMENSION(NULL, yres)
        || ymis >= yres) {
        g_warning("Dimensions %d x %d are invalid.", xres, yres - ymis);
        return FALSE;
    }

    /* Not sure about missing lines, so use an inequality in this case. */
    if ((ymis == 0 && value_var->size != 4*xres*yres)
        || (ymis && value_var->size < 4*xres*(yres - ymis))) {
        g_warning("Data size does not match Lines and Columns.");
        return FALSE;
    }

    xreal = fabs(gdef_get_real_var(xreal_var));
    yreal = fabs(gdef_get_real_var(yreal_var));
    gwy_debug("xreal: %g, yreal: %g", xreal, yreal);
    /* Use negated positive conditions to catch NaNs */
    if (!(xreal > 0.0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!(yreal > 0.0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    dfield = gwy_data_field_new(xres, yres - ymis, xreal, yreal, FALSE);
    siunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string(siunit, "m");
    siunit = gwy_data_field_get_si_unit_z(dfield);
    unit = gwy_enuml_to_string(gdef_get_int_var(zunit_var),
                               "m", 0,
                               "V", 1,
                               "A", 2,
                               "N", 3,
                               "deg", 4,
                               "", 5,
                               "1/m", 6,
                               NULL);
    gwy_si_unit_set_from_string(siunit, unit);

    data = gwy_data_field_get_data(dfield);
    rawdata = value_var->data;
    for (i = 0; i < xres*(yres - ymis); i++)
        data[i] = gwy_get_gfloat_le(&rawdata);

    return dfield;
}

static gchar*
gdef_read_channel_comment(GDEFControlBlock *block)
{
    GDEFVariable *data_var, *comment_var;
    GDEFControlBlock *datablock;

    data_var = gdef_find_variable(block, "Data", VAR_DATABLOCK, FALSE);
    g_return_val_if_fail(data_var, NULL);

    datablock = (GDEFControlBlock*)data_var->data;
    comment_var = gdef_find_variable(datablock, "Comment", VAR_CHAR, TRUE);
    if (!comment_var)
        return NULL;

    return gdef_get_string_var(comment_var);
}

static GwyContainer*
gdef_read_channel_meta(GDEFControlBlock *block)
{
    GwyContainer *meta;
    gchar *s;
    guint i;

    meta = gwy_container_new();
    for (i = 0; i < block->n_variables; i++) {
        GDEFVariable *var = block->variables + i;

        if (var->type == VAR_INTEGER || var->type == VAR_WORD
            || var->type == VAR_DWORD) {
            s = g_strdup_printf("%ld", (glong)gdef_get_int_var(var));
            gwy_container_set_string_by_name(meta, var->name, s);
        }
        else if (var->type == VAR_DOUBLE || var->type == VAR_FLOAT) {
            s = g_strdup_printf("%g", gdef_get_real_var(var));
            gwy_container_set_string_by_name(meta, var->name, s);
        }
    }

    s = gdef_read_channel_comment(block);
    if (s)
        gwy_container_set_string_by_name(meta, "Comment", s);
    else
        g_free(s);

    if (gwy_container_get_n_items(meta))
        return meta;

    g_object_unref(meta);
    return NULL;
}

static GwyContainer*
gdef_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize size = 0;
    GError *err = NULL;
    GDEFHeader header;
    GDEFControlBlock *block, *blocks = NULL;
    GDEFVariable *var;
    GQuark quark;
    gchar *key, *title;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer;
    gwy_debug("buffer: %p", buffer);
    if (!gdef_read_header(&header, &p, size, error))
        goto fail;
    if (!(blocks = gdef_read_variable_lists(&p, size - (p - buffer), 0, error)))
        goto fail;

    i = 0;
    for (block = blocks; block; block = block->next) {
        GwyDataField *dfield;
#ifdef DEBUG
        guint j;

        for (j = 0; j < block->n_variables; j++) {
            var = block->variables + j;
            if (var->type != VAR_DATABLOCK)
                gwy_debug("%s 0x%08x", var->name, (guint)((const guchar*)var->data - buffer));
        }
#endif

        if ((dfield = gdef_read_channel(block))) {
            if (!container)
                container = gwy_container_new();

            quark = gwy_app_get_data_key_for_id(i);
            gwy_container_set_object(container, quark, dfield);
            g_object_unref(dfield);

            var = gdef_find_variable(block, "SourceChannel", VAR_INTEGRAL, FALSE);
            if (var) {
                title = gwy_enuml_to_string(gdef_get_int_var(var),
                                            "Light Level", 0,
                                            "Lateral Bending", 1,
                                            "Amplitude", 2,
                                            "X Sensor", 3,
                                            "Y Sensor", 4,
                                            "Loop Output", 5,
                                            "Selftest", 6,
                                            "Ext. Input 1", 7,
                                            "Ext. Input 2", 8,
                                            "Bending", 9,
                                            "Error Signal", 10,
                                            "Topography", 11,
                                            "Phase", 12,
                                            "Filtered Bending", 13,
                                            "Current", 14,
                                            "Z Sensor", 15,
                                            "Unknown", 16,
                                            NULL);
                key = g_strdup_printf("/%d/data/title", i);
                gwy_container_set_string_by_name(container, key,
                                                 g_strdup(title));
                g_free(key);

                /* XXX: That's what I've been told is necessary to obtain
                 * values in degrees. */
                if (gdef_get_int_var(var) == 12) {
                    gwy_data_field_multiply(dfield, 18.0);
                }
            }
            else {
                gwy_app_channel_title_fall_back(container, i);
            }

            if ((meta = gdef_read_channel_meta(block))) {
                key = g_strdup_printf("/%d/meta", i);
                gwy_container_set_object_by_name(container, key, meta);
                g_object_unref(meta);
                g_free(key);
            }
            i++;
        }
    }

    if (!container)
        err_NO_DATA(error);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    g_free(header.description);
    gdef_free_control_block_list(blocks);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
