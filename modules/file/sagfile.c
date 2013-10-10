/*
 *  @(#) $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
 *  E-mail: yeti@gwyddion.net
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
#define DEBUG 1
/**
 * [FILE-MAGIC-USERGUIDE]
 * Zemax grid SAG data
 * .sag
 * Read
 **/

#include "config.h"
#include <stdlib.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/stats.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>
#include "err.h"

#define EXTENSION ".sag"

typedef enum {
    SAG_UNIT_MM = 0,
    SAG_UNIT_CM = 1,
    SAG_UNIT_IN = 2,
    SAG_UNIT_M = 3,
} SagUnit;

typedef struct {
    guint xres;
    guint yres;
    gdouble xreal;
    gdouble yreal;
    SagUnit unit;
    gdouble xoff;
    gdouble yoff;
} SagFile;

static gboolean      module_register(void);
static gint          sag_detect     (const GwyFileDetectInfo *fileinfo,
                                     gboolean only_name);
static GwyContainer* sag_load       (const gchar *filename,
                                     GwyRunType mode,
                                     GError **error);
static guint         sag_read_header(const gchar *header,
                                     guint len,
                                     SagFile *sagfile,
                                     GError **error);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    module_register,
    N_("Imports Zemax grid SAG data files."),
    "Yeti <yeti@gwyddion.net>",
    "0.1",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("sagfile",
                           N_("Zemax grid SAG data (.sag)"),
                           (GwyFileDetectFunc)&sag_detect,
                           (GwyFileLoadFunc)&sag_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
sag_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    SagFile sagfile;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (sag_read_header(fileinfo->head, fileinfo->buffer_len, &sagfile, NULL))
        return 80;

    return 0;
}

static GwyContainer*
sag_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;

    err_NO_DATA(error);

    return container;
}

static guint
sag_read_header(const gchar *header,
                guint len,
                SagFile *sagfile,
                GError **error)
{
    const gchar *p = header;
    gchar *end;

    /* Weed out binary files quickly. */
    if (len < 16 || (header[0] != '!' && !g_ascii_isdigit(header[0])))
        goto fail;

    /* Avoid memory allocations and stuff since this is also used in the
     * detection path which should be fast. */
    while (header[0] == '!') {
        while (*header && *header != '\n' && *header != '\r')
            header++;
        while (*header == '\n' || *header == '\r')
            header++;

        if (!*header)
            goto fail;
    }

    sagfile->xres = strtol(header, &end, 10);
    gwy_debug("xres %u", sagfile->xres);
    if (end == header)
        goto fail;
    header = end;

    sagfile->yres = strtol(header, &end, 10);
    gwy_debug("yres %u", sagfile->yres);
    if (end == header)
        goto fail;
    header = end;

    sagfile->xreal = g_ascii_strtod(header, &end);
    gwy_debug("xreal %g", sagfile->xreal);
    if (end == header)
        goto fail;
    header = end;

    sagfile->yreal = g_ascii_strtod(header, &end);
    gwy_debug("yreal %g", sagfile->yreal);
    if (end == header)
        goto fail;
    header = end;

    sagfile->unit = strtol(header, &end, 10);
    gwy_debug("unit %u", sagfile->unit);
    if (end == header)
        goto fail;
    header = end;

    sagfile->xoff = g_ascii_strtod(header, &end);
    gwy_debug("xoff %g", sagfile->xoff);
    if (end == header)
        goto fail;
    header = end;

    sagfile->yoff = g_ascii_strtod(header, &end);
    gwy_debug("yoff %g", sagfile->yoff);
    if (end == header)
        goto fail;
    header = end;

    while (*header && *header != '\n' && *header != '\r')
        header++;
    while (*header == '\n' || *header == '\r')
        header++;

    return header - p;

fail:
    err_FILE_TYPE(error, "SAG");
    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
