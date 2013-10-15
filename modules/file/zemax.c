/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net
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
 * [FILE-MAGIC-USERGUIDE]
 * Zemax grid sag data
 * .dat
 * Read
 **/

#include "config.h"
#include <stdlib.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include "err.h"

#define EXTENSION ".dat"

typedef enum {
    ZEMAX_UNIT_MM = 0,
    ZEMAX_UNIT_CM = 1,
    ZEMAX_UNIT_IN = 2,
    ZEMAX_UNIT_M = 3,
    ZEMAX_NUNITS,
} ZemaxUnit;

typedef struct {
    guint xres;
    guint yres;
    gdouble dx;
    gdouble dy;
    ZemaxUnit unit;
    gdouble xoff;
    gdouble yoff;
} ZemaxFile;

static gboolean      module_register  (void);
static gint          zemax_detect     (const GwyFileDetectInfo *fileinfo,
                                       gboolean only_name);
static GwyContainer* zemax_load       (const gchar *filename,
                                       GwyRunType mode,
                                       GError **error);
static guint         zemax_read_header(const gchar *header,
                                       guint len,
                                       ZemaxFile *zmxfile,
                                       GError **error);
static gboolean      is_empty         (GwyDataField *field);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Zemax grid sag data files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("zemax",
                           N_("Zemax grid sag data (.dat)"),
                           (GwyFileDetectFunc)&zemax_detect,
                           (GwyFileLoadFunc)&zemax_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
zemax_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    ZemaxFile zmxfile;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (zemax_read_header(fileinfo->head, fileinfo->buffer_len, &zmxfile, NULL))
        return 60;

    return 0;
}

static GwyContainer*
zemax_load(const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    /* The last field is special, it is the mask of invalid pixels. */
    static const gchar *titles[4] = {
        "Height", "dZ/dX", "dZ/dY", "d²Z/dXdY"
    };
    static const gchar *units[4] = {
        "m", "rad", "rad", "rad/m"
    };

    ZemaxFile zmxfile;
    GwyContainer *container = NULL, *meta;
    GwyDataField *fields[5];
    gchar *p, *line, *end, *buffer = NULL;
    GError *err = NULL;
    gdouble q = 1.0;
    gsize size = 0;
    GString *str;
    guint len, i, k, nchannels;
    gboolean havemask = FALSE;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    gwy_clear(fields, G_N_ELEMENTS(fields));
    if (!(len = zemax_read_header(buffer, size, &zmxfile, error)))
        goto fail;

    p = buffer + len;

    if (zmxfile.unit == ZEMAX_UNIT_MM)
        q = 1e-3;
    else if (zmxfile.unit == ZEMAX_UNIT_CM)
        q = 1e-2;
    else if (zmxfile.unit == ZEMAX_UNIT_IN)
        q = 0.0254;

    for (i = 0; i < G_N_ELEMENTS(fields); i++) {
        fields[i] = gwy_data_field_new(zmxfile.xres,
                                       zmxfile.yres,
                                       zmxfile.xres*zmxfile.dx*q,
                                       zmxfile.yres*zmxfile.dy*q,
                                       FALSE);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(fields[i]),
                                    "m");
    }

    for (k = 0; k < zmxfile.xres*zmxfile.yres; k++) {
        do {
            line = gwy_str_next_line(&p);
        } while (line && line[0] == '!');

        if (!line) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("File is truncated."));
            goto fail;
        }

        for (i = 0; i < G_N_ELEMENTS(fields); i++) {
            fields[i]->data[k] = g_ascii_strtod(line, &end);
            if (end == line) {
                if (i < 4) {
                    g_set_error(error, GWY_MODULE_FILE_ERROR,
                                GWY_MODULE_FILE_ERROR_DATA,
                                _("Data line %u does not contain four items."),
                                k+1);
                    goto fail;
                }
                fields[i]->data[k] = 0.0;
            }
            line = end;
        }
        if (fields[4]->data[k])
            havemask = TRUE;
    }

    gwy_data_field_multiply(fields[0], q);
    gwy_data_field_multiply(fields[3], 1.0/q);

    nchannels = G_N_ELEMENTS(titles);
    if (is_empty(fields[1]) && is_empty(fields[2]) && is_empty(fields[3]))
        nchannels = 1;

    meta = gwy_container_new();
    gwy_container_set_string_by_name(meta, "Decenter X",
                                     g_strdup_printf("%g m", q*zmxfile.xoff));
    gwy_container_set_string_by_name(meta, "Decenter Y",
                                     g_strdup_printf("%g m", q*zmxfile.yoff));

    container = gwy_container_new();
    str = g_string_new(NULL);
    for (i = 0; i < nchannels; i++) {
        GQuark quark = gwy_app_get_data_key_for_id(i);
        GwyContainer *channelmeta;

        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(fields[i]),
                                    units[i]);
        gwy_container_set_object(container, quark, fields[i]);
        g_string_assign(str, g_quark_to_string(quark));
        g_string_append(str, "/title");
        gwy_container_set_string_by_name(container, str->str,
                                         g_strdup(titles[i]));
        if (havemask) {
            GwyDataField *mfield = gwy_data_field_duplicate(fields[4]);
            gwy_container_set_object(container, gwy_app_get_mask_key_for_id(i),
                                     mfield);
            g_object_unref(mfield);
        }
        g_string_printf(str, "/%u/meta", i);
        channelmeta = gwy_container_duplicate(meta);
        gwy_container_set_object_by_name(container, str->str, channelmeta);
        g_object_unref(channelmeta);
    }
    g_string_free(str, TRUE);
    g_object_unref(meta);

