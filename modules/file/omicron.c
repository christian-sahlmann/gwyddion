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
#define DEBUG 1
#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>
#include <libprocess/stats.h>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "err.h"
#include "get.h"

#define MAGIC "Omicron SPM Control"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define EXTENSION_HEADER ".par"

typedef enum {
    SCAN_UNKNOWN = 0,
    SCAN_FORWARD = 1,
    SCAN_BACKWARD = -1
} ScanDirection;

typedef struct {
    gchar type;    /* Z or I */
    ScanDirection scandir;
    gint min_raw;
    gint max_raw;
    gdouble min_phys;
    gdouble max_phys;
    gdouble resolution;
    const gchar *units;
    const gchar *filename;
    const gchar *name;
} OmicronTopoChannel;

typedef struct {
    GHashTable *meta;
    GPtrArray *topo_channels;
    GPtrArray *spec_channels;
} OmicronFile;

static gboolean      module_register        (void);
static gint          omicron_detect         (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* omicron_load           (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static gboolean      omicron_read_header    (gchar *buffer,
                                             OmicronFile *ofile,
                                             GError **error);
static gboolean      omicron_read_topo_header(gchar **buffer,
                                              OmicronTopoChannel *channel,
                                              GError **error);
/*
static gint          omicron_sscanf         (const gchar *str,
                                             const gchar *format,
                                             ...);
static GwyDataField* omicron_read_data_field(const guchar *buffer,
                                             gsize size,
                                             OmicronFile *ofile,
                                             GError **error);
static void          omicron_store_metadata (OmicronFile *ofile,
                                             GwyContainer *container);
static gchar*        omicron_find_data_name (const gchar *header_name);
*/
static void          omicron_file_free      (OmicronFile *ofile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Omicron data files (two-part .par + .tf*, .tb*, .sf*, .sb*)."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("omicron",
                           N_("Omicron files (.par + data)"),
                           (GwyFileDetectFunc)&omicron_detect,
                           (GwyFileLoadFunc)&omicron_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
omicron_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    guint i;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION_HEADER)
               ? 15 : 0;

    /* Quick check to skip most non-matching files */
    if (fileinfo->buffer_len < 100
        || fileinfo->buffer[0] != ';')
        return 0;

    for (i = 1; i + MAGIC_SIZE+1 < fileinfo->buffer_len; i++) {
        if (fileinfo->buffer[i] != ';' && !g_ascii_isspace(fileinfo->buffer[i]))
            break;
    }
    if (memcmp(fileinfo->buffer + i, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
omicron_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    OmicronFile ofile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gchar *text = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;

    if (!g_file_get_contents(filename, &text, NULL, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    memset(&ofile, 0, sizeof(OmicronFile));
    if (!omicron_read_header(text, &ofile, error)) {
        omicron_file_free(&ofile);
        g_free(text);
        return NULL;
    }

    /*
    if (ofile.data_type < OMICRON_UINT8
        || ofile.data_type > OMICRON_FLOAT
        || type_sizes[ofile.data_type] == 0) {
        err_UNSUPPORTED(error, _("data type"));
        omicron_file_free(&ofile);
        return NULL;
    }

    if (!(data_name = omicron_find_data_name(filename))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("No corresponding data file was found for header file."));
        omicron_file_free(&ofile);
        return NULL;
    }

    if (!gwy_file_get_contents(data_name, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        omicron_file_free(&ofile);
        return NULL;
    }

    dfield = omicron_read_data_field(buffer, size, &ofile, error);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (!dfield) {
        omicron_file_free(&ofile);
        return NULL;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    omicron_store_metadata(&ofile, container);
    */
    omicron_file_free(&ofile);
    g_free(text);

    err_NO_DATA(error);
    return container;
}

static gboolean
omicron_read_header(gchar *buffer,
                    OmicronFile *ofile,
                    GError **error)
{
    gchar *line, *val, *comment;

    ofile->meta = g_hash_table_new(g_str_hash, g_str_equal);

    while ((line = gwy_str_next_line(&buffer))) {
        while (g_ascii_isspace(*line))
            line++;
        if (!line[0] || line[0] == ';')
            continue;

        val = strchr(line, ':');
        if (!val) {
            /* TODO: no colon */
            continue;
        }
        if (val == line) {
            /* TODO: line starts with colon */
            continue;
        }
        *val = '\0';
        val++;
        g_strstrip(line);
        /* FIXME: what about `;[units]' comments? */
        comment = strchr(val, ';');
        if (comment) {
            *comment = '\0';
            comment++;
            g_strstrip(comment);
        }
        g_strstrip(val);

        if (gwy_strequal(line, "Topographic Channel")) {
            OmicronTopoChannel *channel;

            gwy_debug("Topographic Channel found (type %c)", val[0]);
            channel = g_new0(OmicronTopoChannel, 1);
            channel->type = val[0];
            if (!omicron_read_topo_header(&buffer, channel, error)) {
                g_free(channel);
                return FALSE;
            }
            if (!ofile->topo_channels)
                ofile->topo_channels = g_ptr_array_new();
            g_ptr_array_add(ofile->topo_channels, channel);
        }
        else if (gwy_strequal(line, "Spectroscopic Channel")) {
            gwy_debug("Spectroscopic Channel found");
        }
        else {
            gwy_debug("<%s> = <%s>", line, val);
            g_hash_table_insert(ofile->meta, line, val);
        }
    }

    return TRUE;
}

#define NEXT_LINE(buffer, line, optional, err) \
    if (!(line = gwy_str_next_line(buffer))) { \
        g_set_error(error, GWY_MODULE_FILE_ERROR, \
                    GWY_MODULE_FILE_ERROR_DATA, \
                    _("File header ended unexpectedly.")); \
        return FALSE; \
    } \
    g_strstrip(line); \
    if (!*line) { \
        if (optional) \
            return TRUE; \
        g_set_error(error, GWY_MODULE_FILE_ERROR, \
                    GWY_MODULE_FILE_ERROR_DATA, \
                    _("Channel information ended unexpectedly.")); \
        return FALSE; \
    } \
    if ((p = strchr(line, ';'))) \
        *p = '\0'; \
    g_strstrip(line)

static gboolean
omicron_read_topo_header(gchar **buffer,
                         OmicronTopoChannel *channel,
                         GError **error)
{
    gchar *line, *p;

    /* Direction */
    NEXT_LINE(buffer, line, FALSE, error);
    gwy_debug("Scan direction: %s", line);
    if (gwy_strequal(line, "Forward"))
        channel->scandir = SCAN_FORWARD;
    else if (gwy_strequal(line, "Backward"))
        channel->scandir = SCAN_BACKWARD;
    else
        channel->scandir = SCAN_UNKNOWN;

    /* Raw range */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->min_raw = atoi(line);
    NEXT_LINE(buffer, line, FALSE, error);
    channel->max_raw = atoi(line);
    gwy_debug("Raw range: [%d, %d]", channel->min_raw, channel->max_raw);

    /* Physical range */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->min_phys = g_ascii_strtod(line, NULL);
    NEXT_LINE(buffer, line, FALSE, error);
    channel->max_phys = g_ascii_strtod(line, NULL);
    gwy_debug("Physical range: [%g, %g]", channel->min_phys, channel->max_phys);

    /* Resolution */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->resolution = g_ascii_strtod(line, NULL);
    gwy_debug("Physical Resolution: %g", channel->resolution);

    /* Units */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->units = line;
    gwy_debug("Units: <%s>", channel->units);

    /* Filename */
    NEXT_LINE(buffer, line, FALSE, error);
    channel->filename = line;
    gwy_debug("Filename: <%s>", channel->filename);

    /* Name */
    NEXT_LINE(buffer, line, TRUE, error);
    channel->name = line;
    gwy_debug("Channel name: <%s>", channel->name);

    return TRUE;
}

#if 0
#define NEXT(buffer, line, err) \
    do { \
        if (!(line = gwy_str_next_line(&buffer))) { \
            g_set_error(error, GWY_MODULE_FILE_ERROR, \
                        GWY_MODULE_FILE_ERROR_DATA, \
                        _("File header ended unexpectedly.")); \
            return FALSE; \
        } \
    } while (g_str_has_prefix(line, "\t:")); \
    g_strstrip(line)

static gboolean
omicron_read_header(gchar *buffer,
                    OmicronFile *ofile,
                    GError **error)
{
    gchar *line;
    gint type1, type2;

    line = gwy_str_next_line(&buffer);
    if (!line)
        return FALSE;

    NEXT(buffer, line, error);
    /* garbage */

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "i", &ofile->format_version) != 1) {
        err_UNSUPPORTED(error, _("format version"));
        return FALSE;
    }

    NEXT(buffer, line, error);
    ofile->date = g_strdup(line);
    NEXT(buffer, line, error);
    ofile->time = g_strdup(line);
    NEXT(buffer, line, error);
    ofile->sample_name = g_strdup(line);
    NEXT(buffer, line, error);
    ofile->remark = g_strdup(line);

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "ii", &ofile->ascii_flag, &type1) != 2) {
        err_INVALID(error, _("format flags"));
        return FALSE;
    }
    ofile->data_type = type1;

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "ii", &ofile->xres, &ofile->yres) != 2) {
        err_INVALID(error, _("resolution"));
        return FALSE;
    }

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "ii", &type1, &type2) != 2) {
        /* FIXME */
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing or invalid some integers heaven knows what "
                      "they mean but that should be here."));
        return FALSE;
    }
    ofile->dim_x = type1;
    ofile->dim_y = type2;

    NEXT(buffer, line, error);
    ofile->unit_x = g_strdup(line);

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "ddi",
                       &ofile->start_x, &ofile->end_x,
                       &ofile->log_flag_x) != 3) {
        err_INVALID(error, _("x scale parameters"));
        return FALSE;
    }

    NEXT(buffer, line, error);
    ofile->unit_y = g_strdup(line);

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "ddii",
                       &ofile->start_y, &ofile->end_y,
                       &ofile->ineq_flag, &ofile->log_flag_y) != 4) {
        err_INVALID(error, _("y scale parameters"));
        return FALSE;
    }

    NEXT(buffer, line, error);
    ofile->unit_z = g_strdup(line);

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "ddddi",
                       &ofile->max_raw_z, &ofile->min_raw_z,
                       &ofile->max_z, &ofile->min_z,
                       &ofile->log_flag_z) != 5) {
        err_INVALID(error, _("z scale parameters"));
        return FALSE;
    }

    NEXT(buffer, line, error);
    if (omicron_sscanf(line, "dddi",
                       &ofile->stm_voltage, &ofile->stm_current,
                       &ofile->scan_time, &ofile->accum) != 4) {
        err_INVALID(error, _("data type parameters"));
        return FALSE;
    }

    NEXT(buffer, line, error);
    /* reserved */

    NEXT(buffer, line, error);
    ofile->stm_voltage_unit = g_strdup(line);

    NEXT(buffer, line, error);
    ofile->stm_current_unit = g_strdup(line);

    NEXT(buffer, line, error);
    ofile->ad_name = g_strdup(line);

    /* There is more stuff after that, but heaven knows what it means... */

    return TRUE;
}

