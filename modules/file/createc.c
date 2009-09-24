/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Rok Zitko.
 *  E-mail: rok.zitko@ijs.si.
 *  Copyright (C) 2009 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  Based on nanoscope.c, Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

/* TODO:
 * - multiple images
 * - constant height or current
 * - saving
*/

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-createc-spm">
 *   <comment>Createc SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="[Parameter]"/>
 *     <match type="string" offset="0" value="[Paramco32]"/>
 *   </magic>
 * </mime-type>
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>

#include "err.h"

enum {
    HEADER_SIZE = 16384,
};

typedef enum {
    CREATEC_NONE,
    CREATEC_1,
    CREATEC_2,
    CREATEC_2Z,
} CreatecVersion;

static gboolean       module_register       (void);
static gint           createc_detect        (const GwyFileDetectInfo *fileinfo,
                                             gboolean only_name);
static GwyContainer*  createc_load          (const gchar *filename,
                                             GwyRunType mode,
                                             GError **error);
static CreatecVersion createc_get_version   (const gchar *buffer,
                                             gsize size);
static GHashTable*    read_hash             (gchar *buffer);
static GwyDataField*  hash_to_data_field    (GHashTable *hash,
                                             gint version,
                                             const gchar *buffer,
                                             gsize size,
                                             GError **error);
static void           read_binary_data      (gint n,
                                             gdouble *data,
                                             const gchar *buffer,
                                             gint bpp);
static GwyContainer*  createc_get_metadata  (GHashTable *hash);
static gchar*         unpack_compressed_data(const guchar *buffer,
                                             gsize size,
                                             gsize imagesize,
                                             gsize *datasize,
                                             GError **error);

static const GwyEnum versions[] = {
    { "[Parameter]", CREATEC_1,  },
    { "[Paramet32]", CREATEC_2,  },
    { "[Paramco32]", CREATEC_2Z, },
};

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Createc data files."),
    "Rok Zitko <rok.zitko@ijs.si>",
    "0.10",
    "Rok Zitko, David Nečas (Yeti)",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("createc",
                           N_("Createc files (.dat)"),
                           (GwyFileDetectFunc)&createc_detect,
                           (GwyFileLoadFunc)&createc_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
createc_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, ".dat") ? 10 : 0;

    if (createc_get_version(fileinfo->head, fileinfo->buffer_len))
        return 100;

    return 0;
}

static GwyContainer*
createc_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *meta, *container = NULL;
    guchar *buffer = NULL;
    gchar *p, *head;
    gsize size = 0;
    guint len;
    GError *err = NULL;
    GHashTable *hash = NULL;
    GwyDataField *dfield;
    CreatecVersion version;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (!(version = createc_get_version(buffer, size))) {
        err_FILE_TYPE(error, "Createc");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    head = g_memdup(buffer, HEADER_SIZE + 1);
    head[HEADER_SIZE] = '\0';
    len = strlen(gwy_enum_to_string(version, versions, G_N_ELEMENTS(versions)));
    for (p = head + len; g_ascii_isspace(*p); p++)
        ;
    hash = read_hash(p);
    dfield = hash_to_data_field(hash, version, buffer, size, error);

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);

        gwy_app_channel_title_fall_back(container, 0);

        meta = createc_get_metadata(hash);
        if (meta)
            gwy_container_set_object_by_name(container, "/0/meta", meta);
        g_object_unref(meta);
    }

    /* Must not free earlier, it holds the hash's strings */
    g_free(head);
    if (hash)
        g_hash_table_destroy(hash);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static CreatecVersion
createc_get_version(const gchar *buffer,
                    gsize size)
{
    guint i, len;

    for (i = 0; i < G_N_ELEMENTS(versions); i++) {
        len = strlen(versions[i].name);
        if (size > len && memcmp(versions[i].name, buffer, len) == 0) {
            gwy_debug("header=%s, version=%u",
                      versions[i].name, versions[i].value);
            return versions[i].value;
        }
    }
    gwy_debug("header=%.*s, no version matched",
              (int)strlen(versions[0].name), buffer);
    return CREATEC_NONE;
}

