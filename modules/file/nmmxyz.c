/*
 *  $Id$
 *  Copyright (C) 2016 David Necas (Yeti).
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
#define DEBUG 1
/**
 * [FILE-MAGIC-USERGUIDE]
 * Nano Measuring Machine profile data
 * *.dsc + *.dat
 * Read
 **/

#include <stdlib.h>
#include <string.h>
#include <libgwyddion/gwymacros.h>
#include <libgwyddion/gwymath.h>
#include <libgwymodule/gwymodule-file.h>
#include <libprocess/datafield.h>
#include <app/data-browser.h>
#include <app/gwymoduleutils-file.h>

#include "get.h"
#include "err.h"

#define PointXYZ GwyTriangulationPointXYZ

#define EXTENSION ".dsc"
#define DASHED_LINE "------------------------------------------"

typedef struct {
    guint id;
    guint npts;
    gchar *date;
    gchar *short_name;
    gchar *long_name;
} NMMXYZProfileDescription;

static gboolean      module_register           (void);
static gint          nmmxyz_detect             (const GwyFileDetectInfo *fileinfo,
                                                gboolean only_name);
static GwyContainer* nmmxyz_load               (const gchar *filename,
                                                GwyRunType mode,
                                                GError **error);
static void          find_data_range           (const gdouble *alldata,
                                                guint ndata,
                                                guint blocksize,
                                                gdouble *xmin,
                                                gdouble *xmax,
                                                gdouble *ymin,
                                                gdouble *ymax);
static void          read_data_file            (GArray *data,
                                                const gchar *filename,
                                                guint nrec);
static gboolean      profile_descriptions_match(GArray *descs1,
                                                GArray *descs2);
static GArray*       read_profile_description  (const gchar *filename);
static void          free_profile_descriptions (GArray *dscs);

static GwyModuleInfo module_info = {
    GWY_MODULE_ABI_VERSION,
    &module_register,
    N_("Imports Nano Measuring Machine profile files."),
    "Yeti <yeti@gwyddion.net>",
    "1.0",
    "David Nečas (Yeti)",
    "2016",
};

GWY_MODULE_QUERY(module_info)

static gboolean
module_register(void)
{
    gwy_file_func_register("nmmxyz",
                           N_("Nano Measuring Machine files (*.dsc)"),
                           (GwyFileDetectFunc)&nmmxyz_detect,
                           (GwyFileLoadFunc)&nmmxyz_load,
                           NULL,
                           NULL);
    return TRUE;
}

static gint
nmmxyz_detect(const GwyFileDetectInfo *fileinfo, gboolean only_name)
{
    gint score = 0;

    if (only_name)
        return g_str_has_suffix(fileinfo->name_lowercase, EXTENSION) ? 15 : 0;

    if (g_str_has_prefix(fileinfo->head, DASHED_LINE)
        && strstr(fileinfo->head, "Scan procedure description file"))
        score = 80;

    return score;
}

