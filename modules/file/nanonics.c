/*
 *  $Id$
 *  Copyright (C) 2009-2011 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * [FILE-MAGIC-FREEDESKTOP]
 * <mime-type type="application/x-nanonics-spm">
 *   <comment>Nanonics SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="NAN File\n-Start Header-"/>
 *   </magic>
 *   <glob pattern="*.nan"/>
 *   <glob pattern="*.NAN"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Nanonics NAN
 * .nan
 * Read
 **/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC_LINE      "NAN File\n"
#define MAGIC_LINE_SIZE (sizeof(MAGIC_LINE) - 1)

#define MAGIC      MAGIC_LINE "-Start Header-"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".nan"

#define Micrometer 1e-6

typedef struct {
    guint header_size;
    guint page_size;
    guint page_header_size;
    guint page_data_size;
    guint xres;
    guint yres;
    gdouble xreal;
    gdouble yreal;
    GHashTable *meta;
    GHashTable **pagemeta;
} NanonicsFile;

static gboolean      module_register         (void);
static gint          nanonics_detect         (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* nanonics_load           (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static GHashTable*   nanonics_read_header    (gchar *text,
                                              const gchar *name,
                                              GError **error);
static void nanonics_parse_comment(GHashTable *hash,
                       const gchar *comment);
static GwyDataField* nanonics_read_data_field(const NanonicsFile *nfile,
                                              guint id,
                                              gboolean retrace,
                                              const guchar *buffer);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanonics NAN data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti)",
    "2009",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanonics",
                           N_("Nanonics files (.nan)"),
                           (GwyFileDetectFunc)&nanonics_detect,
                           (GwyFileLoadFunc)&nanonics_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
nanonics_detect(const GwyFileDetectInfo *fileinfo,
                gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 10 : 0;

    if (fileinfo->buffer_len < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 80;
}

static GwyContainer*
nanonics_load(const gchar *filename,
              G_GNUC_UNUSED GwyRunType mode,
              GError **error)
{
    enum { size_guess = 4096 };

    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gchar *s, *header = NULL;
    NanonicsFile nfile;
    gsize size = 0;
    GError *err = NULL;
    guint i, ndata = 0, header_size;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Nanonics");
        goto fail;
    }

    gwy_clear(&nfile, 1);
    header = g_strndup(buffer + MAGIC_LINE_SIZE,
                       MIN(size - MAGIC_LINE_SIZE, size_guess));
    if ((s = strstr(header, "HeaderLength="))) {
        header_size = g_ascii_strtoull(s + strlen("HeaderLength="), NULL, 10);
        if (header_size > size_guess && header_size <= size - MAGIC_LINE_SIZE) {
            g_free(header);
            header = g_strndup(buffer + MAGIC_LINE_SIZE, header_size);
        }
    }
    if (!(s = strstr(header, "-End Header-"))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected header end marker ‘%s’ was not found."),
                    "-End Header-");
        goto fail;
    }
    header_size = (s - header) + strlen("-End Header-");
    header[header_size] = '\0';

    if (!(nfile.meta = nanonics_read_header(header, "Header", error)))
        goto fail;

    g_free(header);
    header = NULL;

    if (!require_keys(nfile.meta, error,
                      "HeaderLength", "DataLength",
                      "ReF", "ReS", "WSF", "WSS",
                      NULL))
        goto fail;

    /* Must explicitly specify base, the numbers often start with 0 */
    nfile.header_size = strtol(g_hash_table_lookup(nfile.meta, "HeaderLength"),
                               NULL, 10);
    nfile.page_size = strtol(g_hash_table_lookup(nfile.meta, "DataLength"),
                             NULL, 10);

    if (nfile.header_size != header_size + MAGIC_LINE_SIZE) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("HeaderLength %u differs from actual header length %u"),
                    nfile.header_size, header_size);
        goto fail;
    }

    if ((s = g_hash_table_lookup(nfile.meta, "Number of channels"))) {
        ndata = g_ascii_strtoull(s, NULL, 10);
        /* It should be only set if we did find any channel descriptions. */
        g_assert(ndata);
        if (err_SIZE_MISMATCH(error,
                              nfile.page_size*ndata,
                              size - nfile.header_size, FALSE))
            goto fail;
        gwy_debug("ndata (from comment): %u", ndata);
    }
    else {
        ndata = (size - nfile.header_size)/nfile.page_size;
        gwy_debug("ndata (from size): %u", ndata);
        if (!ndata) {
            err_NO_DATA(error);
            goto fail;
        }
    }

    nfile.xres = strtol(g_hash_table_lookup(nfile.meta, "ReF"), NULL, 10);
    nfile.yres = strtol(g_hash_table_lookup(nfile.meta, "ReS"), NULL, 10);
    gwy_debug("xres: %u, yres: %u", nfile.xres, nfile.yres);
    if (err_DIMENSION(error, nfile.xres) || err_DIMENSION(error, nfile.yres))
        goto fail;

    nfile.xreal = g_ascii_strtod(g_hash_table_lookup(nfile.meta, "WSF"), NULL);
    if (!(nfile.xreal = fabs(nfile.xreal))) {
        g_warning("Real x size is 0.0, fixing to 1.0");
        nfile.xreal = 1.0;
    }
    nfile.xreal *= Micrometer;

    nfile.yreal = g_ascii_strtod(g_hash_table_lookup(nfile.meta, "WSS"), NULL);
    if (!(nfile.yreal = fabs(nfile.yreal))) {
        g_warning("Real y size is 0.0, fixing to 1.0");
        nfile.yreal = 1.0;
    }
    nfile.yreal *= Micrometer;

    nfile.page_data_size = sizeof(guint32)*nfile.xres*nfile.yres;
    gwy_debug("page data size: %u", nfile.page_data_size);
    /* Well, there is probably a stricter page header size lower bound than 4 */
    if (err_SIZE_MISMATCH(error, nfile.page_data_size + 4, nfile.page_size,
                          FALSE))
        goto fail;

    nfile.page_header_size = nfile.page_size - nfile.page_data_size;
    gwy_debug("page header size: %u", nfile.page_header_size);
    nfile.pagemeta = g_new0(GHashTable*, ndata);
    s = buffer + nfile.header_size;
    for (i = 0; i < ndata; i++) {
        gwy_debug("reading page header %u", i);
        header = g_strndup(s + i*nfile.page_size, nfile.page_header_size);
        if (!(nfile.pagemeta[i] = nanonics_read_header(header, "Channel Header",
                                                       &err))) {
            if (i == 0) {
                g_propagate_error(error, err);
                goto fail;
            }
            else {
                g_warning("Cannot read the expected number of channels %u, "
                          "failed after %u.", ndata, i);
                ndata = i;
            }
        }
        g_free(header);
        header = NULL;
    }

    container = gwy_container_new();
    s += nfile.page_header_size;
    for (i = 0; i < ndata; i++) {
        guint j;

        for (j = 0; j < 2; j++) {
            GwyDataField *dfield;
            gchar *key, *title;
            GQuark quark;

            dfield = nanonics_read_data_field(&nfile, i, j,
                                              s + i*nfile.page_size);
            quark = gwy_app_get_data_key_for_id(2*i + j);
            gwy_container_set_object(container, quark, dfield);
            g_object_unref(dfield);

            if ((title = g_hash_table_lookup(nfile.pagemeta[i], "CHN"))) {
                key = g_strconcat(g_quark_to_string(quark), "/title", NULL);
                if (j) {
                    title = g_strconcat(title, " [Retrace]", NULL);
                    gwy_container_set_string_by_name(container, key, title);
                }
                else
                    gwy_container_set_string_by_name(container, key,
                                                     g_strdup(title));
                g_free(key);
            }
        }
    }

