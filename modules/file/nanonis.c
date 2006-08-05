/*
 *  $Id$
 *  Copyright (C) 2006 David Necas (Yeti), Petr Klapetek.
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

/* TODO: metadata */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>

#include <string.h>

#include "err.h"
#include "get.h"

#define MAGIC ":NANONIS_VERSION:"
#define MAGIC_SIZE (sizeof(MAGIC) - 1)

#define EXTENSION ".sxm"

typedef enum {
    DIR_FORWARD  = 1 << 0,
    DIR_BACKWARD = 1 << 1,
    DIR_BOTH     = (DIR_FORWARD || DIR_BACKWARD)
} SXMDirection;

typedef struct {
    gint channel;
    gchar *name;
    gchar *unit;
    SXMDirection direction;
    gdouble calibration;
    gdouble offset;
} SXMDataInfo;

typedef struct {
    GHashTable *meta;
    gchar **z_controller_headers;
    gchar **z_controller_values;
    gint ndata;
    SXMDataInfo *data_info;

    gboolean ok;
    gint xres;
    gint yres;
    gdouble xreal;
    gdouble yreal;
} SXMFile;

static gboolean      module_register(void);
static gint          sxm_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* sxm_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanonis SXM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.2",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
};

/* FIXME: I'm making this up, never seen anything except `both' */
static const GwyEnum directions[] = {
    { "forward",  DIR_FORWARD,  },
    { "backward", DIR_BACKWARD, },
    { "both",     DIR_BOTH,     },
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nanonis",
                           N_("Nanonis SXM files (.sxm)"),
                           (GwyFileDetectFunc)&sxm_detect,
                           (GwyFileLoadFunc)&sxm_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sxm_detect(const GwyFileDetectInfo *fileinfo,
           gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->buffer_len > MAGIC_SIZE
        && memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        score = 100;

    return score;
}

static gchar*
get_next_line_with_error(gchar **p,
                         GError **error)
{
    gchar *line;

    if (!(line = gwy_str_next_line(p))) {
        g_set_error(error, GWY_MODULE_FILE_ERROR,
                    GWY_MODULE_FILE_ERROR_DATA,
                    _("File header ended unexpectedly."));
        return NULL;
    }
    g_strstrip(line);

    return line;
}

static gchar**
split_line_in_place(gchar *line,
                    gchar delim)
{
    gchar **strs;
    guint i, n = 0;

    for (i = 0; line[i]; i++) {
        if ((!i || line[i-1] == delim) && (line[i] && line[i] != delim))
            n++;
    }

    strs = g_new(gchar*, n+1);
    n = 0;
    for (i = 0; line[i]; i++) {
        if ((!i || line[i-1] == delim || !line[i-1])
            && (line[i] && line[i] != delim))
            strs[n++] = line + i;
        else if (i && line[i] == delim && line[i-1] != delim)
            line[i] = '\0';
    }
    strs[n] = NULL;

#ifdef DEBUG
    for (i = 0; strs[i]; i++)
        gwy_debug("%u: <%s>", i, strs[i]);
#endif

    return strs;
}

static void
sxm_free_z_controller(SXMFile *sxmfile)
{
    g_free(sxmfile->z_controller_headers);
    sxmfile->z_controller_headers = NULL;
    g_free(sxmfile->z_controller_values);
    sxmfile->z_controller_values = NULL;
}

static gboolean
sxm_read_tag(SXMFile *sxmfile,
             gchar **p,
             GError **error)
{
    gchar *line, *tag;
    gchar **columns;
    guint len;

    if (!(line = get_next_line_with_error(p, error)))
        return FALSE;

    len = strlen(line);
    if (len < 3 || line[0] != ':' || line[len-1] != ':') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Garbage was found in place of tag header line."));
        return FALSE;
    }
    tag = line+1;
    line[len-1] = '\0';
    gwy_debug("tag: <%s>", tag);

    if (gwy_strequal(tag, "SCANIT_END")) {
        sxmfile->ok = TRUE;
        return TRUE;
    }

    if (gwy_strequal(tag, "Z-CONTROLLER")) {
        /* Headers */
        if (!(line = get_next_line_with_error(p, error)))
            return FALSE;

        if (sxmfile->z_controller_headers) {
            g_warning("Multiple Z-CONTROLLERs, keeping only the last");
            sxm_free_z_controller(sxmfile);
        }

        /* XXX: Documentation says tabs, but I see spaces in the file. */
        g_strdelimit(line, " ", '\t');
        sxmfile->z_controller_headers =  split_line_in_place(line, '\t');

        /* Values */
        if (!(line = get_next_line_with_error(p, error))) {
            sxm_free_z_controller(sxmfile);
            return FALSE;
        }

        sxmfile->z_controller_values = split_line_in_place(line, '\t');
        if (g_strv_length(sxmfile->z_controller_headers)
            != g_strv_length(sxmfile->z_controller_values)) {
            g_warning("The numbers of Z-CONTROLLER headers and values differ");
            sxm_free_z_controller(sxmfile);
        }
        return TRUE;
    }

    if (gwy_strequal(tag, "DATA_INFO")) {
        SXMDataInfo di;
        GArray *data_info;

        /* Headers */
        if (!(line = get_next_line_with_error(p, error)))
            return FALSE;
        /* XXX: Documentation says tabs, but I see spaces in the file. */
        g_strdelimit(line, " ", '\t');
        columns = split_line_in_place(line, '\t');

        if (g_strv_length(columns) < 6
            || !gwy_strequal(columns[0], "Channel")
            || !gwy_strequal(columns[1], "Name")
            || !gwy_strequal(columns[2], "Unit")
            || !gwy_strequal(columns[3], "Direction")
            || !gwy_strequal(columns[4], "Calibration")
            || !gwy_strequal(columns[5], "Offset")) {
            g_set_error(error, GWY_MODULE_FILE_ERROR,
                        GWY_MODULE_FILE_ERROR_DATA,
                        _("DATA_INFO does not contain the expected "
                          "columns: %s."),
                        "Channel Name Unit Direction Calibration Offset");
            g_free(columns);
            return FALSE;
        }

        if (sxmfile->data_info) {
            g_warning("Multiple DATA_INFOs, keeping only the last");
            g_free(sxmfile->data_info);
            sxmfile->data_info = NULL;
        }

        data_info = g_array_new(FALSE, FALSE, sizeof(SXMDataInfo));
        while ((line = get_next_line_with_error(p, error)) && *line) {
            g_strstrip(line);
            if (gwy_strequal(line, ":SCANIT_END:")) {
                sxmfile->ok = TRUE;
                return TRUE;
            }

            columns = split_line_in_place(line, '\t');
            if (g_strv_length(columns) < 6) {
                g_set_error(error, GWY_MODULE_FILE_ERROR,
                            GWY_MODULE_FILE_ERROR_DATA,
                            _("DATA_INFO line contains less than %d fields."),
                            6);
                g_free(columns);
                g_array_free(data_info, TRUE);
                return FALSE;
            }

            di.channel = atoi(columns[0]);
            di.name = columns[1];
            di.unit = columns[2];
            di.direction = gwy_string_to_enum(columns[3],
                                              directions,
                                              G_N_ELEMENTS(directions));
            if (di.direction == (SXMDirection)-1) {
                err_INVALID(error, "Direction");
                g_free(columns);
                g_array_free(data_info, TRUE);
                return FALSE;
            }
            di.calibration = g_ascii_strtod(columns[4], NULL);
            di.offset = g_ascii_strtod(columns[5], NULL);
            g_array_append_val(data_info, di);

            g_free(columns);
            columns = NULL;
        }

        if (!line) {
            g_array_free(data_info, TRUE);
            return FALSE;
        }

        sxmfile->data_info = (SXMDataInfo*)data_info->data;
        sxmfile->ndata = data_info->len;
        g_array_free(data_info, FALSE);
        return TRUE;
    }

    if (!(line = get_next_line_with_error(p, error)))
        return FALSE;

    g_hash_table_insert(sxmfile->meta, tag, line);
    gwy_debug("value: <%s>", line);

    return TRUE;
}

