/*
 *  $Id$
 *  Copyright (C) 2015 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * Senofar PLUx data
 * .plux
 * Read
 **/

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-sensofar-spm">
 *   <comment>Sensofar PLUx data</comment>
 *   <glob pattern="*.plux"/>
 *   <glob pattern="*.PLUX"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"
#include "gwyzip.h"

#define MAGIC "PK\x03\x04"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".plux"
#define BLOODY_UTF8_BOM "\xef\xbb\xbf"

typedef struct {
    GHashTable *hash;
    GHashTable *recipe_hash;
    GArray *layers;
    GString *path;
    GString *str;
    gboolean parsing_recipe;
} PLUxFile;

static gboolean      module_register            (void);
static gint          sensofarx_detect           (const GwyFileDetectInfo *fileinfo,
                                                 gboolean only_name);
static gboolean      sensofarx_is_plausible_file(const guchar *filehead,
                                                 gsize len);
static GwyContainer* sensofarx_load             (const gchar *filename,
                                                 GwyRunType mode,
                                                 GError **error);
static gboolean      sensofarx_parse_index      (GwyZipFile zipfile,
                                                 PLUxFile *pluxfile,
                                                 GError **error);
static void          sensofarx_parse_recipe     (GwyZipFile zipfile,
                                                 PLUxFile *pluxfile);
static gboolean      read_binary_data           (const PLUxFile *pluxfile,
                                                 GwyZipFile zipfile,
                                                 const gchar *filename,
                                                 GwyContainer *container,
                                                 GError **error);
static void          sensofarx_file_free        (PLUxFile *pluxfile);
static GwyContainer* get_metadata               (const PLUxFile *pluxfile,
                                                 guint id);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Reads Sensofar PLUx files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2015",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sensofarx",
                           N_("Sensofar PLUx files (.plux)"),
                           (GwyFileDetectFunc)&sensofarx_detect,
                           (GwyFileLoadFunc)&sensofarx_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sensofarx_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    GwyZipFile zipfile;
    guchar *content;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    /* Generic ZIP file. */
    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    /* It should contain some of the expected file names.  Unfortunately,
     * they seem to store the raw data first and they are to huge to catch
     * anything following.  So this may not be as reliable as we would like. */
    if (!sensofarx_is_plausible_file(fileinfo->head, fileinfo->buffer_len))
        return 0;

    /* We have to realy look inside.  And since index.xml is a popular name
     * for the main XML document within such files, we also have to see if
     * we find "<IMAGE_SIZE_X>" somewehre near the begining of the file. */
    if ((zipfile = gwyzip_open(fileinfo->name))) {
        if (gwyzip_locate_file(zipfile, "index.xml", 1, NULL)) {
            if ((content = gwyzip_get_file_content(zipfile, NULL, NULL))) {
                if (g_strstr_len(content, 4096, "<IMAGE_SIZE_X>"))
                    score = 100;
                g_free(content);
            }
        }
        gwyzip_close(zipfile);
    }

    return score;
}

/* Try to find the name of an expected file somewhere. */
static gboolean
sensofarx_is_plausible_file(const guchar *filehead, gsize len)
{
    static const gchar *filenames[] = {
        "LAYER_0.raw", "LAYER_0.stack.raw",
        "LAYER_1.raw", "LAYER_1.stack.raw",
        "index.xml", "recipe.txt", "display.txt",
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS(filenames); i++) {
        if (gwy_memmem(filehead, len, filenames[i], strlen(filenames[i])))
            return TRUE;
    }

    return FALSE;
}

static GwyContainer*
sensofarx_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    GwyContainer *container = NULL;
    PLUxFile pluxfile;
    GwyZipFile zipfile;

    zipfile = gwyzip_open(filename);
    if (!zipfile) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("Minizip cannot open the file as a ZIP file."));
        return NULL;
    }

    gwy_clear(&pluxfile, 1);
    if (!sensofarx_parse_index(zipfile, &pluxfile, error))
        goto fail;

    if (!pluxfile.layers->len) {
        err_NO_DATA(error);
        goto fail;
    }

    sensofarx_parse_recipe(zipfile, &pluxfile);

    container = gwy_container_new();
    if (!read_binary_data(&pluxfile, zipfile, filename, container, error))
        gwy_object_unref(container);

