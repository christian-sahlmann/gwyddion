/*
 *  $Id$
 *  Copyright (C) 2009 David Necas (Yeti).
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

/**
 * [FILE-MAGIC-USERGUIDE]
 * WaveMetrics IGOR binary wave v5
 * .ibw
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"
#include "get.h"

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
    IGOR_BASE = 0,
    IGOR_ASYLUM_MPF3D = 1,
    IGOR_ASYLUM_FORCE = 2,
} IgorFileProducerVariant;

typedef enum {
    IGOR_TEXT     = 0x00,
    IGOR_COMPLEX  = 0x01,   /* Flag */
    IGOR_SINGLE   = 0x02,
    IGOR_DOUBLE   = 0x04,
    IGOR_INT8     = 0x08,
    IGOR_INT16    = 0x10,
    IGOR_INT32    = 0x20,
    IGOR_UNSIGNED = 0x40,   /* Flag for integers */
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
    guint npts;           /* Total number of points
                             (multiply dimensions up to first zero). */
    IgorDataType type;
    guint lock;           /* Reserved (0). */
    gchar whpad1[6];      /* Reserved (0). */
    guint wh_version;     /* Reserved (1). */
    gchar bname[MAX_WAVE_NAME5+1];  /* Wave name, nul-terminated. */
    guint whpad2;         /* Reserved (0). */
    guint dfolder;        /* Pointer, ignore. */

    /* Dimensioning info. [0] == rows, [1] == cols etc */
    guint n_dim[MAXDIMS];   /* Number of items in a dimension,
                               0 means no data. */
    gdouble sfA[MAXDIMS];   /* Index value for element e of dimension
                               d = sfA[d]*e + sfB[d]. */
    gdouble sfB[MAXDIMS];

    /* SI units */
    gchar data_units[MAX_UNIT_CHARS+1];  /* Natural data units, nul if none. */
    gchar dim_units[MAXDIMS][MAX_UNIT_CHARS+1];   /* Natural dimension units,
                                                     nul if none. */

    gboolean fs_valid;           /* TRUE if full scale values have meaning. */
    guint whpad3;                /* Reserved (0). */
    gdouble top_full_scale;      /* The instrument max full scale value. */
    gdouble bot_full_scale;      /* The instrument min full scale value. */

    /* There is more stuff.  But it's either marked reserved, unused or private
     * to Igor.  Do not bother with that... */
} IgorWaveHeader5;

typedef struct {
    gchar *name;
    const gchar *units;
} AsylumChannelInfo;

typedef struct {
    guint16 (*get_guint16)(const guchar **p);
    gint16 (*get_gint16)(const guchar **p);
    guint32 (*get_guint32)(const guchar **p);
    gint32 (*get_gint32)(const guchar **p);
    gfloat (*get_gfloat)(const guchar **p);
    gdouble (*get_gdouble)(const guchar **p);
    IgorFileProducerVariant variant;
    guint wave_header_size;
    guint headers_size;
    guint type_size;
    IgorBinHeader header;
    IgorWaveHeader5 wave5;
    guint nchannels;
    /* Maybe only in Asylum Research files.  Titles dynamically allocated. */
    GPtrArray *titles;
    /* Only in Asylum Research files... */
    GHashTable *meta;
    AsylumChannelInfo *channel_info;
    /* Processing data */
    const gchar **ignore_prefixes;
    GwyContainer *channelmeta;
} IgorFile;

static gboolean      module_register       (void);
static gint          igor_detect           (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* igor_load             (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static guint         igor_read_headers     (IgorFile *igorfile,
                                            const guchar *buffer,
                                            gsize size,
                                            gboolean check_only,
                                            GError **error);
static guint         igor_checksum         (gconstpointer buffer,
                                            gsize size,
                                            gboolean lsb);
static guint         igor_data_type_size   (IgorDataType type);
static GwyDataField* igor_read_data_field  (const IgorFile *igorfile,
                                            const guchar *buffer,
                                            guint i,
                                            const gchar *zunits,
                                            gboolean imaginary);
static GwyContainer* igor_get_metadata     (IgorFile *igorfile,
                                            guint id);
static GPtrArray*    read_channel_labels   (const gchar *p,
                                            guint n, guint l);
static gchar*        canonicalize_title    (const gchar *title);
static const gchar*  channel_title_to_units(const gchar *title);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Igor binary waves (.ibw)."),
    "Yeti <yeti@gwyddion.net>",
    "0.5",
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
                             TRUE, NULL))
           return 100;
    }

    return 0;
}