fail:
    g_free(header);
    if (nfile.meta)
        g_hash_table_destroy(nfile.meta);
    if (nfile.pagemeta) {
        for (i = 0; i < ndata; i++) {
            if (nfile.pagemeta[i])
                g_hash_table_destroy(nfile.pagemeta[i]);
        }
        g_free(nfile.pagemeta);
    }
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

static GHashTable*
nanonics_read_header(gchar *text, const gchar *name, GError **error)
{
    GHashTable *hash;
    gchar *line, *p, *s, *val, *marker;
    GString *comment = NULL;

    p = text;

    line = gwy_str_next_line(&p);
    g_strstrip(line);
    marker = g_strdup_printf("-Start %s-", name);
    if (!gwy_strequal(line, marker)) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected header start marker ‘%s’ but found ‘%s’."),
                    marker, line);
        g_free(marker);
        return NULL;
    }
    g_free(marker);

    marker = g_strdup_printf("-End %s-", name);
    hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        g_strstrip(line);
        if (gwy_strequal(line, marker))
            break;
        if (!*line)
            continue;

        if (comment) {
            g_string_append_c(comment, '\n');
            g_string_append(comment, line);
            if (line[strlen(line)-1] == ']') {
                gwy_debug("comment: <%s>", comment->str);
                g_string_erase(comment, 0, 1);
                g_string_truncate(comment, comment->len-1);
                g_hash_table_insert(hash,
                                    g_strdup("comment"),
                                    g_string_free(comment, FALSE));
                comment = NULL;
            }
            continue;
        }

        while (line && (s = strchr(line, '='))) {
            *s = '\0';
            g_strchomp(line);
            for (val = s+1; g_ascii_isspace(*val); val++)
                ;

            s = line;
            if ((line = strchr(val, ','))) {
                *line = '\0';
                line++;
            }
            g_strchomp(val);
            if (gwy_strequal(s, "comment")) {
                if (val[0] == '[') {
                    if (val[strlen(val)-1] != ']') {
                        /* Start gathering multiline comment. */
                        comment = g_string_new(val);
                        break;
                    }
                    else {
                        /* In-line comment; just remove the brackets. */
                        val[strlen(val)-1] = '\0';
                        val += 1;
                        g_hash_table_insert(hash, g_strdup(s), g_strdup(val));
                        gwy_debug("<%s>=<%s>", s, val);
                    }
                }
            }
            else {
                g_hash_table_insert(hash, g_strdup(s), g_strdup(val));
                gwy_debug("<%s>=<%s>", s, val);
            }
        }

    }

    /* Should not happen if the comment is properly terminated. */
    if (comment)
        g_string_free(comment, TRUE);
    else if ((s = g_hash_table_lookup(hash, "comment")) && strchr(s, '\n'))
        nanonics_parse_comment(hash, s);

    if (!line) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Expected header end marker ‘%s’ was not found."),
                    marker);
        g_hash_table_destroy(hash);
        g_free(marker);
        return NULL;
    }

    line = gwy_str_next_line(&p);
    if (line)
        g_warning("Text beyond %s", marker);
    g_free(marker);

    return hash;
}