fail:
    gwyzip_close(zipfile);
    sensofarx_file_free(&pluxfile);

    return container;
}

static gboolean
read_binary_data(const PLUxFile *pluxfile,
                 GwyZipFile zipfile,
                 const gchar *filename,
                 GwyContainer *container,
                 GError **error)
{
    GArray *layers = pluxfile->layers;
    GHashTable *hash = pluxfile->hash;
    GString *str = pluxfile->str;
    GwyDataField *dfield, *mask;
    GwyContainer *meta;
    guchar *content;
    gchar *datafilename, *title;
    gsize contentsize, expected_size;
    guint xres, yres;
    gdouble xreal, yreal;
    GQuark quark;
    gint id;
    guint i;

    if (!require_keys(pluxfile->hash, error,
                      "/xml/GENERAL/IMAGE_SIZE_X", "/xml/GENERAL/IMAGE_SIZE_Y",
                      "/xml/GENERAL/FOV_X", "/xml/GENERAL/FOV_Y",
                      NULL))
        return FALSE;

    xres = atoi(g_hash_table_lookup(hash, "/xml/GENERAL/IMAGE_SIZE_X"));
    if (err_DIMENSION(error, xres))
        return FALSE;

    yres = atoi(g_hash_table_lookup(hash, "/xml/GENERAL/IMAGE_SIZE_Y"));
    if (err_DIMENSION(error, yres))
        return FALSE;

    xreal = g_ascii_strtod(g_hash_table_lookup(hash, "/xml/GENERAL/FOV_X"),
                           NULL);
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }

    yreal = g_ascii_strtod(g_hash_table_lookup(hash, "/xml/GENERAL/FOV_Y"),
                           NULL);
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    for (i = 0; i < layers->len; i++) {
        id = g_array_index(layers, gint, i);
        g_string_printf(str, "/xml/LAYER_%d/FILENAME_Z", id);
        datafilename = g_hash_table_lookup(hash, str->str);
        if (!datafilename) {
            gwy_debug("Did not find FILENAME_Z for %s", str->str);
            continue;
        }
        gwy_debug("FILENAME_Z %s: %s", str->str, datafilename);

        if (!gwyzip_locate_file(zipfile, datafilename, 1, error)
            || !(content = gwyzip_get_file_content(zipfile, &contentsize,
                                                       error))) {
            return FALSE;
        }

        expected_size = xres*yres*sizeof(gfloat);
        if (err_SIZE_MISMATCH(error, expected_size, contentsize, TRUE)) {
            g_free(content);
            return FALSE;
        }

        dfield = gwy_data_field_new(xres, yres,
                                    xres*xreal*1e-6, yres*yreal*1e-6,
                                    FALSE);
        gwy_convert_raw_data(content, xres*yres, 1,
                             GWY_RAW_DATA_FLOAT,
                             GWY_BYTE_ORDER_LITTLE_ENDIAN,
                             gwy_data_field_get_data(dfield),
                             1e-6, 0.0);
        g_free(content);

        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "m");

        gwy_container_set_object(container, gwy_app_get_data_key_for_id(id),
                                 dfield);
        if ((mask = gwy_app_channel_mask_of_nans(dfield, TRUE))) {
            quark = gwy_app_get_mask_key_for_id(id);
            gwy_container_set_object(container, quark, mask);
            g_object_unref(mask);
        }
        g_object_unref(dfield);

        g_string_printf(str, "/%d/data/title", id);
        title = g_strdup("Z");
        gwy_container_set_string_by_name(container, str->str, title);

        if ((meta = get_metadata(pluxfile, id))) {
            g_string_printf(str, "/%d/meta", id);
            gwy_container_set_object_by_name(container, str->str, meta);
            g_object_unref(meta);
        }

        gwy_file_channel_import_log_add(container, id, NULL, filename);

        id++;
    }

    return TRUE;
}

