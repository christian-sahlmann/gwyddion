/*
 *  @(#) $Id$
 *  Copyright (C) 2007 David Necas (Yeti).
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
 * <mime-type type="application/x-burleigh-export-spm">
 *   <comment>Burleigh exported SPM data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value=".Image Data"/>
 *   </magic>
 * </mime-type>
 **/

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwyutils.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/datafield.h>
#include <libgwymodule/gwymodule-file.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define EXTENSION_TEXT ".txt"
#define EXTENSION_BIN ".bin"

#define MAGIC ".Image Data"
#define MAGIC_SIZE (sizeof(MAGIC)-1)

#define MIN_FILE_SIZE 120

typedef struct {
    gint xres;
    gint yres;
    gdouble xscale;
    gdouble yscale;
    gdouble zscale;
    gdouble zres;
    GwySIUnit *xyunits;
    GwySIUnit *zunits;
    gboolean binary;
    guint length;
    guint bpp;
} BurleighExpHeader;

static gboolean      module_register         (void);
static gint          burleigh_exp_detect     (const GwyFileDetectInfo *fileinfo,
                                              gboolean only_name);
static GwyContainer* burleigh_exp_load       (const gchar *filename,
                                              GwyRunType mode,
                                              GError **error);
static gboolean      burleigh_exp_read_header(BurleighExpHeader *header,
                                              gchar *buf,
                                              GError **error);
static void          free_header             (BurleighExpHeader *header);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Burleigh text/bin exported images."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2007",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("burleigh_exp",
                           N_("Burleigh exported data (.txt, .bin)"),
                           (GwyFileDetectFunc)&burleigh_exp_detect,
                           (GwyFileLoadFunc)&burleigh_exp_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
burleigh_exp_detect(const GwyFileDetectInfo *fileinfo,
                    gboolean only_name)
{
    if (only_name)
        return 0;

    if (fileinfo->buffer_len < MIN_FILE_SIZE+1)
        return 0;

    if (memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) == 0)
        return 100;

    return 0;
}

static GwyContainer*
burleigh_exp_load(const gchar *filename,
                  G_GNUC_UNUSED GwyRunType mode,
                  GError **error)
{
    GwyContainer *container = NULL;
    gchar *buffer = NULL;
    BurleighExpHeader header;
    gsize size = 0;
    GError *err = NULL;
    GwyDataField *dfield;
    gdouble *data;
    guint i, n;

    if (!g_file_get_contents(filename, &buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        return NULL;
    }
    if (size < MIN_FILE_SIZE + 2) {
        err_TOO_SHORT(error);
        g_free(buffer);
        return NULL;
    }

    if (!burleigh_exp_read_header(&header, buffer, error))
        goto fail;

    n = header.xres * header.yres;
    if (header.binary) {
        if (header.bpp != 16) {
            err_BPP(error, header.bpp);
            goto fail;
        }
        else if (err_SIZE_MISMATCH(error, header.length + 2*n, size, TRUE))
            goto fail;
    }

    dfield = gwy_data_field_new(header.xres, header.yres,
                                header.xscale, header.yscale,
                                FALSE);
    data = gwy_data_field_get_data(dfield);

    if (header.binary) {
        const gint16 *d16 = (const gint16*)(buffer + header.length);

        for (i = 0; i < n; i++)
            data[i] = GINT16_FROM_LE(d16[i]);
    }
    else {
        gchar *p = buffer + header.length;

        for (i = 0; i < n; i++)
            data[i] = strtol(p, &p, 10);
    }

    gwy_data_field_multiply(dfield, header.zscale/32768.0);

    /* Units references released in free_header() */
    gwy_data_field_set_si_unit_xy(dfield, header.xyunits);
    gwy_data_field_set_si_unit_z(dfield, header.zunits);

    container = gwy_container_new();
    gwy_container_set_object(container, gwy_app_get_data_key_for_id(0), dfield);
    g_object_unref(dfield);
    gwy_app_channel_title_fall_back(container, 0);

fail:
    free_header(&header);
    g_free(buffer);

    return container;
}

static void
free_header(BurleighExpHeader *header)
{
    gwy_object_unref(header->xyunits);
    gwy_object_unref(header->zunits);
}

static gboolean
parse_scale(gchar **p,
            const gchar *name,
            double *value,
            GwySIUnit **units,
            GError **error)
{
    gint power10;
    gchar *vp, *line;

    line = gwy_str_next_line(p);
    if (!line) {
        err_MISSING_FIELD(error, name);
        return FALSE;
    }

    vp = strchr(line, ':');
    if (!vp) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing colon in header line."));
        return FALSE;
    }
    *vp = '\0';
    vp++;

    gwy_debug("<%s> = <%s>", name, vp);
    if (!gwy_strequal(line, name)) {
        err_MISSING_FIELD(error, name);
        return FALSE;
    }

    *value = g_ascii_strtod(vp, &vp);
    *units = gwy_si_unit_new_parse(vp, &power10);
    *value *= pow10(power10);

    if (!*value) {
        g_warning("%s is 0.0, fixing to 1.0", name);
        *value = 1.0;
    }

    return TRUE;
}

static gboolean
parse_dim(gchar **p,
          const gchar *name,
          gint *value,
          GError **error)
{
    gchar *vp, *line;

    line = gwy_str_next_line(p);
    if (!line) {
        err_MISSING_FIELD(error, name);
        return FALSE;
    }

    vp = strchr(line, ':');
    if (!vp) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Missing colon in header line."));
        return FALSE;
    }
    *vp = '\0';
    vp++;

    gwy_debug("<%s> = <%s>", name, vp);
    if (!gwy_strequal(line, name)) {
        err_MISSING_FIELD(error, name);
        return FALSE;
    }

    *value = strtol(vp, NULL, 10);
    if (err_DIMENSION(error, *value))
        return FALSE;

    return TRUE;
}

static gboolean
burleigh_exp_read_header(BurleighExpHeader *header,
                         gchar *buf,
                         GError **error)
{
    GwySIUnit *yunits = NULL, *dummy = NULL;
    gchar *p, *line;

    memset(header, 0, sizeof(BurleighExpHeader));
    p = buf;

    /* Magic header */
    if (!(line = gwy_str_next_line(&p))
        || strncmp(line, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Burleigh export");
        return FALSE;
    }

    /* Skip all other lines starting with a dot */
    while ((line = gwy_str_next_line(&p))) {
        if (sscanf(line, ".Binary Format, Header Length=%u, Integer %u bits",
                   &header->length, &header->bpp))
            header->binary = TRUE;
        if (!line || !p || p[0] != '.')
            break;
    }

    if (!line) {
        err_FILE_TYPE(error, "Burleigh export");
        return FALSE;
    }

    if (!parse_scale(&p, "X Scale", &header->xscale, &header->xyunits, error))
        return FALSE;
    if (!parse_dim(&p, "X Pixel", &header->xres, error))
        return FALSE;
    if (!parse_scale(&p, "Y Scale", &header->yscale, &yunits, error))
        return FALSE;
    /* FIXME: Check sanity */
    g_object_unref(yunits);
    if (!parse_dim(&p, "Y Pixel", &header->yres, error))
        return FALSE;
    if (!parse_scale(&p, "Z Scale", &header->zscale, &header->zunits, error))
        return FALSE;
    if (!parse_scale(&p, "Z Res.(value/digital)", &header->zres, &dummy,
                     error))
        return FALSE;
    g_object_unref(dummy);

    if (!header->binary)
        header->length = p - buf;

    return TRUE;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
