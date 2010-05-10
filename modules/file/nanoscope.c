/*
 *  @(#) $Id$
 *  Copyright (C) 2004-2007 David Necas (Yeti), Petr Klapetek.
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
 * <mime-type type="application/x-nanoscope-iii-spm">
 *   <comment>Nanoscope III SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="\\*File list\r\n"/>
 *     <match type="string" offset="0" value="?*File list\r\n"/>
 *   </magic>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Veeco Nanoscope III
 * .001 .002 etc.
 * Read
 **/
#define DEBUG 1
#include "config.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwydgets/gwygraphmodel.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC_BIN "\\*File list\r\n"
#define MAGIC_TXT "?*File list\r\n"
#define MAGIC_SIZE (sizeof(MAGIC_TXT)-1)

#define MAGIC_BIN_PARTIAL "\\*File list"
#define MAGIC_TXT_PARTIAL "?*File list"
#define MAGIC_SIZE_PARTIAL (sizeof(MAGIC_TXT_PARTIAL)-1)

#define MAGIC_FORCE_BIN "\\*Force file list\r\n"
#define MAGIC_FORCE_SIZE (sizeof(MAGIC_FORCE_BIN)-1)

typedef enum {
    NANOSCOPE_FILE_TYPE_NONE = 0,
    NANOSCOPE_FILE_TYPE_BIN,
    NANOSCOPE_FILE_TYPE_TXT,
    NANOSCOPE_FILE_TYPE_FORCE_BIN,
    NANOSCOPE_FILE_TYPE_BROKEN
} NanoscopeFileType;

typedef enum {
    NANOSCOPE_VALUE_OLD = 0,
    NANOSCOPE_VALUE_VALUE,
    NANOSCOPE_VALUE_SCALE,
    NANOSCOPE_VALUE_SELECT
} NanoscopeValueType;