static void
sensofarx_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
                        const gchar *element_name,
                        G_GNUC_UNUSED const gchar **attribute_names,
                        G_GNUC_UNUSED const gchar **attribute_values,
                        gpointer user_data,
                        G_GNUC_UNUSED GError **error)
{
    PLUxFile *pluxfile = (PLUxFile*)user_data;
    gchar *path, *end, *s;
    gint i;

    g_string_append_c(pluxfile->path, '/');
    g_string_append(pluxfile->path, element_name);
    path = pluxfile->path->str;
    gwy_debug("%s", path);

    if (!pluxfile->parsing_recipe && g_str_has_prefix(path, "/xml/LAYER_")) {
        s = path + strlen("/xml/LAYER_");
        if (!strchr(s, '/')) {
            i = strtol(s, &end, 10);
            if (!*end) {
                gwy_debug("LAYER_%d", i);
                g_array_append_val(pluxfile->layers, i);
                return;
            }
        }
    }
}

static void
sensofarx_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
                      const gchar *element_name,
                      gpointer user_data,
                      G_GNUC_UNUSED GError **error)
{
    PLUxFile *pluxfile = (PLUxFile*)user_data;
    guint n, len = pluxfile->path->len;
    gchar *path = pluxfile->path->str;

    n = strlen(element_name);
    g_return_if_fail(g_str_has_suffix(path, element_name));
    g_return_if_fail(len > n);
    g_return_if_fail(path[len-1 - n] == '/');
    gwy_debug("%s", path);

    g_string_set_size(pluxfile->path, len-1 - n);
}

static void
sensofarx_text(G_GNUC_UNUSED GMarkupParseContext *context,
               const gchar *text,
               G_GNUC_UNUSED gsize text_len,
               gpointer user_data,
               G_GNUC_UNUSED GError **error)
{
    PLUxFile *pluxfile = (PLUxFile*)user_data;
    GHashTable *hash;
    gchar *path = pluxfile->path->str;
    GString *str = pluxfile->str;

    if (!strlen(text))
        return;

    g_string_assign(str, text);
    g_strstrip(str->str);
    if (!strlen(str->str))
        return;

    gwy_debug("%s <%s>", path, str->str);
    hash = (pluxfile->parsing_recipe ? pluxfile->recipe_hash : pluxfile->hash);
    g_hash_table_insert(hash, g_strdup(path), g_strdup(str->str));
}

static gboolean
sensofarx_parse_index(GwyZipFile zipfile,
                      PLUxFile *pluxfile,
                      GError **error)
{
    GMarkupParser parser = {
        &sensofarx_start_element,
        &sensofarx_end_element,
        &sensofarx_text,
        NULL,
        NULL,
    };
    GMarkupParseContext *context = NULL;
    guchar *content = NULL, *s;
    gboolean ok = FALSE;

    if (!gwyzip_locate_file(zipfile, "index.xml", 1, error)
        || !(content = gwyzip_get_file_content(zipfile, NULL, error)))
        return FALSE;

    gwy_strkill(content, "\r");
    s = content;
    /* Not seen in the wild but the XML people tend to use BOM in UTF-8... */
    if (g_str_has_prefix(s, BLOODY_UTF8_BOM))
        s += strlen(BLOODY_UTF8_BOM);

    pluxfile->path = g_string_new(NULL);
    pluxfile->str = g_string_new(NULL);
    pluxfile->hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                           g_free, g_free);
    pluxfile->layers = g_array_new(FALSE, FALSE, sizeof(gint));

    context = g_markup_parse_context_new(&parser, 0, pluxfile, NULL);
    if (!g_markup_parse_context_parse(context, s, -1, error))
        goto fail;
    if (!g_markup_parse_context_end_parse(context, error))
        goto fail;

    /* XXX: This does not mean much.  Caller needs to check if we have
     * any images, they have dimensions, etc. */
    ok = TRUE;

fail:
    if (context)
        g_markup_parse_context_free(context);
    g_free(content);

    return ok;
}

