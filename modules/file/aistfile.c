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

/*
 * Warning: Madness ahead.
 *
 * Files are saved and loaded using Qt serialization facilities but
 * inconsistently so, data stored by the Qt serialization layer are in
 * big-endian byte order (because people at TrollTech think it is more portable
 * or something) while AIST does not go that far as to think about byte order
 * so they dump stuff to QByteArrays in the native order which happens to be
 * little-endian because they run it on x86.  Bite me.
 */

/*
 * TODO:
 * How to detect the format?  Is the top-level node always called noname?
 * How to parse the units?  Their idea of units is something like `Ox [um]'.
 * How to interpret curve data?  It seems to be just a single list of values.
 * What is the meaning of view data component?
 * What it the meaning of the advanced data at the end?
 * What is the bit order, alignment and interpretation of the mask bitmap?
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".aist"

#define MAGIC "\000\000\000\000\014\000n\000o\000n\000a\000m\000e"
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

typedef struct {
    AistCommon common;
    guint res;
    gchar *xunits, *yunits;
} AistCurve;

typedef struct {
    GwyContainer *container;
    gint channel_id;
    gint graph_id;
} AistContext;

static gboolean      module_register(void);
static gint          aist_detect    (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* aist_load      (const gchar *filename,
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

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

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

/* NB: Returned value is a direct pointer to the input buffer.  Do not free! */
static gboolean
read_qt_byte_array(const guchar **p, gsize *size,
                   guint *len, const guchar **value)
{
    *value = NULL;
    if (!read_qt_int(p, size, len))
        return FALSE;

    if (*size < *len)
        return FALSE;

    gwy_debug("QByteArray of length %u", *len);
    *value = *p;
    *size -= *len;
    (*p) += *len;

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

/* Lateral units are a weird thing but they contain the actual units in
 * bracket somewhere. */
static GwySIUnit*
extract_units(const gchar *label,
              gdouble *q)
{
    GwySIUnit *unit;
    gint power10;
    const gchar *s;
    gchar *t;

    if ((s = strchr(label, '['))) {
        t = g_strdup(s + 1);
        if (strchr(t, ']'))
            *strchr(t, ']') = '\0';
        unit = gwy_si_unit_new_parse(t, &power10);
        g_free(t);
    }
    else
        unit = gwy_si_unit_new_parse(label, &power10);

    *q = pow10(power10);
    return unit;
}

static GwyDataField*
make_mask_field(GwyDataField *dfield,
                const guchar *data)
{
    GwyDataField *mfield;
    guint xres, yres, i, j, rowstride;
    const guchar *datarow;
    gdouble *d;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    /* FIXME */
    rowstride = xres;

    /* Do not create the mask if it is all zero. */
    for (i = 0; i < rowstride*yres; i++) {
        if (!data[i])
            break;
    }
    if (i == rowstride*yres)
        return NULL;

    mfield = gwy_data_field_new_alike(dfield, FALSE);
    d = gwy_data_field_get_data(mfield);

    for (i = 0; i < yres; i++) {
        datarow = data + (yres-1 - i)*rowstride;
        for (j = 0; j < xres; j++) {
            *(d++) = !datarow[j];
        }
    }

    return mfield;
}

static gboolean
read_aist_raster(const guchar **p, gsize *size, AistContext *context)
{
    AistRaster raster;
    GwyDataField *dfield;
    GwySIUnit *xyunit, *zunit;
    gboolean ok = FALSE;
    guint i, j, n, len;
    const guchar *data;
    gchar *s;
    gchar key[32];
    gdouble *d;
    gdouble qxy, qz;

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

    if (!read_qt_byte_array(p, size, &len, &data))
        goto fail;
    n = raster.xres * raster.yres;
    if (len != n*sizeof(gdouble))
        goto fail;

    xyunit = extract_units(raster.xunits, &qxy);
    zunit = extract_units(raster.zunits, &qz);
    dfield = gwy_data_field_new(raster.xres, raster.yres,
                                qxy*fabs(raster.right - raster.left),
                                qxy*fabs(raster.bottom - raster.top),
                                FALSE);
    gwy_data_field_set_si_unit_xy(dfield, xyunit);
    gwy_data_field_set_si_unit_z(dfield, zunit);
    gwy_data_field_set_xoffset(dfield, qxy*MIN(raster.left, raster.right));
    gwy_data_field_set_yoffset(dfield, qxy*MIN(raster.top, raster.bottom));

    g_object_unref(xyunit);
    g_object_unref(zunit);

    d = gwy_data_field_get_data(dfield);
    for (i = 0; i < raster.yres; i++) {
        for (j = 0; j < raster.xres; j++) {
            d[(raster.yres-1 - i)*raster.xres + j]
                = qz*gwy_get_gdouble_le(&data);
        }
    }

    g_snprintf(key, sizeof(key), "/%d/data", context->channel_id);
    gwy_container_set_object_by_name(context->container, key, dfield);
    g_object_unref(dfield);

    if ((s = strchr(raster.common.name, '[')))
        s = g_strchomp(g_strndup(raster.common.name, s - raster.common.name));
    else
        s = g_strdup(raster.common.name);
    g_snprintf(key, sizeof(key), "/%d/data/title", context->channel_id);
    gwy_container_set_string_by_name(context->container, key, s);

    g_snprintf(key, sizeof(key), "/%d/mask", context->channel_id);

    context->channel_id++;

    /* At this moment we consider the loading successful. */
    ok = TRUE;

    /* The mask raster is byte-valued. */
    if (read_qt_byte_array(p, size, &len, &data)) {
        if (len == raster.xres*raster.yres) {
            dfield = make_mask_field(dfield, data);
            if (dfield) {
                gwy_container_set_object_by_name(context->container, key,
                                                 dfield);
                g_object_unref(dfield);
            }
        }

        /* TODO: what is this so-called view data? */
        read_qt_byte_array(p, size, &len, &data);
    }

fail:
    free_aist_common(&raster.common);
    g_free(raster.xunits);
    g_free(raster.yunits);
    g_free(raster.zunits);
    return ok;
}

static gboolean
read_aist_curve(const guchar **p, gsize *size, AistContext *context)
{
    AistCurve curve;
    GwySIUnit *xunit, *yunit;
    gboolean ok = FALSE;
    guint len;
    const guchar *data;
    gdouble qx, qy;

    gwy_clear(&curve, 1);

    gwy_debug("reading common");
    if (!read_aist_common(p, size, &curve.common))
        goto fail;

    gwy_debug("reading curve");
    if (!read_qt_int(p, size, &curve.res))
        goto fail;

    if (!read_qt_byte_array(p, size, &len, &data))
        goto fail;
    if (len != curve.res*sizeof(gdouble))
        goto fail;

    /* There's another array after that, then the units. */
    if (!read_qt_byte_array(p, size, &len, &data))
        goto fail;

    if (!read_qt_string(p, size, &curve.xunits)
        || !read_qt_string(p, size, &curve.yunits))
    xunit = extract_units(curve.xunits, &qx);
    yunit = extract_units(curve.yunits, &qy);

    /* TODO: Do something useful with the data.
     * How we create a y vs. x graph of it? */

    g_object_unref(xunit);
    g_object_unref(yunit);

    context->graph_id++;

    ok = TRUE;

fail:
    free_aist_common(&curve.common);
    g_free(curve.xunits);
    g_free(curve.yunits);
    return ok;
}

static gboolean
read_aist_data(const guchar **p, gsize *size, AistContext *context)
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

    /* Do this even if we fail, data are skippable. */
    *size -= datasize;
    *p += datasize;

    if (gwy_strequal(type, "raster")) {
        gwy_debug("raster data");
        if (!read_aist_raster(&datap, &datasize, context))
            goto fail;
    }
    else if (gwy_strequal(type, "curve")) {
        gwy_debug("curve data");
        if (!read_aist_curve(&datap, &datasize, context))
            goto fail;
    }

    ok = TRUE;

fail:
    g_free(type);
    return ok;
}

static gboolean
read_aist_tree(const guchar **p, gsize *size, AistContext *context)
{
    gboolean is_data_node;
    gchar *name = NULL;
    guint i, nchildren;
    gboolean ok = FALSE;

    if (!read_qt_bool(p, size, &is_data_node))
        goto fail;

    if (is_data_node) {
        gwy_debug("reading data");
        if (!read_aist_data(p, size, context))
            goto fail;
    }

    if (!read_qt_string(p, size, &name)
        || !read_qt_int(p, size, &nchildren))
        goto fail;

    for (i = 0; i < nchildren; i++) {
        gwy_debug("recursing");
        ok = read_aist_tree(p, size, context);
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
    AistContext context;
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

    context.container = gwy_container_new();
    context.channel_id = 0;
    context.graph_id = 0;
    read_aist_tree(&p, &remaining, &context);

    gwy_file_abandon_contents(buffer, size, NULL);

    if (context.channel_id == 0) {
        g_object_unref(context.container);
        context.container = NULL;
        err_NO_DATA(error);
    }

    return context.container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
