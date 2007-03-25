/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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
 * <mime-type type="application/x-nanoscope-ii-spm">
 *   <comment>Nanoscope II SPM data</comment>
 *   <magic priority="50">
 *     <match type="string" offset="0" value="Data_File_Type 7\r\n"/>
 *   </magic>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC "Data_File_Type 7\r\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define Nanometer (1e-9)

enum { HEADER_SIZE = 2048 };

typedef struct {
    GHashTable *hash;
    GwyDataField *data_field;
} NanoscopeData;

static gboolean        module_register       (void);
static gint            nanoscope_detect      (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*   nanoscope_load        (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static GwyDataField*   hash_to_data_field    (GHashTable *hash,
                                              const guchar *buffer,
                                              gsize size,
                                              GError **error);
static GHashTable*     read_hash             (gchar *buffer,
                                              GError **error);
static gboolean        require_keys          (GHashTable *hash,
                                              GError **error,
                                              ...);
static GwyContainer*   nanoscope_get_metadata(GHashTable *hash);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Digital Instruments Nanoscope II data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanoscope-ii",
                           N_("Nanoscope II files"),
                           (GwyFileDetectFunc)&nanoscope_detect,
                           (GwyFileLoadFunc)&nanoscope_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nanoscope_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && (memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        && fileinfo->file_size > HEADER_SIZE)
        score = 100;

    return score;
}

static GwyContainer*
nanoscope_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    GwyContainer *meta, *container = NULL;
    GwyDataField *dfield;
    GError *err = NULL;
    guchar *buffer = NULL;
    gchar *header;
    gsize size = 0;
    GHashTable *hash;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size <= HEADER_SIZE || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Nanoscope II");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    header = g_memdup(buffer, HEADER_SIZE + 1);
    header[HEADER_SIZE] = '\0';
    hash = read_hash(header + MAGIC_SIZE, error);
    /* Must keep header allocated, hash items are direct pointers to it */
    if (!hash) {
        g_free(header);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = hash_to_data_field(hash, buffer + HEADER_SIZE, size - HEADER_SIZE,
                                &err);
    meta = nanoscope_get_metadata(hash);
    gwy_file_abandon_contents(buffer, size, NULL);
    g_hash_table_destroy(hash);
    g_free(header);
    if (!dfield)
        return NULL;

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    if (meta) {
        gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);
    }

    gwy_app_channel_title_fall_back(container, 0);

    return container;
}

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   const guchar *buffer,
                   gsize size,
                   GError **error)
{
    GwyDataField *dfield;
    GwySIUnit *unitz, *unitxy;
    gchar *val;
    gint xres, yres, i, j;
    gdouble xreal, yreal, q;
    gdouble *data;
    const gint16 *d16;

    if (!require_keys(hash, error, "num_samp", "scan_sz", "z_scale", NULL))
        return NULL;

    val = g_hash_table_lookup(hash, "num_samp");
    xres = yres = atoi(val);
    if (err_DIMENSION(error, xres))
        return NULL;

    if (err_SIZE_MISMATCH(error, xres*yres*sizeof(gint16), size, FALSE))
        return NULL;

    val = g_hash_table_lookup(hash, "scan_sz");
    xreal = yreal = Nanometer*g_ascii_strtod(val, NULL);
    if (xreal <= 0) {
        err_INVALID(error, "scan_sz");
        return NULL;
    }

    val = g_hash_table_lookup(hash, "z_scale");
    q = Nanometer/16384.0*g_ascii_strtod(val, NULL);
    if (q <= 0.0) {
        err_INVALID(error, "scan_sz");
        return NULL;
    }
    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    d16 = (const gint16*)buffer;

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++)
            data[(yres-1 - i)*xres + j] = q*GINT16_FROM_LE(d16[i*xres + j]);
    }

    unitz = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, unitz);
    g_object_unref(unitz);

    unitxy = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    g_object_unref(unitxy);

    return dfield;
}

static GHashTable*
read_hash(gchar *buffer,
          GError **error)
{
    GHashTable *hash;
    gchar *line, *value;

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    while ((line = gwy_str_next_line(&buffer)) && line[0] != '\x1a') {
        g_strstrip(line);
        if (!line[0])
            continue;

        gwy_debug("<%s>", line);
        value = strchr(line, '=');
        if (!value) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Malformed header line (missing =)."));
            g_hash_table_destroy(hash);

            return NULL;
        }
        *value = '\0';
        g_strstrip(line);

        value++;
        g_strstrip(value);
        if (*value)
            g_hash_table_insert(hash, line, value);
    }

    return hash;
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

static GwyContainer*
nanoscope_get_metadata(GHashTable *hash)
{
    static const struct {
        gchar *key;
        gchar *name;
        gchar *unit;
    }
    metadata[] = {
        { "bias_volt[0]", "Bias", " mV" },
        { "scan_rate", "Scan rate", " Hz" },
        { "time", "Date", NULL },
    };

    GwyContainer *meta;
    const gchar *val;
    guint i;

    meta = gwy_container_new();
    for (i = 0; i < G_N_ELEMENTS(metadata); i++) {
        if ((val = g_hash_table_lookup(hash, metadata[i].key)))
            gwy_container_set_string_by_name(meta, metadata[i].name,
                                             g_strconcat(val, metadata[i].unit,
                                                         NULL));
    }
    if (gwy_container_get_n_items(meta))
        return meta;

    g_object_unref(meta);
    return NULL;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
