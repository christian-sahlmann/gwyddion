/*
 *  @(#) $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#define DEBUG 1
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-femtoscan-spm">
 *   <comment>FemtoScan SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\\*Surface file list\n"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # FemtoScan SPM
 * 0 string \*Surface file\ list\x0d\x0a FemtoScan SPM data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * FemtoScan SPM
 * ,spm.
 * Read
 **/
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "\\*Surface file list\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define DATA_LENGTH "\\Data length:"

static gboolean      module_register   (void);
static gint          femtoscan_detect  (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* femtoscan_load    (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyDataField* hash_to_data_field(GHashTable *hash,
                                        gchar *buffer,
                                        gsize size,
                                        GError **error);
static GwyContainer* get_metadata      (GHashTable *hash,
                                        GList *globals);
static gsize         header_length     (const guchar *buffer,
                                        gsize size);
static GList*        parse_header      (gchar *buffer,
                                        GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports FemtoScan SPM data files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("femtoscan",
                           N_("FemtoScan SPM files"),
                           (GwyFileDetectFunc)&femtoscan_detect,
                           (GwyFileLoadFunc)&femtoscan_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
femtoscan_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->head, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
femtoscan_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    GwyContainer *meta, *container = NULL;
    GError *err = NULL;
    guchar *buffer = NULL;
    G_GNUC_UNUSED const gchar *self;
    gsize size = 0, headerlen;
    GList *l, *all_sections = NULL, *globals = NULL, *images = NULL;
    gchar *header, *key, *title;
    gint id = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!(headerlen = header_length(buffer, size))) {
        err_INVALID(error, "Data length");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    header = g_memdup(buffer, headerlen+1);
    header[headerlen] = '\0';
    all_sections = parse_header(header, error);

    container = gwy_container_new();

    for (l = all_sections; l; l = g_list_next(l)) {
        GHashTable *hash = (GHashTable*)l->data;
        GwyDataField *dfield;

        self = g_hash_table_lookup(hash, "#self");
        gwy_debug("processing section %s", self);
        if (!require_keys(hash, NULL, "Samps/line", "Number of lines",
                          "Scan size", "Scan size Y",
                          "Data offset", "Data length",
                          "Image data", "stream", "Z scale",
                          NULL)) {
            globals = g_list_append(globals, hash);
            continue;
        }
        images = g_list_append(images, hash);

        dfield = hash_to_data_field(hash, buffer, size, err ? NULL : &err);
        if (!dfield)
            continue;

        gwy_container_set_object(container,
                                 gwy_app_get_data_key_for_id(id), dfield);
        g_object_unref(dfield);

        title = (gchar*)g_hash_table_lookup(hash, "Image data");
        key = g_strdup_printf("/%d/data/title", id);
        gwy_container_set_const_string_by_name(container, key, title);
        g_free(key);

        id++;
    }

    id = 0;
    for (l = images; l; l = g_list_next(l)) {
        meta = get_metadata((GHashTable*)l->data, globals);
        key = g_strdup_printf("/%d/meta", id);
        gwy_container_set_object_by_name(container, key, meta);
        g_free(key);
        g_object_unref(meta);
        id++;
    }

    for (l = all_sections; l; l = g_list_next(l))
        g_hash_table_destroy((GHashTable*)l->data);
    g_list_free(all_sections);
    g_list_free(globals);
    g_list_free(images);
    g_free(header);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (!id) {
        gwy_object_unref(container);
        if (err)
            g_propagate_error(error, err);
        else
            err_NO_DATA(error);
    }

    return container;
}

static void
add_metadata(gpointer hkey,
             gpointer hvalue,
             gpointer user_data)
{
    gchar *key = (gchar*)hkey, *val = (gchar*)hvalue;
    const gchar *prefix;
    gchar *v, *w;

    if (gwy_strequal(key, "#self"))
        return;

    prefix = (const gchar*)g_object_get_data(G_OBJECT(user_data), "prefix");
    key = g_strconcat(prefix, "::", key, NULL);

    v = g_strdup(val);
    if (strchr(v, '\272')) {
        w = gwy_strreplace(v, "\272", "deg", -1);
        g_free(v);
        v = w;
    }
    if (strchr(v, '~')) {
        w = gwy_strreplace(v, "~", "µ", -1);
        g_free(v);
        v = w;
    }
    gwy_container_set_string_by_name(GWY_CONTAINER(user_data), key, v);
    g_free(key);
}

static GwyContainer*
get_metadata(GHashTable *hash,
             GList *globals)
{
    GwyContainer *meta;
    const gchar *self;
    GList *l;

    meta = gwy_container_new();
    self = g_hash_table_lookup(hash, "#self");
    g_object_set_data(G_OBJECT(meta), "prefix", (gpointer)self);
    g_hash_table_foreach(hash, add_metadata, meta);

    for (l = globals; l; l = g_list_next(l)) {
        hash = (GHashTable*)l->data;
        self = g_hash_table_lookup(hash, "#self");
        g_object_set_data(G_OBJECT(meta), "prefix", (gpointer)self);
        g_hash_table_foreach(hash, add_metadata, meta);
    }

    g_object_set_data(G_OBJECT(meta), "prefix", NULL);

    return meta;
}

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   gchar *buffer,
                   gsize size,
                   GError **error)
{
    GwyDataField *dfield;
    GwySIUnit *unitz, *unitx, *unity;
    gchar *val, *end;
    gint xres, yres, power10, bpp = 2;
    gsize offset, datalen;
    gdouble xreal, yreal, zscale;
    gdouble *data;

    /* NB: The called must do the require_keys() check for that we can
     * just take the values from @hash! */
    offset = strtol(g_hash_table_lookup(hash, "Data offset"), NULL, 10);
    datalen = strtol(g_hash_table_lookup(hash, "Data length"), NULL, 10);
    if (offset + datalen > size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Image data are outside the file."));
        return NULL;
    }

    xres = atoi(g_hash_table_lookup(hash, "Samps/line"));
    yres = atoi(g_hash_table_lookup(hash, "Number of lines"));
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        return NULL;
    if (err_SIZE_MISMATCH(error, bpp*xres*yres, datalen, TRUE))
        return NULL;

    val = g_hash_table_lookup(hash, "Scan size");
    xreal = g_ascii_strtod(val, &end);
    if (end == val) {
        err_INVALID(error, "Scan size");
        return NULL;
    }
    val = end;
    unitx = gwy_si_unit_new_parse(val, &power10);
    xreal *= pow10(power10);
    gwy_debug("xreal = %g", xreal);

    val = g_hash_table_lookup(hash, "Scan size Y");
    yreal = g_ascii_strtod(val, &end);
    if (end == val) {
        err_INVALID(error, "Scan size Y");
        g_object_unref(unitx);
        return NULL;
    }
    val = end;
    unity = gwy_si_unit_new_parse(val, &power10);
    yreal *= pow10(power10);
    gwy_debug("yreal = %g", yreal);

    if (!gwy_si_unit_equal(unitx, unity)) {
        g_warning("X and Y units differ, using X.");
    }
    gwy_object_unref(unity);

    val = g_hash_table_lookup(hash, "Z scale");
    zscale = g_ascii_strtod(val, &end);
    if (end == val) {
        err_INVALID(error, "Z scale");
        g_object_unref(unitx);
        return NULL;
    }
    val = end;
    unitz = gwy_si_unit_new_parse(val, &power10);
    zscale *= pow10(power10);
    gwy_debug("zscale = %g", zscale);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    gwy_convert_raw_data(buffer + offset, xres*yres, 1,
                         GWY_RAW_DATA_SINT16, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         data, zscale/65536.0, 0.0);
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    gwy_data_field_set_si_unit_xy(dfield, unitx);
    g_object_unref(unitx);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    g_object_unref(unitz);

    return dfield;
}

static gsize
header_length(const guchar *buffer, gsize size)
{
    const guchar *p;
    gsize datalen;

    size = MIN(size, 4096);
    if (!(p = gwy_memmem(buffer, size, DATA_LENGTH, strlen(DATA_LENGTH))))
        return 0;

    p += strlen(DATA_LENGTH);
    datalen = strtol(p, NULL, 10);
    if (datalen >= size-1 || datalen < (p - buffer) + strlen(DATA_LENGTH) + 4)
        return 0;

    return datalen;
}

static GList*
parse_header(gchar *buffer, G_GNUC_UNUSED GError **error)
{
    GList *all_sections = NULL;
    GHashTable *hash = NULL;
    gchar *line, *colon;

    while ((line = gwy_str_next_line(&buffer))) {
        if (line[0] == '\x1a')
            break;

        g_strchomp(line);
        if (strncmp(line, "\\*", 2) == 0) {
            gwy_debug("section <%s>", line + 2);
            hash = g_hash_table_new(gwy_ascii_strcase_hash,
                                    gwy_ascii_strcase_equal);
            g_hash_table_insert(hash, "#self", line + 2);
            all_sections = g_list_append(all_sections, hash);
            continue;
        }

        /* A special field not begining with a backlash. */
        if (g_str_has_prefix(line, "stream: ")) {
            g_hash_table_insert(hash, "stream", line + strlen("stream: "));
            gwy_debug("stream <%s>", line + strlen("stream: "));
            continue;
        }

        if (line[0] != '\\') {
            g_warning("Strange line does not begin with a backslash: %s", line);
            continue;
        }
        line++;

        colon = strchr(line, ':');
        if (!colon) {
            g_warning("Strange line does not begin with a colon: %s", line);
            continue;
        }
        *colon = '\0';
        colon++;
        g_strchug(colon);
        gwy_debug("field <%s>=<%s>", line, colon);
        g_hash_table_insert(hash, line, colon);
    }

    return all_sections;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