/*
 * Old-style record is
 * \Foo: HardValue (HardScale)
 * where HardScale is optional.
 *
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
    GwyGraphModel *graph_model;
} NanoscopeData;

static gboolean        module_register       (void);
static gint            nanoscope_detect      (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer*   nanoscope_load        (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static GwyDataField*   hash_to_data_field    (GHashTable *hash,
                                              GHashTable *scannerlist,
                                              GHashTable *scanlist,
                                              NanoscopeFileType file_type,
                                              gboolean has_version,
                                              guint bufsize,
                                              gchar *buffer,
                                              gint gxres,
                                              gint gyres,
                                              gchar **p,
                                              GError **error);
static GwyGraphModel*  hash_to_curve         (GHashTable *hash,
                                              GHashTable *scannerlist,
                                              GHashTable *scanlist,
                                              NanoscopeFileType file_type,
                                              guint bufsize,
                                              gchar *buffer,
                                              gint gxres,
                                              GError **error);
static gboolean        read_text_data        (gint n,
                                              gdouble *data,
                                              gchar **buffer,
                                              gint bpp,
                                              GError **error);
static gboolean        read_binary_data      (gint n,
                                              gdouble *data,
                                              gchar *buffer,
                                              gint bpp,
                                              GError **error);
static GHashTable*     read_hash             (gchar **buffer,
                                              GError **error);
static void            get_scan_list_res     (GHashTable *hash,
                                              gint *xres,
                                              gint *yres);
static GwySIUnit*      get_physical_scale    (GHashTable *hash,
                                              GHashTable *scannerlist,
                                              GHashTable *scanlist,
                                              gboolean has_version,
                                              gdouble *scale,
                                              GError **error);
static GwyContainer*   nanoscope_get_metadata(GHashTable *hash,
                                              GList *list);
static NanoscopeValue* parse_value           (const gchar *key,
                                              gchar *line);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Veeco (Digital Instruments) Nanoscope data files, "
       "version 3 or newer."),
    "Yeti <yeti@gwyddion.net>",
    "0.25",
    "David Nečas (Yeti) & Petr Klapetek",
    "2004",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanoscope",
                           N_("Nanoscope III files"),
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
        && (!memcmp(fileinfo->head, MAGIC_TXT_PARTIAL, MAGIC_SIZE_PARTIAL)
            || !memcmp(fileinfo->head, MAGIC_BIN_PARTIAL, MAGIC_SIZE_PARTIAL)
            || !memcmp(fileinfo->head, MAGIC_FORCE_BIN, MAGIC_FORCE_SIZE)))
        score = 100;

    return score;
}

static GwyContainer*
nanoscope_load(const gchar *filename,
               G_GNUC_UNUSED GwyRunType mode,
               GError **error)
{
    GwyContainer *meta, *container = NULL;
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
    gboolean ok, has_version = FALSE;

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
        else if (!memcmp(buffer, MAGIC_FORCE_BIN, MAGIC_FORCE_SIZE))
            file_type = NANOSCOPE_FILE_TYPE_FORCE_BIN;
        else if (!memcmp(buffer, MAGIC_TXT_PARTIAL, MAGIC_SIZE_PARTIAL)
                 || !memcmp(buffer, MAGIC_BIN_PARTIAL, MAGIC_SIZE_PARTIAL))
          file_type = NANOSCOPE_FILE_TYPE_BROKEN;
    }
    if (!file_type) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File is not a Nanoscope file, "
                      "or it is a unknown subtype."));
        g_free(buffer);
        return NULL;
    }
    if (file_type == NANOSCOPE_FILE_TYPE_BROKEN) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File has been damaged by change of line endings, "
                      "resulting in corruption of the binary part of the file."
                      "\n\n"
                      "Typically, this occurs if the file is treated as text "
                      "when sent by e-mail uncompressed, sent by FTP in ascii "
                      "mode (use binary), compressed by ‘Send to compressed "
                      "folder’ in some versions of MS Windows, or any other "
                      "file transfer that attempts to store text "
                      "platform-independently."));
        g_free(buffer);
        return NULL;
    }

    gwy_debug("File type: %d", file_type);
    /* as we already know file_type, fix the first char for hash reading */
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
        if (gwy_strequal(self, "File list")) {
            has_version = !!g_hash_table_lookup(hash, "Version");
            gwy_debug("has Version: %d", has_version);
            continue;
        }
        if (gwy_stramong(self, "Ciao scan list", "Afm list", "Stm list",
                         "NC Afm list", NULL)) {
            get_scan_list_res(hash, &xres, &yres);
            scanlist = hash;
        }
        if (!gwy_stramong(self, "Ciao image list", "AFM image list",
                          "STM image list", "NCAFM image list",
                          "Ciao force image list", NULL))
            continue;

        if (file_type == NANOSCOPE_FILE_TYPE_FORCE_BIN) {
            ndata->graph_model = hash_to_curve(hash, scannerlist, scanlist,
                                               file_type,
                                               size, buffer,
                                               xres,
                                               error);
            ok = ok && ndata->graph_model;
        }
        else {
            ndata->data_field = hash_to_data_field(hash, scannerlist, scanlist,
                                                   file_type, has_version,
                                                   size, buffer,
                                                   xres, yres,
                                                   &p, error);
            ok = ok && ndata->data_field;
        }
    }

    if (ok) {
        gchar key[40];

        i = 0;
        container = gwy_container_new();
        for (l = list; l; l = g_list_next(l)) {
            ndata = (NanoscopeData*)l->data;
            if (ndata->data_field) {
                const gchar *name;

                g_snprintf(key, sizeof(key), "/%d/data", i);
                gwy_container_set_object_by_name(container, key,
                                                 ndata->data_field);
                g_snprintf(key, sizeof(key), "/%d/data/title", i);
                name = NULL;
                if ((val = g_hash_table_lookup(ndata->hash,
                                               "@2:Image Data"))
                    || (val = g_hash_table_lookup(ndata->hash,
                                                  "@3:Image Data"))) {
                    if (val->soft_scale)
                        name = val->soft_scale;
                    else if (val->hard_value_str)
                        name = val->hard_value_str;
                }
                else if ((val = g_hash_table_lookup(ndata->hash, "Image data")))
                    name = val->hard_value_str;

                if (name) {
                    gsize nlen = strlen(name);

                    /* Unquote name if it's quoted. */
                    if (name[0] == '"' && name[nlen-1] == '"')
                        gwy_container_set_string_by_name(container, key,
                                                         g_strndup(name+1,
                                                                   nlen-2));
                    else
                        gwy_container_set_string_by_name(container, key,
                                                         g_strdup(name));
                }

                meta = nanoscope_get_metadata(ndata->hash, list);
                g_snprintf(key, sizeof(key), "/%d/meta", i);
                gwy_container_set_object_by_name(container, key, meta);
                g_object_unref(meta);

                gwy_app_channel_check_nonsquare(container, i);
                i++;
            }
            else if (ndata->graph_model) {
                GQuark quark = gwy_app_get_graph_key_for_id(i+1);
                gwy_container_set_object(container, quark, ndata->graph_model);
                i++;
            }
        }
        if (!i)
            gwy_object_unref(container);
    }

    for (l = list; l; l = g_list_next(l)) {
        ndata = (NanoscopeData*)l->data;
        gwy_object_unref(ndata->data_field);
        gwy_object_unref(ndata->graph_model);
        if (ndata->hash)
            g_hash_table_destroy(ndata->hash);
        g_free(ndata);
    }
    g_free(buffer);
    g_list_free(list);

    if (!container && ok)
        err_NO_DATA(error);

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
        *xres = GWY_ROUND(val->hard_value);
    if ((val = g_hash_table_lookup(hash, "Lines")))
        *yres = GWY_ROUND(val->hard_value);
    gwy_debug("Global xres, yres = %d, %d", *xres, *yres);
}

