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

/* FIXME: Not sure where these come from, but the files tend to bear
 * `created by SPIP'.  The field names resemble BCR, but the format is not
 * the same.  So let's call the format SPIP ASCII data... */
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-spip-asc">
 *   <comment>SPIP ASCII data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="# File Format = ASCII\r\n"/>
 *   </magic>
 *   <glob pattern="*.asc"/>
 *   <glob pattern="*.ASC"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * SPIP ASCII
 * .asc
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

#define MAGIC_BARE "# File Format = ASCII"
#define MAGIC1 MAGIC_BARE "\r\n"
#define MAGIC2 MAGIC_BARE "\n"
#define MAGIC1_SIZE (sizeof(MAGIC1)-1)
#define MAGIC2_SIZE (sizeof(MAGIC2)-1)
#define EXTENSION ".asc"

#define Nanometer (1e-9)

static gboolean      module_register(void);
static gint          asc_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* asc_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports SPIP ASC files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("spip-asc",
                           N_("SPIP ASCII files (.asc)"),
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
asc_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield = NULL, *mfield = NULL;
    GwyTextHeaderParser parser;
    GwySIUnit *unit;
    gchar *line, *p, *value, *buffer = NULL;
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
    if (!gwy_strequal(line, MAGIC_BARE)) {
        err_FILE_TYPE(error, "SPIP ASCII data");
        goto fail;
    }

    gwy_clear(&parser, 1);
    parser.line_prefix = "#";
    parser.key_value_separator = "=";
    parser.terminator = "# Start of Data:";
    parser.error = &header_error;
    parser.end = &header_end;
    if (!(hash = gwy_text_header_parse(p, &parser, &p, &err))) {
        g_propagate_error(error, err);
        goto fail;
    }
    if (!require_keys(hash, error,
                      "x-pixels", "y-pixels", "x-length", "y-length",
                      NULL))
        goto fail;

    xres = atoi(g_hash_table_lookup(hash, "x-pixels"));
    yres = atoi(g_hash_table_lookup(hash, "y-pixels"));
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    xreal = Nanometer * g_ascii_strtod(g_hash_table_lookup(hash, "x-length"),
                                       NULL);
    yreal = Nanometer * g_ascii_strtod(g_hash_table_lookup(hash, "y-length"),
                                       NULL);
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

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    if ((value = g_hash_table_lookup(hash, "z-unit"))) {
        gint power10;

        unit = gwy_si_unit_new_parse(value, &power10);
        gwy_data_field_set_si_unit_z(dfield, unit);
        g_object_unref(unit);
        q = pow10(power10);
    }
    else if ((value = g_hash_table_lookup(hash, "Bit2nm"))) {
        q = Nanometer * g_ascii_strtod(value, NULL);
        unit = gwy_si_unit_new("m");
        gwy_data_field_set_si_unit_z(dfield, unit);
        g_object_unref(unit);
    }
    else
        q = 1.0;

    data = gwy_data_field_get_data(dfield);
    value = p;
    for (i = 0; i < xres*yres; i++) {
        data[i] = q*g_ascii_strtod(value, &p);
        value = p;
    }

    if ((value = g_hash_table_lookup(hash, "voidpixels")) && atoi(value)) {
        mfield = gwy_data_field_new_alike(dfield, FALSE);
        data = gwy_data_field_get_data(mfield);
        value = p;
        for (i = 0; i < xres*yres; i++) {
            data[i] = 1.0 - g_ascii_strtod(value, &p);
            value = p;
        }
        if (!gwy_app_channel_remove_bad_data(dfield, mfield))
            gwy_object_unref(mfield);
    }

    container = gwy_container_new();

    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);

    if (mfield) {
        gwy_container_set_object(container, gwy_app_get_mask_key_for_id(0),
                                 mfield);
        g_object_unref(mfield);
    }

fail:
    g_free(buffer);
    if (hash)
        g_hash_table_destroy(hash);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
