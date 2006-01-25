/*
 *  $Id$
 *  Copyright (C) 2005 David Necas (Yeti), Petr Klstptek.
 *  E-mail: yeti@gwyddion.net, klstptek@gwyddion.net.
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

/*
 * TODO:
 * - skip Comments parts
 * - read split files (and accept any kind of EOL in magic)
 */

#include "config.h"
#include <glib/gprintf.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule.h>
#include <libprocess/datafield.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "err.h"

#define MAGIC "UK SOFT\r\n"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define DATA_MAGIC "Data_section  \r\n"
#define DATA_MAGIC_SIZE (sizeof(DATA_MAGIC) - 1)

#define EXTENSION ".stp"

#define KEY_LEN 14

#define Angstrom (1e-10)

typedef struct {
    gint id;
    gint xres;
    gint yres;
    const guint16 *data;
    GHashTable *meta;
} STPData;

typedef struct {
    guint n;
    STPData *buffers;
    GHashTable *meta;
} STPFile;

typedef struct {
    const gchar *key;
    const gchar *meta;
    const gchar *format;
} MetaDataFormat;

static gboolean      module_register       (const gchar *name);
static gint          stpfile_detect        (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer* stpfile_load          (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static guint         find_data_start       (const guchar *buffer,
                                            gsize size);
static void          stpfile_free          (STPFile *stpfile);
static void          read_data_field       (GwyDataField *dfield,
                                            STPData *stpdata);
static guint         file_read_header      (STPFile *stpfile,
                                            gchar *buffer);
static void          process_metadata      (STPFile *stpfile,
                                            guint id,
                                            GwyContainer *container,
                                            const gchar *container_key);

static const GwyEnum channels[] = {
    { N_("Topography"),                           1  },
    { N_("Current or Deflection or Amplitude"),   2  },
    { N_("Electrochemical voltage"),              8  },
    { N_("Topography and SPS"),                   9  },
    { N_("Topography and SPS scripted"),          10 },
    { N_("Electrochemical current"),              12 },
    { N_("Friction or Phase"),                    13 },
    { N_("AUX in BNC"),                           14 },
    { N_("External"),                             99 },
};

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Molecular Imaging STP data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.3.0",
    "David Nečas (Yeti), Petr Klapetek, Chris Anderson",
    "2006",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_file_func_register("stpfile",
                           N_("STP files (.stp)"),
                           (GwyFileDetectFunc)&stpfile_detect,
                           (GwyFileLoadFunc)&stpfile_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
stpfile_detect(const GwyFileDetectInfo *fileinfo,
               gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && !memcmp(fileinfo->buffer, MAGIC, MAGIC_SIZE))
        score = 100;

    return score;
}

static GwyContainer*
stpfile_load(const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    STPFile *stpfile;
    GwyContainer *container = NULL;
    gchar *container_key = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    guint header_size;
    gchar *p;
    gboolean ok;
    guint i = 0, pos;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (strncmp(buffer, MAGIC, MAGIC_SIZE)
        || size <= DATA_MAGIC_SIZE
        || !(header_size = find_data_start(buffer, size))) {
        err_FILE_TYPE(error, "STP");
        gwy_file_abandon_contents(buffer, size, &err);
        return NULL;
    }

    stpfile = g_new0(STPFile, 1);
    p = g_strndup(buffer, header_size);
    ok = file_read_header(stpfile, p);
    g_free(p);

    /* TODO: check size */
    if (ok) {
        pos = header_size;
        for (i = 0; i < stpfile->n; i++) {
            stpfile->buffers[i].data = (const guint16*)(buffer + pos);
            pos += 2*stpfile->buffers[i].xres * stpfile->buffers[i].yres;

            dfield = gwy_data_field_new(stpfile->buffers[i].xres,
                                        stpfile->buffers[i].yres,
                                        1.0, 1.0, FALSE);
            read_data_field(dfield, stpfile->buffers + i);

            if (!container)
                container = gwy_container_new();

            container_key = g_strdup_printf("/%i/data", i);
            gwy_container_set_object_by_name(container, container_key, dfield);
            g_object_unref(dfield);
            process_metadata(stpfile, i, container, container_key);
            g_free(container_key);
        }
    }

    gwy_file_abandon_contents(buffer, size, NULL);
    stpfile_free(stpfile);

    if (!container) {
        err_NO_DATA(error);
        return NULL;
    }

    return container;
}

static guint
find_data_start(const guchar *buffer,
                gsize size)
{
    const guchar *p;

    size -= DATA_MAGIC_SIZE;

    for (p = buffer;
         p && strncmp(p, DATA_MAGIC, DATA_MAGIC_SIZE);
         p = memchr(p+1, (DATA_MAGIC)[0], size - (p - buffer) - 1))
        ;

    return p ? (p - buffer) + DATA_MAGIC_SIZE : 0;
}

static void
stpfile_free(STPFile *stpfile)
{
    guint i;

    for (i = 0; i < stpfile->n; i++)
        g_hash_table_destroy(stpfile->buffers[i].meta);

    g_free(stpfile->buffers);
    g_hash_table_destroy(stpfile->meta);
    g_free(stpfile);
}

static guint
file_read_header(STPFile *stpfile,
                 gchar *buffer)
{
    STPData *data = NULL;
    GHashTable *meta;
    gchar *line, *key, *value = NULL;

    stpfile->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, g_free);
    meta = stpfile->meta;
    while ((line = gwy_str_next_line(&buffer))) {
        if (!strncmp(line, "buffer_id     ", KEY_LEN)) {
            gwy_debug("buffer id = %s", line + KEY_LEN);
            stpfile->n++;
            stpfile->buffers = g_renew(STPData, stpfile->buffers, stpfile->n);
            data = stpfile->buffers + (stpfile->n - 1);
            data->meta = g_hash_table_new_full(g_str_hash, g_str_equal,
                                               g_free, g_free);
            data->data = NULL;
            data->xres = data->yres = 0;
            data->id = atol(line + KEY_LEN);
            meta = data->meta;
        }
        if (!line[0] || line[0] == ' ')
            continue;

        key = g_strstrip(g_strndup(line, KEY_LEN));
        value = g_strstrip(g_strdup(line + KEY_LEN));
        g_hash_table_replace(meta, key, value);

        if (data) {
            if (gwy_strequal(key, "samples_x"))
                data->xres = atol(value);
            if (gwy_strequal(key, "samples_y"))
                data->yres = atol(value);
        }
    }

    return stpfile->n;
}

static gboolean
stpfile_get_double(GHashTable *meta,
                   const gchar *key,
                   gdouble *value)
{
    gchar *p, *end;
    gdouble r;

    p = g_hash_table_lookup(meta, key);
    if (!p)
        return FALSE;

    r = g_ascii_strtod(p, &end);
    if (end == p)
        return FALSE;

    *value = r;
    return TRUE;
}

static void
process_metadata(STPFile *stpfile,
                 guint id,
                 GwyContainer *container,
                 const gchar *container_key)
{
    static const MetaDataFormat global_metadata[] = {
        { "software", "Software", "%s" },
        { "op_mode", "Operation mode", "%s" },
        { "x_v_to_angs_0", "X calibration (linear)", "%s Å/V" },
        { "x_v_to_angs_1", "X calibration (quadratic)", "%s Å/V<sup>2</sup>" },
        { "y_v_to_angs_0", "Y calibration (linear)", "%s Å/V" },
        { "y_v_to_angs_1", "Y calibration (quadratic)", "%s Å/V<sup>2</sup>" },
        { "z_v_to_angs", "Z calibration", "%s Å/V" },
        { "servo_sense", "Servo sign", "%s" },
    };
    static const MetaDataFormat local_metadata[] = {
        { "i_servo_gain", "Integral servo gain", "%s" },
        { "p_servo_gain", "Proportional servo gain", "%s" },
        { "servo_range", "Servo range", "%s V" },
        { "tip_bias", "Force setpoint (tip bias)", "%s V" },
        { "line_freq", "Line frequency (requested)", "%s Hz" },
    };
    STPData *data;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble q, r;
    gchar *p, *s;
    const gchar *title;
    GString *key, *value;
    gint power10;
    guint mode, i;
    gchar *channel_key = NULL;

    dfield = GWY_DATA_FIELD(gwy_container_get_object_by_name(container,
                                                             container_key));
    data = stpfile->buffers + id;
    if ((p = g_hash_table_lookup(data->meta, "source_mode"))) {
        mode = atol(p);
        title = gwy_enum_to_string(mode, channels, G_N_ELEMENTS(channels));
        if (title) {
            channel_key = g_strdup_printf("%s/title", container_key);
            gwy_container_set_string_by_name(container, channel_key,
                                             g_strdup(title));
            g_free(channel_key);

            /* If this is the first channel, store the title under
               /filename/title as well for compatability with 1.x. */
            if (id == 0)
                gwy_container_set_string_by_name(container, "/filename/title",
                                                 g_strdup(title));
        }
    }
    else
        mode = 1;

    /* Fix value scale */
    switch (mode) {
        case 1:
        case 9:
        case 10:
        if (stpfile_get_double(stpfile->meta, "z_v_to_angs", &q)
            && stpfile_get_double(data->meta, "servo_range", &r)) {
            gwy_data_field_multiply(dfield, q*r/32768*Angstrom);
            gwy_debug("z_v_to_angs = %g, servo_range = %g", q, r);
        }
        else
            gwy_data_field_multiply(dfield, Angstrom/32768);
        break;

        case 2:    /* this channel is converted to nm by orig soft, but it
                      doesn't make much sense */
        case 8:
        case 12:
        case 13:
        case 14:
        gwy_data_field_add(dfield, -32768.0);
        gwy_data_field_multiply(dfield, 10.0/32768.0);
        siunit = gwy_si_unit_new("V");
        gwy_data_field_set_si_unit_z(dfield, siunit);
        g_object_unref(siunit);
        break;

        case 99:
        if (stpfile_get_double(data->meta, "ExSourceZFact", &q)) {
            s = g_hash_table_lookup(data->meta, "ExSourceZBip");
            if (s && gwy_strequal(s, "TRUE"))
                gwy_data_field_add(dfield, -32768);
            gwy_data_field_multiply(dfield, q);

            if ((s = g_hash_table_lookup(data->meta, "ExSourceZUnit"))) {
                siunit = GWY_SI_UNIT(gwy_si_unit_new_parse(s, &power10));
                gwy_data_field_set_si_unit_z(dfield, siunit);
                g_object_unref(siunit);
                gwy_data_field_multiply(dfield, pow10(power10));
            }
        }
        break;

        default:
        g_warning("Unknown mode %u", mode);
        gwy_data_field_multiply(dfield, Angstrom/32768);
        break;
    }

    /* Fix lateral scale */
    if (!stpfile_get_double(data->meta, "length_x", &r)) {
        g_warning("Missing or invalid x length");
        r = 1e4;
    }
    gwy_data_field_set_xreal(dfield, r*Angstrom);

    if (!stpfile_get_double(data->meta, "length_y", &r)) {
        g_warning("Missing or invalid y length");
        r = 1e4;
    }
    gwy_data_field_set_yreal(dfield, r*Angstrom);

    /* Metadata */
    key = g_string_new("/meta/");
    value = g_string_new("");

    /* Global */
    for (i = 0; i < G_N_ELEMENTS(global_metadata); i++) {
        if (!(p = g_hash_table_lookup(stpfile->meta, global_metadata[i].key)))
            continue;

        g_string_truncate(key, sizeof("/meta"));
        g_string_append(key, global_metadata[i].meta);
        g_string_printf(value, global_metadata[i].format, p);
        gwy_container_set_string_by_name(container, key->str,
                                         g_strdup(value->str));
    }

    /* Local */
    for (i = 0; i < G_N_ELEMENTS(local_metadata); i++) {
        if (!(p = g_hash_table_lookup(data->meta, local_metadata[i].key)))
            continue;

        g_string_truncate(key, sizeof("/meta"));
        g_string_append(key, local_metadata[i].meta);
        g_string_printf(value, local_metadata[i].format, p);
        gwy_container_set_string_by_name(container, key->str,
                                         g_strdup(value->str));
    }

    g_string_free(key, TRUE);
    g_string_free(value, TRUE);

    /* Special */
    if ((p = g_hash_table_lookup(data->meta, "Date"))
        && (s = g_hash_table_lookup(data->meta, "time")))
        gwy_container_set_string_by_name(container, "/meta/Date",
                                         g_strconcat(p, " ", s, NULL));
    if ((p = g_hash_table_lookup(data->meta, "scan_dir"))) {
        if (gwy_strequal(p, "0"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Scanning direction",
                                             g_strdup("Top to bottom"));
        else if (gwy_strequal(p, "1"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Scanning direction",
                                             g_strdup("Bottom to top"));
    }
    if ((p = g_hash_table_lookup(data->meta, "collect_mode"))) {
        if (gwy_strequal(p, "1"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Line direction",
                                             g_strdup("Left to right"));
        else if (gwy_strequal(p, "2"))
            gwy_container_set_string_by_name(container,
                                             "/meta/Line direction",
                                             g_strdup("Right to left"));
    }
}

static void
read_data_field(GwyDataField *dfield,
                STPData *stpdata)
{
    gdouble *data;
    const guint16 *row;
    gint i, j, xres, yres;

    xres = stpdata->xres;
    yres = stpdata->yres;
    gwy_data_field_resample(dfield, xres, yres, GWY_INTERPOLATION_NONE);
    data = gwy_data_field_get_data(dfield);
    for (i = 0; i < yres; i++) {
        row = stpdata->data + (yres-1 - i)*xres;
        for (j = 0; j < xres; j++)
            data[i*xres + j] = GUINT16_FROM_LE(row[j]);
    }
}

/* vim: set cin et ts=4 sw=4
cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