static GwyContainer*
igor_load(const gchar *filename,
          G_GNUC_UNUSED GwyRunType mode,
          GError **error)
{
    GwyContainer *meta = NULL, *container = NULL;
    GwyDataField *dfield = NULL;
    GwyTextHeaderParser parser;
    IgorFile igorfile;
    IgorWaveHeader5 *wave5;
    GError *err = NULL;
    guchar *p, *buffer = NULL;
    gint xres, yres;
    gsize expected_size, size = 0;
    gchar *note = NULL;
    const gchar *value;
    gchar key[64];
    guint i, chid;
    GQuark quark;
    guint nlabels;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    gwy_clear(&igorfile, 1);
    if (!igor_read_headers(&igorfile, buffer, size, FALSE, error))
        goto fail;

    /* Only accept v5 files because older do not support 2D data */
    if (igorfile.header.version != 5) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Format version is %d.  Only version 5 is supported."),
                    igorfile.header.version);
        goto fail;
    }

    /* Detect Asylum research files, leave it at generic if not detected. */
    if (memcmp(buffer + size-5, "MFP3D", 5) == 0)
        igorfile.variant = IGOR_ASYLUM_MPF3D;
    else if (memcmp(buffer + size-5, "Force", 5) == 0)
        igorfile.variant = IGOR_ASYLUM_FORCE;
    gwy_debug("producer variant %u", igorfile.variant);

    /* Must have exactly 3 dims: xres, yres, nchannels */
    wave5 = &igorfile.wave5;
    xres = wave5->n_dim[0];
    yres = wave5->n_dim[1];
    igorfile.nchannels = wave5->n_dim[2];
    if (igorfile.nchannels==0) igorfile.nchannels=1;

    if (!xres || !yres || !igorfile.nchannels || wave5->n_dim[3]) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Only two-dimensional data are supported."));
        goto fail;
    }

    igorfile.type_size = igor_data_type_size(wave5->type);
    if (!igorfile.type_size) {
        err_DATA_TYPE(error, wave5->type);
        goto fail;
    }

    if (wave5->npts != xres*yres*igorfile.nchannels) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Number of data points %u does not match resolutions "
                      "%u×%u×%u."),
                    wave5->npts, xres, yres, igorfile.nchannels);
        goto fail;
    }

    if (igorfile.header.wfm_size <= igorfile.wave_header_size) {
        err_INVALID(error, "wfmSize");
        goto fail;
    }

    expected_size = igorfile.header.wfm_size - igorfile.wave_header_size;
    if (expected_size != wave5->npts*igorfile.type_size) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data size %u does not match "
                      "the number of data points %u×%u."),
                    (guint)expected_size, wave5->npts, igorfile.type_size);
    }

    if (err_SIZE_MISMATCH(error, expected_size + igorfile.headers_size, size,
                          FALSE))
        goto fail;

    p = buffer + igorfile.headers_size + expected_size;
    gwy_debug("remaning data size: %lu", (gulong)(size - (p - buffer)));

    p += igorfile.header.formula_size;
    if ((igorfile.variant == IGOR_ASYLUM_FORCE
         || igorfile.variant == IGOR_ASYLUM_MPF3D)
        && igorfile.header.note_size
        && (p - buffer) + igorfile.header.note_size <= size) {
        note = g_strndup((const gchar*)p, size);
        gwy_clear(&parser, 1);
        parser.key_value_separator = ":";
        igorfile.meta = gwy_text_header_parse(note, &parser, NULL, NULL);
    }
    p += igorfile.header.note_size;

    /* FIXME: Support extended units for non-Asylum files! */
    p += igorfile.header.data_e_units_size;
    for (i = 0; i < MAXDIMS; i++)
        p += igorfile.header.dim_e_units_size[i];

    /* Skip labels of x and y dimension, we don't know what to do with them. */
    for (i = 0; i < 2; i++)
        p += igorfile.header.dim_labels_size[i];

    /* FIXME: The labels are mandatory only in Asylum Research files. */
    nlabels=igorfile.header.dim_labels_size[2]/(MAX_WAVE_NAME5+1);
    expected_size = (MAX_WAVE_NAME5 + 1)*(nlabels);
    if ( (p - buffer) + expected_size > size ) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot read channel labels."));
        goto fail;
    }
    igorfile.titles = read_channel_labels(p, igorfile.nchannels+1, nlabels);
    p += igorfile.header.dim_labels_size[2];

    if (igorfile.meta) {
        igorfile.channel_info = g_new(AsylumChannelInfo, igorfile.nchannels);
        for (i = 0; i < igorfile.nchannels; i++) {
            AsylumChannelInfo *chinfo = igorfile.channel_info + i;
            const gchar *title = g_ptr_array_index(igorfile.titles, i+1);

            chinfo->name = canonicalize_title(title);
            g_snprintf(key, sizeof(key), "%sUnit", chinfo->name);
            value = g_hash_table_lookup(igorfile.meta, key);
            if (value)
                chinfo->units = value;
            else
                chinfo->units = channel_title_to_units(chinfo->name);
        }
    }

    container = gwy_container_new();

    for (i = chid = 0; i < igorfile.nchannels; i++, chid++) {
        const gchar *title = g_ptr_array_index(igorfile.titles, i+1);
        const gchar *zunits = NULL;

        if (igorfile.channel_info) {
            AsylumChannelInfo *chinfo = igorfile.channel_info + i;
            zunits = chinfo->units;
            meta = igor_get_metadata(&igorfile, i + 1);
        }
        dfield = igor_read_data_field(&igorfile, buffer, i, zunits, FALSE);
        quark = gwy_app_get_data_key_for_id(chid);
        gwy_container_set_object(container, quark, dfield);
        g_object_unref(dfield);
        if (meta) {
            g_snprintf(key, sizeof(key), "/%d/meta", chid);
            gwy_container_set_object_by_name(container, key, meta);
        }

        if(title) {
            g_snprintf(key, sizeof(key), "/%d/data/title", chid);
            gwy_container_set_string_by_name(container, key, g_strdup(title));
        }
        gwy_app_channel_title_fall_back(container,chid);

        if (wave5->type & IGOR_COMPLEX) {
            chid++;
            dfield = igor_read_data_field(&igorfile, buffer, i, zunits, TRUE);
            quark = gwy_app_get_data_key_for_id(chid);
            gwy_container_set_object(container, quark, dfield);
            g_object_unref(dfield);
            if (meta) {
                g_snprintf(key, sizeof(key), "/%d/meta", chid);
                /* container still holds a reference */
                g_object_unref(meta);
                meta = gwy_container_duplicate(meta);
                gwy_container_set_object_by_name(container, key, meta);
            }

            if (title) {
                g_snprintf(key, sizeof(key), "/%d/data/title", chid);
                gwy_container_set_string_by_name(container, key, g_strdup(title));
            };
            gwy_app_channel_title_fall_back(container,chid);
        }
        gwy_object_unref(meta);
    }

