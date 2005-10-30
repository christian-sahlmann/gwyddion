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
#include <libprocess/stats.h>
#include <app/gwyapp.h>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "get.h"

#define MAGIC ":STM data\r\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION_HEADER ".hdr"
#define EXTENSION_DATA ".dat"

typedef enum {
    UNISOKU_UINT8  = 2,
    UNISOKU_SINT8  = 3,
    UNISOKU_UINT16 = 4,
    UNISOKU_SINT16 = 5,
    UNISOKU_FLOAT  = 8
} UnisokuDataType;

typedef enum {
    UNISOKU_DIM_LENGTH = 1,
    UNISOKU_DIM_TIME = 2,
    UNISOKU_DIM_CURRENT = 3,
    UNISOKU_DIM_VOLTAGE = 4,
    UNISOKU_DIM_TEMPERATURE = 5,
    UNISOKU_DIM_INVERSE_LENGTH = 6,
    UNISOKU_DIM_INVERSE_TIME = 7,
    UNISOKU_DIM_OTHER = 8
} UnisokuDimType;

typedef struct {
    gint format_version;
    gchar *date;
    gchar *time;
    gchar *sample_name;
    gchar *remark;
    gboolean ascii_flag;
    UnisokuDataType data_type;
    gint xres;
    gint yres;
    UnisokuDimType dim_x;
    UnisokuDimType dim_y;
    gchar *unit_x;
    gdouble start_x;
    gdouble end_x;
    gboolean log_flag_x;
    gchar *unit_y;
    gdouble start_y;
    gdouble end_y;
    gboolean log_flag_y;
    gboolean ineq_flag;
    gchar *unit_z;
    gdouble min_raw_z;
    gdouble max_raw_z;
    gdouble min_z;
    gdouble max_z;
    gboolean log_flag_z;
    gdouble stm_voltage;
    gdouble stm_current;
    gdouble scan_time;
    gint accum;
    gchar *stm_voltage_unit;
    gchar *stm_current_unit;
    gchar *ad_name;
} UnisokuFile;

static gboolean      module_register        (const gchar *name);
static gint          unisoku_detect         (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* unisoku_load           (const gchar *filename);
static gboolean      unisoku_read_header    (gchar *buffer,
                                             UnisokuFile *ufile);
static gint          unisoku_sscanf         (const gchar *str,
                                             const gchar *format,
                                             ...);
static GwyDataField* unisoku_read_data_field(const guchar *buffer,
                                             gsize size,
                                             UnisokuFile *ufile);
static void          unisoku_store_metadata (UnisokuFile *ufile,
                                             GwyContainer *container);
static gchar*        unisoku_find_data_name (const gchar *header_name);
static void          unisoku_file_free      (UnisokuFile *ufile);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Unisoku data files (two-part .hdr + .dat)."),
    "Yeti <yeti@gwyddion.net>",
    "0.3",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

static const guint type_sizes[] = { 0, 0, 1, 1, 2, 2, 0, 0, 4 };

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo unisoku_func_info = {
        "unisoku",
        N_("Unisoku files (.hdr + .dat)"),
        (GwyFileDetectFunc)&unisoku_detect,
        (GwyFileLoadFunc)&unisoku_load,
        NULL
    };

    gwy_file_func_register(name, &unisoku_func_info);

    return TRUE;
}

static gint
unisoku_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_HEADER)
               ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE) == 0
        && g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_HEADER)) {
        gchar *data_name;

        if ((data_name = unisoku_find_data_name(fileinfo->name))) {
            score = 100;
            g_free(data_name);
        }
    }

    return score;
}

