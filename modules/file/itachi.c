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
#define DEBUG 1
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
#if 0
    GwyContainer *container = NULL, *meta;
    GwyDataField *dfield = NULL;
    GwySIUnit *unit;
    gchar *line, *p, *value, *header, *buffer = NULL;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal, q;
    guint year, month, day, hour, minute, second;
    gint i, xres, yres, power10;
    gdouble *data;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    p = buffer;
    line = gwy_str_next_line(&p);
    if (!g_str_has_prefix(line, MAGIC)) {
        err_FILE_TYPE(error, "Attocube ASC");
        goto fail;
    }

    line = gwy_str_next_line(&p);
    if (!line
        || sscanf(line, "# %u-%u-%uT%u:%u:%u",
                  &year, &month, &day, &hour, &minute, &second) != 6) {
        err_FILE_TYPE(error, "Attocube ASC");
        goto fail;
    }

    header = p;
    p = strstr(header, DATA_MAGIC);
    if (!p) {
        err_FILE_TYPE(error, "Attocube ASC");
        goto fail;
    }
    *p = '\0';
    p += strlen(DATA_MAGIC);

    gwy_clear(&parser, 1);
    parser.line_prefix = "#";
    parser.key_value_separator = ":";
    parser.item = &lowercase_value;
    hash = gwy_text_header_parse(header, &parser, NULL, NULL);

    if (!require_keys(hash, error,
                      "x-pixels", "y-pixels", "x-length", "y-length",
                      NULL))
        goto fail;

    xres = atoi(g_hash_table_lookup(hash, "x-pixels"));
    yres = atoi(g_hash_table_lookup(hash, "y-pixels"));
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    xreal = g_ascii_strtod(g_hash_table_lookup(hash, "x-length"), NULL);
    yreal = g_ascii_strtod(g_hash_table_lookup(hash, "y-length"), NULL);
    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);


    if ((value = g_hash_table_lookup(hash, "x-unit"))) {
        if ((line = g_hash_table_lookup(hash, "y-unit"))
            && !gwy_strequal(line, value))
            g_warning("X and Y units differ, using X");

        unit = gwy_si_unit_new_parse(value, &power10);
        gwy_data_field_set_si_unit_xy(dfield, unit);
        g_object_unref(unit);

        q = pow10(power10);
        xreal *= q;
        yreal *= q;
        gwy_data_field_set_xreal(dfield, xreal);
        gwy_data_field_set_yreal(dfield, yreal);
    }
    else
        q = 1.0;

    if ((value = g_hash_table_lookup(hash, "x-offset")))
        gwy_data_field_set_xoffset(dfield, q*g_ascii_strtod(value, NULL));
    if ((value = g_hash_table_lookup(hash, "y-offset")))
        gwy_data_field_set_yoffset(dfield, q*g_ascii_strtod(value, NULL));

    if ((value = g_hash_table_lookup(hash, "z-unit"))) {
        unit = gwy_si_unit_new_parse(value, &power10);
        gwy_data_field_set_si_unit_z(dfield, unit);
        g_object_unref(unit);
        q = pow10(power10);
    }
    else
        q = 1.0;

    data = gwy_data_field_get_data(dfield);
    value = p;
    for (i = 0; i < xres*yres; i++) {
        data[i] = q*g_ascii_strtod(value, &p);
        value = p;
    }

    container = gwy_container_new();

    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);

    if ((value = g_hash_table_lookup(hash, "display")))
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(value));

    meta = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    value = g_strdup_printf("%04u-%02u-%02u %02u:%02u:%02u",
                            year, month, day, hour, minute, second);
    gwy_container_set_string_by_name(meta, "Date", value);

    if ((value = g_hash_table_lookup(hash, "scanspeed")))
        gwy_container_set_string_by_name(meta, "Scan Speed", g_strdup(value));

fail:
    g_free(buffer);
    g_hash_table_destroy(hash);

    return container;
#endif
    err_NO_DATA(error);
    return NULL;
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
