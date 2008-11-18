/*
 *  $Id: spmlabf.c 8773 2008-11-18 10:09:10Z yeti-dn $
 *  Copyright (C) 2008 David Necas (Yeti).
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
 * <mime-type type="application/x-opd-spm">
 *   <comment>OPD SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="\x01\x00Directory"/>
 *   </magic>
 *   <glob pattern="*.opd"/>
 *   <glob pattern="*.OPD"/>
 * </mime-type>
 **/
#define DEBUG
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

/* Not a real magic header, but should catch the stuff */
#define MAGIC "\x01\x00" "Directory"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".opd"

enum {
    BLOCK_SIZE = 24,
    BLOCK_NAME_SIZE = 16,
};

/* XXX: Just guessing from what I've seen in the files. */
enum {
    OPD_DIRECTORY = 1,
    OPD_ARRAY = 3,
    OPD_TEXT = 5,
    OPD_SHORT = 6,
    OPD_FLOAT = 7,
    OPD_DOUBLE = 8,
    OPD_LONG = 12,
} OPDDataType;

/* The header consists of a sequence of these creatures. */
typedef struct {
    /* This is in the file */
    char name[BLOCK_NAME_SIZE + 1];
    guint type;
    guint size;
    guint flags;
    /* Derived info */
    guint pos;
    const guchar *data;
} OPDBlock;

static gboolean      module_register(void);
static gint          opd_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* opd_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gboolean      require_keys   (OPDBlock *header,
                                     guint nblocks,
                                     GError **error,
                                     ...);
static gboolean      check_sizes    (OPDBlock *header,
                                     guint nblocks,
                                     GError **error);
static guint         find_block     (OPDBlock *header,
                                     guint nblocks,
                                     const gchar *name);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Wyko OPD files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2008",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("opdfile",
                           N_("Wyko OPD files (.opd)"),
                           (GwyFileDetectFunc)&opd_detect,
                           (GwyFileLoadFunc)&opd_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
opd_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < BLOCK_SIZE + 2
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static void
get_block(OPDBlock *block, const guchar **p)
{
    memset(block->name, 0, BLOCK_NAME_SIZE + 1);
    strncpy(block->name, *p, BLOCK_NAME_SIZE);
    *p += BLOCK_NAME_SIZE;
    g_strstrip(block->name);
    block->type = gwy_get_guint16_le(p);
    block->size = gwy_get_guint32_le(p);
    block->flags = gwy_get_guint16_le(p);
}

