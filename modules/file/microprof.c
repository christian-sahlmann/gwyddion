/*
 *  $Id: microprof.c 6038 2006-05-23 09:16:26Z yeti-dn $
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
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
 * <mime-type type="application/x-microprof-txt">
 *   <comment>MicroProf FRT text data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="HeaderLines"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-microprof">
 *   <comment>MicroProf FRT data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="FRTM_"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * MicroProf TXT
 * .txt
 * Read
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * MicroProf FRT
 * .frt
 * Read
 **/

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include <string.h>
#include <stdlib.h>

#include "err.h"

#define MAGIC      "FRTM_"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)
#define MAGIC_TXT      "HeaderLines="
#define MAGIC_TXT_SIZE (sizeof(MAGIC_TXT) - 1)

#define EXTENSION ".frt"
#define EXTENSION_TXT ".txt"

enum {
    MICROPROF_HEADER_SIZE = 124,
    MICROPROF_MIN_TEXT_SIZE = 80,
};

typedef struct {
    guint xres;
    guint yres;
    gdouble xrange;
    gdouble yrange;
    gdouble zscale;
    const guchar *data;
} MicroProfFile;

static gboolean      module_register          (void);
static gint          microprof_detect         (const GwyFileDetectInfo *fileinfo,
                                               gboolean only_name);
static gint          microprof_txt_detect     (const GwyFileDetectInfo *fileinfo,
                                               gboolean only_name);
static GwyContainer* microprof_load           (const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);
static GwyDataField* microprof_read_data_field(const MicroProfFile *mfile,
                                               const guchar *buffer);
static GwyContainer* microprof_txt_load       (const gchar *filename,
                                               GwyRunType mode,
                                               GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports MicroProf FRT profilometer data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.4",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("microprof",
                           N_("MicroProf FRT files (.frt)"),
                           (GwyFileDetectFunc)&microprof_detect,
                           (GwyFileLoadFunc)&microprof_load,
                           NULL,
                           NULL);
    gwy_file_func_register("microprof_txt",
                           N_("MicroProf FRT text files (.txt)"),
                           (GwyFileDetectFunc)&microprof_txt_detect,
                           (GwyFileLoadFunc)&microprof_txt_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
microprof_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->file_size < MICROPROF_HEADER_SIZE
        || fileinfo->buffer_len < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 80;
}

static gint
microprof_txt_detect(const GwyFileDetectInfo *fileinfo,
                     gboolean only_name)
{
    GwyTextHeaderParser parser;
    GHashTable *meta;
    const guchar *p;
    gchar *buffer;
    gsize size;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_TXT) ? 10 : 0;

    if (fileinfo->buffer_len < MICROPROF_MIN_TEXT_SIZE
        || memcmp(fileinfo->head, MAGIC_TXT, MAGIC_TXT_SIZE) != 0)
        return 0;

    if (!(p = strstr(fileinfo->head, "\n\n"))
        && !(p = strstr(fileinfo->head, "\r\r"))
        && !(p = strstr(fileinfo->head, "\r\n\r\n")))
        return 0;

    size = p - (const guchar*)fileinfo->head;
    buffer = g_memdup(fileinfo->head, size);
    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    meta = gwy_text_header_parse(buffer, &parser, NULL, NULL);
    if (g_hash_table_lookup(meta, "XSize")
        && g_hash_table_lookup(meta, "YSize")
        && g_hash_table_lookup(meta, "XRange")
        && g_hash_table_lookup(meta, "YRange")
        && g_hash_table_lookup(meta, "ZScale"))
        score = 90;

    g_free(buffer);
    if (meta)
        g_hash_table_destroy(meta);

    return score;
}

static GwyContainer*
microprof_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    enum {
        XRES = 0x0026, YRES = 0x002a,
        XRANGE = 0x0038, YRANGE = 0x0040,
        ZRANGE = 0x006e,
    };
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    MicroProfFile mfile;
    gsize size = 0;
    GError *err = NULL;
    guint i, ndata, datasize;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MICROPROF_HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "MicroProf");
        goto fail;
    }

    p = buffer + XRES;
    mfile.xres = gwy_get_guint16_le(&p);
    if (err_DIMENSION(error, mfile.xres))
        goto fail;

    p = buffer + YRES;
    mfile.yres = gwy_get_guint16_le(&p);
    if (err_DIMENSION(error, mfile.xres))
        goto fail;

    p = buffer + XRANGE;
    mfile.xrange = gwy_get_gdouble_le(&p);
    if (!(mfile.xrange = fabs(mfile.xrange))) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        mfile.xrange = 1.0;
    }

    p = buffer + YRANGE;
    mfile.yrange = gwy_get_gdouble_le(&p);
    if (!(mfile.yrange = fabs(mfile.yrange))) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        mfile.yrange = 1.0;
    }

    p = buffer + ZRANGE;
    mfile.zscale = gwy_get_gdouble_le(&p);

    datasize = 2*mfile.xres*mfile.yres;
    if (err_SIZE_MISMATCH(error, datasize, size - MICROPROF_HEADER_SIZE, FALSE))
        goto fail;
    /* FIXME: There is weird stuff between channels.  Need specs.
    ndata = (size - MICROPROF_HEADER_SIZE)/datasize;
    if (!ndata) {
        err_NO_DATA(error);
        goto fail;
    }
    */
    ndata = 1;
    container = gwy_container_new();

    mfile.data = buffer + MICROPROF_HEADER_SIZE;
    for (i = 0; i < ndata; i++) {
        GwyDataField *dfield;
        GQuark quark;

        dfield = microprof_read_data_field(&mfile,
                                           mfile.data + i*datasize);
        quark = gwy_app_get_data_key_for_id(i);
        gwy_container_set_object(container, quark, dfield);
        g_object_unref(dfield);

        gwy_app_channel_title_fall_back(container, i);
    }

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    return container;
}

