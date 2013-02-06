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

typedef struct {
    gint xres;
    gint yres;
    gdouble xoff;
    gdouble yoff;
    gdouble xreal;
    gdouble yreal;
} GridInfo;

static gboolean      module_register    (void);
static gint          gxyzf_detect       (const GwyFileDetectInfo *fileinfo,
                                         gboolean only_name);
static GwyContainer* gxyzf_load         (const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);
static gboolean      search             (const GridInfo *ginfo,
                                         GSList **grid,
                                         const gdouble *points,
                                         guint pointlen,
                                         gint irange,
                                         gint jrange,
                                         gdouble x,
                                         gdouble y,
                                         guint *k);
static GSList**      sort_into_grid     (const gdouble *points,
                                         guint pointlen,
                                         guint npoints,
                                         const GridInfo *ginfo);
static void          estimate_dimensions(GHashTable *hash,
                                         gdouble *points,
                                         guint pointlen,
                                         guint npoints,
                                         GridInfo *ginfo);
static guint         estimate_res       (const gdouble *points,
                                         guint pointlen,
                                         guint npoints,
                                         gdouble *off,
                                         gdouble *real,
                                         gdouble *dispersion);

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
    GwyDataField **dfields = NULL;
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    guchar *p, *value, *buffer = NULL, *header = NULL, *datap;
    gdouble *points;
    gsize size;
    GError *err = NULL;
    GwySIUnit **zunits = NULL;
    GwySIUnit *xyunit = NULL, *zunit = NULL;
    guint nchan = 0, pointlen, pointsize, npoints, i, j, id;
    GridInfo ginfo;
    GSList **grid = NULL;

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

    /* If there is ZUnits it applies to all channels. */
    if ((value = g_hash_table_lookup(hash, "ZUnits")))
        zunit = gwy_si_unit_new(value);
    else {
        zunits = g_new0(GwySIUnit*, nchan);
        for (id = 0; id < nchan; id++) {
            gchar buf[16];
            g_snprintf(buf, sizeof(buf), "ZUnits%u", id+1);
            value = g_hash_table_lookup(hash, buf);
            zunits[id] = gwy_si_unit_new(value);
        }
    }

    points = (gdouble*)datap;

    estimate_dimensions(hash, points, pointlen, npoints, &ginfo);

    dfields = g_new0(GwyDataField*, nchan);
    for (id = 0; id < nchan; id++) {
        GwySIUnit *unit;

        dfields[id] = gwy_data_field_new(ginfo.xres, ginfo.yres,
                                         ginfo.xreal, ginfo.yreal,
                                         FALSE);
        gwy_data_field_set_xoffset(dfields[id], ginfo.xoff);
        gwy_data_field_set_yoffset(dfields[id], ginfo.yoff);
        unit = gwy_data_field_get_si_unit_z(dfields[id]);
        if (zunit)
            gwy_serializable_clone(G_OBJECT(zunit), G_OBJECT(unit));
        else
            gwy_serializable_clone(G_OBJECT(zunits[id]), G_OBJECT(unit));
        unit = gwy_data_field_get_si_unit_xy(dfields[id]);
        gwy_serializable_clone(G_OBJECT(xyunit), G_OBJECT(unit));
    }

    grid = sort_into_grid(points, pointlen, npoints, &ginfo);

    for (i = 0; i < ginfo.yres; i++) {
        gdouble y = (i + 0.5)*ginfo.yreal/ginfo.yres + ginfo.yoff;
        for (j = 0; j < ginfo.xres; j++) {
            gdouble x = (j + 0.5)*ginfo.xreal/ginfo.xres + ginfo.xoff;
            gint irange = 3, jrange = 3;
            guint k = G_MAXUINT;

            while (!search(&ginfo, grid, points, pointlen, irange, jrange,
                           x, y, &k)) {
                irange = 2*irange - 1;
                jrange = 2*jrange - 1;
            }
            g_assert(k != G_MAXUINT);

            for (id = 0; id < nchan; id++) {
                dfields[id]->data[i*ginfo.xres + j] = points[k*pointlen + id+2];
            }
        }
    }

    container = gwy_container_new();
    for (id = 0; id < nchan; id++) {
        gchar buf[32];
        gwy_container_set_object(container, gwy_app_get_data_key_for_id(id),
                                 dfields[id]);
        g_snprintf(buf, sizeof(buf), "Title%u", id+1);
        if ((value = g_hash_table_lookup(hash, buf))) {
            g_snprintf(buf, sizeof(buf), "/%d/data/title", id);
            gwy_container_set_string_by_name(container, buf, g_strdup(value));
        }
    }

    /* Visualise the raw data.
     * XXX: Crashes if xres and yres do not match raw data! */
    for (id = 0; id < pointlen; id++) {
        GwyDataField *dfield = gwy_data_field_new(ginfo.xres, ginfo.yres,
                                                  1.0, 1.0, FALSE);
        gchar buf[32];
        guint k;

        for (k = 0; k < ginfo.xres*ginfo.yres; k++)
            dfield->data[k] = points[k*pointlen + id];
        gwy_container_set_object(container,
                                 gwy_app_get_data_key_for_id(nchan+id),
                                 dfield);
        g_object_unref(dfield);

        g_snprintf(buf, sizeof(buf), "/%d/data/title", nchan+id);
        if (id == 0)
            gwy_container_set_string_by_name(container, buf, g_strdup("X"));
        else if (id == 1)
            gwy_container_set_string_by_name(container, buf, g_strdup("Y"));
        else
            gwy_container_set_string_by_name(container, buf,
                                             g_strdup_printf("Raw %u", id-1));
    }