static GwyContainer*
unisoku_load(const gchar *filename)
{
    UnisokuFile ufile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gchar *text = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gchar *data_name;

    if (!g_file_get_contents(filename, &text, NULL, &err)) {
        g_warning("Cannot read file `%s'", filename);
        g_clear_error(&err);
        return NULL;
    }

    memset(&ufile, 0, sizeof(UnisokuFile));
    if (!unisoku_read_header(text, &ufile)) {
        unisoku_file_free(&ufile);
        g_free(text);
        return NULL;
    }
    g_free(text);

    if (ufile.data_type < UNISOKU_UINT8
        || ufile.data_type > UNISOKU_FLOAT
        || type_sizes[ufile.data_type] == 0) {
        g_warning("Uknown data type: %d", ufile.data_type);
        unisoku_file_free(&ufile);
        return NULL;
    }

    if (!(data_name = unisoku_find_data_name(filename))) {
        g_warning("No data file found for header file `%s'", filename);
        unisoku_file_free(&ufile);
        return NULL;
    }

    if (!gwy_file_get_contents(data_name, &buffer, &size, &err)) {
        g_warning("Cannot read file `%s'", filename);
        unisoku_file_free(&ufile);
        g_clear_error(&err);
        return NULL;
    }

    dfield = unisoku_read_data_field(buffer, size, &ufile);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (!dfield) {
        g_warning("Failed to read data from `%s'", filename);
        unisoku_file_free(&ufile);
        return NULL;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    unisoku_store_metadata(&ufile, container);
    unisoku_file_free(&ufile);

    return container;
}

#define NEXT(buffer, line) \
    do { \
        if (!(line = gwy_str_next_line(&buffer))) { \
            g_warning("Unexpected end of header"); \
            return FALSE; \
        } \
    } while (g_str_has_prefix(line, "\t:")); \
    g_strstrip(line)

static gboolean
unisoku_read_header(gchar *buffer,
                    UnisokuFile *ufile)
{
    gchar *line;
    gint type1, type2;

    line = gwy_str_next_line(&buffer);
    if (!line)
        return FALSE;

    NEXT(buffer, line);
    /* garbage */

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "i", &ufile->format_version) != 1)
        return FALSE;

    NEXT(buffer, line);
    ufile->date = g_strdup(line);
    NEXT(buffer, line);
    ufile->time = g_strdup(line);
    NEXT(buffer, line);
    ufile->sample_name = g_strdup(line);
    NEXT(buffer, line);
    ufile->remark = g_strdup(line);

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "ii", &ufile->ascii_flag, &type1) != 2)
        return FALSE;
    ufile->data_type = type1;

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "ii", &ufile->xres, &ufile->yres) != 2)
        return FALSE;

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "ii", &type1, &type2) != 2)
        return FALSE;
    ufile->dim_x = type1;
    ufile->dim_y = type2;

    NEXT(buffer, line);
    ufile->unit_x = g_strdup(line);

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "ddi",
                       &ufile->start_x, &ufile->end_x,
                       &ufile->log_flag_x) != 3)
        return FALSE;

    NEXT(buffer, line);
    ufile->unit_y = g_strdup(line);

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "ddii",
                       &ufile->start_y, &ufile->end_y,
                       &ufile->ineq_flag, &ufile->log_flag_y) != 4)
        return FALSE;

    NEXT(buffer, line);
    ufile->unit_z = g_strdup(line);

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "ddddi",
                       &ufile->max_raw_z, &ufile->min_raw_z,
                       &ufile->max_z, &ufile->min_z,
                       &ufile->log_flag_z) != 5)
        return FALSE;

    NEXT(buffer, line);
    if (unisoku_sscanf(line, "dddi",
                       &ufile->stm_voltage, &ufile->stm_current,
                       &ufile->scan_time, &ufile->accum) != 4)
        return FALSE;

    NEXT(buffer, line);
    /* reserved */

    NEXT(buffer, line);
    ufile->stm_voltage_unit = g_strdup(line);

    NEXT(buffer, line);
    ufile->stm_current_unit = g_strdup(line);

    NEXT(buffer, line);
    ufile->ad_name = g_strdup(line);

    /* There is more stuff after that, but heaven knows what it means... */

    return TRUE;
}

static gint
unisoku_sscanf(const gchar *str,
               const gchar *format,
               ...)
{
    va_list ap;
    gchar *endptr;
    gint *pi;
    gdouble *pd;
    gint count = 0;

    va_start(ap, format);
    while (*format) {
        switch (*format++) {
            case 'i':
            pi = va_arg(ap, gint*);
            g_assert(pi);
            *pi = strtol(str, &endptr, 10);
            break;

            case 'd':
            pd = va_arg(ap, gdouble*);
            g_assert(pd);
            *pd = g_ascii_strtod(str, &endptr);
            break;

            default:
            g_return_val_if_reached(0);
            break;
        }
        if ((gchar*)str == endptr)
            break;

        count++;
        str = endptr;
    }
    va_end(ap);

    return count;
}

