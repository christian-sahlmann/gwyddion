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
#define DEBUG 1
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

#define EXTENSION ".ibw"

enum {
    MAXDIMS = 4,
    MAX_UNIT_CHARS = 3,
    MAX_WAVE_NAME2 = 18,
    MAX_WAVE_NAME5 = 31,
    MIN_FILE_SIZE = 8 + 110 + 16,
    HEADER_SIZE1 = 8,
    HEADER_SIZE2 = 16,
    HEADER_SIZE3 = 20,
    HEADER_SIZE5 = 64,
    WAVE_SIZE2 = 110,
    WAVE_SIZE5 = 320,
};

typedef enum {
    IGOR_TEXT     = 0x00,
    IGOR_COMPLEX  = 0x01,
    IGOR_SINGLE   = 0x02,
    IGOR_DOUBLE   = 0x04,
    IGOR_INT8     = 0x08,
    IGOR_INT16    = 0x10,
    IGOR_INT32    = 0x20,
    IGOR_UNSIGNED = 0x40,
} IgorDataType;

/* The header fields we read, they are stored differently in different
 * versions */
typedef struct {
    gint version;    /* Version number */
    gint checksum;   /* Checksum of this header and the wave header */
    guint wfm_size;     /* Size of the WaveHeader5 data structure plus the wave
                           data. */
    guint formula_size; /* The size of the dependency formula, if any. */
    guint note_size;    /* The size of the note text. */
    guint pict_size;    /* Reserved (0). */
    guint data_e_units_size; /* The size of optional extended data units. */
    guint dim_e_units_size[MAXDIMS]; /* The size of optional extended dimension
                                        units. */
    guint dim_labels_size[MAXDIMS]; /* The size of optional dimension labels. */
    guint indices_size;   /* The size of string indicies if this is a text
                             wave. */
    guint options_size1;  /* Reserved (0). */
    guint options_size2;  /* Reserved (0). */
} IgorBinHeader;

typedef struct {
    guint next;           /* Pointer, ignore. */
    guint creation_date;  /* DateTime of creation. */
    guint mod_date;       /* DateTime of last modification. */
    guint npnts;          /* Total number of points
                             (multiply dimensions up to first zero). */
    IgorDataType type;
    guint lock;           /* Reserved (0). */
    gchar whpad1[6];      /* Reserved (0). */
    guint wh_version;     /* Reserved (1). */
    gchar *bname;         /* Wave name, nul-terminated. */
    guint whpad2;         /* Reserved (0). */
    guint dfolder;        /* Pointer, ignore. */

    /* Dimensioning info. [0] == rows, [1] == cols etc */
    guint n_dim[MAXDIMS];   /* Number of of items in a dimension,
                               0 means no data. */
    gdouble sfA[MAXDIMS];   /* Index value for element e of dimension
                               d = sfA[d]*e + sfB[d]. */
    gdouble sfB[MAXDIMS];

    /* SI units */
    gchar *data_units;           /* Natural data units, null if none. */
    gchar *dim_units[MAXDIMS];   /* Natural dimension units, null if none. */

    gboolean fs_valid;           /* TRUE if full scale values have meaning. */
    guint whpad3;                /* Reserved (0). */
    gdouble top_full_scale;      /* The max value for wave. */
    gdouble bot_full_scale;      /* The max full scale value for wave. */

    /* There is more stuff.  But it's either marked reserved, unused or private
     * to Igor.  Do not bother with that... */
} IgorWaveHeader5;

typedef struct {
    guint16 (*get_guint16)(const guchar **p);
    gint16 (*get_gint16)(const guchar **p);
    guint32 (*get_guint32)(const guchar **p);
    gint32 (*get_gint32)(const guchar **p);
    gfloat (*get_gfloat)(const guchar **p);
    gdouble (*get_gdouble)(const guchar **p);
    IgorBinHeader header;
    IgorWaveHeader5 wave5;
} IgorFile;

static gboolean      module_register     (void);
static gint          igor_detect         (const GwyFileDetectInfo *fileinfo,
                                          gboolean only_name);
static GwyContainer* igor_load           (const gchar *filename,
                                          GwyRunType mode,
                                          GError **error);
static guint         igor_read_headers   (IgorFile *igorfile,
                                          const guchar *buffer,
                                          gsize size,
                                          GError **error);
static guint         igor_checksum       (const IgorFile *igorfile,
                                          const guchar *buffer,
                                          gsize size);
static GwyDataField* igor_read_data_field(const guchar *buffer,
                                          gsize size,
                                          IgorFile *igorfile,
                                          GError **error);
static GwyContainer* igor_get_metadata   (IgorFile *igorfile);
static void          igor_file_free      (IgorFile *igorfile);

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
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len >= MIN_FILE_SIZE) {
       IgorFile igorfile;

       if (igor_read_headers(&igorfile, fileinfo->head, fileinfo->buffer_len,
                             NULL)) {
           igor_file_free(&igorfile);
           return 100;
       }
    }

    return 0;
}

static GwyContainer*
igor_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
#if 0
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
#endif
    return NULL;
}

/* Reads @header and initializes @reader for the correct byte order.  Returns
 * the number of bytes read, 0 on error. */
