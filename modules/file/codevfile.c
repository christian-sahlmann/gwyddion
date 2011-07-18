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

typedef enum {
    CODEV_INT_SURFACE_DEFORMATION = 1,
    CODEV_INT_WAVEFRONT_DEFORMATION,
    CODEV_INT_INTENSITY_FILTER,
    CODEV_INT_COATING_THICKNESS_VARIATION,
    CODEV_INT_BIREFRINGENCE,
    CODEV_INT_CRYSTAL_AXIS_ORIENTATION,
} CodeVGridDataType;

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
    GwyDataField *dfield = NULL, *mfield = NULL;
    CodeVGridDataType type;
    gchar *line, *p, *comment, *end, *buffer = NULL;
    const gchar *unit, *title;
    gchar **fields = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal;
    gint i, xres, yres, no_data_value = 32767;
    guint fi;
    gdouble scale_size, wavelength, q = 1.0, x_scale = 1.0;
    gboolean nearest_neighbour = FALSE;
    gdouble *data, *mdata;

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
    comment = line;
    if (!(line = gwy_str_next_line(&p))) {
        err_FILE_TYPE(error, "Code V INT");
        goto fail;
    }
    gwy_debug("comment <%s>", comment);

    fields = split_line_in_place(line, ' ');
    if (!fields
        || g_strv_length(fields) < 8
        || !gwy_strequal(fields[0], "GRD")
        || !(xres = atoi(fields[1]))
        || !(yres = atoi(fields[2]))
        || !(type = gwy_stramong(fields[3],
                                 "SUR", "WFR", "FIL", "THV", "BIR", "CAO",
                                 NULL))
        || !gwy_strequal(fields[4], "WVL")
        || (!(wavelength = g_ascii_strtod(fields[5], &end))
              && end == fields[5])) {
        err_FILE_TYPE(error, "Code V INT");
        goto fail;
    }
    gwy_debug("type %u (%s)", type, fields[3]);
    gwy_debug("xres %d, yres %d", xres, yres);
    gwy_debug("wavelength %g", wavelength);
    fi = 6;
    if (gwy_strequal(fields[fi], "NNR")) {
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
    if (!scale_size) {
        g_warning("Zero SSZ, fixing to 1.0");
        scale_size = 1.0;
    }
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
    if (!x_scale) {
        g_warning("Zero XSC, fixing to 1.0");
        x_scale = 1.0;
    }

    /* There may be more stuff but we do not know anything about it. */

    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        goto fail;

    yreal = 1.0;
    xreal = x_scale*yreal;
    dfield = gwy_data_field_new(xres, yres, xreal, yreal, TRUE);

    if (type == CODEV_INT_SURFACE_DEFORMATION) {
        q = 1e-6*wavelength/scale_size;
        unit = "m";
        title = "Surface";
    }
    else if (type == CODEV_INT_WAVEFRONT_DEFORMATION) {
        q = 1e-6*wavelength/scale_size;
        unit = "m";
        title = "Wavefront";
    }
    else {
        g_warning("Don't know how to convert this grid data type to physical "
                  "units.");
        title = fields[3];
    }

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), unit);

    mfield = gwy_data_field_new_alike(dfield, TRUE);
    data = gwy_data_field_get_data(dfield);
    mdata = gwy_data_field_get_data(mfield);

    for (i = 0; i < xres*yres; i++, p = end) {
        gint value = strtol(p, &end, 10);

        if (value != no_data_value && (type != CODEV_INT_INTENSITY_FILTER
                                       || value >= 0)) {
            mdata[i] = 1.0;
            data[i] = q*value;
        }
    }

    if (!gwy_app_channel_remove_bad_data(dfield, mfield))
        gwy_object_unref(mfield);

    container = gwy_container_new();

    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);
    gwy_app_channel_check_nonsquare(container, 0);

    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup(title));

    if (mfield) {
        gwy_data_field_invert(mfield, FALSE, TRUE, FALSE);
        gwy_container_set_object(container, gwy_app_get_mask_key_for_id(0),
                                 mfield);
        g_object_unref(mfield);
    }

    meta = gwy_container_new();

    gwy_container_set_string_by_name(meta, "Comment", g_strdup(comment));
    gwy_container_set_string_by_name(meta, "Interpolation",
                                     g_strdup(nearest_neighbour
                                              ? "NNR" : "Linear"));
    gwy_container_set_string_by_name(meta, "Wavelength",
                                     g_strdup_printf("%g μm", wavelength));

    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

fail:
    g_free(fields);
    g_free(buffer);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
