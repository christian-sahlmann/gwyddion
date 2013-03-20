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
#define DEBUG 1
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
static GHashTable*         tsc_parse_text_fields(GwyTIFF *tiff,
                                                 const GwyTIFFEntry *entry);
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
    GHashTable *hash;
    gint i;
    const gchar *value;
    gdouble *data;
    gdouble xstep, ystep;

    hash = tsc_parse_text_fields(tiff, entry);

    if ((value = g_hash_table_lookup(hash, "PixelSizeX"))) {
        gwy_debug("PixelSizeX %s", value);
        xstep = g_strtod(value, NULL);
        if (!((xstep = fabs(xstep)) > 0))
            g_warning("Real pixel width is 0.0, fixing to 1.0");
    }
    else {
        err_MISSING_FIELD(error, "PixelSizeX");
        goto fail;
    }

    if ((value = g_hash_table_lookup(hash, "PixelSizeY"))) {
        gwy_debug("PixelSizeY %s", value);
        ystep = g_strtod(value, NULL);
        if (!((ystep = fabs(ystep)) > 0))
            g_warning("Real pixel height is 0.0, fixing to 1.0");
    }
    else {
        err_MISSING_FIELD(error, "PixelSizeY");
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
    g_hash_table_destroy(hash);
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

static GHashTable*
tsc_parse_text_fields(GwyTIFF *tiff, const GwyTIFFEntry *entry)
{
    const guchar *p = entry->value;
    guint len = entry->count, pos;
    const gchar *eol = tiff->data + tiff->get_guint32(&p), *buf = eol;
    GHashTable *hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             g_free, g_free);

    /* Cannot start at the first byte anyway. */
    eol += 2;
    do {
        /* Try to find things that look like Key=Value\r\n.
         * They are embedded in some binary rubbish. */
        guint eqpos = 0, eqcount = 0;
        pos = eol - buf;
        if (!(eol = gwy_memmem(eol, len - pos, "\r\n", 2)))
            break;
        pos = (eol - buf) - 1;
        while (pos && buf[pos] >= 0x20 && buf[pos] < 0x7f) {
            if (buf[pos] == '=') {
                eqpos = pos;
                eqcount++;
            }
            pos--;
        }
        pos++;
        /* Both key and value must be nonempty.  Otherwise accept anything. */
        if (eqpos > pos && eqpos+1 < (guint)(eol - buf) && eqcount == 1) {
            gchar *key = g_strndup(buf + pos, eqpos - pos);
            gchar *value = g_strndup(buf + eqpos+1, (eol - buf) - (eqpos+1));
            gwy_debug("<%s>=<%s>", key, value);
            g_hash_table_insert(hash, key, value);
        }

        eol += 2;
    } while ((guint)(eol - buf) < len);

    return hash;
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
