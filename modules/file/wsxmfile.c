/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klapetek.
 *  E-mail: yeti@gwyddion.net, klwsxmtek@gwyddion.net.
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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-wsxm-spm">
 *   <comment>WSxM SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="WSxM file copyright Nanotec Electronica\r\nSxM Image file\r\n"/>
 *   </magic>
 *   <glob pattern="*.tom"/>
 *   <glob pattern="*.TOM"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC1 "WSxM file copyright Nanotec Electronica"
#define MAGIC2 "SxM Image file"
#define MAGIC1_SIZE (sizeof(MAGIC1) - 1)
#define MAGIC2_SIZE (sizeof(MAGIC2) - 1)
#define MAGIC_SIZE (MAGIC1_SIZE + MAGIC2_SIZE)

#define SIZE_HEADER "Image header size:"

typedef enum {
    WSXM_DATA_INT16,
    WSXM_DATA_DOUBLE
} WSxMDataType;

static gboolean      module_register   (void);
static gint          wsxmfile_detect   (const GwyFileDetectInfo *fileinfo,
                                        gboolean only_name);
static GwyContainer* wsxmfile_load     (const gchar *filename,
                                        GwyRunType mode,
                                        GError **error);
static GwyDataField* read_data_field   (const guchar *buffer,
                                        gint xres,
                                        gint yres,
                                        WSxMDataType type);
static void          process_metadata  (GHashTable *wsxmmeta,
                                        GwyContainer *container);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanotec WSxM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.10",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("wsxmfile",
                           N_("WSXM files (.tom)"),
                           (GwyFileDetectFunc)&wsxmfile_detect,
                           (GwyFileLoadFunc)&wsxmfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

/* Return pointer to character after newline */
static const char
*read_newline(const char *str)
{
    if (str[0] == '\n')
        return &str[1];
    else if (str[0] == '\r' && str[1] == '\n')
        return &str[2];

    return NULL;
}

/* Return pointer to character after magic */
static const char
*wsxmfile_check_magic(const char *head) {
    const char *rest;

    if (!memcmp(head, MAGIC1, MAGIC1_SIZE)
        && (rest = read_newline(&head[MAGIC1_SIZE]))
        && !memcmp(rest, MAGIC2, MAGIC2_SIZE)
        && (rest = read_newline(&rest[MAGIC2_SIZE])))
        return rest;

    return NULL;
}

static gint
wsxmfile_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, ".tom") ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && wsxmfile_check_magic(fileinfo->head))
        score = 100;

    return score;
}

static gpointer
convert_to_utf8(G_GNUC_UNUSED const gchar *key,
                const gchar *value,
                G_GNUC_UNUSED gpointer user_data)
{
    return g_convert(value, strlen(value), "UTF-8", "ISO-8859-1",
                     NULL, NULL, NULL);
}