fail:
    gwy_file_abandon_contents(buffer, size, NULL);
    g_free(note);
    if (igorfile.channel_info) {
        for (i = 0; i < igorfile.nchannels; i++)
            g_free(igorfile.channel_info[i].name);
        g_free(igorfile.channel_info);
    }
    if (igorfile.meta)
        g_hash_table_destroy(igorfile.meta);
    if (igorfile.titles) {
        g_ptr_array_foreach(igorfile.titles, (GFunc)g_free, NULL);
        g_ptr_array_free(igorfile.titles, TRUE);
    }

    return container;
}

/* Reads @header and initializes @reader for the correct byte order.  Returns
 * the number of bytes read, 0 on error. */
static guint
igor_read_headers(IgorFile *igorfile,
                  const guchar *buffer,
                  gsize size,
                  gboolean check_only,
                  GError **error)
{
    IgorBinHeader *header;
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

    /* Check if version is known and the buffer size */
    if (version == 1)
        headers_size = HEADER_SIZE1 + WAVE_SIZE2;
    else if (version == 2)
        headers_size = HEADER_SIZE2 + WAVE_SIZE2;
    else if (version == 3)
        headers_size = HEADER_SIZE3 + WAVE_SIZE2;
    else if (version == 5)
        headers_size = HEADER_SIZE5 + WAVE_SIZE5;
    else {
        err_FILE_TYPE(error, "IGOR Pro");
        return 0;
    }
    gwy_debug("expected headers_size %lu", (gulong)headers_size);
    if (size < headers_size) {
        err_TOO_SHORT(error);
        return 0;
    }

    /* Check the checksum */
    chksum = igor_checksum(buffer, headers_size, lsb);
    gwy_debug("checksum %u", chksum);
    if (chksum) {
        err_FILE_TYPE(error, "IGOR Pro");
        return 0;
    }

    /* If only detection is required, we can stop now. */
    if (check_only)
        return headers_size;

    /* If the checksum is correct the file is likely IGOR file and we can
     * start the expensive actions. */
    gwy_clear(igorfile, 1);
    header = &igorfile->header;
    header->version = version;
    igorfile->headers_size = headers_size;
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

    /* Read the rest of the binary header */
    if (header->version == 1) {
        igorfile->wave_header_size = 110;
        header->wfm_size = igorfile->get_guint32(&p);
        header->checksum = igorfile->get_guint16(&p);
    }
    else if (header->version == 2) {
        igorfile->wave_header_size = 110;
        header->wfm_size = igorfile->get_guint32(&p);
        header->note_size = igorfile->get_guint32(&p);
        header->pict_size = igorfile->get_guint32(&p);
        header->checksum = igorfile->get_guint16(&p);
    }
    else if (header->version == 3) {
        igorfile->wave_header_size = 110;
        header->wfm_size = igorfile->get_guint32(&p);
        header->note_size = igorfile->get_guint32(&p);
        header->formula_size = igorfile->get_guint32(&p);
        header->pict_size = igorfile->get_guint32(&p);
        header->checksum = igorfile->get_guint16(&p);
    }
    else if (header->version == 5) {
        igorfile->wave_header_size = 320;
        header->checksum = igorfile->get_guint16(&p);
        header->wfm_size = igorfile->get_guint32(&p);
        header->formula_size = igorfile->get_guint32(&p);
        gwy_debug("formula_size: %u", header->formula_size);
        header->note_size = igorfile->get_guint32(&p);
        gwy_debug("note_size: %u", header->note_size);
        header->data_e_units_size = igorfile->get_guint32(&p);
        gwy_debug("data_e_units_size: %u", header->data_e_units_size);
        for (i = 0; i < MAXDIMS; i++) {
            header->dim_e_units_size[i] = igorfile->get_guint32(&p);
            gwy_debug("dim_e_units_size[%u]: %u", i, header->dim_e_units_size[i]);
        }
        for (i = 0; i < MAXDIMS; i++) {
            header->dim_labels_size[i] = igorfile->get_guint32(&p);
            gwy_debug("dim_labels_size[%u]: %u", i, header->dim_labels_size[i]);
        }
        header->indices_size = igorfile->get_guint32(&p);
        header->options_size1 = igorfile->get_guint32(&p);
        header->options_size2 = igorfile->get_guint32(&p);
    }
    else {
        g_assert_not_reached();
    }

    gwy_debug("wfm_size: %u", header->wfm_size);

    /* Read the wave header */
    if (version == 5) {
        IgorWaveHeader5 *wave5 = &igorfile->wave5;

        wave5->next = igorfile->get_guint32(&p);
        wave5->creation_date = igorfile->get_guint32(&p);
        wave5->mod_date = igorfile->get_guint32(&p);
        wave5->npts = igorfile->get_guint32(&p);
        wave5->type = igorfile->get_guint16(&p);
        gwy_debug("type: %u, npts: %u", wave5->type, wave5->npts);
        wave5->lock = igorfile->get_guint16(&p);
        get_CHARARRAY(wave5->whpad1, &p);
        wave5->wh_version = igorfile->get_guint16(&p);
        get_CHARARRAY0(wave5->bname, &p);
        wave5->whpad2 = igorfile->get_guint32(&p);
        wave5->dfolder = igorfile->get_guint32(&p);
        for (i = 0; i < MAXDIMS; i++) {
            wave5->n_dim[i] = igorfile->get_guint32(&p);
            gwy_debug("n_dim[%u]: %u", i, wave5->n_dim[i]);
        }
        for (i = 0; i < MAXDIMS; i++)
            wave5->sfA[i] = igorfile->get_gdouble(&p);
        for (i = 0; i < MAXDIMS; i++)
            wave5->sfB[i] = igorfile->get_gdouble(&p);
        get_CHARARRAY0(wave5->data_units, &p);
        gwy_debug("data_units: <%s>", wave5->data_units);
        for (i = 0; i < MAXDIMS; i++) {
            get_CHARARRAY0(wave5->dim_units[i], &p);
            gwy_debug("dim_units[%u]: <%s>", i, wave5->dim_units[i]);
        }
        wave5->fs_valid = !!igorfile->get_guint16(&p);
        wave5->whpad3 = igorfile->get_guint16(&p);
        wave5->top_full_scale = igorfile->get_gdouble(&p);
        wave5->bot_full_scale = igorfile->get_gdouble(&p);
    }

    return headers_size;
}

