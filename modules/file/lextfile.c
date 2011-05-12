/*
 *  @(#) $Id$
 *  Copyright (C) 2010 David Necas (Yeti), Petr Klapetek.
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-olympus-lext-4000">
 *   <comment>Olympus LEXT 4000</comment>
 *   <magic priority="10">
 *     <match type="string" offset="0" value="II\x2a\x00"/>
 *   </magic>
 *   <glob pattern="*.lext"/>
 *   <glob pattern="*.LEXT"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Olympus LEXT 4000
 * .lext
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

/* Really.  They use factor 1e-6 and the value is in microns. */
#define Picometer 1e-12

#ifdef HAVE_MEMRCHR
#define strlenrchr(s,c,len) (gchar*)memrchr((s),(c),(len))
#else
#define strlenrchr(s,c,len) strchr((s),(c))
#endif

#if GLIB_CHECK_VERSION(2, 12, 0)
#define TREAT_CDATA_AS_TEXT G_MARKUP_TREAT_CDATA_AS_TEXT
#else
#define TREAT_CDATA_AS_TEXT 0
#endif

#define MAGIC      "II\x2a\x00"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define MAGIC_COMMENT "<TiffTagDescData "

typedef struct {
    GString *path;
    GHashTable *hash;
} LextFile;

static gboolean      module_register (void);
static gint          lext_detect      (const GwyFileDetectInfo *fileinfo,
                                      gboolean only_name);
static GwyContainer* lext_load        (const gchar *filename,
                                      GwyRunType mode,
                                      GError **error);
static GwyContainer* lext_load_tiff   (const GwyTIFF *tiff,
                                      GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports LEXT data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2010",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("lext",
                           N_("Olympus LEXT OLS4000 (.lext)"),
                           (GwyFileDetectFunc)&lext_detect,
                           (GwyFileLoadFunc)&lext_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
lext_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
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
        && gwy_tiff_get_string0(tiff, GWY_TIFFTAG_IMAGE_DESCRIPTION, &comment)
        && strstr(comment, MAGIC_COMMENT))
        score = 100;

    g_free(comment);
    if (tiff)
        gwy_tiff_free(tiff);

    return score;
}

static GwyContainer*
lext_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyTIFF *tiff;
    GwyContainer *container = NULL;

    tiff = gwy_tiff_load(filename, error);
    if (!tiff)
        return NULL;

    container = lext_load_tiff(tiff, error);
    gwy_tiff_free(tiff);

    return container;
}

static void
start_element(G_GNUC_UNUSED GMarkupParseContext *context,
              const gchar *element_name,
              G_GNUC_UNUSED const gchar **attribute_names,
              G_GNUC_UNUSED const gchar **attribute_values,
              gpointer user_data,
              GError **error)
{
    LextFile *lfile = (LextFile*)user_data;

    if (!lfile->path->len && !gwy_strequal(element_name, "TiffTagDescData")) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                    _("Top-level element is not ‘TiffTagDescData’."));
        return;
    }

    g_string_append_c(lfile->path, '/');
    g_string_append(lfile->path, element_name);
}

static void
end_element(G_GNUC_UNUSED GMarkupParseContext *context,
            const gchar *element_name,
            gpointer user_data,
            G_GNUC_UNUSED GError **error)
{
    LextFile *lfile = (LextFile*)user_data;
    gchar *pos;

    pos = strlenrchr(lfile->path->str, '/', lfile->path->len);
    /* GMarkupParser should raise a run-time error if this does not hold. */
    g_assert(pos && strcmp(pos + 1, element_name) == 0);
    g_string_truncate(lfile->path, pos - lfile->path->str);
}

static void
text(G_GNUC_UNUSED GMarkupParseContext *context,
     const gchar *value,
     G_GNUC_UNUSED gsize value_len,
     gpointer user_data,
     G_GNUC_UNUSED GError **error)
{
    LextFile *lfile = (LextFile*)user_data;
    const gchar *path = lfile->path->str;
    gchar *val = g_strdup(value);

    g_strstrip(val);
    if (*val) {
        gwy_debug("<%s> <%s>", path, val);
        g_hash_table_replace(lfile->hash, g_strdup(path), val);
    }
    else
        g_free(val);
}

static void
titlecase_channel_name(gchar *name)
{
    *name = g_ascii_toupper(*name);
    name++;
    while (*name) {
        *name = g_ascii_tolower(*name);
        name++;
    }
}

