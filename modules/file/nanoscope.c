/*
 *  @(#) $Id$
 *  Copyright (C) 2004 David Necas (Yeti), Petr Klapetek.
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

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule.h>

#include "err.h"

#define MAGIC_BIN "\\*File list\r\n"
#define MAGIC_TXT "?*File list\r\n"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

typedef enum {
    NANOSCOPE_FILE_TYPE_NONE = 0,
    NANOSCOPE_FILE_TYPE_BIN,
    NANOSCOPE_FILE_TYPE_TXT
} NanoscopeFileType;

typedef enum {
    NANOSCOPE_VALUE_OLD = 0,
    NANOSCOPE_VALUE_VALUE,
    NANOSCOPE_VALUE_SCALE,
    NANOSCOPE_VALUE_SELECT
} NanoscopeValueType;

/*
 * Old-style record is
 * \Foo: HardValue
 * New-style record is
 * \@Bar: V [SoftScale] (HardScale) HardValue
 * where SoftScale and HardScale are optional.
 */
typedef struct {
    NanoscopeValueType type;
    const gchar *soft_scale;
    gdouble hard_scale;
    const gchar *hard_scale_units;
    gdouble hard_value;
    const gchar *hard_value_str;
    const gchar *hard_value_units;
} NanoscopeValue;

typedef struct {
    GHashTable *hash;
    GwyDataField *data_field;
} NanoscopeData;

static gboolean        module_register     (const gchar *name);
static gint            nanoscope_detect    (const GwyFileDetectInfo *fileinfo,
                                            gboolean only_name);
static GwyContainer*   nanoscope_load      (const gchar *filename,
                                            GwyRunType mode,
                                            GError **error);
static GwyDataField*   hash_to_data_field  (GHashTable *hash,
                                            GHashTable *scannerlist,
                                            GHashTable *scanlist,
                                            NanoscopeFileType file_type,
                                            guint bufsize,
                                            gchar *buffer,
                                            gint gxres,
                                            gint gyres,
                                            gchar **p,
                                            GError **error);
static gboolean        read_ascii_data     (gint n,
                                            gdouble *data,
                                            gchar **buffer,
                                            gint bpp,
                                            GError **error);
static gboolean        read_binary_data    (gint n,
                                            gdouble *data,
                                            gchar *buffer,
                                            gint bpp,
                                            GError **error);
static GHashTable*     read_hash           (gchar **buffer,
                                            GError **error);

static void            get_scan_list_res   (GHashTable *hash,
                                            gint *xres,
                                            gint *yres);
static GwySIUnit*      get_physical_scale  (GHashTable *hash,
                                            GHashTable *scannerlist,
                                            GHashTable *scanlist,
                                            gdouble *scale,
                                            GError **error);
static void            fill_metadata       (GwyContainer *data,
                                            GHashTable *hash,
                                            GList *list);
static NanoscopeValue* parse_value         (const gchar *key,
                                            gchar *line);
static gboolean        require_keys        (GHashTable *hash,
                                            GError **error,
                                            ...);

/* The module info. */
static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Veeco Nanoscope data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.11",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

/* This is the ONLY exported symbol.  The argument is the module info.
 * NO semicolon after. */
GWY_MODULE_QUERY(module_info)