static GwyContainer*
nmmxyz_load(const gchar *filename,
            G_GNUC_UNUSED GwyRunType mode,
            GError **error)
{
    GwyContainer *container = NULL;
    GArray *data = NULL;
    GArray *dscs_first = NULL;
    gchar *dirname = NULL, *basename = NULL, *s;
    GDir *dir = NULL;
    const gchar *fname;
    gdouble xmin, xmax, ymin, ymax;
    PointXYZ *points = NULL;
    guint i, j, blocksize = 0, npts = 0;
    const gdouble *d;
    gint xres, yres;

    dirname = g_path_get_dirname(filename);
    basename = g_path_get_basename(filename);
    if ((s = strrchr(basename, '.')))
        *s = '\0';

    dir = g_dir_open(dirname, 0, NULL);
    if (!dir)
        goto fail;

    data = g_array_new(FALSE, FALSE, sizeof(gdouble));
    while ((fname = g_dir_read_name(dir))) {
        GArray *dscs;
        guint len;

        if (!g_str_has_prefix(fname, basename)
            || !g_str_has_suffix(fname, EXTENSION))
            continue;

        s = g_build_filename(dirname, fname, NULL);
        dscs = read_profile_description(s);
        g_free(s);

        if (!dscs)
            continue;

        gwy_debug("Found DSC file %s (%u records)", fname, (guint)dscs->len);
        if (!dscs_first) {
            dscs_first = dscs;
            blocksize = dscs_first->len;
        }
        else {
            if (!profile_descriptions_match(dscs_first, dscs)) {
                gwy_debug("non-matching profile descriptions for %s", fname);
                free_profile_descriptions(dscs);
                continue;
            }
            free_profile_descriptions(dscs);
        }

        s = g_build_filename(dirname, fname, NULL);
        len = strlen(s);
        s[len-1] = 't';
        s[len-2] = 'a';
        read_data_file(data, s, blocksize);
        g_free(s);
    }

    if (!data->len) {
        err_NO_DATA(error);
        goto fail;
    }

    npts = data->len/blocksize;
    d = (const gdouble*)data->data;
    find_data_range(d, npts, blocksize, &xmin, &xmax, &ymin, &ymax);

    points = g_new(PointXYZ, npts);
    for (i = 0; i < npts; i++) {
        const gdouble *xy = d + i*blocksize;
        points[i].x = xy[0];
        points[i].y = xy[1];
    }

    /* XXX: Needs some safeguards.  But anyway, this will be selectable in the
     * GUI when we have it. */
    xres = (gint)ceil(2048.0*(xmax - xmin)/(ymax - ymin));
    yres = (gint)ceil(2048.0*(ymax - ymin)/(xmax - xmin));

    container = gwy_container_new();
    for (j = 0; j < blocksize-2; j++) {
        const NMMXYZProfileDescription *dsc
            = &g_array_index(dscs_first, NMMXYZProfileDescription, i);

        GwyDataField *dfield = gwy_data_field_new(xres, yres,
                                                  xmax - xmin, ymax - ymin,
                                                  FALSE);
        GQuark quark;

        gwy_data_field_set_xoffset(dfield, xmin);
        gwy_data_field_set_yoffset(dfield, ymin);
        for (i = 0; i < npts; i++)
            points[i].z = d[i*blocksize + 2 + j];

        gwy_data_field_average_xyz(dfield, NULL, points, npts);

        quark = gwy_app_get_data_key_for_id(j);
        gwy_container_set_object(container, quark, dfield);
        g_object_unref(dfield);

        quark = gwy_app_get_data_title_key_for_id(j);
        gwy_container_set_const_string(container, quark, dsc->long_name);
    }

fail:
    g_free(points);
    free_profile_descriptions(dscs_first);
    if (data)
        g_array_free(data, TRUE);
    if (dir)
        g_dir_close(dir);
    g_free(basename);
    g_free(dirname);

    return container;
}

static void
find_data_range(const gdouble *alldata, guint ndata, guint blocksize,
                gdouble *pxmin, gdouble *pxmax, gdouble *pymin, gdouble *pymax)
{
    gdouble xmin = G_MAXDOUBLE;
    gdouble ymin = G_MAXDOUBLE;
    gdouble xmax = -G_MAXDOUBLE;
    gdouble ymax = -G_MAXDOUBLE;
    guint i;

    gwy_debug("finding the XY range from %u records", ndata);
    for (i = 0; i < ndata; i++) {
        const gdouble *xy = alldata + i*blocksize;
        if (xy[0] < xmin)
            xmin = xy[0];
        if (xy[0] > xmax)
            xmax = xy[0];
        if (xy[1] < ymin)
            ymin = xy[1];
        if (xy[1] > ymax)
            ymax = xy[1];
    }

    *pxmin = xmin;
    *pxmax = xmax;
    *pymin = ymin;
    *pymax = ymax;
    gwy_debug("full data range [%g,%g]x[%g,%g]", xmin, xmax, ymin, ymax);
}

