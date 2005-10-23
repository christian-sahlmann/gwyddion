/*
 *  $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klapetek@gwyddion.net.
 *
 *  Roughly based on code in Kasgira by MV <kasigra@seznam.cz>.
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

#define EXTENSION ".sdf"

enum {
    SDF_HEADER_SIZE_BIN = 8 + 10 + 2*12 + 2*2 + 4*8 + 3*1
};

typedef enum {
    SDF_UINT8  = 0,
    SDF_UINT16 = 1,
    SDF_UINT32 = 2,
    SDF_FLOAT  = 3,
    SDF_SINT8  = 4,
    SDF_SINT16 = 5,
    SDF_SINT32 = 6,
    SDF_DOUBLE = 7,
    SDF_NTYPES
} SDFDataType;

typedef struct {
    gchar version[8];
    gchar manufacturer[10];
    gchar creation[12];
    gchar modification[12];
    gint xres;
    gint yres;
    gdouble xscale;
    gdouble yscale;
    gdouble zscale;
    gdouble zres;
    gint compression;
    SDFDataType data_type;
    gint check_type;
    const guchar *data;

    gint expected_size;
} SDFile;

static gboolean      module_register        (const gchar *name);
static gint          sdfile_detect          (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* sdfile_load            (const gchar *filename);
static gboolean      sdfile_read_header_bin (const guchar **p,
                                             gsize *len,
                                             SDFile *sdfile);
static gboolean      sdfile_read_header_text(const guchar **p,
                                             gsize *len,
                                             SDFile *sdfile);
static GwyDataField* read_data_field_bin    (SDFile *sdfile);
static GwyDataField* read_data_field_text   (SDFile *sdfile);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Surfstand group SDF (Surface Data File) files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

static const guint type_sizes[] = { 1, 2, 4, 4, 1, 2, 4, 8 };

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo sdfile_func_info = {
        "sdfile_afm",
        N_("Surfstand SDF files"),
        (GwyFileDetectFunc)&sdfile_detect,
        (GwyFileLoadFunc)&sdfile_load,
        NULL
    };

    gwy_file_func_register(name, &sdfile_func_info);

    return TRUE;
}

static gint
sdfile_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    SDFile sdfile;
    const guchar *p;
    gsize len;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    p = fileinfo->buffer;
    len = fileinfo->buffer_len;
    if (sdfile_read_header_bin(&p, &len, &sdfile)
        && SDF_HEADER_SIZE_BIN + sdfile.expected_size == fileinfo->file_size
        && !sdfile.compression
        && !sdfile.check_type)
        return 100;

    p = fileinfo->buffer;
    len = fileinfo->buffer_len;
    if (sdfile_read_header_text(&p, &len, &sdfile)
        && sdfile.expected_size <= fileinfo->file_size
        && !sdfile.compression
        && !sdfile.check_type)
        return 100;

    return 0;
}

static GwyContainer*
sdfile_load(const gchar *filename)
{
    SDFile sdfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwySIUnit *siunit;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_warning("Cannot read file `%s'", filename);
        g_clear_error(&err);
        return NULL;
    }
    p = buffer;
    len = size;
    if (sdfile_read_header_bin(&p, &len, &sdfile)
        && sdfile.expected_size == len
        && !sdfile.compression
        && !sdfile.check_type)
        dfield = read_data_field_bin(&sdfile);
    else {
        p = buffer;
        len = size;
        if (sdfile_read_header_text(&p, &len, &sdfile)
            && sdfile.expected_size <= len
            && !sdfile.compression
            && !sdfile.check_type)
            dfield = read_data_field_text(&sdfile);
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield) {
        g_warning("Failed to read data from `%s'", filename);
        return NULL;
    }

    gwy_data_field_multiply(dfield, sdfile.zscale);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);

    return container;
}

static gboolean
sdfile_read_header_bin(const guchar **p,
                       gsize *len,
                       SDFile *sdfile)
{
    if (*len < SDF_HEADER_SIZE_BIN)
        return FALSE;

    memset(sdfile, 0, sizeof(SDFile));
    get_CHARARRAY(sdfile->version, p);
    get_CHARARRAY(sdfile->manufacturer, p);
    get_CHARARRAY(sdfile->creation, p);
    get_CHARARRAY(sdfile->modification, p);
    sdfile->xres = get_WORD(p);
    sdfile->yres = get_WORD(p);
    sdfile->xscale = get_DOUBLE(p);
    sdfile->yscale = get_DOUBLE(p);
    sdfile->zscale = get_DOUBLE(p);
    sdfile->zres = get_DOUBLE(p);
    sdfile->compression = **p;
    (*p)++;
    sdfile->data_type = **p;
    (*p)++;
    sdfile->check_type = **p;
    (*p)++;
    sdfile->data = *p;

    if (sdfile->data_type < SDF_NTYPES)
        sdfile->expected_size = type_sizes[sdfile->data_type]
                                * sdfile->xres * sdfile->yres;
    else
        sdfile->expected_size = -1;

    *len -= SDF_HEADER_SIZE_BIN;
    return TRUE;
}

static gboolean
sdfile_read_header_text(const guchar **p,
                        gsize *len,
                        SDFile *sdfile)
{
    enum { PING_SIZE = 400 };
    gchar *star;
    guint data_type;
    gchar *header;
    gsize size;

    /* We do not need exact lenght of the minimum file */
    if (*len < 160)
        return FALSE;

    /* Make a nul-terminated copy */
    size = MIN(*len+1, PING_SIZE);
    header = g_newa(gchar, size);
    memcpy(header, *p, size-1);
    header[size-1] = '\0';

    memset(sdfile, 0, sizeof(SDFile));
    if (sscanf(header,
               "%8s "
               "ManufacID %10s "
               "CreateDate %12s "
               "ModDate %12s "
               "NumPoints %d "
               "NumProfiles %d "
               "Xscale %lf "
               "Yscale %lf "
               "Zscale %lf "
               "Zresolution %lf "
               "Compression %d "
               "DataType %u "
               "CheckType %d "
               "*",
               sdfile->version,
               sdfile->manufacturer,
               sdfile->creation,
               sdfile->modification,
               &sdfile->xres,
               &sdfile->yres,
               &sdfile->xscale,
               &sdfile->yscale,
               &sdfile->zscale,
               &sdfile->zres,
               &sdfile->compression,
               &data_type,
               &sdfile->check_type) != 13)
        return FALSE;
    sdfile->data_type = data_type;

    /* at least */
    if (sdfile->data_type < SDF_NTYPES)
        sdfile->expected_size = 2*sdfile->xres * sdfile->yres;
    else
        sdfile->expected_size = -1;

    /* find standalone `*' */
    star = header + 128;
    do {
        star = memchr(star, '*', size - (star - header) - 1);
        if (g_ascii_isspace(*(star-1)) && g_ascii_isspace(*(star+1))) {
            star++;
            *len -= star - header;
            *p += star - header;
            sdfile->data = *p;
            return TRUE;
        }
        star++;
    } while (star + 1 < header + size);

    g_assert_not_reached();

    return FALSE;
}

