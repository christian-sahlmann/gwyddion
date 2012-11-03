/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-shimadzu-spm">
 *   <comment>Shimadzu SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="Shimadzu SPM File Format"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Shimadzu
 * .sph .spp .001 .002 etc.
 * Read
 **/
#define DEBUG
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

enum {
    HEADER_SIZE = 32768
};

/* These are used as type sizes, too */
typedef enum {
    SHIMADZU_SHORT = 2,
    SHIMADZU_FLOAT = 4
} ShimadzuDataType;

#define MAGIC "Shimadzu SPM File Format Version "
#define MAGIC_SIZE (sizeof(MAGIC)-1)

static gboolean      module_register      (void);
static gint          shimadzu_detect      (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* shimadzu_load        (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static GwyDataField* read_binary_data     (const gchar *buffer,
                                           gsize size,
                                           GHashTable *hash,
                                           GError **error);
static GwyDataField* read_text_data       (const gchar *buffer,
                                           gint text_data_start,
                                           GHashTable *hash,
                                           GError **error);
static GHashTable*   read_hash            (gchar *buffer,
                                           gint *text_data_start,
                                           GError **error);
static gboolean      get_scales           (GHashTable *hash,
                                           gint *xres,
                                           gint *yres,
                                           gdouble *xreal,
                                           gdouble *yreal,
                                           gdouble *xoff,
                                           gdouble *yoff,
                                           GwySIUnit *si_unit_xy,
                                           gdouble *zscale,
                                           gdouble *zoff,
                                           GwySIUnit *si_unit_z,
                                           GError **error);
static GwyContainer* shimadzu_get_metadata(GHashTable *hash);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Shimadzu SPM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti)",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("shimadzu",
                           N_("Shimadzu files"),
                           (GwyFileDetectFunc)&shimadzu_detect,
                           (GwyFileLoadFunc)&shimadzu_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
shimadzu_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= HEADER_SIZE + 2
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
shimadzu_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *meta, *container = NULL;
    GwyDataField *dfield = NULL;
    GError *err = NULL;
    gchar *buffer = NULL;
    GHashTable *hash;
    gchar *head;
    gsize size = 0;
    gboolean ok;
    gint text_data_start;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        err_TOO_SHORT(error);
        return NULL;
    }
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Shimadzu");
        g_free(buffer);
        return NULL;
    }

    head = g_memdup(buffer, HEADER_SIZE+1);
    head[HEADER_SIZE] = '\0';

    /* text_data_start is set to nonzero if data are text */
    hash = read_hash(head, &text_data_start, error);
    ok = !!hash;
    if (ok) {
        if (text_data_start)
            dfield = read_text_data(buffer, text_data_start, hash, error);
        else
            dfield = read_binary_data(buffer, size, hash, error);

        ok = !!dfield;
    }

    if (ok) {
        GQuark quark;
        const gchar *title;

        gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

        container = gwy_container_new();
        quark = gwy_app_get_data_key_for_id(0);
        gwy_container_set_object(container, quark, dfield);
        g_object_unref(dfield);

        meta = shimadzu_get_metadata(hash);
        gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);

        title = g_hash_table_lookup(hash, "Channel");
        if (title && *title)
            gwy_container_set_string_by_name(container, "/0/data/title",
                                             g_strdup(title));
        else
            gwy_app_channel_title_fall_back(container, 0);
    }

    g_free(head);
    g_free(buffer);
    g_hash_table_destroy(hash);

    return container;
}