static gboolean
module_register(const gchar *name)
{
    gwy_file_func_register("nanoscope",
                           N_("Nanoscope files"),
                           (GwyFileDetectFunc)&nanoscope_detect,
                           (GwyFileLoadFunc)&nanoscope_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nanoscope_detect(const GwyFileDetectInfo *fileinfo,
                 gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && (memcmp(fileinfo->buffer, MAGIC_TXT, MAGIC_SIZE) == 0
            || memcmp(fileinfo->buffer, MAGIC_BIN, MAGIC_SIZE) == 0))
        score = 100;

    return score;
}

static GwyContainer*
nanoscope_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    GwyContainer *container = NULL;
    GError *err = NULL;
    gchar *buffer = NULL;
    gchar *p;
    const gchar *self;
    gsize size = 0;
    NanoscopeFileType file_type;
    NanoscopeData *ndata;
    NanoscopeValue *val;
    GHashTable *hash, *scannerlist = NULL, *scanlist = NULL;
    GList *l, *list = NULL;
    gint i, xres = 0, yres = 0;
    gboolean ok;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    file_type = NANOSCOPE_FILE_TYPE_NONE;
    if (size > MAGIC_SIZE) {
        if (!memcmp(buffer, MAGIC_TXT, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_TXT;
        else if (!memcmp(buffer, MAGIC_BIN, MAGIC_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_BIN;
    }
    if (!file_type) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is not a Nanoscope file, "
                      "or it is a unknown subtype."));
        g_free(buffer);
        return NULL;
    }
    gwy_debug("File type: %d", file_type);
    /* as already know file_type, fix the first char for hash reading */
    *buffer = '\\';

    p = buffer;
    while ((hash = read_hash(&p, &err))) {
        ndata = g_new0(NanoscopeData, 1);
        ndata->hash = hash;
        list = g_list_append(list, ndata);
    }
    if (err) {
        g_propagate_error(error, err);
        ok = FALSE;
    }
    else
        ok = TRUE;

    for (l = list; ok && l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        hash = ndata->hash;
        self = g_hash_table_lookup(hash, "#self");
        /* The alternate names were found in files written by some beast
         * called Nanoscope E software */
        if (gwy_strequal(self, "Scanner list")
            || gwy_strequal(self, "Microscope list")) {
            scannerlist = hash;
            continue;
        }
        if (gwy_strequal(self, "Ciao scan list")
            || gwy_strequal(self, "Afm list")) {
            get_scan_list_res(hash, &xres, &yres);
            scanlist = hash;
        }
        if (!gwy_strequal(self, "Ciao image list")
            && !gwy_strequal(self, "AFM image list"))
            continue;

        ndata->data_field = hash_to_data_field(hash, scannerlist, scanlist,
                                               file_type, size, buffer,
                                               xres, yres,
                                               &p, error);
        ok = ok && ndata->data_field;
    }

    if (ok) {
        gchar key[32];

        i = 0;
        container = gwy_container_new();
        for (l = list; l; l = g_list_next(l)) {
            ndata = (NanoscopeData*)l->data;
            if (ndata->data_field) {
                g_snprintf(key, sizeof(key), "/%d/data", i);
                gwy_container_set_object_by_name(container, key,
                                                 ndata->data_field);
                if ((val = g_hash_table_lookup(ndata->hash, "@2:Image Data"))
                    && val->soft_scale) {
                    g_snprintf(key, sizeof(key), "/%d/data/title", i);
                    gwy_container_set_string_by_name(container, key,
                                                     g_strdup(val->soft_scale));
                }
                i++;
            }
        }
        /* FIXME: which metadata to put where? */
        /* fill_metadata(container, ndata->hash, list); */
        if (!i)
            gwy_object_unref(container);
    }

    for (l = list; l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        gwy_object_unref(ndata->data_field);
        if (ndata->hash)
            g_hash_table_destroy(ndata->hash);
        g_free(ndata);
    }
    g_free(buffer);
    g_list_free(list);

    return container;
}

static void
get_scan_list_res(GHashTable *hash,
                  gint *xres, gint *yres)
{
    NanoscopeValue *val;

    /* XXX: Some observed files contained correct dimensions only in
     * a global section, sizes in `image list' sections were bogus.
     * Version: 0x05300001 */
    if ((val = g_hash_table_lookup(hash, "Samps/line")))
        *xres = ROUND(val->hard_value);
    if ((val = g_hash_table_lookup(hash, "Lines")))
        *yres = ROUND(val->hard_value);
    gwy_debug("Global xres, yres = %d, %d", *xres, *yres);
}

static void
add_metadata(gpointer hkey,
             gpointer hvalue,
             gpointer user_data)
{
    static gchar buffer[256];
    gchar *key = (gchar*)hkey;
    NanoscopeValue *val = (NanoscopeValue*)hvalue;
    gchar *s, *v, *w;

    if (gwy_strequal(key, "#self")
        || !val->hard_value_str
        || !val->hard_value_str[0])
        return;

    if (key[0] == '@')
        key++;
    /* FIXME: naughty /-avoiding trick */
    s = gwy_strreplace(key, "/", "∕", (guint)-1);
    g_snprintf(buffer, sizeof(buffer), "/meta/%s", s);
    v = g_strdup(val->hard_value_str);
    if (strchr(v, '\272')) {
        w = gwy_strreplace(v, "\272", "deg", -1);
        g_free(v);
        v = w;
    }
    if (strchr(v, '~')) {
        w = gwy_strreplace(v, "~", "µ", -1);
        g_free(v);
        v = w;
    }
    gwy_container_set_string_by_name(GWY_CONTAINER(user_data), buffer, v);
    g_free(s);
}

static void
fill_metadata(GwyContainer *data,
              GHashTable *hash,
              GList *list)
{
    static const gchar *hashes[] = {
        "File list", "Scanner list", "Equipment list", "Ciao scan list",
    };
    GList *l;
    guint i;

    for (l = list; l; l = g_list_next(l)) {
        GHashTable *h = ((NanoscopeData*)l->data)->hash;
        for (i = 0; i < G_N_ELEMENTS(hashes); i++) {
            if (gwy_strequal(g_hash_table_lookup(h, "#self"), hashes[i])) {
                g_hash_table_foreach(h, add_metadata, data);
                break;
            }
        }
    }
    g_hash_table_foreach(hash, add_metadata, data);
}

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   GHashTable *scannerlist,
                   GHashTable *scanlist,
                   NanoscopeFileType file_type,
                   guint bufsize,
                   gchar *buffer,
                   gint gxres,
                   gint gyres,
                   gchar **p,
                   GError **error)
{
    NanoscopeValue *val;
    GwyDataField *dfield;
    GwySIUnit *unitz, *unitxy;
    gchar *s, *end;
    gchar un[5];
    gint xres, yres, bpp, offset, size, power10;
    gdouble xreal, yreal, q;
    gdouble *data;

    if (!require_keys(hash, error, "Samps/line", "Number of lines",
                      "Scan size", "Data offset", "Data length", NULL))
        return NULL;

    val = g_hash_table_lookup(hash, "Samps/line");
    xres = ROUND(val->hard_value);

    val = g_hash_table_lookup(hash, "Number of lines");
    yres = ROUND(val->hard_value);

    val = g_hash_table_lookup(hash, "Bytes/pixel");
    bpp = val ? ROUND(val->hard_value) : 2;

    /* scan size */
    val = g_hash_table_lookup(hash, "Scan size");
    xreal = g_ascii_strtod(val->hard_value_str, &end);
    if (errno || *end != ' ') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot parse `Scan size' field."));
        return NULL;
    }
    s = end+1;
    yreal = g_ascii_strtod(s, &end);
    if (errno || *end != ' ') {
        /* Nanoscope E files don't have two numbers here */
        yreal = xreal;
        end = s;
        /*g_warning("Cannot parse <Scan size>: <%s>", s);
        return NULL;*/
    }
    if (sscanf(end+1, "%4s", un) != 1) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot parse `Scan size' field."));
        return NULL;
    }
    unitxy = gwy_si_unit_new_parse(un, &power10);
    q = pow10(power10);
    xreal *= q;
    yreal *= q;

    offset = size = 0;
    if (file_type == NANOSCOPE_FILE_TYPE_BIN) {
        val = g_hash_table_lookup(hash, "Data offset");
        offset = ROUND(val->hard_value);

        val = g_hash_table_lookup(hash, "Data length");
        size = ROUND(val->hard_value);
        if (size != bpp*xres*yres) {
            /* Keep square pixels */
            if (gxres) {
                xreal *= (gdouble)gxres/xres;
                xres = gxres;
            }
            if (gyres) {
                yreal *= (gdouble)gyres/yres;
                yres = gyres;
            }
            if (size != bpp*xres*yres) {
                err_SIZE_MISMATCH(error, size, bpp*xres*yres);
                return NULL;
            }
        }

        if (offset + size > (gint)bufsize) {
            err_SIZE_MISMATCH(error, offset + size, bufsize);
            return NULL;
        }
    }

    q = 1.0;
    unitz = get_physical_scale(hash, scannerlist, scanlist, &q, error);
    if (!unitz)
        return NULL;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    switch (file_type) {
        case NANOSCOPE_FILE_TYPE_TXT:
        if (!read_ascii_data(xres*yres, data, p, bpp, error)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        case NANOSCOPE_FILE_TYPE_BIN:
        if (!read_binary_data(xres*yres, data, buffer + offset, bpp, error)) {
            g_object_unref(dfield);
            return NULL;
        }
        break;

        default:
        g_assert_not_reached();
        break;
    }
    gwy_data_field_multiply(dfield, q);
    gwy_data_field_invert(dfield, TRUE, FALSE, FALSE);
    gwy_data_field_set_si_unit_z(dfield, unitz);
    g_object_unref(unitz);

    gwy_data_field_set_si_unit_xy(dfield, unitxy);
    g_object_unref(unitxy);

    return dfield;
}