static GwyContainer*
lext_load_tiff(const GwyTIFF *tiff, GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GwyTIFFImageReader *reader = NULL;
    GMarkupParser parser = { start_element, end_element, text, NULL, NULL };
    GHashTable *hash = NULL;
    GMarkupParseContext *context = NULL;
    LextFile lfile;
    gchar *comment = NULL;
    gchar *title = NULL;
    const gchar *value;
    GError *err = NULL;
    guint dir_num = 0;
    GString *key;

    /* Comment with parameters is common for all data fields */
    if (!gwy_tiff_get_string0(tiff, GWY_TIFFTAG_IMAGE_DESCRIPTION, &comment)
        || !strstr(comment, MAGIC_COMMENT)) {
        g_free(comment);
        err_FILE_TYPE(error, "LEXT");
        return NULL;
    }

    /* Read the comment header. */
    lfile.hash = hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              g_free, g_free);
    lfile.path = key = g_string_new(NULL);
    context = g_markup_parse_context_new(&parser, TREAT_CDATA_AS_TEXT,
                                         &lfile, NULL);
    if (!g_markup_parse_context_parse(context, comment, strlen(comment), &err)
        || !g_markup_parse_context_end_parse(context, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("XML parsing failed: %s"), err->message);
        g_clear_error(&err);
        goto fail;
    }

    for (dir_num = 0; dir_num < gwy_tiff_get_n_dirs(tiff); dir_num++) {
        double xscale, yscale, zfactor;
        GQuark quark;
        gdouble *data;
        gint i;

        g_free(title);
        title = NULL;

        if (!gwy_tiff_get_string(tiff, dir_num, GWY_TIFFTAG_IMAGE_DESCRIPTION,
                                 &title)) {
            g_warning("Directory %u has no ImageDescription.", dir_num);
            continue;
        }

        /* Ignore the first directory, thumbnail and anything called INVALID */
        if (dir_num == 0)
            continue;
        gwy_debug("Channel <%s>", title);
        titlecase_channel_name(title);
        if (gwy_stramong(title, "Thumbnail", "Invalid", NULL))
            continue;

        reader = gwy_tiff_image_reader_free(reader);
        /* Request a reader, this ensures dimensions and stuff are defined. */
        reader = gwy_tiff_get_image_reader(tiff, dir_num, 1, &err);
        if (!reader) {
            g_warning("Ignoring directory %u: %s", dir_num, err->message);
            g_clear_error(&err);
            continue;
        }

        g_string_printf(key, "/TiffTagDescData/%sInfo/%sDataPerPixelX",
                        title, title);
        if (!(value = g_hash_table_lookup(hash, (gpointer)key->str))) {
            g_warning("Cannot find %s", key->str);
        }
        xscale = Picometer * g_ascii_strtod(value, NULL);

        g_string_printf(key, "/TiffTagDescData/%sInfo/%sDataPerPixelY",
                        title, title);
        if (!(value = g_hash_table_lookup(hash, (gpointer)key->str))) {
            g_warning("Cannot find %s", key->str);
        }
        yscale = Picometer * g_ascii_strtod(value, NULL);

        g_string_printf(key, "/TiffTagDescData/%sInfo/%sDataPerPixelZ",
                        title, title);
        if (!(value = g_hash_table_lookup(hash, (gpointer)key->str))) {
            g_warning("Cannot find %s", key->str);
        }
        zfactor = g_ascii_strtod(value, NULL);

        siunit = gwy_si_unit_new("m");
        dfield = gwy_data_field_new(reader->width, reader->height,
                                    reader->width * xscale,
                                    reader->height * yscale,
                                    FALSE);
        // units
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);

        if (gwy_strequal(title, "Height")) {
            siunit = gwy_si_unit_new("m");
            zfactor *= Picometer;
        }
        else if (gwy_strequal(title, "Intensity")) {
            siunit = gwy_si_unit_new(NULL);
        }
        else
            siunit = gwy_si_unit_new(NULL);

        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);

        data = gwy_data_field_get_data(dfield);
        for (i = 0; i < reader->height; i++)
            gwy_tiff_read_image_row(tiff, reader, 0, i, zfactor, 0.0,
                                    data + i*reader->width);

        /* add read datafield to container */
        if (!container)
            container = gwy_container_new();

        quark = gwy_app_get_data_key_for_id(dir_num);
        gwy_container_set_object(container, quark, dfield);

        g_string_printf(key, "/%u/data/title", dir_num);
        gwy_container_set_string_by_name(container, key->str, title);
        title = NULL;

        // free resources
        g_object_unref(dfield);
    }

fail:
    g_free(title);
    g_free(comment);
    if (reader) {
        gwy_tiff_image_reader_free(reader);
        reader = NULL;
    }
    if (hash)
        g_hash_table_destroy(hash);
    if (key)
        g_string_free(key, TRUE);
    if (context)
        g_markup_parse_context_free(context);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