fail:
    g_free(buffer);
    if (hash)
        g_hash_table_destroy(hash);
    gwy_object_unref(xyunit);
    gwy_object_unref(zunit);
    if (zunits) {
        for (i = 0; i < nchan; i++)
            gwy_object_unref(zunits[i]);
        g_free(zunits);
    }
    if (dfields) {
        for (i = 0; i < nchan; i++)
            gwy_object_unref(dfields[i]);
        g_free(dfields);
    }
    if (grid) {
        for (i = 0; i < npoints; i++)
            g_slist_free(grid[i]);
        g_free(grid);
    }

    return container;
}

static gboolean
search(const GridInfo *ginfo, GSList **grid,
       const gdouble *points, guint pointlen,
       gint irange, gint jrange,
       gdouble x, gdouble y, guint *k)
{
    gint xres = ginfo->xres, yres = ginfo->yres;
    gdouble xq = xres/ginfo->xreal, xo = ginfo->xoff;
    gdouble yq = yres/ginfo->yreal, yo = ginfo->yoff;
    gint xi = (gint)floor(xq*(x - xo)), yi = (gint)floor(yq*(y - yo));
    gint xfrom = MAX(xi - jrange, 0), xto = MIN(xi + jrange, xres-1),
         yfrom = MAX(yi - irange, 0), yto = MIN(yi + irange, yres-1);
    gdouble safedist = fmin((irange + 0.5)/yq, (jrange + 0.5)/xq),
            safedist2 = safedist*safedist;
    gdouble mind2 = G_MAXDOUBLE;
    gint i, j;

    for (i = yfrom; i <= yto; i++) {
        for (j = xfrom; j <= xto; j++) {
            GSList *l;

            for (l = grid[i*xres + j]; l; l = g_slist_next(l)) {
                guint kk = GPOINTER_TO_UINT(l->data);
                const gdouble *pt = points + pointlen*kk;
                gdouble dx = x - pt[0], dy = y - pt[1];
                gdouble d2 = dx*dx + dy*dy;
                if (d2 < mind2 && d2 < safedist2) {
                    mind2 = d2;
                    *k = kk;
                    break;
                }
            }
        }
    }

    return mind2 < G_MAXDOUBLE;
}

