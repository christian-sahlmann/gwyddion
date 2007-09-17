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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-shimadzu-spm">
 *   <comment>Shimadzu SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="Shimadzu SPM File Format"/>
 *   </magic>
 * </mime-type>
 **/
#define DEBUG 1
#include "config.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

enum {
    HEADER_SIZE = 32768
};

#define MAGIC "Shimadzu SPM File Format Version 2."
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
static gboolean      require_keys         (GHashTable *hash,
                                           GError **error,
                                           ...);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Shimadzu SPM data files, version 2."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
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
    gint xres, yres, i;
    guint expected;
    gdouble xreal, yreal, zscale, xoff, yoff, zoff;
    GwySIUnit *unitxy, *unitz;
    GwyDataField *dfield = NULL;
    const gint16 *d16;
    gdouble *d;

    unitxy = gwy_si_unit_new(NULL);
    unitz = gwy_si_unit_new(NULL);

    if (!get_scales(hash, &xres, &yres, &xreal, &yreal, &xoff, &yoff, unitxy,
                    &zscale, &zoff, unitz, error))
        goto fail;

    expected = 2*xres*yres + HEADER_SIZE;
    if (err_SIZE_MISMATCH(error, expected, size, TRUE))
        goto fail;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_data_field_set_xoffset(dfield, xoff);
    gwy_data_field_set_yoffset(dfield, yoff);
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    d = gwy_data_field_get_data(dfield);
    d16 = (const gint16*)(buffer + HEADER_SIZE);

    for (i = 0; i < xres*yres; i++)
        d[i] = zscale*GINT16_FROM_LE(d16[i]) + zoff;

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

    *text_data_start = 0;
    p = buffer;
    hash = g_hash_table_new(g_str_hash, g_str_equal);
    line = gwy_str_next_line(&p);

    g_hash_table_insert(hash, "Version", line + MAGIC_SIZE-2);
    next_is = WHATEVER;
    while ((line = gwy_str_next_line(&p))) {
        gint llen;
        gchar *rb;

        if (line[0] == '/')
            line++;

        if (line[0] == '\x1a') {
            /* Apparently a binary data marker */
            *text_data_start = 0;
            break;
        }

        g_strstrip(line);
        llen = strlen(line);
        /* sections */
        if (line[0] == '[' && (rb = strchr(line, ']'))) {
            *rb = '\0';
            line++;
            g_strstrip(line);
            gwy_debug("section %s", line);
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
            g_hash_table_insert(hash, "ProcessProfile", line);
            next_is = WHATEVER;
            continue;
        }
        if (next_is == COMMENT) {
            g_hash_table_insert(hash, "Comment", line);
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
        gwy_debug("%s = %s", line, value);
        g_hash_table_insert(hash, line, value);
    }

    if (*text_data_start) {
        g_strstrip(line);
        if (!g_str_has_prefix(line, "Unit(") || !g_str_has_suffix(line, ")")) {
            g_warning("Cannot parse DATA unit: %s", line);
            g_hash_table_insert(hash, "DATA Unit", "1");
        }
        else {
            line += strlen("Unit(");
            line[strlen(line)-1] = '\0';
            g_hash_table_insert(hash, "DATA Unit", line);
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

    /* Dimensions are mandatory. */
    if (!require_keys(hash, error,
                      "PixelsX", "PixelsY", "PixelsZ",
                      "SizeX", "SizeY", "SizeZ",
                      NULL))
        return FALSE;

    *xres = atoi(g_hash_table_lookup(hash, "PixelsX"));
    if (err_DIMENSION(error, *xres))
        return FALSE;
    *yres = atoi(g_hash_table_lookup(hash, "PixelsY"));
    if (err_DIMENSION(error, *yres))
        return FALSE;

    unit = gwy_si_unit_new(NULL);

    p = g_hash_table_lookup(hash, "SizeX");
    *xreal = fabs(g_ascii_strtod(p, &p));
    if (!*xreal) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        *xreal = 1.0;
    }
    gwy_si_unit_set_from_string_parse(si_unit_xy, p, &power10);
    *xreal *= pow10(power10);

    p = g_hash_table_lookup(hash, "SizeY");
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

    zp = atoi(g_hash_table_lookup(hash, "PixelsZ"));
    if (!zp) {
        g_warning("Z pixels is 0, fixing to 1");
        zp = 1;
    }
    p = g_hash_table_lookup(hash, "SizeZ");
    *zscale = g_ascii_strtod(p, &p);
    gwy_si_unit_set_from_string_parse(si_unit_z, p, &power10);
    *zscale *= pow10(power10)/zp;

    /* Offsets are optional. */
    *xoff = 0.0;
    if ((p = g_hash_table_lookup(hash, "OffsetX"))) {
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
    if ((p = g_hash_table_lookup(hash, "OffsetY"))) {
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
    if ((p = g_hash_table_lookup(hash, "OffsetZ"))) {
        *zoff = g_ascii_strtod(p, &p);
        gwy_si_unit_set_from_string_parse(unit, p, &power10);
        if (!gwy_si_unit_equal(unit, si_unit_z))
            *zoff *= pow10(power10);
        else {
            g_warning("Z offset units differ from Z size units, ignoring.");
            *zoff = 0.0;
        }
    }

    g_object_unref(unit);

    return TRUE;
}

/* FIXME: This is rough, we should fix/special-case some values.
 * Avoid Name and Microscope, apparently contain crap. */
static GwyContainer*
shimadzu_get_metadata(GHashTable *hash)
{
    static const gchar keys[] =
        "DataName\0GroupName\0CurrentRange\0Angle\0Rate\0Comment\0"
        "Direction\0OperatingPoint\0IntegralGain\0ProportionalGain\0"
        "SamplingFrequency\0Mode\0Channel\0Version\0ProcessProfile\0"
        "VoltageRangeX\0VoltageRangeY\0VoltageRangeZ\0"
        "MaxRangeX\0MaxRangeY\0MaxRangeZ\0"
        "SensitivityX\0SensitivityY\0SensitivityZ\0"
        "SizeGainX\0SizeGainY\0SizeGainZ\0";

    GwyContainer *meta;
    const gchar *k, *v;

    meta = gwy_container_new();

    for (k = keys; *k; k += strlen(k)+1) {
        v = g_hash_table_lookup(hash, k);
        if (v && *v)
            gwy_container_set_string_by_name(meta, k, g_strdup(v));
    }

    return meta;
}

static gboolean
require_keys(GHashTable *hash,
             GError **error,
             ...)
{
    va_list ap;
    const gchar *key;

    va_start(ap, error);
    while ((key = va_arg(ap, const gchar *))) {
        if (!g_hash_table_lookup(hash, key)) {
            err_MISSING_FIELD(error, key);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
