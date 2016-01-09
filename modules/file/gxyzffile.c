/*
 *  $Id$
 *  Copyright (C) 2013-2016 David Necas (Yeti).
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
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
 * [FILE-MAGIC-FILEMAGIC]
 * # Gwyddion simple XYZ files (GXYZF), see http://gwyddion.net/
 * 0 string Gwyddion\ XYZ\ Field\ 1.0\x0d\x0a Gwyddion XYZ field SPM data version 1.0
 **/

/**
 * [FILE-MAGIC-USERGUIDE]
 * Gwyddion XYZ data
 * .gxyzf
 * Read[1] Export
 * [1] XYZ data are interpolated to a regular grid upon import.
 **/

#include <string.h>
#include <stdlib.h>
#include <glib/gstdio.h>
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

#define PointXYZ GwyTriangulationPointXYZ

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
static gboolean      gxyzf_export       (GwyContainer *container,
                                         const gchar *filename,
                                         GwyRunType mode,
                                         GError **error);
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
    "1.4",
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
                           (GwyFileSaveFunc)&gxyzf_export);

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
    GwyTextHeaderParser parser;
    GHashTable *hash = NULL;
    guchar *p, *value, *buffer = NULL, *header = NULL, *datap;
    PointXYZ *xyzpoints = NULL;
    gdouble *points = NULL;
    gsize size;
    GError *err = NULL;
    GwySIUnit **zunits = NULL;
    GwySIUnit *xyunit = NULL, *zunit = NULL;
    guint nchan = 0, pointlen, pointsize, npoints, i, id;
    GridInfo ginfo;

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
    if (nchan > 1) {
        xyzpoints = g_new(PointXYZ, npoints);
        for (i = 0; i < npoints; i++) {
            xyzpoints[i].x = points[i*pointlen];
            xyzpoints[i].y = points[i*pointlen+1];
        }
    }

    estimate_dimensions(hash, points, pointlen, npoints, &ginfo);

    container = gwy_container_new();
    for (id = 0; id < nchan; id++) {
        GwyDataField *dfield;
        GwySIUnit *unit;
        gchar buf[32];

        if (nchan > 1) {
            for (i = 0; i < npoints; i++)
                xyzpoints[i].z = points[i*pointlen + id+2];
        }

        dfield = gwy_data_field_new(ginfo.xres, ginfo.yres,
                                    ginfo.xreal, ginfo.yreal,
                                    FALSE);
        gwy_data_field_set_xoffset(dfield, ginfo.xoff);
        gwy_data_field_set_yoffset(dfield, ginfo.yoff);

        if (nchan > 1)
            gwy_data_field_average_xyz(dfield, NULL, xyzpoints, npoints);
        else
            gwy_data_field_average_xyz(dfield, NULL, (PointXYZ*)datap, npoints);

        unit = gwy_data_field_get_si_unit_z(dfield);
        if (zunit)
            gwy_serializable_clone(G_OBJECT(zunit), G_OBJECT(unit));
        else
            gwy_serializable_clone(G_OBJECT(zunits[id]), G_OBJECT(unit));
        unit = gwy_data_field_get_si_unit_xy(dfield);
        gwy_serializable_clone(G_OBJECT(xyunit), G_OBJECT(unit));

        gwy_container_set_object(container, gwy_app_get_data_key_for_id(id),
                                 dfield);
        g_object_unref(dfield);

        g_snprintf(buf, sizeof(buf), "Title%u", id+1);
        if ((value = g_hash_table_lookup(hash, buf))) {
            g_snprintf(buf, sizeof(buf), "/%d/data/title", id);
            gwy_container_set_string_by_name(container, buf, g_strdup(value));
        }
        gwy_file_channel_import_log_add(container, id, NULL, filename);
    }

fail:
    g_free(xyzpoints);
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

    return container;
}

static inline void
append_double(gdouble *target, const gdouble v)
{
    union { guchar pp[8]; double d; } u;
    u.d = v;
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
    GWY_SWAP(guchar, u.pp[0], u.pp[7]);
    GWY_SWAP(guchar, u.pp[1], u.pp[6]);
    GWY_SWAP(guchar, u.pp[2], u.pp[5]);
    GWY_SWAP(guchar, u.pp[3], u.pp[4]);
#endif
    *target = u.d;
}