static GwyDataField*
read_binary_data(const gchar *buffer,
                 gsize size,
                 GHashTable *hash,
                 GError **error)
{
    ShimadzuDataType data_type;
    gint xres, yres, i;
    guint expected;
    gdouble xreal, yreal, zscale, xoff, yoff, zoff;
    GwySIUnit *unitxy, *unitz;
    GwyDataField *dfield = NULL;
    gdouble *d;
    const gchar *s;

    if (!(s = g_hash_table_lookup(hash, "DataType"))) {
        err_MISSING_FIELD(error, "DataType");
        return NULL;
    }

    if (g_ascii_strcasecmp(s, "short") == 0)
        data_type = SHIMADZU_SHORT;
    else if (g_ascii_strcasecmp(s, "float") == 0)
        data_type = SHIMADZU_FLOAT;
    else {
        err_UNSUPPORTED(error, "DataType");
        return NULL;
    }

    unitxy = gwy_si_unit_new(NULL);
    unitz = gwy_si_unit_new(NULL);

    if (!get_scales(hash, &xres, &yres, &xreal, &yreal, &xoff, &yoff, unitxy,
                    &zscale, &zoff, unitz, error))
        goto fail;

    expected = data_type*xres*yres + HEADER_SIZE;
    if (err_SIZE_MISMATCH(error, expected, size, FALSE))
        goto fail;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_data_field_set_xoffset(dfield, xoff);
    gwy_data_field_set_yoffset(dfield, yoff);
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    d = gwy_data_field_get_data(dfield);

    if (data_type == SHIMADZU_SHORT) {
        const gint16 *d16 = (const gint16*)(buffer + HEADER_SIZE);

        for (i = 0; i < xres*yres; i++)
            d[i] = zscale*GUINT16_FROM_LE(d16[i]) + zoff;
    }
    else if (data_type == SHIMADZU_FLOAT) {
        const guchar *p = buffer + HEADER_SIZE;

        for (i = 0; i < xres*yres; i++)
            d[i] = zscale*gwy_get_gfloat_le(&p) + zoff;
    }
    else {
        g_assert_not_reached();
    }

fail:
    g_object_unref(unitxy);
    g_object_unref(unitz);
    return dfield;
}

static GwyDataField*
read_text_data(const gchar *buffer,
               gint text_data_start,
               GHashTable *hash,
               GError **error)
{
    const gchar *p;
    gchar *end;
    gint xres, yres, i, power10;
    gdouble xreal, yreal, zscale, xoff, yoff, zoff;
    GwySIUnit *unitxy, *unitz;
    GwyDataField *dfield = NULL;
    gdouble *d;

    unitxy = gwy_si_unit_new(NULL);
    unitz = gwy_si_unit_new(NULL);

    if (!get_scales(hash, &xres, &yres, &xreal, &yreal, &xoff, &yoff, unitxy,
                    &zscale, &zoff, unitz, error))
        goto fail;

    p = g_hash_table_lookup(hash, "DATA Unit");
    gwy_si_unit_set_from_string_parse(unitz, p, &power10);
    zscale = pow10(power10);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_data_field_set_xoffset(dfield, xoff);
    gwy_data_field_set_yoffset(dfield, yoff);
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    d = gwy_data_field_get_data(dfield);

    p = (const gchar*)buffer + text_data_start;
    for (i = 0; i < xres*yres; i++) {
        d[i] = zscale*g_ascii_strtod(p, &end) + zoff;
        if (end == p) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Cannot parse data values after %d of %d."),
                        i, xres*yres);
            gwy_object_unref(dfield);
            goto fail;
        }
        p = end + (*end == ',');
    }

fail:
    g_object_unref(unitxy);
    g_object_unref(unitz);
    return dfield;
}