static void
add_metadata(gpointer hkey,
             gpointer hvalue,
             gpointer user_data)
{
    gchar *key = (gchar*)hkey;
    NanoscopeValue *val = (NanoscopeValue*)hvalue;
    gchar *v, *w;

    if (gwy_strequal(key, "#self")
        || !val->hard_value_str
        || !val->hard_value_str[0])
        return;

    if (key[0] == '@')
        key++;
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
    gwy_container_set_string_by_name(GWY_CONTAINER(user_data), key, v);
}

/* FIXME: This is a bit simplistic */
static GwyContainer*
nanoscope_get_metadata(GHashTable *hash,
                       GList *list)
{
    static const gchar *hashes[] = {
        "File list", "Scanner list", "Equipment list", "Ciao scan list",
    };
    GwyContainer *meta;
    GList *l;
    guint i;

    meta = gwy_container_new();

    for (l = list; l; l = g_list_next(l)) {
        GHashTable *h = ((NanoscopeData*)l->data)->hash;
        for (i = 0; i < G_N_ELEMENTS(hashes); i++) {
            if (gwy_strequal(g_hash_table_lookup(h, "#self"), hashes[i])) {
                g_hash_table_foreach(h, add_metadata, meta);
                break;
            }
        }
    }
    g_hash_table_foreach(hash, add_metadata, meta);

    return meta;
}