static guint
igor_read_headers(IgorFile *igorfile,
                  const guchar *buffer,
                  gsize size,
                  GError **error)
{
    IgorBinHeader *header;
    IgorWaveHeader5 *wave5;
    gsize headers_size;
    guint version, chksum, i;
    gboolean lsb;
    const guchar *p = buffer;

    if (size < HEADER_SIZE1) {
        err_TOO_SHORT(error);
        return 0;
    }

    /* The lower byte of version is nonzero.  Use it to detect endianess. */
    version = gwy_get_guint16_le(&p);
    gwy_debug("raw version: 0x%04x", version);

    /* Keep the rejection code path fast by performing version sanity check
     * as the very first thing. */
    if ((lsb = (version & 0xff))) {
        gwy_debug("little endian");
    }
    else {
        gwy_debug("big endian");
        version /= 0x100;
    }

    if (version != 1 && version != 2 && version != 3 && version != 5) {
        err_FILE_TYPE(error, "IGOR Pro");
        return 0;
    }

    memset(igorfile, 0, sizeof(IgorFile));
    header = &igorfile->header;
    wave5 = &igorfile->wave5;
    header->version = version;
    gwy_debug("format version: %u", header->version);

    if (lsb) {
        igorfile->get_guint16 = gwy_get_guint16_le;
        igorfile->get_gint16 = gwy_get_gint16_le;
        igorfile->get_guint32 = gwy_get_guint32_le;
        igorfile->get_gint32 = gwy_get_gint32_le;
        igorfile->get_gfloat = gwy_get_gfloat_le;
        igorfile->get_gdouble = gwy_get_gdouble_le;
    }
    else {
        igorfile->get_guint16 = gwy_get_guint16_be;
        igorfile->get_gint16 = gwy_get_gint16_be;
        igorfile->get_guint32 = gwy_get_guint32_be;
        igorfile->get_gint32 = gwy_get_gint32_be;
        igorfile->get_gfloat = gwy_get_gfloat_be;
        igorfile->get_gdouble = gwy_get_gdouble_be;
    }

    /* Check if version is known and the buffer size */
    if (header->version == 1)
        headers_size = HEADER_SIZE1 + WAVE_SIZE2;
    else if (header->version == 2)
        headers_size = HEADER_SIZE2 + WAVE_SIZE2;
    else if (header->version == 3)
        headers_size = HEADER_SIZE3 + WAVE_SIZE2;
    else if (header->version == 5)
        headers_size = HEADER_SIZE5 + WAVE_SIZE5;
    else {
        g_assert_not_reached();
    }
    gwy_debug("expected headers_size %lu", (gulong)headers_size);
    if (size < headers_size) {
        err_TOO_SHORT(error);
        return 0;
    }

    /* Read the rest of the binary header */
    if (header->version == 1) {
        header->wfm_size = igorfile->get_guint32(&p);
        header->checksum = igorfile->get_guint16(&p);
    }
    else if (header->version == 2) {
        header->wfm_size = igorfile->get_guint32(&p);
        header->note_size = igorfile->get_guint32(&p);
        header->pict_size = igorfile->get_guint32(&p);
        header->checksum = igorfile->get_guint16(&p);
    }
    else if (header->version == 3) {
        header->wfm_size = igorfile->get_guint32(&p);
        header->note_size = igorfile->get_guint32(&p);
        header->formula_size = igorfile->get_guint32(&p);
        header->pict_size = igorfile->get_guint32(&p);
        header->checksum = igorfile->get_guint16(&p);
    }
    else if (header->version == 5) {
        header->checksum = igorfile->get_guint16(&p);
        header->wfm_size = igorfile->get_guint32(&p);
        header->formula_size = igorfile->get_guint32(&p);
        header->note_size = igorfile->get_guint32(&p);
        header->data_e_units_size = igorfile->get_guint32(&p);
        for (i = 0; i < MAXDIMS; i++)
            header->dim_e_units_size[i] = igorfile->get_guint32(&p);
        for (i = 0; i < MAXDIMS; i++)
            header->dim_labels_size[i] = igorfile->get_guint32(&p);
        header->indices_size = igorfile->get_guint32(&p);
        header->options_size1 = igorfile->get_guint32(&p);
        header->options_size2 = igorfile->get_guint32(&p);
    }
    else {
        g_assert_not_reached();
    }

    /* Check the checksum */
    chksum = igor_checksum(igorfile, buffer, headers_size);
    gwy_debug("checksum calculated %u, header %u", chksum, header->checksum);
    if (chksum != header->checksum)
        return 0;

    return p - buffer;
}

static guint
igor_checksum(const IgorFile *igorfile,
              const guchar *buffer, gsize size)
{
    const guchar *p = buffer;
    guint n, sum;

    /* This ignores the last byte should the size be odd, IGOR seems to do
     * the same. */
    g_printerr(">>> %u\n", (guint)size/2);
    for (sum = 0, n = size/2; n; n--)
        sum += igorfile->get_guint16(&p);

    return sum & 0xffff;
}

#if 0
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

#endif

static void
igor_file_free(IgorFile *igorfile)
{
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
