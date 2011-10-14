/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
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
 * <mime-type type="application/x-itachi-sem">
 *   <comment>Itachi SEM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="[SemImageFile]"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Itachi S-3700 and S-4800 SEM data
 * .txt + image
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define Nanometre 1e-9

#define MAGIC "[SemImageFile]"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define HEADER_EXTENSION ".txt"

static gboolean      module_register      (void);
static gint          itachi_detect        (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* itachi_load          (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static GHashTable*   itachi_load_header   (const gchar *filename,
                                           gchar **header,
                                           GError **error);
static gchar*        itachi_find_data_name(const gchar *header_name,
                                           const gchar *image_name);
static void          store_meta           (gpointer key,
                                           gpointer value,
                                           gpointer user_data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Itachi S-3700 and S-4800 SEM files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("itachi",
                           N_("Itachi SEM files (.txt + image)"),
                           (GwyFileDetectFunc)&itachi_detect,
                           (GwyFileLoadFunc)&itachi_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
itachi_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    GHashTable *hash;
    gchar *header, *imagename, *fullname;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase,
                                HEADER_EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    gwy_debug("magic ok");
    hash = itachi_load_header(fileinfo->name, &header, NULL);
    if (!hash)
        return 0;

    if ((imagename = g_hash_table_lookup(hash, "ImageName"))) {
        gwy_debug("imagename <%s>", imagename);
        fullname = itachi_find_data_name(fileinfo->name, imagename);
        if (fullname) {
            g_free(fullname);
            score = 100;
        }
    }
    g_free(header);

    return score;
}

static GwyContainer*
itachi_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    GwyContainer *container = NULL, *meta;
    GdkPixbuf *pixbuf = NULL;
    GwyDataField *dfield = NULL;
    gchar *value, *imagename = NULL, *header = NULL;
    guchar *pixels;
    GHashTable *hash = NULL;
    GError *err = NULL;
    gdouble dx;
    gint pxres, pyres, hxres, hyres, rowstride, nchannels, i, j;
    gdouble *data;

    if (!(hash = itachi_load_header(filename, &header, error)))
        return NULL;

    if (!require_keys(hash, error,
                      "ImageName", "DataSize", "PixelSize",
                      NULL))
        goto fail;
    value = g_hash_table_lookup(hash, "ImageName");

    if (!(imagename = itachi_find_data_name(filename, value))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("No corresponding data file was found for header file."));
        goto fail;
    }

    if (!(pixbuf = gdk_pixbuf_new_from_file(imagename, &err))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    _("Cannot load image: %s"), err->message);
        g_clear_error(&err);
        goto fail;
    }

    /* We know the image dimensions so check them. */
    pxres = gdk_pixbuf_get_width(pixbuf);
    pyres = gdk_pixbuf_get_height(pixbuf);

    value = g_hash_table_lookup(hash, "DataSize");
    if (sscanf(value, "%ux%u", &hxres, &hyres) != 2) {
        err_INVALID(error, "DataSize");
        goto fail;
    }
    if (hxres != pxres || hyres != pyres) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Field DataSize %ux%u does not match image dimensions "
                      "%ux%u."),
                    hxres, hyres, pxres, pyres);
        goto fail;
    }
    if (err_DIMENSION(error, hxres) || err_DIMENSION(error, hyres))
        goto fail;

    dx = g_ascii_strtod(g_hash_table_lookup(hash, "PixelSize"), NULL);
    /* Use negated positive conditions to catch NaNs */
    if (!((dx = fabs(dx)) > 0)) {
        g_warning("Pixel size is 0.0, fixing to 1.0");
        dx = 1.0;
    }
    dx *= Nanometre;

    dfield = gwy_data_field_new(hxres, hyres, hxres*dx, hyres*dx, FALSE);
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");

    data = gwy_data_field_get_data(dfield);
    pixels = gdk_pixbuf_get_pixels(pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    nchannels = gdk_pixbuf_get_n_channels(pixbuf);
    for (i = 0; i < hyres; i++) {
        gdouble *drow = data + i*hxres;
        guchar *p = pixels + i*rowstride;

        for (j = 0; j < hxres; j++, p += nchannels)
            drow[j] = (p[0] + p[1] + p[2])/765.0;
    }

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);

    if ((value = g_hash_table_lookup(hash, "SampleName"))
        && *value)
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(value));
    else
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup("SEM"));

    meta = gwy_container_new();
    g_hash_table_foreach(hash, store_meta, meta);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

fail:
    gwy_object_unref(pixbuf);
    g_free(imagename);
    g_free(header);
    g_hash_table_destroy(hash);

    return container;
}

static GHashTable*
itachi_load_header(const gchar *filename,
                   gchar **header,
                   GError **error)
{
    gchar *line, *p;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    gsize size;
    GError *err = NULL;

    *header = NULL;
    if (!g_file_get_contents(filename, header, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = *header;
    line = gwy_str_next_line(&p);
    if (!gwy_strequal(line, MAGIC)) {
        err_FILE_TYPE(error, "Itachi SEM");
        g_free(header);
        *header = NULL;
        return NULL;
    }

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    gwy_debug("reading header");
    hash = gwy_text_header_parse(p, &parser, NULL, NULL);
    gwy_debug("header %p", hash);
    return hash;
}

static gchar*
itachi_find_data_name(const gchar *header_name,
                      const gchar *image_name)
{
    gchar *dirname = g_path_get_dirname(header_name);
    gchar *filename, *iname;

    filename = g_build_filename(dirname, image_name, NULL);
    gwy_debug("trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_free(dirname);
        return filename;
    }
    g_free(filename);

    iname = g_ascii_strup(image_name, -1);
    filename = g_build_filename(dirname, iname, NULL);
    gwy_debug("trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_free(iname);
        g_free(dirname);
        return filename;
    }
    g_free(iname);
    g_free(filename);

    iname = g_ascii_strdown(image_name, -1);
    filename = g_build_filename(dirname, iname, NULL);
    gwy_debug("trying <%s>", filename);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
        g_free(iname);
        g_free(dirname);
        return filename;
    }
    g_free(iname);
    g_free(filename);
    g_free(dirname);

    gwy_debug("failed");

    return NULL;
}

static void
store_meta(gpointer key,
           gpointer value,
           gpointer user_data)
{
    GwyContainer *meta = (GwyContainer*)user_data;

    if (g_utf8_validate(value, -1, NULL)) {
        gwy_container_set_string_by_name(meta, key, g_strdup(value));
    }
    else {
        gchar *s = g_convert(value, -1, "UTF-8", "CP1252", NULL, NULL, NULL);
        if (s)
            gwy_container_set_string_by_name(meta, key, s);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
