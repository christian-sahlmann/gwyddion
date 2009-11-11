/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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
#define DEBUG 1
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define EXTENSION ".aist"

#define Nanometer 1e-9
#define NanoAmpere 1e-9

#define MAGIC "\x00\x00"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

typedef struct {
    guint id;
    guint index;
    gchar *name;
    gchar *description;
} AistCommon;

typedef struct {
    AistCommon common;
    guint xres, yres;
    gdouble left, right, top, bottom;
    gchar *xunits, *yunits, *zunits;
} AistRaster;

static gboolean       module_register        (void);
static gint           aist_detect         (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*  aist_load           (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports AIST-NT data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("aistfile",
                           N_("AIST-NT files (.aist)"),
                           (GwyFileDetectFunc)&aist_detect,
                           (GwyFileLoadFunc)&aist_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
aist_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return (g_str_has_suffix(fileinfo->name_lowercase, EXTENSION)) ? 10 : 0;

    /* FIXME FIXME FIXME
     * They do not have anything resembling magic header and the data size is
     * not written anywhere either. */
    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 50;

    return score;
}

static gboolean
read_qt_string(const guchar **p, gsize *size, gchar **value)
{
    const gunichar2 *utf16native;
    gunichar2 *must_free;
    guint len;

    *value = NULL;

    if (*size < sizeof(guint32))
        return FALSE;

    len = gwy_get_guint32_be(p);
    *size -= sizeof(guint32);
    gwy_debug("QString length: %u", len);

    if (*size < len || len % sizeof(gunichar2))
        return FALSE;

#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    utf16native = (const gunichar2*)(*p);
    must_free = NULL;
#endif
#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
    utf16native = must_free = g_new(gunichar2, len/sizeof(gunichar2));
    swab(*p, must_free, len);
#endif
    *value = g_utf16_to_utf8(utf16native, len/sizeof(gunichar2), NULL, NULL,
                             NULL);
    gwy_debug("QString data: <%s>", *value);
    g_free(must_free);

    *size -= len;
    *p += len;

    return TRUE;
}

static gboolean
read_qt_int(const guchar **p, gsize *size, guint *value)
{
    *value = 0;
    if (*size < sizeof(guint32))
        return FALSE;

    *value = gwy_get_guint32_be(p);
    gwy_debug("QInt data: %u", *value);
    *size -= sizeof(guint32);

    return TRUE;
}

static gboolean
read_qt_double(const guchar **p, gsize *size, gdouble *value)
{
    *value = 0;
    if (*size < sizeof(gdouble))
        return FALSE;

    *value = gwy_get_gdouble_be(p);
    gwy_debug("QDouble data: %g", *value);
    *size -= sizeof(gdouble);

    return TRUE;
}

static gboolean
read_qt_bool(const guchar **p, gsize *size, gboolean *value)
{
    *value = FALSE;
    if (*size < sizeof(guchar))
        return FALSE;

    *value = !!**p;
    gwy_debug("QBool data: %d", *value);
    *size -= sizeof(guchar);
    (*p)++;

    return TRUE;
}

static void
free_aist_common(AistCommon *common)
{
    g_free(common->name);
    g_free(common->description);
}

static gboolean
read_aist_common(const guchar **p, gsize *size,
                 AistCommon *common)
{
    if (!read_qt_int(p, size, &common->id)
        || !read_qt_string(p, size, &common->name)
        || !read_qt_string(p, size, &common->description)
        || !read_qt_int(p, size, &common->index)) {
        free_aist_common(common);
        return FALSE;
    }
    return TRUE;
}

static gboolean
read_aist_raster(const guchar **p, gsize *size)
{
    AistRaster raster;
    gboolean ok = FALSE;

    gwy_clear(&raster, 1);

    gwy_debug("reading common");
    if (!read_aist_common(p, size, &raster.common))
        goto fail;

    gwy_debug("reading raster");
    if (!read_qt_int(p, size, &raster.xres)
        || !read_qt_int(p, size, &raster.yres)
        || !read_qt_double(p, size, &raster.left)
        || !read_qt_double(p, size, &raster.right)
        || !read_qt_double(p, size, &raster.top)
        || !read_qt_double(p, size, &raster.bottom)
        || !read_qt_string(p, size, &raster.xunits)
        || !read_qt_string(p, size, &raster.yunits)
        || !read_qt_string(p, size, &raster.zunits))
        goto fail;

    /* TODO: Read the fields. */

    ok = TRUE;

fail:
    free_aist_common(&raster.common);
    g_free(raster.xunits);
    g_free(raster.yunits);
    g_free(raster.zunits);
    return ok;
}

static gboolean
read_aist_data(const guchar **p, gsize *size)
{
    gchar *type = NULL;
    gboolean ok = FALSE;
    const guchar *datap;
    gsize datasize;
    guint len;

    if (!read_qt_string(p, size, &type)
        || !read_qt_int(p, size, &len))
        goto fail;

    if (*size < len)
        goto fail;

    datasize = len;
    datap = *p;
    if (gwy_strequal(type, "raster")) {
        gwy_debug("raster data");
        read_aist_raster(&datap, &datasize);
    }

    *size -= datasize;
    *p += datasize;

    ok = TRUE;

fail:
    g_free(type);
    return ok;
}

static gboolean
read_aist_tree(const guchar **p, gsize *size)
{
    gboolean is_data_node;
    gchar *name = NULL;
    guint i, nchildren;
    gboolean ok = FALSE;

    if (!read_qt_bool(p, size, &is_data_node))
        goto fail;

    if (is_data_node) {
        gwy_debug("reading data");
        if (!read_aist_data(p, size))
            goto fail;
    }

    if (!read_qt_string(p, size, &name)
        || !read_qt_int(p, size, &nchildren))
        goto fail;

    for (i = 0; i < nchildren; i++) {
        gwy_debug("recursing");
        ok = read_aist_tree(p, size);
        gwy_debug("returned");
        if (!ok)
            goto fail;
    }
    ok = TRUE;

fail:
    g_free(name);
    return ok;
}

static GwyContainer*
aist_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize remaining, size = 0;
    GError *err = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    p = buffer;
    remaining = size;
    read_aist_tree(&p, &remaining);

    err_NO_DATA(error);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