/* The way the checksum is constructed (header->checksum is the complement),
 * the return value is expected to be zero */
static guint
igor_checksum(gconstpointer buffer, gsize size, gboolean lsb)
{
    const guint16 *p = (const guint16*)buffer;
    guint i, sum;

    /* This ignores the last byte should the size be odd, IGOR seems to do
     * the same. */
    if (lsb) {
        for (sum = 0, i = 0; i < size/2; ++i)
            sum += GUINT16_FROM_LE(p[i]);
    }
    else {
        for (sum = 0, i = 0; i < size/2; ++i)
            sum += GUINT16_FROM_BE(p[i]);
    }

    return sum & 0xffff;
}

static guint
igor_data_type_size(IgorDataType type)
{
    IgorDataType basetype;
    gsize size;

    basetype = type & ~(IGOR_UNSIGNED | IGOR_COMPLEX);
    if (basetype == IGOR_SINGLE || basetype == IGOR_INT32)
        size = 4;
    else if (basetype == IGOR_INT16)
        size = 2;
    else if (basetype == IGOR_DOUBLE)
        size = 8;
    else if (basetype == IGOR_INT8)
        size = 1;
    else
        return 0;

    /* unsigned is invalid on floats */
    if ((type & IGOR_UNSIGNED)
        && (basetype == IGOR_DOUBLE || basetype == IGOR_SINGLE))
        return 0;

    if (type & IGOR_COMPLEX)
        size *= 2;

    return size;
}