static GwyDataField*
hash_to_data_field(GHashTable *hash,
                   GHashTable *scannerlist,
                   GHashTable *scanlist,
                   NanoscopeFileType file_type,
                   gboolean has_version,
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
    gboolean size_ok, use_global;

    if (!require_keys(hash, error, "Samps/line", "Number of lines",
                      "Scan size", "Data offset", "Data length", NULL))
        return NULL;

    val = g_hash_table_lookup(hash, "Samps/line");
    xres = GWY_ROUND(val->hard_value);

    val = g_hash_table_lookup(hash, "Number of lines");
    yres = GWY_ROUND(val->hard_value);

    val = g_hash_table_lookup(hash, "Bytes/pixel");
    bpp = val ? GWY_ROUND(val->hard_value) : 2;

    /* scan size */
    val = g_hash_table_lookup(hash, "Scan size");
    xreal = g_ascii_strtod(val->hard_value_str, &end);
    if (errno || *end != ' ') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot parse `Scan size' field."));
        return NULL;
    }
    gwy_debug("xreal = %g", xreal);
    s = end+1;
    yreal = g_ascii_strtod(s, &end);
    if (errno || *end != ' ') {
        /* Old files don't have two numbers here, assume equal dimensions */
        yreal = xreal;
        end = s;
    }
    gwy_debug("yreal = %g", yreal);
    while (g_ascii_isspace(*end))
        end++;
    if (sscanf(end, "%4s", un) != 1) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot parse `Scan size' field."));
        return NULL;
    }
    gwy_debug("xy unit: <%s>", un);
    unitxy = gwy_si_unit_new_parse(un, &power10);
    q = pow10(power10);
    xreal *= q;
    yreal *= q;

    offset = size = 0;
    if (file_type == NANOSCOPE_FILE_TYPE_BIN) {
        val = g_hash_table_lookup(hash, "Data offset");
        offset = GWY_ROUND(val->hard_value);

        val = g_hash_table_lookup(hash, "Data length");
        size = GWY_ROUND(val->hard_value);

        size_ok = FALSE;
        use_global = FALSE;

        /* Try channel size and local size */
        if (!size_ok && size == bpp*xres*yres)
            size_ok = TRUE;

        if (!size_ok && size == bpp*gxres*gyres) {
            size_ok = TRUE;
            use_global = TRUE;
        }

        /* If they don't match exactly, try whether they at least fit inside */
        if (!size_ok && size > bpp*MAX(xres*yres, gxres*gyres)) {
            size_ok = TRUE;
            use_global = (xres*yres < gxres*gyres);
        }

        if (!size_ok && size > bpp*MIN(xres*yres, gxres*gyres)) {
            size_ok = TRUE;
            use_global = (xres*yres > gxres*gyres);
        }

        if (!size_ok) {
            err_SIZE_MISMATCH(error, size, bpp*xres*yres, TRUE);
            return NULL;
        }

        if (use_global) {
            if (gxres) {
                xreal *= (gdouble)gxres/xres;
                xres = gxres;
            }
            if (gyres) {
                yreal *= (gdouble)gyres/yres;
                yres = gyres;
            }
        }
        else {
            /* Reported by Peter Eaton.  No test case that would contradict
             * this known. */
            yreal *= yres;
            yreal /= xres;
        }

        if (err_DIMENSION(error, xres) || err_DIMENSION(error, yres))
            return NULL;

        if (err_SIZE_MISMATCH(error, offset + size, bufsize, FALSE))
            return NULL;

        /* Use negated positive conditions to catch NaNs */
        if (!((xreal = fabs(xreal)) > 0)) {
            g_warning("Real x size is 0.0, fixing to 1.0");
            xreal = 1.0;
        }
        if (!((yreal = fabs(yreal)) > 0)) {
            g_warning("Real y size is 0.0, fixing to 1.0");
            yreal = 1.0;
        }
    }

    q = 1.0;
    unitz = get_physical_scale(hash, scannerlist, scanlist, has_version,
                               &q, error);
    if (!unitz)
        return NULL;

    dfield = gwy_data_field_new(xres, yres, xreal, yreal, FALSE);
    data = gwy_data_field_get_data(dfield);
    switch (file_type) {
        case NANOSCOPE_FILE_TYPE_TXT:
        if (!read_text_data(xres*yres, data, p, bpp, error)) {
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
                   gboolean has_version,
                   gdouble *scale,
                   GError **error)
{
    GwySIUnit *siunit, *siunit2;
    NanoscopeValue *val, *sval;
    gchar *key;
    gint q;

    /* Very old style scales (files without Version field) */
    if (!has_version) {
        if (!(val = g_hash_table_lookup(hash, "Z magnify image"))) {
            err_MISSING_FIELD(error, "Z magnify image");
            return NULL;
        }

        /* TODO: Luminescence */
        siunit = gwy_si_unit_new("m");
        /* According to Markus, the units are 1/100 nm, but his scale
         * calculation is raw/655.36 [nm].  We have the factor 1/65536 applied
         * automatically, that gives 1e-7 [m].  Whatever. */
        *scale = 1e-7 * val->hard_value;
        return siunit;
    }

    /* XXX: This is a damned heuristics.  For some value types we try to guess
     * a different quantity scale to look up. */
    if (!(val = g_hash_table_lookup(hash, "@2:Z scale"))) {
        if (!(val = g_hash_table_lookup(hash, "Z scale"))) {
            err_MISSING_FIELD(error, "Z scale");
            return NULL;
        }

        /* Old style scales */
        siunit = gwy_si_unit_new_parse(val->hard_value_units, &q);
        *scale = val->hard_value * pow10(q);
        return siunit;
    }

    /* Resolve reference to a soft scale */
    if (val->soft_scale) {
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
            gwy_debug("Scale2 = %g %s",
                      sval->hard_value, sval->hard_value_units);
            *scale *= pow10(q);
            gwy_debug("Total scale = %g %s/LSB",
                    *scale, gwy_si_unit_get_string(siunit,
                                                    GWY_SI_UNIT_FORMAT_PLAIN));
            g_object_unref(siunit2);
        }
        g_free(key);
    }
    else {
        /* We have '@2:Z scale' but the reference to soft scale is missing,
         * the quantity is something in the hard units (seen for Potential). */
        siunit = gwy_si_unit_new_parse(val->hard_value_units, &q);
        *scale = val->hard_value * pow10(q);
    }

    return siunit;
}

