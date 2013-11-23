/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

/**
 * [FILE-MAGIC-USERGUIDE]
 * S94 STM files
 * .s94
 * Read
 **/

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwyapp.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define EXTENSION ".s94"

#define Nanometre (1e-9)
#define NanoAmpere (1e-9)

enum {
    HEADER_LEN = 60
};

typedef enum {
    S94_TOPOGRAPHY = 0,
    S94_CURRENT = 1,
} S94ImageMode;

typedef struct {
    guint xres;
    guint yres;
    gboolean swapxy;
    S94ImageMode mode;
    guint orig_imageno;
    gdouble xreal;
    gdouble yreal;
    gdouble xoff;
    gdouble yoff;
    gdouble scanspeed;
    gdouble voltage;
    guint zgain;      /* should be 1 to 3 */
    guint section;    /* xy ramp amplifier; should be 1 to 3 */
    gdouble Kp;
    gdouble Tn;
    gdouble Tv;
    gdouble It;
    guint angle;
    guint zflag;      /* unused */
    /* calculated */
    gdouble q;
} S94File;

static gboolean      module_register(void);
static gint          s94_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* s94_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gboolean      s94_read_header(S94File *s94file,
                                     const guchar *p,
                                     gsize size,
                                     GError **error);
static GwyContainer* s94_get_meta   (const S94File *s94file);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports S94 STM data files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("s94file",
                           N_("S94 STM files (.s94)"),
                           (GwyFileDetectFunc)&s94_detect,
                           (GwyFileLoadFunc)&s94_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
s94_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    guint xres, yres, swap, zgain, section;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len <= HEADER_LEN)
        return 0;

    xres = ((guint)fileinfo->head[1] << 8 | fileinfo->head[0]);
    yres = ((guint)fileinfo->head[3] << 8 | fileinfo->head[2]);
    swap = ((guint)fileinfo->head[5] << 8 | fileinfo->head[4]);
    zgain = ((guint)fileinfo->head[37] << 8 | fileinfo->head[36]);
    section = ((guint)fileinfo->head[39] << 8 | fileinfo->head[38]);
    if (2*xres*yres + HEADER_LEN == fileinfo->file_size
        && (swap == 0 || swap == 1)
        && (zgain >= 1 && zgain <= 3)
        && (section >= 1 && section <= 3))
        return 80;

    return 0;
}

static GwyContainer*
s94_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL, *meta;
    guchar *buffer = NULL;
    const gchar *title, *unit;
    gsize size = 0;
    GError *err = NULL;
    S94File s94file;
    GwyDataField *dfield;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (!s94_read_header(&s94file, buffer, size, error))
        goto fail;

    dfield = gwy_data_field_new(s94file.xres, s94file.yres,
                                s94file.xreal * Nanometre,
                                s94file.yreal * Nanometre,
                                FALSE);
    gwy_convert_raw_data(buffer + HEADER_LEN, s94file.xres*s94file.yres,
                         1, GWY_RAW_DATA_SINT16, GWY_BYTE_ORDER_LITTLE_ENDIAN,
                         dfield->data, s94file.q, 0.0);
    gwy_data_field_set_xoffset(dfield, s94file.xoff * Nanometre);
    gwy_data_field_set_yoffset(dfield, s94file.yoff * Nanometre);

    if (s94file.swapxy) {
        gwy_data_field_rotate(dfield, 90, GWY_INTERPOLATION_ROUND);
        gwy_data_field_invert(dfield, FALSE, TRUE, FALSE);
    }

    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_xy(dfield), "m");
    unit = gwy_enuml_to_string(s94file.mode,
                               "m", S94_TOPOGRAPHY,
                               "A", S94_CURRENT,
                                NULL);
    /* Keep the data unitless otherwise. */
    gwy_si_unit_set_from_string(gwy_data_field_get_si_unit_z(dfield), unit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    title = gwy_enuml_to_string(s94file.mode,
                                "Topography", S94_TOPOGRAPHY,
                                "Current", S94_CURRENT,
                                NULL);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup(title ? title : "Unknown"));

    meta = s94_get_meta(&s94file);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

