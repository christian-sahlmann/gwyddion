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

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-igor-binary-wave">
 *   <comment>Igor binary wave</comment>
 *   <glob pattern="*.ibw"/>
 *   <glob pattern="*.IBW"/>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

#define MAGIC1 "\x01\x00"
#define MAGIC2 "\x02\x00"
#define MAGIC3 "\x03\x00"
#define MAGIC5 "\x05\x00"
#define MAGIC_SIZE 2

#define EXTENSION ".ibw"

enum {
    MIN_FILE_SIZE = 8 + 110 + 16
};

/* The header fields we read, they are stored differently in different
 * versions */
typedef struct {
    gint version;    /* Version number */
    gint checksum;   /* Checksum of this header and the wave header */
    guint wfm_size;     /* Size of the WaveHeader5 data structure plus the wave
                           data. */
    guint formula_size; /* The size of the dependency formula, if any. */
    guint note_size;    /* The size of the note text. */
    guint data_e_units_size; /* The size of optional extended data units. */
    guint dim_e_units_size[MAXDIMS]; /* The size of optional extended dimension
                                        units. */
    guint dim_labels_size[MAXDIMS]; /* The size of optional dimension labels. */
    guint sIndicesSize;   /* The size of string indicies if this is a text
                              wave. */
} BinHeader;

static gboolean      module_register        (void);
static gint          igor_detect         (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer* igor_load           (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static gboolean      igor_read_header    (gchar *buffer,
                                             IgorFile *ufile,
                                             GError **error);
static GwyDataField* igor_read_data_field(const guchar *buffer,
                                             gsize size,
                                             IgorFile *ufile,
                                             GError **error);
static GwyContainer* igor_get_metadata   (IgorFile *ufile);
static void          igor_file_free      (IgorFile *ufile);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Igor binary waves (.ibw)."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("igorfile",
                           N_("Igor binary waves (.ibw)"),
                           (GwyFileDetectFunc)&igor_detect,
                           (GwyFileLoadFunc)&igor_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
igor_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && fileinfo->file_size >= MIN_FILE_SIZE) {

        if (memcmp(fileinfo->head, MAGIC1)) {
        }
    }

    return score;
}

static GwyContainer*
igor_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    IgorFile ufile;
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    gchar *text = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    gchar *data_name;

    if (!g_file_get_contents(filename, &text, NULL, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    memset(&ufile, 0, sizeof(IgorFile));
    if (!igor_read_header(text, &ufile, error)) {
        igor_file_free(&ufile);
        g_free(text);
        return NULL;
    }
    g_free(text);

    if (ufile.data_type < IGOR_UINT8
        || ufile.data_type > IGOR_FLOAT
        || type_sizes[ufile.data_type] == 0) {
        err_DATA_TYPE(error, ufile.data_type);
        igor_file_free(&ufile);
        return NULL;
    }

    if (!gwy_file_get_contents(data_name, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        igor_file_free(&ufile);
        return NULL;
    }

    dfield = igor_read_data_field(buffer, size, &ufile, error);
    gwy_file_abandon_contents(buffer, size, NULL);

    if (!dfield) {
        igor_file_free(&ufile);
        return NULL;
    }

    container = gwy_container_new();
    gwy_container_set_object_by_name(container, "/0/data", dfield);
    g_object_unref(dfield);
    gwy_app_channel_title_fall_back(container, 0);

    meta = igor_get_metadata(&ufile);
    gwy_container_set_object_by_name(container, "/0/meta", meta);
    g_object_unref(meta);

    igor_file_free(&ufile);

    return container;
}

static GwyDataField*
igor_read_data_field(const guchar *buffer,
                        gsize size,
                        IgorFile *ufile,
                        GError **error)
{
    gint i, n, power10;
    const gchar *unit;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble q, pmin, pmax, rmin, rmax;
    gdouble *data;

    n = ufile->xres * ufile->yres;
    if (err_SIZE_MISMATCH(error, n*type_sizes[ufile->data_type], size, FALSE))
        return NULL;

    dfield = gwy_data_field_new(ufile->xres, ufile->yres,
                                fabs((ufile->end_x - ufile->start_x)),
                                fabs((ufile->end_y - ufile->start_y)),
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    /* FIXME: what to do when ascii_flag is set? */
    switch (ufile->data_type) {
        case IGOR_UINT8:
        for (i = 0; i < n; i++)
            data[i] = buffer[i];
        break;

        case IGOR_SINT8:
        for (i = 0; i < n; i++)
            data[i] = (signed char)buffer[i];
        break;

        case IGOR_UINT16:
        {
            const guint16 *pdata = (const guint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = GUINT16_FROM_LE(pdata[i]);
        }
        break;

        case IGOR_SINT16:
        {
            const gint16 *pdata = (const gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = GINT16_FROM_LE(pdata[i]);
        }
        break;

        case IGOR_FLOAT:
        for (i = 0; i < n; i++)
            data[i] = gwy_get_gfloat_le(&buffer);
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

static GwyContainer*
igor_get_metadata(IgorFile *ufile)
{
    GwyContainer *meta;

    meta = gwy_container_new();

    gwy_container_set_string_by_name(meta, "Date",
                                     g_strconcat(ufile->date, " ",
                                                 ufile->time, NULL));
    if (*ufile->remark)
        gwy_container_set_string_by_name(meta, "Remark",
                                         g_strdup(ufile->remark));
    if (*ufile->sample_name)
        gwy_container_set_string_by_name(meta, "Sample name",
                                         g_strdup(ufile->sample_name));
    if (*ufile->ad_name)
        gwy_container_set_string_by_name(meta, "AD name",
                                         g_strdup(ufile->ad_name));

    return meta;
}

static gchar*
igor_find_data_name(const gchar *header_name)
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
igor_file_free(IgorFile *ufile)
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