static GwyDataField*
unisoku_read_data_field(const guchar *buffer,
                        gsize size,
                        UnisokuFile *ufile)
{
    gint i, n, power10;
    const gchar *unit;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble q, pmin, pmax, rmin, rmax;
    gdouble *data;

    n = ufile->xres * ufile->yres;
    if (n*type_sizes[ufile->data_type] > size) {
        g_warning("Data file is truncated");
        return NULL;
    }

    dfield = gwy_data_field_new(ufile->xres, ufile->yres,
                                fabs((ufile->end_x - ufile->start_x)),
                                fabs((ufile->end_y - ufile->start_y)),
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    /* FIXME: what to do when ascii_flag is set? */
    switch (ufile->data_type) {
        case UNISOKU_UINT8:
        for (i = 0; i < n; i++)
            data[i] = buffer[i];
        break;

        case UNISOKU_SINT8:
        for (i = 0; i < n; i++)
            data[i] = (signed char)buffer[i];
        break;

        case UNISOKU_UINT16:
        {
            const guint16 *pdata = (const guint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = GUINT16_FROM_LE(pdata[i]);
        }
        break;

        case UNISOKU_SINT16:
        {
            const gint16 *pdata = (const gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = GINT16_FROM_LE(pdata[i]);
        }
        break;

        case UNISOKU_FLOAT:
        for (i = 0; i < n; i++)
            data[i] = get_FLOAT(&buffer);
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }

    unit = ufile->unit_x;
    if (!*unit)
        unit = "nm";
    siunit = gwy_si_unit_new_parse(unit, &power10);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    q = pow10((gdouble)power10);
    gwy_data_field_set_xreal(dfield, q*gwy_data_field_get_xreal(dfield));
    gwy_data_field_set_yreal(dfield, q*gwy_data_field_get_yreal(dfield));
    g_object_unref(siunit);

    unit = ufile->unit_z;
    /* XXX: No fallback yet, just make z unitless */
    siunit = gwy_si_unit_new_parse(unit, &power10);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    q = pow10((gdouble)power10);
    pmin = q*ufile->min_z;
    pmax = q*ufile->max_z;
    rmin = ufile->min_raw_z;
    rmax = ufile->max_raw_z;
    gwy_data_field_multiply(dfield, (pmax - pmin)/(rmax - rmin));
    gwy_data_field_add(dfield, (pmin*rmax - pmax*rmin)/(rmax - rmin));
    g_object_unref(siunit);

    return dfield;
}

static void
unisoku_store_metadata(UnisokuFile *ufile,
                       GwyContainer *container)
{
    gwy_container_set_string_by_name(container, "/meta/Date",
                                     g_strconcat(ufile->date, " ",
                                                 ufile->time, NULL));
    if (*ufile->remark)
        gwy_container_set_string_by_name(container, "/meta/Remark",
                                         g_strdup(ufile->remark));
    if (*ufile->sample_name)
        gwy_container_set_string_by_name(container, "/meta/Sample name",
                                         g_strdup(ufile->sample_name));
    if (*ufile->ad_name)
        gwy_container_set_string_by_name(container, "/meta/AD name",
                                         g_strdup(ufile->ad_name));
}

static gchar*
unisoku_find_data_name(const gchar *header_name)
{
    GString *data_name;
    gchar *retval;
    gboolean ok = FALSE;

    data_name = g_string_new(header_name);
    g_string_truncate(data_name,
                      data_name->len - (sizeof(EXTENSION_HEADER) - 1));
    g_string_append(data_name, EXTENSION_DATA);
    if (g_file_test(data_name->str, G_FILE_TEST_IS_REGULAR))
        ok = TRUE;
    else {
        g_ascii_strup(data_name->str
                      + data_name->len - (sizeof(EXTENSION_DATA) - 1),
                      -1);
        if (g_file_test(data_name->str, G_FILE_TEST_IS_REGULAR))
            ok = TRUE;
    }
    retval = data_name->str;
    g_string_free(data_name, !ok);

    return ok ? retval : NULL;
}

static void
unisoku_file_free(UnisokuFile *ufile)
{
    g_free(ufile->date);
    g_free(ufile->time);
    g_free(ufile->sample_name);
    g_free(ufile->remark);
    g_free(ufile->unit_x);
    g_free(ufile->unit_y);
    g_free(ufile->unit_z);
    g_free(ufile->stm_voltage_unit);
    g_free(ufile->stm_current_unit);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

