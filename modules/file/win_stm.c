/*
 *  @(#) $Id$
 *  Copyright (C) 2014 Jeffrey J. Schwartz.
 *  E-mail: schwartz@physics.ucla.edu
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

/*
 * This module serves to open WinSTM data files in Gywddion.
 * Currently, multiple data channels are supported with limited
 * meta data import.  No file export is supported; it is assumed
 * that no changes will be saved, or if so then another file
 * format will be used.
 *
 * TODO: Import full meta data list.
 */

#include <stdio.h>
#include <string.h>
#include <glib/gstdio.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>
#include "err.h"
#include "get.h"

#define EXTENSION ".stm"

#define MAGIC "WinSTM"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

enum {
    HEADER_SIZE = 1368,
    DATA_SIZE = 768
};

typedef struct {
    gint32 NChan;
    gint32 NSpec;
    gint32 Xres;
    gint32 Yres;
    gdouble Gain;
    gdouble RangeX;
    gdouble RangeY;
    gdouble Bias;
    gdouble Current;
} WinSTM_File;

static gboolean      module_register(void);
static gint          winSTM_detect  (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* winSTM_load    (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports WinSTM (.stm) files."),
    "Jeffrey J. Schwartz <schwartz@physics.ucla.edu>",
    "0.5",
    "Jeffrey J. Schwartz",
    "April 2014",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("win_stm",
                           N_("WinSTM files (.stm)"),
                           (GwyFileDetectFunc)&winSTM_detect,
                           (GwyFileLoadFunc)&winSTM_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
winSTM_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;
    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;
    return 0;
}

static GwyContainer*
winSTM_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    WinSTM_File stm;
    GwyContainer *container = gwy_container_new();
    GwyDataField *dfield;
    guchar *buffer;
    const guchar *p;
    GError *err = NULL;
    gsize size;
    gdouble *data;
    guint filemin;
    gint k, v;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        g_clear_error(&err);
        return NULL;
    }
    filemin = HEADER_SIZE;
    if (size <= filemin) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "WinSTM");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    p = buffer + MAGIC_SIZE;
    p += 926;
    stm.NChan = gwy_get_gint32_le(&p);
    stm.NSpec = gwy_get_gint16_le(&p);
    p += 86;
    stm.RangeX = gwy_get_gdouble_le(&p) * (1E-10);
    stm.RangeY = gwy_get_gdouble_le(&p) * (1E-10);
    p += 328;
    filemin = HEADER_SIZE + DATA_SIZE;
    if (size < filemin) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    for (v = 0; v < stm.NChan; v++) {
        gchar key[40];
        gchar UnitWord[64];
        gchar Title[100];
        gchar *pch;
        gchar *s;
        GwyContainer *meta;
        p += 2;
        get_CHARARRAY0(Title, &p);
        p += 522;
        stm.Gain = gwy_get_gdouble_le(&p);
        get_CHARARRAY0(UnitWord, &p);
        pch = strstr(UnitWord, "ngstroms");
        if (pch != NULL)
            stm.Gain *= 1E-10;
        if (strcmp(UnitWord, "pAmps") == 0)
            stm.Gain *= 1E-12;
        stm.Bias = gwy_get_gdouble_le(&p);
        stm.Current = gwy_get_gdouble_le(&p) * (1E3);
        stm.Xres = gwy_get_gint32_le(&p);
        stm.Yres = gwy_get_gint32_le(&p);
        p += 48;
        dfield = gwy_data_field_new(stm.Xres, stm.Yres,
                        stm.RangeX, stm.RangeY, FALSE);
        data = gwy_data_field_get_data(dfield);
        filemin = HEADER_SIZE + DATA_SIZE + ((v+1) * 4 * stm.Xres * stm.Yres);
        if (size < filemin) {
            err_TOO_SHORT(error);
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }

        for (k = 0; k < stm.Xres * stm.Yres; k++)
            data[k] = gwy_get_gint32_le(&p) * stm.Gain;
        gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
        if (pch != NULL)
            gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "m");
        else {
            if (strcmp(UnitWord, "pAmps") == 0)
                gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "A");
            else
                gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), "V");
        }
        g_snprintf(key, sizeof(key), "/%i/data", v);
        gwy_container_set_object_by_name(container, key, dfield);
        g_snprintf(key, sizeof(key), "/%i/data/title", v);
        s = g_convert(Title, -1, "iso-8859-1", "utf-8", NULL, NULL, NULL);
        gwy_container_set_string_by_name(container, key, (guchar *)s);
        meta = gwy_container_new();
        gwy_container_set_string_by_name(meta, "Bias",
                    (const guchar *)g_strdup_printf("%.3f V", stm.Bias));
        gwy_container_set_string_by_name(meta, "Current Set Point",
                    (const guchar *)g_strdup_printf("%.3f pA", stm.Current));
        g_snprintf(key, sizeof(key), "/%i/meta", v);
        gwy_container_set_object_by_name(container, key, meta);
        g_object_unref(meta);
        g_object_unref(dfield);
    }
    gwy_file_channel_import_log_add(container, 0, "win_stm", filename);
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
