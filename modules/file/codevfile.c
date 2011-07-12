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
#define DEBUG 1
/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-code-v-int">
 *   <comment>Code V interferogram data</comment>
 *   <glob pattern="*.int"/>
 *   <glob pattern="*.INT"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Code V interferogram data
 * .int
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

#define EXTENSION ".int"

static gboolean      module_register(void);
static gint          int_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* int_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Code V interferogram files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("codevfile",
                           N_("Code V interferogram files (.int)"),
                           (GwyFileDetectFunc)&int_detect,
                           (GwyFileLoadFunc)&int_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
int_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    const gchar *p = fileinfo->head;
    gboolean expect_magic = FALSE;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    do {
        if (expect_magic) {
            guint x, y;
            return 100*(sscanf(p, "GRD %u %u ", &x, &y) == 2);
        }
        /* Not a comment means the title and the GRD-line should follow then. */
        if (*p != '!')
            expect_magic = TRUE;

    } while ((p = strstr(p, "\r\n")) && (p += 2));

    return 0;
}

static gchar**
split_line_in_place(gchar *line,
                    gchar delim)
{
    gchar **strs;
    guint i, n = 0;

    for (i = 0; line[i]; i++) {
        if ((!i || line[i-1] == delim) && (line[i] && line[i] != delim))
            n++;
    }

    strs = g_new(gchar*, n+1);
    n = 0;
    for (i = 0; line[i]; i++) {
        if ((!i || line[i-1] == delim || !line[i-1])
            && (line[i] && line[i] != delim))
            strs[n++] = line + i;
        else if (i && line[i] == delim && line[i-1] != delim)
            line[i] = '\0';
    }
    strs[n] = NULL;

#ifdef DEBUG
    for (i = 0; strs[i]; i++)
        gwy_debug("%u: <%s>", i, strs[i]);
#endif

    return strs;
}

static GwyContainer*
int_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL, *meta;
    GwyDataField *dfield = NULL;
    GwySIUnit *unit;
    gchar *line, *p, *type, *title, *end, *buffer = NULL;
    gchar **fields = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal, q;
    gint i, xres, yres, power10, no_data_value = 32768;
    guint fi;
    gdouble scale_size, wavelength, x_scale = 1.0;
    gboolean nearest_neighbour = FALSE;
    gdouble *data;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    /* Skip comments. */
    p = buffer;
    for (line = gwy_str_next_line(&p);
         line && line[0] == '!';
         line = gwy_str_next_line(&p)) {
        gwy_debug("comment <%s>", line);
    }
    if (!line) {
        err_FILE_TYPE(error, "Code V INT");
        goto fail;
    }

    /* The title. */
    title = line;
    if (!(line = gwy_str_next_line(&p))) {
        err_FILE_TYPE(error, "Code V INT");
        goto fail;
    }
    gwy_debug("title <%s>", title);

    fields = split_line_in_place(line, ' ');
    if (!fields
        || g_strv_length(fields) < 8
        || !gwy_strequal(fields[0], "GRD")
        || !(xres = atoi(fields[1]))
        || !(yres = atoi(fields[2]))
        || !gwy_stramong(fields[3], "SUR", "WFR", "FIL", NULL)
        || !gwy_strequal(fields[4], "WVL")
        || (!(wavelength = g_ascii_strtod(fields[5], &end))
              && end == fields[5])) {
        err_FILE_TYPE(error, "Code V INT");
        goto fail;
    }
    type = fields[3];
    gwy_debug("type <%s>", type);
    gwy_debug("xres %d, yres %d", xres, yres);
    gwy_debug("wavelength %g", wavelength);
    fi = 6;
    if (gwy_strequal(fields[fi], "NNB")) {
        nearest_neighbour = TRUE;
        fi++;
    }
    gwy_debug("nearest_neighbour %d", nearest_neighbour);

    if (!fields[fi] || !gwy_strequal(fields[fi], "SSZ")) {
        err_FILE_TYPE(error, "Code V INT");
        goto fail;
    }
    fi++;
    if (!(scale_size = g_ascii_strtod(fields[fi], &end)) && end == fields[fi]) {
        err_FILE_TYPE(error, "Code V INT");
        goto fail;
    }
    gwy_debug("scale_size %g", scale_size);
    fi++;

    if (fields[fi] && gwy_strequal(fields[fi], "NDA")) {
        fi++;
        if (!fields[fi]) {
            err_FILE_TYPE(error, "Code V INT");
            goto fail;
        }
        no_data_value = atoi(fields[fi]);
        fi++;
    }
    gwy_debug("no_data_value %d", no_data_value);

    if (fields[fi] && gwy_strequal(fields[fi], "XSC")) {
        fi++;
        if (!fields[fi]) {
            err_FILE_TYPE(error, "Code V INT");
            goto fail;
        }
        if (!(x_scale = g_ascii_strtod(fields[fi], &end))
            && end == fields[fi]) {
            err_FILE_TYPE(error, "Code V INT");
            goto fail;
        }
        fi++;
    }
    gwy_debug("x_scale %g", x_scale);

    /* There may be more stuff but we do not know anything about it. */

    //dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);

#if 0
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
#endif

    err_NO_DATA(error);

fail:
    g_free(fields);
    g_free(buffer);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
