/*
 *  @(#) $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
 * Carl Zeiss SEM scans
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

#define MAGIC_COMMENT "\r\nAP_PIXEL_SIZE\r\n"

enum {
    ZEISS_HEADER_TAG = 34118,
};

static gboolean      module_register(void);
static gint          zeiss_detect   (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* zeiss_load     (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static GwyContainer* zeiss_load_tiff(const GwyTIFF *tiff,
                                     GError **error);
static void          add_meta       (gpointer hkey,
                                     gpointer hvalue,
                                     gpointer user_data);
static GHashTable*   parse_comment  (gchar *text);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Carl Zeiss SEM images."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "201",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("zeiss",
                           N_("Carl Zeiss SEM scans (.tif)"),
                           (GwyFileDetectFunc)&zeiss_detect,
                           (GwyFileLoadFunc)&zeiss_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
zeiss_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    GwyTIFF *tiff;
    gint score = 0;
    gchar *comment = NULL;

    if (only_name)
        return score;

    /* Weed out non-TIFFs */
    if (fileinfo->buffer_len <= MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* Use GwyTIFF for detection to avoid problems with fragile libtiff. */
    if ((tiff = gwy_tiff_load(fileinfo->name, NULL))
        && gwy_tiff_get_string0(tiff, ZEISS_HEADER_TAG, &comment)
        && strstr(comment, MAGIC_COMMENT))
        score = 100;

    g_free(comment);
    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
zeiss_load(const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    GwyTIFF *tiff;
    GwyContainer *container = NULL;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    container = zeiss_load_tiff(tiff, error);
    gwy_tiff_free(tiff);

    return container;
}

static GwyContainer*
zeiss_load_tiff(const GwyTIFF *tiff, GError **error)
{
    GwyContainer *container = NULL, *meta = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GwyTIFFImageReader *reader = NULL;
    GHashTable *hash = NULL;
    gint i, power10;
    gchar *value, *end, *comment = NULL;
    gdouble *data;
    double factor, dx;

    /* Comment with parameters is common for all data fields */
    if (!gwy_tiff_get_string0(tiff, ZEISS_HEADER_TAG, &comment)
        || !strstr(comment, MAGIC_COMMENT)) {
        err_FILE_TYPE(error, "Carl Zeiss SEM");
        goto fail;
    }

    /* Read the comment header. */
    hash = parse_comment(comment);
    if (!(value = g_hash_table_lookup(hash, "Pixel Size"))) {
        err_MISSING_FIELD(error, "Pixel Size");
        goto fail;
    }

    dx = g_ascii_strtod(value, &end);
    gwy_debug("dx %g", dx);
    /* Use negated positive conditions to catch NaNs */
    if (!((dx = fabs(dx)) > 0)) {
        g_warning("Real pixel size is 0.0, fixing to 1.0");
        dx = 1.0;
    }

    /* Request a reader, this ensures dimensions and stuff are defined. */
    if (!(reader = gwy_tiff_get_image_reader(tiff, 0, 1, error)))
        goto fail;

    siunit = gwy_si_unit_new_parse(end, &power10);
    factor = pow10(power10);
    dfield = gwy_data_field_new(reader->width, reader->height,
                                reader->width * factor * dx,
                                reader->height * factor * dx,
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < reader->height; i++)
        gwy_tiff_read_image_row(tiff, reader, 0, i, 1.0, 0.0,
                                data + i*reader->width);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Secondary electron count"));

    meta = gwy_container_new();
    g_hash_table_foreach(hash, add_meta, meta);
    if (gwy_container_get_n_items(meta))
        gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

fail:
    if (hash)
        g_hash_table_destroy(hash);
    g_free(comment);

    return container;
}

static void
add_meta(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    gchar *value = g_convert((const gchar*)hvalue, -1,
                             "UTF-8", "ISO-8859-1",
                             NULL, NULL, NULL);
    if (value)
        gwy_container_set_string_by_name(GWY_CONTAINER(user_data),
                                         (gchar*)hkey, value);
}

static GHashTable*
parse_comment(gchar *text)
{
    gchar *line, *value, *p = text;
    GHashTable *hash = g_hash_table_new(g_str_hash, g_str_equal);

    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        if (g_str_has_prefix(line, "Date :")
            || g_str_has_prefix(line, "Time :"))
            value = line + 6;
        else if (!(value = strchr(line, '=')))
            continue;

        *value = '\0';
        value++;
        g_strstrip(line);
        g_strstrip(value);
        g_hash_table_insert(hash, line, value);
        gwy_debug("<%s> = <%s>", line, value);
    }

    return hash;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