static GwySIUnit*
get_physical_scale(GHashTable *hash,
                   GHashTable *scannerlist,
                   GHashTable *scanlist,
                   gdouble *scale,
                   GError **error)
{
    GwySIUnit *siunit, *siunit2;
    NanoscopeValue *val, *sval;
    gchar *key;
    gint q;

    /* XXX: This is a damned heuristics.  For some value types we try to guess
     * a different quantity scale to look up. */
    if (!(val = g_hash_table_lookup(hash, "@2:Z scale"))) {
        err_MISSING_FIELD(error, "@2:Z scale");
        return NULL;
    }
    key = g_strdup_printf("@%s", val->soft_scale);

    if (!(sval = g_hash_table_lookup(scannerlist, key))
        && (!scanlist || !(sval = g_hash_table_lookup(scanlist, key)))) {
        g_warning("`%s' not found", key);
        g_free(key);
        /* XXX */
        *scale = val->hard_value;
        return gwy_si_unit_new("");
    }

    *scale = val->hard_value*sval->hard_value;

    if (!sval->hard_value_units || !*sval->hard_value_units) {
        if (gwy_strequal(val->soft_scale, "Sens. Phase"))
            siunit = gwy_si_unit_new("deg");
        else
            siunit = gwy_si_unit_new("V");
    }
    else {
        siunit = gwy_si_unit_new_parse(sval->hard_value_units, &q);
        siunit2 = gwy_si_unit_new("V");
        gwy_si_unit_multiply(siunit, siunit2, siunit);
        gwy_debug("Scale1 = %g V/LSB", val->hard_value);
        gwy_debug("Scale2 = %g %s", sval->hard_value, sval->hard_value_units);
        *scale *= pow10(q);
        gwy_debug("Total scale = %g %s/LSB",
                  *scale, gwy_si_unit_get_unit_string(siunit));
        g_object_unref(siunit2);
    }
    g_free(key);

    return siunit;
}

