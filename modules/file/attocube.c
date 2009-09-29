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

/* The format is quite similar to SPIP-ASC, however, there are enough
 * differences to warrant another module. */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-attocube-asc">
 *   <comment>Attocube ASCII data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="# Daisy frame view snapshot"/>
 *   </magic>
 *   <glob pattern="*.asc"/>
 *   <glob pattern="*.ASC"/>
 * </mime-type>
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

#define MAGIC_BARE "# Daisy frame view snapshot"
#define MAGIC1 MAGIC_BARE "\r\n"
#define MAGIC2 MAGIC_BARE "\n"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2_SIZE (sizeof(MAGIC2)-1)
#define DATA_MAGIC "# Start of Data:"
#define EXTENSION ".asc"

static gboolean      module_register(void);
static gint          asc_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* asc_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Attocube Systems ASC files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("attocube",
                           N_("Attocube ASCII files (.asc)"),
                           (GwyFileDetectFunc)&asc_detect,
                           (GwyFileLoadFunc)&asc_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
asc_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < MAX(MAGIC1_SIZE, MAGIC2_SIZE)
        || (memcmp(fileinfo->head, MAGIC1, MAGIC1_SIZE) != 0
            && memcmp(fileinfo->head, MAGIC2, MAGIC2_SIZE) != 0))
        return 0;

    return 100;
}

static gboolean
lowercase_value(G_GNUC_UNUSED const GwyTextHeaderContext *context,
                GHashTable *hash,
                gchar *key,
                gchar *value,
                G_GNUC_UNUSED gpointer user_data,
                G_GNUC_UNUSED GError **error)
{
    gchar *p;

    for (p = value; *p; p++)
        *p = g_ascii_tolower(*p);
    g_hash_table_replace(hash, key, value);

    return TRUE;
}

static GwyContainer*
asc_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
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
    if (!gwy_strequal(line, MAGIC_BARE)) {
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
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
