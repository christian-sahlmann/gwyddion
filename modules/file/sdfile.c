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
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"
#include "get.h"

#define EXTENSION ".sdf"
#define MICROMAP_EXTENSION ".sdfa"

#define Micrometer (1e-6)

enum {
    SDF_HEADER_SIZE_BIN = 8 + 10 + 2*12 + 2*2 + 4*8 + 3*1
};

enum {
    SDF_MIN_TEXT_SIZE = 160
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
    GHashTable *extras;
    gchar *data;

    gint expected_size;
} SDFile;

static gboolean      module_register        (void);
static gint          sdfile_detect_bin      (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static gint          sdfile_detect_text     (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static gint          micromap_detect        (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* sdfile_load_bin        (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static GwyContainer* sdfile_load_text       (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static GwyContainer* micromap_load          (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static gboolean      check_params           (const SDFile *sdfile,
                                             guint len,
                                             GError **error);
static gboolean      sdfile_read_header_bin (const guchar **p,
                                             gsize *len,
                                             SDFile *sdfile,
                                             GError **error);
static gboolean      sdfile_read_header_text(gchar **buffer,
                                             gsize *len,
                                             SDFile *sdfile,
                                             GError **error);
static gchar*        sdfile_next_line       (gchar **buffer,
                                             const gchar *key,
                                             GError **error);
static GwyDataField* sdfile_read_data_bin   (SDFile *sdfile);
static GwyDataField* sdfile_read_data_text  (SDFile *sdfile,
                                             GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Surfstand group SDF (Surface Data File) files."),
    "Yeti <yeti@gwyddion.net>",
    "0.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

static const guint type_sizes[] = { 1, 2, 4, 4, 1, 2, 4, 8 };

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sdfile-bin",
                           N_("Surfstand SDF files, binary (.sdf)"),
                           (GwyFileDetectFunc)&sdfile_detect_bin,
                           (GwyFileLoadFunc)&sdfile_load_bin,
                           NULL,
                           NULL);
    gwy_file_func_register("sdfile-txt",
                           N_("Surfstand SDF files, text (.sdf)"),
                           (GwyFileDetectFunc)&sdfile_detect_text,
                           (GwyFileLoadFunc)&sdfile_load_text,
                           NULL,
                           NULL);
    gwy_file_func_register("micromap",
                           N_("Micromap SDF files (.sdfa)"),
                           (GwyFileDetectFunc)&micromap_detect,
                           (GwyFileLoadFunc)&micromap_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sdfile_detect_bin(const GwyFileDetectInfo *fileinfo,
                  gboolean only_name)
{
    SDFile sdfile;
    const gchar *p;
    gsize len;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    p = fileinfo->head;
    len = fileinfo->buffer_len;
    if (len <= SDF_HEADER_SIZE_BIN || fileinfo->head[0] != 'b')
        return 0;
    if (sdfile_read_header_bin((const guchar**)&p, &len, &sdfile, NULL)
        && SDF_HEADER_SIZE_BIN + sdfile.expected_size == fileinfo->file_size
        && !sdfile.compression
        && !sdfile.check_type)
        return 90;

    return 0;
}

static gint
sdfile_detect_text(const GwyFileDetectInfo *fileinfo,
                   gboolean only_name)
{
    SDFile sdfile;
    gchar *buffer, *p;
    gsize len;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    len = fileinfo->buffer_len;
    if (len <= SDF_MIN_TEXT_SIZE || fileinfo->head[0] != 'a')
        return 0;

    buffer = p = g_memdup(fileinfo->head, len);
    if (sdfile_read_header_text(&p, &len, &sdfile, NULL)
        && sdfile.expected_size <= fileinfo->file_size
        && !sdfile.compression
        && !sdfile.check_type)
        score = 90;

    g_free(buffer);

    return score;
}

/* Perform generic SDF detection, then check for Micromap specific stuff */
static gint
micromap_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    SDFile sdfile;
    gchar *buffer, *p;
    gsize len;
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase,
                                MICROMAP_EXTENSION) ? 18 : 0;

    len = fileinfo->buffer_len;
    if (len <= SDF_MIN_TEXT_SIZE || fileinfo->head[0] != 'a')
        return 0;

    buffer = p = g_memdup(fileinfo->head, len);
    if (sdfile_read_header_text(&p, &len, &sdfile, NULL)
        && sdfile.expected_size <= fileinfo->file_size
        && !sdfile.compression
        && !sdfile.check_type
        && strncmp(sdfile.manufacturer, "Micromap", sizeof("Micromap")-1) == 0
        && strstr(fileinfo->tail, "OBJECTIVEMAG")
        && strstr(fileinfo->tail, "TUBEMAG")
        && strstr(fileinfo->tail, "CAMERAXPIXEL")
        && strstr(fileinfo->tail, "CAMERAYPIXEL"))
        score = 100;

    g_free(buffer);

    return score;
}

static void
sdfile_set_units(SDFile *sdfile,
                 GwyDataField *dfield)
{
    GwySIUnit *siunit;

    gwy_data_field_multiply(dfield, sdfile->zscale);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);
}

static GwyContainer*
sdfile_load_bin(const gchar *filename,
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

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    len = size;
    p = buffer;
    if (sdfile_read_header_bin(&p, &len, &sdfile, error)) {
        if (check_params(&sdfile, len, error))
            dfield = sdfile_read_data_bin(&sdfile);
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    if (!dfield)
        return NULL;

    sdfile_set_units(&sdfile, dfield);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));

    return container;
}

static GwyContainer*
sdfile_load_text(const gchar *filename,
                 G_GNUC_UNUSED GwyRunType mode,
                 GError **error)
{
    SDFile sdfile;
    GwyContainer *container = NULL;
    gchar *p, *buffer = NULL;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    len = size;
    p = buffer;
    if (sdfile_read_header_text(&p, &len, &sdfile, error)) {
        if (check_params(&sdfile, len, error))
            dfield = sdfile_read_data_text(&sdfile, error);
    }

    if (!dfield) {
        g_free(buffer);
        return NULL;
    }

    sdfile_set_units(&sdfile, dfield);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));

    g_free(buffer);
    if (sdfile.extras)
        g_hash_table_destroy(sdfile.extras);

    return container;
}

#define GET_MICROMAP_PARAM(hash, key, val, error, var) \
    if (!(val = g_hash_table_lookup(hash, key))) { \
        err_MISSING_FIELD(error, key); \
        goto fail; \
    } \
    var = g_ascii_strtod(val, NULL)

static GwyContainer*
micromap_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    SDFile sdfile;
    GwyContainer *container = NULL;
    gchar *p, *buffer = NULL;
    gsize len, size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gdouble objectivemag, tubemag, cameraxpixel, cameraypixel;
    const gchar *val;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    len = size;
    p = buffer;
    if (sdfile_read_header_text(&p, &len, &sdfile, error)) {
        if (check_params(&sdfile, len, error))
            dfield = sdfile_read_data_text(&sdfile, error);
    }
    if (!dfield) {
        g_free(buffer);
        return NULL;
    }

    if (!sdfile.extras) {
        err_MISSING_FIELD(error, "OBJECTIVEMAG");
        goto fail;
    }
    GET_MICROMAP_PARAM(sdfile.extras, "OBJECTIVEMAG", val, error, objectivemag);
    GET_MICROMAP_PARAM(sdfile.extras, "TUBEMAG", val, error, tubemag);
    GET_MICROMAP_PARAM(sdfile.extras, "CAMERAXPIXEL", val, error, cameraxpixel);
    GET_MICROMAP_PARAM(sdfile.extras, "CAMERAYPIXEL", val, error, cameraypixel);

    sdfile_set_units(&sdfile, dfield);
    gwy_data_field_set_xreal(dfield,
                             Micrometer * sdfile.xres * objectivemag
                             * tubemag * cameraxpixel);
    gwy_data_field_set_yreal(dfield,
                             Micrometer * sdfile.yres * objectivemag
                             * tubemag * cameraypixel);

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    gwy_container_set_string_by_name(container, "/0/data/title",
                                     g_strdup("Topography"));

fail:
    g_object_unref(dfield);
    g_free(buffer);
    if (sdfile.extras)
        g_hash_table_destroy(sdfile.extras);

    return container;
}