static void
read_data_file(GArray *data, const gchar *filename, guint nrec)
{
    gchar *buffer = NULL, *line, *p, *end;
    gdouble *rec = NULL;
    gsize size;
    guint i, linecount = 0;

    gwy_debug("reading data file %s", filename);
    if (!g_file_get_contents(filename, &buffer, &size, NULL)) {
        gwy_debug("cannot read file %s", filename);
        goto fail;
    }

    p = buffer;
    rec = g_new(gdouble, nrec);

    while ((line = gwy_str_next_line(&p))) {
        for (i = 0; i < nrec; i++) {
            rec[i] = g_ascii_strtod(line, &end);
            if (end == line) {
                gwy_debug("line %u terminated prematurely", linecount);
                goto fail;
            }
            line = end;
        }
        /* Now we have read a complete record so append it to the array. */
        g_array_append_vals(data, rec, nrec);
        linecount++;
    }

    gwy_debug("read %u records", linecount);

fail:
    g_free(rec);
    g_free(buffer);
}

static gboolean
profile_descriptions_match(GArray *descs1, GArray *descs2)
{
    guint i;

    if (descs1->len != descs2->len) {
        gwy_debug("non-matching channel numbers %u vs %u",
                  (guint)descs1->len, (guint)descs2->len);
        return FALSE;
    }

    for (i = 0; i < descs1->len; i++) {
        const NMMXYZProfileDescription *desc1
            = &g_array_index(descs1, NMMXYZProfileDescription, i);
        const NMMXYZProfileDescription *desc2
            = &g_array_index(descs2, NMMXYZProfileDescription, i);

        if (!gwy_strequal(desc1->short_name, desc2->short_name)) {
            gwy_debug("non-matching channel names %s vs %s",
                      desc1->short_name, desc2->short_name);
            return FALSE;
        }
    }

    return TRUE;
}

static GArray*
read_profile_description(const gchar *filename)
{
    GArray *dscs = NULL;
    NMMXYZProfileDescription dsc;
    gchar *buffer = NULL, *line, *p;
    gchar **pieces = NULL;
    gsize size;
    guint i;

    if (!g_file_get_contents(filename, &buffer, &size, NULL))
        goto fail;

    p = buffer;
    if (!(line = gwy_str_next_line(&p)) || !gwy_strequal(line, DASHED_LINE))
        goto fail;

    dscs = g_array_new(FALSE, FALSE, sizeof(NMMXYZProfileDescription));
    while ((line = gwy_str_next_line(&p))) {
        if (gwy_strequal(line, DASHED_LINE))
            break;

        pieces = g_strsplit(line, " : ", -1);
        if (g_strv_length(pieces) != 5)
            goto fail;

        dsc.id = atoi(pieces[0]);
        dsc.date = g_strdup(pieces[1]);
        dsc.short_name = g_strdup(pieces[2]);
        dsc.npts = atoi(pieces[3]);
        dsc.long_name = g_strdup(pieces[4]);
        g_array_append_val(dscs, dsc);
    }

    /* The ids should match the line numbers. */
    for (i = 0; i < dscs->len; i++) {
        if (g_array_index(dscs, NMMXYZProfileDescription, i).id != i)
            goto fail;
    }

    /* We cannot read files with different number of points for each channel. */
    for (i = 1; i < dscs->len; i++) {
        if (g_array_index(dscs, NMMXYZProfileDescription, i).npts
            != g_array_index(dscs, NMMXYZProfileDescription, i).npts)
            goto fail;
    }

    return dscs;

fail:
    g_free(buffer);
    free_profile_descriptions(dscs);
    if (pieces)
        g_strfreev(pieces);

    return NULL;
}

static void
free_profile_descriptions(GArray *dscs)
{
    guint i;

    if (!dscs)
        return;

    for (i = 0; i < dscs->len; i++) {
        NMMXYZProfileDescription *dsc
            = &g_array_index(dscs, NMMXYZProfileDescription, i);

        g_free(dsc->date);
        g_free(dsc->short_name);
        g_free(dsc->long_name);
    }
    g_array_free(dscs, TRUE);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */
