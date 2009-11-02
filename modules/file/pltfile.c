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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanosurf-plt-spm">
 *   <comment>Nanosurf PLT data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="#Channel: ">
 *       <match type="string" offset="12-32" value="#Frame  :"/>
 *     </match>
 *   </magic>
 *   <glob pattern="*.plt"/>
 *   <glob pattern="*.PLT"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nanosurf PLT
 * .plt
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

#define MAGIC1 "#Channel:"
#define MAGIC2 "#Frame  :"
#define MAGIC3 "#Lines  :"
#define MAGIC4 "#Points :"
#define EXTENSION ".plt"

static gboolean      module_register(void);
static gint          plt_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* plt_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanosurf PLT files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("pltfile",
                           N_("Nanosurf PLT files (.plt)"),
                           (GwyFileDetectFunc)&plt_detect,
                           (GwyFileLoadFunc)&plt_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
plt_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (strncmp(fileinfo->head, MAGIC1, sizeof(MAGIC1)-1) == 0
        && strstr(fileinfo->head, MAGIC2)
        && strstr(fileinfo->head, MAGIC3)
        && strstr(fileinfo->head, MAGIC4))
        return 90;

    return 0;
}

static GwyContainer*
plt_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield = NULL;
    GwyTextHeaderParser parser;
    GwySIUnit *xunit, *yunit, *zunit;
    gchar *p, *value, *buffer = NULL;
    GHashTable *hash = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal, zreal;
    gint i, xres, yres;
    gdouble *data;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    if (strncmp(buffer, MAGIC1, MIN(size, sizeof(MAGIC1)-1))) {
        err_FILE_TYPE(error, "Nanosurf PLT");
        goto fail;
    }

    /* Find the first line not starting with '#' */
    for (p = buffer; (p - buffer) + 1 < size; p++) {
        if ((p[0] == '\n' || p[0] == '\r')
            && (p[1] != '\n' && p[1] != '#')) {
            break;
        }
    }
    *p = '\0';
    p++;

    gwy_clear(&parser, 1);
    parser.line_prefix = "#";
    parser.key_value_separator = ":";
    hash = gwy_text_header_parse(buffer, &parser, NULL, NULL);
    if (!require_keys(hash, error,
                      "Channel", "Lines", "Points",
                      "XRange", "YRange", "ZRange",
                      NULL))
        goto fail;

    xres = atoi(g_hash_table_lookup(hash, "Points"));
    yres = atoi(g_hash_table_lookup(hash, "Lines"));
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    value = g_hash_table_lookup(hash, "XRange");
    xreal = g_ascii_strtod(value, &value);
    xunit = gwy_si_unit_new(value);

    value = g_hash_table_lookup(hash, "YRange");
    yreal = g_ascii_strtod(value, &value);
    yunit = gwy_si_unit_new(value);

    value = g_hash_table_lookup(hash, "ZRange");
    zreal = g_ascii_strtod(value, &value);
    zunit = gwy_si_unit_new(value);

    /* Use negated positive conditions to catch NaNs */
    if (!((xreal = fabs(xreal)) > 0)) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }
    if (!((yreal = fabs(yreal)) > 0)) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    if (!gwy_si_unit_equal(xunit, yunit))
        g_warning("X and Y units differ, using X");

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    gwy_data_field_set_si_unit_xy(dfield, xunit);
    gwy_data_field_set_si_unit_z(dfield, zunit);
    g_object_unref(xunit);
    g_object_unref(yunit);
    g_object_unref(zunit);

    data = gwy_data_field_get_data(dfield);
    value = p;
    for (i = 0; i < xres*yres; i++) {
        data[i] = g_ascii_strtod(value, &p);
        value = p;
    }

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);

    if ((value = g_hash_table_lookup(hash, "Channel")))
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(value));
    else
        gwy_app_channel_title_fall_back(container, 0);

fail:
    g_free(buffer);
    g_hash_table_destroy(hash);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