static GwyDataField*
igor_read_data_field(const IgorFile *igorfile,
                     const guchar *buffer,
                     guint i,
                     const gchar *zunits,
                     gboolean imaginary)
{
    const IgorWaveHeader5 *wave5;
    guint n, xres, yres, skip;
    GwyDataField *dfield;
    GwySIUnit *unit;
    gdouble *data;
    const guchar *p;
    gint power10;
    gdouble q;

    wave5 = &igorfile->wave5;
    xres = wave5->n_dim[0];
    yres = wave5->n_dim[1];
    p = buffer + igorfile->headers_size + xres*yres*igorfile->type_size*i;
    n = xres*yres;

    dfield = gwy_data_field_new(xres, yres,
                                wave5->sfA[0]*xres, wave5->sfA[1]*yres,
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    g_return_val_if_fail(!imaginary || (wave5->type & IGOR_COMPLEX), dfield);
    skip = imaginary ? igorfile->type_size/2 : 0;
    if (imaginary)
        p += skip;

    /* TODO: Support extended units */
    unit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string_parse(unit, wave5->dim_units[0], &power10);
    gwy_data_field_set_xreal(dfield, pow10(power10)*wave5->sfA[0]*xres);
    gwy_data_field_set_yreal(dfield, pow10(power10)*wave5->sfA[1]*yres);

    unit = gwy_data_field_get_si_unit_z(dfield);
    gwy_si_unit_set_from_string_parse(unit, zunits ? zunits : wave5->data_units,
                                      &power10);
    q = pow10(power10);

    switch ((guint)wave5->type) {
        case IGOR_INT8:
        {
            const gint8 *ps = (const gint8*)buffer;
            while (n--) {
                *(data++) = *(ps++) * q;
                ps += skip;
            }
        }
        break;

        case IGOR_INT8 | IGOR_UNSIGNED:
        while (n--) {
            *(data++) = *(p++) * q;
            p += skip;
        }
        break;

        case IGOR_INT16:
        while (n--) {
            *(data++) = igorfile->get_gint16(&p) * q;
            p += skip;
        }
        break;

        case IGOR_INT16 | IGOR_UNSIGNED:
        while (n--) {
            *(data++) = igorfile->get_guint16(&p) * q;
            p += skip;
        }
        break;

        case IGOR_INT32:
        while (n--) {
            *(data++) = igorfile->get_gint32(&p) * q;
            p += skip;
        }
        break;

        case IGOR_INT32 | IGOR_UNSIGNED:
        while (n--) {
            *(data++) = igorfile->get_guint32(&p) * q;
            p += skip;
        }
        break;

        case IGOR_SINGLE:
        while (n--) {
            *(data++) = igorfile->get_gfloat(&p) * q;
            p += skip;
        }
        break;

        case IGOR_DOUBLE:
        while (n--) {
            *(data++) = igorfile->get_gdouble(&p) * q;
            p += skip;
        }
        break;

        default:
        g_return_val_if_reached(NULL);
        break;
    }

    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);

    return dfield;
}