static GwyDataField*
microprof_read_data_field(const MicroProfFile *mfile,
                          const guchar *buffer)
{
    const guint16 *d16 = (const guint16*)buffer;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *d;
    guint xres, yres, i, j;

    xres = mfile->xres;
    yres = mfile->yres;
    dfield = gwy_data_field_new(xres, yres,
                                mfile->xrange, mfile->yrange, FALSE);
    d = gwy_data_field_get_data(dfield);

    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            d[(yres-1 - i)*xres + j] = mfile->zscale*GUINT16_FROM_LE(*d16);
            d16++;
        }
    }

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string(siunit, "m");

    siunit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_set_from_string(siunit, "m");

    return dfield;
}

static GwyContainer*
microprof_txt_load(const gchar *filename,
                   G_GNUC_UNUSED GwyRunType mode,
                   GError **error)
{
    GwyContainer *container = NULL;
    guchar *p, *buffer = NULL;
    GwyTextHeaderParser parser;
    GHashTable *meta = NULL;
    GwySIUnit *siunit;
    gchar *header = NULL, *s, *prev;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gdouble xreal, yreal, zscale, v;
    gint hlines, xres, yres, i, j;
    gdouble *d;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MICROPROF_MIN_TEXT_SIZE
        || memcmp(buffer, MAGIC_TXT, MAGIC_TXT_SIZE) != 0) {
        err_FILE_TYPE(error, "MicroProf");
        goto fail;
    }

    hlines = atoi(buffer + MAGIC_TXT_SIZE);
    if (hlines < 7) {
        err_FILE_TYPE(error, "MicroProf");
        goto fail;
    }

    /* Skip specified number of lines */
    for (p = buffer, i = 0; i < hlines; i++) {
        while (*p != '\n' && (gsize)(p - buffer) < size)
            p++;
        if ((gsize)(p - buffer) == size) {
            err_FILE_TYPE(error, "MicroProf");
            goto fail;
        }
        /* Now skip the \n */
        p++;
    }

    header = g_memdup(buffer, p - buffer + 1);
    header[p - buffer] = '\0';

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    meta = gwy_text_header_parse(header, &parser, NULL, NULL);

    if (!(s = g_hash_table_lookup(meta, "XSize"))
        || !((xres = atoi(s)) > 0)) {
        err_INVALID(error, "XSize");
        goto fail;
    }

    if (!(s = g_hash_table_lookup(meta, "YSize"))
        || !((yres = atoi(s)) > 0)) {
        err_INVALID(error, "YSize");
        goto fail;
    }

    if (!(s = g_hash_table_lookup(meta, "XRange"))
        || !((xreal = g_ascii_strtod(s, NULL)) > 0.0)) {
        err_INVALID(error, "YRange");
        goto fail;
    }

    if (!(s = g_hash_table_lookup(meta, "YRange"))
        || !((yreal = g_ascii_strtod(s, NULL)) > 0.0)) {
        err_INVALID(error, "YRange");
        goto fail;
    }

    if (!(s = g_hash_table_lookup(meta, "ZScale"))
        || !((zscale = g_ascii_strtod(s, NULL)) > 0.0)) {
        err_INVALID(error, "ZScale");
        goto fail;
    }

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    d = gwy_data_field_get_data(dfield);
    s = (gchar*)p;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            prev = s;
            /* Skip x */
            v = strtol(s, &s, 10);
            if (v != j)
                g_warning("Column number mismatch");
            /* Skip y */
            v = strtol(s, &s, 10);
            if (v != i)
                g_warning("Row number mismatch");
            /* Read value */
            d[(yres-1 - i)*xres + j] = strtol(s, &s, 10)*zscale;

            /* Check whether we moved in the file */
            if (s == prev) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("File contains less than XSize*YSize data "
                              "points."));
                goto fail;
            }
        }
    }

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    if (meta)
        g_hash_table_destroy(meta);
    g_free(header);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

