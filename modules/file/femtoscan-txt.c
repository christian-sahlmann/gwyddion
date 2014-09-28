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
 * <mime-type type="application/x-femtoscan-txt">
 *   <comment>FemtoScan text SPM data</comment>
 *   <magic priority="60">
 *     <match type="string" offset="0" value="\tX,nm\t0\t"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FILEMAGIC]
 * # FemtoScan text data
 * 0 string \tX,nm\t0\t FemtoScan SPM text data
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * FemtoScan text data
 * .txt
 * Read[1]
 * [1] Regular sampling in both X and Y direction is assumed.
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

/* FIXME: This seems to be a pretty safe bet but the axes may be in principle
 * different than in nm and starting from 0. */
#define MAGIC "\tX,nm\t0\t"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define MAGIC2 "Y,nm\tZ,"
#define EXTENSION ".txt"

#define Nanometre (1e-9)

static gboolean      module_register(void);
static gint          femto_detect   (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* femto_load     (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports FemtoScan TXT files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("femtoscan-txt",
                           N_("FemtoScan text files (.txt)"),
                           (GwyFileDetectFunc)&femto_detect,
                           (GwyFileLoadFunc)&femto_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
femto_detect(const GwyFileDetectInfo *fileinfo,
             gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static guint
read_values(GArray *values, gchar *p)
{
    guint oldlen = values->len;

    while (TRUE) {
        gchar *end;
        gdouble v;

        v = g_ascii_strtod(p, &end);
        if (end == p)
            return values->len - oldlen;
        g_array_append_val(values, v);
        p = end;
    }
}

static GwyContainer*
femto_load(const gchar *filename,
           G_GNUC_UNUSED GwyRunType mode,
           GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield = NULL;
    GArray *xcal = NULL, *ycal = NULL, *data = NULL;
    GwySIUnit *unit = NULL;
    gchar *line, *p, *buffer = NULL;
    gsize size;
    GError *err = NULL;
    gdouble xreal, yreal;
    guint xres, yres;
    gint power10;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    p = buffer;
    line = gwy_str_next_line(&p);
    if (!g_str_has_prefix(line, MAGIC)) {
        err_FILE_TYPE(error, "FemtoScan TXT data");
        goto fail;
    }

    xcal = g_array_new(FALSE, FALSE, sizeof(gdouble));
    read_values(xcal, line + strlen("\tX,nm\t"));
    if (err_DIMENSION(error, xcal->len))
        goto fail;

    line = gwy_str_next_line(&p);
    if (!g_str_has_prefix(line, MAGIC2)) {
        err_FILE_TYPE(error, "FemtoScan TXT data");
        goto fail;
    }
    unit = gwy_si_unit_new_parse(line + strlen(MAGIC2), &power10);

    ycal = g_array_new(FALSE, FALSE, sizeof(gdouble));
    data = g_array_new(FALSE, FALSE, sizeof(gdouble));
    while ((line = gwy_str_next_line(&p))) {
        gchar *end;
        gdouble v;
        guint n;

        if (!*line)
            break;
        if (g_ascii_isspace(line[0])) {
            g_strstrip(line);
            if (!*line)
                break;
        }

        v = g_ascii_strtod(line, &end);
        if (end == line) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Data line does not start with a Y abscissa."));
            goto fail;
        }

        g_array_append_val(ycal, v);
        if ((n = read_values(data, end)) != xcal->len) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Data line length %u does not correspond to "
                          "the number of X abscissas %u."),
                        n, (guint)xcal->len);
            goto fail;
        }
    }
    if (err_DIMENSION(error, ycal->len))
        goto fail;

    xres = xcal->len;
    yres = ycal->len;
    /* Linearise brutally all calibration. */
    xreal = (g_array_index(xcal, gdouble, xres-1)
             - g_array_index(xcal, gdouble, 0))*xres/(xres - 1.0)*Nanometre;
    yreal = (g_array_index(ycal, gdouble, yres-1)
             - g_array_index(ycal, gdouble, 0))*yres/(yres - 1.0)*Nanometre;
    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    memcpy(dfield->data, data->data, xres*yres*sizeof(gdouble));

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    gwy_data_field_set_si_unit_z(dfield, unit);
    if (power10)
        gwy_data_field_multiply(dfield, pow10(power10));

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    gwy_file_channel_import_log_add(container, 0, NULL, filename);

fail:
    gwy_object_unref(unit);
    gwy_object_unref(dfield);
    g_free(buffer);
    if (xcal)
        g_array_free(xcal, TRUE);
    if (ycal)
        g_array_free(ycal, TRUE);
    if (data)
        g_array_free(data, TRUE);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