/* Read the ASCII header and fill the hash with key/value pairs */
static GHashTable*
read_hash(gchar *buffer)
{
    GHashTable *hash;
    gchar *line, *eq, *p;

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    p = buffer;
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        g_strstrip(line);
        if (!*line)
            continue;
        eq = strchr(line, '=');
        if (!eq) {
            g_warning("Stray line `%s'", line);
            continue;
        }
        *(eq++) = '\0';
        g_strchomp(line);
        while (g_ascii_isspace(*eq))
            eq++;

        /* drop entries without keyword */
        if (line[0]) {
            g_hash_table_insert(hash, line, eq);
            gwy_debug("<%s>: <%s>", line, eq);
        }
    }

    return hash;
}

#define createc_atof(x) g_ascii_strtod(x, NULL)

/* Macros to extract integer/double variables in hash_to_data_field() */
/* Any missing keyword/value pair is fatal, so we return a NULL pointer. */
#define HASH_GET(key, var, typeconv, err) \
    if (!(s = g_hash_table_lookup(hash, key))) { \
        err_MISSING_FIELD(err, key); \
        goto fail; \
    } \
    var = typeconv(s)

/* Support for alternative keywords in some (apparently newer) versions of dat
 * files */
#define HASH_GET2(key1, key2, var, typeconv, err) \
    if (!(s = g_hash_table_lookup(hash, key1))) { \
      if (!(s = g_hash_table_lookup(hash, key2))) { \
          g_set_error(err, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                      _("Neither `%s' nor `%s' header field found."), \
                      key1, key2); \
        goto fail; \
      } \
    } \
    var = typeconv(s)

#define HASH_INT(key, var, err)    HASH_GET(key, var, atoi, err)
#define HASH_DOUBLE(key, var, err) HASH_GET(key, var, createc_atof, err)
#define HASH_STRING(key, var, err) HASH_GET(key, var, /* */, err)

#define HASH_INT2(key1, key2, var, err)    HASH_GET2(key1, key2, var, atoi, err)
#define HASH_DOUBLE2(key1, key2, var, err) HASH_GET2(key1, key2, var, createc_atof, err)
#define HASH_STRING2(key1, key2, var, err) HASH_GET2(key1, key2, var, /* */, err)

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   gint version,
                   const gchar *buffer,
                   gsize size,
                   GError **error)
{
    GwyDataField *dfield = NULL;
    GwySIUnit *unit;
    const gchar *s; /* for HASH_GET macros */
    gint xres, yres, bpp, offset;
    gchar *imagedata = NULL;
    gsize datasize;
    gdouble xreal, yreal, q;
    gboolean is_current;
    gdouble *data;
    gint ti1, ti2; /* temporary storage */
    gdouble td; /* temporary storage */

    if (!hash)
        return NULL;

    if (version == CREATEC_1) {
        bpp = 2;
        offset = 16384 + 2; /* header + 2 unused bytes */
    }
    else if (version == CREATEC_2) {
        bpp = 4;
        offset = 16384 + 4; /* header + 4 unused bytes */
    }
    else if (version == CREATEC_2Z) {
        bpp = 4;
        offset = 16384; /* header */
    }
    else {
        g_return_val_if_reached(NULL);
    }

    if (size < offset) {
        err_TOO_SHORT(error);
        return NULL;
    }

    is_current = FALSE;

    HASH_INT2("Num.X", "Num.X / Num.X", xres, error);
    HASH_INT2("Num.Y", "Num.Y / Num.Y", yres, error);
    if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
        return NULL;

    if (version == CREATEC_2Z) {
        if (!(imagedata = unpack_compressed_data(buffer + offset, size - offset,
                                                 bpp*xres*yres,
                                                 &datasize, error)))
            return NULL;

        /* Point buffer to the decompressed data. */
        buffer = imagedata;
        size = datasize;
        offset = 4;   /* the usual 4 unused bytes */
    }

    if (err_SIZE_MISMATCH(error, offset + bpp*xres*yres, size, FALSE))
        goto fail;

    HASH_INT2("Delta X", "Delta X / Delta X [Dac]", ti1, error);
    HASH_INT2("GainX", "GainX / GainX", ti2, error);
    HASH_DOUBLE("Xpiezoconst", td, error); /* lowcase p, why? */
    xreal = xres * ti1; /* dacs */
    xreal *= 20.0/65536.0 * ti2; /* voltage per dac */
    xreal *= td * 1.0e-10; /* piezoconstant [A/V] */
    if (!(xreal = fabs(xreal))) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        xreal = 1.0;
    }

    HASH_INT2("Delta Y", "Delta Y / Delta Y [Dac]", ti1, error);
    HASH_INT2("GainY", "GainY / GainY", ti2, error);
    HASH_DOUBLE("YPiezoconst", td, error); /* upcase P */
    yreal = yres * ti1;
    yreal *= 20.0/65536.0 * ti2;
    yreal *= td * 1.0e-10;
    if (!(yreal = fabs(yreal))) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        yreal = 1.0;
    }

    HASH_INT2("GainZ", "GainZ / GainZ", ti2, error);
    HASH_DOUBLE("ZPiezoconst", td, error); /* upcase P */
    q = 1.0; /* unity dac */
    q *= 20.0/65536.0 * ti2; /* voltage per dac */
    q *= td * 1.0e-10; /* piezoconstant [A/V] */

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    read_binary_data(xres*yres, data, buffer + offset, bpp);
    gwy_data_field_multiply(dfield, q);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    unit = gwy_si_unit_new(is_current ? "A" : "m");
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

