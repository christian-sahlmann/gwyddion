/*
 *  @(#) $Id$
 *  Copyright (C) 2004 Rok Zitko
 *  E-mail: rok.zitko@ijs.si
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
 * - multiple images / selection dialog
 * - constant height or current
 * - saving
 *
 * (Yeti):
 * FIXME: I do not have specs.
*/

#include "config.h"
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAGIC_TXT "[Parameter]"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

static gboolean      module_register      (const gchar *name);
static gint          createc_detect       (const GwyFileDetectInfo *fileinfo,
                                           gboolean only_name);
static GwyContainer* createc_load         (const gchar *filename,
                                           GwyRunType mode,
                                           GError **error);
static GHashTable*   read_hash            (gchar *buffer);
static GwyDataField* hash_to_data_field   (GHashTable *hash,
                                           gchar *buffer,
                                           GError **error);
static gboolean      read_binary_data     (gint n,
                                           gdouble *data,
                                           gchar *buffer,
                                           gint bpp,
                                           GError **error);
static void          store_metadata       (GwyContainer *data,
                                           GHashTable *hash);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Createc data files."),
    "Rok Zitko <rok.zitko@ijs.si>",
    "0.5",
    "Rok Zitko",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    static GwyFileFuncInfo createc_func_info = {
        "createc",
        N_("Createc files (.dat)"),
        (GwyFileDetectFunc)&createc_detect,
        (GwyFileLoadFunc)&createc_load,
        NULL,
        NULL
    };

    gwy_file_func_register(name, &createc_func_info);

    return TRUE;
}

static gint
createc_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, ".dat") ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->buffer, MAGIC_TXT, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static GwyContainer*
createc_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    gchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GHashTable *hash = NULL;
    GwyDataField *dfield;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_IO,
                    "%s", err->message);
        g_clear_error(&err);
        return NULL;
    }
    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC_TXT, MAGIC_SIZE) != 0) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is not a Createc image file."));
        g_free(buffer);
        return NULL;
    }

    hash = read_hash(buffer);

    dfield = hash_to_data_field(hash, buffer, error);

    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);

        store_metadata(container, hash);

    }
    g_hash_table_destroy(hash);
    g_free(buffer);

    return container;
}

