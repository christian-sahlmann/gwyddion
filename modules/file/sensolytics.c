/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

/* 1.1 = linescans, 1.2 = image, 1.3 = potentiostat, 1.4 = freq. spectra.
 * At present we import just images. */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-sensolytics-spm">
 *   <comment>Sensolytics SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="# Sensolytics: 1.2\r"/>
 *     <match type="string" offset="0" value="# Sensolytics: 1.2\n"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Sensolytics DAT
 * .dat
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

/* Not a real magic header, but should catch the stuff */
#define MAGIC "# Sensolytics: 1.2"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".dat"

#define Micrometer 1e-6

typedef struct {
    gint xres, yres;
    gdouble xreal, yreal;
} Dimensions;

typedef struct {
    GwyDataField *dfield;
    gdouble *data;
    const gchar *name;
    gdouble q;
} SensolyticsChannel;

static gboolean            module_register(void);
static gint                sly_detect     (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer*       sly_load       (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static gboolean            read_dimensions(GHashTable *hash,
                                           gint *ndata,
                                           Dimensions *dimensions,
                                           GError **error);
static SensolyticsChannel* create_fields  (GHashTable *hash,
                                           gchar *line,
                                           gint ndata,
                                           const Dimensions *dimensions);
static GwyContainer*       get_meta       (GHashTable *hash);
static void                clone_meta     (GwyContainer *container,
                                           GwyContainer *meta,
                                           guint nchannels);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Sensolytics text files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sensolytics",
                           N_("Sensolytics text files (.dat)"),
                           (GwyFileDetectFunc)&sly_detect,
                           (GwyFileLoadFunc)&sly_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sly_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len < MAGIC_SIZE + 2
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0
        || !g_ascii_isspace(fileinfo->head[MAGIC_SIZE]))
        return 0;

    return 100;
}

static GwyContainer*
sly_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL, *meta = NULL;
    gchar *buffer = NULL;
    GError *err = NULL;
    GHashTable *hash = NULL;
    gchar *p, *line, *value;
    guint expecting_data = 0;
    SensolyticsChannel *channels = NULL;
    Dimensions dimensions;
    gint ndata = 0, i;

    if (!g_file_get_contents(filename, &buffer, NULL, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer;
    line = gwy_str_next_line(&p);
    g_strstrip(line);
    if (!gwy_strequal(line, MAGIC)) {
        err_FILE_TYPE(error, "Sensolytics");
        goto fail;
    }

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        if (!line[0])
            continue;

        if (expecting_data) {
            expecting_data--;

            /* The columns are comma-separated and numbers use decimal points.
             * Do not tempt the number parsing functions more than necessary
             * and fix commas to tab characters. */
            g_strdelimit(line, ",", '\t');

            /* Ignore X, Y and Z, each is two values */
            for (i = 0; i < 6; i++)
                g_ascii_strtod(line, &line);

            for (i = 0; i < ndata; i++)
                channels[i].data[expecting_data]
                    = channels[i].q * g_ascii_strtod(line, &line);
        }
        else {
            g_strstrip(line);
            if (line[0] != '#') {
                g_warning("Comment line does not start with #.");
                continue;
            }

            do {
                line++;
            } while (g_ascii_isspace(*line));

            if (g_str_has_prefix(line, "X [")) {
                if (channels) {
                    g_warning("Multiple data headers!?");
                    continue;
                }

                if (!read_dimensions(hash, &ndata, &dimensions, error)
                    || !(channels = create_fields(hash, line,
                                                  ndata, &dimensions)))
                    goto fail;
                expecting_data = dimensions.xres * dimensions.yres;
                continue;
            }

            value = strchr(line, ':');
            if (!value) {
                if (!gwy_strequal(line, "ArrayScan"))
                    g_warning("Non-parameter-like line %s", line);
                continue;
            }
            *value = '\0';
            g_strchomp(line);
            do {
                value++;
            } while (g_ascii_isspace(*value));

            if (gwy_strequal(line, "Warning"))
                continue;

            gwy_debug("<%s>=<%s>", line, value);
            g_hash_table_insert(hash, line, value);
        }
    }

    if (!channels) {
        err_NO_DATA(error);
        goto fail;
    }

    container = gwy_container_new();
    for (i = 0; i < ndata; i++) {
        GQuark key = gwy_app_get_data_key_for_id(i);

        gwy_data_field_invert(channels[i].dfield, FALSE, TRUE, FALSE);
        gwy_container_set_object(container, key, channels[i].dfield);
        gwy_app_channel_check_nonsquare(container, i);
        if (channels[i].name) {
            gchar *s = g_strconcat(g_quark_to_string(key), "/title", NULL);
            gwy_container_set_string_by_name(container, s,
                                             g_strdup(channels[i].name));
            g_free(s);
        }
        else
            gwy_app_channel_title_fall_back(container, i);
    }

    meta = get_meta(hash);
    clone_meta(container, meta, ndata);
    g_object_unref(meta);

fail:
    g_free(buffer);
    if (hash)
        g_hash_table_destroy(hash);
    if (channels) {
        for (i = 0; i < ndata; i++)
            g_object_unref(channels[i].dfield);
        g_free(channels);
    }

    return container;
}