fail:
    g_free(imagedata);

    return dfield;
}

/* Macro for storing meta data */

#define HASH_STORE(key) \
    if ((val = g_hash_table_lookup(hash, key))) { \
        gwy_debug("key = %s, val = %s", key, val); \
        gwy_container_set_string_by_name(meta, key, g_strdup(val)); \
    }

static GwyContainer*
createc_get_metadata(GHashTable *hash)
{
    /* Relocation-less storage */
    static const gchar tobestored[] =
        "Titel\0"
        "Titel / Titel\0"
        "Length x[A]\0"
        "Length y[A]\0"
        "Z-Res. [A]: +/- \0"
        "BiasVoltage\0"
        "BiasVoltage / BiasVolt.[mV]\0"
        "Current[A]\0"
        "Delta X\0"
        "Delta X / Delta X [Dac]\0"
        "Delta Y\0"
        "Delta Y / Delta Y [Dac]\0"
        "Delay X+\0"
        "Delay X+ / Delay X+\0"
        "Delay X-\0"
        "Delay X- / Delay X-\0"
        "Delay Y\0"
        "Delay Y / Delay Y\0"
        "D-DeltaX\0"
        "D-DeltaX / D-DeltaX\0"
        "Rotation\0"
        "Rotation / Rotation\0"
        "GainX\0"
        "GainX / GainX\0"
        "GainY\0"
        "GainY / GainY\0"
        "GainZ\0"
        "GainZ / GainZ\0"
        "Gainpreamp\0"
        "Gainpreamp / GainPre 10^\0"
        "Chan(1,2,4)\0"
        "Chan(1,2,4) / Chan(1,2,4)\0"
        "Scancoarse\0"
        "Scancoarse / Scancoarse\0"
        "Scantype\0"
        "Scantype / Scantype\0"
        "FBIset\0"
        "FBLogIset\0"
        "FBRC\0"
        "FBLingain\0"
        "FBLog\0"
        "FBPropGain\0"
        "ZPiezoconst\0"
        "Xpiezoconst\0"
        "YPiezoconst\0"
        "Sec/line:\0"
        "Sec/Image:\0"
        "Channels\0"
        "Channels / Channels\0"
        "Dactonmx\0"
        "Dacto[A]xy\0"
        "Dactonmz\0"
        "Dacto[A]z\0"
        "memo:0\0"
        "memo:1\0"
        "memo:2\0"
        "T_ADC2[K]\0"
        "T_ADC3[K]\0"
        "\0";
    GwyContainer *meta;
    const gchar *ctr;
    gchar *val;

    meta = gwy_container_new();

    for (ctr = tobestored; *ctr; ctr += strlen(ctr) + 1)
        HASH_STORE(ctr);

    if (!gwy_container_get_n_items(meta)) {
        g_object_unref(meta);
        meta = NULL;
    }

    return meta;
}

