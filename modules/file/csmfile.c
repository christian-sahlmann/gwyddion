/*
 *  $Id$
 *  Copyright (C) 2011 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
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

/* XXX: The file looks like Windows bitmap until you look beyond the nominal
 * file end.  And the type of data inside is not what the BMP header says at
 * all.  => Epic FAIL. */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-benyuan-csm-spm">
 *   <comment>Benyuan CSM data</comment>
 *   <glob pattern="*.csm"/>
 *   <glob pattern="*.CSM"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Benyuan CSM
 * .csm
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define Nanometre 1e-9

#define MAGIC "Version = CSPM"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".csm"

enum {
    BMP_HEADER_SIZE = 54
};

static gboolean      module_register(void);
static gint          csmfile_detect (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* csmfile_load   (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gboolean      read_bmp_header(const guchar *p,
                                     guint *xres,
                                     guint *yres,
                                     guint *size);
static void          store_meta     (gpointer key,
                                     gpointer value,
                                     gpointer user_data);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Benyuan CSM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2011",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("csmfile",
                           N_("Benyuan CSM files (.csm)"),
                           (GwyFileDetectFunc)&csmfile_detect,
                           (GwyFileLoadFunc)&csmfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
csmfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    guint size, xres, yres;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    // Weed out files that do not pretend to be Windows bitmaps quickly.
    if (fileinfo->buffer_len < BMP_HEADER_SIZE
        || !read_bmp_header(fileinfo->head, &size, &xres, &yres))
        return 0;

    // Weed out incorrect Windows bitmaps but also those no extra data beyond
    // the nominal end.
    if (fileinfo->file_size <= size)
        return 0;

    // Look for "Version = CSPM" somewhere near the end.
    if (!gwy_memmem(fileinfo->tail, fileinfo->buffer_len,
                    MAGIC, MAGIC_SIZE))
        return 0;


    return 90;
}

static GwyContainer*
csmfile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *meta, *container = NULL;
    GHashTable *hash = NULL;
    guchar *d24, *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint xres, yres, hxres, hyres, bmpsize, header_size, maxval, i, j;
    gdouble real, zmin, zmax, q, z0;
    GwyTextHeaderParser parser;
    GwySIUnit *unit = NULL;
    gchar *value, *end, *header = NULL;
    gdouble *data;
    gint power10;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < BMP_HEADER_SIZE) {
        err_TOO_SHORT(error);
        goto fail;
    }
    if (!read_bmp_header(buffer, &xres, &yres, &bmpsize)
        || size <= bmpsize) {
        err_FILE_TYPE(error, "CSM");
        goto fail;
    }

    header_size = size - bmpsize;
    header = g_new(gchar, header_size + 1);
    memcpy(header, buffer + bmpsize, header_size);
    header[header_size] = '\0';

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    hash = gwy_text_header_parse(header, &parser, NULL, NULL);

    if (!(value = g_hash_table_lookup(hash, "Image width"))) {
        err_MISSING_FIELD(error, "Image width");
        goto fail;
    }
    hxres = atoi(value);
    if (hxres != xres) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header image width field does not match BMP width."));
        goto fail;
    }
    if (err_DIMENSION(error, xres))
        goto fail;

    if (!(value = g_hash_table_lookup(hash, "Image height"))) {
        err_MISSING_FIELD(error, "Image height");
        goto fail;
    }
    hyres = atoi(value);
    if (hyres != yres) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Header image height field does not match BMP height."));
        goto fail;
    }
    if (err_DIMENSION(error, yres))
        goto fail;

    if (!(value = g_hash_table_lookup(hash, "ScanSize"))) {
        err_MISSING_FIELD(error, "ScanSize");
        goto fail;
    }
    real = g_ascii_strtod(value, NULL);
    /* Use negated positive conditions to catch NaNs */
    if (!((real = fabs(real)) > 0)) {
        g_warning("Real size is 0.0, fixing to 1.0");
        real = 1.0;
    }

    if (!(value = g_hash_table_lookup(hash, "HeightScale"))) {
        err_MISSING_FIELD(error, "HeightScale");
        goto fail;
    }
    zmax = g_ascii_strtod(value, &end);
    unit = gwy_si_unit_new_parse(end, &power10);

    /* Optional stuff for which we try to fall back. */
    if (!(value = g_hash_table_lookup(hash, "StartHeightScale")))
        zmin = 0.0;
    else
        zmin = g_ascii_strtod(value, NULL);

    if (!(value = g_hash_table_lookup(hash, "MaxValue")))
        maxval = 0xffff;
    else
        maxval = MAX(atoi(value), 1);


    dfield = gwy_data_field_new(xres, yres, real*Nanometre, real*Nanometre,
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    d24 = buffer + BMP_HEADER_SIZE;
    q = pow10(power10)*(zmax - zmin)/maxval;
    z0 = pow10(power10)*zmin;
    for (i = 0; i < yres; i++) {
        gdouble *row = data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++, row++, d24 += 3)
            *row = (d24[0] + 256.0*d24[1])*q + z0;
    }

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    gwy_data_field_set_si_unit_z(dfield, unit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    meta = gwy_container_new();
    g_hash_table_foreach(hash, store_meta, meta);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    if ((value = g_hash_table_lookup(hash, "sTitle"))
        && g_utf8_validate(value, -1, NULL)) {
        gwy_container_set_string_by_name(container, "/0/data/title",
                                         g_strdup(value));
    }
    else
        gwy_app_channel_title_fall_back(container, 0);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    gwy_object_unref(unit);
    if (header)
        g_free(header);
    if (hash)
        g_hash_table_destroy(hash);

    return container;
}

static gboolean
read_bmp_header(const guchar *p,
                guint *xres,
                guint *yres,
                guint *size)
{
    guint x;

    if (p[0] != 'B' || p[1] != 'M')
        return FALSE;
    p += 2;

    if ((*size = gwy_get_guint32_le(&p)) < BMP_HEADER_SIZE)   /* Size */
        return FALSE;
    if ((x = gwy_get_guint32_le(&p)) != 0)   /* Reserved */
        return FALSE;
    if ((x = gwy_get_guint32_le(&p)) != 54)   /* Offset */
        return FALSE;
    if ((x = gwy_get_guint32_le(&p)) != 40)   /* Header size */
        return FALSE;
    if ((*xres = gwy_get_guint32_le(&p)) == 0)   /* Width */
        return FALSE;
    if ((*yres = gwy_get_guint32_le(&p)) == 0)   /* Height */
        return FALSE;
    if ((x = gwy_get_guint16_le(&p)) != 1)   /* Bit planes */
        return FALSE;
    if ((x = gwy_get_guint16_le(&p)) != 24)   /* BPP */
        return FALSE;
    if ((x = gwy_get_guint32_le(&p)) != 0)   /* Compression */
        return FALSE;
    if ((x = gwy_get_guint32_le(&p)) + BMP_HEADER_SIZE != *size)   /* Compresed size */
        return FALSE;

    if (3*(*xres)*(*yres) + BMP_HEADER_SIZE != *size)
        return FALSE;

    return TRUE;
}

static void
store_meta(gpointer key,
           gpointer value,
           gpointer user_data)
{
    GwyContainer *meta = (GwyContainer*)user_data;

    if (g_utf8_validate(value, -1, NULL)) {
        gwy_container_set_string_by_name(meta, key, g_strdup(value));
    }
    else {
        // FIXME: Is this Windows-locale dependent?
        gchar *s = g_convert(value, -1, "UTF-8", "GB2312", NULL, NULL, NULL);
        if (s)
            gwy_container_set_string_by_name(meta, key, s);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
