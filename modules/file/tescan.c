/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * Tescan MIRA SEM images
 * .tif
 * Read
 **/

#include "config.h"
#include <stdlib.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include "err.h"
#include "gwytiff.h"

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define MAGIC_FIELD "PixelSizeX="
#define MAGIC_FIELD_SIZE (sizeof(MAGIC_FIELD)-1)

enum {
    TESCAN_TIFF_TAG = 50431,
};

typedef enum {
    TESCAN_BLOCK_LAST = 0,
    TESCAN_BLOCK_THUMBNAIL = 1, /* JPEG */
    TESCAN_BLOCK_MAIN = 2,
    TESCAN_BLOCK_SEM = 3,
    TESCAN_BLOCK_GAMA = 4,
    TESCAN_BLOCK_FIB = 5,
    TESCAN_BLOCK_NTYPES,
} TescanBlockType;

typedef struct {
    TescanBlockType type;
    guint32 size;
    const guchar *data;
} TescanBlock;

typedef struct {
    GHashTable *target;
    const gchar *prefix;
} BlockCopyInfo;

static gboolean            module_register      (void);
static gint                tsc_detect           (const GwyFileDetectInfo *fileinfo,
                                                 gboolean only_name);
static GwyContainer*       tsc_load             (const gchar *filename,
                                                 GwyRunType mode,
                                                 GError **error);
static GwyContainer*       tsc_load_tiff        (GwyTIFF *tiff,
                                                 const GwyTIFFEntry *entry,
                                                 GError **error);
static const GwyTIFFEntry* tsc_find_header      (GwyTIFF *tiff,
                                                 GError **error);
static GArray*             tsc_get_blocks       (GwyTIFF *tiff,
                                                 const GwyTIFFEntry *entry,
                                                 GError **error);
static void                tsc_parse_text_fields(GHashTable *globalhash,
                                                 const gchar *prefix,
                                                 TescanBlock *block);
static void                copy_with_prefix     (gpointer hkey,
                                                 gpointer hvalue,
                                                 gpointer user_data);
static GwyContainer*       get_meta             (GHashTable *hash);
static void                add_meta             (gpointer hkey,
                                                 gpointer hvalue,
                                                 gpointer user_data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Tescan SEM images."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("tescan",
                           N_("Tescan MIRA SEM image (.tif)"),
                           (GwyFileDetectFunc)&tsc_detect,
                           (GwyFileLoadFunc)&tsc_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
tsc_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    guint score = 0;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return score;

    /* Use GwyTIFF for detection to avoid problems with fragile libtiff.
     * Progressively try more fine tests. */
    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
         && tsc_find_header(tiff, NULL))
        score = 100;

    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
tsc_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyTIFF *tiff;
    const GwyTIFFEntry *entry;
    GwyContainer *container = NULL;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    entry = tsc_find_header(tiff, error);
    if (!entry) {
        gwy_tiff_free(tiff);
        return NULL;
    }

    container = tsc_load_tiff(tiff, entry, error);
    gwy_tiff_free(tiff);

    return container;
}

static GwyContainer*
tsc_load_tiff(GwyTIFF *tiff, const GwyTIFFEntry *entry, GError **error)
{
    GwyContainer *container = NULL, *meta;
    GwyDataField *dfield;
    GwyTIFFImageReader *reader = NULL;
    GHashTable *hash = NULL;
    GArray *blocks = NULL;
    gint i;
    const gchar *value;
    gdouble *data;
    gdouble xstep, ystep;

    if (!(blocks = tsc_get_blocks(tiff, entry, error)))
        goto fail;

    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    for (i = 0; i < blocks->len; i++) {
        TescanBlock *block = &g_array_index(blocks, TescanBlock, i);
        if (block->type == TESCAN_BLOCK_MAIN)
            tsc_parse_text_fields(hash, "Main", block);
        else if (block->type == TESCAN_BLOCK_SEM)
            tsc_parse_text_fields(hash, "SEM", block);
        else if (block->type == TESCAN_BLOCK_GAMA)
            tsc_parse_text_fields(hash, "GAMA", block);
        else if (block->type == TESCAN_BLOCK_FIB)
            tsc_parse_text_fields(hash, "FIB", block);
    }

    if ((value = g_hash_table_lookup(hash, "Main::PixelSizeX"))) {
        gwy_debug("Main::PixelSizeX %s", value);
        xstep = g_strtod(value, NULL);
        if (!((xstep = fabs(xstep)) > 0))
            g_warning("Real pixel width is 0.0, fixing to 1.0");
    }
    else {
        err_MISSING_FIELD(error, "Main::PixelSizeX");
        goto fail;
    }

    if ((value = g_hash_table_lookup(hash, "Main::PixelSizeY"))) {
        gwy_debug("Main::PixelSizeY %s", value);
        ystep = g_strtod(value, NULL);
        if (!((ystep = fabs(ystep)) > 0))
            g_warning("Real pixel height is 0.0, fixing to 1.0");
    }
    else {
        err_MISSING_FIELD(error, "Main::PixelSizeY");
        goto fail;
    }

    /* Request a reader, this ensures dimensions and stuff are defined. */
    if (!(reader = gwy_tiff_get_image_reader(tiff, 0, 1, error)))
        goto fail;

    dfield = gwy_data_field_new(reader->width, reader->height,
                                reader->width * xstep,
                                reader->height * ystep,
                                FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");

    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < reader->height; i++)
        gwy_tiff_read_image_row(tiff, reader, 0, i,
                                1.0/((1 << reader->bits_per_sample) - 1),
                                0.0,
                                data + i*reader->width);

    container = gwy_container_new();

    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    /* FIXME: Just kidding. */
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Intensity"));

    if ((meta = get_meta(hash))) {
        gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);
    }