static gboolean
check_params(const SDFile *sdfile,
             guint len,
             GError **error)
{
    if (sdfile->data_type >= SDF_NTYPES) {
        err_DATA_TYPE(error, sdfile->data_type);
        return FALSE;
    }
    if (sdfile->expected_size > len) {
        err_SIZE_MISMATCH(error, sdfile->expected_size, len);
        return FALSE;
    }
    if (sdfile->compression) {
        err_UNSUPPORTED(error, "Compression");
        return FALSE;
    }
    if (sdfile->check_type) {
        err_UNSUPPORTED(error, "CheckType");
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
        err_TOO_SHORT(error);
        return FALSE;
    }

    memset(sdfile, 0, sizeof(SDFile));
    get_CHARARRAY(sdfile->version, p);
    get_CHARARRAY(sdfile->manufacturer, p);
    get_CHARARRAY(sdfile->creation, p);
    get_CHARARRAY(sdfile->modification, p);
    sdfile->xres = gwy_get_guint16_le(p);
    sdfile->yres = gwy_get_guint16_le(p);
    sdfile->xscale = gwy_get_gdouble_le(p);
    sdfile->yscale = gwy_get_gdouble_le(p);
    sdfile->zscale = gwy_get_gdouble_le(p);
    sdfile->zres = gwy_get_gdouble_le(p);
    sdfile->compression = **p;
    (*p)++;
    sdfile->data_type = **p;
    (*p)++;
    sdfile->check_type = **p;
    (*p)++;
    sdfile->data = (gchar*)*p;

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
        err_MISSING_FIELD(error, key); \
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
                    _("Invalid `%s' value: %d."), key, field); \
        return FALSE; \
    }

