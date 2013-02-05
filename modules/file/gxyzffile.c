/*
 *  $Id$
 *  Copyright (C) 2013 David Necas (Yeti).
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
 * <mime-type type="application/x-gxyzf-spm">
 *   <comment>Gwyddion XYZ data</comment>
 *   <magic priority="80">
 *     <match type="string" offset="0" value="Gwyddion XYZ Field 1.0\n"/>
 *   </magic>
 *   <glob pattern="*.gxyzf"/>
 *   <glob pattern="*.GXYZF"/>
 * </mime-type>
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Gwyddion XYZ data
 * .gxyzf
 * Read[1]
 * [1] XYZ data are interpolated to a regular grid upon import.
 **/
#define DEBUG 1
#include <string.h>
#include <stdlib.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwyddion/gwyutils.h>
#include <libprocess/stats.h>
#include <libprocess/triangulation.h>
#include <app/gwymoduleutils-file.h>
#include <app/data-browser.h>

#include "err.h"

#define MAGIC "Gwyddion XYZ Field 1.0\n"
#define MAGIC_SIZE (sizeof(MAGIC)-1)
#define EXTENSION ".gxyzf"

static gboolean      module_register    (void);
static gint          gxyzf_detect       (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer* gxyzf_load         (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);
static int           compare_xy         (const void *pa,
                                         const void *pb);
static void          estimate_dimensions(GHashTable *hash,
                                         const gdouble *points,
                                         guint pointlen,
                                         guint npoints,
                                         guint *xres,
                                         guint *yres,
                                         gdouble *xoff,
                                         gdouble *yoff,
                                         gdouble *xreal,
                                         gdouble *yreal);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Gwyddion XYZ field files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2013",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("gxyzfile",
                           N_("GwyXYZ data files"),
                           (GwyFileDetectFunc)&gxyzf_detect,
                           (GwyFileLoadFunc)&gxyzf_load,
                           NULL,
                           NULL);

    return TRUE;
}

static gint
gxyzf_detect(const GwyFileDetectInfo *fileinfo,
              gboolean only_name)
{
    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 20 : 0;

    if (fileinfo->file_size < MAGIC_SIZE
        || memcmp(fileinfo->head, MAGIC, MAGIC_SIZE) != 0)
        return 0;

    return 100;
}

static GwyContainer*
gxyzf_load(const gchar *filename,
         G_GNUC_UNUSED GwyRunType mode,
         GError **error)
{
    GwyContainer *container = NULL;
    GwyDataField *dfield;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    guchar *p, *value, *buffer = NULL, *header = NULL, *datap;
    gdouble *points;
    gsize size;
    GError *err = NULL;
    GwySIUnit **zunits = NULL;
    GwySIUnit *xyunit = NULL;
    guint nchan = 0, pointlen, pointsize, npoints, i, xres, yres;
    gdouble xoff, xreal, yoff, yreal;

    if (!g_file_get_contents(filename, (gchar**)&buffer, &size, &err)) {
        err_GET_FILE_CONTENTS(error, &err);
        goto fail;
    }

    if (size < MAGIC_SIZE || memcmp(buffer, MAGIC, MAGIC_SIZE) != 0) {
        err_FILE_TYPE(error, "Gwyddion XYZ Field");
        goto fail;
    }

    p = buffer + MAGIC_SIZE;
    datap = memchr(p, '\0', size - (p - buffer));
    if (!datap) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("File header is truncated."));
        goto fail;
    }
    header = g_strdup(p);
    datap += 8 - ((datap - buffer) % 8);

    gwy_clear(&parser, 1);
    parser.key_value_separator = "=";
    if (!(hash = gwy_text_header_parse(header, &parser, NULL, NULL))) {
        g_propagate_error(error, err);
        goto fail;
    }

    if (!(value = g_hash_table_lookup(hash, "NChannels"))) {
        err_MISSING_FIELD(error, "NChannels");
        goto fail;
    }
    nchan = atoi(value);
    if (nchan < 1 || nchan > 1024) {
        err_INVALID(error, "NChannels");
        goto fail;
    }

    pointlen = nchan + 2;
    pointsize = pointlen*sizeof(gdouble);
    if ((size - (datap - buffer)) % pointsize) {
        g_set_error(error, GWY_MODULE_FILE_ERROR, GWY_MODULE_FILE_ERROR_DATA,
                    _("Data size %lu is not a multiple of point size %u."),
                    (gulong)(size - (datap - buffer)), pointsize);
        goto fail;
    }
    npoints = (size - (datap - buffer))/pointsize;

    value = g_hash_table_lookup(hash, "XYUnits");
    xyunit = gwy_si_unit_new(value);

    zunits = g_new(GwySIUnit*, nchan);
    for (i = 0; i < nchan; i++) {
        gchar buf[16];
        g_snprintf(buf, sizeof(buf), "ZUnits%u", i);
        value = g_hash_table_lookup(hash, buf);
        zunits[i] = gwy_si_unit_new(value);
    }

    points = (gdouble*)datap;
    qsort(points, npoints, pointsize, compare_xy);

    estimate_dimensions(hash, points, pointlen, npoints,
                        &xres, &yres, &xoff, &yoff, &xreal, &yreal);

    err_NO_DATA(error);

fail:
    g_free(buffer);
    if (hash)
        g_hash_table_destroy(hash);
    if (zunits) {
        for (i = 0; i < nchan; i++)
            gwy_object_unref(zunits[i]);
        g_free(zunits);
    }
    gwy_object_unref(xyunit);

    return container;
}

static int
compare_xy(const void *pa, const void *pb)
{
    const gdouble *a = (const gdouble*)pa, *b = (const gdouble*)pb;
    gdouble va = a[1], vb = b[1];
    if (va < vb)
        return -1;
    if (va > vb)
        return 1;
    va = a[0];
    vb = b[0];
    if (va < vb)
        return -1;
    if (va > vb)
        return 1;
    return 0;
}

static void
estimate_dimensions(GHashTable *hash,
                    const gdouble *points, guint pointlen, guint npoints,
                    guint *xres, guint *yres,
                    gdouble *xoff, gdouble *yoff,
                    gdouble *xreal, gdouble *yreal)
{
    const gchar *value;

    /* Take resolution hints that NMM stores in the files, if possible. */
    *xres = *yres = 0;
    if ((value = g_hash_table_lookup(hash, "XRes"))) {
        *xres = atoi(value);
        if (err_DIMENSION(NULL, *xres))
            *xres = 0;
    }
    if ((value = g_hash_table_lookup(hash, "YRes"))) {
        *yres = atoi(value);
        if (err_DIMENSION(NULL, *yres))
            *yres = 0;
    }
    gwy_debug("xres hint %u, yres hint %u", *xres, *yres);
    *xres = *yres = 0;   // force estimate

    if (!*xres || !*yres) {
        gdouble x0 = points[0], xprev = x0;
        guint i;
        *yres = 1;
        for (i = 1; i < npoints; i++) {
            gdouble x = points[i*pointlen];
            if (fabs(x - x0) < fabs(x - xprev))
                (*yres)++;
            xprev = x;
        }
        gwy_debug("estimated yres %u", *yres);
    }
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
