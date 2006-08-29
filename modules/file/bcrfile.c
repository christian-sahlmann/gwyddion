/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klbcrtek.
 *  E-mail: yeti@gwyddion.net, klbcrtek@gwyddion.net.
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
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/stats.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "err.h"
#include "get.h"

#define HEADER_SIZE 2048

#define MAGIC1 "fileformat = bcrstm\n"
#define MAGIC_SIZE1 (sizeof(MAGIC1) - 1)
#define MAGIC2 "fileformat = bcrf\n"
#define MAGIC_SIZE2 (sizeof(MAGIC2) - 1)
#define MAGIC_SIZE (MAX(MAGIC_SIZE1, MAGIC_SIZE2))

/* values are bytes per pixel */
typedef enum {
    BCR_FILE_INT16 = 2,
    BCR_FILE_FLOAT = 4
} BCRFileType;

static gboolean      module_register           (void);
static gint          bcrfile_detect          (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* bcrfile_load              (const gchar *filename,
                                                GwyRunType mode,
                                                GError **error);
static GwyDataField* file_load_real            (const guchar *buffer,
                                                gsize size,
                                                GHashTable *meta,
                                                GwyDataField **voidmask,
                                                GError **error);
static GwyDataField* read_data_field           (const guchar *buffer,
                                                gint xres,
                                                gint yres,
                                                BCRFileType type,
                                                gboolean little_endian);
static GwyDataField* read_data_field_with_voids(const guchar *buffer,
                                                gint xres,
                                                gint yres,
                                                BCRFileType type,
                                                gboolean little_endian,
                                                GwyDataField **voidmask);
static void          load_metadata             (gchar *buffer,
                                                GHashTable *meta);
static void          store_metadata            (GHashTable *meta,
                                                GwyContainer *container);
static void          guess_channel_type        (GwyContainer *data,
                                                const gchar *key);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Image Metrology BCR data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.7",
    "David Nečas (Yeti) & Petr Klapetek",
    "2005",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("bcrfile",
                           N_("BCR files (.bcr, .bcrf)"),
                           (GwyFileDetectFunc)&bcrfile_detect,
                           (GwyFileLoadFunc)&bcrfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
bcrfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return (g_str_has_suffix(fileinfo->name_lowercase, ".bcr")
                || g_str_has_suffix(fileinfo->name_lowercase, ".bcrf"))
                ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && (!memcmp(fileinfo->head, MAGIC1, MAGIC_SIZE1)
            || !memcmp(fileinfo->head, MAGIC2, MAGIC_SIZE2)))
        score = 100;

    return score;
}

static GwyContainer*
bcrfile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL, *voidmask = NULL;
    GHashTable *meta = NULL;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < HEADER_SIZE) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    meta = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    dfield = file_load_real(buffer, size, meta, &voidmask, error);
    gwy_file_abandon_contents(buffer, size, NULL);
    if (dfield) {
        container = gwy_container_new();
        gwy_container_set_object_by_name(container, "/0/data", dfield);
        g_object_unref(dfield);
        guess_channel_type(container, "/0/data");
        if (voidmask) {
            gwy_container_set_object_by_name(container, "/0/mask", voidmask);
            g_object_unref(voidmask);
        }
        store_metadata(meta, container);
    }
    g_hash_table_destroy(meta);

    return container;
}