static gboolean
read_ascii_data(gint n, gdouble *data,
                gchar **buffer,
                gint bpp,
                GError **error)
{
    gint i;
    gdouble q;
    gchar *end;
    long l, min, max;

    q = 1.0/(1 << (8*bpp));
    min = 10000000;
    max = -10000000;
    for (i = 0; i < n; i++) {
        /*data[i] = q*strtol(*buffer, &end, 10);*/
        l = strtol(*buffer, &end, 10);
        min = MIN(l, min);
        max = MAX(l, max);
        data[i] = q*l;
        if (end == *buffer) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Garbage after data sample #%d."), i);
            return FALSE;
        }
        *buffer = end;
    }
    gwy_debug("min = %ld, max = %ld", min, max);
    return TRUE;
}

/* FIXME: We hope Nanoscope files always use little endian, because we only
 * have seen them on Intel. */
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
        err_BPP(error, bpp);
        return FALSE;
        break;
    }

    return TRUE;
}

static GHashTable*
read_hash(gchar **buffer,
          GError **error)
{
    GHashTable *hash;
    NanoscopeValue *value;
    gchar *line, *colon;

    line = gwy_str_next_line(buffer);
    if (line[0] != '\\' || line[1] != '*')
        return NULL;
    if (gwy_strequal(line, "\\*File list end")) {
        gwy_debug("FILE LIST END");
        return NULL;
    }

    hash = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hash, "#self", line + 2);    /* self */
    gwy_debug("hash table <%s>", line + 2);
    while ((*buffer)[0] == '\\' && (*buffer)[1] && (*buffer)[1] != '*') {
        line = gwy_str_next_line(buffer) + 1;
        if (!line || !line[0] || !line[1] || !line[2]) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Truncated header line."));
            goto fail;
        }
        colon = line;
        if (line[0] == '@' && g_ascii_isdigit(line[1]) && line[2] == ':')
            colon = line+3;
        colon = strchr(colon, ':');
        if (!colon || !g_ascii_isspace(colon[1])) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("Missing colon in header line."));
            goto fail;
        }
        *colon = '\0';
        do {
            colon++;
        } while (g_ascii_isspace(*colon));
        value = parse_value(line, colon);
        if (value)
            g_hash_table_insert(hash, line, value);
    }

    /* Fix random stuff in Nanoscope E files */
    if ((value = g_hash_table_lookup(hash, "Samps/line"))
        && !g_hash_table_lookup(hash, "Number of lines")
        && value->hard_value_units
        && g_ascii_isdigit(value->hard_value_units[0])) {
        NanoscopeValue *val;

        val = g_new0(NanoscopeValue, 1);
        val->hard_value = g_ascii_strtod(value->hard_value_units, NULL);
        val->hard_value_str = value->hard_value_units;
        g_hash_table_insert(hash, "Number of lines", val);
    }

    return hash;