static GwyContainer*
opd_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    OPDBlock directory_block, *header = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwySIUnit *siunitx, *siunity, *siunitz, *siunit;
    const guchar *p;
    gchar *end, *s;
    gdouble *data, *row;
    guint nblocks, offset, xres, yres, i, j;
    gdouble xreal, yreal, q;
    gint power10;
    const gfloat *pdata;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }
    if (size < BLOCK_SIZE + 2) {
        err_TOO_SHORT(error);
        goto fail;
    }

    p = buffer + 2;
    get_block(&directory_block, &p);
    directory_block.pos = 2;
    directory_block.data = buffer + 2;
    gwy_debug("<%s> size=0x%08x, pos=0x%08x, type=%u, flags=0x%04x",
              directory_block.name, directory_block.size, directory_block.pos,
              directory_block.type, directory_block.flags);
    /* This check may need to be relieved a bit */
    if (!gwy_strequal(directory_block.name, "Directory")
        || directory_block.type != OPD_DIRECTORY
        || directory_block.flags != 0xffff) {
        err_FILE_TYPE(error, "Wyko OPD data");
        goto fail;
    }

    nblocks = directory_block.size/BLOCK_SIZE;
    if (size < BLOCK_SIZE*nblocks + 2) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated"));
        goto fail;
    }

    /* We already read the directory, do not count it */
    nblocks--;
    header = g_new(OPDBlock, nblocks);
    offset = directory_block.pos + directory_block.size;
    for (i = j = 0; i < nblocks; i++) {
        get_block(header + j, &p);
        header[j].pos = offset;
        header[j].data = buffer + offset;
        offset += header[j].size;
        /* Skip void header blocks */
        if (header[j].size) {
            gwy_debug("<%s> size=0x%08x, pos=0x%08x, type=%u, flags=0x%04x",
                      header[j].name, header[j].size, header[j].pos,
                      header[j].type, header[j].flags);
            j++;
        }
        if (offset > size) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Item `%s' is beyond the end of the file."),
                        header[i].name);
            goto fail;
        }
    }
    nblocks = j;

    if (!require_keys(header, nblocks, error,
                      "Pixel_size", OPD_FLOAT,
                      "Wavelength", OPD_FLOAT,
                      NULL)
        || !check_sizes(header, nblocks, error))
        goto fail;

    i = find_block(header, nblocks, "Pixel_size");
    p = header[i].data;
    xreal = gwy_get_gfloat_le(&p);
    gwy_debug("xreal = %g", xreal);

#if 0
    p += DATA_MAGIC_SIZE;
    data_offset = atoi(g_hash_table_lookup(hash, "DataOffset"));
    if (p - buffer != data_offset) {
        g_warning("DataOffset %d differs from end of [Data] %u",
                  data_offset, (unsigned int)(p - buffer));
        p = buffer + data_offset;
    }

    xres = atoi(g_hash_table_lookup(hash, "ResolutionX"));
    yres = atoi(g_hash_table_lookup(hash, "ResolutionY"));
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    if (err_SIZE_MISMATCH(error, data_offset + 4*xres*yres, size, TRUE))
        goto fail;

    xreal = strtod(g_hash_table_lookup(hash, "ScanRangeX"), &end);
    siunitx = gwy_si_unit_new_parse(end, &power10);
    xreal *= pow10(power10);
    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }

    yreal = strtod(g_hash_table_lookup(hash, "ScanRangeY"), &end);
    siunity = gwy_si_unit_new_parse(end, &power10);
    yreal *= pow10(power10);
    /* Use negated positive conditions to catch NaNs */
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    q = strtod(g_hash_table_lookup(hash, "ZTransferCoefficient"), &end);
    siunitz = gwy_si_unit_new_parse(end, &power10);
    q *= pow10(power10);

    siunit = gwy_si_unit_new("V");
    gwy_si_unit_multiply(siunit, siunitz, siunitz);
    g_object_unref(siunit);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gfloat*)p;
    for (i = 0; i < yres; i++) {
        row = data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++)
            row[j] = q*gwy_get_gfloat_le(&p);
    }

    if (!gwy_si_unit_equal(siunitx, siunity))
        g_warning("Incompatible x and y units");

    gwy_data_field_set_si_unit_xy(dfield, siunitx);
    g_object_unref(siunitx);
    g_object_unref(siunity);

    gwy_data_field_set_si_unit_z(dfield, siunitz);
    g_object_unref(siunitz);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);

    if ((s = g_hash_table_lookup(hash, "DataName")))
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(s));
    else
        gwy_app_channel_title_fall_back(container, 0);

    meta = gwy_container_new();

    ...

    if (gwy_container_get_n_items(meta))
        gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);
#endif

    err_NO_DATA(error);

fail:
    g_free(header);
    gwy_object_unref(dfield);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static gboolean
require_keys(OPDBlock *header,
             guint nblocks,
             GError **error,
             ...)
{
    va_list ap;
    const gchar *key;
    guint i, type;

    va_start(ap, error);
    while ((key = va_arg(ap, const gchar*))) {
        type = va_arg(ap, guint);
        if ((i = find_block(header, nblocks, key)) == nblocks) {
            err_MISSING_FIELD(error, key);
            va_end(ap);
            return FALSE;
        }
        if (header[i].type != type)
            err_INVALID(error, header[i].name);
    }
    va_end(ap);

    return TRUE;
}

static gboolean
check_sizes(OPDBlock *header,
            guint nblocks,
            GError **error)
{
    /*                             0  1  2  3  4  5  6  7  8  9 10 11 12 */
    static const guint sizes[] = { 0, 0, 0, 0, 0, 0, 2, 4, 8, 0, 0, 0, 4 };

    guint i, type;

    for (i = 0; i < nblocks; i++) {
        type = header[i].type;
        if (type < G_N_ELEMENTS(sizes)) {
            if (sizes[type]) {
                if (header[i].size != sizes[type]) {
                    err_INVALID(error, header[i].name);
                    return FALSE;
                }
            }
            else if (type == OPD_DIRECTORY) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Nested directories found"));
                return FALSE;
            }
            else if (type == OPD_ARRAY) {
                /* TODO */
            }
            else if (type == OPD_TEXT) {
                /* TODO */
            }
            else {
                g_warning("Unknown item type %u", type);
            }
        }
    }

    return TRUE;
}

static guint
find_block(OPDBlock *header,
           guint nblocks,
           const gchar *name)
{
    unsigned int i;

    for (i = 0; i < nblocks; i++) {
        if (gwy_strequal(header[i].name, name))
            return i;
    }
    return nblocks;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