static GwyContainer*
wsxmfile_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    const gchar *rest = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    GHashTable *meta = NULL;
    WSxMDataType type = WSXM_DATA_INT16;
    guint header_size;
    gchar *p, *header;
    gboolean ok = TRUE;
    gint xres = 0, yres = 0;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (!(rest = wsxmfile_check_magic(buffer))
        || !memcmp(rest, SIZE_HEADER, sizeof(SIZE_HEADER))
        || (header_size = strtol(rest + sizeof(SIZE_HEADER), &p, 10)) < 1) {
        err_FILE_TYPE(error, "WSXM");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }
    if (size < header_size) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    header = g_strndup(buffer, header_size);
    p = strchr(header, '[');
    if (!p) {
        err_FILE_TYPE(error, "WSxM");
        gwy_file_abandon_contents(buffer, size, NULL);
        g_free(header);
        return NULL;
    }
    meta = gwy_parse_text_header(p, NULL, "[\x1a]", NULL, "::", NULL, ":",
                                 convert_to_utf8, NULL, g_free);

    if (ok
        && (!(p = g_hash_table_lookup(meta, "General Info::Number of columns"))
         || (xres = atol(p)) <= 0)) {
        err_INVALID(error, _("number of columns"));
        ok = FALSE;
    }

    if (ok
        && (!(p = g_hash_table_lookup(meta, "General Info::Number of rows"))
         || (yres = atol(p)) <= 0)) {
        err_INVALID(error, _("number of rows"));
        ok = FALSE;
    }

    if (ok
        && (p = g_hash_table_lookup(meta, "General Info::Image Data Type"))) {
        if (gwy_strequal(p, "double"))
            type = WSXM_DATA_DOUBLE;
        else
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Unknown data type `%s'."), p);
    }

    if (ok)
        ok = !err_SIZE_MISMATCH(error, 2*xres*yres, (guint)size - header_size,
                                FALSE);
    if (ok)
        dfield = read_data_field(buffer + header_size, xres, yres, type);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        process_metadata(meta, container);
    }
    g_hash_table_destroy(meta);
    g_free(header);

    return container;
}

static void
store_meta(gpointer key,
           gpointer value,
           gpointer user_data)
{
    GwyContainer *meta = (GwyContainer*)user_data;

    gwy_container_set_string_by_name(meta, key, g_strdup(value));
}

static void
process_metadata(GHashTable *wsxmmeta,
                 GwyContainer *container)
{
    const gchar *nometa[] = {
        "General Info::Z Amplitude",
        "Control::X Amplitude", "Control::Y Amplitude",
        "General Info::Number of rows", "General Info::Number of columns",
    };
    GwyDataField *dfield;
    GwySIUnit *siunit;
    GwyContainer *meta;
    gdouble r;
    gchar *p, *end;
    gint power10;
    guint i;
    gdouble min, max;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             "/0/data"));

    /* Fix value scale */
    if (!(p = g_hash_table_lookup(wsxmmeta, "General Info::Z Amplitude"))
        || (r = g_ascii_strtod(p, &end)) <= 0
        || end == p) {
        g_warning("Missing or invalid Z Amplitude");
        gwy_data_field_multiply(dfield, 1e-9);
    }
    else {
        /* import `arbitrary units' as unit-less */
        while (g_ascii_isspace(*end))
            end++;
        if (gwy_strequal(end, "a.u."))
            siunit = gwy_si_unit_new("");
        else {
            siunit = gwy_si_unit_new_parse(end, &power10);
            r *= pow10(power10);
        }
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);

        gwy_data_field_get_min_max(dfield, &min, &max);
        gwy_data_field_multiply(dfield, r/(max - min));

        gwy_app_channel_title_fall_back(container, 0);
    }

    /* Fix lateral scale */
    if (!(p = g_hash_table_lookup(wsxmmeta, "Control::X Amplitude"))
        || (r = g_ascii_strtod(p, &end)) <= 0
        || end == p) {
        g_warning("Missing or invalid X Amplitude");
    }
    else {
        siunit = gwy_si_unit_new_parse(end, &power10);
        gwy_data_field_set_si_unit_xy(dfield, siunit);
        g_object_unref(siunit);

        gwy_data_field_set_xreal(dfield, r*pow10(power10));
    }

    if (!(p = g_hash_table_lookup(wsxmmeta, "Control::Y Amplitude"))
        || (r = g_ascii_strtod(p, &end)) <= 0
        || end == p) {
        g_warning("Missing or invalid Y Amplitude");
        gwy_data_field_set_yreal(dfield, gwy_data_field_get_xreal(dfield));
    }
    else {
        siunit = gwy_si_unit_new_parse(end, &power10);
        g_object_unref(siunit);
        gwy_data_field_set_yreal(dfield, r*pow10(power10));
    }

    /* And store everything else as metadata */
    for (i = 0; i < G_N_ELEMENTS(nometa); i++)
        g_hash_table_remove(wsxmmeta, nometa[i]);

    meta = gwy_container_new();
    g_hash_table_foreach(wsxmmeta, store_meta, meta);
    if (gwy_container_get_n_items(meta))
        gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);
}

static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres,
                WSxMDataType type)
{
    GwyDataField *dfield;
    gdouble *data;
    guint i;

    dfield = gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE);
    data = gwy_data_field_get_data(dfield);
    switch (type) {
        case WSXM_DATA_INT16: {
            const gint16 *p = (const gint16*)buffer;

            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_LE(p[i]);
        }
        break;

        case WSXM_DATA_DOUBLE: {
            for (i = 0; i < xres*yres; i++)
                data[i] = gwy_get_gdouble_le(&buffer);
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gwy_data_field_invert(dfield, TRUE, TRUE, FALSE);

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