#define READ_FLOAT(line, key, val, field, check, error) \
    NEXT(line, key, val, error) \
    field = g_ascii_strtod(val, NULL); \
    if (check && field <= 0.0) { \
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                    _("Invalid `%s' value: %g."), key, field); \
        return FALSE; \
    }

/* NB: Buffer must be writable and nul-terminated, its initial part is
 * overwritten */
static gboolean
sdfile_read_header_text(gchar **buffer,
                        gsize *len,
                        SDFile *sdfile,
                        GError **error)
{
    gchar *val, *p;

    /* We do not need exact lenght of the minimum file */
    if (*len < SDF_MIN_TEXT_SIZE) {
        err_TOO_SHORT(error);
        return FALSE;
    }

    memset(sdfile, 0, sizeof(SDFile));
    p = *buffer;

    val = g_strstrip(gwy_str_next_line(&p));
    strncpy(sdfile->version, val, sizeof(sdfile->version));

    READ_STRING(p, "ManufacID", val, sdfile->manufacturer, error)
    READ_STRING(p, "CreateDate", val, sdfile->creation, error)
    READ_STRING(p, "ModDate", val, sdfile->modification, error)
    READ_INT(p, "NumPoints", val, sdfile->xres, TRUE, error)
    READ_INT(p, "NumProfiles", val, sdfile->yres, TRUE, error)
    READ_FLOAT(p, "Xscale", val, sdfile->xscale, TRUE, error)
    READ_FLOAT(p, "Yscale", val, sdfile->yscale, TRUE, error)
    READ_FLOAT(p, "Zscale", val, sdfile->zscale, TRUE, error)
    READ_FLOAT(p, "Zresolution", val, sdfile->zres, FALSE, error)
    READ_INT(p, "Compression", val, sdfile->compression, FALSE, error)
    READ_INT(p, "DataType", val, sdfile->data_type, FALSE, error)
    READ_INT(p, "CheckType", val, sdfile->check_type, FALSE, error)

    /* at least */
    if (sdfile->data_type < SDF_NTYPES)
        sdfile->expected_size = 2*sdfile->xres * sdfile->yres;
    else
        sdfile->expected_size = -1;

    /* Skip possible extra header lines */
    do {
        val = gwy_str_next_line(&p);
        if (!val)
            break;
        val = g_strstrip(val);
        if (g_ascii_isalpha(val[0])) {
            gwy_debug("Extra header line: <%s>\n", val);
        }
    } while (val[0] == ';' || g_ascii_isalpha(val[0]));

    if (!val || *val != '*') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing data start marker (*)."));
        return FALSE;
    }

    *buffer = p;
    *len -= p - *buffer;
    sdfile->data = (gchar*)*buffer;
    return TRUE;
}

static gchar*
sdfile_next_line(gchar **buffer,
                 const gchar *key,
                 GError **error)
{
    guint klen;
    gchar *value, *line;

    do {
        line = gwy_str_next_line(buffer);
    } while (line && line[0] == ';');

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
    if (value[0] == '=') {
        value++;
        g_strstrip(value);
    }

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
    /* We assume Intel byteorder, although the format does not specify
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
            data[i] = gwy_get_gfloat_le(&p);
        break;

        case SDF_DOUBLE:
        p = sdfile->data;
        for (i = 0; i < n; i++)
            data[i] = gwy_get_gdouble_le(&p);
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
    gchar *p, *end, *line;

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
        p = sdfile->data;
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
        p = sdfile->data;
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

    /* Find out if there is anything beyond the end-of-data-marker */
    while (*end && *end != '*')
        end++;
    if (!*end) {
        gwy_debug("Missing end-of-data marker `*' was ignored");
        return dfield;
    }

    do {
        end++;
    } while (g_ascii_isspace(*end));
    if (!*end)
        return dfield;

    /* Read the extra stuff */
    end--;
    sdfile->extras = g_hash_table_new(g_str_hash, g_str_equal);
    while ((line = gwy_str_next_line(&end))) {
        g_strstrip(line);
        if (!*line || *line == ';')
            continue;
        for (p = line; g_ascii_isalnum(*p); p++)
            ;
        if (!*p || (*p != '=' && !g_ascii_isspace(*p)))
            continue;
        *p = '\0';
        p++;
        while (*p == '=' || g_ascii_isspace(*p))
            p++;
        if (!*p)
            continue;
        g_strstrip(p);
        gwy_debug("extra: <%s> = <%s>", line, p);
        g_hash_table_insert(sdfile->extras, line, p);
    }

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