static gint
omicron_sscanf(const gchar *str,
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
omicron_read_data_field(const guchar *buffer,
                        gsize size,
                        OmicronFile *ofile,
                        GError **error)
{
    gint i, n, power10;
    const gchar *unit;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble q, pmin, pmax, rmin, rmax;
    gdouble *data;

    n = ofile->xres * ofile->yres;
    if (n*type_sizes[ofile->data_type] > size) {
        err_SIZE_MISMATCH(error, n*type_sizes[ofile->data_type], size);
        return NULL;
    }

    dfield = gwy_data_field_new(ofile->xres, ofile->yres,
                                fabs((ofile->end_x - ofile->start_x)),
                                fabs((ofile->end_y - ofile->start_y)),
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    /* FIXME: what to do when ascii_flag is set? */
    switch (ofile->data_type) {
        case OMICRON_UINT8:
        for (i = 0; i < n; i++)
            data[i] = buffer[i];
        break;

        case OMICRON_SINT8:
        for (i = 0; i < n; i++)
            data[i] = (signed char)buffer[i];
        break;

        case OMICRON_UINT16:
        {
            const guint16 *pdata = (const guint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = GUINT16_FROM_LE(pdata[i]);
        }
        break;

        case OMICRON_SINT16:
        {
            const gint16 *pdata = (const gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = GINT16_FROM_LE(pdata[i]);
        }
        break;

        case OMICRON_FLOAT:
        for (i = 0; i < n; i++)
            data[i] = get_FLOAT(&buffer);
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }

    unit = ofile->unit_x;
    if (!*unit)
        unit = "nm";
    siunit = gwy_si_unit_new_parse(unit, &power10);
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    q = pow10((gdouble)power10);
    gwy_data_field_set_xreal(dfield, q*gwy_data_field_get_xreal(dfield));
    gwy_data_field_set_yreal(dfield, q*gwy_data_field_get_yreal(dfield));
    g_object_unref(siunit);

    unit = ofile->unit_z;
    /* XXX: No fallback yet, just make z unitless */
    siunit = gwy_si_unit_new_parse(unit, &power10);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    q = pow10((gdouble)power10);
    pmin = q*ofile->min_z;
    pmax = q*ofile->max_z;
    rmin = ofile->min_raw_z;
    rmax = ofile->max_raw_z;
    gwy_data_field_multiply(dfield, (pmax - pmin)/(rmax - rmin));
    gwy_data_field_add(dfield, (pmin*rmax - pmax*rmin)/(rmax - rmin));
    g_object_unref(siunit);

    return dfield;
}

static void
omicron_store_metadata(OmicronFile *ofile,
                       GwyContainer *container)
{
    gwy_container_set_string_by_name(container, "/meta/Date",
                                     g_strconcat(ofile->date, " ",
                                                 ofile->time, NULL));
    if (*ofile->remark)
        gwy_container_set_string_by_name(container, "/meta/Remark",
                                         g_strdup(ofile->remark));
    if (*ofile->sample_name)
        gwy_container_set_string_by_name(container, "/meta/Sample name",
                                         g_strdup(ofile->sample_name));
    if (*ofile->ad_name)
        gwy_container_set_string_by_name(container, "/meta/AD name",
                                         g_strdup(ofile->ad_name));
}

static gchar*
omicron_find_data_name(const gchar *header_name)
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
#endif

static void
omicron_file_free(OmicronFile *ofile)
{
    if (ofile->meta) {
        g_hash_table_destroy(ofile->meta);
        ofile->meta = NULL;
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