static GwyDataField*
file_load_real(const guchar *buffer,
               gsize size,
               GHashTable *meta,
               GwyDataField **voidmask,
               GError **error)
{
    GwyDataField *dfield;
    GwySIUnit *siunit1 = NULL, *siunit2 = NULL;
    gboolean intelmode = TRUE, voidpixels = FALSE;
    BCRFileType type;
    gdouble q, qq;
    gint xres, yres, power10;
    guchar *s;

    s = g_memdup(buffer, HEADER_SIZE);
    s[HEADER_SIZE-1] = '\0';
    load_metadata(s, meta);
    g_free(s);

    if (!(s = g_hash_table_lookup(meta, "fileformat"))) {
        err_FILE_TYPE(error, "BCR/BCFR");
        return NULL;
    }

    if (gwy_strequal(s, "bcrstm"))
        type = BCR_FILE_INT16;
    else if (gwy_strequal(s, "bcrf"))
        type = BCR_FILE_FLOAT;
    else {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Unknown file type header: `%s'."), s);
        return NULL;
    }
    gwy_debug("File type: %u", type);

    if (!(s = g_hash_table_lookup(meta, "xpixels"))) {
        err_MISSING_FIELD(error, "xpixels");
        return NULL;
    }
    xres = atol(s);

    if (!(s = g_hash_table_lookup(meta, "ypixels"))) {
        err_MISSING_FIELD(error, "ypixels");
        return NULL;
    }
    yres = atol(s);

    if ((s = g_hash_table_lookup(meta, "intelmode")))
        intelmode = !!atol(s);

    /* This is in fact an int, but we only care whether it's nonzero */
    if ((s = g_hash_table_lookup(meta, "voidpixels")))
        voidpixels = !!atol(s);

    if (size < HEADER_SIZE + xres*yres*type) {
        err_SIZE_MISMATCH(error, xres*yres*type, (guint)(size - HEADER_SIZE));
        return NULL;
    }

    if (voidpixels)
        dfield = read_data_field_with_voids(buffer + HEADER_SIZE, xres, yres,
                                            type, intelmode, voidmask);
    else
        dfield = read_data_field(buffer + HEADER_SIZE, xres, yres,
                                 type, intelmode);

    if ((s = g_hash_table_lookup(meta, "xlength"))
        && (q = g_ascii_strtod(s, NULL)) > 0) {
        if (!(s = g_hash_table_lookup(meta, "xunit")))
            s = "nm";

        siunit1 = gwy_si_unit_new_parse(s, &power10);
        q *= pow10(power10);
        gwy_data_field_set_si_unit_xy(dfield, siunit1);
        gwy_data_field_set_xreal(dfield, q);
    }

    if ((s = g_hash_table_lookup(meta, "ylength"))
        && (q = g_ascii_strtod(s, NULL)) > 0) {
        if (!(s = g_hash_table_lookup(meta, "yunit")))
            s = "nm";

        siunit2 = gwy_si_unit_new_parse(s, &power10);
        q *= pow10(power10);
        if (siunit1 && !gwy_si_unit_equal(siunit1, siunit2))
            g_warning("Incompatible x and y units");
        g_object_unref(siunit2);
        gwy_data_field_set_yreal(dfield, q);
    }
    gwy_object_unref(siunit1);

    if (!(s = g_hash_table_lookup(meta, "zunit")))
        s = "nm";
    siunit1 = gwy_si_unit_new_parse(s, &power10);
    gwy_data_field_set_si_unit_z(dfield, siunit1);
    g_object_unref(siunit1);
    q = pow10(power10);

    if (type == BCR_FILE_INT16
        && (s = g_hash_table_lookup(meta, "bit2nm"))
        && (qq = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_multiply(dfield, q*qq);
    /*
    else
       FIXME: ignoring powers of 10 is the right thing when zunit is unset,
       but otherwise?
     */

    if ((s = g_hash_table_lookup(meta, "zmin"))
        && (qq = g_ascii_strtod(s, NULL)) > 0)
        gwy_data_field_add(dfield, q*qq - gwy_data_field_get_min(dfield));

    if (voidpixels) {
        gwy_data_field_set_xreal(*voidmask, gwy_data_field_get_xreal(dfield));
        gwy_data_field_set_yreal(*voidmask, gwy_data_field_get_yreal(dfield));
        siunit1 = gwy_si_unit_duplicate(gwy_data_field_get_si_unit_xy(dfield));
        gwy_data_field_set_si_unit_xy(*voidmask, siunit1);
        g_object_unref(siunit1);
        siunit1 = gwy_si_unit_new(NULL);
        gwy_data_field_set_si_unit_z(*voidmask, siunit1);
        g_object_unref(siunit1);
    }

    return dfield;
}

static void
store_metadata(GHashTable *meta,
               GwyContainer *container)
{
    const struct {
        const gchar *id;
        const gchar *unit;
        const gchar *key;
    }
    metakeys[] = {
        { "scanspeed",   "nm/s",    "Scan speed"        },
        { "xoffset",     "nm",      "X offset"          },
        { "yoffset",     "nm",      "Y offset"          },
        { "bias",        "V",       "Bias voltage"      },
        { "current",     "nA",      "Tunneling current" },
        { "starttime",   NULL,      "Scan time"         },
        /* FIXME: I've seen other stuff, but don't know interpretation */
    };
    gchar *value;
    GString *key;
    guint i;

    key = g_string_new("");
    for (i = 0; i < G_N_ELEMENTS(metakeys); i++) {
        if (!(value = g_hash_table_lookup(meta, metakeys[i].id)))
            continue;

        g_string_printf(key, "/meta/%s", metakeys[i].key);
        if (metakeys[i].unit)
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup_printf("%s %s",
                                                             value,
                                                             metakeys[i].unit));
        else
            gwy_container_set_string_by_name(container, key->str,
                                             g_strdup(value));
    }
    g_string_free(key, TRUE);
}

/**
 * guess_channel_type:
 * @data: A data container.
 * @key: Data channel key.
 *
 * Adds a channel title based on data field units.
 *
 * The guess is very simple, but probably better than `Unknown channel' in
 * most cases.  If there already is a title it is left intact, making use of
 * this function as a fallback easier.
 **/
static void
guess_channel_type(GwyContainer *data,
                   const gchar *key)
{
    GwySIUnit *siunit, *test;
    GwyDataField *dfield;
    const gchar *title;
    GQuark quark;
    gchar *s;

    s = g_strconcat(key, "/title", NULL);
    quark = g_quark_from_string(s);
    g_free(s);
    if (gwy_container_contains(data, quark))
        return;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(data, key));
    g_return_if_fail(GWY_IS_DATA_FIELD(dfield));
    siunit = gwy_data_field_get_si_unit_z(dfield);
    test = gwy_si_unit_new(NULL);
    title = NULL;

    if (!title) {
        gwy_si_unit_set_from_string(test, "m");
        if (gwy_si_unit_equal(siunit, test))
            title = "Topography";
    }
    if (!title) {
        gwy_si_unit_set_from_string(test, "A");
        if (gwy_si_unit_equal(siunit, test))
            title = "Current";
    }
    if (!title) {
        gwy_si_unit_set_from_string(test, "deg");
        if (gwy_si_unit_equal(siunit, test))
            title = "Phase";
    }

    g_object_unref(test);
    if (title)
        gwy_container_set_string(data, quark, g_strdup(title));
}

static GwyDataField*
read_data_field(const guchar *buffer,
                gint xres,
                gint yres,
                BCRFileType type,
                gboolean little_endian)
{
    const guint16 *p = (const guint16*)buffer;
    GwyDataField *dfield;
    gdouble *data;
    guint i;

    dfield = gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE);
    data = gwy_data_field_get_data(dfield);
    switch (type) {
        case BCR_FILE_INT16:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_LE(p[i]);
        }
        else {
            for (i = 0; i < xres*yres; i++)
                data[i] = GINT16_FROM_BE(p[i]);
        }
        break;

        case BCR_FILE_FLOAT:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT_LE(&buffer);
        }
        else {
            for (i = 0; i < xres*yres; i++)
                data[i] = get_FLOAT_BE(&buffer);
        }
        gwy_data_field_multiply(dfield, 1e-9);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return dfield;
}

