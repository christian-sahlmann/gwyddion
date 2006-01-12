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
static GwyContainer* sdfile_load            (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static gboolean      check_params           (const SDFile *sdfile,
                                             guint len,
                                             GError **error);
static gboolean      sdfile_read_header_bin (const guchar **p,
                                             gsize *len,
                                             SDFile *sdfile,
                                             GError **error);
static gboolean      sdfile_read_header_text(const guchar **buffer,
                                             gsize *len,
                                             SDFile *sdfile,
                                             gint *steps,
                                             GError **error);
static gchar*        sdfile_next_line       (gchar **buffer,
                                             const gchar *key,
                                             GError **error);
static GwyDataField* sdfile_read_data_bin   (SDFile *sdfile);
static GwyDataField* sdfile_read_data_text  (SDFile *sdfile,
                                             GError **error);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Surfstand group SDF (Surface Data File) files."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
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
        "sdfile",
        N_("Surfstand SDF files"),
        (GwyFileDetectFunc)&sdfile_detect,
        (GwyFileLoadFunc)&sdfile_load,
        NULL,
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
    gint steps;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    p = fileinfo->buffer;
    len = fileinfo->buffer_len;
    if (sdfile_read_header_bin(&p, &len, &sdfile, NULL)
        && SDF_HEADER_SIZE_BIN + sdfile.expected_size == fileinfo->file_size
        && !sdfile.compression
        && !sdfile.check_type)
        return 100;

    p = fileinfo->buffer;
    len = fileinfo->buffer_len;
    if (sdfile_read_header_text(&p, &len, &sdfile, &steps, NULL)
        && sdfile.expected_size <= fileinfo->file_size
        && !sdfile.compression
        && !sdfile.check_type)
        return 100;

    return 0;
}

static GwyContainer*
sdfile_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    SDFile sdfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const guchar *p;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GwySIUnit *siunit;
    gint steps;

    steps = 0;
    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    "%s", err->message);
        g_clear_error(&err);
        return NULL;
    }
    len = size;
    p = buffer;
    if (sdfile_read_header_text(&p, &len, &sdfile, &steps, error)) {
        if (check_params(&sdfile, len, error))
            dfield = sdfile_read_data_text(&sdfile, error);
    }
    else if (steps == 0) {
        p = buffer;
        len = size;
        if (sdfile_read_header_bin(&p, &len, &sdfile, error)) {
            if (check_params(&sdfile, len, error))
                dfield = sdfile_read_data_bin(&sdfile);
        }
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

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
check_params(const SDFile *sdfile,
             guint len,
             GError **error)
{
    if (sdfile->data_type >= SDF_NTYPES) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unsupported value of DataType: %u."),
                    sdfile->data_type);
        return FALSE;
    }
    if (sdfile->expected_size > len) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected data size %u bytes, but found %u bytes."),
                    sdfile->expected_size, len);
        return FALSE;
    }
    if (sdfile->compression) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unsupported value of Compression: %d."),
                    sdfile->compression);
        return FALSE;
    }
    if (sdfile->check_type) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unsupported value of CheckType: %d."),
                    sdfile->check_type);
        return FALSE;
    }

    return TRUE;
}

static gboolean
sdfile_read_header_bin(const guchar **p,
                       gsize *len,
                       SDFile *sdfile,
                       GError **error)
{
    if (*len < SDF_HEADER_SIZE_BIN) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is too short."));
        return FALSE;
    }

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

#define NEXT(line, key, val, error) \
    if (!(val = sdfile_next_line(&line, key, error))) { \
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                    _("Missing `%s' header field."), key); \
        return FALSE; \
    }

#define READ_STRING(line, key, val, field, error) \
    NEXT(line, key, val, error) \
    strncpy(field, val, sizeof(field));

#define READ_INT(line, key, val, field, check, error) \
    NEXT(line, key, val, error) \
    field = atoi(val); \
    if (check && field <= 0) { \
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                    _("Wrong `%s' value: %d."), key, field); \
        return FALSE; \
    }

#define READ_FLOAT(line, key, val, field, check, error) \
    NEXT(line, key, val, error) \
    field = g_ascii_strtod(val, NULL); \
    if (check && field <= 0.0) { \
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                    _("Wrong `%s' value: %g."), key, field); \
        return FALSE; \
    }

static gboolean
sdfile_read_header_text(const guchar **buffer,
                        gsize *len,
                        SDFile *sdfile,
                        gint *steps,
                        GError **error)
{
    enum { PING_SIZE = 400 };
    gchar *val, *p, *header;
    gsize size;

    /* We do not need exact lenght of the minimum file */
    *steps = 0;
    if (*len < 160) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is too short."));
        return FALSE;
    }

    /* Make a nul-terminated copy */
    size = MIN(*len+1, PING_SIZE);
    header = g_newa(gchar, size);
    memcpy(header, *buffer, size-1);
    header[size-1] = '\0';

    memset(sdfile, 0, sizeof(SDFile));
    p = header;

    val = g_strstrip(gwy_str_next_line(&p));
    strncpy(sdfile->version, val, sizeof(sdfile->version));

    READ_STRING(p, "ManufacID", val, sdfile->manufacturer, error)
    (*steps)++;
    READ_STRING(p, "CreateDate", val, sdfile->creation, error)
    READ_STRING(p, "ModDate", val, sdfile->modification, error)
    READ_INT(p, "NumPoints", val, sdfile->xres, TRUE, error)
    READ_INT(p, "NumProfiles", val, sdfile->yres, TRUE, error)
    (*steps)++;
    READ_FLOAT(p, "Xscale", val, sdfile->xscale, TRUE, error)
    READ_FLOAT(p, "Yscale", val, sdfile->yscale, TRUE, error)
    READ_FLOAT(p, "Zscale", val, sdfile->zscale, TRUE, error)
    READ_FLOAT(p, "Zresolution", val, sdfile->zres, FALSE, error)
    (*steps)++;
    READ_INT(p, "Compression", val, sdfile->compression, FALSE, error)
    READ_INT(p, "DataType", val, sdfile->data_type, FALSE, error)
    READ_INT(p, "CheckType", val, sdfile->check_type, FALSE, error)
    (*steps)++;

    /* at least */
    if (sdfile->data_type < SDF_NTYPES)
        sdfile->expected_size = 2*sdfile->xres * sdfile->yres;
    else
        sdfile->expected_size = -1;

    val = g_strstrip(gwy_str_next_line(&p));
    if (!val || *val != '*') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing data start marker (*)."));
        return FALSE;
    }

    *buffer += p - header;
    *len -= p - header;
    sdfile->data = *buffer;
    return TRUE;
}

static gchar*
sdfile_next_line(gchar **buffer,
                 const gchar *key,
                 GError **error)
{
    guint klen;
    gchar *value, *line;

    line = gwy_str_next_line(buffer);
    if (!line) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("End of file reached when looking for `%s' field."), key);
        return NULL;
    }

    klen = strlen(key);
    if (strncmp(line, key, klen) != 0
        || !g_ascii_isspace(line[klen])) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Invalid line found when looking for `%s' field."), key);
        return NULL;
    }

    value = line + klen;
    g_strstrip(value);

    return value;
}

static GwyDataField*
sdfile_read_data_bin(SDFile *sdfile)
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
sdfile_read_data_text(SDFile *sdfile,
                      GError **error)
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
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("End of file reached when reading sample #%d "
                              "of %d"), i, n);
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
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("End of file reached when reading sample #%d "
                              "of %d"), i, n);
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