static GwyGraphModel*
hash_to_curve(GHashTable *hash,
              GHashTable *scannerlist,
              GHashTable *scanlist,
              NanoscopeFileType file_type,
              guint bufsize,
              gchar *buffer,
              gint gxres,
              GError **error)
{
    NanoscopeValue *val;
    GwyDataLine *dline;
    GwyGraphModel *gmodel;
    GwyGraphCurveModel *gcmodel;
    GwySIUnit *unitz, *unitx;
    gchar *s, *end;
    gchar un[5];
    gint xres, bpp, offset, size, power10;
    gdouble xreal, q;
    gdouble *data;
    gboolean size_ok, use_global;

    if (!require_keys(hash, error, "Samps/line", "Data offset", "Data length",
                      NULL))
        return NULL;

    if (!require_keys(scanlist, error, "Scan size", NULL))
        return NULL;

    val = g_hash_table_lookup(hash, "Samps/line");
    xres = GWY_ROUND(val->hard_value);

    val = g_hash_table_lookup(hash, "Bytes/pixel");
    bpp = val ? GWY_ROUND(val->hard_value) : 2;

    /* scan size */
    val = g_hash_table_lookup(scanlist, "Scan size");
    xreal = g_ascii_strtod(val->hard_value_str, &end);
    if (errno || *end != ' ') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot parse `Scan size' field."));
        return NULL;
    }
    gwy_debug("xreal = %g", xreal);
    while (g_ascii_isspace(*end))
        end++;
    if (sscanf(end, "%4s", un) != 1) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Cannot parse `Scan size' field."));
        return NULL;
    }
    gwy_debug("x unit: <%s>", un);
    unitx = gwy_si_unit_new_parse(un, &power10);
    q = pow10(power10);
    xreal *= q;

    offset = size = 0;
    if (file_type == NANOSCOPE_FILE_TYPE_FORCE_BIN) {
        val = g_hash_table_lookup(hash, "Data offset");
        offset = GWY_ROUND(val->hard_value);

        val = g_hash_table_lookup(hash, "Data length");
        size = GWY_ROUND(val->hard_value);

        size_ok = FALSE;
        use_global = FALSE;

        /* Try channel size and local size */
        if (!size_ok && size == 2*bpp*xres)
            size_ok = TRUE;

        if (!size_ok && size == 2*bpp*gxres) {
            size_ok = TRUE;
            use_global = TRUE;
        }

        /* If they don't match exactly, try whether they at least fit inside */
        if (!size_ok && size > bpp*MAX(2*xres, 2*gxres)) {
            size_ok = TRUE;
            use_global = (xres < gxres);
        }

        if (!size_ok && size > bpp*MIN(2*xres, 2*gxres)) {
            size_ok = TRUE;
            use_global = (xres > gxres);
        }

        if (!size_ok) {
            err_SIZE_MISMATCH(error, size, bpp*xres, TRUE);
            return NULL;
        }

        if (use_global) {
            if (gxres) {
                xreal *= (gdouble)gxres/xres;
                xres = gxres;
            }
        }
        else {
            /* Reported by Peter Eaton.  No test case that would contradict
             * this known. */
            xreal *= xres;
        }

        if (err_DIMENSION(error, xres))
            return NULL;

        if (err_SIZE_MISMATCH(error, offset + size, bufsize, FALSE))
            return NULL;

        /* Use negated positive conditions to catch NaNs */
        if (!((xreal = fabs(xreal)) > 0)) {
            g_warning("Real x size is 0.0, fixing to 1.0");
            xreal = 1.0;
        }
    }

    q = 1.0;
    /*
    unitz = get_physical_scale(hash, scannerlist, scanlist, has_version,
                               &q, error);
                               */
    unitz = gwy_si_unit_new(NULL);
    if (!unitz)
        return NULL;

    gmodel = gwy_graph_model_new();

    dline = gwy_data_line_new(xres, xreal, FALSE);
    gwy_data_line_set_si_unit_y(dline, unitz);
    g_object_unref(unitz);
    gwy_data_line_set_si_unit_x(dline, unitx);
    g_object_unref(unitx);

    data = gwy_data_line_get_data(dline);
    switch (file_type) {
        case NANOSCOPE_FILE_TYPE_FORCE_BIN:
        if (!read_binary_data(xres, data, buffer + offset, bpp, error)) {
            g_object_unref(dline);
            g_object_unref(gmodel);
            return NULL;
        }
        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, dline, 0, 0);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);

        if (!read_binary_data(xres, data, buffer + offset + bpp*xres, bpp,
                              error)) {
            g_object_unref(dline);
            g_object_unref(gmodel);
            return NULL;
        }
        gcmodel = gwy_graph_curve_model_new();
        gwy_graph_curve_model_set_data_from_dataline(gcmodel, dline, 0, 0);
        gwy_graph_model_add_curve(gmodel, gcmodel);
        g_object_unref(gcmodel);
        break;

        default:
        g_assert_not_reached();
        break;
    }

    gwy_graph_model_set_units_from_data_line(gmodel, dline);
    g_object_unref(dline);

    return gmodel;
}

