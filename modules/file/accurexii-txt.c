/*
 *  $Id$
 *  Copyright (C) 2014 David Necas (Yeti).
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
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-accurexii-txt">
 *   <comment>Accurex II text data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="[Header Section]">
 *       <match type="string" offset="16:240" value="Stage Type"/>
 *     </math>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Accurex II text data
 * .txt
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "[Header Section]"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define MAGIC2 "Stage Type"
#define MAGIC3 "Probe Type"
#define DATA_MAGIC "[Data Section]"
#define EXTENSION ".txt"

static gboolean      module_register(void);
static gint          acii_detect    (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* acii_load      (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static void          add_meta       (gpointer hkey,
                                     gpointer hvalue,
                                     gpointer user_data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Accurex II text files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("accurexii-txt",
                           N_("Accurex II text files (.txt)"),
                           (GwyFileDetectFunc)&acii_detect,
                           (GwyFileLoadFunc)&acii_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
acii_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    if (!strstr(fileinfo->head, MAGIC2)
        || !strstr(fileinfo->head, MAGIC3))
        return 0;

    return 90;
}

static GwyContainer*
acii_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL, *meta;
    GwyDataField *dfield = NULL;
    GwySIUnit *xunit, *yunit, *zunit;
    gchar *end, *p, *value, *header = NULL, *buffer = NULL;
    gchar **dims;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal, q;
    guint i, xres, yres;
    gint power10x, power10y, power10z;
    gdouble *data;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    /* This one requires no string search. */
    if (memcmp(buffer, MAGIC, MAGIC_SIZE)) {
        err_FILE_TYPE(error, "Accurex II TXT");
        goto fail;
    }

    if (!(p = strstr(buffer + MAGIC_SIZE, DATA_MAGIC))) {
        err_FILE_TYPE(error, "Accurex II TXT");
        goto fail;
    }

    header = g_convert(buffer + MAGIC_SIZE, p - buffer - MAGIC_SIZE,
                       "UTF-8", "ISO-8859-1", NULL, NULL, &err);
    if (!header) {
        /* XXX: Can this actually happen? */
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header cannot be converted from ISO-8859-1 "
                      "character set: %s"),
                    err->message);
        g_clear_error(&err);
        goto fail;
    }

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    hash = gwy_text_header_parse(header, &parser, NULL, NULL);

    if (!require_keys(hash, error, "Image Size", "Image Resolution", NULL))
        goto fail;


    value = (gchar*)g_hash_table_lookup(hash, "Image Resolution");
    gwy_debug("imgres <%s>", value);
    if (sscanf(value, "%u x %u", &xres, &yres) != 2) {
        err_INVALID(error, "Image Resolution");
        goto fail;
    }
    gwy_debug("xres %u, yres %u", xres, yres);
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    value = (gchar*)g_hash_table_lookup(hash, "Image Size");
    dims = g_strsplit(value, "x", 0);
    if (!dims || g_strv_length(dims) != 2) {
        g_strfreev(dims);
        err_INVALID(error, "Image Size");
        goto fail;
    }
    xreal = g_ascii_strtod(dims[0], &end);
    xunit = gwy_si_unit_new_parse(end, &power10x);
    yreal = g_ascii_strtod(dims[1], &end);
    yunit = gwy_si_unit_new_parse(end, &power10y);
    g_strfreev(dims);

    /* Use negated positive conditions to catch NaNs */
    if (!(xreal > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!(yreal > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }
    if (!gwy_si_unit_equal(xunit, yunit)) {
        g_warning("X and Y units differ, using X");
    }

    xreal *= pow10(power10x);
    yreal *= pow10(power10y);

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_serializable_clone(G_OBJECT(xunit),
                           G_OBJECT(gwy_data_field_get_si_unit_xy(dfield)));

    g_object_unref(xunit);
    g_object_unref(yunit);

    p += strlen(DATA_MAGIC);
    while (g_ascii_isspace(*p))
        p++;

    q = 1.0;
    if (g_str_has_prefix(p, "Z-unit:")) {
        value = gwy_str_next_line(&p);
        zunit = gwy_si_unit_new_parse(value + strlen("Z unit:"), &power10z);
        gwy_serializable_clone(G_OBJECT(zunit),
                               G_OBJECT(gwy_data_field_get_si_unit_z(dfield)));
        g_object_unref(zunit);
        q *= pow10(power10z);
    }

    data = gwy_data_field_get_data(dfield);
    value = p;
    for (i = 0; i < xres*yres; i++) {
        data[i] = q*g_ascii_strtod(value, &p);
        value = p;
    }

    container = gwy_container_new();

    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);

    if ((value = g_hash_table_lookup(hash, "Data Type")))
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(value));

    meta = gwy_container_new();
    g_hash_table_foreach(hash, add_meta, meta);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    gwy_file_channel_import_log_add(container, 0, "accurexii-txt", filename);

fail:
    g_free(header);
    g_free(buffer);
    if (hash)
        g_hash_table_destroy(hash);

    return container;
}

static void
add_meta(gpointer hkey, gpointer hvalue, gpointer user_data)
{
    gwy_container_set_string_by_name(GWY_CONTAINER(user_data),
                                     (gchar*)hkey,
                                     g_strdup((gchar*)hvalue));
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