static void
read_binary_data(gint n,
                 gdouble *data,
                 const gchar *buffer,
                 gint bpp)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8*bpp));
    switch (bpp) {
        case 2:
        {
            const gint16 *p = (const gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*GINT16_FROM_LE(p[i]);
        }
        break;

        case 4:
        {
            const guchar *p = (const guchar*)buffer;

            for (i = 0; i < n; i++)
                data[i] = gwy_get_gfloat_le(&p);
        }

        break;

        default:
        g_return_if_reached();
        break;
    }
}

#ifdef HAVE_ZLIB
/* XXX: Common with matfile.c */
static inline gboolean
zinflate_into(z_stream *zbuf,
              gint flush_mode,
              gsize csize,
              const guchar *compressed,
              GByteArray *output,
              GError **error)
{
    gint status;
    gboolean retval = TRUE;

    gwy_clear(zbuf, 1);
    zbuf->next_in = (char*)compressed;
    zbuf->avail_in = csize;
    zbuf->next_out = output->data;
    zbuf->avail_out = output->len;

    /* XXX: zbuf->msg does not seem to ever contain anything, so just report
     * the error codes. */
    if ((status = inflateInit(zbuf)) != Z_OK) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_SPECIFIC,
                    _("zlib initialization failed with error %d, "
                      "cannot decompress data."),
                    status);
        return FALSE;
    }

    if ((status = inflate(zbuf, flush_mode)) != Z_OK
        /* zlib return Z_STREAM_END also when we *exactly* exhaust all input.
         * But this is no error, in fact it should happen every time, so check
         * for it specifically. */
        && !(status == Z_STREAM_END
             && zbuf->total_in == csize
             && zbuf->total_out == output->len)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Decompression of compressed data failed with "
                      "error %d."),
                    status);
        retval = FALSE;
    }

    status = inflateEnd(zbuf);
    /* This should not really happen whatever data we pass in.  And we have
     * already our output, so just make some noise and get over it.  */
    if (status != Z_OK)
        g_critical("inflateEnd() failed with error %d", status);

    return retval;
}

static gchar*
unpack_compressed_data(const guchar *buffer,
                       gsize size,
                       gsize imagesize,
                       gsize *datasize,
                       GError **error)
{
    gsize compressed_size = size - HEADER_SIZE;
    /* Estimate how many data fields we might decompress */
    guint ndata = compressed_size/imagesize + 1;
    gsize expected_size = ndata*imagesize + 4;  /* 4 unused bytes, as usual */
    z_stream zbuf; /* decompression stream */
    GByteArray *output;
    gboolean ok;

    gwy_debug("Expecting to decompress %u data fields", ndata);
    output = g_byte_array_sized_new(expected_size);
    g_byte_array_set_size(output, expected_size);
    ok = zinflate_into(&zbuf, Z_SYNC_FLUSH, compressed_size, buffer,
                       output, error);
    *datasize = output->len;

    return g_byte_array_free(output, !ok);
}
#else
static gchar*
unpack_compressed_data(G_GNUC_UNUSED const guchar *buffer,
                       G_GNUC_UNUSED gsize size,
                       G_GNUC_UNUSED gsize imagesize,
                       gsize *datasize,
                       GError **error)
{
    *datasize = 0;
    g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_SPECIFIC,
                _("Cannot decompress compressed data.  Zlib support was "
                  "not built in."));
    return NULL;
}
#endif

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