static void
read_data_field(GwyContainer *container,
                gint *id,
                const SXMFile *sxmfile,
                const SXMDataInfo *data_info,
                SXMDirection dir,
                const guchar **p)
{
    GwyDataField *dfield;
    GwySIUnit *siunit;
    gdouble *data;
    gint j;
    gchar key[32];
    gchar *s;
    gboolean flip_vertically = FALSE, flip_horizontally = FALSE;

    dfield = gwy_data_field_new(sxmfile->xres, sxmfile->yres,
                                sxmfile->xreal, sxmfile->yreal,
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    for (j = 0; j < sxmfile->xres*sxmfile->yres; j++)
        *(data++) = get_FLOAT_BE(p);

    siunit = gwy_si_unit_new("m");
    gwy_data_field_set_si_unit_xy(dfield, siunit);
    g_object_unref(siunit);

    siunit = gwy_si_unit_new(data_info->unit);
    gwy_data_field_set_si_unit_z(dfield, siunit);
    g_object_unref(siunit);

    g_snprintf(key, sizeof(key), "/%d/data", *id);
    gwy_container_set_object_by_name(container, key, dfield);
    g_object_unref(dfield);

    g_strlcat(key, "/title", sizeof(key));
    if (!dir)
        gwy_container_set_string_by_name(container, key,
                                         g_strdup(data_info->name));
    else {
        gchar *title;

        title = g_strdup_printf("%s (%s)", data_info->name,
                                dir == DIR_BACKWARD ? "Backward" : "Forward");
        gwy_container_set_string_by_name(container, key, title);
        /* Don't free title, container eats it */
    }

    if (dir == DIR_BACKWARD)
        flip_horizontally = TRUE;

    if ((s = g_hash_table_lookup(sxmfile->meta, "SCAN_DIR"))
        && gwy_strequal(s, "up"))
        flip_vertically = TRUE;

    gwy_data_field_invert(dfield, flip_vertically, flip_horizontally, FALSE);

    (*id)++;
}

static GwyContainer*
sxm_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    SXMFile sxmfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size1 = 0, size = 0;
    GError *err = NULL;
    const guchar *p;
    gchar *header, *hp, *s, *endptr;
    gchar **columns;
    gboolean rotated = FALSE;
    guint i;

    if (!gwy_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }

    if (size < MAGIC_SIZE + 400) {
        err_TOO_SHORT(error);
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    if (memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Nanonis");
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    /* Extract header (we need it writable) */
    p = memchr(buffer, '\x1a', size);
    if (!p || p + 1 == buffer + size || p[1] != '\x04') {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing data start marker \\x1a\\x04."));
        gwy_file_abandon_contents(buffer, size, NULL);
        return NULL;
    }

    memset(&sxmfile, 0, sizeof(SXMFile));
    sxmfile.meta = g_hash_table_new(g_str_hash, g_str_equal);

    header = g_memdup(buffer, p - buffer + 1);
    header[p - buffer] = '\0';
    hp = header;
    /* Move p to actual data start */
    p += 2;

    /* Parse header */
    do {
        if (!sxm_read_tag(&sxmfile, &hp, error)) {
            sxm_free_z_controller(&sxmfile);
            g_free(sxmfile.data_info);
            g_free(header);
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }
    } while (!sxmfile.ok);

    /* Data info */
    if (sxmfile.ok) {
        if (!sxmfile.data_info) {
            err_NO_DATA(error);
            sxmfile.ok = FALSE;
        }
    }

    /* Data type */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCANIT_TYPE"))) {
            gwy_debug("s: <%s>", s);
            columns = split_line_in_place(s, ' ');
            if (g_strv_length(columns) == 2
                && gwy_strequal(columns[0], "FLOAT")
                && gwy_strequal(columns[1], "LSBFIRST"))
                size1 = sizeof(gfloat);
            else {
                err_UNSUPPORTED(error, "SCANIT_TYPE");
                sxmfile.ok = FALSE;
            }
            g_free(columns);
        }
        else {
            err_MISSING_FIELD(error, "SCANIT_TYPE");
            sxmfile.ok = FALSE;
        }
    }

    /* Check for rotated data */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCAN_ANGLE"))) {
            if (g_ascii_strtod(s, NULL) == 90.0) {
                gwy_debug("data is rotated");
                rotated = TRUE;
            }
        }
    }

    /* Pixel sizes */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCAN_PIXELS"))) {
            if (sscanf(s, "%d %d", &sxmfile.xres, &sxmfile.yres) == 2) {
                if (rotated)
                    GWY_SWAP(gint, sxmfile.xres, sxmfile.yres);
                size1 *= sxmfile.xres * sxmfile.yres;
                gwy_debug("xres: %d, yres: %d", sxmfile.xres, sxmfile.yres);
                gwy_debug("size1: %u", (guint)size1);
            }
            else {
                err_INVALID(error, "SCAN_PIXELS");
                sxmfile.ok = FALSE;
            }
        }
        else {
            err_MISSING_FIELD(error, "SCAN_PIXELS");
            sxmfile.ok = FALSE;
        }
    }

    /* Physical dimensions */
    if (sxmfile.ok) {
        if ((s = g_hash_table_lookup(sxmfile.meta, "SCAN_RANGE"))) {
            sxmfile.xreal = g_ascii_strtod(s, &endptr);
            if (endptr != s) {
                s = endptr;
                sxmfile.yreal = g_ascii_strtod(s, &endptr);
                gwy_debug("xreal: %g, yreal: %g", sxmfile.xreal, sxmfile.yreal);
            }
            if (s == endptr) {
                err_INVALID(error, "SCAN_RANGE");
                sxmfile.ok = FALSE;
            }
        }
        else {
            err_MISSING_FIELD(error, "SCAN_RANGE");
            sxmfile.ok = FALSE;
        }
    }

    /* Check file size */
    if (sxmfile.ok) {
        gsize expected_size;

        expected_size = p - buffer;
        for (i = 0; i < sxmfile.ndata; i++) {
            guint d = sxmfile.data_info[i].direction;

            if (d == DIR_BOTH)
                expected_size += 2*size1;
            else if (d == DIR_FORWARD || d == DIR_BACKWARD)
                expected_size += size1;
            else {
                g_assert_not_reached();
            }
        }
        if (size != expected_size) {
            err_SIZE_MISMATCH(error, expected_size, size);
            sxmfile.ok = FALSE;
        }
    }

    /* Read data */
    if (sxmfile.ok) {
        gint id = 0;

        container = gwy_container_new();
        for (i = 0; i < sxmfile.ndata; i++) {
            guint d = sxmfile.data_info[i].direction;

            if (d == DIR_BOTH) {
                read_data_field(container, &id,
                                &sxmfile, sxmfile.data_info + i,
                                DIR_FORWARD, &p);
                read_data_field(container, &id,
                                &sxmfile, sxmfile.data_info + i,
                                DIR_BACKWARD, &p);
            }
            else if (d == DIR_FORWARD || d == DIR_BACKWARD) {
                read_data_field(container, &id,
                                &sxmfile, sxmfile.data_info + i, d, &p);
            }
            else {
                g_assert_not_reached();
            }
        }
    }

    sxm_free_z_controller(&sxmfile);
    g_free(sxmfile.data_info);
    g_hash_table_destroy(sxmfile.meta);
    g_free(header);
    gwy_file_abandon_contents(buffer, size, NULL);

    return container;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