static GSList**
sort_into_grid(const gdouble *points, guint pointlen, guint npoints,
               const GridInfo *ginfo)
{
    GSList **grid = g_new(GSList*, ginfo->xres*ginfo->yres);
    gdouble xq = ginfo->xres/ginfo->xreal, xo = ginfo->xoff;
    gdouble yq = ginfo->yres/ginfo->yreal, yo = ginfo->yoff;
    guint i;

    for (i = 0; i < npoints; i++) {
        gdouble x = points[i*pointlen], y = points[i*pointlen + 1];
        guint xi = (guint)floor(xq*(x - xo)), yi = (guint)floor(yq*(y - yo));
        guint k = yi*ginfo->xres + xi;

        grid[k] = g_slist_prepend(grid[k], GUINT_TO_POINTER(i));
    }

    return grid;
}

static void
estimate_dimensions(GHashTable *hash,
                    gdouble *points, guint pointlen, guint npoints,
                    GridInfo *ginfo)
{
    guint xres, yres, estxres, estyres;
    gdouble shx, shy;
    const gchar *value;

    /* Take resolution hints that NMM stores in the files, if possible. */
    xres = yres = 0;
    if ((value = g_hash_table_lookup(hash, "XRes"))) {
        xres = atoi(value);
        if (err_DIMENSION(NULL, xres))
            xres = 0;
    }
    if ((value = g_hash_table_lookup(hash, "YRes"))) {
        yres = atoi(value);
        if (err_DIMENSION(NULL, yres))
            yres = 0;
    }
    gwy_debug("hint xres %u, hint %u", xres, yres);

    estxres = estimate_res(points, pointlen, npoints,
                           &ginfo->xoff, &ginfo->xreal, &shx);
    estyres = estimate_res(points+1, pointlen, npoints,
                           &ginfo->yoff, &ginfo->yreal, &shy);
    gwy_debug("estimated xres %u (width=%g), yres %u (width=%g)",
              estxres, shx, estyres, shy);

    if (!xres || !yres) {
        if (shx < shy) {
            xres = estxres;
            yres = npoints/estxres;
        }
        else {
            xres = npoints/estyres;
            yres = estyres;
        }
    }

    ginfo->xres = xres;
    ginfo->yres = yres;
    gwy_debug("final xres %u, hint %u", ginfo->xres, ginfo->yres);
    gwy_debug("xreal %g, xoff %g", ginfo->xreal, ginfo->xoff);
    gwy_debug("yreal %g, yoff %g", ginfo->yreal, ginfo->yoff);
}

static guint
estimate_res(const gdouble *points, guint pointlen, guint npoints,
             gdouble *off, gdouble *real, gdouble *dispersion)
{
    guint n = (guint)(sqrt(5000.0*npoints) + 1), i, res, prev;
    gdouble min = G_MAXDOUBLE, max = G_MINDOUBLE, d, s2;
    const gdouble *p = points;
    guint *counts;

    for (i = 0; i < npoints; i++) {
        gdouble v = *p;
        if (v < min)
            min = v;
        if (v > max)
            max = v;
        p += pointlen;
    }
    d = max - min;
    min -= 1e-9*d;
    max += 1e-9*d;

    counts = g_new0(guint, n);
    p = points;
    for (i = 0; i < npoints; i++) {
        guint j = (guint)((*p - min)/(max - min)*n);
        counts[j]++;
        p += pointlen;
    }

    res = 2;
    prev = 0;
    s2 = 0.0;
    for (i = 1; i < n-1; i++) {
        if (counts[i] > counts[i-1] && counts[i] > counts[i+1]) {
            s2 += (gdouble)(i - prev)*(i - prev);
            res++;
            prev = i;
        }
    }
    g_free(counts);

    s2 += (gdouble)(n - prev)*(n - prev);
    s2 /= (res - 1);
    s2 -= (gdouble)n*n/(res - 1.0)/(res - 1.0);
    s2 = sqrt(MAX(s2, 0.0));

    gwy_debug("%u %g %u", res, s2, n);
    *dispersion = s2;

    *off = min;
    *real = max - min;
    d = *real/res;
    *off -= 0.4999*d;
    *real += 0.4999*d;

    return res;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