static void
nanonics_parse_comment(GHashTable *hash,
                       const gchar *comment)
{
    gchar *buffer = g_strdup(comment);
    gchar *p = buffer, *line, *value, **fields;
    gboolean reading_channels = FALSE;
    guint id, nchannels = 0;

    for (line = gwy_str_next_line(&p); line; line = gwy_str_next_line(&p)) {
        g_strstrip(line);
        if (gwy_strequal(line, "Analog channels:")) {
            reading_channels = TRUE;
            continue;
        }
        if (reading_channels) {
            if (sscanf(line, "%d )", &id) == 1) {
                if (id != nchannels) {
                    g_warning("Channel #%u has non-sequential number %u.",
                              nchannels, id);
                    id = nchannels;
                }
                line = strchr(line, ')') + 1;
                fields = g_strsplit(line, ",", 0);
                if (fields
                    && g_strv_length(fields) >= 3
                    && g_str_has_prefix(fields[1], "Units:")
                    && g_str_has_prefix(fields[2], "Formula:")) {
                    value = g_strstrip(fields[0]);
                    gwy_debug("Channel %u name: <%s>", id, value);
                    g_hash_table_insert(hash, g_strdup_printf("Channel%u", id),
                                        g_strdup(value));
                    value = g_strstrip(fields[1] + strlen("Units:"));
                    gwy_debug("Channel %u units: <%s>", id, value);
                    g_hash_table_insert(hash, g_strdup_printf("Units%u", id),
                                        g_strdup(value));
                    value = g_strstrip(fields[2] + strlen("Formula:"));
                    gwy_debug("Channel %u formula: <%s>", id, value);
                    g_hash_table_insert(hash, g_strdup_printf("Formula%u", id),
                                        g_strdup(value));
                }
                g_strfreev(fields);
                nchannels++;
                continue;
            }
            reading_channels = FALSE;
        }
        if ((value = strstr(line, ": "))) {
            *value = '\0';
            value += 2;
            g_strstrip(value);
            g_strchomp(line);
            g_hash_table_insert(hash, g_strdup(line), g_strdup(value));
            gwy_debug("<%s>=<%s>", line, value);
        }
    }

    if (nchannels)
        g_hash_table_insert(hash, g_strdup("Number of channels"),
                            g_strdup_printf("%u", nchannels));

    g_free(buffer);
}

static GwyDataField*
nanonics_read_data_field(const NanonicsFile *nfile,
                         guint id,
                         gboolean retrace,
                         const guchar *buffer)
{
    const gint16 *d16 = (const gint16*)buffer;
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gint xres, yres, i, j, power10;
    gdouble *d;
    gchar *s;

    xres = nfile->xres;
    yres = nfile->yres;
    dfield = gwy_data_field_new(xres, yres, nfile->xreal, nfile->yreal, FALSE);
    d = gwy_data_field_get_data(dfield);

    for (i = 0; i < yres; i++) {
        if (retrace) {
            for (j = 0; j < xres; j++) {
                gint16 v = d16[(i + 1)*2*xres - 1 - j];
                *(d++) = GINT16_FROM_BE(v);
            }
        }
        else {
            for (j = 0; j < xres; j++) {
                gint16 v = d16[i*2*xres + j];
                *(d++) = GINT16_FROM_BE(v);
            }
        }
    }

    siunit = gwy_data_field_get_si_unit_xy(dfield);
    gwy_si_unit_set_from_string(siunit, "m");

    if ((s = g_hash_table_lookup(nfile->pagemeta[id], "CHU"))) {
        /* Fix some verbose units
         * XXX: Modifies the string in-place (can do since it's allocated but
         * be careful). */
        if (gwy_strequal(s, "Pi")) {
            gwy_data_field_multiply(dfield, G_PI);
            s[0] = '\0';
        }
        if (g_str_has_suffix(s, "Volts"))
            s[strlen(s) - strlen("Volts") + 1] = '\0';
        else if (g_str_has_suffix(s, "Newton"))
            s[strlen(s) - strlen("Newton") + 1] = '\0';

        siunit = gwy_data_field_get_si_unit_z(dfield);
        gwy_si_unit_set_from_string_parse(siunit, s, &power10);

        if (power10)
            gwy_data_field_multiply(dfield, pow10(power10));
    }

    return dfield;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