static GwyDataField*
read_data_field_with_voids(const guchar *buffer,
                           gint xres,
                           gint yres,
                           BCRFileType type,
                           gboolean little_endian,
                           GwyDataField **voidmask)
{
    const guint16 *p = (const guint16*)buffer;
    GwyDataField *dfield;
    gdouble *data, *voids;
    gdouble sum;
    guint i, ngood;

    dfield = gwy_data_field_new(xres, yres, 1e-6, 1e-6, FALSE);
    *voidmask = gwy_data_field_new(xres, yres, 1e-6, 1e-6, TRUE);
    data = gwy_data_field_get_data(dfield);
    voids = gwy_data_field_get_data(*voidmask);
    ngood = 0;
    sum = 0.0;
    switch (type) {
        case BCR_FILE_INT16:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++) {
                data[i] = GINT16_FROM_LE(p[i]);
                if (data[i] == 32767.0)
                    voids[i] = 1.0;
                else {
                    ngood++;
                    sum += data[i];
                }
            }
        }
        else {
            for (i = 0; i < xres*yres; i++) {
                data[i] = GINT16_FROM_BE(p[i]);
                if (data[i] == 32767.0)
                    voids[i] = 1.0;
                else {
                    ngood++;
                    sum += data[i];
                }
            }
        }
        break;

        case BCR_FILE_FLOAT:
        if (little_endian) {
            for (i = 0; i < xres*yres; i++) {
                data[i] = get_FLOAT_LE(&buffer);
                if (data[i] > 1.7e38)
                    voids[i] = 1.0;
                else {
                    ngood++;
                    sum += data[i];
                }
            }
        }
        else {
            for (i = 0; i < xres*yres; i++) {
                data[i] = get_FLOAT_BE(&buffer);
                if (data[i] > 1.7e38)
                    voids[i] = 1.0;
                else {
                    ngood++;
                    sum += data[i];
                }
            }
        }
        gwy_data_field_multiply(dfield, 1e-9);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    if (!ngood || ngood == xres*yres) {
        if (!ngood) {
            g_warning("Data contain no valid pixels.");
            gwy_data_field_clear(dfield);
        }
        gwy_object_unref(*voidmask);
        return dfield;
    }

    /* Replace void data with mean value */
    sum /= ngood;
    for (i = 0; i < xres*yres; i++) {
        if (voids[i])
            data[i] = sum;
    }

    return dfield;
}

static void
load_metadata(gchar *buffer,
              GHashTable *meta)
{
    gchar *line, *p;
    gchar *key, *value;

    while ((line = gwy_str_next_line(&buffer))) {
        if (line[0] == '%' || line[0] == '#')
            continue;

        p = strchr(line, '=');
        if (!p || p == line || !p[1])
            continue;

        key = g_strstrip(g_strndup(line, p-line));
        if (!key[0]) {
            g_free(key);
            continue;
        }
        value = g_strstrip(g_strdup(p+1));
        if (!value[0]) {
            g_free(key);
            g_free(value);
            continue;
        }

        g_hash_table_insert(meta, key, value);
        gwy_debug("<%s> = <%s>", key, value);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