fail:
    if (hash)
        g_hash_table_destroy(hash);
    if (blocks)
        g_array_free(blocks, TRUE);
    if (reader) {
        gwy_tiff_image_reader_free(reader);
        reader = NULL;
    }

    return container;
}

static const GwyTIFFEntry*
tsc_find_header(GwyTIFF *tiff, GError **error)
{
    const GwyTIFFEntry *entry;
    const guchar *p;

    if (!(entry = gwy_tiff_find_tag(tiff, 0, TESCAN_TIFF_TAG))
        || (entry->type != GWY_TIFF_BYTE
            && entry->type != GWY_TIFF_SBYTE)) {
        err_FILE_TYPE(error, "Tescan MIRA");
        return NULL;
    }

    p = entry->value;
    p = tiff->data + tiff->get_guint32(&p);
    if (!gwy_memmem(p, entry->count, MAGIC_FIELD, MAGIC_FIELD_SIZE)) {
        err_MISSING_FIELD(error, MAGIC_FIELD);
        return NULL;
    }

    return entry;
}

static GArray*
tsc_get_blocks(GwyTIFF *tiff, const GwyTIFFEntry *entry, GError **error)
{
    const guchar *t = entry->value;
    const guchar *p = tiff->data + tiff->get_guint32(&t);
    const guchar *end = p + entry->count;
    GArray *blocks = g_array_new(FALSE, FALSE, sizeof(TescanBlock));
    gboolean seen_last = FALSE;

    while (p < end) {
        TescanBlock block;

        if (seen_last)
            g_warning("The terminating block is not really last.");

        if ((end - p) < 6) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Parameter header is truncated"));
            goto fail;
        }

        block.size = tiff->get_guint32(&p);
        block.type = tiff->get_guint16(&p);
        gwy_debug("block of type %u and size %u", block.type, block.size);
        /* FIXME: Emit a better message for block.size < 2? */
        if (block.size > (gulong)(end - p) + 2 || block.size < 2) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Parameter header is truncated"));
            goto fail;
        }
        if (block.type >= TESCAN_BLOCK_NTYPES)
            g_warning("Unknown block type %u.", block.type);
        if (block.type == TESCAN_BLOCK_LAST)
            seen_last = TRUE;

        block.data = p;
        g_array_append_val(blocks, block);
        p += block.size - 2;
    }
    if (!seen_last)
        g_warning("Have not seen the terminating block.");

    return blocks;

fail:
    g_array_free(blocks, TRUE);
    return NULL;
}

static void
tsc_parse_text_fields(GHashTable *globalhash, const gchar *prefix,
                      TescanBlock *block)
{
    BlockCopyInfo block_copy_info;
    GwyTextHeaderParser parser;
    GHashTable *hash;
    gchar *data;

    data = g_new(gchar, block->size-1);
    memcpy(data, block->data, block->size-2);
    data[block->size-2] = '\0';

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    hash = gwy_text_header_parse(data, &parser, NULL, NULL);

    block_copy_info.target = globalhash;
    block_copy_info.prefix = prefix;
    g_hash_table_foreach(hash, copy_with_prefix, &block_copy_info);

    g_free(data);
    g_hash_table_destroy(hash);
}

static void
copy_with_prefix(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    BlockCopyInfo *block_copy_info = (BlockCopyInfo*)user_data;
    gchar *key = g_strconcat(block_copy_info->prefix, "::", hkey, NULL);
    g_hash_table_insert(block_copy_info->target, key, g_strdup(hvalue));
}

static GwyContainer*
get_meta(GHashTable *hash)
{
    GwyContainer *meta = gwy_container_new();
    g_hash_table_foreach(hash, add_meta, meta);
    if (gwy_container_get_n_items(meta))
        return meta;

    g_object_unref(meta);
    return NULL;
}

static void
add_meta(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    gchar *value = hvalue, *skey = hkey;

    if (!strlen(value))
        return;

    gwy_container_set_string_by_name(GWY_CONTAINER(user_data),
                                     skey, g_strdup(value));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