static gboolean
read_text_data(gint n, gdouble *data,
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

    hash = g_hash_table_new_full(g_str_hash, g_str_equal,
                                 NULL, g_free);
    g_hash_table_insert(hash, "#self", g_strdup(line + 2));    /* self */
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

        while ((*buffer)[0] == '\r') {
            g_warning("Possibly split line encountered.  "
                      "Trying to synchronize.");
            line = gwy_str_next_line(buffer) + 1;
            line = gwy_str_next_line(buffer) + 1;
        }
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

    /* Fix random stuff in MultiMode V (and possibly other) files */
    if ((value = g_hash_table_lookup(hash, "Scan Size"))
        && !g_hash_table_lookup(hash, "Scan size")) {
        g_hash_table_insert(hash, "Scan size",
                            g_memdup(value, sizeof(NanoscopeValue)));
        g_hash_table_remove(hash, "Scan Size");
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
        if (p-line > 0 && *p == ' ') {
            do {
                p++;
            } while (g_ascii_isspace(*p));
            if ((q = strchr(p, '('))) {
                *q = '\0';
                q++;
                val->hard_scale = g_ascii_strtod(q, &q);
                if (*q != ')')
                    val->hard_scale = 0.0;
            }
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
        int paren_level;
        do {
            line++;
        } while (g_ascii_isspace(*line));
        for (p = line, paren_level = 1; *p && paren_level; p++) {
            if (*p == ')')
                paren_level--;
            else if (*p == '(')
                paren_level++;
        }
        if (!*p) {
            g_warning("Cannot parse hard scale <%s> for key <%s>", line, key);
            g_free(val);
            return NULL;
        }
        p--;
        val->hard_scale = g_ascii_strtod(line, &q);
        while (g_ascii_isspace(*q))
            q++;
        if (p-q > 0) {
            *p = '\0';
            val->hard_scale_units = q;
            g_strchomp(q);
            if (g_str_has_suffix(q, "/LSB"))
                q[strlen(q) - 4] = '\0';
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

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