fail:
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static gboolean
s94_read_header(S94File *s94file,
                const guchar *p,
                gsize size,
                GError **error)
{
    if (size <= HEADER_LEN) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    s94file->xres = gwy_get_guint16_le(&p);
    s94file->yres = gwy_get_guint16_le(&p);
    s94file->swapxy = gwy_get_guint16_le(&p);
    s94file->mode = gwy_get_guint16_le(&p);
    s94file->orig_imageno = gwy_get_guint32_le(&p);
    s94file->xreal = gwy_get_gfloat_le(&p);
    s94file->yreal = gwy_get_gfloat_le(&p);
    s94file->xoff = gwy_get_gfloat_le(&p);
    s94file->yoff = gwy_get_gfloat_le(&p);
    s94file->scanspeed = gwy_get_gfloat_le(&p);
    s94file->voltage = gwy_get_gfloat_le(&p);
    s94file->zgain = gwy_get_guint16_le(&p);
    s94file->section = gwy_get_guint16_le(&p);
    s94file->Kp = gwy_get_gfloat_le(&p);
    s94file->Tn = gwy_get_gfloat_le(&p);
    s94file->Tv = gwy_get_gfloat_le(&p);
    s94file->It = gwy_get_gfloat_le(&p);
    s94file->angle = gwy_get_guint16_le(&p);
    s94file->zflag = gwy_get_guint16_le(&p);

    if (err_DIMENSION(error, s94file->xres)
        || err_DIMENSION(error, s94file->yres))
        return FALSE;

    if (err_SIZE_MISMATCH(error, 2*s94file->xres*s94file->yres + HEADER_LEN,
                          size, TRUE))
        return FALSE;

    if (!(s94file->xreal = fabs(s94file->xreal))) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        s94file->xreal = 1.0;
    }
    if (!(s94file->yreal = fabs(s94file->yreal))) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        s94file->yreal = 1.0;
    }

    if (s94file->mode == S94_TOPOGRAPHY)
        s94file->q = 5.5/65536.0 * (1 << (2*(s94file->zgain - 1))) * Nanometre;
    else if (s94file->mode == S94_CURRENT)
        s94file->q = 20.0/65536.0 * NanoAmpere;
    else {
        g_warning("Unknown mode %u.\n", s94file->mode);
        s94file->q = 1.0/65536.0;
    }

    return TRUE;
}

static GwyContainer*
s94_get_meta(const S94File *s94file)
{
    GwyContainer *meta = gwy_container_new();

    gwy_container_set_string_by_name(meta, "Image number (original)",
                                     g_strdup_printf("%u",
                                                     s94file->orig_imageno));
    gwy_container_set_string_by_name(meta, "Image mode",
                                     g_strdup_printf("%u", s94file->mode));
    gwy_container_set_string_by_name(meta, "X points",
                                     g_strdup_printf("%u", s94file->xres));
    gwy_container_set_string_by_name(meta, "Y points",
                                     g_strdup_printf("%u", s94file->yres));
    gwy_container_set_string_by_name(meta, "X size",
                                     g_strdup_printf("%g nm", s94file->xreal));
    gwy_container_set_string_by_name(meta, "Y size",
                                     g_strdup_printf("%g nm", s94file->yreal));
    gwy_container_set_string_by_name(meta, "X offset",
                                     g_strdup_printf("%g nm", s94file->xoff));
    gwy_container_set_string_by_name(meta, "Y offset",
                                     g_strdup_printf("%g nm", s94file->yoff));
    gwy_container_set_string_by_name(meta, "X/Y swapped",
                                     g_strdup(s94file->swapxy ? "Yes" : "No"));
    gwy_container_set_string_by_name(meta, "Scan angle",
                                     g_strdup_printf("%u deg", s94file->angle));
    gwy_container_set_string_by_name(meta, "Scan speed",
                                     g_strdup_printf("%g nm/s",
                                                     s94file->scanspeed));
    gwy_container_set_string_by_name(meta, "Z gain",
                                     g_strdup_printf("%u", s94file->zgain));
    gwy_container_set_string_by_name(meta, "Section",
                                     g_strdup_printf("%u", s94file->section));
    gwy_container_set_string_by_name(meta, "Kp",
                                     g_strdup_printf("%g", s94file->Kp));
    gwy_container_set_string_by_name(meta, "Tn",
                                     g_strdup_printf("%g", s94file->Tn));
    gwy_container_set_string_by_name(meta, "Tv",
                                     g_strdup_printf("%g", s94file->Tv));
    gwy_container_set_string_by_name(meta, "It",
                                     g_strdup_printf("%g", s94file->It));

    return meta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
