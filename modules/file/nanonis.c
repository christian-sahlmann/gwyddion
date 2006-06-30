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
#define DEBUG 1
#include "config.h"
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

typedef struct {
    GHashTable *meta;
    const guchar **data;
    gboolean header_ok;
} SXMFile;

static gboolean      module_register(void);
static gint          sxm_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* sxm_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static gboolean    data_field_has_highly_nosquare_samples(GwyDataField *dfield);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nanonis SXM data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti) & Petr Klapetek",
    "2006",
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

static gboolean
sxm_read_tag(SXMFile *sxmfile,
             gchar **p,
             GError **error)
{
    gchar *line, *tag;
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
        sxmfile->header_ok = TRUE;
        return TRUE;
    }

    if (gwy_strequal(tag, "Z-CONTROLLER")) {
        line = get_next_line_with_error(p, error);
        line = get_next_line_with_error(p, error);
        /* TODO: Parse Z-CONTROLLER */
        return TRUE;
    }

    if (gwy_strequal(tag, "DATA_INFO")) {
        while ((line = get_next_line_with_error(p, error)) && *line)
            ;
        /* TODO: Parse DATA_INFO */
        return TRUE;
    }

    if (!(line = get_next_line_with_error(p, error)))
        return FALSE;

    g_hash_table_insert(sxmfile->meta, tag, line);
    gwy_debug("value: <%s>", line);

    return TRUE;
}

static GwyContainer*
sxm_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    SXMFile sxmfile;
    GwyContainer *container = NULL;
    guchar *buffer = NULL;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield = NULL;
    const guchar *p;
    gchar *header, *hp;

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

    do {
        if (!sxm_read_tag(&sxmfile, &hp, error)) {
            g_free(header);
            gwy_file_abandon_contents(buffer, size, NULL);
            return NULL;
        }
    } while (!sxmfile.header_ok);

    g_free(header);
    gwy_file_abandon_contents(buffer, size, NULL);
    g_hash_table_destroy(sxmfile.meta);

    return container;
}

static gboolean
data_field_has_highly_nosquare_samples(GwyDataField *dfield)
{
    gint xres, yres;
    gdouble xreal, yreal, q;

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);

    q = (xreal/xres)/(yreal/yres);

    /* The threshold is somewhat arbitrary.  Fortunately, most files encoutered
     * in practice have either q very close to 1, or 2 or more */
    return q > G_SQRT2 || q < 1.0/G_SQRT2;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
