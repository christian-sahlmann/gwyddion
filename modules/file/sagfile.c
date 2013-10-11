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
    SAG_UNIT_MM = 0,
    SAG_UNIT_CM = 1,
    SAG_UNIT_IN = 2,
    SAG_UNIT_M = 3,
    SAG_NUNITS,
} SagUnit;

typedef struct {
    guint xres;
    guint yres;
    gdouble dx;
    gdouble dy;
    SagUnit unit;
    gdouble xoff;
    gdouble yoff;
} SagFile;

static gboolean      module_register(void);
static gint          sag_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* sag_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static guint         sag_read_header(const gchar *header,
                                     guint len,
                                     SagFile *sagfile,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Zemax grid sag data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sagfile",
                           N_("Zemax grid sag data (.dat)"),
                           (GwyFileDetectFunc)&sag_detect,
                           (GwyFileLoadFunc)&sag_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sag_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    SagFile sagfile;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (sag_read_header(fileinfo->head, fileinfo->buffer_len, &sagfile, NULL))
        return 60;

    return 0;
}

static GwyContainer*
sag_load(const gchar *filename,
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

    SagFile sagfile;
    GwyContainer *container = NULL;
    GwyDataField *fields[5];
    gchar *p, *line, *end, *buffer = NULL;
    GError *err = NULL;
    gdouble q = 1.0;
    gsize size = 0;
    GString *str;
    guint len, i, k;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    gwy_clear(fields, G_N_ELEMENTS(fields));
    if (!(len = sag_read_header(buffer, size, &sagfile, error)))
        goto fail;

    p = buffer + len;

    if (sagfile.unit == SAG_UNIT_MM)
        q = 1e-3;
    else if (sagfile.unit == SAG_UNIT_CM)
        q = 1e-2;
    else if (sagfile.unit == SAG_UNIT_IN)
        q = 0.0254;

    for (i = 0; i < G_N_ELEMENTS(fields); i++) {
        fields[i] = gwy_data_field_new(sagfile.xres,
                                       sagfile.yres,
                                       sagfile.xres*sagfile.dx*q,
                                       sagfile.yres*sagfile.dy*q,
                                       FALSE);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(fields[i]),
                                    "m");
    }

    for (k = 0; k < sagfile.xres*sagfile.yres; k++) {
        line = gwy_str_next_line(&p);
        if (!line) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("File is truncated."));
            goto fail;
        }

        for (i = 0; i < G_N_ELEMENTS(fields); i++) {
            fields[i]->data[k] = g_ascii_strtod(line, &end);
            if (end == line) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("Data line %u does not contain five items."),
                            k+1);
                goto fail;
            }
            line = end;
        }
    }

    gwy_data_field_multiply(fields[0], q);
    gwy_data_field_multiply(fields[3], 1.0/q);

    if (!gwy_data_field_get_avg(fields[4]))
        gwy_object_unref(fields[4]);

    container = gwy_container_new();
    str = g_string_new(NULL);
    for (i = 0; i < G_N_ELEMENTS(titles); i++) {
        GQuark quark = gwy_app_get_data_key_for_id(i);
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(fields[i]),
                                    units[i]);
        gwy_container_set_object(container, quark, fields[i]);
        g_string_assign(str, g_quark_to_string(quark));
        g_string_append(str, "/title");
        gwy_container_set_string_by_name(container, str->str,
                                         g_strdup(titles[i]));
        if (fields[4]) {
            GwyDataField *mfield = gwy_data_field_duplicate(fields[4]);
            gwy_container_set_object(container, gwy_app_get_mask_key_for_id(i),
                                     mfield);
            g_object_unref(mfield);
        }
    }
    g_string_free(str, TRUE);

fail:
    g_free(buffer);
    for (i = 0; i < G_N_ELEMENTS(fields); i++)
        gwy_object_unref(fields[i]);

    return container;
}

/* XXX: This is a weak detection.  We basically accept anything that has seven
 * reasonable numbers on the first line. */
static guint
sag_read_header(const gchar *header,
                guint len,
                SagFile *sagfile,
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

    sagfile->xres = strtol(header, &end, 10);
    gwy_debug("xres %u", sagfile->xres);
    if (end == header)
        goto fail;
    if (err_DIMENSION(error, sagfile->xres)) {
        g_free(line);
        return 0;
    }
    header = end;

    sagfile->yres = strtol(header, &end, 10);
    gwy_debug("yres %u", sagfile->yres);
    if (end == header)
        goto fail;
    if (err_DIMENSION(error, sagfile->yres)) {
        g_free(line);
        return 0;
    }
    header = end;

    sagfile->dx = g_ascii_strtod(header, &end);
    gwy_debug("dx %g", sagfile->dx);
    if (end == header)
        goto fail;
    /* Use negated positive conditions to catch NaNs */
    if (!((sagfile->dx = fabs(sagfile->dx)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        sagfile->dx = 1.0;
    }
    header = end;

    sagfile->dy = g_ascii_strtod(header, &end);
    gwy_debug("dy %g", sagfile->dy);
    if (end == header)
        goto fail;
    /* Use negated positive conditions to catch NaNs */
    if (!((sagfile->dy = fabs(sagfile->dy)) > 0)) {
        g_warning("Real x step is 0.0, fixing to 1.0");
        sagfile->dy = 1.0;
    }
    header = end;

    sagfile->unit = strtol(header, &end, 10);
    gwy_debug("unit %u", sagfile->unit);
    if (end == header)
        goto fail;
    if (sagfile->unit >= SAG_NUNITS) {
        g_free(line);
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unit code %d is invalid or unsupported."),
                    sagfile->unit);
        return 0;
    }
    header = end;

    sagfile->xoff = g_ascii_strtod(header, &end);
    gwy_debug("xoff %g", sagfile->xoff);
    if (end == header)
        goto fail;
    header = end;

    sagfile->yoff = g_ascii_strtod(header, &end);
    gwy_debug("yoff %g", sagfile->yoff);
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