/* NB: We only export the current channel even though the format can, in
 * principle, represent multiple channels at the same coordinates. */
static gboolean
gxyzf_export(GwyContainer *container,
             const gchar *filename,
             G_GNUC_UNUSED GwyRunType mode,
             GError **error)
{
    static const gchar zeroes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    GString *header = NULL;
    gdouble *ddbl = NULL;
    guint i, j, k, xres, yres, padding;
    gint id;
    GwyDataField *dfield;
    const gdouble *d;
    gdouble xreal, yreal, xoff, yoff;
    gchar *s;
    GwySIUnit *unit, *emptyunit;
    FILE *fh;

    gwy_app_data_browser_get_current(GWY_APP_DATA_FIELD, &dfield,
                                     GWY_APP_DATA_FIELD_ID, &id,
                                     0);
    if (!dfield) {
        err_NO_CHANNEL_EXPORT(error);
        return FALSE;
    }

    if (!(fh = gwy_fopen(filename, "wb"))) {
        err_OPEN_WRITE(error);
        return FALSE;
    }

    xres = gwy_data_field_get_xres(dfield);
    yres = gwy_data_field_get_yres(dfield);
    xreal = gwy_data_field_get_xreal(dfield);
    yreal = gwy_data_field_get_yreal(dfield);
    xoff = gwy_data_field_get_xoffset(dfield);
    yoff = gwy_data_field_get_yoffset(dfield);

    header = g_string_new(MAGIC);
    g_string_append_printf(header, "NChannels = %u\n", 1);
    g_string_append_printf(header, "NPoints = %u\n", xres*yres);

    emptyunit = gwy_si_unit_new(NULL);
    unit = gwy_data_field_get_si_unit_xy(dfield);
    if (!gwy_si_unit_equal(unit, emptyunit)) {
        s = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN);
        g_string_append_printf(header, "XYUnits = %s\n", s);
        g_free(s);
    }
    unit = gwy_data_field_get_si_unit_z(dfield);
    if (!gwy_si_unit_equal(unit, emptyunit)) {
        s = gwy_si_unit_get_string(unit, GWY_SI_UNIT_FORMAT_PLAIN);
        g_string_append_printf(header, "ZUnits1 = %s\n", s);
        g_free(s);
    }
    g_object_unref(emptyunit);

    s = gwy_app_get_data_field_title(container, id);
    g_string_append_printf(header, "Title1 = %s\n", s);
    g_free(s);

    g_string_append_printf(header, "XRes = %u\n", xres);
    g_string_append_printf(header, "YRes = %u\n", yres);

    if (fwrite(header->str, 1, header->len, fh) != header->len) {
        err_WRITE(error);
        goto fail;
    }

    padding = 8 - (header->len % 8);
    if (fwrite(zeroes, 1, padding, fh) != padding) {
        err_WRITE(error);
        goto fail;
    }
    g_string_free(header, TRUE);
    header = NULL;

    ddbl = g_new(gdouble, 3*xres*yres);
    d = gwy_data_field_get_data_const(dfield);
    k = 0;
    for (i = 0; i < yres; i++) {
        for (j = 0; j < xres; j++) {
            append_double(ddbl + k++, (j + 0.5)*xreal/xres + xoff);
            append_double(ddbl + k++, (i + 0.5)*yreal/yres + yoff);
            append_double(ddbl + k++, *(d++));
        }
    }

    if (fwrite(ddbl, sizeof(gdouble), 3*xres*yres, fh) != 3*xres*yres) {
        err_WRITE(error);
        goto fail;
    }
    g_free(ddbl);
    fclose(fh);

    return TRUE;

fail:
    if (fh)
        fclose(fh);
    g_unlink(filename);
    if (header)
        g_string_free(header, TRUE);
    g_free(ddbl);

    return FALSE;
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
    gwy_debug("hint xres %u, yres %u", xres, yres);

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
    gwy_debug("final xres %u, yres %u", ginfo->xres, ginfo->yres);
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