static gboolean
read_dimensions(GHashTable *hash,
                gint *ndata,
                Dimensions *dimensions,
                GError **error)
{
    const gchar *value;

    /* Number of fields */
    if (!(value = g_hash_table_lookup(hash, "Channels"))) {
        err_MISSING_FIELD(error, "Channels");
        return FALSE;
    }
    *ndata = atoi(value);
    if (*ndata <= 0 || *ndata > 1024) {
        err_INVALID(error, "Channels");
        return FALSE;
    }

    /* Pixel sizes */
    if (!(value = g_hash_table_lookup(hash, "Lines"))) {
        err_MISSING_FIELD(error, "Lines");
        return FALSE;
    }
    dimensions->yres = atoi(value);
    if (err_DIMENSION(error, dimensions->yres))
        return FALSE;

    if (!(value = g_hash_table_lookup(hash, "Rows"))) {
        err_MISSING_FIELD(error, "Rows");
        return FALSE;
    }
    /* XXX: When the file says Rows, it actually means Columns-1.  Bite me. */
    dimensions->xres = atoi(value) + 1;
    if (err_DIMENSION(error, dimensions->xres))
        return FALSE;

    /* Real sizes */
    if (!(value = g_hash_table_lookup(hash, "X-Length"))) {
        err_MISSING_FIELD(error, "X-Length");
        return FALSE;
    }
    dimensions->xreal = Micrometer * g_ascii_strtod(value, NULL);
    if (!((dimensions->xreal = fabs(dimensions->xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        dimensions->xreal = 1.0;
    }

    if (!(value = g_hash_table_lookup(hash, "Y-Length"))) {
        err_MISSING_FIELD(error, "Y-Length");
        return FALSE;
    }
    dimensions->yreal = Micrometer * g_ascii_strtod(value, NULL);
    if (!((dimensions->yreal = fabs(dimensions->yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        dimensions->yreal = 1.0;
    }

    return TRUE;
}

static SensolyticsChannel*
create_fields(GHashTable *hash,
              /* Can we obtain any useful information from this? */
              G_GNUC_UNUSED gchar *line,
              gint ndata,
              const Dimensions *dimensions)
{
    SensolyticsChannel *channels;
    GString *str;
    const gchar *value;
    GwySIUnit *unit;
    gint i, power10;

    str = g_string_new(NULL);
    channels = g_new0(SensolyticsChannel, ndata+1);
    for (i = 0; i < ndata; i++) {
        channels[i].dfield = gwy_data_field_new(dimensions->xres,
                                                dimensions->yres,
                                                dimensions->xreal,
                                                dimensions->yreal,
                                                FALSE);
        channels[i].data = gwy_data_field_get_data(channels[i].dfield);

        unit = gwy_si_unit_new("m");
        gwy_data_field_set_si_unit_xy(channels[i].dfield, unit);
        g_object_unref(unit);

        g_string_printf(str, "Channel %d Unit", i+1);
        channels[i].q = 1.0;
        if ((value = g_hash_table_lookup(hash, str->str))) {
            unit = gwy_si_unit_new_parse(value, &power10);
            gwy_data_field_set_si_unit_z(channels[i].dfield, unit);
            g_object_unref(unit);
            channels[i].q = pow10(power10);
        }
        else
            g_warning("Channel %d has no units", i+1);

        g_string_printf(str, "Channel %d Name", i+1);
        if (!(channels[i].name = g_hash_table_lookup(hash, str->str)))
            g_warning("Channel %d has no name", i+1);
    }

    return channels;
}

static GwyContainer*
get_meta(GHashTable *hash)
{
    static const gchar *keys[] = {
        "Mode", "max. Speed", "Waiting Time", "Scan started", "Scan ended",
    };
    GwyContainer *meta = gwy_container_new();
    const gchar *value;
    guint i;

    for (i = 0; i < G_N_ELEMENTS(keys); i++) {
        if (!(value = g_hash_table_lookup(hash, keys[i])))
            continue;

        gwy_container_set_string_by_name(meta, keys[i], g_strdup(value));
    }

    return meta;
}
static void
clone_meta(GwyContainer *container, GwyContainer *meta, guint nchannels)
{
    guint i;
    gchar s[32];

    if (!gwy_container_get_n_items(meta))
        return;

    /* Simply store identical metadata for each channel */
    for (i = 0; i < nchannels; i++) {
        GwyContainer *m = gwy_container_duplicate(meta);
        g_snprintf(s, sizeof(s), "/%u/meta", i);
        gwy_container_set_object_by_name(container, s, m);
        g_object_unref(m);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