static GHashTable*
read_hash(gchar *buffer,
          gint *text_data_start,
          GError **error)
{
    enum {
        WHATEVER = 0,
        PROCESS_PROFILE,
        COMMENT,
    } next_is;
    GHashTable *hash;
    gchar *p, *line, *value;
    GString *key;

    *text_data_start = 0;
    p = buffer;
    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    line = gwy_str_next_line(&p);
    key = g_string_new(NULL);

    g_hash_table_insert(hash, g_strdup("Version"), line + MAGIC_SIZE);
    next_is = WHATEVER;
    while ((line = gwy_str_next_line(&p))) {
        guint len;
        gchar *rb;

        if (line[0] == '/')
            line++;

        if (line[0] == '\x1a') {
            /* Apparently a binary data marker */
            *text_data_start = 0;
            break;
        }

        g_strstrip(line);
        /* sections */
        if (line[0] == '[' && (rb = strchr(line, ']'))) {
            *rb = '\0';
            line++;
            g_strstrip(line);
            gwy_debug("section %s", line);
            g_string_assign(key, line);
            g_string_append(key, "::");
            if (gwy_strequal(line, "PROCESS PROFILE")) {
                next_is = PROCESS_PROFILE;
                continue;
            }
            if (gwy_strequal(line, "COMMENT")) {
                next_is = COMMENT;
                continue;
            }
            if (g_str_has_prefix(line, "DATA ")) {
                line += strlen("DATA");
                *text_data_start = p - buffer;
                break;
            }
            next_is = WHATEVER;
            /* Other sectioning seems too be uninteresting. */
            continue;
        }

        if (next_is == PROCESS_PROFILE) {
            g_hash_table_insert(hash, g_strdup("ProcessProfile"), line);
            next_is = WHATEVER;
            continue;
        }
        if (next_is == COMMENT) {
            g_hash_table_insert(hash, g_strdup("Comment"), line);
            next_is = WHATEVER;
            continue;
        }

        next_is = WHATEVER;
        value = strchr(line, ':');
        if (!value) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Missing colon in header line."));
            g_hash_table_destroy(hash);
            return NULL;
        }
        *value = '\0';
        value++;
        g_strstrip(line);
        g_strstrip(value);
        len = key->len;
        g_string_append(key, line);
        gwy_debug("%s = %s", key->str, value);
        g_hash_table_replace(hash, g_strdup(key->str), value);
        g_string_truncate(key, len);
    }

    if (*text_data_start) {
        g_strstrip(line);
        if (!g_str_has_prefix(line, "Unit(") || !g_str_has_suffix(line, ")")) {
            g_warning("Cannot parse DATA unit: %s", line);
            g_hash_table_insert(hash, g_strdup("DATA Unit"), "1");
        }
        else {
            line += strlen("Unit(");
            line[strlen(line)-1] = '\0';
            g_hash_table_insert(hash, g_strdup("DATA Unit"), line);
        }
    }

    return hash;
}