fail:
    g_hash_table_destroy(hash);
    return NULL;
}

/* General parameter line parser */
static NanoscopeValue*
parse_value(const gchar *key, gchar *line)
{
    NanoscopeValue *val;
    gchar *p, *q;

    val = g_new0(NanoscopeValue, 1);

    /* old-style values */
    if (key[0] != '@') {
        val->hard_value = g_ascii_strtod(line, &p);
        if (p-line > 0 && *p == ' ' && !strchr(p+1, ' ')) {
            do {
                p++;
            } while (g_ascii_isspace(*p));
            val->hard_value_units = p;
        }
        val->hard_value_str = line;
        return val;
    }

    /* type */
    switch (line[0]) {
        case 'V':
        val->type = NANOSCOPE_VALUE_VALUE;
        break;

        case 'S':
        val->type = NANOSCOPE_VALUE_SELECT;
        break;

        case 'C':
        val->type = NANOSCOPE_VALUE_SCALE;
        break;

        default:
        g_warning("Cannot parse value type <%s> for key <%s>", line, key);
        g_free(val);
        return NULL;
        break;
    }

    line++;
    if (line[0] != ' ') {
        g_warning("Cannot parse value type <%s> for key <%s>", line, key);
        g_free(val);
        return NULL;
    }
    do {
        line++;
    } while (g_ascii_isspace(*line));

    /* softscale */
    if (line[0] == '[') {
        if (!(p = strchr(line, ']'))) {
            g_warning("Cannot parse soft scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        if (p-line-1 > 0) {
            *p = '\0';
            val->soft_scale = line+1;
        }
        line = p+1;
        if (line[0] != ' ') {
            g_warning("Cannot parse soft scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        do {
            line++;
        } while (g_ascii_isspace(*line));
    }

    /* hardscale (probably useless) */
    if (line[0] == '(') {
        do {
            line++;
        } while (g_ascii_isspace(*line));
        if (!(p = strchr(line, ')'))) {
            g_warning("Cannot parse hard scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        val->hard_scale = g_ascii_strtod(line, &q);
        while (g_ascii_isspace(*q))
            q++;
        if (p-q > 0) {
            *p = '\0';
            val->hard_scale_units = q;
        }
        line = p+1;
        if (line[0] != ' ') {
            g_warning("Cannot parse hard scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        do {
            line++;
        } while (g_ascii_isspace(*line));
    }

    /* hard value (everything else) */
    switch (val->type) {
        case NANOSCOPE_VALUE_SELECT:
        val->hard_value_str = line;
        break;

        case NANOSCOPE_VALUE_SCALE:
        val->hard_value = g_ascii_strtod(line, &p);
        break;

        case NANOSCOPE_VALUE_VALUE:
        val->hard_value = g_ascii_strtod(line, &p);
        if (p-line > 0 && *p == ' ' && !strchr(p+1, ' ')) {
            do {
                p++;
            } while (g_ascii_isspace(*p));
            val->hard_value_units = p;
        }
        val->hard_value_str = line;
        break;

        default:
        g_assert_not_reached();
        break;
    }

    return val;
}

static gboolean
require_keys(GHashTable *hash,
             GError **error,
             ...)
{
    va_list ap;
    const gchar *key;

    va_start(ap, error);
    while ((key = va_arg(ap, const gchar *))) {
        if (!g_hash_table_lookup(hash, key)) {
            err_MISSING_FIELD(error, key);
            va_end(ap);
            return FALSE;
        }
    }
    va_end(ap);

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