static GPtrArray*
read_channel_labels(const gchar *p,
                    guint n, guint l)
{
    GPtrArray *array = g_ptr_array_sized_new(n);
    guint i;

    for (i = 0; i < l; i++) {
        g_ptr_array_add(array,
                        g_strndup(p + i*(MAX_WAVE_NAME5 + 1), MAX_WAVE_NAME5));
        gwy_debug("label%u=%s", i, (gchar*)g_ptr_array_index(array, i));
    }
    for (i = l; i < n; i++) {
      g_ptr_array_add(array,NULL);
      gwy_debug("label%u=NULL", i);
    };

    return array;
}

static gchar*
canonicalize_title(const gchar *title)
{
    gchar *name, *s;
    guint len;

    name = g_strdup(title);
    len = strlen(name);

    if ((s = strstr(name, "Mod"))) {
        gchar *t = s + strlen("Mod");
        while (g_ascii_isdigit(*t))
            t++;
        if (!*t)
            *s = '\0';
        len = s - name;
    }

    if (g_str_has_suffix(name, "Trace"))
        name[len - strlen("Trace")] = '\0';
    else if (g_str_has_suffix(name, "Retrace"))
        name[len - strlen("Retrace")] = '\0';

    return name;
}

static const gchar*
channel_title_to_units(const gchar *title)
{
    if (g_str_has_prefix(title, "Height")
        || g_str_has_prefix(title, "ZSensor")
        || g_str_has_prefix(title, "Deflection")
        || g_str_has_prefix(title, "Amplitude"))
        return "m";
    if (g_str_has_prefix(title, "Phase"))
        return "deg";
    if (g_str_has_prefix(title, "Current"))
        return "A";
    if (g_str_has_prefix(title, "Frequency"))
        return "Hz";
    if (g_str_has_prefix(title, "Capacitance"))
        return "F";
    if (g_str_has_prefix(title, "Potential"))
        return "V";
    if (g_str_has_prefix(title, "Count"))
        return "";
    /* Everything else is in Volts. */
    return "V";
}

static void
gather_channel_meta(gpointer hkey,
                    gpointer hvalue,
                    gpointer user_data)
{
    const gchar *key = (const gchar*)hkey;
    const gchar *value = (const gchar*)hvalue;
    IgorFile *igorfile = (IgorFile*)user_data;
    guint i;

    if (!*value)
        return;
    for (i = 0; i < igorfile->nchannels; i++) {
        if (g_str_has_prefix(key, igorfile->channel_info[i].name))
            return;
    }
    for (i = 0; igorfile->ignore_prefixes[i]; i++) {
        if (g_str_has_prefix(key, igorfile->ignore_prefixes[i]))
            return;
    }

    if (g_utf8_validate(value, -1, NULL))
        value = g_strdup(value);
    else
        value = g_convert(value, -1, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);

    if (value)
        gwy_container_set_string_by_name(igorfile->channelmeta, key, value);
}

static GwyContainer*
igor_get_metadata(IgorFile *igorfile,
                  G_GNUC_UNUSED guint id)
{
    static const gchar *ignore_prefixes[] = {
        "Channel", "ColorMap", "Display", "Flatten", "PlaneFit", "Planefit",
        NULL
    };

    if (!igorfile->meta)
        return NULL;

    if (!igorfile->ignore_prefixes)
        igorfile->ignore_prefixes = ignore_prefixes;

    igorfile->channelmeta = gwy_container_new();
    g_hash_table_foreach(igorfile->meta, gather_channel_meta, igorfile);

    return igorfile->channelmeta;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