static gboolean
get_scales(GHashTable *hash,
           gint *xres, gint *yres,
           gdouble *xreal, gdouble *yreal,
           gdouble *xoff, gdouble *yoff,
           GwySIUnit *si_unit_xy,
           gdouble *zscale,
           gdouble *zoff,
           GwySIUnit *si_unit_z,
           GError **error)
{
    GwySIUnit *unit;
    gint power10, zp;
    gchar *p;
    gboolean has_unit = FALSE;

    /* Dimensions are mandatory. */
    if (!require_keys(hash, error,
                      "SCANNING PARAMS::PixelsX",
                      "SCANNING PARAMS::PixelsY",
                      "SCANNING PARAMS::PixelsZ",
                      "SCANNING PARAMS::SizeX",
                      "SCANNING PARAMS::SizeY",
                      "SCANNING PARAMS::SizeZ",
                      NULL))
        return FALSE;

    *xres = atoi(g_hash_table_lookup(hash, "SCANNING PARAMS::PixelsX"));
    if (err_DIMENSION(error, *xres))
        return FALSE;
    *yres = atoi(g_hash_table_lookup(hash, "SCANNING PARAMS::PixelsY"));
    if (err_DIMENSION(error, *yres))
        return FALSE;

    unit = gwy_si_unit_new(NULL);

    p = g_hash_table_lookup(hash, "SCANNING PARAMS::SizeX");
    *xreal = fabs(g_ascii_strtod(p, &p));
    if (!*xreal) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        *xreal = 1.0;
    }
    gwy_si_unit_set_from_string_parse(si_unit_xy, p, &power10);
    *xreal *= pow10(power10);

    p = g_hash_table_lookup(hash, "SCANNING PARAMS::SizeY");
    *yreal = fabs(g_ascii_strtod(p, &p));
    if (!*yreal) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        *yreal = 1.0;
    }
    gwy_si_unit_set_from_string_parse(unit, p, &power10);
    *yreal *= pow10(power10);
    if (!gwy_si_unit_equal(unit, si_unit_xy)) {
        g_warning("X and Y units differ, using X");
    }

    zp = atoi(g_hash_table_lookup(hash, "SCANNING PARAMS::PixelsZ"));
    if (!zp) {
        g_warning("Z pixels is 0, fixing to 1");
        zp = 1;
    }
    p = g_hash_table_lookup(hash, "SCANNING PARAMS::SizeZ");
    *zscale = g_ascii_strtod(p, &p);
    gwy_si_unit_set_from_string_parse(si_unit_z, p, &power10);
    *zscale *= pow10(power10)/zp;
    /* XXX: Version may have UNIT section.  Not sure how to use it. This seems
     * wrong. */
    if ((p = g_hash_table_lookup(hash, "UNIT::Unit"))) {
        has_unit = TRUE;
        gwy_si_unit_set_from_string_parse(si_unit_z, p, &power10);
        *zscale *= pow10(power10);
        if ((p = g_hash_table_lookup(hash, "UNIT::Conv")))
            *zscale *= g_ascii_strtod(p, NULL);
    }

    /* Offsets are optional. */
    *xoff = 0.0;
    if ((p = g_hash_table_lookup(hash, "SCANNING PARAMS::OffsetX"))) {
        *xoff = g_ascii_strtod(p, &p);
        gwy_si_unit_set_from_string_parse(unit, p, &power10);
        if (gwy_si_unit_equal(unit, si_unit_xy))
            *xoff *= pow10(power10);
        else {
            g_warning("X offset units differ from X size units, ignoring.");
            *xoff = 0.0;
        }
    }

    *yoff = 0.0;
    if ((p = g_hash_table_lookup(hash, "SCANNING PARAMS::OffsetY"))) {
        *yoff = g_ascii_strtod(p, &p);
        gwy_si_unit_set_from_string_parse(unit, p, &power10);
        if (gwy_si_unit_equal(unit, si_unit_xy))
            *yoff *= pow10(power10);
        else {
            g_warning("Y offset units differ from Y size units, ignoring.");
            *yoff = 0.0;
        }
    }

    *zoff = 0.0;
    // Don't know what to do with the offset when UNIT section is present.
    // It seems to be always 0 in wrong units, so skip it.
    if (!has_unit) {
        if ((p = g_hash_table_lookup(hash, "SCANNING PARAMS::OffsetZ"))) {
            *zoff = g_ascii_strtod(p, &p);
            gwy_si_unit_set_from_string_parse(unit, p, &power10);
            if (gwy_si_unit_equal(unit, si_unit_z))
                *zoff *= pow10(power10);
            else {
                g_warning("Z offset units differ from Z size units, ignoring.");
                *zoff = 0.0;
            }
        }
    }

    g_object_unref(unit);

    return TRUE;
}

static void
add_metadata(gpointer hkey,
             gpointer hvalue,
             gpointer user_data)
{
    const gchar *key = (const gchar*)hkey, *value = (const gchar*)hvalue;
    GwyContainer *meta = (GwyContainer*)user_data;

    if (g_utf8_validate(key, -1, NULL) && g_utf8_validate(value, -1, NULL))
        gwy_container_set_string_by_name(meta, key, g_strdup(value));
}

static GwyContainer*
shimadzu_get_metadata(GHashTable *hash)
{
    GwyContainer *meta = gwy_container_new();
    g_hash_table_foreach(hash, add_metadata, meta);
    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