static GwyDataField*
read_data_field_bin(SDFile *sdfile)
{
    gint i, n;
    GwyDataField *dfield;
    gdouble *data;
    const guchar *p;

    dfield = gwy_data_field_new(sdfile->xres, sdfile->yres,
                                sdfile->xres * sdfile->xscale,
                                sdfile->yres * sdfile->yscale,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    n = sdfile->xres * sdfile->yres;
    /* FIXME: we assume Intel byteorder, although the format does not specify
     * any order.  But it was developed in PC context... */
    switch (sdfile->data_type) {
        case SDF_UINT8:
        for (i = 0; i < n; i++)
            data[i] = sdfile->data[i];
        break;

        case SDF_SINT8:
        for (i = 0; i < n; i++)
            data[i] = (signed char)sdfile->data[i];
        break;

        case SDF_UINT16:
        {
            const guint16 *pdata = (const guint16*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GUINT16_FROM_LE(pdata[i]);
        }
        break;

        case SDF_SINT16:
        {
            const gint16 *pdata = (const gint16*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GINT16_FROM_LE(pdata[i]);
        }
        break;

        case SDF_UINT32:
        {
            const guint32 *pdata = (const guint32*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GUINT32_FROM_LE(pdata[i]);
        }
        break;

        case SDF_SINT32:
        {
            const gint32 *pdata = (const gint32*)(sdfile->data);

            for (i = 0; i < n; i++)
                data[i] = GINT32_FROM_LE(pdata[i]);
        }
        break;

        case SDF_FLOAT:
        p = sdfile->data;
        for (i = 0; i < n; i++)
            data[i] = get_FLOAT(&p);
        break;

        case SDF_DOUBLE:
        p = sdfile->data;
        for (i = 0; i < n; i++)
            data[i] = get_DOUBLE(&p);
        break;

        default:
        g_object_unref(dfield);
        g_return_val_if_reached(NULL);
        break;
    }

    return dfield;
}

static GwyDataField*
read_data_field_text(SDFile *sdfile)
{
    gint i, n;
    GwyDataField *dfield;
    gdouble *data;
    const gchar *p, *end;

    dfield = gwy_data_field_new(sdfile->xres, sdfile->yres,
                                sdfile->xres * sdfile->xscale,
                                sdfile->yres * sdfile->yscale,
                                FALSE);
    data = gwy_data_field_get_data(dfield);
    n = sdfile->xres * sdfile->yres;
    switch (sdfile->data_type) {
        case SDF_UINT8:
        case SDF_SINT8:
        case SDF_UINT16:
        case SDF_SINT16:
        case SDF_UINT32:
        case SDF_SINT32:
        p = (const gchar*)sdfile->data;
        for (i = 0; i < n; i++) {
            data[i] = strtol(p, (gchar**)&end, 10);
            if (p == end) {
                g_object_unref(dfield);
                return NULL;
            }
            p = end;
        }
        break;

        case SDF_FLOAT:
        case SDF_DOUBLE:
        p = (const gchar*)sdfile->data;
        for (i = 0; i < n; i++) {
            data[i] = g_ascii_strtod(p, (gchar**)&end);
            if (p == end) {
                g_object_unref(dfield);
                return NULL;
            }
            p = end;
        }
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

