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
 * <mime-type type="application/x-witec-ascii-export">
 *   <comment>WITec ASCII data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="//Exported ASCII-File"/>
 *   </magic>
 *   <glob pattern="*.asc"/>
 *   <glob pattern="*.ASC"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * WITec ASCII export
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

#define MAGIC "//Exported ASCII-File"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".dat"

static gboolean      module_register(void);
static gint          dat_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* dat_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports WITec ASCII export files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("witec-asc",
                           N_("WITec ASCII files (.dat)"),
                           (GwyFileDetectFunc)&dat_detect,
                           (GwyFileLoadFunc)&dat_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
dat_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static gboolean
header_error(G_GNUC_UNUSED const GwyTextHeaderContext *context,
             GError *error,
             G_GNUC_UNUSED gpointer user_data)
{
    return error->code == GWY_TEXT_HEADER_ERROR_TERMINATOR;
}

static void
header_end(G_GNUC_UNUSED const GwyTextHeaderContext *context,
           gsize length,
           gpointer user_data)
{
    gchar **pp = (gchar**)user_data;

    *pp += length;
}

static GwyContainer*
dat_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield = NULL;
    GwyTextHeaderParser parser;
    GwySIUnit *xyunit = NULL, *zunit = NULL;
    gint power10xy, power10z;
    gchar *line, *p, *value, *title, *buffer = NULL;
    GHashTable *hash = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal, q;
    gint i, xres, yres;
    gdouble *data;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    p = buffer;
    line = gwy_str_next_line(&p);
    if (!gwy_strequal(line, MAGIC)) {
        err_FILE_TYPE(error, "WITec ASCII export");
        goto fail;
    }

    line = gwy_str_next_line(&p);
    if (!line) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated"));
        goto fail;
    }
    g_strstrip(line);
    if (!gwy_strequal(line, "[Header]")) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected header start marker ‘%s’ but found ‘%s’."),
                    "[Header]", line);
        goto fail;
    }

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    parser.terminator = "[Data]";
    parser.error = &header_error;
    parser.end = &header_end;
    if (!(hash = gwy_text_header_parse(p, &parser, &p, &err))) {
        g_propagate_error(error, err);
        goto fail;
    }
    if (!require_keys(hash, error,
                      "PointsPerLine", "LinesPerImage",
                      "ScanUnit", "ScanWidth", "ScanHeight", "DataUnit",
                      NULL))
        goto fail;

    xres = atoi(g_hash_table_lookup(hash, "PointsPerLine"));
    yres = atoi(g_hash_table_lookup(hash, "LinesPerImage"));
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    xyunit = gwy_si_unit_new_parse(g_hash_table_lookup(hash, "ScanUnit"),
                                   &power10xy);
    zunit = gwy_si_unit_new_parse(g_hash_table_lookup(hash, "DataUnit"),
                                  &power10z);
    xreal = g_ascii_strtod(g_hash_table_lookup(hash, "ScanWidth"), NULL);
    yreal = g_ascii_strtod(g_hash_table_lookup(hash, "ScanHeight"), NULL);
    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }
    xreal *= pow10(power10xy);
    yreal *= pow10(power10xy);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);

    gwy_data_field_set_si_unit_xy(dfield, xyunit);
    gwy_data_field_set_si_unit_z(dfield, zunit);
    q = pow10(power10z);

    data = gwy_data_field_get_data(dfield);
    value = p;
    for (i = 0; i < xres*yres; i++) {
        data[i] = q*g_ascii_strtod(value, &p);
        if (p == value && (!*p || g_ascii_isspace(*p))) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("End of file reached when reading sample #%d of %d"),
                        i, xres*yres);
            goto fail;
        }
        if (p == value) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Malformed data encountered when reading sample "
                          "#%d of %d"),
                        i, xres*yres);
            goto fail;
        }
        value = p;
    }

    container = gwy_container_new();

    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);

    title = g_hash_table_lookup(hash, "ImageName");
    if (title) {
        guint len = strlen(title);
        if (title[0] == '"' && title[len-1] == '"') {
            title[len-1] = '\0';
            title++;
        }
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(title));
    }
    else
        gwy_app_channel_title_fall_back(container, 0);

    gwy_app_channel_check_nonsquare(container, 0);

fail:
    g_object_unref(dfield);
    g_free(buffer);
    gwy_object_unref(xyunit);
    gwy_object_unref(zunit);
    if (hash)
        g_hash_table_destroy(hash);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