fail:
    g_free(buffer);
    for (i = 0; i < G_N_ELEMENTS(fields); i++)
        gwy_object_unref(fields[i]);

    return container;
}

/* XXX: This is a weak detection.  We basically accept anything that has seven
 * reasonable numbers on the first line. */
static guint
zemax_read_header(const gchar *header,
                  guint len,
                  ZemaxFile *zmxfile,
                  GError **error)
{
    const gchar *p = header, *lineend;
    gchar *end, *line = NULL;

    /* Weed out binary files quickly. */
    if (len < 16 || (header[0] != '!' && !g_ascii_isdigit(header[0])))
        goto fail;

    /* Avoid premature memory allocations and stuff since this is also used in
     * the detection path which should be fast.  Find the first line which does
     * not start with '!'. */
    while (header[0] == '!') {
        while (*header && *header != '\n' && *header != '\r')
            header++;
        while (*header == '\n' || *header == '\r')
            header++;

        if (!*header)
            goto fail;
    }

    /* Find its end. */
    lineend = header;
    while (*lineend && *lineend != '\n' && *lineend != '\r')
        lineend++;
    while (*lineend == '\n' || *lineend == '\r')
        lineend++;

    line = g_strndup(header, lineend-header);
    header = line;

    zmxfile->xres = strtol(header, &end, 10);
    gwy_debug("xres %u", zmxfile->xres);
    if (end == header)
        goto fail;
    if (err_DIMENSION(error, zmxfile->xres)) {
        g_free(line);
        return 0;
    }
    header = end;

    zmxfile->yres = strtol(header, &end, 10);
    gwy_debug("yres %u", zmxfile->yres);
    if (end == header)
        goto fail;
    if (err_DIMENSION(error, zmxfile->yres)) {
        g_free(line);
        return 0;
    }
    header = end;

    zmxfile->dx = g_ascii_strtod(header, &end);
    gwy_debug("dx %g", zmxfile->dx);
    if (end == header)
        goto fail;
    /* Use negated positive conditions to catch NaNs */
    if (!((zmxfile->dx = fabs(zmxfile->dx)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        zmxfile->dx = 1.0;
    }
    header = end;

    zmxfile->dy = g_ascii_strtod(header, &end);
    gwy_debug("dy %g", zmxfile->dy);
    if (end == header)
        goto fail;
    /* Use negated positive conditions to catch NaNs */
    if (!((zmxfile->dy = fabs(zmxfile->dy)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        zmxfile->dy = 1.0;
    }
    header = end;

    zmxfile->unit = strtol(header, &end, 10);
    gwy_debug("unit %u", zmxfile->unit);
    if (end == header)
        goto fail;
    if (zmxfile->unit >= ZEMAX_NUNITS) {
        g_free(line);
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unit code %d is invalid or unsupported."),
                    zmxfile->unit);
        return 0;
    }
    header = end;

    zmxfile->xoff = g_ascii_strtod(header, &end);
    gwy_debug("xoff %g", zmxfile->xoff);
    if (end == header)
        goto fail;
    header = end;

    zmxfile->yoff = g_ascii_strtod(header, &end);
    gwy_debug("yoff %g", zmxfile->yoff);
    if (end == header)
        goto fail;
    header = end;

    while (*header && g_ascii_isspace(*header))
        header++;

    if (*header) {
        g_free(line);
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("The first line contains too many items."));
        return 0;
    }

    g_free(line);
    return lineend - p;

fail:
    g_free(line);
    err_FILE_TYPE(error, "Zemax");
    return 0;
}

static gboolean
is_empty(GwyDataField *field)
{
    gdouble min, max;
    gwy_data_field_get_min_max(field, &min, &max);
    return min == 0.0 && max == 0.0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