static void
sensofarx_parse_recipe(GwyZipFile zipfile,
                       PLUxFile *pluxfile)
{
    GMarkupParser parser = {
        &sensofarx_start_element,
        &sensofarx_end_element,
        &sensofarx_text,
        NULL,
        NULL,
    };
    GMarkupParseContext *context = NULL;
    guchar *content = NULL, *s;

    /* XXX: for some reason the file tends to be named ‘./recipe.txt’ in the
     * archive. */
    if ((!gwyzip_locate_file(zipfile, "recipe.txt", 1, NULL)
         && !gwyzip_locate_file(zipfile, "./recipe.txt", 1, NULL))
        || !(content = gwyzip_get_file_content(zipfile, NULL, NULL)))
        return;

    gwy_strkill(content, "\r");
    s = content;
    /* Not seen in the wild but the XML people tend to use BOM in UTF-8... */
    if (g_str_has_prefix(s, BLOODY_UTF8_BOM))
        s += strlen(BLOODY_UTF8_BOM);

    pluxfile->recipe_hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, g_free);
    pluxfile->parsing_recipe = TRUE;
    context = g_markup_parse_context_new(&parser, 0, pluxfile, NULL);
    if (!g_markup_parse_context_parse(context, s, -1, NULL)
        || !g_markup_parse_context_end_parse(context, NULL)) {
        g_hash_table_destroy(pluxfile->recipe_hash);
        pluxfile->recipe_hash = NULL;
    }
    pluxfile->parsing_recipe = FALSE;

    if (context)
        g_markup_parse_context_free(context);
    g_free(content);
}

static void
sensofarx_file_free(PLUxFile *pluxfile)
{
    if (pluxfile->hash)
        g_hash_table_destroy(pluxfile->hash);
    if (pluxfile->recipe_hash)
        g_hash_table_destroy(pluxfile->recipe_hash);
    if (pluxfile->path)
        g_string_free(pluxfile->path, TRUE);
    if (pluxfile->str)
        g_string_free(pluxfile->str, TRUE);
    if (pluxfile->layers)
        g_array_free(pluxfile->layers, TRUE);
}

static void
add_recipe_meta(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    const gchar *path = (const gchar*)hkey;
    gchar *name, *p;
    gboolean keepcap = TRUE;

    if (!g_str_has_prefix(path, "/xml/")
        || g_str_has_suffix(path, "/FOVINBLACK"))
        return;

    name = gwy_strreplace(path + strlen("/xml/"), "/", "::", (gsize)-1);
    for (p = name; *p; p++) {
        if (*p == '_') {
            *p = ' ';
            keepcap = TRUE;
        }
        else if (*p == ':')
            keepcap = TRUE;
        else if (keepcap)
            keepcap = FALSE;
        else
            *p = g_ascii_tolower(*p);
    }

    gwy_container_set_const_string_by_name((GwyContainer*)user_data,
                                           name, (const gchar*)hvalue);
    g_free(name);
}

static GwyContainer*
get_metadata(const PLUxFile *pluxfile, guint id)
{
    GHashTable *hash = pluxfile->hash;
    GwyContainer *meta = gwy_container_new();
    gchar *name, *value;
    gchar buf[40], c;
    guint n, i;

    if ((value = g_hash_table_lookup(hash, "/xml/GENERAL/AUTHOR")))
        gwy_container_set_const_string_by_name(meta, "General::Author", value);
    if ((value = g_hash_table_lookup(hash, "/xml/GENERAL/DATE")))
        gwy_container_set_const_string_by_name(meta, "General::Date", value);

    if ((value = g_hash_table_lookup(hash, "/xml/INFO/SIZE"))
        && (n = atoi(value))) {
        for (i = 0; i < n; i++) {
            g_snprintf(buf, sizeof(buf), "/xml/INFO/ITEM_%u/NAME", i);
            name = g_hash_table_lookup(hash, buf);
            g_snprintf(buf, sizeof(buf), "/xml/INFO/ITEM_%u/VALUE", i);
            value = g_hash_table_lookup(hash, buf);
            if (name && value && strlen(name) && strlen(value)) {
                name = g_strconcat("Info::", name, NULL);
                gwy_container_set_const_string_by_name(meta, name, value);
                g_free(name);
            }
        }
    }

    for (c = 'X'; c <= 'Z'; c++) {
        g_snprintf(buf, sizeof(buf), "/xml/LAYER_%u/POSITION_%c", id, c);
        if ((value = g_hash_table_lookup(hash, buf))) {
            value = g_strconcat(value, " µm", NULL);
            g_snprintf(buf, sizeof(buf), "Layer::Position %c", c);
            gwy_container_set_const_string_by_name(meta, buf, value);
        }
    }

    if (pluxfile->recipe_hash)
        g_hash_table_foreach(pluxfile->recipe_hash, add_recipe_meta, meta);

    if (gwy_container_get_n_items(meta))
        return meta;

    g_object_unref(meta);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
