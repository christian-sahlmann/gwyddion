/*
 *  $Id$
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

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <app/gwyapp.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "get.h"

#define MAGIC "AFM/Ver. "
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION ".afm"

#define Nanometer 1e-9

enum { HEADER_SIZE = 640 };

static gboolean      module_register(const gchar *name);
static gint          hitachi_detect (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* hitachi_load   (const gchar *filename);
static GwyDataField* read_data_field(const guchar *buffer,
                                     guint size);


/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Hitachi AFM files."),
    "Yeti <yeti@gwyddion.net>",
    "0.0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo hitachi_func_info = {
        "hitachi_afm",
        N_("Hitachi AFM files"),
        (GwyFileDetectFunc)&hitachi_detect,
        (GwyFileLoadFunc)&hitachi_load,
        NULL
    };

    gwy_file_func_register(name, &hitachi_func_info);

    return TRUE;
}

static gint
hitachi_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= HEADER_SIZE + 2
        && !memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
hitachi_load(const gchar *filename)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file %s", filename);
        g_clear_error(&err);
        return NULL;
    }
    if (size < HEADER_SIZE + 2) {
        g_warning("File %s is truncated", filename);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    dfield = read_data_field(buffer, size);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    return container;
}

static GwyDataField*
read_data_field(const guchar *buffer, guint size)
{
    enum {
        XREAL_OFFSET  = 0x16c,
        YREAL_OFFSET  = 0x176,
        ZSCALE_OFFSET = 0x184,
        XRES_OFFSET   = 0x1dc,
        YRES_OFFSET   = 0x1dc,
    };
    gint xres, yres, i;
    gdouble xreal, yreal, q;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data;
    const gint16 *pdata;
    const guchar *p;

    p = buffer + XRES_OFFSET;
    xres = get_DWORD(&p);
    p = buffer + YRES_OFFSET;
    yres = get_DWORD(&p);

    if (size != 2*xres*yres + HEADER_SIZE) {
        g_warning("File size doesn't match, expecting %d, got %u",
                  2*xres*yres + HEADER_SIZE, size);
        return NULL;
    }

    p = buffer + XREAL_OFFSET;
    xreal = get_DOUBLE(&p) * Nanometer;
    p = buffer + YREAL_OFFSET;
    yreal = get_DOUBLE(&p) * Nanometer;
    p = buffer + ZSCALE_OFFSET;
    q = get_DOUBLE(&p) * Nanometer;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    pdata = (const gint16*)(buffer + HEADER_SIZE);
    for (i = 0; i < xres*yres; i++)
        data[i] = GINT16_TO_LE(pdata[i])/65536.0*q;

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