/* Read the ASCII header and fill the hash with key/value pairs */
static GHashTable*
read_hash(gchar *buffer)
{
    GHashTable *hash;
    gchar *line, *eq, *p;

    p = buffer;

    line = gwy_str_next_line(&p);
    if (!line)
        return NULL;
    if (!strstr(line, "[Parameter]") != 0)
        return NULL;

    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    while (p[0]) {
        line = gwy_str_next_line(&p);
        if (!line)
            goto fail;
        eq = strchr(line, '=');
        if (!eq)
            goto fail;
        *eq = '\0';
        if (*line != '\0') { /* drop entries without keyword */
          g_hash_table_insert(hash, g_strdup(line), g_strdup(eq + 1));
          gwy_debug("<%s>: <%s>\n", line, eq + 1);
        }
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}

#define createc_atof(x) g_ascii_strtod(x, NULL)

/* Macros to extract integer/double variables in hash_to_data_field() */
/* Any missing keyword/value pair is fatal, so we return a NULL pointer. */
#define HASH_GET(key, var, typeconv, err) \
    if (!(s = g_hash_table_lookup(hash, key))) { \
        g_set_error(err, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                    _("Missing `%s' field."), key); \
        return NULL; \
    } \
    var = typeconv(s)

/* Support for alternative keywords in some versions of dat files */
#define HASH_GET2(key1, key2, var, typeconv, err) \
    if (!(s = g_hash_table_lookup(hash, key1))) { \
      if (!(s = g_hash_table_lookup(hash, key2))) { \
          g_set_error(err, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA, \
                      _("Neither `%s' nor `%s' field found."), key1, key2); \
        return NULL; \
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
                   gchar *buffer,
                   GError **error)
{
    GwyDataField *dfield;
    GwySIUnit *unit;
    const gchar *s; /* for HASH_GET macros */
    gint xres, yres, bpp, offset;
    gdouble xreal, yreal, q;
    gboolean is_current;
    gdouble *data;
    gint ti1, ti2; /* temporary storage */
    gdouble td; /* temporary storage */

    bpp = 2; /* int16, always */
    offset = 16384 + 2; /* header + 2 offset bytes */
    is_current = FALSE;

    HASH_INT2("Num.X", "Num.X / Num.X", xres, error);
    HASH_INT2("Num.Y", "Num.Y / Num.Y", yres, error);

    HASH_INT2("Delta X", "Delta X / Delta X [Dac]", ti1, error);
    HASH_INT2("GainX", "GainX / GainX", ti2, error);
    HASH_DOUBLE("Xpiezoconst", td, error); /* lowcase p, why? */
    xreal = xres * ti1; /* dacs */
    xreal *= 20.0/65536.0 * ti2; /* voltage per dac */
    xreal *= td * 1.0e-10; /* piezoconstant [A/V] */

    HASH_INT2("Delta Y", "Delta Y / Delta Y [Dac]", ti1, error);
    HASH_INT2("GainY", "GainY / GainY", ti2, error);
    HASH_DOUBLE("YPiezoconst", td, error); /* upcase P */
    yreal = yres * ti1;
    yreal *= 20.0/65536.0 * ti2;
    yreal *= td * 1.0e-10;

    HASH_INT2("GainZ", "GainZ / GainZ", ti2, error);
    HASH_DOUBLE("ZPiezoconst", td, error); /* upcase P */
    q = 1.0; /* unity dac */
    q *= 20.0/65536.0 * ti2; /* voltage per dac */
    q *= td * 1.0e-10; /* piezoconstant [A/V] */

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    if (!read_binary_data(xres*yres, data, buffer + offset, bpp, error)) {
        g_object_unref(dfield);
        return NULL;
    }
    gwy_data_field_multiply(dfield, q);

    unit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, unit);
    g_object_unref(unit);

    unit = gwy_si_unit_new(is_current ? "A" : "m");
    gwy_data_field_set_si_unit_z(dfield, unit);
    g_object_unref(unit);

    return dfield;
}

/* Macro for storing meta data */

#define HASH_STORE(key) \
    if ((val = g_hash_table_lookup(hash, key))) { \
        g_string_printf(metakey, "/meta/%s", key); \
        gwy_debug("key = %s, val = %s\n", key, val); \
        gwy_container_set_string_by_name(data, metakey->str, g_strdup(val)); \
    }

static void
store_metadata(GwyContainer *data, GHashTable *hash)
{
    gchar *val;
    GString *metakey; /* for HASH_STORE macro */
    gchar *tobestored[] = {
        "Titel", "Titel / Titel",
        "Length x[A]",
        "Length y[A]",
        "Z-Res. [A]: +/- ",
        "BiasVoltage", "BiasVoltage / BiasVolt.[mV]",
        "Current[A]",
        "Delta X", "Delta X / Delta X [Dac]",
        "Delta Y", "Delta Y / Delta Y [Dac]",
        "Delay X+", "Delay X+ / Delay X+",
        "Delay X-", "Delay X- / Delay X-",
        "Delay Y", "Delay Y / Delay Y",
        "D-DeltaX", "D-DeltaX / D-DeltaX",
        "Rotation", "Rotation / Rotation",
        "GainX", "GainX / GainX",
        "GainY", "GainY / GainY",
        "GainZ", "GainZ / GainZ",
        "Gainpreamp", "Gainpreamp / GainPre 10^",
        "Chan(1,2,4)", "Chan(1,2,4) / Chan(1,2,4)",
        "Scancoarse", "Scancoarse / Scancoarse",
        "Scantype", "Scantype / Scantype",
        "FBIset",
        "FBLogIset",
        "FBRC",
        "FBLingain",
        "FBLog",
        "FBPropGain",
        "ZPiezoconst",
        "Xpiezoconst",
        "YPiezoconst",
        "Sec/line:",
        "Sec/Image:",
        "Channels", "Channels / Channels",
        "Dactonmx", "Dacto[A]xy",
        "Dactonmz", "Dacto[A]z",
        "memo:0",
        "memo:1",
        "memo:2",
        "T_ADC2[K]",
        "T_ADC3[K]",
        NULL
    };
    gint ctr = 0;

    metakey = g_string_new("");
    while (tobestored[ctr]) {
        HASH_STORE(tobestored[ctr]);
        ctr++;
    }
    g_string_free(metakey, TRUE);
}

static gboolean
read_binary_data(gint n, gdouble *data,
                 gchar *buffer,
                 gint bpp,
                 GError **error)
{
    gint i;
    gdouble q;

    q = 1.0/(1 << (8*bpp));
    switch (bpp) {
        case 1:
        for (i = 0; i < n; i++)
            data[i] = q*buffer[i];
        break;

        case 2:
        {
            gint16 *p = (gint16*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*GINT16_FROM_LE(p[i]);
        }
        break;

        case 4:
        {
            gint32 *p = (gint32*)buffer;

            for (i = 0; i < n; i++)
                data[i] = q*GINT32_FROM_LE(p[i]);
        }

        break;

        default:
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unimplemented number of bits per pixel: %d."), bpp);
        return FALSE;
        break;
    }

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
